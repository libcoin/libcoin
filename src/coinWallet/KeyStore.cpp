// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

//#include "headers.h>
//#include "db.h>

#include <vector>

#include <coinWallet/KeyStore.h>
#include <coin/Script.h>
#include <coin/Transaction.h>

using namespace std;

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

bool Sign1(const PubKeyHash& pubKeyhash, const KeyStore& keystore, uint256 hash, int nHashType, Script& scriptSigRet)
{
    CKey key;
    if (!keystore.getKey(pubKeyhash, key))
        return false;
    
    vector<unsigned char> vchSig;
    if (!key.Sign(hash, vchSig))
        return false;
    vchSig.push_back((unsigned char)nHashType);
    scriptSigRet << vchSig;
    
    return true;
}

bool SignN(const vector<valtype>& multisigdata, const KeyStore& keystore, uint256 hash, int nHashType, Script& scriptSigRet)
{
    int nSigned = 0;
    int nRequired = multisigdata.front()[0];
    for (vector<valtype>::const_iterator it = multisigdata.begin()+1; it != multisigdata.begin()+multisigdata.size()-1; it++) {
        const valtype& pubkey = *it;
        PubKeyHash pubKeyHash;
        pubKeyHash = toPubKeyHash(pubkey);
        if (Sign1(pubKeyHash, keystore, hash, nHashType, scriptSigRet)) {
            ++nSigned;
            if (nSigned == nRequired) break;
        }
    }
    return nSigned==nRequired;
}

//
// Sign scriptPubKey with private keys stored in keystore, given transaction hash and hash type.
// Signatures are returned in scriptSigRet (or returns false if scriptPubKey can't be signed),
// unless whichTypeRet is TX_SCRIPTHASH, in which case scriptSigRet is the redemption script.
// Returns false if scriptPubKey could not be completely satisified.
//
bool Solver(const KeyStore& keystore, const Script& scriptPubKey, uint256 hash, int nHashType, Script& scriptSigRet, txnouttype& whichTypeRet)
{
    scriptSigRet.clear();
    
    vector<valtype> vSolutions;
    if (!Solver(scriptPubKey, whichTypeRet, vSolutions))
        return false;
    
    PubKeyHash pubKeyhash;
    switch (whichTypeRet) {
        case TX_NONSTANDARD:
            return false;
        case TX_PUBKEY:
            pubKeyhash = toPubKeyHash(vSolutions[0]);
            return Sign1(pubKeyhash, keystore, hash, nHashType, scriptSigRet);
        case TX_PUBKEYHASH:
            pubKeyhash = uint160(vSolutions[0]);
            if (!Sign1(pubKeyhash, keystore, hash, nHashType, scriptSigRet))
                return false;
            else {
                valtype vch;
                keystore.getPubKey(pubKeyhash, vch);
                scriptSigRet << vch;
            }
            return true;
        case TX_SCRIPTHASH:
            return keystore.getScript(uint160(vSolutions[0]), scriptSigRet);
            
        case TX_MULTISIG:
            scriptSigRet << OP_0; // workaround CHECKMULTISIG bug
            return (SignN(vSolutions, keystore, hash, nHashType, scriptSigRet));
    }
    return false;
}

bool ExtractAddress(const Script& script, PubKeyHash& pubKeyHash, ScriptHash& scriptHash)
{
    pubKeyHash = 0;
    scriptHash = 0;
    
    vector<valtype> vSolutions;
    txnouttype whichType;
    if (!Solver(script, whichType, vSolutions))
        return false;
    
    if (whichType == TX_PUBKEY) {
        pubKeyHash = toPubKeyHash(vSolutions[0]);
        return true;
    }
    else if (whichType == TX_PUBKEYHASH) {
        pubKeyHash = uint160(vSolutions[0]);
        return true;
    }
    else if (whichType == TX_SCRIPTHASH) {
        scriptHash = uint160(vSolutions[0]);
        return true;
    }
    // Multisig txns have more than one address...
    return false;
}

