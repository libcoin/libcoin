
#ifndef NODE_H
#define NODE_H

#include <boost/asio.hpp>
#include <string>
#include <boost/noncopyable.hpp>
#include "coinChain/Peer.h"
#include "coinChain/PeerManager.h"
#include "coinChain/MessageHandler.h"
#include "coinChain/BlockChain.h"
#include "coinChain/EndpointPool.h"
#include "coinChain/ChatClient.h"
#include "coinChain/Chain.h"
#include "coinChain/TransactionFilter.h"
#include "coinChain/BlockFilter.h"

#include <boost/interprocess/sync/file_lock.hpp>
#include <fstream>


// Make sure only a single bitcoin process is using the data directory.
// lock the lock file or else throw an exception
class LockFile : boost::noncopyable {
public:
    class Touch : boost::noncopyable {
    public:
        /// Touch is like Posix touch - it touches the file to ensure it exists.
        Touch(std::string lock_file_name) {
            std::fstream f(lock_file_name.c_str(), std::ios_base::app | std::ios_base::in | std::ios_base::out);
            if(!f.is_open())
                throw std::runtime_error(("Cannot open the lockfile ( " + lock_file_name + " ) - do you have permissions?").c_str());
        }
    };
    LockFile(std::string lockfile) : 
    _touch(lockfile), 
    _file_lock(lockfile.c_str()) {
        if( !_file_lock.try_lock())
            throw std::runtime_error( ("Cannot obtain a lock on data directory ( " + lockfile + " ) Bitcoin is probably already running.").c_str());
    }
private:
    Touch _touch;
    boost::interprocess::file_lock _file_lock;
};

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
    /// Construct the node to listen on the specified TCP address and port. Further, connect to IRC (irc.lfnet.org)
    explicit Node(const Chain& chain = bitcoin, std::string dataDir = "", const std::string& address = "0.0.0.0", const std::string& port = "0", bool proxy = false, const std::string& irc = "92.243.23.21");
    
    /// Run the server's io_service loop.
    void run();

    /// Shutdown the Node.
    void shutdown() { _io_service.dispatch(boost::bind(&Node::handle_stop, this)); }
    
    /// Add an endpoint to the endpoool (endpoint or "host:port" versions)
    void addPeer(std::string);
    void addPeer(boost::asio::ip::tcp::endpoint ep);
    
    /// Add an endpoint to the connect list - if this is used the endpointpool is disabled! (endpoint or "host:port" versions)
    void connectPeer(std::string);
    void connectPeer(boost::asio::ip::tcp::endpoint ep);    
    
    typedef std::set<boost::asio::ip::tcp::endpoint> endpoints;
    
    /// Return the number of connections.
    unsigned int getConnectionCount() const {
        return _peerManager.getNumInbound() + _peerManager.getNumOutbound();
    }
    
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
    void post(const Transaction& tx) { _io_service.dispatch(boost::bind(&TransactionFilter::process, static_cast<TransactionFilter*>(_transactionFilter.get()), tx, _peerManager.getAllPeers())); }
    
    /// Post a Block to the Node. (Note that we use dispatch - no need to post if we are already in the same thread)
    void post(const Block& block) { _io_service.dispatch(boost::bind(&BlockFilter::process, static_cast<BlockFilter*>(_blockFilter.get()), block, _peerManager.getAllPeers())); }
        
    /// Subscribe to Transaction accept notifications
    void subscribe(TransactionFilter::listener_ptr listener) { static_cast<TransactionFilter*>(_transactionFilter.get())->subscribe(listener); }
    
    /// Subscribe to supply reminders of inventory (could e.g. be for transactions in a wallet)
    void subscribe(TransactionFilter::reminder_ptr reminder) { static_cast<TransactionFilter*>(_transactionFilter.get())->subscribe(reminder); }
    
    /// Subscribe to Block accept notifications
    void subscribe(BlockFilter::listener_ptr listener) { static_cast<BlockFilter*>(_blockFilter.get())->subscribe(listener); }
    
    /// Get a handle to the io_service.
    boost::asio::io_service& get_io_service() { return _io_service; }
    
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
    
    /// Get a candidate to connect to using the internal list of peers.
    Endpoint getCandidate(const std::set<unsigned int>& not_in);

    /// The data directory holding the block file, the address file, the database and logfiles
    std::string _dataDir;
    
    /// The lock file
    LockFile _fileLock;
    
    /// The io_service used to perform asynchronous operations.
    boost::asio::io_service _io_service;
    
    /// Acceptor used to listen for incoming connections.
    boost::asio::ip::tcp::acceptor _acceptor;

    /// The connection manager which owns all live connections.
    PeerManager _peerManager;
    
    /// The next connection to be connected to or accepted.
    peer_ptr _new_server; // connection from another peer
    peer_ptr _new_client; // connection to another peer
    
    /// Deadline timer for the connect operation
    boost::asio::deadline_timer _connection_deadline;
    
    /// list of endpoints that overrules the endpointPool database.
    endpoints _connection_list;
    
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
