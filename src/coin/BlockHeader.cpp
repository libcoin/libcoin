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

#include <coin/BlockHeader.h>

using namespace std;
using namespace boost;

ostream& operator<<(ostream& os, const BlockHeader& bh) {
    return os << const_binary<int>(bh._version) << bh._prevBlock << bh._merkleRoot << const_binary<unsigned int>(bh._time) << const_binary<unsigned int>(bh._bits) << const_binary<unsigned int>(bh._nonce);
}

istream& operator>>(istream& is, BlockHeader& bh) {
    return is >> binary<int>(bh._version) >> bh._prevBlock >> bh._merkleRoot >> binary<unsigned int>(bh._time) >> binary<unsigned int>(bh._bits) >> binary<unsigned int>(bh._nonce);
}

uint256 BlockHeader::getHash() const {
    return Hash(BEGIN(_version), END(_nonce));
}

template<typename T1>
inline uint256 HalfHash(const T1 pbegin, const T1 pend) {
    static unsigned char pblank[1];
    uint256 hash1;
    SHA256((pbegin == pend ? pblank : (unsigned char*)&pbegin[0]), (pend - pbegin) * sizeof(pbegin[0]), (unsigned char*)&hash1);
    return hash1;
}

uint256 BlockHeader::getHalfHash() const {
    return HalfHash(BEGIN(_version), END(_nonce));
}
