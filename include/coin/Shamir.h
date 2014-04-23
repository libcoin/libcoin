/* -*-c++-*- libcoin - Copyright (C) 2014 Michael Gronager
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

#ifndef SHAMIR_H
#define SHAMIR_H

#include <coin/Export.h>
#include <coin/BigNum.h>
#include <coin/uint256.h>

class COIN_EXPORT Shamir {
public:
    Shamir(unsigned char shares, unsigned char quorum, CBigNum order = CBigNum(0));
    
    typedef std::pair<unsigned char, uint256> Share;
    typedef std::map<unsigned char, uint256> Shares;
    
    Shares split(uint256 secret) const;
    
    uint256 recover(const Shares& shares) const;
    
    unsigned char quorum() const;
    
private:
    CBigNum rnd() const;
    
private:
    unsigned char _shares;
    unsigned char _quorum;
    CBigNum _order;
};

#endif // SHAMIR_H
