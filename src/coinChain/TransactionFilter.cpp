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
    if (origin->nVersion == 0) {
        throw OriginNotReady();
    }    
    if (msg.command() == "tx") {        
        CDataStream data(msg.payload());
        //        CDataStream payload(msg.payload());
        Transaction tx;
        data >> tx;
        
        Inventory inv(MSG_TX, tx.getHash());
        origin->AddInventoryKnown(inv);
        
        process(tx, origin->getAllPeers());
    }
    else if (msg.command() == "getdata") {
        vector<Inventory> vInv;
        CDataStream data(msg.payload());
        data >> vInv;
        if (vInv.size() > 50000)
            return error("message getdata size() = %d", vInv.size());
        
        BOOST_FOREACH(const Inventory& inv, vInv) {
            log_debug("received getdata for: %s\n", inv.toString().c_str());
            
            if (inv.getType() == MSG_TX) {
                // Send stream from relay memory
                map<Inventory, CDataStream>::iterator mi = _relay.find(inv);
                if (mi != _relay.end())
                    origin->PushMessage(inv.getCommand(), (*mi).second);
            }
            
            // Track requests for our stuff
            //            Inventory(inv.hash);
        }
    }
    else if (msg.command() == "inv") {
        vector<Inventory> vInv;
        CDataStream data(msg.payload());
        data >> vInv;
        if (vInv.size() > 50000)
            return error("message inv size() = %d", vInv.size());
        
        //        CTxDB txdb("r");
        BOOST_FOREACH(const Inventory& inv, vInv) {
            if (inv.getType() == MSG_TX) {
                origin->AddInventoryKnown(inv);
                
                bool fAlreadyHave = alreadyHave(inv);
                log_debug("  got inventory: %s  %s\n", inv.toString().c_str(), fAlreadyHave ? "have" : "new");
                
                if (!fAlreadyHave)
                    origin->AskFor(inv);
            }            
        }
    }
    
    // We could call the wallet reminders here - it is not the ideal spot ???    

    return true;
}

void TransactionFilter::process(Transaction& tx, Peers peers) {
    vector<uint256> workQueue;
    Inventory inv(MSG_TX, tx.getHash());
    CDataStream payload;
    payload << tx;
    //    bool fMissingInputs = false;
    try {
        _blockChain.claim(tx);
        log_info("accepted txn: %s", tx.getHash().toString().substr(0,10));

        for(Listeners::iterator listener = _listeners.begin(); listener != _listeners.end(); ++listener)
            (*listener->get())(tx);
        relayMessage(peers, inv, payload);
        _alreadyAskedFor.erase(inv);
        workQueue.push_back(inv.getHash());
        
        // Recursively process any orphan transactions that depended on this one
        for (int i = 0; i < workQueue.size(); i++) {
            uint256 hashPrev = workQueue[i];
            for (multimap<uint256, CDataStream*>::iterator mi = _orphanTransactionsByPrev.lower_bound(hashPrev);
                 mi != _orphanTransactionsByPrev.upper_bound(hashPrev);
                 ++mi) {
                const CDataStream& payload = *((*mi).second);
                Transaction tx;
                CDataStream(payload) >> tx;
                Inventory inv(MSG_TX, tx.getHash());
                
                try {
                    _blockChain.claim(tx);
                    for(Listeners::iterator listener = _listeners.begin(); listener != _listeners.end(); ++listener)
                        (*listener->get())(tx);
                    log_info("   accepted orphan tx %s", inv.getHash().toString().substr(0,10).c_str());
                    //                        SyncWithWallets(tx, NULL, true);
                    relayMessage(peers, inv, payload);
                    _alreadyAskedFor.erase(inv);
                    workQueue.push_back(inv.getHash());
                }
                catch (...) {
                    
                }
            }
        }
        
        BOOST_FOREACH(uint256 hash, workQueue)
        eraseOrphanTx(hash);
    }
    catch (BlockChain::Reject& e) {
        log_info("acceptTransaction Warning: %s", e.what());
        log_debug("storing orphan tx %s", inv.getHash().toString().substr(0,10).c_str());
        addOrphanTx(tx);
    }
    catch (BlockChain::Error& e) {
        log_info("acceptTransaction Error: %s", e.what());
    }
}


// Private methods
void TransactionFilter::addOrphanTx(const Transaction& tx) {
    uint256 hash = tx.getHash();
    CDataStream payload;
    payload << tx;
    if (_orphanTransactions.count(hash))
        return;
    CDataStream* p = _orphanTransactions[hash] = new CDataStream(payload);
    BOOST_FOREACH(const Input& txin, tx.getInputs())
    _orphanTransactionsByPrev.insert(make_pair(txin.prevout().hash, p));
}

void TransactionFilter::eraseOrphanTx(uint256 hash) {
    if (!_orphanTransactions.count(hash))
        return;
    const CDataStream* p = _orphanTransactions[hash];
    Transaction tx;
    CDataStream(*p) >> tx;
    BOOST_FOREACH(const Input& txin, tx.getInputs()) {
        for (multimap<uint256, CDataStream*>::iterator mi = _orphanTransactionsByPrev.lower_bound(txin.prevout().hash);
             mi != _orphanTransactionsByPrev.upper_bound(txin.prevout().hash);) {
            if ((*mi).second == p)
                _orphanTransactionsByPrev.erase(mi++);
            else
                mi++;
        }
    }
    delete p;
    _orphanTransactions.erase(hash);
}

bool TransactionFilter::alreadyHave(const Inventory& inv) {
    if (inv.getType() == MSG_TX)
        return _orphanTransactions.count(inv.getHash()) || _blockChain.haveTx(inv.getHash());
    else // Don't know what it is, just say we already got one
        return true;
}


// The Relay system is only used for Transactions - hence we put it here.

inline void TransactionFilter::relayInventory(const Peers& peers, const Inventory& inv) {
    // Put on lists to offer to the other nodes
    for(Peers::const_iterator peer = peers.begin(); peer != peers.end(); ++peer)
        (*peer)->PushInventory(inv);
}

template<typename T> void TransactionFilter::relayMessage(const Peers& peers, const Inventory& inv, const T& a) {
    CDataStream ss(SER_NETWORK);
    ss.reserve(10000);
    ss << a;
    relayMessage(peers, inv, ss);
}

template<> inline void TransactionFilter::relayMessage<>(const Peers& peers, const Inventory& inv, const CDataStream& ss) {
    // Expire old relay messages
    while (!_relayExpiration.empty() && _relayExpiration.front().first < GetTime()) {
        _relay.erase(_relayExpiration.front().second);
        _relayExpiration.pop_front();
    }
    
    // Save original serialized message so newer versions are preserved
    _relay[inv] = ss;
    _relayExpiration.push_back(std::make_pair(GetTime() + 15 * 60, inv));
    
    relayInventory(peers, inv);
}