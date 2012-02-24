// Copyright (c) 2011 The Bitcoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef _CRYPTOKEYSTORE_H_
#define _CRYPTOKEYSTORE_H_

#include <coin/KeyStore.h>

#include <coinWallet/Export.h>
#include <coinWallet/Crypter.h>

typedef std::map<PubKeyHash, std::pair<std::vector<unsigned char>, std::vector<unsigned char> > > CryptedKeyMap;

/// Keystore which keeps the private keys encrypted
/// It derives from the basic key store, which is used if no encryption is active.
class COINWALLET_EXPORT CCryptoKeyStore : public BasicKeyStore
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
    CCryptoKeyStore() : fUseCrypto(false) {}
    
    bool IsCrypted() const {
        return fUseCrypto;
    }
    
    bool IsLocked() const {
        if (!IsCrypted())
            return false;
        bool result;
        result = vMasterKey.empty();
        return result;
    }
    
    bool Lock() {
        if (!SetCrypted())
            return false;
        
        vMasterKey.clear();
        
        return true;
    }
    
    virtual bool addCryptedKey(const std::vector<unsigned char> &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret);
    bool addKey(const CKey& key);
    bool haveKey(const PubKeyHash &hash) const
    {
        if (!IsCrypted())
            return BasicKeyStore::haveKey(hash);
        return mapCryptedKeys.count(hash) > 0;
        return false;
    }
    bool getKey(const PubKeyHash &hash, CKey& keyOut) const;
    bool getPubKey(const PubKeyHash &hash, std::vector<unsigned char>& vchPubKeyOut) const;
    void getKeys(std::set<PubKeyHash> &setAddress) const {
        if (!IsCrypted()) {
            BasicKeyStore::getKeys(setAddress);
            return;
        }
        setAddress.clear();
        CryptedKeyMap::const_iterator mi = mapCryptedKeys.begin();
        while (mi != mapCryptedKeys.end()) {
            setAddress.insert((*mi).first);
            mi++;
        }
    }
    
protected:
    mutable CCriticalSection cs_KeyStore;
};

#endif // _CRYPTOKEYSTORE_H_