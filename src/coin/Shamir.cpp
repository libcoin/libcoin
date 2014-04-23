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

#include <coin/Shamir.h>

#include <coin/Key.h>

#include <openssl/rand.h>

Shamir::Shamir(unsigned char shares, unsigned char quorum, CBigNum order) : _shares(shares), _quorum(quorum), _order(order) {
    if (_order == CBigNum(0)) {
        Key key(CBigNum((123)));
        _order = key.order();
    }
}

Shamir::Shares Shamir::split(uint256 secret) const {
    Shares shares;
    std::vector<CBigNum> coef;
    coef.push_back(CBigNum(secret));
    for (unsigned char i = 1; i < _quorum; ++i)
        coef.push_back(rnd());
    
    for (unsigned char x = 1; x <= _shares; ++x) {
        CBigNum accum = coef[0];
        for (unsigned char i = 1; i < _quorum; ++i)
            accum = (accum + (coef[i] * ModPow(CBigNum(x), i, _order))) % _order;
        shares[x] = accum.getuint256();
    }
    
    return shares;
}

uint256 Shamir::recover(const Shamir::Shares& shares) const {
    CBigNum accum = 0;
    for(Shares::const_iterator formula = shares.begin(); formula != shares.end(); ++formula)
    {
        CBigNum numerator = 1;
        CBigNum denominator = 1;
        for(Shares::const_iterator count = shares.begin(); count != shares.end(); ++count)
        {
            if(formula == count) continue; // If not the same value
            unsigned char startposition = formula->first;
            unsigned char nextposition = count->first;
            numerator = (-nextposition * numerator) % _order;
            denominator = ((startposition - nextposition)*denominator) % _order;
        }
        accum = (_order + accum + (CBigNum(formula->second) * numerator * ModInverse(denominator, _order))) % _order;
    }
    return accum.getuint256();
}

unsigned char Shamir::quorum() const {
    return _quorum;
}

CBigNum Shamir::rnd() const {
    uint256 r;
    RAND_bytes((unsigned char*)&r, 32);
    return CBigNum(r) % _order;
}
