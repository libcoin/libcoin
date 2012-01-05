
#ifndef PEER_H
#define PEER_H

#include "btcNode/PeerManager.h"
#include "btcNode/MessageHandler.h"
#include "btcNode/Endpoint.h"
#include "btcNode/MessageParser.h"
#include "btcNode/BlockIndex.h"
#include "btcNode/Inventory.h"

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <openssl/rand.h>

class CRequestTracker
{
public:
    void (*fn)(void*, CDataStream&);
    void* param1;
    
    explicit CRequestTracker(void (*fnIn)(void*, CDataStream&)=NULL, void* param1In=NULL)
    {
    fn = fnIn;
    param1 = param1In;
    }
    
    bool IsNull()
    {
    return fn == NULL;
    }
};

class Peer : public boost::enable_shared_from_this<Peer>, private boost::noncopyable
{
public:
    /// Construct a peer connection with the given io_service.
    explicit Peer(boost::asio::io_service& io_service, PeerManager& manager, MessageHandler& handler, bool inbound, bool proxy, int betsHeight);
    
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
    const uint64 getNonce() const { return _nonce; }

private:
    void handle_read(const boost::system::error_code& e, std::size_t bytes_transferred);
    void handle_write(const boost::system::error_code& e, std::size_t bytes_transferred);
    
    /// Handle the 3 stages of a reply
    void reply();
    void trickle();
    void broadcast();

    void Peer::check_activity();
    
public:
    // socket
    uint64 nServices;
    CDataStream vSend;
    CDataStream vRecv;

    int64 nLastSend;
    int64 nLastRecv;
    int64 nLastSendEmpty;
    int64 nTimeConnected;
    unsigned int nHeaderStart;
    unsigned int nMessageStart;
    Endpoint addr;
    int nVersion;
    std::string strSubVer;
    bool fClient;
    bool fInbound;
    bool fNetworkNode;
    bool fSuccessfullyConnected;
    bool fDisconnect;
    
public:
    int64 nReleaseTime;
    std::map<uint256, CRequestTracker> mapRequests;
    uint256 hashContinue;
    CBlockLocator locatorLastGetBlocksBegin;
    uint256 hashLastGetBlocksEnd;
    int nStartingHeight;
    
    // flood relay
    std::vector<Endpoint> vAddrToSend;
    std::set<Endpoint> setAddrKnown;
    bool fGetAddr;
    std::set<uint256> setKnown;
    
    // inventory based relay
    std::set<Inventory> setInventoryKnown;
    std::vector<Inventory> vInventoryToSend;

    std::multimap<int64, Inventory> mapAskFor;
    
public:
        
    void AddAddressKnown(const Endpoint& addr);
    
    void PushAddress(const Endpoint& addr);
    
    void AddInventoryKnown(const Inventory& inv);
    
    void PushInventory(const Inventory& inv);
    
    void AskFor(const Inventory& inv);
    
    void BeginMessage(const char* pszCommand);
    
    void AbortMessage();
    
    void EndMessage();
    
    void EndMessageAbortIfEmpty();
    
    void PushVersion();
    
    void PushMessage(const char* pszCommand) {
        try {
            BeginMessage(pszCommand);
            EndMessage();
        }
        catch (...) {
            AbortMessage();
            throw;
        }
    }
    
    template<typename T1>
    void PushMessage(const char* pszCommand, const T1& a1) {
        try {
            BeginMessage(pszCommand);
            vSend << a1;
            EndMessage();
        }
        catch (...) {
            AbortMessage();
            throw;
        }
    }
    
    template<typename T1, typename T2>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2) {
        try {
            BeginMessage(pszCommand);
            vSend << a1 << a2;
            EndMessage();
        }
        catch (...) {
            AbortMessage();
            throw;
        }
    }
    
    void PushRequest(const char* pszCommand, void (*fn)(void*, CDataStream&), void* param1) {
        uint256 hashReply;
        RAND_bytes((unsigned char*)&hashReply, sizeof(hashReply));
        
        mapRequests[hashReply] = CRequestTracker(fn, param1);
        
        PushMessage(pszCommand, hashReply);
    }
    
    template<typename T1>
    void PushRequest(const char* pszCommand, const T1& a1, void (*fn)(void*, CDataStream&), void* param1) {
        uint256 hashReply;
        RAND_bytes((unsigned char*)&hashReply, sizeof(hashReply));
        
        mapRequests[hashReply] = CRequestTracker(fn, param1);
        
        PushMessage(pszCommand, hashReply, a1);
    }
    
    template<typename T1, typename T2>
    void PushRequest(const char* pszCommand, const T1& a1, const T2& a2, void (*fn)(void*, CDataStream&), void* param1) {
        uint256 hashReply;
        RAND_bytes((unsigned char*)&hashReply, sizeof(hashReply));
        
        mapRequests[hashReply] = CRequestTracker(fn, param1);
        
        PushMessage(pszCommand, hashReply, a1, a2);
    }
    
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5, const T6& a6, const T7& a7, const T8& a8) {
        try {
            BeginMessage(pszCommand);
            vSend << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8;
            EndMessage();
        }
        catch (...) {
            AbortMessage();
            throw;
        }
    }
    
    bool ProcessMessages();
    
    void PushGetBlocks(const CBlockLocator locatorBegin, uint256 hashEnd);

private:

    /// Socket for the connection to the peer.
    boost::asio::ip::tcp::socket _socket;
    
    /// The manager for this connection.
    PeerManager& _peerManager;
    
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
    
    /// Use proxy or not - this is just the flag - we need to carry around the address too
    bool _proxy;
    
    /// the local nonce - used to detect connections to self
    uint64 _nonce;

    /// Streambufs for reading and writing
    boost::asio::streambuf _send;
    boost::asio::streambuf _recv;
    
    /// Buffer for incoming data. Should be changed to streams in the future.
    boost::array<char, 8192> _buffer;
};

#endif // PEER_H
