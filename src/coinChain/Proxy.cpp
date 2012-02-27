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

#include <coinChain/Proxy.h>

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <signal.h>

using namespace std;
using namespace boost;
using namespace asio;

class application_category : public boost::system::error_category 
{ 
public: 
    const char *name() const { return "application"; } 
    std::string message(int ev) const { return "proxy could not connect"; } 
}; 

application_category __application_category; 

Proxy& Proxy::operator()(boost::asio::ip::tcp::socket& socket) { _socket = &socket; return *this; }

void Proxy::async_connect(ip::tcp::endpoint ep, boost::function<void (const system::error_code&)> connection_handler) {
    _endpoint = ep;
    _connection_handler = connection_handler;
    
    _socket->async_connect(_server, bind(&Proxy::handle_proxy_connect, this, placeholders::error));
}

void Proxy::handle_proxy_connect(const system::error_code& e) {
    // write the 
    if (!e && _socket->is_open()) { // connected successfully to the proxy server

        Socks4::Request req(Socks4::Request::connect, _endpoint, "");
        async_write(*_socket, req.buffers(), bind(&Proxy::handle_write, this, placeholders::error, placeholders::bytes_transferred));
        // this async_read returns only when the buffer has been filled
        async_read(*_socket, _reply.buffers(), bind(&Proxy::handle_proxy_reply, this, placeholders::error, placeholders::bytes_transferred));
    }
    else {
        printf("Failed connect to proxy server: \"%s\" to: %s\n", e.message().c_str(), lexical_cast<string>(_server).c_str());
        _connection_handler(e);
    }

}

void Proxy::handle_write(const system::error_code& e, size_t bytes_transferred) {
    if (!e) {
        // ignore
    }
    else if (e != error::operation_aborted) {
        printf("Proxy write error %s, disconnecting...\n", e.message().c_str());
        // forward the error to the connection handler callback
        _connection_handler(e);
    }
}

void Proxy::handle_proxy_reply(const system::error_code& e, size_t bytes_transferred) {
    if (!e && _reply.success()) {
        // buffers are now filled - check if it was successfull
        _connection_handler(e);
    }
    else if (e != error::operation_aborted) {
        printf("Proxy write error %s, disconnecting...\n", e.message().c_str());
        // forward the error to the connection handler callback
        _connection_handler(e);
    }
    else if (!_reply.success()) {
        printf("Proxy connection error - status code %d...\n", _reply.status());
        system::error_code err(_reply.status(), __application_category);
        _connection_handler(err);
    }
    else 
        _connection_handler(e);        
}
