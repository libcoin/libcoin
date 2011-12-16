
#include "btcNode/TransactionFilter.h"

#include "btcNode/net.h"

using namespace std;
using namespace boost;

bool TransactionFilter::operator()(Message& msg) {
    if (msg.origin->nVersion == 0) {
        throw OriginNotReady();
    }
    if (msg.command == "tx") {        
        vector<uint256> workQueue;
        CDataStream payload(msg.payload);
        Transaction tx;
        msg.payload >> tx;
        
        Inventory inv(MSG_TX, tx.GetHash());
        msg.origin->AddInventoryKnown(inv);
        
        bool fMissingInputs = false;
        if (_blockChain.acceptTransaction(tx, fMissingInputs)) {
            relayMessage(inv, payload);
            mapAlreadyAskedFor.erase(inv);
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
                    Inventory inv(MSG_TX, tx.GetHash());
                    
                    if (_blockChain.acceptTransaction(tx)){
                        printf("   accepted orphan tx %s\n", inv.getHash().toString().substr(0,10).c_str());
                        //                        SyncWithWallets(tx, NULL, true);
                        relayMessage(inv, payload);
                        mapAlreadyAskedFor.erase(inv);
                        workQueue.push_back(inv.getHash());
                    }
                }
            }
            
            BOOST_FOREACH(uint256 hash, workQueue)
                eraseOrphanTx(hash);
        }
        else if (fMissingInputs) {
            printf("storing orphan tx %s\n", inv.getHash().toString().substr(0,10).c_str());
            addOrphanTx(payload);
        }
    }
    else if (msg.command == "getdata") {
        vector<Inventory> vInv;
        msg.payload >> vInv;
        if (vInv.size() > 50000)
            return error("message getdata size() = %d", vInv.size());
        
        BOOST_FOREACH(const Inventory& inv, vInv) {
            if (fShutdown)
                return true;
            printf("received getdata for: %s\n", inv.toString().c_str());
            
            if (inv.getType() == MSG_TX) {
                // Send stream from relay memory
                CRITICAL_BLOCK(cs_mapRelay) {
                    map<Inventory, CDataStream>::iterator mi = _relay.find(inv);
                    if (mi != _relay.end())
                        msg.origin->PushMessage(inv.getCommand(), (*mi).second);
                }
            }
            
            // Track requests for our stuff
            //            Inventory(inv.hash);
        }
    }
    else if (msg.command == "inv") {
        vector<Inventory> vInv;
        msg.payload >> vInv;
        if (vInv.size() > 50000)
            return error("message inv size() = %d", vInv.size());
        
        //        CTxDB txdb("r");
        BOOST_FOREACH(const Inventory& inv, vInv) {
            if (fShutdown)
                return true;
            if (inv.getType() == MSG_TX) {
                msg.origin->AddInventoryKnown(inv);
                
                bool fAlreadyHave = alreadyHave(inv);
                printf("  got inventory: %s  %s\n", inv.toString().c_str(), fAlreadyHave ? "have" : "new");
                
                if (!fAlreadyHave)
                    msg.origin->AskFor(inv);
            }            
        }
    }
    


    return true;
}


// Private methods
void TransactionFilter::addOrphanTx(const CDataStream& payload) {
    Transaction tx;
    CDataStream(payload) >> tx;
    uint256 hash = tx.GetHash();
    if (_orphanTransactions.count(hash))
        return;
    CDataStream* p = _orphanTransactions[hash] = new CDataStream(payload);
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    _orphanTransactionsByPrev.insert(make_pair(txin.prevout.hash, p));
}

void TransactionFilter::eraseOrphanTx(uint256 hash) {
    if (!_orphanTransactions.count(hash))
        return;
    const CDataStream* p = _orphanTransactions[hash];
    Transaction tx;
    CDataStream(*p) >> tx;
    BOOST_FOREACH(const CTxIn& txin, tx.vin) {
        for (multimap<uint256, CDataStream*>::iterator mi = _orphanTransactionsByPrev.lower_bound(txin.prevout.hash);
             mi != _orphanTransactionsByPrev.upper_bound(txin.prevout.hash);) {
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

inline void TransactionFilter::relayInventory(const Inventory& inv) {
    // Put on lists to offer to the other nodes
    CRITICAL_BLOCK(cs_vNodes)
    BOOST_FOREACH(CNode* pnode, vNodes)
        pnode->PushInventory(inv);
}

template<typename T> void TransactionFilter::relayMessage(const Inventory& inv, const T& a) {
    CDataStream ss(SER_NETWORK);
    ss.reserve(10000);
    ss << a;
    relayMessage(inv, ss);
}

template<> inline void TransactionFilter::relayMessage<>(const Inventory& inv, const CDataStream& ss) {
    CRITICAL_BLOCK(cs_mapRelay) {
        // Expire old relay messages
        while (!_relayExpiration.empty() && _relayExpiration.front().first < GetTime()) {
            _relay.erase(_relayExpiration.front().second);
            _relayExpiration.pop_front();
        }
        
        // Save original serialized message so newer versions are preserved
        _relay[inv] = ss;
        _relayExpiration.push_back(std::make_pair(GetTime() + 15 * 60, inv));
    }
    
    relayInventory(inv);
}