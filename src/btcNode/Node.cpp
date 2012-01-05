
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

Node::Node(const string& address, const string& port, bool proxy, const string& irc) : _io_service(), _signals(_io_service), _acceptor(_io_service), _peerManager(*this), _new_peer(), _deadline(_io_service), _messageHandler(), _endpointPool(), _blockChain(), _chatClient(_io_service, irc, _endpointPool, proxy), _proxy(proxy) {
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
    
    accept_or_connect();
    
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
    _new_peer.reset(new Peer(_io_service, _peerManager, _messageHandler, false, _proxy, _blockChain.getBestHeight())); // false means outbound
    _new_peer->addr = ep;
    // Set a deadline for the connect operation.
    _deadline.expires_from_now(posix_time::seconds(5));
    _new_peer->socket().async_connect(ep, bind(&Node::handle_connect, this, placeholders::error));
    // Start the deadline actor. You will note that we're not setting any
    // particular deadline here. Instead, the connect and input actors will
    // update the deadline prior to each asynchronous operation.
    _deadline.async_wait(bind(&Node::check_deadline, this));
}

void Node::check_deadline()
{
    // Check whether the deadline has passed. We compare the deadline against
    // the current time since a new asynchronous operation may have moved the
    // deadline before this actor had a chance to run.
    if (_deadline.expires_at() <= deadline_timer::traits_type::now()) {
        // The deadline has passed. The socket is closed so that any outstanding
        // asynchronous operations are cancelled.
        printf("Closing socket of: %s\n", _new_peer->addr.toString().c_str());
        _new_peer->socket().close();
        
        // There is no longer an active deadline. The expiry is set to positive
        // infinity so that the actor takes no action until a new deadline is set.
        _deadline.expires_at(posix_time::pos_infin);
    }
    
    // Put the actor back to sleep.
    _deadline.async_wait(bind(&Node::check_deadline, this));
}

void Node::handle_connect(const system::error_code& e) {
    
    if (!e && _new_peer->socket().is_open()) {
        _deadline.expires_at(posix_time::pos_infin);
        _peerManager.start(_new_peer);
    }
    else {
        printf("Failed connect: \"%s\" to: %s\n", e.message().c_str(), _new_peer->addr.toString().c_str());
    }

    _endpointPool.getEndpoint(_new_peer->addr.getKey()).setLastTry(GetAdjustedTime());
        
    printf("Outbound connections are now: %d\n", _peerManager.getNumOutbound()); 

    accept_or_connect();
}

void Node::start_accept() {
    _new_peer.reset(new Peer(_io_service, _peerManager, _messageHandler, true, _proxy, _blockChain.getBestHeight())); // true means inbound
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
    
    accept_or_connect();
}

void Node::accept_or_connect() {
    // first cancel ongoing accepts or connects:
    _acceptor.cancel();
    _new_peer.reset();
    
    // now check if need to connect to yet another peer, or if we just wait in silence for others to connect to us
    if(_peerManager.getNumOutbound() < _max_outbound)
        start_connect();
    else if (_peerManager.getNumInbound() < _max_inbound) // start_accept will not be called again before we get a read/write error on a socket
        start_accept();    
}

void Node::post_accept_or_connect() {
    _io_service.post(bind(&Node::accept_or_connect, this));
}

void Node::handle_stop() {
    // The server is stopped by cancelling all outstanding asynchronous
    // operations. Once all operations have finished the io_service::run() call
    // will exit.
    _acceptor.close();
    _peerManager.stop_all();
}
