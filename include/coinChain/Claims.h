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

#ifndef CLAIMS_H
#define CLAIMS_H

#include <coin/BigNum.h>
#include <coin/Key.h>
#include <coin/Script.h>
#include <coin/Transaction.h>

class BloomFilter;

class Claims {
public:
    // Inserting a claim means looking up if it depends on other transactions and then sum up the size and fee for those
    struct Claim : public Transaction {
        Claim(const Transaction& txn, int64_t fee, unsigned int ts = 0) : Transaction(txn), fee(fee) {
            size = serialize_size(txn);
            delta_spendables = txn.getNumOutputs()-txn.getNumInputs();
            timestamp = ts ? ts : UnixTime::s();
        }
        unsigned int timestamp;
        unsigned int size;
        int64_t fee;
        int delta_spendables;
        std::vector<uint256> depends;
    };
    
    typedef std::map<uint256, Claim> Confirmations;
    typedef std::set<Coin> Spents;
    
    class OptimizeForFeeThenDeltaSpendables : std::binary_function<const uint256&, const uint256&, bool> {
    public:
        OptimizeForFeeThenDeltaSpendables(const Confirmations& confirmations) : _confirmations(confirmations) {}
        // this algorithm optimizes for two conditions priotized:
        // 1. Maximize the fee - i.e. sort transactions (incl grouped dependent transactions) based on fee/size
        // 2. Minimize the number of spendables - i.e. sort transactions based on delta-spendables = #outputs-#inputs
        // include as many transactions as possible within the max size of a block
        bool operator()(const uint256& lhs, const uint256& rhs) {
            // check if the transaction depends on other transactions
            const Claim& l = _confirmations.find(lhs)->second;
            const Claim& r = _confirmations.find(rhs)->second;
            
            if (l.fee*r.size > r.fee*l.size)
                return true;
            else if (l.fee*r.size < r.fee*l.size)
                return false;
            else
                return (l.delta_spendables < r.delta_spendables);
        }
        
    private:
        const Confirmations& _confirmations;
    };

    typedef std::set<uint256, OptimizeForFeeThenDeltaSpendables> Priorities;
    
    typedef std::multimap<Script, Coin> Scripts;

public:
    Claims() : _priorities(OptimizeForFeeThenDeltaSpendables(_confirmations)) {}
    
    bool spent(const Coin& coin) const {
        return _spents.count(coin);
    }
    
    bool have(const uint256& hash) const {
        return _confirmations.count(hash);
    }
    
    size_t count() const {
        return _confirmations.size();
    }
    
    unsigned int timestamp(uint256 hash) const {
        Confirmations::const_iterator cnf = _confirmations.find(hash);
        return (cnf == _confirmations.end()) ? 0 : cnf->second.timestamp;
    }
    
    const Output prev(const Coin& coin) const;
    
    void insert(Transaction txn, const Spents& spents, int64_t fee, unsigned int ts = 0);
    
    void erase(uint256 hash);
    
    // claimed, i.e. outputs, by script
    std::vector<std::pair<Coin, Output> > claimed(const Script& script) const {
        typedef std::pair<Scripts::const_iterator, Scripts::const_iterator> Range;
        Range range = _scripts.equal_range(script);
        
        std::vector<std::pair<Coin, Output> > outputs;
        for (Scripts::const_iterator o = range.first; o != range.second; ++o) {
            Confirmations::const_iterator cnf = _confirmations.find(o->second.hash);
            if (cnf == _confirmations.end())
                continue;
            const Transaction& txn = cnf->second;
            outputs.push_back(std::make_pair<Coin, Output>(o->second, txn.getOutput(o->second.index)));
        }

        return outputs;
    }
    
    // Transactions ordered in dependent order
    std::vector<Transaction> transactions(int64_t& fee, size_t header_and_coinbase = 1000) const;
    
    // Transaction hashes for all claims
    std::vector<uint256> claims(BloomFilter& filter) const;
    
    // current transaction fee threshold to get into a block
    int64_t threshold(size_t header_and_coinbase = 1000) const;
    
    void purge(unsigned int before);

private:
    size_t insert_claim(const Claim& claim, std::vector<Transaction>& txns, std::set<uint256>& inserted ) const;
    
private:
    // a set of confirmations and their fee sorted by fee / size and delta spendables
    Confirmations _confirmations;
    Priorities _priorities;
    Spents _spents;
    Scripts _scripts;
};

#endif // CLAIMS_H
