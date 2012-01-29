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

#ifndef _CPUHASHER_H_
#define _CPUHASHER_H_

#include "coinMine/Miner.h"

class CPUHasher : public Miner::Hasher {
public:
    virtual bool operator()(Block& block, unsigned int nonces);
    
    /// Description - optional
    virtual const std::string description() const { 
        return
        "The CPU Hasher is the hasher also part of the original Satoshi client.\n"
        "It slow compared to more recent GPU miners and is hence supplied mainly\n"
        "for informational and educational purposes. E.g. if you want to setup\n"
        "your own coin chain in a class room.\n";
    }
    
    /// Return true if the hashing algorithm is supported on this platform, false otherwise - mandatory.
    virtual const bool supported() const { return true; }; // No special requirements, hence always supported
};

#endif // _CPUHASHER_H_
