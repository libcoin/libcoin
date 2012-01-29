
#ifndef HTTP_CONNECTION_HPP
#define HTTP_CONNECTION_HPP

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
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

#include <boost/enable_shared_from_this.hpp>
#include <boost/asio/ssl.hpp>
#include "coinHTTP/Reply.h"
#include "coinHTTP/Request.h"
#include "coinHTTP/RequestHandler.h"
#include "coinHTTP/RequestParser.h"

class ConnectionManager;

typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket;

/// Represents a single connection from a client.
class Connection : public boost::enable_shared_from_this<Connection>, private boost::noncopyable
{
public:
    /// Construct a connection with the given io_service.
    explicit Connection(boost::asio::io_service& io_service, ConnectionManager& manager, RequestHandler& handler);
    
    /// Construct a secure connection with the given io_service and ssl context.
    explicit Connection(boost::asio::io_service& io_service, boost::asio::ssl::context& context, ConnectionManager& manager, RequestHandler& handler);
    
    /// Get the socket associated with the connection.
    boost::asio::ip::tcp::socket& socket();
    
    /// Start the first asynchronous operation for the connection.
    void start();
    
    /// Stop all asynchronous operations associated with the connection.
    void stop();
    
private:
    /// Secure connectiontions need to perform a handshake first.
    virtual void handle_handshake(const boost::system::error_code& error);
    
    /// Handle completion of a read operation.
    void handle_read(const boost::system::error_code& e,
                             std::size_t bytes_transferred);
    
    /// Handle completion of a write operation.
    void handle_write(const boost::system::error_code& e);
    
    /// Dummy context to enable initialization of ghost ssl socket
    boost::asio::ssl::context _ctx;
    
    /// Socket for the connection.
    boost::asio::ip::tcp::socket _socket;    
    
    /// Socket for the connection.
    ssl_socket _ssl_socket;
    
    /// Flag to determine if we are running secure
    bool _secure;

    /// The manager for this connection.
    ConnectionManager& _connectionManager;
    
    /// The handler used to process the incoming request.
    RequestHandler& _requestHandler;
    
    /// Buffer for incoming data.
    boost::array<char, 8192> _buffer;
    
    /// The incoming request.
    Request _request;
    
    /// The parser for the incoming request.
    RequestParser _requestParser;
    
    /// The reply to be sent back to the client.
    Reply _reply;
};

typedef boost::shared_ptr<Connection> connection_ptr;

#endif // HTTP_CONNECTION_H
