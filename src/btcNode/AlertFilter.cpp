
#include "btcNode/AlertFilter.h"
#include "btcNode/Alert.h"

#include "btcNode/net.h"

using namespace std;
using namespace boost;

bool AlertFilter::operator() (Peer* origin, Message& msg) {
    if (origin->nVersion == 0) {
        throw OriginNotReady();
    }
    if (msg.command() == "alert") { // alert might need to be created with something; all peers/Node, like node->broadcast(alert)
        Alert alert;
        CDataStream data(msg.payload());
        data >> alert;
        
        if (alert.processAlert()) {
            // Relay
            origin->setKnown.insert(alert.getHash());
#ifdef _LIBBTC_ASIO_
            Peers peers = origin->getAllPeers();
            for (Peers::iterator peer = peers.begin(); peer != peers.end(); ++peer) {
                alert.relayTo(*peer);
            }
#else
            CRITICAL_BLOCK(cs_vNodes)
                BOOST_FOREACH(Peer* pnode, vNodes)
                    alert.relayTo(pnode);
#endif
        }
    }
}

