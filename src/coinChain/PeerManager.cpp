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

#include <coinChain/PeerManager.h>
#include <coinChain/Peer.h>
#include <coinChain/Node.h>

#include <algorithm>
#include <boost/bind.hpp>
#include <boost/asio.hpp>

using namespace std;
using namespace boost;

PeerManager::PeerManager(Node& node) : _peerBlockCounts(5, node.blockChain().chain().totalBlocksEstimate()), _node(node) {}

void PeerManager::start(peer_ptr p) {
    _peers.insert(p);
    p->start();
}

void PeerManager::post_stop(peer_ptr p) {
    _node.post_stop(p);
}

void PeerManager::stop(peer_ptr p) {
    p->stop();
    _peers.erase(p);
    // we have stopped a node - we need to check if we need to connect to another node now.
    _node.post_accept_or_connect();
}

void PeerManager::stop_all() {
    for_each(_peers.begin(), _peers.end(), boost::bind(&Peer::stop, _1));
    _peers.clear();
}

int PeerManager::prioritize(const Inventory& inv) {
    Priorities::const_iterator req = _priorities.find(inv);
    int at = UnixTime::s();
    if (req != _priorities.end())
        at = req->second + _retry_delay;
    _priorities[inv] = at;
    return at;
}

void PeerManager::dequeue(const Inventory& inv) {
    _priorities.erase(inv);
}

bool PeerManager::queued(const Inventory& inv) const {
    return ( _priorities.find(inv) != _priorities.end());
}


void PeerManager::ready(peer_ptr peer) {
    _node.post_ready(peer);
}

int PeerManager::getBestHeight() const {
    return _node.blockChain().getBestHeight();
}


const set<unsigned int> PeerManager::getPeerIPList() const {
    // iterate the list of peers and accumulate their IPv4s
    set<unsigned int> ips;
    for (Peers::const_iterator peer = _peers.begin(); peer != _peers.end(); ++peer) {
        system::error_code ec;
        const asio::ip::tcp::endpoint& ep = (*peer)->socket().remote_endpoint(ec);
        if(!ec)
            ips.insert(ep.address().to_v4().to_ulong());
        else
            log_warn("WARNING: non-bound Peers in the Peer list ???: %s", ec.message().c_str());
    }
    return ips;
}

const unsigned int PeerManager::getNumOutbound() const {
    unsigned int outbound = 0;
    for (Peers::const_iterator peer = _peers.begin(); peer != _peers.end(); ++peer) {
        if(!(*peer)->fInbound) outbound++;
    }
    return outbound;
}

const unsigned int PeerManager::getNumInbound() const {
    unsigned int inbound = 0;
    for (Peers::const_iterator peer = _peers.begin(); peer != _peers.end(); ++peer) {
        if((*peer)->fInbound) inbound++;
    }
    return inbound;    
}
