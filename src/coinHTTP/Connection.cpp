
#include "coinHTTP/Connection.h"
#include <vector>
#include <boost/bind.hpp>
#include "coinHTTP/ConnectionManager.h"
#include "coinHTTP/RequestHandler.h"

using namespace boost;
using namespace asio;

Connection::Connection(io_service& io_service, ConnectionManager& manager, RequestHandler& handler) : _ctx(ssl::context::sslv2), _socket(io_service), _ssl_socket(io_service, _ctx), _secure(false), _connectionManager(manager), _requestHandler(handler) {
}

Connection::Connection(io_service& io_service, ssl::context& context, ConnectionManager& manager, RequestHandler& handler) : _ctx(ssl::context::sslv2), _socket(io_service), _ssl_socket(io_service, context), _secure(true), _connectionManager(manager), _requestHandler(handler) {
}


ip::tcp::socket& Connection::socket() {
    if(_secure)
        return _ssl_socket.next_layer();
    else
        return _socket;
}

void Connection::start() {
    if(_secure)
        _ssl_socket.async_handshake(boost::asio::ssl::stream_base::server,
                                    boost::bind(&Connection::handle_handshake, this,
                                                boost::asio::placeholders::error));    
    else
        _socket.async_read_some(buffer(_buffer), bind(&Connection::handle_read, shared_from_this(), placeholders::error, placeholders::bytes_transferred));
}

void Connection::stop() {
    socket().close();
}

void Connection::handle_handshake(const boost::system::error_code& error) {
    if (!error) {        
        _ssl_socket.async_read_some(buffer(_buffer), bind(&Connection::handle_read, shared_from_this(), placeholders::error, placeholders::bytes_transferred));
    }
    else {
        _connectionManager.stop(shared_from_this());;
    }
}

void Connection::handle_read(const system::error_code& e, std::size_t bytes_transferred) {
    if (!e) {
        tribool result;
        tie(result, tuples::ignore) = _requestParser.parse(_request, _buffer.data(), _buffer.data() + bytes_transferred);
        
        if (result) {
            if (_request.method == "GET") {
                _requestHandler.handleGET(_request, _reply);
                if(_secure)
                    async_write(_ssl_socket, _reply.to_buffers(), bind(&Connection::handle_write, shared_from_this(), placeholders::error));
                else
                    async_write(_socket, _reply.to_buffers(), bind(&Connection::handle_write, shared_from_this(), placeholders::error));
            }
            else if(_request.method == "POST") {
                _requestHandler.handlePOST(_request, _reply);
                if(_secure)
                    async_write(_ssl_socket, _reply.to_buffers(), bind(&Connection::handle_write, shared_from_this(), placeholders::error));
                else
                    async_write(_socket, _reply.to_buffers(), bind(&Connection::handle_write, shared_from_this(), placeholders::error));
            }
        }
        else if (!result) {
            _reply = Reply::stock_reply(Reply::bad_request);
            if(_secure)
                async_write(_ssl_socket, _reply.to_buffers(), bind(&Connection::handle_write, shared_from_this(), placeholders::error));
            else
                async_write(_socket, _reply.to_buffers(), bind(&Connection::handle_write, shared_from_this(), placeholders::error));
        }
        else {
            if(_secure)
                _ssl_socket.async_read_some(buffer(_buffer), bind(&Connection::handle_read, shared_from_this(), placeholders::error, placeholders::bytes_transferred));
            else
                _socket.async_read_some(buffer(_buffer), bind(&Connection::handle_read, shared_from_this(), placeholders::error, placeholders::bytes_transferred));
        }
    }
    else if (e != error::operation_aborted) {
        _connectionManager.stop(shared_from_this());
    }
}

void Connection::handle_write(const system::error_code& e) {
    if (!e) {
        // Initiate graceful connection closure.
        system::error_code ignored_ec;
        socket().shutdown(ip::tcp::socket::shutdown_both, ignored_ec);
    }
    
    if (e != error::operation_aborted) {
        _connectionManager.stop(shared_from_this());
    }
}


