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
#include <coinChain/Inventory.h>

#include <set>
#include <map>
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
    PeerManager(Node& node);
    
    /// Add the specified connection to the manager.
    void manage(peer_ptr p);
    
    /// called from the peer to notify that a connection attempt was cancelled
    void cancel(peer_ptr p);
    
    /// Stop the specified connection.
    void stop(peer_ptr p);
    void post_stop(peer_ptr p);
    
    /// Stop all connections.
    void stop_all();
    
    /// Returns a list of platform specific unsigned ints that contains the ipv4 addresses of connected peers
    const std::set<unsigned int> getPeerIPList() const;
    
    /// Returns the number of outbound connections from this node
    const unsigned int getNumOutbound(bool pending = false) const;
    
    /// Returns the number of inbound connections from this node
    const unsigned int getNumInbound() const;
    
    /// Returns all the connected peers
    Peers getAllPeers() const;

    /// Prioritize when to request this inventory - if already requested the returned time will be last queued request + 2min,  else it is now
    int prioritize(const Inventory& inv);
    
    /// Dequeue an inventory request, most like called when the inventory was received
    void dequeue(const Inventory& inv);
    
    /// Query if this inventory is already requested
    bool queued(const Inventory& inv) const;
    
    /// Get the median count of blocks in the last five connected peers.
    int getPeerMedianNumBlocks() const { return _peerBlockCounts.median(); }

    /// Record the block count in a newly connect peer.
    void recordPeerBlockCount(int height) { _peerBlockCounts.input(height); }
    
    /// Notify the connection of a new peer
    void ready(peer_ptr peer);
    
    int getBestHeight() const;
    
private:
    /// The managed connections.
    Peers _peers;
    
    /// The pending inventory
    typedef std::map<Inventory, int> Priorities;
    Priorities _priorities;
    
    // Median filter over a stream of values
    // Returns the median of the last N numbers
    template <typename T> class MedianFilter {
    private:
        std::vector<T> _values;
        std::vector<T> _sorted;
        unsigned int _size;
    public:
        MedianFilter(unsigned int size, T initial_value): _size(size) {
            _values.reserve(size);
            _values.push_back(initial_value);
            _sorted = _values;
        }
        
        void input(T value) {
            if(_values.size() == _size) {
                _values.erase(_values.begin());
            }
            _values.push_back(value);
            
            _sorted.resize(_values.size());
            std::copy(_values.begin(), _values.end(), _sorted.begin());
            std::sort(_sorted.begin(), _sorted.end());
        }
        
        T median() const {
            int size = _sorted.size();
            assert(size > 0);
            if(size & 1) { // Odd number of elements
                return _sorted[size/2];
            }
            else { // Even number of elements
                return (_sorted[size/2-1] + _sorted[size/2]) / 2;
            }
        }
        
        unsigned int size() const {
            return _values.size();
        }
        
        std::vector<T> sorted () const {
            return _sorted;
        }
    };
    
    /// Amount of blocks that the Peers claim to have.
    MedianFilter<int> _peerBlockCounts;
    
    /// The delegate
    Node& _node;
    
    static const int _retry_delay = 45;
};

#endif // PEER_MANAGER_H
