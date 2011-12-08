
#ifndef HTTP_CONNECTION_HPP
#define HTTP_CONNECTION_HPP

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "btcHTTP/Reply.h"
#include "btcHTTP/Request.h"
#include "btcHTTP/RequestHandler.h"
#include "btcHTTP/RequestParser.h"

class ConnectionManager;

/// Represents a single connection from a client.
class Connection : public boost::enable_shared_from_this<Connection>, private boost::noncopyable
{
public:
    /// Construct a connection with the given io_service.
    explicit Connection(boost::asio::io_service& io_service, ConnectionManager& manager, RequestHandler& handler);
    
    /// Get the socket associated with the connection.
    boost::asio::ip::tcp::socket& socket();
    
    /// Start the first asynchronous operation for the connection.
    void start();
    
    /// Stop all asynchronous operations associated with the connection.
    void stop();
    
private:
    /// Handle completion of a read operation.
    void handle_read(const boost::system::error_code& e,
                     std::size_t bytes_transferred);
    
    /// Handle completion of a write operation.
    void handle_write(const boost::system::error_code& e);
    
    /// Socket for the connection.
    boost::asio::ip::tcp::socket _socket;
    
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
