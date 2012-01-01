#ifdef _LIBBTC_ASIO_

#include "btcNode/PeerManager.h"
#include <algorithm>
#include <boost/bind.hpp>

using namespace std;
using namespace boost;

void PeerManager::start(peer_ptr p) {
    _peers.insert(p);
    p->start();
}

void PeerManager::stop(peer_ptr p) {
    _peers.erase(p);
    p->stop();
}

void PeerManager::stop_all() {
    for_each(_peers.begin(), _peers.end(), bind(&Peer::stop, _1));
    _peers.clear();
}

const set<unsigned int>& getPeerIPList() const {
    // iterate the list of peers and accumulate their IPv4s
    set<unsigned int> ips;
    for (Peer::const_iterator peer = _peers.begin(); peer != _peers.end(); ++peer) {
        ips.insert(peer->socket().remote_endpoint().address().to_v4().to_ulong());
    }
    return ips;
}

const unsigned int getNumOutbound() const {
    unsigned int outbound = 0;
    for (Peer::const_iterator peer = _peers.begin(); peer != _peers.end(); ++peer) {
        if(!peer->fInbound) outbound++;
    }
    return outbound;
}

const unsigned int getNumInbound() const {
    unsigned int inbound = 0;
    for (Peer::const_iterator peer = _peers.begin(); peer != _peers.end(); ++peer) {
        if(peer->fInbound) inbound++;
    }
    return inbound;    
}

#endif // _LIBBTC_ASIO_
