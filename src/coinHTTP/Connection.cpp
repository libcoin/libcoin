/* -*-c++-*- libcoin - Copyright (C) 2012 Michael Gronager
 *
 * libcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * libcoin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libcoin.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <coinHTTP/Connection.h>
#include <vector>
#include <coinHTTP/ConnectionManager.h>
#include <coinHTTP/RequestHandler.h>
#include <coinHTTP/RPC.h>

#include <coin/Logger.h>

#include <boost/bind.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>

using namespace boost;
using namespace asio;
using namespace std;


Connection::Connection(io_service& io_service, ConnectionManager& manager, RequestHandler& handler, std::ostream& access_log) : _ctx(io_service, ssl::context::sslv23), _socket(io_service), _ssl_socket(io_service, _ctx), _secure(false), _keep_alive(io_service), _exec_postpone(io_service), _connectionManager(manager), _requestHandler(handler), _reply(_request), _access_log(access_log), _max_request_duration(boost::posix_time::milliseconds(10000)) , _exec_retry_duration(boost::posix_time::milliseconds(5000)) {
}

Connection::Connection(io_service& io_service, ssl::context& context, ConnectionManager& manager, RequestHandler& handler, std::ostream& access_log) : _ctx(io_service, ssl::context::sslv23), _socket(io_service), _ssl_socket(io_service, context), _secure(true), _keep_alive(io_service), _exec_postpone(io_service), _connectionManager(manager), _requestHandler(handler), _reply(_request), _access_log(access_log), _max_request_duration(boost::posix_time::milliseconds(10000)) , _exec_retry_duration(boost::posix_time::milliseconds(5000)) {
}


ip::tcp::socket& Connection::socket() {
    if(_secure)
        return _ssl_socket.next_layer();
    else
        return _socket;
}

const ip::tcp::socket& Connection::socket() const {
    if(_secure)
        //return _ssl_socket.next_layer(); -- need the ugly cast hack below for boost < 1.48 - missing const version of next_layer()
        return (*(const_cast<ssl_socket*>(&_ssl_socket))).next_layer();
    else
        return _socket;
}

void Connection::start() {
    if(_secure)
        _ssl_socket.async_handshake(boost::asio::ssl::stream_base::server,
                                    boost::bind(&Connection::handle_handshake, this,
                                                boost::asio::placeholders::error));    
    else {
        _buffer_iterator = _buffer.begin();
        _socket.async_read_some(buffer(_buffer), boost::bind(&Connection::handle_read, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
    }
}

void Connection::stop() {
    socket().close();
}

void Connection::handle_handshake(const boost::system::error_code& error) {
    if (!error) {        
        _buffer_iterator = _buffer.begin();
        _ssl_socket.async_read_some(buffer(_buffer), boost::bind(&Connection::handle_read, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
    }
    else {
        _connectionManager.stop(shared_from_this());
    }
}

void Connection::log_request() const {
    // 127.0.0.1 - frank [10/Oct/2000:13:55:36 -0700] "GET /apache_pb.gif HTTP/1.0" 200 2326 "http://www.example.com/start.html" "Mozilla/4.08 [en] (Win98; I ;Nav)"

    boost::system::error_code ec;
    ip::tcp::endpoint remote = socket().remote_endpoint(ec);
    if(ec) // unbound requests are artefacts (result from write calling read, e.g. when trying ssl on non ssl conn)
        return;
    _access_log << remote.address() << " - ";
    Headers::const_iterator header = _request.headers().find("Authorization");
    std::string basic_auth;
    if (header != _request.headers().end())
        basic_auth = header->second;
    if (basic_auth.length() < 9)
        _access_log << "- ";
    else if (basic_auth.substr(0,6) != "Basic ")
        _access_log << "- ";
    else {
        Auth auth(basic_auth.substr(6));
        if(auth.username() == "")
            _access_log << "- ";
        else
            _access_log << auth.username() << " ";
    }
    _access_log << posix_time::second_clock::local_time() << " ";
    _access_log << "\"" << _request.method() << " ";
    _access_log << _request.uri() << " ";
    _access_log << "HTTP/" << _request.version() << "\" ";
    _access_log << _reply.status() << " ";
    _access_log << _reply.content_length() << " ";
    header = _request.headers().find("Referer");
    if (header != _request.headers().end())
        _access_log << "\"" << header->second << "\" ";
    else 
        _access_log << "- ";
    header = _request.headers().find("User-Agent");
    if (header != _request.headers().end())
        _access_log << "\"" << header->second << "\"";
    else
        _access_log << "-";
    //    _access_log << " " << hex << (long) this << dec;
    // finally, if POST, log the rpc method if present
    if (_request.method() == "POST") {
        try {
            RPC rpc(_request);
            string rpc_method = rpc.method();
            if (rpc_method.size())
                _access_log << " " << rpc_method;
        }
        catch (...) {
            _access_log << " " << "Malformed RPC call";
        }
    }
    
    _access_log << endl;
}

void Connection::handle_read(const system::error_code& e, std::size_t bytes_transferred) {
    _keep_alive.cancel();
    if (!e) {
        tribool result;
        // loop over all this to get all the data...
        _bytes_read = bytes_transferred;
        boost::tie(result, _buffer_iterator) = _requestParser.parse(_request, _buffer_iterator, _buffer.begin() + bytes_transferred);
        
        if (result) {
            // fill in the extra field of the Request
            boost::system::error_code ec;
            ip::tcp::endpoint remote = socket().remote_endpoint(ec);
            if(!ec) // unbound requests are artefacts (result from write calling read, e.g. when trying ssl on non ssl conn)
                _request.remote = remote.address();
            _request.timestamp = boost::posix_time::microsec_clock::local_time();

            if (e != boost::asio::error::operation_aborted) {
                if (boost::posix_time::microsec_clock::local_time() - _request.timestamp > _max_request_duration) {
                    _reply.status(Reply::gateway_timeout);
                    handle_exec();
                }
                else
                    _requestHandler.async_exec(_request, _reply, boost::bind(&Connection::handle_exec, shared_from_this()));
            }
        }
        else if (!result) {
            _reply.status(Reply::bad_request);
            log_request();
            _request.reset();
            if(_secure)
                async_write(_ssl_socket, _reply.to_buffers(), boost::bind(&Connection::handle_write, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
            else
                async_write(_socket, _reply.to_buffers(), boost::bind(&Connection::handle_write, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
        }
        else {
            _buffer_iterator = _buffer.begin();
            if(_secure)
                _ssl_socket.async_read_some(buffer(_buffer), boost::bind(&Connection::handle_read, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
            else
                _socket.async_read_some(buffer(_buffer), boost::bind(&Connection::handle_read, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
        }
    }
    else if (e != error::operation_aborted) {
        _connectionManager.stop(shared_from_this());
    }
}

// handle_exec is called from the async_exec thread, but will do a async write on the Server thread
void Connection::handle_exec() {
    log_request();
    _request.reset();
    if(_secure)
        async_write(_ssl_socket, _reply.to_buffers(), boost::bind(&Connection::handle_write, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
    else
        async_write(_socket, _reply.to_buffers(), boost::bind(&Connection::handle_write, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
}

void Connection::handle_write(const system::error_code& e, size_t bytes_transferred) {
    bool keep_alive = true; // assuming HTTP 1.1
    if (_request.version() == "1.0")
        keep_alive = false; // keep alive is false for HTTP 1.0
    Headers::const_iterator header = _request.headers().find("Connection");
    if (header != _request.headers().end()) // if A Connection field is supplied - use it
        keep_alive = (header->second != "close");
    
    if (!e) {
        if (keep_alive) { // read again, start deadline timer
            _keep_alive.expires_from_now(_exec_retry_duration); // 2 seconds
            _keep_alive.async_wait(boost::bind(&Connection::handle_timeout, this, asio::placeholders::error));
            // we need to enpty the buffer first:
            if (_buffer_iterator < _buffer.begin() + _bytes_read) { // call handle_read again
                handle_read(e, _bytes_read); 
            }
            else {
                _buffer_iterator = _buffer.begin();
                if(_secure)
                    _ssl_socket.async_read_some(buffer(_buffer), boost::bind(&Connection::handle_read, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
                else
                    _socket.async_read_some(buffer(_buffer), boost::bind(&Connection::handle_read, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
            }
        }
        else { // Initiate graceful connection closure.
            system::error_code ignored_ec;
            socket().shutdown(ip::tcp::socket::shutdown_both, ignored_ec);
        }
    }
    else if (e != error::operation_aborted) {
        _connectionManager.stop(shared_from_this());
    }
}

void Connection::handle_timeout(const boost::system::error_code& e) {
    if(!e) {
        // Check whether the timeout has passed. We compare the deadline against
        // the current time.
        if (_keep_alive.expires_at() <= deadline_timer::traits_type::now()) {
            // The deadline has passed. Do a graceful shutdown
            system::error_code ignored_ec;
            socket().shutdown(ip::tcp::socket::shutdown_both, ignored_ec);
        }        
    }
    else if (e != error::operation_aborted) {
        log_debug("Possible boost keep_alive timer error in Server: %s\n", e.message().c_str());
    }
}

