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

#include <coinChain/TransactionFilter.h>
#include <coinChain/BlockChain.h>
#include <coinChain/Peer.h>
#include <coinChain/Inventory.h>

#include <coin/Transaction.h>

using namespace std;
using namespace boost;

bool TransactionFilter::operator()(Peer* origin, Message& msg) {
    if (!origin->initialized()) {
        throw OriginNotReady();
    }    
    if (msg.command() == "tx") {
        Transaction tx;
        istringstream is(msg.payload());
        is >> tx;
        
        Inventory inv(MSG_TX, tx.getHash());

        origin->dequeue(inv);

        origin->AddInventoryKnown(inv);
        
        process(tx, origin->getAllPeers());
    }
    else if (msg.command() == "getdata") {
        vector<Inventory> vInv;
        istringstream is(msg.payload());
        is >> vInv;
        if (vInv.size() > 50000)
            return error("message getdata size() = %d", vInv.size());
        
        BOOST_FOREACH(const Inventory& inv, vInv) {
            log_debug("received getdata for: %s\n", inv.toString().c_str());
            
            if (inv.getType() == MSG_TX) {
                // Send stream from relay memory
                Relay::iterator mi = _relay.find(inv);
                if (mi != _relay.end())
                    origin->push((*mi).second);
            }
        }
    }
    else if (msg.command() == "mempool") {
        // get all claims and forward invs them to anyone querying
        vector<uint256> claims = _blockChain.claims(origin->filter);
        vector<Inventory> invs;
        
        BOOST_FOREACH(uint256& hash, claims) {
            invs.push_back(Inventory(MSG_TX, hash));
            if (invs.size() == MAX_INV_SZ) {
                origin->PushMessage("inv", invs);
                invs.clear();
            }
        }
        if (invs.size() > 0)
            origin->PushMessage("inv", invs);
    }
    else if (msg.command() == "inv") {
        vector<Inventory> vInv;
        istringstream is(msg.payload());
        is >> vInv;
        if (vInv.size() > 50000)
            return error("message inv size() = %d", vInv.size());
        
        BOOST_FOREACH(const Inventory& inv, vInv) {
            if (inv.getType() == MSG_TX) {
                origin->AddInventoryKnown(inv);
                
                bool already_have = have(inv);
                log_debug("  got inventory: %s  %s\n", inv.toString().c_str(), already_have ? "have" : "new");
                
                if (!already_have)
                    origin->queue(inv);
            }
        }
    }
    
    return true;
}

void TransactionFilter::process(Transaction& txn, Peers peers) {
    vector<uint256> workQueue;
    Inventory inv(txn);
    
    if (_blockChain.haveTx(inv.getHash())) {
        // we have it already - just relay and exit
        relay(peers, txn);
        return;
    }
    //    bool fMissingInputs = false;
    try {
        _blockChain.claim(txn);
        log_debug("accepted txn: %s", inv.getHash().toString().substr(0,10));

        for(Listeners::iterator listener = _listeners.begin(); listener != _listeners.end(); ++listener)
            (*listener->get())(txn);
        relay(peers, txn);
        workQueue.push_back(inv.getHash());
        
        // Recursively process any orphan transactions that depended on this one
        for (unsigned int i = 0; i < workQueue.size(); i++) {
            uint256 hashPrev = workQueue[i];
            for (OrphansByPrev::iterator mi = _orphansByPrev.lower_bound(hashPrev);
                 mi != _orphansByPrev.upper_bound(hashPrev);
                 ++mi) {
                Transaction& tx = mi->second->second;
                Inventory inv(tx);
                
                try {
                    _blockChain.claim(tx);
                    for(Listeners::iterator listener = _listeners.begin(); listener != _listeners.end(); ++listener)
                        (*listener->get())(tx);
                    log_debug("   accepted orphan tx %s", inv.getHash().toString().substr(0,10).c_str());
                    //                        SyncWithWallets(tx, NULL, true);
                    relay(peers, tx);
                    workQueue.push_back(inv.getHash());
                }
                catch (...) {
                    
                }
            }
        }
        
        BOOST_FOREACH(uint256 hash, workQueue)
            eraseOrphan(hash);
    }
    catch (BlockChain::Reject& e) {
        log_debug("acceptTransaction Warning: %s", e.what());
        log_debug("storing orphan tx %s", inv.getHash().toString().substr(0,10).c_str());
        addOrphan(txn);
    }
    catch (BlockChain::Error& e) {
        log_warn("acceptTransaction Error: %s", e.what());
    }
}


// Private methods
void TransactionFilter::addOrphan(const Transaction& txn) {
    uint256 hash = txn.getHash();

    pair<Orphans::iterator, bool> ret = _orphans.insert(make_pair(hash, txn));

    if (!ret.second)
        return;
    
    BOOST_FOREACH(const Input& txin, txn.getInputs())
        _orphansByPrev.insert(make_pair(txin.prevout().hash, ret.first));
}

void TransactionFilter::eraseOrphan(uint256 hash) {
    if (!_orphans.count(hash))
        return;

    Orphans::iterator o = _orphans.find(hash);
    Transaction txn = o->second;

    BOOST_FOREACH(const Input& txin, txn.getInputs()) {
        for (OrphansByPrev::iterator mi = _orphansByPrev.lower_bound(txin.prevout().hash);
             mi != _orphansByPrev.upper_bound(txin.prevout().hash);) {
            if ((*mi).second == o)
                _orphansByPrev.erase(mi++);
            else
                mi++;
        }
    }
    _orphans.erase(o);
}

bool TransactionFilter::have(const Inventory& inv) {
    if (inv.getType() == MSG_TX)
        return _orphans.count(inv.getHash()) || _blockChain.haveTx(inv.getHash());
    else // Don't know what it is, just say we already got one
        return true;
}

void TransactionFilter::relay(const Peers& peers, const Transaction& txn) {
    // Expire old relay messages
    while (!_relayExpiration.empty() && _relayExpiration.front().first < UnixTime::s()) {
        _relay.erase(_relayExpiration.front().second);
        _relayExpiration.pop_front();
    }

    Inventory inv(txn);
    
    // Save original serialized message so newer versions are preserved
    _relay[inv] = txn;
    _relayExpiration.push_back(std::make_pair(UnixTime::s() + 15 * 60, inv));
    
    // Put on lists to offer to the other nodes
    for(Peers::const_iterator peer = peers.begin(); peer != peers.end(); ++peer)
        (*peer)->push(inv);
}
