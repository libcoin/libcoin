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

#include <coinMine/Miner.h>
#include <coinChain/Node.h>
#include <coinMine/CPUHasher.h>

using namespace std;
using namespace boost;

Miner::Miner(Node& node, CReserveKey& reservekey) : _node(node), _idle_timer(_io_service), _update_interval(2000), _generate(false), _address(0), _pub_key(), _reserve_key(reservekey), _hashes_per_second(100000) { registerHasher(hasher_ptr(new CPUHasher)); }

Miner::Miner(Node& node, PubKey& pubkey) : _node(node), _idle_timer(_io_service), _update_interval(2000), _generate(false), _address(0), _pub_key(pubkey), _reserve_key(NULL), _hashes_per_second(100000) { registerHasher(hasher_ptr(new CPUHasher)); }


Miner::Miner(Node& node, PubKeyHash& address) : _node(node), _idle_timer(_io_service), _update_interval(2000), _generate(false), _address(address), _pub_key(), _reserve_key(NULL), _hashes_per_second(100000) { registerHasher(hasher_ptr(new CPUHasher)); }

void Miner::run() {
    _idle_timer.expires_at(posix_time::pos_infin);
    _idle_timer.async_wait(boost::bind(&Miner::handle_work, this));
    _io_service.run();
}


void Miner::setGenerate(bool gen) {
    if (gen && !_generate) { // if we are commensing generation we need to initiate the event loop
        _generate = gen;
        _io_service.post(boost::bind(&Miner::handle_generate, this));
    }
    else
        _generate = gen;
}

void Miner::handle_generate() {
    const CBlockIndex* bestIndex = _node.blockChain().getBestIndex();
    
    if(!_generate)
        return;
    // generate the coin base transaction
    Transaction tx;
    unsigned int extraNonce = 1; // dosn't really matter for anything...
    Script signature = Script() << bestIndex->nBits << CBigNum(extraNonce);
    tx.addInput(Input(Coin(), signature));
    
    Script script;
    if (_address != 0)
        script << OP_DUP << OP_HASH160 << _address << OP_EQUALVERIFY << OP_CHECKSIG;
    else if (_pub_key.size())
        script << _pub_key << OP_CHECKSIG;
    else
        script << _reserve_key.GetReservedKey() << OP_CHECKSIG;
    tx.addOutput(Output(_node.blockChain().chain().subsidy(bestIndex->nHeight+1), script));
    
    // generate a block candidate
    Block block(PROTOCOL_VERSION, bestIndex->GetBlockHash(), 0, max(bestIndex->GetMedianTimePast()+1, GetAdjustedTime()), _node.blockChain().chain().nextWorkRequired(bestIndex), 0);
    
    block.addTransaction(tx);
    fillinTransactions(block, bestIndex);
    
    // run the hasher 
    Hasher* p = NULL;
    if (_hashers.size() == 1)
        p = _hashers.begin()->get();
    else if (_override_name.size()) {
        for (Hashers::iterator i = _hashers.begin(); i != _hashers.end(); ++i) {
            if ((*i)->name() == _override_name) {
                p = i->get();                
                break;
            }
        }
    }
    else
        printf("Multiple hashers defined but automatic choosing not implemented");
    
    if(!p) {
        printf("No suitable hashers defined!!");
        return;
    }
    Hasher& hasher = *p;
    
    uint64 start_time = GetTimeMillis();
    unsigned int nonces = _update_interval*_hashes_per_second/1000;
    bool success = hasher(block, nonces);
    uint64 delta = GetTimeMillis() - start_time;                
    
    if(success) { // block found!
        uint256 hash = block.getHash();
        uint256 hashTarget = CBigNum().SetCompact(block.getBits()).getuint256();
        
        if (hash > hashTarget)
            return;
        
        //// debug print
        printf("BitcoinMiner:\n");
        printf("proof-of-work found  \n  hash: %s  \ntarget: %s\n", hash.GetHex().c_str(), hashTarget.GetHex().c_str());
        block.print();
        printf("%s ", DateTimeStrFormat("%x %H:%M", GetTime()).c_str());
        printf("generated %s\n", FormatMoney(block.getTransaction(0).getOutput(0).value()).c_str());
        
        // Remove key from key pool
        if (_pub_key.empty() && _address == 0)
            _reserve_key.KeepKey();
        
        // Process this block the same as if we had received it from another node
        _node.post(block);                    
    }
    else // only update the timing if we ran through all nonces
        _hashes_per_second = 1000*nonces/delta;
    
    // continue mining
    _io_service.post(boost::bind(&Miner::handle_generate, this));    
}

class COrphan
{
public:
    Transaction* ptx;
    set<uint256> setDependsOn;
    double dPriority;
    
    COrphan(Transaction* ptxIn) {
        ptx = ptxIn;
        dPriority = 0;
    }
    
