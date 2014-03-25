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

#ifndef SPENDABLES_H
#define SPENDABLES_H

#include <coin/util.h>

#include <coin/Transaction.h>

#include <coinChain/MerkleTrie.h>


/// Confirmation is the Transaction class incl its DB index
struct Confirmation : public Transaction {
    Confirmation() : cnf(0), count(0) {}
    Confirmation(int64_t cnf, int version, unsigned int locktime, int64_t count) : Transaction(version, locktime), cnf(cnf), count(count) {}
    Confirmation(const Transaction& txn, int64_t cnf = 0, int64_t count = 0) : Transaction(txn), cnf(cnf), count(count) {}
    
    int64_t cnf;
    int64_t count;
    
    bool is_confirmed() { return (0 < count) && (count < LOCKTIME_THRESHOLD); }
    bool is_coinbase() {return (cnf < 0); }
};

struct Unspent {
    typedef Coin Key;
    typedef uint256 Hash;
    typedef unsigned short KeyWidth;
    
    Unspent() : key(), output(), coin(0), cnf(0) {}
    Unspent(int64_t coin, uint256 hash, unsigned int idx, int64_t value, Script script, int64_t count, int64_t cnf) : key(hash, idx), output(value, script), coin(coin), count(count), cnf(cnf) { }

    Key key;
    Output output;

    int64_t coin;
    int64_t count;
    int64_t cnf;
    
    bool is_valid() const { return (!key.isNull()) && (!output.isNull()) && (cnf != 0); }
    
    bool operator!() { return !is_valid(); }
    
#ifndef _WIN32
    static inline uint32_t log2(const uint32_t x) {
        uint32_t y;
        asm ( "\tbsr %1, %0\n"
             : "=r"(y)
             : "r" (x)
             );
        return y;
    }
#else
    static inline uint32_t log2(uint32_t x) {
        uint32_t y = -1;
        while (x != 0) {
            x >>= 1;
            y++;
        }
        return y;
    }
#endif

    static std::string key_str(const Key& k, size_t a = 0, size_t b = 0) {
        if (a == 0 && b == 0) b = 8*sizeof(Key);
        unsigned int* data = (unsigned int*)&k;
        std::string bin;
        for (size_t i = 0; i < sizeof(Key)/4; ++i) {
            for (size_t j = 0; j < 32; ++j) {
                bin += (data[i]&(1<<j))?'1':'0';
            }
        }
        return bin.substr(a, b-a);
    }
    
    static int compare(const Key& ka, const Key& kb) {
        // treat keys as bitfields
        // delete the first depth bits
        // compare bits until there is a difference
        // return log2 of the difference + if a>b, - if a<b
        uint32_t* pa = (uint32_t*)&ka;
        uint32_t* pb = (uint32_t*)&kb;
        
        size_t n = sizeof(Key)/4;
        
        while (n--) {
            uint32_t a = pa[n];
            uint32_t b = pb[n];
            if (a > b)
                return -log2(a^b) - 1 - 32*n;
            else if (a < b)
                return +log2(a^b) + 1 + 32*n;
        }
        return 0;
    }
    
    inline friend std::ostream& operator<<(std::ostream& os, const Unspent& u) {
        return os << u.key << u.output;
    }
    
    inline friend std::istream& operator>>(std::istream& is, Unspent& u) {
        return is >> u.key >> u.output;
    }

    Hash hash() const {
        return serialize_hash(*this);
    }
    static Hash hash(Hash a, Hash b) { return ::Hash(a.begin(), a.end(), b.begin(), b.end()); }
};

typedef std::vector<Unspent> Unspents;

typedef MerkleTrie<Unspent> Spendables;


#endif // SPENDABLES_H
