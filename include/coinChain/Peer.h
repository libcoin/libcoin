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

    inline friend std::ostream& operator<<(std::ostream& os, const Peer& p) {
        /// when NTP implemented, change to just nTime = GetAdjustedTime()
        int64_t nTime = (p.fInbound ? GetAdjustedTime() : UnixTime::s());
        //    Endpoint remote = _socket.remote_endpoint();
        Endpoint local = p._socket.local_endpoint();
        Endpoint addrYou = (p._proxy ? Endpoint("0.0.0.0") : p.addr);
        Endpoint addrMe = (p._proxy ? Endpoint("0.0.0.0") : local);
        // hack to avoid serializing the time
        os << const_binary<int>(p.nVersion) << const_binary<uint64_t>(NODE_NETWORK) << const_binary<int64_t>(nTime);
        os << const_binary<uint64_t>(addrYou.getServices()) << const_binary<boost::asio::ip::address_v6::bytes_type>(addrYou.getIPv6()) << const_binary<unsigned short>(htons(addrYou.port()));
        os << const_binary<uint64_t>(addrMe.getServices()) << const_binary<boost::asio::ip::address_v6::bytes_type>(addrMe.getIPv6()) << const_binary<unsigned short>(htons(addrMe.port()));
        os << const_binary<uint64_t>(p._nonce) << const_varstr(p._sub_version);
        os << const_binary<int>(p._peerManager.getBestHeight());
        os << const_binary<bool>(true);
        return os;
    }
    
    inline friend std::istream& operator>>(std::istream& is, Peer& p) {
        int64_t nTime;
        is >> binary<int>(p.nVersion) >> binary<uint64_t>(p.nServices) >> binary<int64_t>(nTime);
        // the endpoints in a version message is without time:
        
        uint64_t services;
        boost::asio::ip::address_v6::bytes_type ipv6;
        unsigned short port;
        is >> binary<uint64_t>(services) >> binary<boost::asio::ip::address_v6::bytes_type>(ipv6) >> binary<unsigned short>(port);
        
//        v._me = Endpoint(services, ipv6, port);
        
        if (p.nVersion == 10300)
            p.nVersion = 300;
        if (p.nVersion >= 106 && !is.eof()) {
            is >> binary<uint64_t>(services) >> binary<boost::asio::ip::address_v6::bytes_type>(ipv6) >> binary<unsigned short>(port);
//            v_from = Endpoint(services, ipv6, port);
        }
        uint64_t nonce;
        is >> binary<uint64_t>(nonce);
        if (p.nVersion >= 106 && !is.eof())
            is >> varstr(p._sub_version);
        if (p.nVersion >= 209 && !is.eof())
            is >> binary<int>(p._startingHeight);
        if (!is.eof())
            is >> binary<bool>(p.relayTxes); // set to true after we get the first filter* message
        else
            p.relayTxes = true;
        
        if (nonce == p._nonce)
            p.fDisconnect = true;
        else
            p._nonce = nonce;
        
        p.fClient = !(p.nServices & NODE_NETWORK);
        
        AddTimeData(p.addr.getIP(), nTime);
        
        return is;
    }
    
    template<typename Stream>
    void Serialize(Stream& stream, int nType=0, int nVersion=PROTOCOL_VERSION) const
    {
        std::ostringstream os;
        os << (*this);
        std::string s = os.str();
        stream.write((const char*)&s[0], s.size());
    }

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
    
    /// Is this connected through a proxy.
    const bool getProxy() const { return _proxy; }
    
    /// Get the local nonce
    const uint64_t getNonce() const { return _nonce; }
    
    /// Set and record the Peer starting height
    void setStartingHeight(int height) { _peerManager.recordPeerBlockCount(_startingHeight = height); }
    
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
        return fDisconnect;
    }
    
    /// has the peer been initialized ?
    bool initialized() const {
        return (nVersion != 0);
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
    uint64_t nServices;

    Endpoint addr;
    int nVersion;
    std::string strSubVer;
    bool relayTxes;
    bool fClient;
    bool fInbound;
    bool fNetworkNode;
    bool fDisconnect;
    
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
    
    /// Buffer for incoming data.
    //    boost::array<char, 8192> _buffer;
    
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
    
    /// Buffer for incoming data. Should be changed to streams in the future.
    boost::array<char, 65536> _buffer;
    
    static const unsigned int _initial_timeout = 60; // seconds. Initial timeout if no read activity
    static const unsigned int _suicide_timeout = 90*60; // seconds. Suicide timeout if no read activity
    static const unsigned int _heartbeat_timeout = 30*60; // seconds. Heartbeat timeout to show activity
};

#endif // PEER_H