    void print() const {
        printf("COrphan(hash=%s, dPriority=%.1f)\n", ptx->getHash().toString().substr(0,10).c_str(), dPriority);
        BOOST_FOREACH(uint256 hash, setDependsOn)
        printf("   setDependsOn %s\n", hash.toString().substr(0,10).c_str());
    }
};

void Miner::fillinTransactions(Block& block, const CBlockIndex* prev) {
    // Collect memory pool transactions into the block
    int64 nFees = 0;
    
    // Priority order to process transactions
    list<COrphan> vOrphan; // list memory doesn't move
    map<uint256, vector<COrphan*> > mapDependers;
    multimap<double, Transaction*> mapPriority;
    Transactions transactions = _node.blockChain().unconfirmedTransactions();
    for (Transactions::iterator itx = transactions.begin(); itx != transactions.end(); ++itx) {
        Transaction& tx = *itx;
        if (tx.isCoinBase() || !_node.blockChain().isFinal(tx))
            continue;
        
        COrphan* porphan = NULL;
        double dPriority = 0;
        BOOST_FOREACH(const Input& txin, tx.getInputs()) {
            // Read prev transaction
            Transaction txPrev;
            //            TxIndex txindex;
            _node.blockChain().getTransaction(txin.prevout().hash, txPrev);
            if (txPrev.isNull()) {
                // Has to wait for dependencies
                if (!porphan) {
                    // Use list for automatic deletion
                    vOrphan.push_back(COrphan(&tx));
                    porphan = &vOrphan.back();
                }
                mapDependers[txin.prevout().hash].push_back(porphan);
                porphan->setDependsOn.insert(txin.prevout().hash);
                continue;
            }
            int64 nValueIn = txPrev.getOutput(txin.prevout().index).value();
            
            // Read block header
            int nConf = _node.blockChain().getDepthInMainChain(txin.prevout().hash);
            
            dPriority += (double)nValueIn * nConf;
            
            if (fDebug)
                printf("priority     nValueIn=%-12I64d nConf=%-5d dPriority=%-20.1f\n", nValueIn, nConf, dPriority);
        }
        
        // Priority is sum(valuein * age) / txsize
        dPriority /= ::GetSerializeSize(tx, SER_NETWORK);
        
        if (porphan)
            porphan->dPriority = dPriority;
        else
            mapPriority.insert(make_pair(-dPriority, &tx));
        
        if (fDebug) {
            printf("priority %-20.1f %s\n%s", dPriority, tx.getHash().toString().substr(0,10).c_str(), tx.toString().c_str());
            if (porphan)
                porphan->print();
            printf("\n");
        }
    }
    
    // Collect transactions into block
    map<uint256, TxIndex> mapTestPool;
    uint64 nBlockSize = 1000;
    int nBlockSigOps = 100;
    while (!mapPriority.empty()) {
        // Take highest priority transaction off priority queue
        double dPriority = -(*mapPriority.begin()).first;
        Transaction& tx = *(*mapPriority.begin()).second;
        mapPriority.erase(mapPriority.begin());
        
        // Size limits
        unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK);
        if (nBlockSize + nTxSize >= MAX_BLOCK_SIZE_GEN)
            continue;
        int nTxSigOps = tx.getSigOpCount();
        if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
            continue;
        
        // Transaction fee required depends on block size
        bool fAllowFree = (nBlockSize + nTxSize < 4000 || Transaction::allowFree(dPriority));
        int64 nMinFee = tx.getMinFee(nBlockSize, fAllowFree, true);
        
        // Connecting shouldn't fail due to dependency on other memory pool transactions
        // because we're already processing them in order of dependency
        map<uint256, TxIndex> mapTestPoolTmp(mapTestPool);
        if (!_node.blockChain().connectInputs(tx, mapTestPoolTmp, DiskTxPos(1,1,1), prev, nFees, false, true, nMinFee))
            continue;
        swap(mapTestPool, mapTestPoolTmp);
        
        // Added
        block.addTransaction(tx);
        nBlockSize += nTxSize;
        nBlockSigOps += nTxSigOps;
        
        // Add transactions that depend on this one to the priority queue
        uint256 hash = tx.getHash();
        if (mapDependers.count(hash)) {
            BOOST_FOREACH(COrphan* porphan, mapDependers[hash]) {
                if (!porphan->setDependsOn.empty()) {
                    porphan->setDependsOn.erase(hash);
                    if (porphan->setDependsOn.empty())
                        mapPriority.insert(make_pair(-porphan->dPriority, porphan->ptx));
                }
            }
        }
    }
    Transaction& coinBase = block.getTransaction(0);
    // replace the counbase output to update the value:
    Output output(coinBase.getOutput(0).value() + nFees, coinBase.getOutput(0).script());
    coinBase.replaceOutput(0, output);
    
    block.updateMerkleTree();    
}



const string Miner::Hasher::name() const {
    string n = typeid(*this).name();
    // remove trailing numbers from the typeid
    while (n.find_first_of("0123456789") == 0)
        n.erase(0, 1);
    return n;
};

