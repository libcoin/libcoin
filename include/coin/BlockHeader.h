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

#ifndef BLOCK_HEADER_H
#define BLOCK_HEADER_H

#include <coin/Export.h>
//#include <coin/serialize.h>
#include <coin/uint256.h>
#include <coin/BigNum.h>
#include <coin/Serialization.h>

class COIN_EXPORT BlockHeader {
public:
    BlockHeader() {
        setNull();
    }
    
    BlockHeader(const int version, const uint256 prevBlock, const uint256 merkleRoot, const int time, const int bits, const int nonce) : _version(version), _prevBlock(prevBlock), _merkleRoot(merkleRoot), _time(time), _bits(bits), _nonce(nonce) {
    }
    
    inline friend std::ostream& operator<<(std::ostream& os, const BlockHeader& bh) {
        return os << const_binary<int>(bh._version) << bh._prevBlock << bh._merkleRoot << const_binary<unsigned int>(bh._time) << const_binary<unsigned int>(bh._bits) << const_binary<unsigned int>(bh._nonce);
    }
    
    inline friend std::istream& operator>>(std::istream& is, BlockHeader& bh) {
        return is >> binary<int>(bh._version) >> bh._prevBlock >> bh._merkleRoot >> binary<unsigned int>(bh._time) >> binary<unsigned int>(bh._bits) >> binary<unsigned int>(bh._nonce);
    }

    void setNull() {
        _version = 1;
        _prevBlock = 0;
        _merkleRoot = 0;
        _time = 0;
        _bits = 0;
        _nonce = 0;
    }
    
    bool isNull() const {
        return (_bits == 0);
    }
    
    uint256 getHash() const;
    
    uint256 getHalfHash() const;
    
    int64_t getBlockTime() const {
        return (int64_t)_time;
    }
    
    const int getVersion() const { return _version; }
    
    const uint256 getPrevBlock() const { return _prevBlock; }
    
    const uint256 getMerkleRoot() const { return _merkleRoot; }
    
    const int getTime() const { return _time; }
    
    void setTime(int time) { _time = time; }
    
    const int getBits() const { return _bits; }
    
    const uint256 getTarget() const {
        return CBigNum().SetCompact(getBits()).getuint256();
    }
        
    const int getNonce() const { return _nonce; }
    
    void setNonce(unsigned int nonce) { _nonce = nonce; }
    
    friend bool operator==(const BlockHeader& l, const BlockHeader& r) {
        return (l._version == r._version &&
                l._prevBlock == r._prevBlock &&
                l._merkleRoot == r._merkleRoot &&
                l._time == r._time &&
                l._bits == r._bits &&
                l._nonce == r._nonce);
    }
    
    friend bool operator!=(const BlockHeader& l, const BlockHeader& r) {
        return !(l == r);
    }
    
protected:
    int _version;
    uint256 _prevBlock;
    uint256 _merkleRoot;
    unsigned int _time;
    unsigned int _bits;
    unsigned int _nonce;
};

#endif // BLOCK_HEADER_H
