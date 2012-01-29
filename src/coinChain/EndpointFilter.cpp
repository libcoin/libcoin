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

#include "coinChain/EndpointFilter.h"
#include "coinChain/EndpointPool.h"
#include "coinChain/Peer.h"

#include <string>

using namespace std;
using namespace boost;

bool EndpointFilter::operator()(Peer* origin, Message& msg) {
    if (origin->nVersion == 0) {
        throw OriginNotReady();
    }
    if (msg.command() == "addr") {
        vector<Endpoint> endpoints;
        CDataStream data(msg.payload());
        data >> endpoints;
        
        // Don't want addr from older versions unless seeding
        if (origin->nVersion < 209)
            return true;
        if (origin->nVersion < 31402 && _endpointPool.getPoolSize() > 1000)
            return true;
        if (endpoints.size() > 1000)
            return error("message addr size() = %d", endpoints.size());
        
        // Store the new addresses
        //        CAddrDB addrDB;
        //addrDB.TxnBegin();
        int64 now = GetAdjustedTime();
        int64 since = now - 10 * 60;
        BOOST_FOREACH(Endpoint& ep, endpoints) {
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
                uint256 hashRand = hashSalt ^ (((int64)ep.getIP())<<32) ^ ((GetTime() + ep.getIP())/(24*60*60));
                hashRand = Hash(BEGIN(hashRand), END(hashRand));
                multimap<uint256, Peer*> mapMix;
                Peers peers = origin->getAllPeers();
                for(Peers::iterator peer = peers.begin(); peer != peers.end(); ++peer) { // vNodes is a list kept in the peerManager - the peerManager is referenced by the Node and the Peer - but we could query it - e.g. from the Node ??
                    if ((*peer)->nVersion < 31402)
                        continue;
                    unsigned int nPointer;
                    memcpy(&nPointer, &(*peer), sizeof(nPointer));
                    uint256 hashKey = hashRand ^ nPointer;
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
    
    // Update the last seen time for this node's address
    if (origin->fNetworkNode)
        if (msg.command() == "version" || msg.command() == "addr" || msg.command() == "inv" || msg.command() == "getdata" || msg.command() == "ping")
            _endpointPool.currentlyConnected(origin->addr);
    
    // Address refresh broadcast - has been moved into the EndpointFilter... - this means that it is only induced by some of the commands registered to this filter, but it is a 24hours recheck, so it should not matter.
    static int64 nLastRebroadcast;
    if (GetTime() - nLastRebroadcast > 24 * 60 * 60) {
        nLastRebroadcast = GetTime();
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


