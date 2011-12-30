
#include "btcNode/EndpointFilter.h"
#include "btcNode/EndpointPool.h"

#include "btcNode/net.h"

#include <string>

using namespace std;
using namespace boost;

bool EndpointFilter::operator()(CNode* origin, Message& msg) {
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
            if (fShutdown)
                return true;
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
                CRITICAL_BLOCK(cs_vNodes) {
                    // Use deterministic randomness to send to the same nodes for 24 hours
                    // at a time so the setAddrKnowns of the chosen nodes prevent repeats
                    static uint256 hashSalt; // belongs to the Handler/Node...
                    if (hashSalt == 0)
                        RAND_bytes((unsigned char*)&hashSalt, sizeof(hashSalt));
                    uint256 hashRand = hashSalt ^ (((int64)ep.getIP())<<32) ^ ((GetTime() + ep.getIP())/(24*60*60));
                    hashRand = Hash(BEGIN(hashRand), END(hashRand));
                    multimap<uint256, CNode*> mapMix;
                    BOOST_FOREACH(CNode* pnode, vNodes) { // vNodes is a list kept in the peerManager - the peerManager is referenced by the Node and the Peer - but we could query it - e.g. from the Node ??
                        if (pnode->nVersion < 31402)
                            continue;
                        unsigned int nPointer;
                        memcpy(&nPointer, &pnode, sizeof(nPointer));
                        uint256 hashKey = hashRand ^ nPointer;
                        hashKey = Hash(BEGIN(hashKey), END(hashKey));
                        mapMix.insert(make_pair(hashKey, pnode));
                    }
                    int nRelayNodes = 2;
                    for (multimap<uint256, CNode*>::iterator mi = mapMix.begin(); mi != mapMix.end() && nRelayNodes-- > 0; ++mi)
                        ((*mi).second)->PushAddress(ep);
                }
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
    
    // Update the last seen time for this node's address
    if (origin->fNetworkNode)
        if (msg.command() == "version" || msg.command() == "addr" || msg.command() == "inv" || msg.command() == "getdata" || msg.command() == "ping")
            _endpointPool.currentlyConnected(origin->addr);
    
    return true;
}


