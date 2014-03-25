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

#include <coinChain/EndpointFilter.h>
#include <coinChain/EndpointPool.h>
#include <coinChain/Peer.h>

#include <string>

using namespace std;
using namespace boost;

bool EndpointFilter::operator()(Peer* origin, Message& msg) {
    log_trace("args: origin: %s, msg: %s", origin->addr.toString(), msg.command());
    if (origin->nVersion == 0) {
        throw OriginNotReady();
    }
    if (msg.command() == "addr") {
        vector<Endpoint> endpoints;

        istringstream is(msg.payload());
        is >> endpoints;
        
        // Don't want addr from older versions unless seeding
        if (origin->nVersion < 209)
            return true;
        if (origin->nVersion < 31402 && _endpointPool.getPoolSize() > 1000)
            return true;
        if (endpoints.size() > 1000)
            return error("message addr size() = %d", endpoints.size());
        
        log_debug("PEERS: %d addresses received from %s", endpoints.size(), origin->addr.toString());
        
        // Store the new addresses
        //        CAddrDB addrDB;
        //addrDB.TxnBegin();
        int64_t now = GetAdjustedTime();
        int64_t since = now - 10 * 60;
        BOOST_FOREACH(Endpoint& ep, endpoints) {
            log_trace("adding endpoint: %s", ep.toString());
            // ignore IPv6 for now, since it isn't implemented anyway
            if (!ep.isIPv4())
                continue;
            if (ep.getTime() <= 100000000 || ep.getTime() > now + 10 * 60)
                ep.setTime(now - 5 * 24 * 60 * 60);
            _endpointPool.addEndpoint(ep, 2 * 60 * 60);
            origin->AddAddressKnown(ep);
            
            // TODO: Rewrite this to instead query the Node for the proper set of peers, and send the addresses to them using peer->sendMessage("command", payload);
            
            if (ep.getTime() > since && !origin->fGetAddr && endpoints.size() <= 10 && ep.isRoutable()) {
                // Relay to a limited number of other nodes
                // Use deterministic randomness to send to the same nodes for 24 hours
                // at a time so the setAddrKnowns of the chosen nodes prevent repeats
                static uint256 hashSalt; // belongs to the Handler/Node...
                if (hashSalt == 0)
                    RAND_bytes((unsigned char*)&hashSalt, sizeof(hashSalt));
                uint256 hashRand = hashSalt ^ uint256(((int64_t)ep.getIP())<<32) ^ uint256((UnixTime::s() + ep.getIP())/(24*60*60));
                hashRand = Hash(BEGIN(hashRand), END(hashRand));
                multimap<uint256, Peer*> mapMix;
                Peers peers = origin->getAllPeers();
                for(Peers::iterator peer = peers.begin(); peer != peers.end(); ++peer) { // vNodes is a list kept in the peerManager - the peerManager is referenced by the Node and the Peer - but we could query it - e.g. from the Node ??
                    if ((*peer)->nVersion < 31402)
                        continue;
                    unsigned int nPointer;
                    memcpy(&nPointer, &(*peer), sizeof(nPointer));
                    uint256 hashKey = hashRand ^ uint256(nPointer);
                    hashKey = Hash(BEGIN(hashKey), END(hashKey));
                    mapMix.insert(make_pair(hashKey, peer->get()));
                }
                int nRelayNodes = 2;
                for (multimap<uint256, Peer*>::iterator mi = mapMix.begin(); mi != mapMix.end() && nRelayNodes-- > 0; ++mi)
                    ((*mi).second)->PushAddress(ep);
            }
        }
        //        addrDB.TxnCommit();  // Save addresses (it's ok if this fails)
        if (endpoints.size() < 1000)
            origin->fGetAddr = false;        
    }
    else if (msg.command() == "getaddr") {
        // Nodes rebroadcast an addr every 24 hours
        origin->vAddrToSend.clear();
        set<Endpoint> endpoints = _endpointPool.getRecent(3*60*60, 2500);
        for (set<Endpoint>::iterator ep = endpoints.begin(); ep != endpoints.end(); ++ep)
            origin->PushAddress(*ep);
    }
    else if (msg.command() == "version") {
        if (!origin->fInbound) {
            // Advertise our address
            if (_endpointPool.getLocal().isRoutable() && !origin->getProxy()) {
                Endpoint addr(_endpointPool.getLocal());
                addr.setTime(GetAdjustedTime());
                origin->PushAddress(addr);
            }
            
            // Get recent addresses
            if (origin->nVersion >= 31402 || _endpointPool.getPoolSize() < 1000) {
                origin->PushMessage("getaddr");
                origin->fGetAddr = true;
            }
        }
    }
    else if (msg.command() == "ping") {
        if (origin->nVersion > BIP0031_VERSION) {
            uint64_t nonce = 0;

            istringstream is(msg.payload());
            is >> binary<uint64_t>(nonce);

            // Echo the message back with the nonce. This allows for two useful features:
            //
            // 1) A remote node can quickly check if the connection is operational
            // 2) Remote nodes can measure the latency of the network thread. If this node
            //    is overloaded it won't respond to pings quickly and the remote node can
            //    avoid sending us more work, like chain download requests.
            //
            // The nonce stops the remote getting confused between different pings: without
            // it, if the remote node sends a ping once per second and this node takes 5
            // seconds to respond to each, the 5th ping the remote sends would appear to
            // return very quickly.
            origin->PushMessage("pong", nonce);
        }
    }
    
    
    // Update the last seen time for this node's address
    if (origin->fNetworkNode)
        if (msg.command() == "version" || msg.command() == "addr" || msg.command() == "inv" || msg.command() == "getdata" || msg.command() == "ping")
            _endpointPool.currentlyConnected(origin->addr);
    
    // Endpoint refresh broadcast - has been moved into the EndpointFilter... - this means that it is only induced by some of the commands registered to this filter, but it is a 24hours recheck, so it should not matter.
    static int64_t nLastRebroadcast;
    if (UnixTime::s() - nLastRebroadcast > 24 * 60 * 60) {
        nLastRebroadcast = UnixTime::s();
        // Periodically clear setAddrKnown to allow refresh broadcasts
        origin->setAddrKnown.clear();
        
        // Rebroadcast our address
        if (_endpointPool.getLocal().isRoutable() && origin->getProxy()) {
            Endpoint addr(_endpointPool.getLocal());
            addr.setTime(GetAdjustedTime());
            origin->PushAddress(addr);
        }
    }
        
    // Clear out old addresses periodically so it's not too much work at once.
    if (origin->getAllPeers().size() >= 3)
        _endpointPool.purge();

    return true;
}


