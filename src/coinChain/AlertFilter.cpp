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

#include <coinChain/AlertFilter.h>
#include <coinChain/Alert.h>

#include <coinChain/Peer.h>

using namespace std;
using namespace boost;

bool AlertFilter::operator() (Peer* origin, Message& msg) {
    if (origin->version() == 0) {
        throw OriginNotReady();
    }
    if (msg.command() == "alert") { // alert might need to be created with something; all peers/Node, like node->broadcast(alert)
        Alert alert(_pub_key);
        istringstream is(msg.payload());
        is >> alert;
        
        if (alert.processAlert()) {
            // Relay
            origin->setKnown.insert(alert.getHash());
            Peers peers = origin->getAllPeers();
            if (alert.isInEffect()) {
                for (Peers::iterator peer = peers.begin(); peer != peers.end(); ++peer) {
                    // returns true if wasn't already contained in the set
                    if ((*peer)->setKnown.insert(alert.getHash()).second) {
                        if (alert.appliesTo((*peer)->version(), (*peer)->sub_version()) ||
                            alert.appliesTo(_version, _sub_version) ||
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
                        if (alert.appliesTo(origin->version(), origin->sub_version()) || alert.appliesTo(_version, _sub_version) || GetAdjustedTime() < alert.until())
                            origin->PushMessage("alert", alert);
                    }
                }
            }
    }
    return true;
}
