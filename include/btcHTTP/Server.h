
#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <boost/asio.hpp>
#include <string>
#include "btcHTTP/Connection.h"
#include "btcHTTP/ConnectionManager.h"
#include "btcHTTP/RequestHandler.h"

#include <boost/noncopyable.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio/ssl.hpp>

/// The top-level class of the HTTP server.
class Server : private boost::noncopyable
{
public:
    /// Construct the server to listen on the specified TCP address and port, and
    /// serve up files from the given directory.
    explicit Server(const std::string address = boost::asio::ip::address_v4::loopback().to_string(), const std::string port = "8333", const std::string doc_root = boost::filesystem::initial_path().string());

    /// Set the server credentials - this will also make the server secure.
    void setCredentials(const std::string dataDir, const std::string cert = "hostcert.pem", const std::string key = "hostkey.pem");
    
    bool isSecure() const {
        return _secure;
    }
    
    /// Run the server's io_service loop.
    void run();

    /// Shutdown the Server.
    void shutdown();
    
    /// Register an application handler, e.g. for RPC og CGI
    void registerMethod(method_ptr method) {
        Auth auth;
        _requestHandler.registerMethod(method, auth);
    }
    void registerMethod(method_ptr method, Auth auth) {
        _requestHandler.registerMethod(method, auth);
    }

    /// Remove an application handler, e.g. for RPC og CGI
    void unregisterMethod(const std::string name) {
        _requestHandler.unregisterMethod(name);
    }

    /// Get a handle to the io_service used by the Server
    boost::asio::io_service& get_io_service() { return _io_service; }
    
protected:
    /// Initiate an asynchronous accept operation.
    virtual void start_accept();
    
    /// Handle completion of an asynchronous accept operation.
    void handle_accept(const boost::system::error_code& e);
    
    /// Handle a request to stop the server.
    void handle_stop();
    
    /// The io_service used to perform asynchronous operations.
    boost::asio::io_service _io_service;
    
    /// The TLS context. - we only use this if we run the server using ssl.
    boost::asio::ssl::context _context;
    
    /// Setting to determine if we want to be secure or not
    bool _secure;

    /// The signal_set is used to register for process termination notifications.
    boost::asio::signal_set _signals;
    
    /// Acceptor used to listen for incoming connections.
    boost::asio::ip::tcp::acceptor _acceptor;
    
    /// The connection manager which owns all live connections.
    ConnectionManager _connectionManager;
    
    /// The next connection to be accepted.
    connection_ptr _new_connection;
    
    /// The handler for all incoming requests.
    RequestHandler _requestHandler;
};

#endif // HTTP_SERVER_H
