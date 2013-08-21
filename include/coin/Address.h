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
#include <coin/Export.h>
#include <coin/BigNum.h>

// Why base-58 instead of standard base-64 encoding?
// - Don't want 0OIl characters that look the same in some fonts and
//      could be used to create visually identical looking account numbers.
// - A string with non-alphanumeric characters is not as easily accepted as an account number.
// - E-mail usually won't line-break if there's no punctuation to break at.
// - Doubleclicking selects the whole number as one word if it's all alphanumeric.

/// Utility functions
std::string EncodeBase58Check(const std::vector<unsigned char>& vchIn, const char* alphabet = NULL);
bool DecodeBase58Check(const std::string& str, std::vector<unsigned char>& vchRet, const char* alphabet = NULL);

// This is the raw PubKeyHash - the BitcoinAddress or more correctly the ChainAddress includes the netword Id and is hence better suited for communicating with the outside. For backwards compatability we do, however, need to use the ChainAddress in e.g. the wallet. 

typedef std::vector<unsigned char> PubKey;

class COIN_EXPORT PubKeyHash : public uint160 {
public:
    PubKeyHash(const uint160& b = uint160(0)) : uint160(b) {}
    PubKeyHash(uint64_t b) : uint160(b) {}
    
    PubKeyHash& operator=(const uint160& b) {
        uint160::operator=(b);
        return *this;
    }
    PubKeyHash& operator=(uint64_t b) {
        uint160::operator=(b);
        return *this;
    }
    
    explicit PubKeyHash(const std::string& str) : uint160(str) {}
    
    explicit PubKeyHash(const std::vector<unsigned char>& vch) : uint160(vch) {}
};

PubKeyHash toPubKeyHash(const PubKey& pubkey);
PubKeyHash toPubKeyHash(const std::string& str);

class COIN_EXPORT ScriptHash : public uint160 {
public:
    ScriptHash(const uint160& b = 0) : uint160(b) {}
    ScriptHash(uint64_t b) : uint160(b) {}
    
    ScriptHash& operator=(const uint160& b) {
        uint160::operator=(b);
        return *this;
    }
    ScriptHash& operator=(uint64_t b) {
        uint160::operator=(b);
        return *this;
    }
    
    explicit ScriptHash(const std::string& str) : uint160(str) {}
    
    explicit ScriptHash(const std::vector<unsigned char>& vch) : uint160(vch) {}
};

class Script;
ScriptHash toScriptHash(const Script& script);


// Base class for all base58-encoded data
class COIN_EXPORT Base58Data
{
protected:
    // the version byte
    unsigned char _version;
    
    // the actually encoded data
    std::vector<unsigned char> _data;
    
    void setData(int version, const void* data, size_t size) {
        _version = version;
        _data.resize(size);
        if (!_data.empty())
            memcpy(&_data[0], data, size);
    }
    
    void setData(int version, const unsigned char *pbegin, const unsigned char *pend) {
        setData(version, (void*)pbegin, pend - pbegin);
    }
    
public:
    Base58Data() {
        _version = 0;
        _data.clear();
    }
    
    ~Base58Data() {
        // zero the memory, as it may contain sensitive data
        if(!_data.empty())
            memset(&_data[0], 0, _data.size());
    }
    
    Base58Data(unsigned char version, std::vector<unsigned char> data) : _version(version), _data(data) {
    }
    
    bool setString(const std::string& str) {
        std::vector<unsigned char> temp;
        DecodeBase58Check(str, temp);
        if (temp.empty()) {
            _data.clear();
            _version = 0;
            return false;
        }
        _version = temp[0];
        _data.resize(temp.size() - 1);
        if (!_data.empty())
            memcpy(&_data[0], &temp[1], _data.size());
        memset(&temp[0], 0, temp.size());
        return true;
    }
    
    std::string toString() const {
        std::vector<unsigned char> temp(1, _version);
        temp.insert(temp.end(), _data.begin(), _data.end());
        return EncodeBase58Check(temp);
    }
    
    unsigned char version() const { return _version; } 
    
    int compare(const Base58Data& b58) const {
        if (_version < b58._version) return -1;
        if (_version > b58._version) return  1;
        if (_data < b58._data)   return -1;
        if (_data > b58._data)   return  1;
        return 0;
    }
    
