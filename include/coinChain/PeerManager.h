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

#ifndef PEER_MANAGER_H
#define PEER_MANAGER_H

#include <coin/util.h>
#include <coinChain/Export.h>

#include <set>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

class Peer;
class Node;

typedef boost::shared_ptr<Peer> peer_ptr;
typedef std::set<peer_ptr> Peers;

/// Manages open connections to peers so that they may be cleanly stopped when the Nodes
/// needs to shut down.
class COINCHAIN_EXPORT PeerManager : private boost::noncopyable
{
public:
    /// Constructor - we register the Node as a delegate to enable spawning of new Peers when the old ones die
    PeerManager(Node& node) : _peerBlockCounts(5, 0), _node(node) {}
    
    /// Add the specified connection to the manager and start it.
    void start(peer_ptr p);
    
    /// Stop the specified connection.
    void stop(peer_ptr p);
    void post_stop(peer_ptr p);
    
    /// Stop all connections.
    void stop_all();
    
    /// Returns a list of platform specific unsigned ints that contains the ipv4 addresses of connected peers
    const std::set<unsigned int> getPeerIPList() const;
    
    /// Returns the list of peers
    Peers getPeerList() { return _peers; }
    
    /// Returns the number of outbound connections from this node
    const unsigned int getNumOutbound() const;
    
    /// Returns the number of inbound connections from this node
    const unsigned int getNumInbound() const;
    
    /// Returns all the peers
    Peers getAllPeers() const { return _peers; }

    /// Get the median count of blocks in the last five connected peers.
    int getPeerMedianNumBlocks() const { return _peerBlockCounts.median(); }

    /// Record the block count in a newly connect peer.
    void recordPeerBlockCount(int height) { _peerBlockCounts.input(height); }
    
private:
    /// The managed connections.
    Peers _peers;
    
    /// Amount of blocks that the Peers claim to have.
    CMedianFilter<int> _peerBlockCounts;
    
    /// The delegate
    Node& _node; 
};

#endif // PEER_MANAGER_H
