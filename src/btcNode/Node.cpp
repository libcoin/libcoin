#ifdef _LIBBTC_ASIO_

#include "btcNode/Node.h"
#include <boost/bind.hpp>
#include <signal.h>

using namespace std;
using namespace boost;
using namespace asio;

Node::Node(const string& address, const string& port, const string& irc) : _io_service(), _signals(_io_service), _acceptor(_io_service), _peerManager(), _new_peer(), _messageHandler(), _endpointPool(), _blockChain(), _chatClient(_io_service, irc, _endpointPool) {
    // Register to handle the signals that indicate when the node should exit.
    // It is safe to register for the same signal multiple times in a program,
    // provided all registration for the specified signal is made through Asio.
    _signals.add(SIGINT);
    _signals.add(SIGTERM);
#if defined(SIGQUIT)
    _signals.add(SIGQUIT);
#endif // defined(SIGQUIT)
    _signals.async_wait(bind(&Node::handle_stop, this));
    
    // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
    ip::tcp::resolver resolver(_io_service);
    ip::tcp::resolver::query query(address, port);
    ip::tcp::endpoint endpoint = *resolver.resolve(query);
    _acceptor.open(endpoint.protocol());
    _acceptor.set_option(ip::tcp::acceptor::reuse_address(true));
    _acceptor.bind(endpoint);
    _acceptor.listen();
    
    start_connect();
}

void Node::run() {
    // The io_service::run() call will block until all asynchronous operations
    // have finished. While the server is running, there is always at least one
    // asynchronous operation outstanding: the asynchronous accept call waiting
    // for new incoming connections.
    
    _blockChain.load();
    
    _messageHandler.installFilter(filter_ptr(new VersionFilter));
    _messageHandler.installFilter(filter_ptr(new EndpointFilter(_endpointPool)));
    _messageHandler.installFilter(filter_ptr(new BlockFilter(_blockChain)));
    _messageHandler.installFilter(filter_ptr(new TransactionFilter(_blockChain)));
    _messageHandler.installFilter(filter_ptr(new AlertFilter)); // this only output the alert to stdout
    
    _io_service.run();
}

void Node::start_connect() {
    // we connect to 8 peers, but we take one by one and only connect to new peers if we have less than 8 connections.
    set<unsigned int> not_in = _peerManager.getPeerList();
    Endpoint ep = _endpointPool.getCandidate(not_in, 0);
    
    _new_peer.reset(new Peer(_io_service, _peerManager, _messageHandler, false)); // false means outbound
    _new_peer->socket().async_connect(ep, bind(&Node::handle_connect, this, placeholders::error));
}

void Node::handle_connect(const system::error_code& e) {
    
    if (!e) {
        _peerManager.start(_new_peer);
    }
    
    // now check if need to connect to yet another peer, or if we just wait in silence for others to connect to us
    if(_peerManager.getNumOutbound() < _max_outbound)
        start_connect();
    else if (_peerManager.getNumInbound() < _max_inbound) // start_accept will not be called again before we get a read/write error on a socket
        start_accept();
}

void Node::start_accept() {
    _new_peer.reset(new Peer(_io_service, _peerManager, _messageHandler, true)); // true means inbound
    _acceptor.async_accept(_new_peer->socket(), bind(&Node::handle_accept, this, placeholders::error));
}

void Node::handle_accept(const system::error_code& e) {
    // Check whether the server was stopped by a signal before this completion
    // handler had a chance to run.
    if (!_acceptor.is_open()) {
        return;
    }
    
    if (!e) {
        _peerManager.start(_new_peer);
    }
    
    // now check if need to connect to yet another peer, or if we just wait in silence for others to connect to us
    if(_peerManager.getNumOutbound() < _max_outbound)
        start_connect();
    else if (_peerManager.getNumInbound() < _max_inbound) // start_accept will not be called again before we get a read/write error on a socket
        start_accept();
}

void Node::handle_stop() {
    // The server is stopped by cancelling all outstanding asynchronous
    // operations. Once all operations have finished the io_service::run() call
    // will exit.
    _acceptor.close();
    _peerManager.stop_all();
}
#endif