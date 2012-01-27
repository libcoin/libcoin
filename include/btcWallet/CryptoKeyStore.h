// Copyright (c) 2011 The Bitcoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef _CRYPTOKEYSTORE_H_
#define _CRYPTOKEYSTORE_H_

#include "btc/keystore.h"
#include "btcWallet/Crypter.h"

typedef std::map<ChainAddress, std::pair<std::vector<unsigned char>, std::vector<unsigned char> > > CryptedKeyMap;

class CCryptoKeyStore : public CBasicKeyStore
{
private:
    CryptedKeyMap mapCryptedKeys;
    
    CKeyingMaterial vMasterKey;
    
    // if fUseCrypto is true, mapKeys must be empty
    // if fUseCrypto is false, vMasterKey must be empty
    bool fUseCrypto;
    
protected:
    bool SetCrypted();
    
    // will encrypt previously unencrypted keys
    bool EncryptKeys(CKeyingMaterial& vMasterKeyIn);
    
    bool Unlock(const CKeyingMaterial& vMasterKeyIn);
    
public:
    CCryptoKeyStore(unsigned char networkId) : CBasicKeyStore(networkId), fUseCrypto(false)
    {
    }
    
    bool IsCrypted() const
    {
    return fUseCrypto;
    }
    
    bool IsLocked() const
    {
    if (!IsCrypted())
        return false;
    bool result;
    CRITICAL_BLOCK(cs_KeyStore)
    result = vMasterKey.empty();
    return result;
    }
    
    bool Lock()
    {
    if (!SetCrypted())
        return false;
    
    CRITICAL_BLOCK(cs_KeyStore)
    vMasterKey.clear();
    
    return true;
    }
    
    virtual bool AddCryptedKey(const std::vector<unsigned char> &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret);
    std::vector<unsigned char> GenerateNewKey();
    bool AddKey(const CKey& key);
    bool HaveKey(const ChainAddress &address) const
    {
    CRITICAL_BLOCK(cs_KeyStore)
        {
        if (!IsCrypted())
            return CBasicKeyStore::HaveKey(address);
        return mapCryptedKeys.count(address) > 0;
        }
    }
    bool GetKey(const ChainAddress &address, CKey& keyOut) const;
    virtual bool GetPubKey(const ChainAddress &address, std::vector<unsigned char>& vchPubKeyOut) const;

protected:
    mutable CCriticalSection cs_KeyStore;
};

#endif // _CRYPTOKEYSTORE_H_