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

#ifndef PEER_H
#define PEER_H

#include <coinChain/Export.h>
#include <coinChain/PeerManager.h>
#include <coinChain/MessageHandler.h>
#include <coinChain/Endpoint.h>
#include <coinChain/MessageParser.h>
#include <coinChain/Inventory.h>
#include <coinChain/BloomFilter.h>

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <openssl/rand.h>

// BIP 0031, pong message, is enabled for all versions AFTER this one
static const int BIP0031_VERSION = 60000;

class COINCHAIN_EXPORT Peer : public boost::enable_shared_from_this<Peer>, private boost::noncopyable
{
public:
    /// Construct a peer connection with the given io_service.
    explicit Peer(const Chain& chain, boost::asio::io_service& io_service, PeerManager& manager, MessageHandler& handler, bool inbound, bool proxy, std::string sub_version);

    friend std::ostream& operator<<(std::ostream& os, const Peer& p);
    
    friend std::istream& operator>>(std::istream& is, Peer& p);

    /// Get the socket associated with the peer connection.
    boost::asio::ip::tcp::socket& socket();
    
    /// Start the first asynchronous operation for the peer connection.
    void start();
    
    /// Stop all asynchronous operations associated with the peer connection.
    void stop();
    
    /// Get a list of all Peers from the PeerManager.
    Peers getAllPeers() { return _peerManager.getAllPeers(); }

    /// Flush the various message buffers to the socket.
    void flush();
    
    int version() const { return _version; }
    
    const std::string& sub_version() const { return _sub_version; }
    
    bool client() const { return _client; }
    
    bool inbound() const { return _inbound; }
    
    /// Is this connected through a proxy.
    const bool getProxy() const { return _proxy; }
    
    /// Get the local nonce
    const uint64_t getNonce() const { return _nonce; }
    
    /// Get the Peer starting height.
    int getStartingHeight() const { return _startingHeight; }
    
    /// Successfully connected - notify the PeerManager
    void successfullyConnected();
    
    /// queue inventory request to peer
    void queue(const Inventory& inv);
    
    /// dequeue inventory request - indirectly affecting all peers
    void dequeue(const Inventory& inv);
    
    /// the peer is disconnecting - e.g. due to connection to self
    bool disconnecting() const {
        return _disconnect;
    }
    
    /// has the peer been initialized ?
    bool initialized() const {
        return (_version != 0);
    }
    
private:
    void handle_read(const boost::system::error_code& e, std::size_t bytes_transferred);
    void handle_write(const boost::system::error_code& e, std::size_t bytes_transferred);
    
    /// Handle the 3 stages of a reply
    void reply();
    void trickle();
    void broadcast();

    void check_activity(const boost::system::error_code& e);
    void show_activity(const boost::system::error_code& e);
    
public:
    // socket
    Endpoint addr;
    bool relayTxes;
//    bool fNetworkNode;
    
    BloomFilter filter;
    
public:
    uint256 hashContinue;
    BlockLocator locatorLastGetBlocksBegin;
    uint256 hashLastGetBlocksEnd;
    
    // flood relay
    std::vector<Endpoint> vAddrToSend;
    std::set<Endpoint> setAddrKnown;
    bool fGetAddr;
    std::set<uint256> setKnown;
    
    // inventory based relay
    std::set<Inventory> setInventoryKnown;
    std::vector<Inventory> vInventoryToSend;    
public:
        
    void AddAddressKnown(const Endpoint& addr);
    
    void PushAddress(const Endpoint& addr);
    
    void AddInventoryKnown(const Inventory& inv);
    
    void push(const Inventory& inv);
    void push(const Transaction& inv);
    
    void push(const std::string& command, const std::string& str);
    
    void AskFor(const Inventory& inv);
    
    void PushVersion();
    
    void PushMessage(const char* pszCommand) {
        std::ostringstream os;
        push(pszCommand, os.str());
    }
    
    template<typename T1>
    void PushMessage(const char* pszCommand, const T1& a1) {
        std::ostringstream os;
        os << a1;
        push(pszCommand, os.str());
    }
    
    template<typename T1, typename T2>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2) {
        std::ostringstream os;
        os << a1 << a2;
        push(pszCommand, os.str());
    }

    bool ProcessMessages();
    
    void PushGetBlocks(const BlockLocator locatorBegin, uint256 hashEnd);

private:
    int _version;
    uint64_t _services;
    bool _disconnect;
    bool _client;
    bool _inbound;

    /// The inital height of the connected node.
    int _startingHeight;
    
    bool _successfullyConnected;

    /// The sub version, as announced to the connected peers
    std::string _sub_version;
    
    /// The chain we are serving - the Peer need to know this as well!
    const Chain& _chain; 
    
    /// Socket for the connection to the peer.
    boost::asio::ip::tcp::socket _socket;
    
    /// The manager for this connection.
    PeerManager& _peerManager;
    
    /// The inventory priority queue
    typedef std::multimap<int, Inventory> RequestQueue;
    RequestQueue _queue;
    
    /// The handler delegated from Node.
    MessageHandler& _messageHandler;
    
    /// The message being parsed.
    Message _message;
    
    /// The message parser.
    MessageParser _msgParser;
    
    /// Activity monitoring - if no activity we commit suicide
    bool _activity;
    
    /// Suicide timer
    boost::asio::deadline_timer _suicide;
    
    /// Keep alive timer
    boost::asio::deadline_timer _keep_alive;
    
    /// Use proxy or not - this is just the flag - we need to carry around the address too
    bool _proxy;
    
    /// the local nonce - used to detect connections to self
    uint64_t _nonce;
    
    /// Streambufs for reading and writing
    boost::asio::streambuf _send;
    boost::asio::streambuf _recv;
    
    static const unsigned int _initial_timeout = 60; // seconds. Initial timeout if no read activity
    static const unsigned int _suicide_timeout = 90*60; // seconds. Suicide timeout if no read activity
    static const unsigned int _heartbeat_timeout = 30*60; // seconds. Heartbeat timeout to show activity
};

#endif // PEER_H
