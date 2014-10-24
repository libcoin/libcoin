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

Peer::Peer(const Chain& chain, io_service& io_service, PeerManager& manager, MessageHandler& handler, bool inbound, bool proxy, std::string sub_version) : _chain(chain),_socket(io_service), _peerManager(manager), _messageHandler(handler), _msgParser(), _connection_deadline(io_service), _suicide(io_service), _keep_alive(io_service) {
    _services = 0;
    _version = 0;//chain.protocol_version();

    //    addr = addrIn;
    _client = false; // set by version message
    _inbound = inbound;
//    fNetworkNode = false;
    _successfullyConnected = false;
    _disconnect = false;
    _continue = 0;
    locatorLastGetBlocksBegin.SetNull();
    hashLastGetBlocksEnd = 0;
    _startingHeight = 0;
    fGetAddr = false;    
    _proxy = proxy;
    _nonce = 0;
    RAND_bytes((unsigned char*)&_nonce, sizeof(_nonce));
    _activity = false;
}

Peer::~Peer() {
//    log_info("Deleted PEER: %s", _endpoint.toString());
}

ostream& operator<<(ostream& os, const Peer& p) {
    /// when NTP implemented, change to just nTime = GetAdjustedTime()
    int64_t nTime = (p._inbound ? GetAdjustedTime() : UnixTime::s());
    //    Endpoint remote = _socket.remote_endpoint();
    Endpoint local = p._socket.local_endpoint();
    Endpoint addrYou = (p._proxy ? Endpoint("0.0.0.0") : p._endpoint);
    Endpoint addrMe = (p._proxy ? Endpoint("0.0.0.0") : local);
    // hack to avoid serializing the time
    if (p._version)
        os << const_binary<int>(p._version);
    else // not initialized - we assume we are sending the version info
        os << const_binary<int>(p._chain.protocol_version());
    os << const_binary<uint64_t>(NODE_NETWORK) << const_binary<int64_t>(nTime);
    os << const_binary<uint64_t>(addrYou.getServices()) << const_binary<boost::asio::ip::address_v6::bytes_type>(addrYou.getIPv6()) << const_binary<unsigned short>(htons(addrYou.port()));
    os << const_binary<uint64_t>(addrMe.getServices()) << const_binary<boost::asio::ip::address_v6::bytes_type>(addrMe.getIPv6()) << const_binary<unsigned short>(htons(addrMe.port()));
    os << const_binary<uint64_t>(p._nonce) << const_varstr(p._sub_version);
    os << const_binary<int>(p._peerManager.getBestHeight());
    os << const_binary<bool>(true);
    return os;
}

istream& operator>>(istream& is, Peer& p) {
    int64_t nTime;
    is >> binary<int>(p._version) >> binary<uint64_t>(p._services) >> binary<int64_t>(nTime);
    // the endpoints in a version message is without time:
    
    uint64_t services;
    boost::asio::ip::address_v6::bytes_type ipv6;
    unsigned short port;
    is >> binary<uint64_t>(services) >> binary<boost::asio::ip::address_v6::bytes_type>(ipv6) >> binary<unsigned short>(port);
    
    //        v._me = Endpoint(services, ipv6, port);
    
    if (p._version == 10300)
        p._version = 300;
    if (p._version >= 106 && !is.eof()) {
        is >> binary<uint64_t>(services) >> binary<boost::asio::ip::address_v6::bytes_type>(ipv6) >> binary<unsigned short>(port);
        //            v_from = Endpoint(services, ipv6, port);
    }
    uint64_t nonce;
    is >> binary<uint64_t>(nonce);
    if (p._version >= 106 && !is.eof())
        is >> varstr(p._sub_version);
    if (p._version >= 209 && !is.eof())
        is >> binary<int>(p._startingHeight);
    if (!is.eof())
        is >> binary<bool>(p.relayTxes); // set to true after we get the first filter* message
    else
        p.relayTxes = true;
    
    if (nonce == p._nonce)
        p._disconnect = true;
    else
        p._nonce = nonce;
    
    p._client = !(p._services & NODE_NETWORK);
    
    AddTimeData(p._endpoint.getIP(), nTime);
    
    return is;
}

ip::tcp::socket& Peer::socket() {
    return _socket;
}

