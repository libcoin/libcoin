
#include "btcNode/AlertFilter.h"
#include "btcNode/Alert.h"

#include "btcNode/Peer.h"

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
            Peers peers = origin->getAllPeers();
            if (!alert.isInEffect()) {
                for (Peers::iterator peer = peers.begin(); peer != peers.end(); ++peer) {
                    // returns true if wasn't already contained in the set
                    if ((*peer)->setKnown.insert(alert.getHash()).second) {
                        if (alert.appliesTo((*peer)->nVersion, (*peer)->strSubVer) ||
                            alert.appliesToMe() ||
                            GetAdjustedTime() < alert.until()) {
                            (*peer)->PushMessage("alert", alert);
                        }
                    }
                }
            }
        }
    }
    else if (msg.command() == "version") {
        // Relay alerts
            BOOST_FOREACH(PAIRTYPE(const uint256, Alert)& item, mapAlerts) {
                const Alert& alert = item.second;
                if (!alert.isInEffect()) {
                    if (origin->setKnown.insert(alert.getHash()).second) {
                        if (alert.appliesTo(origin->nVersion, origin->strSubVer) || alert.appliesToMe() || GetAdjustedTime() < alert.until())
                            origin->PushMessage("alert", alert);
                    }
                }
            }
    }
    return true;
}
