// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

//#include "headers.h>
//#include "db.h>

#include <vector>

#include <coin/KeyStore.h>
#include <coin/Script.h>

PubKey KeyStore::generateNewKey() {
    RandAddSeedPerfmon();
    CKey key;
    key.MakeNewKey();
    if (!addKey(key))
        throw std::runtime_error("CKeyStore::GenerateNewKey() : AddKey failed");
    return key.GetPubKey();
}

bool KeyStore::getPubKey(const PubKeyHash &hash, PubKey &pubkey) const {
    CKey key;
    if (!getKey(hash, key))
        return false;
    pubkey = key.GetPubKey();
    return true;
}

bool BasicKeyStore::addKey(const CKey& key) {
    bool compressed = false;
    CSecret secret = key.GetSecret(compressed);
    _keys[toPubKeyHash(key.GetPubKey())] = make_pair(secret, compressed);
    return true;
}

bool BasicKeyStore::addScript(const Script& redeemScript) {
    ScriptHash sh;
    //    _scripts[toScriptHash(redeemScript)] = redeemScript;
    _scripts[sh] = redeemScript;
    return true;
}

bool BasicKeyStore::haveScript(const ScriptHash& hash) const {
    return (_scripts.count(hash) > 0);
}


bool BasicKeyStore::getScript(const ScriptHash& hash, Script& redeemScript) const {
    ScriptMap::const_iterator mi = _scripts.find(hash);
    if (mi != _scripts.end()) {
        redeemScript = (*mi).second;
        return true;
    }
    return false;
}
