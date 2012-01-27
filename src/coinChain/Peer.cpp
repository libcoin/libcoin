
#include "coinChain/Peer.h"
#include <vector>
#include <boost/bind.hpp>
#include "coinHTTP/ConnectionManager.h"
#include "coinHTTP/RequestHandler.h"

using namespace std;
using namespace boost;
using namespace asio;

static map<Inventory, int64> mapAlreadyAskedFor;

Peer::Peer(const Chain& chain, io_service& io_service, PeerManager& manager, MessageHandler& handler, bool inbound, bool proxy, int bestHeight) : _chain(chain),_socket(io_service), _peerManager(manager), _messageHandler(handler), _msgParser(), _suicide(io_service) {
    nServices = 0;

    vSend.SetType(SER_NETWORK);
    vSend.SetVersion(0);
    vRecv.SetType(SER_NETWORK);
    vRecv.SetVersion(0);
    // Version 0.2 obsoletes 20 Feb 2012
    if (GetTime() > 1329696000) {
        vSend.SetVersion(209);
        vRecv.SetVersion(209);
    }
    nLastSend = 0;
    nLastRecv = 0;
    nLastSendEmpty = GetTime();
    nTimeConnected = GetTime();
    nHeaderStart = -1;
    nMessageStart = -1;
    //    addr = addrIn;
    nVersion = 0;
    strSubVer = "";
    fClient = false; // set by version message
    fInbound = inbound;
    fNetworkNode = false;
    fSuccessfullyConnected = false;
    fDisconnect = false;
    //    nRefCount = 0;
    nReleaseTime = 0;
    hashContinue = 0;
    locatorLastGetBlocksBegin.SetNull();
    hashLastGetBlocksEnd = 0;
    nStartingHeight = bestHeight;
    fGetAddr = false;    
    _proxy = proxy;
    _nonce = 0;
    _activity = false;
}

ip::tcp::socket& Peer::socket() {
    return _socket;
}

void Peer::start() {
    printf("Starting Peer: %s\n", addr.toString().c_str());
    // Be shy and don't send version until we hear
    if (!fInbound) {
        PushVersion();
        // write stuff... - to this peer
        for(CDataStream::iterator c = vSend.begin(); c != vSend.end(); ++c)
            _send.sputc(*c);
        vSend.clear();
        async_write(_socket, _send, bind(&Peer::handle_write, shared_from_this(), placeholders::error, placeholders::bytes_transferred));
    }

    //    async_read(_socket, _recv, bind(&Peer::handle_read, shared_from_this(), placeholders::error, placeholders::bytes_transferred));
    _suicide.expires_from_now(posix_time::seconds(_initial_timeout)); // no activity the first 60 seconds means disconnect
    _socket.async_read_some(buffer(_buffer), bind(&Peer::handle_read, shared_from_this(), placeholders::error, placeholders::bytes_transferred));
    
    // and start the deadline timer
    _suicide.async_wait(bind(&Peer::check_activity, this, placeholders::error));
}

void Peer::check_activity(const system::error_code& e) {
    if (!e) {
        if(!_activity)
            _peerManager.post_stop(shared_from_this());
        else {
            _activity = false;
            _suicide.expires_from_now(posix_time::seconds(_heartbeat_timeout)); // 90 minutes of activity once we have started up
            _suicide.async_wait(bind(&Peer::check_activity, this, placeholders::error)); 
        }
    }
    else if (e != error::operation_aborted) {
        printf("Boost deadline timer error in Peer: %s\n", e.message().c_str());
    }

    // we ignore abort errors - they are generated due to timer cancels 
}

void Peer::stop() {
    _suicide.cancel(); // no need to commit suicide when being killed
    _socket.close();
}

void Peer::handle_read(const system::error_code& e, std::size_t bytes_transferred) {
    if (!e) {
        _activity = true;
        string rx;
        for(int i = 0; i < bytes_transferred; i++)
            rx.push_back(_buffer[i]);
        
        vRecv += rx;
        
        // Now call the parser:
        bool fRet = false;
        loop {
            boost::tuple<boost::tribool, CDataStream::iterator> parser_result = _msgParser.parse(_chain, _message, vRecv.begin(), vRecv.end());
            tribool result = get<0>(parser_result);
            vRecv.erase(vRecv.begin(), get<1>(parser_result));
            if (result) {
                if (_messageHandler.handleMessage(this, _message) ) fRet = true;
            }
            else if (!result)
                continue;
            else
                break;
        }

        // now if fRet is true, something were processed by the filters - we want to send to the peers / we check for which vSends cointains stuff and the we run
        
        if (fRet && nVersion > 0) {
            // first reply
            reply();
            
            // then trickle
            Peers peers = _peerManager.getAllPeers();
            size_t rand = GetRand(peers.size());
            for (Peers::iterator peer = peers.begin(); peer != peers.end(); ++peer)
                if(rand-- == 0) {
                    (*peer)->trickle();
                    break;
                }
            
            // then broadcast
            for (Peers::iterator peer = peers.begin(); peer != peers.end(); ++peer)
                (*peer)->broadcast();
            
            // now write to the peers with non-empty vSend buffers
            for (Peers::iterator peer = peers.begin(); peer != peers.end(); ++peer) {
                (*peer)->flush();
            }
        }
        
        // then wait for more data
        _socket.async_read_some(buffer(_buffer), bind(&Peer::handle_read, shared_from_this(), placeholders::error, placeholders::bytes_transferred));
        //        async_read(_socket, _recv, bind(&Peer::handle_read, shared_from_this(), placeholders::error, placeholders::bytes_transferred));
    }
    else if (e != error::operation_aborted) {
        printf("Read error %s, disconnecting... (read %d bytes though) \n", e.message().c_str(), bytes_transferred);
        _peerManager.post_stop(shared_from_this());
    }
}

