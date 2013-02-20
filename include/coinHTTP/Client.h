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

#include <coinHTTP/Export.h>
#include <coinHTTP/Reply.h>

#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/asio/ssl.hpp>

class COINHTTP_EXPORT ClientCompletionHandler {
public:
    virtual void operator()(const boost::system::error_code& err) = 0;
};

class COINHTTP_EXPORT Client : public ClientCompletionHandler
{
public:
    Client();
    Client(boost::asio::io_service& io_service);

    void setSecure(bool secure) { _secure = secure; }
    bool isSecure() const { return _secure; }
    
    virtual void operator()(const boost::system::error_code& err) { _error = err; }
    
    /// Blocking post.
    Reply post(std::string url, std::string content, Headers headers = Headers()) {
        async_post(url, content, *this, headers);
        _io_service.run();
        if(_error)
            _reply.content() += _error.message();
        // parse the reply
        return _reply;
    }
    
    /// Non-blocking post. Requires definition of a callback.
    void async_post(std::string url, std::string content, ClientCompletionHandler& handler, Headers headers = Headers());
    
    /// Blocking get.
    Reply get(std::string url, Headers headers = Headers()) {
        async_get(url, *this, headers);
        _io_service.run();
        // parse the reply
        return _reply;
    }
    
    /// Non-blocking get. Requires definition of a callback.
    void async_get(std::string url, ClientCompletionHandler& handler, Headers headers = Headers());
    

private:
    void handle_resolve(const boost::system::error_code& err, boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
    void handle_connect(const boost::system::error_code& err, boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
    void handle_handshake(const boost::system::error_code& error);
    void handle_write_request(const boost::system::error_code& err);
    void handle_read_status_line(const boost::system::error_code& err, std::size_t bytes_transferred);
    void handle_read_headers(const boost::system::error_code& err, std::size_t bytes_transferred);
    void handle_read_content(const boost::system::error_code& err, std::size_t bytes_transferred);

private:
    boost::asio::io_service _io_service;
    boost::asio::ssl::context _context;
    bool _secure;
    boost::asio::ip::tcp::resolver _resolver;
    boost::asio::ip::tcp::socket _socket;
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> _ssl_socket;
    boost::asio::streambuf _request;
    boost::asio::streambuf _response;
    Reply _reply;
    ClientCompletionHandler& _completion_handler;
    boost::system::error_code _error;
};