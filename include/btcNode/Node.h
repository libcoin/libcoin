
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
#include "btcNode/Chain.h"
#include "btcNode/TransactionFilter.h"
#include "btcNode/BlockFilter.h"

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
    explicit Node(const Chain& chain = bitcoin, std::string dataDir = "", const std::string& address = "0.0.0.0", const std::string& port = "0", bool proxy = false, const std::string& irc = "92.243.23.21");
    
    /// Run the server's io_service loop.
    void run();

    /// Shutdown the Node.
    void shutdown() { handle_stop(); }
    
    /// Accept or connect depending on the number and type of the connected peers.
    void post_accept_or_connect();
    
    void post_stop(peer_ptr p);
    
    /// Register a filter
    void installFilter(filter_ptr filter) {
        _messageHandler.installFilter(filter);
    }

    /// get a const handle to the Block Chain
    const BlockChain& blockChain() const { return _blockChain; }

    /// Post a Transaction to the Node. (Note that we use dispatch - no need to post if we are already in the same thread)
    void post(const Transaction& tx) { _io_service.dispatch(boost::bind(&BlockChain::acceptTransaction, &_blockChain, tx)); }
    
    /// Post a Block to the Node.
    //    void post(const Block& blk) { _io_service.post(); }
    
    /// Subscribe to Transaction accept notifications
    void subscribe(TransactionFilter::listener_ptr listener) { static_cast<TransactionFilter*>(_transactionFilter.get())->subscribe(listener); }
    
    /// Subscribe to supply reminders of inventory (could e.g. be for transactions in a wallet)
    void subscribe(TransactionFilter::reminder_ptr reminder) { static_cast<TransactionFilter*>(_transactionFilter.get())->subscribe(reminder); }
    
    /// Subscribe to Block accept notifications
    void subscribe(BlockFilter::listener_ptr listener) { static_cast<BlockFilter*>(_blockFilter.get())->subscribe(listener); }
    
private:
    /// Initiate an asynchronous accept operation.
    void start_accept();
    
    /// Initiate an asynchronous connect operation.
    void start_connect();
    
    /// Accept or connect depending on the number and type of the connected peers.
    void accept_or_connect();
    
    /// Check the deadline timer and give up
    void check_deadline(const boost::system::error_code& e);

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

    /// The data directory holding the block file, the address file, the database and logfiles
    std::string _dataDir;
    
    /// The connection manager which owns all live connections.
    PeerManager _peerManager;
    
    /// The next connection to be connected to or accepted.
    peer_ptr _new_server; // connection from another peer
    peer_ptr _new_client; // connection to another peer
    
    /// Deadline timer for the connect operation
    boost::asio::deadline_timer _connection_deadline;
    
    /// The handler for all incoming messages.
    MessageHandler _messageHandler;
    
    /// The pool of endpoints, from which connections are initiated.
    EndpointPool _endpointPool;
    
    /// The pool of transactions hides the block chain and the transaction database in a simple interface.
    BlockChain _blockChain;
    
    /// The ChatClient to obtain addresses from IRC
    ChatClient _chatClient;
    
    bool _proxy;
    
    filter_ptr _transactionFilter;
    filter_ptr _blockFilter;

    static const unsigned int _max_outbound = 8;
    static const unsigned int _max_inbound = 125-_max_outbound;
    static const unsigned int _connection_timeout = 5; // seconds
};

#endif // NODE_H
