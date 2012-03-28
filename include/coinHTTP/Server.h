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

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <coinHTTP/Export.h>
#include <coinHTTP/Connection.h>
#include <coinHTTP/ConnectionManager.h>
#include <coinHTTP/RequestHandler.h>

#include <string>
#include <fstream>

#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio/ssl.hpp>

#include <boost/date_time/posix_time/posix_time_io.hpp>

// This is a backup solution for boost prior to 1.47
// We only use the signal_set in Server, and hence it can be done in this simple and more "ugly" way.
// If you e.g. need to start multiple Servers, please upgrade your boost installation !
#ifndef BOOST_ASIO_SIGNAL_SET_HPP
#include <csignal>
#include <boost/function.hpp>
#include <boost/bind.hpp>
class boost__asio__signal_set;
extern boost__asio__signal_set* __signal_set;
class boost__asio__signal_set {
public:
    static void handler(int sig) {
        if(__signal_set)
            __signal_set->handle(sig);
    }
    boost__asio__signal_set(boost::asio::io_service& io_service) : _io_service(io_service) {
        __signal_set = this;
    }
    void add(const int sig) {
        typedef void (*h)(int);
        h ret = std::signal(sig, &handler);
        if (ret == SIG_ERR) {
            //            printf("signal_set: registered signal unsuccessfull\n");
        }
        //        printf("signal_set: registered signal: %d \n", sig);
    }
    void handle(int sig) {
        //        printf("signal_set: received signal: %d \n", sig);
        _io_service.post(_signal_handler);
    }
    void async_wait(boost::function<void (void)> signal_handler) {
        //        printf("signal_set: added handler (async_wait)");
        _signal_handler = signal_handler;
    }
private:
    boost::asio::io_service& _io_service;
    boost::function<void (void)> _signal_handler;
};

#else

typedef boost::asio::signal_set boost__asio__signal_set;

#endif

class COINHTTP_EXPORT Logger 
{
public:
    Logger(std::string dir) : _access(std::cout.rdbuf()) {
        if(dir.size()) {
            _access_file.open((dir + "/access_log").c_str(), std::ios::app); 
            _access.rdbuf(_access_file.rdbuf());
        }
        
        boost::posix_time::time_facet* facet = new boost::posix_time::time_facet("[%d/%b/%Y:%H:%M:%S %q]");
        _access.imbue(std::locale(_access.getloc(), facet));

        _access << std::endl << std::endl << std::endl;
        _access << "Logger started" << std::endl;
    }
    std::ostream& access() { return _access; }
private:
    std::ofstream _access_file;
    std::ostream _access; 
};

/// The top-level class of the HTTP server.
class COINHTTP_EXPORT Server : private boost::noncopyable
{
public:
    /// Construct the server to listen on the specified TCP address and port, and
    /// serve up files from the given directory.
    /// doc_root can alternatively be set to an in memory html document, then this is the only document that will be returned.
    explicit Server(const std::string address = boost::asio::ip::address_v4::loopback().to_string(), const std::string port = "8333", const std::string doc_root = "", const std::string log_dir = "");

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
    boost__asio__signal_set _signals;
    
    /// Acceptor used to listen for incoming connections.
    boost::asio::ip::tcp::acceptor _acceptor;
    
    /// The connection manager which owns all live connections.
    ConnectionManager _connectionManager;
    
    /// The next connection to be accepted.
    connection_ptr _new_connection;
    
    /// The handler for all incoming requests.
    RequestHandler _requestHandler;
    
    /// The logger - currently logging only access, but will later log also e.g. errors
    Logger _logger;
};

#endif // HTTP_SERVER_H
