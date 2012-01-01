#ifdef _LIBBTC_ASIO_

#include "btcHTTP/Connection.h"
#include <vector>
#include <boost/bind.hpp>
#include "btcHTTP/ConnectionManager.h"
#include "btcHTTP/RequestHandler.h"

using namespace boost;
using namespace asio;

Peer::Peer(io_service& io_service, PeerManager& manager, MessageHandler& handler, bool inbound) : _socket(io_service), _peerManager(manager), _msgHandler(handler), _msgParser() {
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
    addr = addrIn;
    nVersion = 0;
    strSubVer = "";
    fClient = false; // set by version message
    fInbound = inbound;
    fNetworkNode = false;
    fSuccessfullyConnected = false;
    fDisconnect = false;
    nRefCount = 0;
    nReleaseTime = 0;
    hashContinue = 0;
    locatorLastGetBlocksBegin.SetNull();
    hashLastGetBlocksEnd = 0;
    nStartingHeight = -1;
    fGetAddr = false;    
}

ip::tcp::socket& Peer::socket() {
    return _socket;
}

void Peer::start() {
    // Be shy and don't send version until we hear
    if (!fInbound) {
        PushVersion();
        // write stuff... - to this peer
        async_write(_socket, vSend.rdbuf(), bind(&Peer::handle_write, shared_from_this(), placeholders::error, placeholders::bytes_transferred));
    }

    async_read(_socket, vRecv.rdbuf(), bind(&Peer::handle_read, shared_from_this(), placeholders::error, placeholders::bytes_transferred));
    //    _socket.async_read_some(buffer(_buffer), bind(&Peer::handle_read, shared_from_this(), placeholders::error, placeholders::bytes_transferred));
}

void Peer::stop() {
    _socket.close();
}

