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

#ifndef BLOOMFILTER_H
#define BLOOMFILTER_H

//#include <coin/serialize.h>
#include <coin/uint256.h>

#include <vector>

class Transaction;
struct Coin;

// 20,000 items with fp rate < 0.1% or 10,000 items and <0.0001%
static const unsigned int MAX_BLOOM_FILTER_SIZE = 36000; // bytes
static const unsigned int MAX_HASH_FUNCS = 50;

// First two bits of nFlags control how much IsRelevantAndUpdate actually updates
// The remaining bits are reserved
enum bloomflags
{
    BLOOM_UPDATE_NONE = 0,
    BLOOM_UPDATE_ALL = 1,
    // Only adds outpoints to the filter if the output is a pay-to-pubkey/pay-to-multisig script
    BLOOM_UPDATE_P2PUBKEY_ONLY = 2,
    BLOOM_UPDATE_MASK = 3
};

/**
 * BloomFilter is a probabilistic filter which SPV clients provide
 * so that we can filter the transactions we sends them.
 *
 * This allows for significantly more efficient transaction and block downloads.
 *
 * Because bloom filters are probabilistic, an SPV node can increase the false-
 * positive rate, making us send them transactions which aren't actually theirs,
 * allowing clients to trade more bandwidth for more privacy by obfuscating which
 * keys are owned by them.
 */
class BloomFilter {
public:
    // Creates a new bloom filter which will provide the given fp rate when filled with the given number of elements
    // Note that if the given parameters will result in a filter outside the bounds of the protocol limits,
    // the filter created will be as close to the given parameters as possible within the protocol limits.
    // This will apply if nFPRate is very low or nElements is unreasonably high.
    // nTweak is a constant which is added to the seed value passed to the hash function
    // It should generally always be a random value (and is largely only exposed for unit testing)
    // nFlags should be one of the BLOOM_UPDATE_* enums (not _MASK)
    BloomFilter(unsigned int nElements, double nFPRate, unsigned int nTweak, unsigned char nFlagsIn);
    BloomFilter() : _full(true) {}
    
    inline friend std::ostream& operator<<(std::ostream& os, const BloomFilter& bf) {
        return os << bf._data << const_binary<unsigned int>(bf._hash_funcs) << const_binary<unsigned int>(bf._tweak) << const_binary<unsigned char>(bf._flags);
    }
    
    inline friend std::istream& operator>>(std::istream& is, BloomFilter& bf) {
        return is >> bf._data >> binary<unsigned int>(bf._hash_funcs) >> binary<unsigned int>(bf._tweak) >> binary<unsigned char>(bf._flags);
    }

    void insert(const std::vector<unsigned char>& key);
    void insert(const Coin& outpoint);
    void insert(const uint256& hash);
    
    bool contains(const std::vector<unsigned char>& key) const;
    bool contains(const Coin& outpoint) const;
    bool contains(const uint256& hash) const;
    
    // True if the size is <= MAX_BLOOM_FILTER_SIZE and the number of hash functions is <= MAX_HASH_FUNCS
    // (catch a filter which was just deserialized which was too big)
    bool isWithinSizeConstraints() const;
    
    // Also adds any outputs which match the filter to the filter (to match their spending txes)
    bool isRelevantAndUpdate(const Transaction& txn);
    
    // Checks for empty and full filters to avoid wasting cpu
    void updateEmptyFull();

    inline friend bool operator==(const BloomFilter& l, const BloomFilter& r) {
        return (l._data == r._data &&
                l._hash_funcs == r._hash_funcs &&
                l._tweak == r._tweak &&
                l._flags == r._flags);
    }
    
    inline friend bool operator!=(const BloomFilter& l, const BloomFilter& r) {
        return !(l == r);
    }
private:
    unsigned int hash(unsigned int num, const std::vector<unsigned char>& data) const;

private:
    std::vector<unsigned char> _data;
    bool _full;
    bool _empty;
    unsigned int _hash_funcs;
    unsigned int _tweak;
    unsigned char _flags;
};

#endif /* BLOOMFILTER_H */
