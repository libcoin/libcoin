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

#include <coinChain/Claims.h>
#include <coinChain/BloomFilter.h>

using namespace std;
using namespace boost;

const Output Claims::prev(const Coin& coin) const {
    Confirmations::const_iterator cnf = _confirmations.find(coin.hash);
    if (cnf != _confirmations.end()) {
        const Transaction& txn = cnf->second;
        if (coin.index < txn.getNumOutputs())
            return txn.getOutput(coin.index);
    }
    return Output();
}

void Claims::insert(Transaction txn, const Spents& spents, int64_t fee, unsigned int ts) {
    Claim claim(txn, fee, ts);
    // check for dependent transactions
    for (Inputs::const_iterator i = txn.getInputs().begin(); i != txn.getInputs().end(); ++i) {
        Confirmations::const_iterator cnf = _confirmations.find(i->prevout().hash);
        if (cnf != _confirmations.end()) {
            claim.depends.push_back(cnf->first);
            claim.size += cnf->second.size;
            claim.fee += cnf->second.fee;
            claim.delta_spendables += txn.getNumOutputs()-txn.getNumInputs();
        }
    }
    // insert all inputs in spent
    uint256 hash = claim.getHash();
    _confirmations.insert(make_pair<uint256, Claim>(hash, claim));
    _priorities.insert(hash);
    _spents.insert(spents.begin(), spents.end());
    
    // insert the outputs in the map of scripts
    for (size_t idx = 0; idx < txn.getNumOutputs(); ++idx)
        _scripts.insert(make_pair<Script, Coin>(txn.getOutput(idx).script(), Coin(hash, idx)));
}

void Claims::erase(uint256 hash) {
    Confirmations::iterator cnf = _confirmations.find(hash);
    if (cnf != _confirmations.end()) {
        _priorities.erase(hash);
        // clean up spendings - we erase all possible spendings from this transaction
        for (unsigned int idx = 0; idx < cnf->second.getNumInputs(); ++idx)
            _spents.erase(cnf->second.getInput(idx).prevout());
        // remove the outputs from the map of scripts
        for (size_t idx = 0; idx < cnf->second.getNumOutputs(); ++idx) {
            typedef pair<Scripts::iterator, Scripts::iterator> Range;
            Range range = _scripts.equal_range(cnf->second.getOutput(idx).script());
            for (Scripts::iterator o = range.first; o != range.second; ++o)
                if (o->second == Coin(hash, idx)) {
                    _scripts.erase(o);
                    break;
                }
        }
        _confirmations.erase(cnf);
    }
}

// Transactions ordered in dependent order
vector<Transaction> Claims::transactions(int64_t& fee, size_t header_and_coinbase) const {
    fee = 0;
    vector<Transaction> txns;
    set<uint256> inserted;
    
    Priorities::const_iterator p = _priorities.begin();
    size_t size = header_and_coinbase; // make room for header and coinbase
    while (p != _priorities.end()) {
        Confirmations::const_iterator cnf = _confirmations.find(*p);
        if (cnf != _confirmations.end()) {
            const Claim& claim = cnf->second;
            if (!inserted.count(cnf->first)) {
                if (size + claim.size >= MAX_BLOCK_SIZE)
                    break;
                size += insert_claim(claim, txns, inserted);
                fee += claim.fee;
            }
        }
        ++p;
    }
    
    return txns;
}

vector<uint256> Claims::claims(BloomFilter& filter) const {
    int64_t fee;
    vector<Transaction> txns = transactions(fee);

    vector<uint256> hashes;
    for (vector<Transaction>::const_iterator tx = txns.begin(); tx != txns.end(); ++tx) {
        if (filter.isRelevantAndUpdate(*tx))
            hashes.push_back(tx->getHash());
    }
    
    return hashes;
}


size_t Claims::insert_claim(const Claim& claim, vector<Transaction>& txns, set<uint256>& inserted ) const {
    size_t size = 0;
    for (vector<uint256>::const_iterator h = claim.depends.begin(); h != claim.depends.end(); ++h) {
        if (!inserted.count(*h)) {
            Confirmations::const_iterator cnf = _confirmations.find(*h);
            if (cnf != _confirmations.end()) {
                const Claim& c = cnf->second;
                size += insert_claim(c, txns, inserted);
            }
        }
    }
    txns.push_back(claim);
    inserted.insert(claim.getHash());
    return size + claim.size;
}

// current transaction fee threshold to get into a block
int64_t Claims::threshold(size_t header_and_coinbase) const {
    Priorities::const_iterator p = _priorities.begin();
    int64_t fee = 0;
    size_t size = header_and_coinbase; // make room for header and coinbase
    while (p != _priorities.end() && size < MAX_BLOCK_SIZE) {
        const Claim& claim = _confirmations.find(*p)->second;
        fee = claim.fee;
        size += claim.size;
    }
    
    return fee;
}

void Claims::purge(unsigned int before) {
    // loop over all confirmations and remove those older than before
    typedef set<uint256> Hashes;
    Hashes death_row;
    for (Confirmations::const_iterator cnf = _confirmations.begin(); cnf != _confirmations.end(); ++cnf) {
        if (cnf->second.timestamp < before)
            death_row.insert(cnf->first);
    }
    for (Hashes::const_iterator h = death_row.begin(); h != death_row.end(); ++h)
        erase(*h);
}