void Peer::handle_read(const system::error_code& e, std::size_t bytes_transferred) {
    if (!e) {
        if (vRecv.empty())
            return true;
        
        // Now call the parser:
        bool fRet = false;
        loop {
            boost::tuple<boost::tribool, CDataStream::iterator> parser_result = _msgParser.parse(_message, vRecv.begin(), vRecv.end());
            tribool result = get<0>(parser_result);
            vRecv.erase(vRecv.begin(), get<1>(parser_result));
            if (result) {
                if (_msgHandler->handleMessage(this, _message) ) fRet = true;
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
            _peerManager.getRandPeer()->trickle();
            
            // then broadcast
            Peers peers = _peerManager.getPeers();
            for (Peers::iterator peer = peers.begin(); peer != peers.end(); ++peer)
                peer->broadcast();
            
            // now write to the peers with non-empty vSend buffers
            for (Peers::iterator peer = peers.begin(); peer != peers.end(); ++peer) {
                peer->flush();
            }
        }
        
        // then wait for more data
        async_read(_socket, vRecv.rdbuf(), bind(&Peer::handle_read, shared_from_this(), placeholders::error, placeholders::bytes_transferred));
    }
    else if (e != error::operation_aborted) {
        _peerManager.stop(shared_from_this());
    }
}

void Peer::reply() {
    CRITICAL_BLOCK(cs_main) {        
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
}

void Peer::trickle() {
    CRITICAL_BLOCK(cs_main) {        
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
        CRITICAL_BLOCK(pto->cs_inventory) {
            vInv.reserve(pto->vInventoryToSend.size());
            BOOST_FOREACH(const Inventory& inv, pto->vInventoryToSend) {
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
        }
        if (!vInv.empty())
            PushMessage("inv", vInv);
    }
}

void Peer::broadcast() {
    CRITICAL_BLOCK(cs_main) {        
        vector<Inventory> vInv;
        vector<Inventory> vInvWait;
        CRITICAL_BLOCK(cs_inventory) {
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
        }
        if (!vInv.empty())
            PushMessage("inv", vInv);
    }
        
    // Address refresh broadcast - should be put into the EndpointFilter...
    static int64 nLastRebroadcast;
    if (GetTime() - nLastRebroadcast > 24 * 60 * 60) {
        nLastRebroadcast = GetTime();
        // Periodically clear setAddrKnown to allow refresh broadcasts
        setAddrKnown.clear();
        
        // Rebroadcast our address
        if (_endpointPool->getLocal().isRoutable() && !fUseProxy) {
            Endpoint addr(_endpointPool->getLocal());
            addr.setTime(GetAdjustedTime());
            PushAddress(addr);
        }
    }
    
    // Clear out old addresses periodically so it's not too much work at once
    _endpointPool->purge();
}

void Peer::flush() {
    if (vSend.size()) {
        async_write(_socket, vSend.rdbuf(), bind(&Peer::handle_write, shared_from_this(), placeholders::error, placeholders::bytes_transferred));
    }
}

void Peer::handle_write(const system::error_code& e) {
    if (!e) {
        // Initiate graceful connection closure.
        system::error_code ignored_ec;
        _socket.shutdown(ip::tcp::socket::shutdown_both, ignored_ec);
    }
    
    if (e != error::operation_aborted) {
        _connectionManager.stop(shared_from_this());
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
    CRITICAL_BLOCK(cs_inventory)
    setInventoryKnown.insert(inv);
}

void Peer::PushInventory(const Inventory& inv) {
    CRITICAL_BLOCK(cs_inventory)
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
    cs_vSend.Enter("cs_vSend", __FILE__, __LINE__);
    if (nHeaderStart != -1)
        AbortMessage();
    nHeaderStart = vSend.size();
    vSend << MessageHeader(pszCommand, 0);
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
    cs_vSend.Leave();
    printf("(aborted)\n");
}

void Peer::EndMessage() {
    if (mapArgs.count("-dropmessagestest") && GetRand(atoi(mapArgs["-dropmessagestest"])) == 0) {
        printf("dropmessages DROPPING SEND MESSAGE\n");
        AbortMessage();
        return;
    }
    
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
    cs_vSend.Leave();
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
    Endpoint addrYou = (fUseProxy ? Endpoint("0.0.0.0") : addr);
    Endpoint addrMe = (fUseProxy ? Endpoint("0.0.0.0") : _endpointPool->getLocal());
    RAND_bytes((unsigned char*)&nLocalHostNonce, sizeof(nLocalHostNonce));
    PushMessage("version", VERSION, nLocalServices, nTime, addrYou, addrMe,
                nLocalHostNonce, std::string(pszSubVer), __blockChain->getBestHeight());
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

bool Peer::ProcessMessages()
{
    Peer* pfrom = this;
    CDataStream& vRecv = pfrom->vRecv;
    if (vRecv.empty())
        return true;
    
    // Now call the parser:
    bool fRet = false;
    loop {
        boost::tuple<boost::tribool, CDataStream::iterator> parser_result = _msgParser.parse(_message, vRecv.begin(), vRecv.end());
        tribool result = get<0>(parser_result);
        vRecv.erase(vRecv.begin(), get<1>(parser_result));
        if (result) {
            if (_msgHandler->handleMessage(pfrom, _message) ) fRet = true;
        }
        else if (!result)
            continue;
        else
            break;
    }
    
    // if something were processed - handle some of the writes
    // first we write stuff back - process the "getdata" part of SendMessages
    // then handle the trickle node - pick a random node and run the addr and tx part of SendMessages
    // then take a few other nodes in random and send out the smaller fraction of the tx'es, or all and handle only the tx fraction + the block invs
    // reply, trickle, all
    Peer* pto = pfrom;
    if (fRet && pto->nVersion != 0) {
        // the ping stuff need to be controlled by a timer! - skip for now...
        //ResendBrokerTransactions();
        CRITICAL_BLOCK(cs_main) {
            
            // ----- R E P L Y -----
            
            //      Message: getdata
            vector<Inventory> vGetData;
            int64 nNow = GetTime() * 1000000;
            
            while (!pto->mapAskFor.empty() && (*pto->mapAskFor.begin()).first <= nNow)  {
                const Inventory& inv = (*pto->mapAskFor.begin()).second;
                // we have already checkked for this (I think???)            if (!AlreadyHave(inv))
                printf("sending getdata: %s\n", inv.toString().c_str());
                vGetData.push_back(inv);
                if (vGetData.size() >= 1000) {
                    pto->PushMessage("getdata", vGetData);
                    vGetData.clear();
                }
                mapAlreadyAskedFor[inv] = nNow;
                pto->mapAskFor.erase(pto->mapAskFor.begin());
            }
            if (!vGetData.empty())
                pto->PushMessage("getdata", vGetData);
            
            // ----- T R I C K L E -----
            // pick a trickle node
            pto = vNodes[GetRand(vNodes.size())];
            vector<Endpoint> vAddr;
            vAddr.reserve(pto->vAddrToSend.size());
            BOOST_FOREACH(const Endpoint& addr, pto->vAddrToSend)
            {
            // returns true if wasn't already contained in the set
            if (pto->setAddrKnown.insert(addr).second)
                {
                vAddr.push_back(addr);
                // receiver rejects addr messages larger than 1000
                if (vAddr.size() >= 1000)
                    {
                    pto->PushMessage("addr", vAddr);
                    vAddr.clear();
                    }
                }
            }
            pto->vAddrToSend.clear();
            if (!vAddr.empty())
                pto->PushMessage("addr", vAddr);
            
            //      Message: inventory
            vector<Inventory> vInv;
            CRITICAL_BLOCK(pto->cs_inventory) {
                vInv.reserve(pto->vInventoryToSend.size());
                BOOST_FOREACH(const Inventory& inv, pto->vInventoryToSend) {
                    if (pto->setInventoryKnown.count(inv))
                        continue;
                    
                    // returns true if wasn't already contained in the set
                    if (pto->setInventoryKnown.insert(inv).second) {
                        vInv.push_back(inv);
                        if (vInv.size() >= 1000) {
                            pto->PushMessage("inv", vInv);
                            vInv.clear();
                        }
                    }
                }
            }
            if (!vInv.empty())
                pto->PushMessage("inv", vInv);
            
            // ----- A L L ----- 
            
            //      Message: inventory
            CRITICAL_BLOCK(cs_vNodes) {
                BOOST_FOREACH(Peer* pto, vNodes) {
                    
                    vector<Inventory> vInv;
                    vector<Inventory> vInvWait;
                    CRITICAL_BLOCK(pto->cs_inventory) {
                        vInv.reserve(pto->vInventoryToSend.size());
                        vInvWait.reserve(pto->vInventoryToSend.size());
                        BOOST_FOREACH(const Inventory& inv, pto->vInventoryToSend) {
                            if (pto->setInventoryKnown.count(inv))
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
                            if (pto->setInventoryKnown.insert(inv).second) {
                                vInv.push_back(inv);
                                if (vInv.size() >= 1000) {
                                    pto->PushMessage("inv", vInv);
                                    vInv.clear();
                                }
                            }
                        }
                        pto->vInventoryToSend = vInvWait;
                    }
                    if (!vInv.empty())
                        pto->PushMessage("inv", vInv);
                }
            }
            
            // Address refresh broadcast
            static int64 nLastRebroadcast;
            if (GetTime() - nLastRebroadcast > 24 * 60 * 60) {
                nLastRebroadcast = GetTime();
                CRITICAL_BLOCK(cs_vNodes) {
                    BOOST_FOREACH(Peer* pnode, vNodes) {
                        // Periodically clear setAddrKnown to allow refresh broadcasts
                        pnode->setAddrKnown.clear();
                        
                        // Rebroadcast our address
                        if (_endpointPool->getLocal().isRoutable() && !fUseProxy) {
                            Endpoint addr(_endpointPool->getLocal());
                            addr.setTime(GetAdjustedTime());
                            pnode->PushAddress(addr);
                        }
                    }
                }
            }
            
            // Clear out old addresses periodically so it's not too much work at once
            _endpointPool->purge();
        }
    }
    
    return true;
    
}

#endif _LIBBTC_ASIO_


