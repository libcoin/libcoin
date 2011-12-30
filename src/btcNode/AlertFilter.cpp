
#include "btcNode/AlertFilter.h"
#include "btcNode/Alert.h"

#include "btcNode/net.h"

using namespace std;
using namespace boost;

bool AlertFilter::operator() (CNode* origin, Message& msg) {
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
            CRITICAL_BLOCK(cs_vNodes)
                BOOST_FOREACH(CNode* pnode, vNodes)
                    alert.relayTo(pnode);
        }
    }
}