void Peer::reply() {
        //      Message: getdata
        vector<Inventory> vGetData;
        int64 nNow = GetTime() * 1000000;
        
        while (!mapAskFor.empty() && (*mapAskFor.begin()).first <= nNow)  {
            const Inventory& inv = (*mapAskFor.begin()).second;
            // we have already checkked for this (I think???)            if (!AlreadyHave(inv))
            printf("sending getdata: %s\n", inv.toString().c_str());
            vGetData.push_back(inv);
            if (vGetData.size() >= 1000) {
                PushMessage("getdata", vGetData);
                vGetData.clear();
            }
            mapAlreadyAskedFor[inv] = nNow;
            mapAskFor.erase(mapAskFor.begin());
        }
        if (!vGetData.empty())
            PushMessage("getdata", vGetData);
}

void Peer::trickle() {
    vector<Endpoint> vAddr;
    vAddr.reserve(vAddrToSend.size());
    BOOST_FOREACH(const Endpoint& addr, vAddrToSend) {
        // returns true if wasn't already contained in the set
        if (setAddrKnown.insert(addr).second) {
            vAddr.push_back(addr);
            // receiver rejects addr messages larger than 1000
            if (vAddr.size() >= 1000) {
                PushMessage("addr", vAddr);
                vAddr.clear();
            }
        }
    }
    vAddrToSend.clear();
    if (!vAddr.empty())
        PushMessage("addr", vAddr);
    
    //      Message: inventory
    vector<Inventory> vInv;
    vInv.reserve(vInventoryToSend.size());
    BOOST_FOREACH(const Inventory& inv, vInventoryToSend) {
        if (setInventoryKnown.count(inv))
            continue;
        
        // returns true if wasn't already contained in the set
        if (setInventoryKnown.insert(inv).second) {
            vInv.push_back(inv);
            if (vInv.size() >= 1000) {
                PushMessage("inv", vInv);
                vInv.clear();
            }
        }
    }
    if (!vInv.empty())
        PushMessage("inv", vInv);
}

void Peer::broadcast() {
    vector<Inventory> vInv;
    vector<Inventory> vInvWait;
    vInv.reserve(vInventoryToSend.size());
    vInvWait.reserve(vInventoryToSend.size());
    BOOST_FOREACH(const Inventory& inv, vInventoryToSend) {
        if (setInventoryKnown.count(inv))
            continue;
        
        if (inv.getType() == MSG_TX) {
            // A specific 1/4 (hash space based) of tx invs blast to all immediately
            static uint256 hashSalt;
            if (hashSalt == 0)
                RAND_bytes((unsigned char*)&hashSalt, sizeof(hashSalt));
            uint256 hashRand = inv.getHash() ^ hashSalt;
            hashRand = Hash(BEGIN(hashRand), END(hashRand));
            bool fTrickleWait = ((hashRand & 3) != 0);
            if (fTrickleWait) {
                vInvWait.push_back(inv);
                continue;
            }
        }
        
        // returns true if wasn't already contained in the set
        if (setInventoryKnown.insert(inv).second) {
            vInv.push_back(inv);
            if (vInv.size() >= 1000) {
                PushMessage("inv", vInv);
                vInv.clear();
            }
        }
    }
    vInventoryToSend = vInvWait;
    
    if (!vInv.empty())
        PushMessage("inv", vInv);
}

void Peer::flush() {
    if (vSend.size()) {
        for(CDataStream::iterator c = vSend.begin(); c != vSend.end(); ++c)
            _send.sputc(*c);
        vSend.clear();
        async_write(_socket, _send, bind(&Peer::handle_write, shared_from_this(), placeholders::error, placeholders::bytes_transferred));
    }
}

void Peer::handle_write(const system::error_code& e, size_t bytes_transferred) {
    /*
    if (!e) {
        // Initiate graceful connection closure.
        system::error_code ignored_ec;
        _socket.shutdown(ip::tcp::socket::shutdown_both, ignored_ec);
    }
    */
    if (!e) {
        // you need show you activity to avoid disconnection
        //        _activity = true;
    }
    else if (e != error::operation_aborted) {
        printf("Write error %s, disconnecting...\n", e.message().c_str());
        _peerManager.post_stop(shared_from_this());
    }
}

