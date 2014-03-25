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

#include <coinChain/BloomFilter.h>
#include <coin/Transaction.h>

#include <coin/Script.h>

#include <math.h>
#include <stdlib.h>

#define LN2SQUARED 0.4804530139182014246671025263266649717305529515945455
#define LN2 0.6931471805599453094172321214581765680755001343602552

using namespace std;

static const unsigned char bit_mask[8] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};

unsigned int MurmurHash3(unsigned int nHashSeed, const std::vector<unsigned char>& vDataToHash);

BloomFilter::BloomFilter(unsigned int elements, double fpRate, unsigned int tweak, unsigned char flags) :
    // The ideal size for a bloom filter with a given number of elements and false positive rate is:
    // - nElements * log(fp rate) / ln(2)^2
    // We ignore filter parameters which will create a bloom filter larger than the protocol limits
    _data(min((unsigned int)(-1  / LN2SQUARED * elements * log(fpRate)), MAX_BLOOM_FILTER_SIZE * 8) / 8),
    // The ideal number of hash functions is filter size * ln(2) / number of elements
    // Again, we ignore filter parameters which will create a bloom filter with more hash functions than the protocol limits
    // See http://en.wikipedia.org/wiki/Bloom_filter for an explanation of these formulas
    _full(false),
    _empty(false),
    _hash_funcs(min((unsigned int)(_data.size() * 8 / elements * LN2), MAX_HASH_FUNCS)),
    _tweak(tweak),
    _flags(flags) {
}

inline unsigned int BloomFilter::hash(unsigned int num, const std::vector<unsigned char>& data) const {
    // 0xFBA4C795 chosen as it guarantees a reasonable bit difference between nHashNum values.
    return MurmurHash3(num * 0xFBA4C795 + _tweak, data) % (_data.size() * 8);
}

void BloomFilter::insert(const vector<unsigned char>& key) {
    if (_full)
        return;
    for (unsigned int i = 0; i < _hash_funcs; i++) {
        unsigned int index = hash(i, key);
        // Sets bit nIndex of vData
        _data[index >> 3] |= bit_mask[7 & index];
    }
    _empty = false;
}

void BloomFilter::insert(const Coin& outpoint) {
    string s = serialize(outpoint);
    insert(vector<unsigned char>(s.begin(), s.end()));
}

void BloomFilter::insert(const uint256& hash) {
    vector<unsigned char> data(hash.begin(), hash.end());
    insert(data);
}

bool BloomFilter::contains(const vector<unsigned char>& key) const {
    if (_full)
        return true;
    if (_empty)
        return false;
    for (unsigned int i = 0; i < _hash_funcs; i++) {
        unsigned int index = hash(i, key);
        // Checks bit nIndex of vData
        if (!(_data[index >> 3] & bit_mask[7 & index]))
            return false;
    }
    return true;
}

bool BloomFilter::contains(const Coin& outpoint) const {
    string s = serialize(outpoint);
    return contains(vector<unsigned char>(s.begin(), s.end()));
}

bool BloomFilter::contains(const uint256& hash) const {
    vector<unsigned char> data(hash.begin(), hash.end());
    return contains(data);
}

bool BloomFilter::isWithinSizeConstraints() const {
    return _data.size() <= MAX_BLOOM_FILTER_SIZE && _hash_funcs <= MAX_HASH_FUNCS;
}

bool Solver(const Script& scriptPubKey, txnouttype& typeRet, vector<vector<unsigned char> >& vSolutionsRet);

bool BloomFilter::isRelevantAndUpdate(const Transaction& txn) {
    if (_full)
        return true;
    if (_empty)
        return false;

    uint256 hash = txn.getHash();
    
    bool found = false;
    // Match if the filter contains the hash of tx
    //  for finding tx when they appear in a block
    if (contains(hash))
        found = true;
    
    for (unsigned int i = 0; i < txn.getNumOutputs(); i++) {
        const Output& output = txn.getOutput(i);
        // Match if the filter contains any arbitrary script data element in any scriptPubKey in tx
        // If this matches, also add the specific output that was matched.
        // This means clients don't have to update the filter themselves when a new relevant tx
        // is discovered in order to find spending transactions, which avoids round-tripping and race conditions.
        Script::const_iterator pc = output.script().begin();
        vector<unsigned char> data;
        while (pc < output.script().end()) {
            opcodetype opcode;
            if (!output.script().getOp(pc, opcode, data))
                break;
            if (data.size() != 0 && contains(data)) {
                found = true;
                if ((_flags & BLOOM_UPDATE_MASK) == BLOOM_UPDATE_ALL)
                    insert(Coin(hash, i));
                else if ((_flags & BLOOM_UPDATE_MASK) == BLOOM_UPDATE_P2PUBKEY_ONLY) {
                    txnouttype type;
                    vector<vector<unsigned char> > vSolutions;
                    if (Solver(output.script(), type, vSolutions) &&
                        (type == TX_PUBKEY || type == TX_MULTISIG))
                        insert(Coin(hash, i));
                }
                break;
            }
        }
    }
    
    if (found)
        return true;
    
    BOOST_FOREACH(const Input& input, txn.getInputs()) {
        // Match if the filter contains an outpoint tx spends
        if (contains(input.prevout()))
            return true;
        
        // Match if the filter contains any arbitrary script data element in any scriptSig in tx
        Script::const_iterator pc = input.signature().begin();
        vector<unsigned char> data;
        while (pc < input.signature().end()) {
            opcodetype opcode;
            if (!input.signature().getOp(pc, opcode, data))
                break;
            if (data.size() != 0 && contains(data))
                return true;
        }
    }
    
    return false;
}

void BloomFilter::updateEmptyFull()
{
    bool full = true;
    bool empty = true;
    for (unsigned int i = 0; i < _data.size(); i++) {
        full &= _data[i] == 0xff;
        empty &= _data[i] == 0;
    }
    _full = full;
    _empty = empty;
}

inline uint32_t ROTL32 ( uint32_t x, int8_t r )
{
    return (x << r) | (x >> (32 - r));
}

unsigned int MurmurHash3(unsigned int nHashSeed, const std::vector<unsigned char>& vDataToHash)
{
    // The following is MurmurHash3 (x86_32), see http://code.google.com/p/smhasher/source/browse/trunk/MurmurHash3.cpp
    uint32_t h1 = nHashSeed;
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;
    
    const int nblocks = vDataToHash.size() / 4;
    
    //----------
    // body
    const uint32_t * blocks = (const uint32_t *)(&vDataToHash[0] + nblocks*4);
    
    for(int i = -nblocks; i; i++)
    {
        uint32_t k1 = blocks[i];
        
        k1 *= c1;
        k1 = ROTL32(k1,15);
        k1 *= c2;
        
        h1 ^= k1;
        h1 = ROTL32(h1,13);
        h1 = h1*5+0xe6546b64;
    }
    
    //----------
    // tail
    const uint8_t * tail = (const uint8_t*)(&vDataToHash[0] + nblocks*4);
    
    uint32_t k1 = 0;
    
    switch(vDataToHash.size() & 3)
    {
        case 3: k1 ^= tail[2] << 16;
        case 2: k1 ^= tail[1] << 8;
        case 1: k1 ^= tail[0];
            k1 *= c1; k1 = ROTL32(k1,15); k1 *= c2; h1 ^= k1;
    };
    
    //----------
    // finalization
    h1 ^= vDataToHash.size();
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;
    
    return h1;
}

