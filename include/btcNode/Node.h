
#ifndef NODE_H
#define NODE_H

#include <boost/asio.hpp>
#include <string>
#include <boost/noncopyable.hpp>
#include "btcNode/Peer.h"
#include "btcNode/PeerManager.h"
#include "btcNode/MessageHandler.h"
#include "btcNode/BlockChain.h"
#include "btcNode/EndpointPool.h"
#include "btcNode/ChatClient.h"

/// The top-level class of the btc Node.
/// Node keeps a list of Peers and accepts and initiates connectes using a list of endpoints
/// To bootstrap the process a connection to IRC is made - or a lookup in the existing endpoints is used
/// Initial implementation relies on the existing endpoints in the endpoint (address) database
/// Node _has_ an EndpointPool (a sealed interface to a database with cache)
/// Open a listen port and initiate connections to 8 other peers - the peers should know if they are clients or servers...
/// Node also _has_ a TransactionPool which is a sealed interface to transactions and blocks

/// NodeOptions... - like testnet etc...

class Node : private boost::noncopyable
{
public:
    /// Construct the server to listen on the specified TCP address and port. Further, connect to IRC (irc.lfnet.org)
    explicit Node(const std::string& address, const std::string& port, bool proxy = false, const std::string& irc = "92.243.23.21");
    
    /// Run the server's io_service loop.
    void run();

    /// Shutdown the Node.
    void shutdown() { handle_stop(); }
    
    /// Accept or connect depending on the number and type of the connected peers.
    void post_accept_or_connect();
    
    /// Register a filter
    void installFilter(filter_ptr filter) {
        _messageHandler.installFilter(filter);
    }

    /// get a handle to the Block Chain
    BlockChain& blockChain() { return _blockChain; }
    
private:
    /// Initiate an asynchronous accept operation.
    void start_accept();
    
    /// Initiate an asynchronous connect operation.
    void start_connect();
    
    /// Accept or connect depending on the number and type of the connected peers.
    void accept_or_connect();
    
    /// Check the deadline timer and give up
    void check_deadline();
    
    /// Handle completion of an asynchronous accept operation.
    void handle_accept(const boost::system::error_code& e);
    
    /// Handle completion of an asynchronous connect operation.
    void handle_connect(const boost::system::error_code& e);
    
    /// Handle a request to stop the server.
    void handle_stop();
    
    /// The io_service used to perform asynchronous operations.
    boost::asio::io_service _io_service;
    
    /// The signal_set is used to register for process termination notifications.
    boost::asio::signal_set _signals;
    
    /// Acceptor used to listen for incoming connections.
    boost::asio::ip::tcp::acceptor _acceptor;
    
    /// The connection manager which owns all live connections.
    PeerManager _peerManager;
    
    /// The next connection to be connected to or accepted.
    peer_ptr _new_peer;
    
    /// Deadline timer for the connect operation
    boost::asio::deadline_timer _deadline;
    
    /// The handler for all incoming messages.
    MessageHandler _messageHandler;
    
    /// The pool of endpoints, from which connections are initiated.
    EndpointPool _endpointPool;
    
    /// The pool of transactions hides the block chain and the transaction database in a simple interface.
    BlockChain _blockChain;
    
    /// The ChatClient to obtain addresses from IRC
    ChatClient _chatClient;
    
    bool _proxy;

    static const unsigned int _max_outbound = 8;
    static const unsigned int _max_inbound = 125-_max_outbound;
};

#endif // NODE_H
