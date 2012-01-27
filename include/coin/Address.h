

#ifndef _ADDRESS_H
#define _ADDRESS_H

#include <string>
#include <vector>
#include "btc/bignum.h"

// Why base-58 instead of standard base-64 encoding?
// - Don't want 0OIl characters that look the same in some fonts and
//      could be used to create visually identical looking account numbers.
// - A string with non-alphanumeric characters is not as easily accepted as an account number.
// - E-mail usually won't line-break if there's no punctuation to break at.
// - Doubleclicking selects the whole number as one word if it's all alphanumeric.

/// Utility functions
inline std::string EncodeBase58Check(const std::vector<unsigned char>& vchIn);
inline bool DecodeBase58Check(const std::string& str, std::vector<unsigned char>& vchRet);

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
