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
#include <coinChain/Spendables.h>

#include <coin/uint256.h>
#include <coin/util.h>

#include <string>
#include <iostream>

#include <boost/lexical_cast.hpp>

#include <stdint.h>

using namespace std;
using namespace boost;


inline int64_t time_microseconds() {
    return (boost::posix_time::ptime(boost::posix_time::microsec_clock::universal_time()) -
            boost::posix_time::ptime(boost::gregorian::date(1970,1,1))).total_microseconds();
}

unsigned int rand_idx(unsigned int n = 100) {
    return rand()%n;
}

uint256 rand_hash(string hex = "") {
    // 5 bytes:
    const char digits[] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
    
    for (size_t i = hex.size(); i < 64; ++i)
        hex += digits[rand()%16];
    return uint256(hex);
}

int main(int argc, char* argv[])
{
    /// We test the Claims class by inserting a series of transactions - so first generate some transactions:
    /// (note the transactions need not be valid as Claims will not check this)
    /// However, first we generate a list of unspents to build transactions from
    
    
    cout << "==== Testing class Claims ====\n" << endl;
    
    
    try {
        cout << "Checking that assignment works" << endl;
        
        Claims claims;
        
        vector<Unspent> unspents;
        for (size_t i = 0; i < 200; ++i)
            unspents.push_back(Unspent(0, rand_hash(), rand_idx(), i*COIN, Script(), 0, 0));
        
        size_t blocks = 0;
        while (blocks < 100 && unspents.size() > 100) {
            /// Then build a transaction from n inputs grabbed from the unspents pool generating m outputs (which are fed back into the pool)
            while (claims.count() < 100 && unspents.size() > 10) {
                Transaction txn;
                Claims::Spents spents;
                spents.clear();
                int64_t amount = 0;
                size_t n = 1+rand()%3;
                for (size_t i = 0; i < n; i++) {
                    size_t idx = rand()%unspents.size();
                    txn.addInput(Input(unspents[idx].key));
                    amount += unspents[idx].output.value();
                    spents.insert(unspents[idx].key);
                    unspents.erase(unspents.begin() + idx); // erase the unspents as it is now used in a txn
                }
                size_t m = 2+rand()%3;
                int64_t fee = rand()%1000000;
                amount -= fee;
                for (size_t j = 0; j < m; j++) {
                    Output output(amount/m, Script());
                    txn.addOutput(output);
                }
                uint256 hash = txn.getHash();
                for (size_t j = 0; j < m; j++) {
                    Output output = txn.getOutput(j);
                    unspents.push_back(Unspent(0, hash, j, output.value(), output.script(), 0, 0));
                }
                claims.insert(txn, spents, fee);
            }
            
            // now get the transactions for a block candidate:
            int64_t fee;
            vector<Transaction> txns = claims.transactions(fee);
            cout << "Created block candidate with " << txns.size() << " transactions and a fee of " << fee << " satoshis." << endl;
             blocks++;
            for (size_t i = 0; i < txns.size(); ++i)
                claims.erase(txns[i].getHash());
            cout << "\t Total unspents is now " << unspents.size() << endl;
            
        }
        
        return 0;
    }
    catch (std::exception &e) {
        cout << "Exception: " << e.what() << endl;
        return 1;
    }
}