void Peer::AddAddressKnown(const Endpoint& addr) {
    setAddrKnown.insert(addr);
}

void Peer::PushAddress(const Endpoint& addr) {
    // Known checking here is only to save space from duplicates.
    // SendMessages will filter it again for knowns that were added
    // after addresses were pushed.
    if (addr.isValid() && !setAddrKnown.count(addr))
        vAddrToSend.push_back(addr);
}

void Peer::AddInventoryKnown(const Inventory& inv) {
    setInventoryKnown.insert(inv);
}

void Peer::PushInventory(const Inventory& inv) {
    if (!setInventoryKnown.count(inv))
        vInventoryToSend.push_back(inv);
}

void Peer::AskFor(const Inventory& inv) {
    // We're using mapAskFor as a priority queue,
    // the key is the earliest time the request can be sent
    int64& nRequestTime = mapAlreadyAskedFor[inv];
    printf("askfor %s   %"PRI64d"\n", inv.toString().c_str(), nRequestTime);
    
    // Make sure not to reuse time indexes to keep things in the same order
    int64 nNow = (GetTime() - 1) * 1000000;
    static int64 nLastTime;
    nLastTime = nNow = std::max(nNow, ++nLastTime);
    
    // Each retry is 2 minutes after the last
    nRequestTime = std::max(nRequestTime + 2 * 60 * 1000000, nNow);
    mapAskFor.insert(std::make_pair(nRequestTime, inv));
}

void Peer::BeginMessage(const char* pszCommand) {
    if (nHeaderStart != -1)
        AbortMessage();
    nHeaderStart = vSend.size();
    vSend << MessageHeader(_chain, pszCommand, 0);
    nMessageStart = vSend.size();
    if (fDebug)
        printf("%s ", DateTimeStrFormat("%x %H:%M:%S", GetTime()).c_str());
    printf("sending: %s ", pszCommand);
}

void Peer::AbortMessage() {
    if (nHeaderStart == -1)
        return;
    vSend.resize(nHeaderStart);
    nHeaderStart = -1;
    nMessageStart = -1;
    printf("(aborted)\n");
}

void Peer::EndMessage() {
    /*
    if (mapArgs.count("-dropmessagestest") && GetRand(atoi(mapArgs["-dropmessagestest"])) == 0) {
        printf("dropmessages DROPPING SEND MESSAGE\n");
        AbortMessage();
        return;
    }
    */
    
    if (nHeaderStart == -1)
        return;
    
    // Set the size
    unsigned int nSize = vSend.size() - nMessageStart;
    memcpy((char*)&vSend[nHeaderStart] + offsetof(MessageHeader, nMessageSize), &nSize, sizeof(nSize));
    
    // Set the checksum
    if (vSend.GetVersion() >= 209) {
        uint256 hash = Hash(vSend.begin() + nMessageStart, vSend.end());
        unsigned int nChecksum = 0;
        memcpy(&nChecksum, &hash, sizeof(nChecksum));
        assert(nMessageStart - nHeaderStart >= offsetof(MessageHeader, nChecksum) + sizeof(nChecksum));
        memcpy((char*)&vSend[nHeaderStart] + offsetof(MessageHeader, nChecksum), &nChecksum, sizeof(nChecksum));
    }
    
    printf("(%d bytes) ", nSize);
    printf("\n");
    
    nHeaderStart = -1;
    nMessageStart = -1;
}

void Peer::EndMessageAbortIfEmpty() {
    if (nHeaderStart == -1)
        return;
    int nSize = vSend.size() - nMessageStart;
    if (nSize > 0)
        EndMessage();
    else
        AbortMessage();
}

void Peer::PushVersion() {
    /// when NTP implemented, change to just nTime = GetAdjustedTime()
    int64 nTime = (fInbound ? GetAdjustedTime() : GetTime());
    //    Endpoint remote = _socket.remote_endpoint();
    Endpoint local = _socket.local_endpoint();
    Endpoint addrYou = (_proxy ? Endpoint("0.0.0.0") : addr);
    Endpoint addrMe = (_proxy ? Endpoint("0.0.0.0") : local);
    RAND_bytes((unsigned char*)&_nonce, sizeof(_nonce));
    uint64 nLocalServices = NODE_NETWORK;
    PushMessage("version", VERSION, nLocalServices, nTime, addrYou, addrMe,
                _nonce, std::string(pszSubVer), nStartingHeight);
}

void Peer::PushGetBlocks(const CBlockLocator locatorBegin, uint256 hashEnd)
{
    // Filter out duplicate requests
    if (locatorBegin == locatorLastGetBlocksBegin && hashEnd == hashLastGetBlocksEnd)
        return;
    locatorLastGetBlocksBegin = locatorBegin;
    hashLastGetBlocksEnd = hashEnd;
    
    PushMessage("getblocks", locatorBegin, hashEnd);
}


