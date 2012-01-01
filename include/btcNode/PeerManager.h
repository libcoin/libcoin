#ifdef _LIBBTC_ASIO_

#ifndef PEER_MANAGER_H
#define PEER_MANAGER_H

#include <set>
#include <boost/noncopyable.hpp>
#include "btcHTTP/Connection.h"

class Peer;

typedef boost::shared_ptr<Peer> peer_ptr;
typedef std::set<peer_ptr> Peers;

/// Manages open connections to peers so that they may be cleanly stopped when the Nodes
/// needs to shut down.
class PeerManager : private boost::noncopyable
{
public:
    /// Add the specified connection to the manager and start it.
    void start(peer_ptr p);
    
    /// Stop the specified connection.
    void stop(peer_ptr p);
    
    /// Stop all connections.
    void stop_all();
    
    /// Returns a list of platform specific unsigned ints that contains the ipv4 addresses of connected peers
    const std::set<unsigned int>& getPeerIPList() const;
    
    /// Returns the number of outbound connections from this node
    const unsigned int getNumOutbound() const;
    
    /// Returns the number of inbound connections from this node
    const unsigned int getNumInbound() const;
    
    /// Returns all the peers
    Peers getAllPeers() { return _peers; }
    
private:
    /// The managed connections.
    Peers _peers;
};

#endif // PEER_MANAGER_H
#endif // _LIBBTC_ASIO_