void Peer::start() {
    log_debug("Starting Peer: %s", _endpoint.toString());
    // Be shy and don't send version until we hear
    if (!_inbound) {
        PushVersion();
        flush();
    }

    _suicide.expires_from_now(posix_time::seconds(_initial_timeout)); // no activity the first 60 seconds means disconnect
    _keep_alive.expires_from_now(posix_time::seconds(_heartbeat_timeout)); // no activity the first 60 seconds means disconnect
    _socket.async_read_some(buffer(_recv.prepare(2048)), boost::bind(&Peer::handle_read, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
    
    // and start the deadline timer
    _suicide.async_wait(boost::bind(&Peer::check_activity, this, asio::placeholders::error));
    _keep_alive.async_wait(boost::bind(&Peer::show_activity, this, asio::placeholders::error));
}

void Peer::connect(const Endpoint& ep, Proxy& proxy, unsigned int timeout) {
    _peerManager.manage(shared_from_this());
    _endpoint = ep;
    // Set a deadline for the connect operation.
    _connection_deadline.expires_from_now(posix_time::milliseconds(timeout));
    // if using a socks4 proxy - we would here establish he connection to the socks server.
    if(proxy)
        proxy(socket()).async_connect(ep, boost::bind(&Peer::handle_connect, shared_from_this(), asio::placeholders::error));
    else
        socket().async_connect(ep, boost::bind(&Peer::handle_connect, shared_from_this(), asio::placeholders::error));
    // start wait for deadline to expire.
    _connection_deadline.async_wait(boost::bind(&Peer::check_deadline, shared_from_this(), asio::placeholders::error));
}

void Peer::handle_connect(const system::error_code& e) {
    _connection_deadline.cancel(); // cancel the deadline timer //expires_at(posix_time::pos_infin);
    
    if (!e && socket().is_open()) {
        start();
    }
    else {
        log_info("Failed connect: \"%s\" to: %s", e.message().c_str(), endpoint().toString().c_str());
        _peerManager.cancel(shared_from_this());
    }    
}

void Peer::check_deadline(const boost::system::error_code& e) {
    if(!e) {
        // Check whether the deadline has passed. We compare the deadline against
        // the current time.
        if (_connection_deadline.expires_at() <= deadline_timer::traits_type::now()) {
            // The deadline has passed. The socket is closed so that any outstanding
            // asynchronous operations are cancelled.
            log_debug("Closing socket of: %s", endpoint().toString());
            socket().close();
            _peerManager.cancel(shared_from_this());
        }
    }
    else if (e != error::operation_aborted) {
        log_warn("Boost deadline timer error in Node: %s", e.message().c_str());
    }
}

void Peer::check_activity(const system::error_code& e) {
    if (!e) {
        if(!_activity || !(rand()%3))
            _peerManager.post_stop(shared_from_this());
        else {
            _activity = false;
            _suicide.expires_from_now(posix_time::seconds(_suicide_timeout/2 + rand()%_suicide_timeout)); // ~90 minutes of activity once we have started up
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
            if (_version > BIP0031_VERSION)
                PushMessage("ping", nonce);
            else
                PushMessage("ping");
            flush();
        }
        _keep_alive.expires_from_now(posix_time::seconds(_heartbeat_timeout)); // show activity each 30 minutes
        _keep_alive.async_wait(boost::bind(&Peer::show_activity, this, asio::placeholders::error));
    }
    else if (e != error::operation_aborted) {
        log_info("Boost keep alive timer error in Peer: %s\n", e.message().c_str());
    }
    
    // we ignore abort errors - they are generated due to timer cancels
}

void Peer::stop() {
    boost::system::error_code ec;
    _suicide.cancel(ec); // no need to commit suicide when being killed
    _socket.shutdown(ip::tcp::socket::shutdown_both, ec);
    if (ec)
        log_debug("socket shutdown error: %s", ec.message()); // An error occurred.
    _socket.close(ec);
    if (ec)
        log_debug("socket close error: %s", ec.message()); // An error occurred.
}

void Peer::handle_read(const system::error_code& e, std::size_t bytes_transferred) {
    log_trace("args error: %s, bytes: %d", e.message(), bytes_transferred);
    if (!e && bytes_transferred > 0) {
        _activity = true;
        _recv.commit(bytes_transferred);

        // Now call the parser:
        bool fRet = false;
        loop {
            boost::tuple<boost::tribool, streamsize> parser_result = _msgParser.parse(_chain, _message, _recv);
            tribool result = get<0>(parser_result);
            if (result) {
                if (_messageHandler.handleMessage(this, _message) ) fRet = true;
            }
            else if (!result) {
                log_warn("Peer %s sending bogus - disconnecting", _endpoint.toString());
                _peerManager.post_stop(shared_from_this());
            }//                continue; // basically, if we get a false result, we should consider to disconnect from the Peer!
            else
                break;
        }

        // now if fRet is true, something were processed by the filters - we want to send to the peers / we check for which vSends cointains stuff and the we run
        
        if (fRet && _version > 0) {
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
        _socket.async_read_some(buffer(_recv.prepare(2048)), boost::bind(&Peer::handle_read, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
    }
    else if (e != error::operation_aborted) {
        log_debug("Read error %s, disconnecting... (read %d bytes though) \n", e.message().c_str(), bytes_transferred);
        _peerManager.post_stop(shared_from_this());
    }
    else
        log_debug("not handled : %s", e.message());
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
    vAddr.reserve(_endpointBuffer.size());
    BOOST_FOREACH(const Endpoint& addr, _endpointBuffer) {
        // returns true if wasn't already contained in the set
        if (_knownEndpoints.insert(addr).second) {
            vAddr.push_back(addr);
            // receiver rejects addr messages larger than 1000
            if (vAddr.size() >= 1000) {
                PushMessage("addr", vAddr);
                vAddr.clear();
            }
        }
    }
    _endpointBuffer.clear();
    if (!vAddr.empty())
        PushMessage("addr", vAddr);
    
    //      Message: inventory
    vector<Inventory> vInv;
//    vInv.reserve(vInventoryToSend.size());
    vector<int> sent;
    int i = 0;
    BOOST_FOREACH(const Inventory& inv, vInventoryToSend) {
        if (_knownInventory.count(inv)) {
            sent.push_back(i);
            continue;
        }
        
        // returns true if wasn't already contained in the set
        if (_knownInventory.insert(inv).second) {
            vInv.push_back(inv);
            sent.push_back(i);
            if (vInv.size() >= 1000) {
                PushMessage("inv", vInv);
                vInv.clear();
            }
        }
        i++;
    }
    if (!vInv.empty())
        PushMessage("inv", vInv);
    
    for (vector<int>::const_reverse_iterator r = sent.rbegin(); r != sent.rend(); ++r)
        vInventoryToSend.erase(vInventoryToSend.begin() + *r);
}

void Peer::broadcast() {
    vector<Inventory> vInv;
    vector<Inventory> vInvWait;
    vInv.reserve(vInventoryToSend.size());
    vInvWait.reserve(vInventoryToSend.size());
    BOOST_FOREACH(const Inventory& inv, vInventoryToSend) {
        if (_knownInventory.count(inv))
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
        if (_knownInventory.insert(inv).second) {
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
    if (_send.size()) {
        async_write(_socket, _send.data(), boost::bind(&Peer::handle_write, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
//        _socket.async_write_some(_send.data(), boost::bind(&Peer::handle_write, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
    }
}

void Peer::handle_write(const system::error_code& e, size_t bytes_transferred) {
    log_trace("args error: %s, bytes: %d", e.message(), bytes_transferred);
    if (!e) {
        // you need show you activity to avoid disconnection
        //        _activity = true;
        _send.consume(bytes_transferred);
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
    _knownEndpoints.insert(addr);
}

void Peer::PushAddress(const Endpoint& addr) {
    // Known checking here is only to save space from duplicates.
    // SendMessages will filter it again for knowns that were added
    // after addresses were pushed.
    if (addr.isValid() && !_knownEndpoints.count(addr))
        _endpointBuffer.push_back(addr);
}

void Peer::AddInventoryKnown(const Inventory& inv) {
    _knownInventory.insert(inv);
}

void Peer::push(const Inventory& inv, bool force) {
    if (force || !_knownInventory.count(inv)) {
        vInventoryToSend.push_back(inv);
    }
    if (force)
        flush();
}

void Peer::push(const Transaction& txn) {
    if(!relayTxes)
        return;
    if (filter.isRelevantAndUpdate(txn))
        PushMessage("tx", txn);
}

void Peer::push(const std::string& command, const std::string& str) {
    ostream os(&_send);
    os << MessageHeader(_chain, command, str);
    os.write(&str[0], str.size());
}
void Peer::PushVersion() {
    PushMessage("version", *this);
}

void Peer::PushGetBlocks(const BlockLocator locatorBegin, uint256 hashEnd)
{
    std::ostringstream os;

    // Filter out duplicate requests
    if (locatorBegin == locatorLastGetBlocksBegin && hashEnd == hashLastGetBlocksEnd)
        return;
    locatorLastGetBlocksBegin = locatorBegin;
    hashLastGetBlocksEnd = hashEnd;

    log_debug("PushGetBlocks to %s, hashEnd: %s", _endpoint.toString(), hashEnd.toString());
    
    os << const_binary<int>(version()) << locatorBegin << hashEnd;
    push("getblocks", os.str());
}


