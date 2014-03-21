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

#include <coinChain/Peer.h>
#include <coinHTTP/ConnectionManager.h>
#include <coinHTTP/RequestHandler.h>

#include <coin/Logger.h>

#include <boost/bind.hpp>

#include <vector>

using namespace std;
using namespace boost;
using namespace asio;

Peer::Peer(const Chain& chain, io_service& io_service, PeerManager& manager, MessageHandler& handler, bool inbound, bool proxy, int bestHeight, std::string sub_version) : _chain(chain),_socket(io_service), _peerManager(manager), _messageHandler(handler), _msgParser(), _suicide(io_service), _keep_alive(io_service) {
    nServices = 0;

    vSend.SetType(SER_NETWORK);
    vSend.SetVersion(0);
    vRecv.SetType(SER_NETWORK);
    vRecv.SetVersion(0);
    // Version 0.2 obsoletes 20 Feb 2012
    if (UnixTime::s() > 1329696000) {
        vSend.SetVersion(209);
        vRecv.SetVersion(209);
    }
    nLastSend = 0;
    nLastRecv = 0;
    nLastSendEmpty = UnixTime::s();
    nTimeConnected = UnixTime::s();
    nHeaderStart = -1;
    nMessageStart = -1;
    //    addr = addrIn;
    nVersion = 0;
    strSubVer = "";
    fClient = false; // set by version message
    fInbound = inbound;
    fNetworkNode = false;
    _successfullyConnected = false;
    fDisconnect = false;
    //    nRefCount = 0;
    nReleaseTime = 0;
    hashContinue = 0;
    locatorLastGetBlocksBegin.SetNull();
    hashLastGetBlocksEnd = 0;
    _startingHeight = bestHeight;
    fGetAddr = false;    
    _proxy = proxy;
    _nonce = 0;
    _activity = false;
}

ip::tcp::socket& Peer::socket() {
    return _socket;
}

