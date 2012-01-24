// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_KEYSTORE_H
#define BITCOIN_KEYSTORE_H

#include "btc/Address.h"
#include "btc/key.h"

class CKeyStore
{
protected:
    mutable CCriticalSection cs_KeyStore;

public:
    virtual bool AddKey(const CKey& key) =0;
    virtual bool HaveKey(const ChainAddress &address) const =0;
    virtual bool HaveKey(const uint160 &asset) const =0;
    virtual bool GetKey(const ChainAddress &address, CKey& keyOut) const =0;
    virtual bool GetKey(const uint160 &asset, CKey& keyOut) const =0;
    virtual bool GetPubKey(const ChainAddress &address, std::vector<unsigned char>& vchPubKeyOut) const;
    virtual bool GetPubKey(const uint160 &asset, std::vector<unsigned char>& vchPubKeyOut) const = 0;
    virtual std::vector<unsigned char> GenerateNewKey();
    virtual unsigned char getId() const = 0;
};

typedef std::map<ChainAddress, CSecret> KeyMap;

class CBasicKeyStore : public CKeyStore
{
protected:
    KeyMap mapKeys;
    unsigned char _id;
public:
    CBasicKeyStore(unsigned char networkId) : _id(networkId) {}
    
    virtual unsigned char getId() const { return _id; }
    
    bool AddKey(const CKey& key);
    bool HaveKey(const ChainAddress &address) const
    {
        bool result;
        CRITICAL_BLOCK(cs_KeyStore)
            result = (mapKeys.count(address) > 0);
        return result;
    }
    bool GetKey(const ChainAddress &address, CKey& keyOut) const
    {
        CRITICAL_BLOCK(cs_KeyStore)
        {
            KeyMap::const_iterator mi = mapKeys.find(address);
            if (mi != mapKeys.end())
            {
                keyOut.SetSecret((*mi).second);
                return true;
            }
        }
        return false;
    }
    virtual bool HaveKey(const uint160 &asset) const { return HaveKey(ChainAddress(_id, asset)); }
    virtual bool GetKey(const uint160 &asset, CKey& keyOut) const { return GetKey(ChainAddress(_id, asset), keyOut); }
    virtual bool GetPubKey(const uint160 &asset, std::vector<unsigned char>& vchPubKeyOut) const { return CKeyStore::GetPubKey(ChainAddress(_id, asset), vchPubKeyOut); }
};

#endif
