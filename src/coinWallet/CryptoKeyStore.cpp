#include <coin/Script.h>
#include <coin/KeyStore.h>
#include <coinWallet/CryptoKeyStore.h>

#include <boost/foreach.hpp>

bool CCryptoKeyStore::SetCrypted()
{
    CRITICAL_BLOCK(cs_KeyStore)
    {
    if (fUseCrypto)
        return true;
    if (!_keys.empty())
        return false;
    fUseCrypto = true;
    }
    return true;
}

bool CCryptoKeyStore::Unlock(const CKeyingMaterial& vMasterKeyIn)
{
    CRITICAL_BLOCK(cs_KeyStore)
    {
    if (!SetCrypted())
        return false;
    
    CryptedKeyMap::const_iterator mi = mapCryptedKeys.begin();
    for (; mi != mapCryptedKeys.end(); ++mi)
        {
        const PubKey &vchPubKey = (*mi).second.first;
        const std::vector<unsigned char> &vchCryptedSecret = (*mi).second.second;
        CSecret vchSecret;
        if(!DecryptSecret(vMasterKeyIn, vchCryptedSecret, Hash(vchPubKey.begin(), vchPubKey.end()), vchSecret))
            return false;
        CKey key;
        key.SetSecret(vchSecret);
        if (key.GetPubKey() == vchPubKey)
            break;
        return false;
        }
    vMasterKey = vMasterKeyIn;
    }
    return true;
}

bool CCryptoKeyStore::addKey(const CKey& key)
{
    CRITICAL_BLOCK(cs_KeyStore)
    {
    if (!IsCrypted())
        return BasicKeyStore::addKey(key);
    
    if (IsLocked())
        return false;
    
    std::vector<unsigned char> vchCryptedSecret;
    PubKey vchPubKey = key.GetPubKey();
    bool fCompressed;
    if (!EncryptSecret(vMasterKey, key.GetSecret(fCompressed), Hash(vchPubKey.begin(), vchPubKey.end()), vchCryptedSecret))
        return false;
    
    if (!addCryptedKey(key.GetPubKey(), vchCryptedSecret))
        return false;
    }
    return true;
}


bool CCryptoKeyStore::addCryptedKey(const PubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret)
{
    CRITICAL_BLOCK(cs_KeyStore)
    {
        if (!SetCrypted())
            return false;
        
        mapCryptedKeys[toPubKeyHash(vchPubKey)] = make_pair(vchPubKey, vchCryptedSecret);
    }
    return true;
}

bool CCryptoKeyStore::getKey(const PubKeyHash &address, CKey& keyOut) const
{
    CRITICAL_BLOCK(cs_KeyStore)
    {
    if (!IsCrypted())
        return BasicKeyStore::getKey(address, keyOut);
    
    CryptedKeyMap::const_iterator mi = mapCryptedKeys.find(address);
    if (mi != mapCryptedKeys.end())
        {
        const PubKey &vchPubKey = (*mi).second.first;
        const std::vector<unsigned char> &vchCryptedSecret = (*mi).second.second;
        CSecret vchSecret;
        if (!DecryptSecret(vMasterKey, vchCryptedSecret, Hash(vchPubKey.begin(), vchPubKey.end()), vchSecret))
            return false;
        keyOut.SetSecret(vchSecret);
        return true;
        }
    }
    return false;
}

bool CCryptoKeyStore::getPubKey(const PubKeyHash &address, PubKey& vchPubKeyOut) const
{
    CRITICAL_BLOCK(cs_KeyStore)
    {
    if (!IsCrypted())
        return KeyStore::getPubKey(address, vchPubKeyOut);
    
    CryptedKeyMap::const_iterator mi = mapCryptedKeys.find(address);
    if (mi != mapCryptedKeys.end())
        {
        vchPubKeyOut = (*mi).second.first;
        return true;
        }
    }
    return false;
}

bool CCryptoKeyStore::EncryptKeys(CKeyingMaterial& vMasterKeyIn)
{
    CRITICAL_BLOCK(cs_KeyStore)
    {
        if (!mapCryptedKeys.empty() || IsCrypted())
            return false;
        
        fUseCrypto = true;
        CKey key;
        BOOST_FOREACH(KeyMap::value_type& mKey, _keys)
        {
            if (!key.SetSecret(mKey.second.first, false))
                return false;
            const std::vector<unsigned char> vchPubKey = key.GetPubKey();
            std::vector<unsigned char> vchCryptedSecret;
            bool fCompressed;
            if (!EncryptSecret(vMasterKeyIn, key.GetSecret(fCompressed), Hash(vchPubKey.begin(), vchPubKey.end()), vchCryptedSecret))
                return false;
            if (!addCryptedKey(vchPubKey, vchCryptedSecret))
                return false;
        }
        _keys.clear();
    }
    return true;
}
