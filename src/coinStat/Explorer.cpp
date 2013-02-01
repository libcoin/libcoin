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


#include <coinStat/Explorer.h>

#include <fstream>
  
using namespace std;
using namespace boost;
/*
void Explorer::TransactionListener::operator()(const Transaction& tx) {
    uint256 hash = tx.getHash();
    
    // for each tx output in the tx check for a pubkey or a pubkeyhash in the script
    for(unsigned int n = 0; n < tx.getNumOutputs(); n++) {
        const Output& txout = tx.getOutput(n);
        PubKeyHash address = txout.getAddress();
        _explorer.insertDebit(address, Coin(hash, n));
    }
    if(!tx.isCoinBase()) {
        for(unsigned int n = 0; n < tx.getNumInputs(); n++) {
            const Input& txin = tx.getInput(n);
            Transaction prevtx;
            _explorer.blockChain().getTransaction(txin.prevout().hash, prevtx);
            
            Output txout = prevtx.getOutput(txin.prevout().index);        
            
            PubKeyHash address = txout.getAddress();
            _explorer.insertCredit(address, Coin(hash, n));
            _explorer.markSpent(address, Coin(txin.prevout().hash, txin.prevout().index));
        }
    }
}

void Explorer::BlockListener::operator()(const Block& block) {
    TransactionList txes = block.getTransactions();
    for(TransactionList::const_iterator tx = txes.begin(); tx != txes.end(); ++tx) {
        uint256 hash = tx->getHash();
        
        // for each tx output in the tx check for a pubkey or a pubkeyhash in the script
        for(unsigned int n = 0; n < tx->getNumOutputs(); n++) {
            const Output& txout = tx->getOutput(n);
            PubKeyHash address = txout.getAddress();
            _explorer.insertDebit(address, Coin(hash, n));
        }
        if(!tx->isCoinBase()) {
            for(unsigned int n = 0; n < tx->getNumInputs(); n++) {
                const Input& txin = tx->getInput(n);
                Transaction prevtx;
                // We need a workaround for blocks that have not ordered its transactions
                _explorer.blockChain().getTransaction(txin.prevout().hash, prevtx);
                
                if(prevtx.isNull()) {
                    log_warn("Warning tx with hash %s not found", txin.prevout().hash.toString().c_str());
                    continue; // this is a hack!
                }
                
                Output txout = prevtx.getOutput(txin.prevout().index);        
                
                PubKeyHash address = txout.getAddress();
                _explorer.insertCredit(address, Coin(hash, n));
                _explorer.markSpent(address, Coin(txin.prevout().hash, txin.prevout().index));
            }
        }
    }
    _explorer.setHeight(_explorer.blockChain().getBlockIndex(block.getHash())->nHeight);
}

void Explorer::getCredit(const PubKeyHash& address, Coins& coins) const {
    pair<Ledger::const_iterator, Ledger::const_iterator> range = _credits.equal_range(address);
    for(Ledger::const_iterator i = range.first; i != range.second; ++i)
        coins.insert(i->second);
}

void Explorer::getDebit(const PubKeyHash& address, Coins& coins) const {
    pair<Ledger::const_iterator, Ledger::const_iterator> range = _debits.equal_range(address);
    for(Ledger::const_iterator i = range.first; i != range.second; ++i)
        coins.insert(i->second);        
}

void Explorer::getCoins(const PubKeyHash& address, Coins& coins) const {
    pair<Ledger::const_iterator, Ledger::const_iterator> range = _coins.equal_range(address);
    for(Ledger::const_iterator i = range.first; i != range.second; ++i)
        coins.insert(i->second);
}

void Explorer::save(std::string filename) {
    ofstream ofs(filename.c_str());
    
    CDataStream ds;
    ds << _height;
    ds << _debits;
    ds << _credits;
    ds << _coins;
    ofs.write(&ds[0], ds.size());
}

void Explorer::load(std::string filename) {
    ifstream ifs(filename.c_str());
    if( ifs.is_open() ) {
        std::stringstream buffer;
        buffer << ifs.rdbuf();        
        CDataStream ds(buffer.str());
        ds >> _height;
        ds >> _debits;
        ds >> _credits;
        ds >> _coins;
    }
}

void Explorer::scan() {
    const CBlockIndex* pindex = _blockChain.getGenesisIndex();
    for(int h = 0; h < _height-250; ++h) if(!(pindex = pindex->pnext)) break; // rescan the last 250 entries
    
    while (pindex) {
        Block block;
        _blockChain.getBlock(pindex, block);
        
        TransactionList txes = block.getTransactions();
        for(TransactionList::const_iterator tx = txes.begin(); tx != txes.end(); ++tx) {
            uint256 hash = tx->getHash();
            
            // for each tx output in the tx check for a pubkey or a pubkeyhash in the script
            for(unsigned int n = 0; n < tx->getNumOutputs(); n++) {
                const Output& txout = tx->getOutput(n);
                PubKeyHash address = txout.getAddress();
                insertDebit(address, Coin(hash, n));
            }
            if(!tx->isCoinBase()) {
                for(unsigned int n = 0; n < tx->getNumInputs(); n++) {
                    const Input& txin = tx->getInput(n);
                    Transaction prevtx;
                    _blockChain.getTransaction(txin.prevout().hash, prevtx);
                    
                    Output txout = prevtx.getOutput(txin.prevout().index);        
                    
                    PubKeyHash address = txout.getAddress();
                    insertCredit(address, Coin(hash, n));
                    markSpent(address, Coin(txin.prevout().hash, txin.prevout().index));
                }
            }
        }
        
        _height = pindex->nHeight;
        if(pindex->nHeight%100 == 0)
            log_info("[coins/block#: %d, %d, %d, %d]\n", pindex->nHeight, _coins.size(), _debits.size(), _credits.size());
        
        pindex = pindex->pnext;
    }
}

void Explorer::insertDebit(const PubKeyHash& address, const Coin& coin) {
    if(_include_spend)
        _debits.insert(make_pair(address, coin));
    _coins.insert(make_pair(address, coin));
}

void Explorer::insertCredit(const PubKeyHash& address, const Coin& coin) {
    if(_include_spend)
        _credits.insert(make_pair(address, coin));
}

void Explorer::markSpent(const PubKeyHash& address, const Coin& coin) {
    pair<Ledger::iterator, Ledger::iterator> range = _coins.equal_range(address);
    for(Ledger::iterator i = range.first; i != range.second; ++i)
        if (i->second == coin) {
            _coins.erase(i);
            break;
        }
}
*/