    bool operator==(const Base58Data& b58) const { return compare(b58) == 0; }
    bool operator<=(const Base58Data& b58) const { return compare(b58) <= 0; }
    bool operator>=(const Base58Data& b58) const { return compare(b58) >= 0; }
    bool operator< (const Base58Data& b58) const { return compare(b58) <  0; }
    bool operator> (const Base58Data& b58) const { return compare(b58) >  0; }
};

// base58-encoded bitcoin addresses
// Public-key-hash-addresses have version 0 (or 192 testnet)
// The data vector contains RIPEMD160(SHA256(pubkey)), where pubkey is the serialized public key
// Script-hash-addresses have version 5 (or 196 testnet)
// The data vector contains RIPEMD160(SHA256(cscript)), where cscript is the serialized redemption script
class COIN_EXPORT ChainAddress : public Base58Data
{
protected:
    uint160 getHash() const {
        assert(_data.size() == 20);
        uint160 hash160;
        memcpy(&hash160, &_data[0], 20);
        return hash160;
    }
    
public:
    enum Type {
        PUBKEYHASH,
        SCRIPTHASH,
        UNDEFINED
    };
    
    bool setHash(unsigned char version, const PubKeyHash& pubKeyHash) {
        setData(version, &pubKeyHash, 20);
        _type = PUBKEYHASH;
        return true;
    }
    
    void setPubKey(unsigned char version, const PubKey& pubKey) {
        PubKeyHash pubKeyHash = toPubKeyHash(pubKey);
        setData(version, &pubKeyHash, 20);
        _type = PUBKEYHASH;
    }
    
    void setType(Type type) { _type = type; }
    
    bool setHash(unsigned char version, const ScriptHash& scriptHash) {
        setData(version, &scriptHash, 20);
        _type = SCRIPTHASH;
        return true;
    }
        
    bool isValid() const {
        return (_data.size() == 20) && (getHash() != 0) && (_type != UNDEFINED);
    }
    
    bool isScriptHash() const {
        return _type == SCRIPTHASH;
    }
    
    bool isPubKeyHash() const {
        return _type == PUBKEYHASH;
    }
    
    PubKeyHash getPubKeyHash() const {
        //        if(!isPubKeyHash())
        //            return 0;
        //else
            return getHash();
    }
    
    ScriptHash getScriptHash() const {
        if(!isScriptHash())
            return 0;
        else
            return getHash();
    }
    
    
    ChainAddress() : _type(UNDEFINED) {}
    
    ChainAddress(unsigned char version, const PubKeyHash& pubKeyHash) {
        setHash(version, pubKeyHash);
    }

    ChainAddress(unsigned char version, const PubKey& pubKey) {
        setPubKey(version, pubKey);
    }

    ChainAddress(unsigned char version, const ScriptHash& scriptHash) {
        setHash(version, scriptHash);
    }

    explicit ChainAddress(const std::string& addr) {
        setString(addr);
    }
    
protected:
    Type _type;
};



/// The ChainAddress supports reading and writing base58 addresses.
/*
class COIN_EXPORT ChainAddress
{
private:
    const Chain& _chain;

public:
    ChainAddress() : 
    bool setHash160(unsigned char version, const PubKeyHash& address) {
        _version = version;
        _hash = hash;
        return true;
    }

    bool setPubKey(unsigned char network_id, const std::vector<unsigned char>& pubKey) {
        return setHash160(network_id, toPubKeyHash(pubKey));
    }

    bool isValid(unsigned char network_id) const {
        return network_id == _id && _address != 0;
    }
    
    unsigned char networkId() const {
        return _id;
    }

    ChainAddress() : _address(0), _id(0) { }

    ChainAddress(unsigned char network_id, PubKeyHash address) {
        setHash160(network_id, address);
    }

    ChainAddress(unsigned char network_id, const std::vector<unsigned char>& pubKey) {
        setPubKey(network_id, pubKey);
    }

    ChainAddress(const std::string& str) {
        setString(str);
    }

    PubKeyHash getPubKeyHash() const {
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
    PubKeyHash _version;    
    unsigned char _id;
};
 
 */

#endif