// Note: Returning also ScriptHashes s not supported, and this function is not used for that purpose anyway.
bool ExtractAddresses(const Script& script, txnouttype& typeRet, vector<PubKeyHash>& hashes, int& nRequiredRet)
{
    hashes.clear();
    typeRet = TX_NONSTANDARD;
    vector<valtype> vSolutions;
    if (!Solver(script, typeRet, vSolutions))
        return false;
    
    if (typeRet == TX_MULTISIG) {
        nRequiredRet = vSolutions.front()[0];
        for (unsigned int i = 1; i < vSolutions.size()-1; i++) {
            PubKeyHash hash;
            hash = toPubKeyHash(vSolutions[i]);
            hashes.push_back(hash);
        }
    }
    else {
        nRequiredRet = 1;
        PubKeyHash hash;
        if (typeRet == TX_PUBKEYHASH)
            hash = uint160(vSolutions.front());
        else if (typeRet == TX_PUBKEY)
            hash = toPubKeyHash(vSolutions.front());
        hashes.push_back(hash);
    }
    
    return true;
}

int HaveKeys(const std::vector<valtype>& pubkeys, const KeyStore& keystore)
{
    int nResult = 0;
    BOOST_FOREACH(const valtype& pubkey, pubkeys) {
        PubKeyHash hash;
        hash = toPubKeyHash(pubkey);
        if (keystore.haveKey(hash))
            ++nResult;
    }
    return nResult;
}

bool IsMine(const KeyStore &keystore, const Script& scriptPubKey)
{
    std::vector<valtype> vSolutions;
    txnouttype whichType;
    if (!Solver(scriptPubKey, whichType, vSolutions))
        return false;
    
    PubKeyHash pubKeyHash;
    switch (whichType) {
        case TX_NONSTANDARD:
            return false;
        case TX_PUBKEY:
            pubKeyHash = toPubKeyHash(vSolutions[0]);
            return keystore.haveKey(pubKeyHash);
        case TX_PUBKEYHASH:
            pubKeyHash = uint160(vSolutions[0]);
            return keystore.haveKey(pubKeyHash);
        case TX_SCRIPTHASH: {
            Script subscript;
            ScriptHash scriptHash = uint160(vSolutions[0]);
            if (!keystore.getScript(scriptHash, subscript))
                return false;
            return IsMine(keystore, subscript);
        }
        case TX_MULTISIG: {
            // Only consider transactions "mine" if we own ALL the
            // keys involved. multi-signature transactions that are
            // partially owned (somebody else has a key that can spend
            // them) enable spend-out-from-under-you attacks, especially
            // in shared-wallet situations.
            std::vector<valtype> keys(vSolutions.begin()+1, vSolutions.begin()+vSolutions.size()-1);
            return (unsigned int)HaveKeys(keys, keystore) == keys.size();
        }
    }
    return false;
}

bool SignSignature(const KeyStore &keystore, const Transaction& txFrom, Transaction& txTo, unsigned int nIn, int nHashType)
{
    assert(nIn < txTo.getNumInputs());
    const Input& txin = txTo.getInput(nIn);
    assert(txin.prevout().index < txFrom.getNumOutputs());
    const Output& txout = txFrom.getOutput(txin.prevout().index);
    return SignSignature(keystore, txout, txTo, nIn, nHashType);
}

bool SignSignature(const KeyStore &keystore, const Output& txout, Transaction& txTo, unsigned int nIn, int nHashType)
{
    assert(nIn < txTo.getNumInputs());
    const Input& txin = txTo.getInput(nIn);
    
    // Leave out the signature from the hash, since a signature can't sign itself.
    // The checksig op will also drop the signatures from its hash.
    uint256 hash = txTo.getSignatureHash(txout.script(), nIn, nHashType);
    
    txnouttype whichType;
    Script signature;
    
    if (!Solver(keystore, txout.script(), hash, nHashType, signature, whichType))
        return false;
    
    if (whichType == TX_SCRIPTHASH) {
        // Solver returns the subscript that need to be evaluated;
        // the final scriptSig is the signatures from that
        // and then the serialized subscript:
        Script subscript = txin.signature();
        
        // Recompute txn hash using subscript in place of scriptPubKey:
        uint256 hash2 = txTo.getSignatureHash(subscript, nIn, nHashType);
        txnouttype subType;
        if (!Solver(keystore, subscript, hash2, nHashType, signature, subType))
            return false;
        if (subType == TX_SCRIPTHASH) // avoid recursive scripts
            return false;
        signature << static_cast<valtype>(subscript); // Append serialized subscript
    }
    
    txTo.replaceInput(nIn, Input(txin.prevout(), signature, txTo.getInput(nIn).sequence()));
    
    // Test solution
    if (!txTo.verify(nIn, txout.script()))
        //    if (!VerifyScript(txout.script(), txTo, nIn, false, 0))
        return false;
    
    return true;
}

