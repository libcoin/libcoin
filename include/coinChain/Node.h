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

#ifndef NODE_H
#define NODE_H

#include <boost/asio.hpp>
#include <string>
#include <boost/noncopyable.hpp>

#include <coinChain/Export.h>
#include <coinChain/Peer.h>
#include <coinChain/PeerManager.h>
#include <coinChain/Proxy.h>
#include <coinChain/MessageHandler.h>
#include <coinChain/BlockChain.h>
#include <coinChain/EndpointPool.h>
#include <coinChain/ChatClient.h>
#include <coinChain/Chain.h>
#include <coinChain/TransactionFilter.h>
#include <coinChain/BlockFilter.h>

#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/filesystem.hpp>
#include <fstream>


// Make sure only a single bitcoin process is using the data directory.
// lock the lock file or else throw an exception
class COINCHAIN_EXPORT FileLock : boost::noncopyable {
public:
    class Touch : boost::noncopyable {
    public:
        /// Touch is like Posix touch - it touches the file to ensure it exists.
        Touch(std::string lock_file_name) {
            if (lock_file_name.size() == 0) return;
            boost::filesystem::create_directories(boost::filesystem::path(lock_file_name).parent_path());
            std::fstream f(lock_file_name.c_str(), std::ios_base::app | std::ios_base::in | std::ios_base::out);
            if(!f.is_open())
                throw std::runtime_error(("Cannot open the lockfile ( " + lock_file_name + " ) - do you have permissions?").c_str());
        }
    };
    FileLock(std::string lockfile) : 
    _touch(lockfile), 
    _file_lock(lockfile.c_str()) {
        if (lockfile.size() == 0) return;
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

class COINCHAIN_EXPORT Node : private boost::noncopyable
{
public:
    /// Construct the node to listen on the specified TCP address and port. Further, connect to IRC (irc.lfnet.org)
    explicit Node(const Chain& chain = bitcoin, std::string dataDir = "", const std::string& address = "0.0.0.0", const std::string& port = "0", boost::asio::ip::tcp::endpoint proxy = boost::asio::ip::tcp::endpoint(), unsigned int timeout = 5000, const std::string& irc = "92.243.23.21");
    
    /// Read an old style block file - this is for rapid initialization.
    void readBlockFile(std::string path, int fileno = 1);
    
    /// Run the server's io_service loop.
    void run();

    /// Shutdown the Node.
    void shutdown() { _io_service.dispatch(boost::bind(&Node::handle_stop, this)); }
    
    /// Set the client name and optionally version
    void setClientVersion(std::string name, std::vector<std::string> comments = std::vector<std::string>(), int client_version = LIBRARY_VERSION);

    /// Get the client version aka subversion as specified in BIP-0014
    int getClientVersion() const; 
    
    std::string getFullClientVersion() const;

    /// Add an endpoint to the endpool (endpoint or "host:port" versions)
    void addPeer(std::string);
    void addPeer(boost::asio::ip::tcp::endpoint ep);
    
    /// Add an endpoint to the connect list - if this is used the endpointpool is disabled! (endpoint or "host:port" versions)
    void connectPeer(std::string);
    void connectPeer(boost::asio::ip::tcp::endpoint ep);    
    
    /// Update the peer list - note all existing peers will be deleted!
    void updatePeers(const std::vector<std::string>& eps);
    
    typedef std::set<boost::asio::ip::tcp::endpoint> endpoints;
    
    /// Return the number of connections.
    unsigned int getConnectionCount() const {
        return _peerManager.getNumInbound() + _peerManager.getNumOutbound();
    }
    
    /// Proxy to get the median block countof the last five connected peers.
    int getPeerMedianNumBlocks() const { return _peerManager.getPeerMedianNumBlocks(); }
    
    /// Accept or connect depending on the number and type of the connected peers.
    void post_accept_or_connect();
    
    void post_stop(peer_ptr p);

    void post_ready(peer_ptr p);
    
    /// Register a filter
    void installFilter(filter_ptr filter) {
        _messageHandler.installFilter(filter);
    }

    /// get a const handle to the Block Chain
    const BlockChain& blockChain() const { return _blockChain; }
    
    /// get the penetration among connected peers of a tx hash - the penetration percentage is the peerPenetration divided by the connection count
    int peerPenetration(const uint256 hash) const;

    /// Post a Transaction to n peers. (Note that we use dispatch - no need to post if we are already in the same thread)
    void post(const Transaction& tx, size_t n = 0);
        
    /// Post a Block to the Node. (Note that we use dispatch - no need to post if we are already in the same thread)
    void post(const Block& block, bool relay = true) {
        uint256 hash = block.getHash();

        Peers peers;
        if (relay)
            peers = _peerManager.getAllPeers();

        if (_blockChain.chain().checkProofOfWork(block))
            _io_service.dispatch(boost::bind(&BlockFilter::process, static_cast<BlockFilter*>(_blockFilter.get()), block, peers));
            else if (hash < block.getShareTarget() )
                _io_service.dispatch(boost::bind(&ShareFilter::process, static_cast<ShareFilter*>(_shareFilter.get()), block, peers));
    }
        
    /// Subscribe to Transaction accept notifications
    void subscribe(TransactionFilter::listener_ptr listener) { static_cast<TransactionFilter*>(_transactionFilter.get())->subscribe(listener); }
    
    /// Subscribe to supply reminders of inventory (could e.g. be for transactions in a wallet)
    void subscribe(TransactionFilter::reminder_ptr reminder) { static_cast<TransactionFilter*>(_transactionFilter.get())->subscribe(reminder); }
    
    /// Subscribe to Block accept notifications
    void subscribe(BlockFilter::listener_ptr listener) { static_cast<BlockFilter*>(_blockFilter.get())->subscribe(listener); }
    
    /// Get a handle to the io_service.
    boost::asio::io_service& get_io_service() { return _io_service; }
    
    /// BlockChain Modes    
    enum Strictness {
        FULL,
        LAST_CHECKPOINT,
        MINIMAL,
        LAZY,
        NONE
    };
    
    Strictness verification() const { return _verification; }
    void verification(Strictness v);
    
    Strictness validation() const { return _validation; }
    void validation(Strictness v);
    
    Strictness persistence() const { return _persistence; }
    void persistence(Strictness v);

    bool searchable() const { return _blockChain.script_to_unspents(); }
    void searchable(bool s) { _blockChain.script_to_unspents(s); }
    
private:
    /// Initiate an asynchronous accept operation.
    void start_accept();
    
    /// Initiate an asynchronous connect operation.
    void start_connect();
    
    /// Accept or connect depending on the number and type of the connected peers.
    void accept_or_connect();
    
    /// Peer ready means that a new peer is connected and has completed the first version handshake
    void peer_ready(peer_ptr p);
    
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

    void update_verification();
    void update_validation();
    void update_persistence();
    
    /// The data directory holding the block file, the address file, the database and logfiles
    std::string _dataDir;
    
    /// The lock file
    FileLock _fileLock;
    
    /// The io_service used to perform asynchronous operations.
    boost::asio::io_service _io_service;
    
    /// Acceptor used to listen for incoming connections.
    boost::asio::ip::tcp::acceptor _acceptor;

    /// The pool of transactions hides the block chain and the transaction database in a simple interface.
    BlockChain _blockChain;
    
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
    
    /// The ChatClient to obtain addresses from IRC
    ChatClient _chatClient;
    
    Proxy _proxy;
    
    filter_ptr _transactionFilter;
    filter_ptr _blockFilter;
    filter_ptr _shareFilter;

    unsigned int _connection_timeout; // seconds

    static const unsigned int _max_outbound = 8;
    static const unsigned int _max_inbound = 125-_max_outbound;
    
    /// Version info
    std::string _client_name;
    std::vector<std::string> _client_comments;
    int _client_version;
        
    /// Strictness modes
    Strictness _verification;
    Strictness _validation;
    Strictness _persistence;
};

#endif // NODE_H