void Peer::start() {
    log_debug("Starting Peer: %s", addr.toString());
    // Be shy and don't send version until we hear
    if (!fInbound) {
        PushVersion();
        // write stuff... - to this peer
        for(CDataStream::iterator c = vSend.begin(); c != vSend.end(); ++c)
            _send.sputc(*c);
        vSend.clear();
        async_write(_socket, _send, boost::bind(&Peer::handle_write, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
    }

    //    async_read(_socket, _recv, boost::bind(&Peer::handle_read, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
    _suicide.expires_from_now(posix_time::seconds(_initial_timeout)); // no activity the first 60 seconds means disconnect
    _keep_alive.expires_from_now(posix_time::seconds(_heartbeat_timeout)); // no activity the first 60 seconds means disconnect
    _socket.async_read_some(buffer(_buffer), boost::bind(&Peer::handle_read, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
    
    // and start the deadline timer
    _suicide.async_wait(boost::bind(&Peer::check_activity, this, asio::placeholders::error));
    _keep_alive.async_wait(boost::bind(&Peer::show_activity, this, asio::placeholders::error));
}

void Peer::check_activity(const system::error_code& e) {
    if (!e) {
        if(!_activity)
            _peerManager.post_stop(shared_from_this());
        else {
            _activity = false;
            _suicide.expires_from_now(posix_time::seconds(_suicide_timeout)); // 90 minutes of activity once we have started up
            _suicide.async_wait(boost::bind(&Peer::check_activity, this, asio::placeholders::error));
        }
    }
    else if (e != error::operation_aborted) {
        log_info("Boost deadline timer error in Peer: %s\n", e.message().c_str());
    }
    
    // we ignore abort errors - they are generated due to timer cancels
}

void Peer::show_activity(const system::error_code& e) {
    if (!e) {
        if(!_activity) { // send a ping - we might get a pong then
            // Keep-alive ping. We send a nonce of zero because we don't use it anywhere
            // right now.
            uint64_t nonce = 0;
            if (nVersion > BIP0031_VERSION)
                PushMessage("ping", nonce);
            else
                PushMessage("ping");
        }
        _suicide.expires_from_now(posix_time::seconds(_heartbeat_timeout)); // show activity each 30 minutes
        _suicide.async_wait(boost::bind(&Peer::show_activity, this, asio::placeholders::error));
    }
    else if (e != error::operation_aborted) {
        log_info("Boost deadline timer error in Peer: %s\n", e.message().c_str());
    }
    
    // we ignore abort errors - they are generated due to timer cancels
}

void Peer::stop() {
    _suicide.cancel(); // no need to commit suicide when being killed
    boost::system::error_code ec;
    _socket.shutdown(ip::tcp::socket::shutdown_both, ec);
    if (ec) {
        log_debug("socket shutdown error: %s", ec.message()); // An error occurred.
    }
    _socket.close();
}

void Peer::handle_read(const system::error_code& e, std::size_t bytes_transferred) {
    log_trace("args error: %s, bytes: %d", e.message(), bytes_transferred);
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
            else if (!result) {
                log_warn("Peer %s sending bogus - disconnecting", addr.toString());
                _peerManager.post_stop(shared_from_this());
            }//                continue; // basically, if we get a false result, we should consider to disconnect from the Peer!
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
        _socket.async_read_some(buffer(_buffer), boost::bind(&Peer::handle_read, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
        //        async_read(_socket, _recv, boost::bind(&Peer::handle_read, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
    }
    else if (e != error::operation_aborted) {
        log_debug("Read error %s, disconnecting... (read %d bytes though) \n", e.message().c_str(), bytes_transferred);
        _peerManager.post_stop(shared_from_this());
    }
    log_trace("exit");
}

void Peer::queue(const Inventory& inv) {
    int at = _peerManager.prioritize(inv);
    _queue.insert(make_pair(at, inv));
}

void Peer::dequeue(const Inventory& inv) {
    _peerManager.dequeue(inv);
}

void Peer::reply() {
    vector<Inventory> get_data;

    int now = UnixTime::s();
    
    while (!_queue.empty() && _queue.begin()->first <= now) {
        const Inventory& inv = _queue.begin()->second;
        if (_peerManager.queued(inv)) { // will check that this is really pending (as opposed to downloaded)
            log_debug("sending getdata: %s", inv.toString().c_str());
            get_data.push_back(inv);
            if (get_data.size() >= 1000) {
                PushMessage("getdata", get_data);
                get_data.clear();
            }
        }
        _queue.erase(_queue.begin());
    }
    if (!get_data.empty())
        PushMessage("getdata", get_data);
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
            bool fTrickleWait = ((hashRand & uint256(3)) != 0);
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
        async_write(_socket, _send, boost::bind(&Peer::handle_write, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
    }
}

void Peer::handle_write(const system::error_code& e, size_t bytes_transferred) {
    log_trace("args error: %s, bytes: %d", e.message(), bytes_transferred);
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
        log_debug("Write error %s, disconnecting...\n", e.message().c_str());
        _peerManager.post_stop(shared_from_this());
    }
    log_trace("exit");
}

void Peer::successfullyConnected() {
    _successfullyConnected = true;
    _peerManager.ready(shared_from_this());
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

void Peer::push(const Inventory& inv) {
    if (!setInventoryKnown.count(inv)) {
        vInventoryToSend.push_back(inv);
    }
}

void Peer::push(const Transaction& txn) {
    if(!relayTxes)
        return;
    if (filter.isRelevantAndUpdate(txn))
        PushMessage("tx", txn);
}

void Peer::BeginMessage(const char* pszCommand) {
    if (nHeaderStart != -1)
        AbortMessage();
    nHeaderStart = vSend.size();
    vSend << MessageHeader(_chain, pszCommand, 0);
    nMessageStart = vSend.size();
    if (fDebug)
        log_debug("%s ", DateTimeStrFormat("%x %H:%M:%S", UnixTime::s()).c_str());
    log_debug("sending: %s ", pszCommand);
}

void Peer::AbortMessage() {
    if (nHeaderStart == -1)
        return;
    vSend.resize(nHeaderStart);
    nHeaderStart = -1;
    nMessageStart = -1;
    log_debug("(aborted)\n");
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
    
    log_debug("(%d bytes) ", nSize);
    
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
    int64_t nTime = (fInbound ? GetAdjustedTime() : UnixTime::s());
    //    Endpoint remote = _socket.remote_endpoint();
    Endpoint local = _socket.local_endpoint();
    Endpoint addrYou = (_proxy ? Endpoint("0.0.0.0") : addr);
    Endpoint addrMe = (_proxy ? Endpoint("0.0.0.0") : local);
    // hack to avoid serializing the time
    addrYou.setLastTry(UINT_MAX);
    addrMe.setLastTry(UINT_MAX);
    RAND_bytes((unsigned char*)&_nonce, sizeof(_nonce));
    uint64_t nLocalServices = NODE_NETWORK;
    PushMessage("version", PROTOCOL_VERSION, nLocalServices, nTime, addrYou, addrMe,
                _nonce, _sub_version, _startingHeight, true);
}

void Peer::PushGetBlocks(const BlockLocator locatorBegin, uint256 hashEnd)
{
    // Filter out duplicate requests
    if (locatorBegin == locatorLastGetBlocksBegin && hashEnd == hashLastGetBlocksEnd)
        return;
    locatorLastGetBlocksBegin = locatorBegin;
    hashLastGetBlocksEnd = hashEnd;
    
    PushMessage("getblocks", locatorBegin, hashEnd);
}


