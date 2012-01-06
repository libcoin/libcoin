
#include "btcNode/Node.h"
#include "btcNode/PeerManager.h"
#include "btcNode/VersionFilter.h"
#include "btcNode/EndpointFilter.h"
#include "btcNode/BlockFilter.h"
#include "btcNode/TransactionFilter.h"
#include "btcNode/AlertFilter.h"
#include <boost/bind.hpp>
#include <signal.h>

using namespace std;
using namespace boost;
using namespace asio;

Node::Node(const string& address, const string& port, bool proxy, const string& irc) : _io_service(), _signals(_io_service), _acceptor(_io_service), _peerManager(*this), _connection_deadline(_io_service), _messageHandler(), _endpointPool(), _blockChain(), _chatClient(_io_service, irc, _endpointPool, proxy), _proxy(proxy) {
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
}

void Node::run() {
    // The io_service::run() call will block until all asynchronous operations
    // have finished. While the server is running, there is always at least one
    // asynchronous operation outstanding: the asynchronous accept call waiting
    // for new incoming connections.
    
    start_accept();
    start_connect();
    
    // Install filters for teh messages. First inserted filters are executed first.
    _messageHandler.installFilter(filter_ptr(new VersionFilter));
    _messageHandler.installFilter(filter_ptr(new EndpointFilter(_endpointPool)));
    _messageHandler.installFilter(filter_ptr(new BlockFilter(_blockChain)));
    _messageHandler.installFilter(filter_ptr(new TransactionFilter(_blockChain)));
    _messageHandler.installFilter(filter_ptr(new AlertFilter)); // this only output the alert to stdout
    
    _io_service.run();
}

void Node::start_connect() {
    // we connect to 8 peers, but we take one by one and only connect to new peers if we have less than 8 connections.
    set<unsigned int> not_in = _peerManager.getPeerIPList();
    Endpoint ep = _endpointPool.getCandidate(not_in, 0);
    stringstream ss;
    ss << ep;
    printf("Trying connect to: %s\n", ss.str().c_str());
    _new_server.reset(new Peer(_io_service, _peerManager, _messageHandler, false, _proxy, _blockChain.getBestHeight())); // false means outbound
    _new_server->addr = ep;
    // Set a deadline for the connect operation.
    _connection_deadline.expires_from_now(posix_time::seconds(_connection_timeout));
    _new_server->socket().async_connect(ep, bind(&Node::handle_connect, this, placeholders::error));
    // start wait for deadline to expire.
    _connection_deadline.async_wait(bind(&Node::check_deadline, this, placeholders::error));
}

void Node::check_deadline(const boost::system::error_code& e) {
    if(!e) {
        // Check whether the deadline has passed. We compare the deadline against
        // the current time.
        if (_connection_deadline.expires_at() <= deadline_timer::traits_type::now()) {
            // The deadline has passed. The socket is closed so that any outstanding
            // asynchronous operations are cancelled.
            printf("Closing socket of: %s\n", _new_server->addr.toString().c_str());
            _new_server->socket().close();
            
            // There is no longer an active deadline. The expiry is set to positive
            // infinity so that the actor takes no action until a new deadline is set.
            //_connection_deadline.expires_at(posix_time::pos_infin);
        }        
    }
    else if (e != error::operation_aborted) {
        printf("Boost deadline timer error in Node: %s\n", e.message().c_str());
    }
}

void Node::handle_connect(const system::error_code& e) {    
    _connection_deadline.cancel(); // cancel the deadline timer //expires_at(posix_time::pos_infin);
    
    if (!e && _new_server->socket().is_open()) {
        _peerManager.start(_new_server);
    }
    else {
        printf("Failed connect: \"%s\" to: %s\n", e.message().c_str(), _new_server->addr.toString().c_str());
    }

    _endpointPool.getEndpoint(_new_server->addr.getKey()).setLastTry(GetAdjustedTime());
    
    _new_server.reset();    
    accept_or_connect();
}

void Node::start_accept() {
    _new_client.reset(new Peer(_io_service, _peerManager, _messageHandler, true, _proxy, _blockChain.getBestHeight())); // true means inbound
    _acceptor.async_accept(_new_client->socket(), bind(&Node::handle_accept, this, placeholders::error));
}

void Node::handle_accept(const system::error_code& e) {
    // Check whether the server was stopped by a signal before this completion
    // handler had a chance to run.
    if (!_acceptor.is_open()) {
        return;
    }
    
    if (!e) {
        _peerManager.start(_new_client);
    }

    _new_client.reset(); // this enables us to test if we have pending accepts
    
    accept_or_connect();
}

void Node::accept_or_connect() {
    // only start a connect or accept if we have not one pending already
    if (!_new_client) {
        printf("Inbound connections are now: %d\n", _peerManager.getNumInbound()); 
        
        if (_peerManager.getNumInbound() < _max_inbound) // start_accept will not be called again before we get a read/write error on a socket
            start_accept();         
    }
        
    if (!_new_server) {
        printf("Outbound connections are now: %d\n", _peerManager.getNumOutbound()); 
        
        if (_peerManager.getNumOutbound() < _max_outbound) // start_accept will not be called again before we get a read/write error on a socket
            start_connect();         
    }
}

void Node::post_accept_or_connect() {
    _io_service.post(bind(&Node::accept_or_connect, this));
}

void Node::post_stop(peer_ptr p) {
    _io_service.post(bind(&PeerManager::stop, &_peerManager, p));    
}

void Node::handle_stop() {
    // The server is stopped by cancelling all outstanding asynchronous
    // operations. Once all operations have finished the io_service::run() call
    // will exit.
    _acceptor.close();
    _peerManager.stop_all();
}
