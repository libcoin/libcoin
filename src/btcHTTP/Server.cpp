
#include "btcHTTP/Server.h"
#include <boost/bind.hpp>
#include <signal.h>

using namespace std;
using namespace boost;
using namespace asio;

Server::Server(const string& address, const string& port, const string& doc_root) : _io_service(), _signals(_io_service), _acceptor(_io_service), _connectionManager(), _new_connection(), _requestHandler(doc_root) {
    // Register to handle the signals that indicate when the server should exit.
    // It is safe to register for the same signal multiple times in a program,
    // provided all registration for the specified signal is made through Asio.
    _signals.add(SIGINT);
    _signals.add(SIGTERM);
#if defined(SIGQUIT)
    _signals.add(SIGQUIT);
#endif // defined(SIGQUIT)
    _signals.async_wait(bind(&Server::handle_stop, this));
    
    // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
    ip::tcp::resolver resolver(_io_service);
    ip::tcp::resolver::query query(address, port);
    ip::tcp::endpoint endpoint = *resolver.resolve(query);
    _acceptor.open(endpoint.protocol());
    _acceptor.set_option(ip::tcp::acceptor::reuse_address(true));
    _acceptor.bind(endpoint);
    _acceptor.listen();
    
    start_accept();
}

void Server::run() {
    // The io_service::run() call will block until all asynchronous operations
    // have finished. While the server is running, there is always at least one
    // asynchronous operation outstanding: the asynchronous accept call waiting
    // for new incoming connections.
    _io_service.run();
}

void Server::start_accept() {
    _new_connection.reset(new Connection(_io_service, _connectionManager, _requestHandler));
    _acceptor.async_accept(_new_connection->socket(), bind(&Server::handle_accept, this, placeholders::error));
}

void Server::handle_accept(const system::error_code& e) {
    // Check whether the server was stopped by a signal before this completion
    // handler had a chance to run.
    if (!_acceptor.is_open()) {
        return;
    }
    
    if (!e) {
        _connectionManager.start(_new_connection);
    }
    
    start_accept();
}

void Server::handle_stop() {
    // The server is stopped by cancelling all outstanding asynchronous
    // operations. Once all operations have finished the io_service::run() call
    // will exit.
    _acceptor.close();
    _connectionManager.stop_all();
}
