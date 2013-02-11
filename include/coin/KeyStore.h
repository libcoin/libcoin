// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_KEYSTORE_H
#define BITCOIN_KEYSTORE_H

#include <coin/Export.h>
#include <coin/Address.h>
#include <coin/Key.h>

/// Payee is an interface to a generalized payee, the receiver of money. The payee can choose the generate a new script for each payment, to reuse a single script over and over again, or even to e.g. to use a deterministic wallet to generate a group of public keys.
class Payee {
public:
    virtual Script current_script() = 0;
    virtual void mark_used(const Script&) {}
};

class StaticPayee : public Payee {
public:
    StaticPayee(const PubKeyHash& pkh) : _pkh(pkh) { }
    virtual Script current_script();
private:
    PubKeyHash _pkh;
};

/// KeyStore is a virtual base class for key stores. It enables mappings between PubKeyHashes and ScriptHashes, whether, all
/// "Bitcoin" address specific stuff are kept in derived classes.
class KeyStore {
public:
    // Add a key to the store.
    virtual bool addKey(const CKey& key) = 0;
    
    // Check whether a key corresponding to a given address is present in the store.
    virtual bool haveKey(const PubKeyHash &hash) const = 0;
    virtual bool getKey(const PubKeyHash &hash, CKey& keyOut) const =0;
    virtual void getKeys(std::set<PubKeyHash> &setAddress) const =0;
    virtual bool getPubKey(const PubKeyHash &address, PubKey& vchPubKeyOut) const;
    
    // Support for BIP 0013 : see https://en.bitcoin.it/wiki/BIP_0013
    virtual bool addScript(const Script& redeemScript) = 0;
    virtual bool haveScript(const ScriptHash &hash) const = 0;
    virtual bool getScript(const ScriptHash &hash, Script& redeemScriptOut) const = 0;
    
    // Generate a new key, and add it to the store
    virtual std::vector<unsigned char> generateNewKey();
    virtual bool getSecret(const PubKeyHash &hash, CSecret& secret, bool &compressed) const {
        CKey key;
        if (!getKey(hash, key))
            return false;
        secret = key.GetSecret(compressed);
        return true;
    }
};

typedef std::map<PubKeyHash, std::pair<CSecret, bool> > KeyMap;
typedef std::map<ScriptHash, Script> ScriptMap;

// Basic key store, that keeps keys in an address->secret map
class BasicKeyStore : public KeyStore {
protected:
    KeyMap _keys;
    ScriptMap _scripts;
    
public:
    bool addKey(const CKey& key);
    bool haveKey(const PubKeyHash &hash) const {
        return (_keys.count(hash) > 0);
    }

    void getKeys(std::set<PubKeyHash> &addresses) const {
        addresses.clear();
        KeyMap::const_iterator mi = _keys.begin();
        while (mi != _keys.end()) {
            addresses.insert((*mi).first);
            mi++;
        }
    }
    
    bool getKey(const PubKeyHash& hash, CKey &key) const {
        KeyMap::const_iterator mi = _keys.find(hash);
        if (mi != _keys.end()) {
            key.Reset();
            key.SetSecret((*mi).second.first, (*mi).second.second);
            return true;
        }
        return false;
    }
    
    virtual bool addScript(const Script& redeemScript);
    virtual bool haveScript(const ScriptHash& hash) const;
    virtual bool getScript(const ScriptHash& hash, Script& redeemScriptOut) const;
};

#endif
