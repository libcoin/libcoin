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

#ifndef _ADDRESS_H
#define _ADDRESS_H

#include <string>
#include <vector>
#include <coin/BigNum.h>

// Why base-58 instead of standard base-64 encoding?
// - Don't want 0OIl characters that look the same in some fonts and
//      could be used to create visually identical looking account numbers.
// - A string with non-alphanumeric characters is not as easily accepted as an account number.
// - E-mail usually won't line-break if there's no punctuation to break at.
// - Doubleclicking selects the whole number as one word if it's all alphanumeric.

/// Utility functions
std::string EncodeBase58Check(const std::vector<unsigned char>& vchIn);
bool DecodeBase58Check(const std::string& str, std::vector<unsigned char>& vchRet);

// This is the raw Address - the BitcoinAddress or more correctly the ChainAddress includes the netword Id and is hence better suited for communicating with the outside. For backwards compatability we do, however, need to use the ChainAddress in e.g. the wallet. 

typedef std::vector<unsigned char> PubKey;

typedef uint160 Address;
Address toAddress(const PubKey& pubkey);

class ChainAddress
{
public:
    bool setHash160(unsigned char network_id, const Address& address) {
        _id = network_id;
        _address = address;
        return true;
    }

    bool setPubKey(unsigned char network_id, const std::vector<unsigned char>& pubKey) {
        return setHash160(network_id, toAddress(pubKey));
    }

    bool isValid(unsigned char network_id) const {
        return network_id == _id && _address != 0;
    }
    
    unsigned char networkId() const {
        return _id;
    }

    ChainAddress() : _address(0), _id(0) { }

    ChainAddress(unsigned char network_id, Address address) {
        setHash160(network_id, address);
    }

    ChainAddress(unsigned char network_id, const std::vector<unsigned char>& pubKey) {
        setPubKey(network_id, pubKey);
    }

    ChainAddress(const std::string& str) {
        setString(str);
    }

    Address getAddress() const {
        return _address;
    }

    std::string toString() const;
    
    bool setString(const std::string& str);
    
    int compare(const ChainAddress& chain_addr) const {
        if (_id < chain_addr._id) return -1;
        if (_id > chain_addr._id) return  1;
        if (_address < chain_addr._address)   return -1;
        if (_address > chain_addr._address)   return  1;
        return 0;
    }
    
    bool operator==(const ChainAddress& chain_addr) const { return compare(chain_addr) == 0; }
    bool operator<=(const ChainAddress& chain_addr) const { return compare(chain_addr) <= 0; }
    bool operator>=(const ChainAddress& chain_addr) const { return compare(chain_addr) >= 0; }
    bool operator< (const ChainAddress& chain_addr) const { return compare(chain_addr) <  0; }
    bool operator> (const ChainAddress& chain_addr) const { return compare(chain_addr) >  0; }
    
private:
    Address _address;    
    unsigned char _id;
};

#endif
