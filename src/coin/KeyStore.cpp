// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

//#include "headers.h>
//#include "db.h>

#include <vector>

#include <coin/KeyStore.h>
#include <coin/Script.h>
#include <coin/Transaction.h>

using namespace std;

Script StaticPayee::current_script() {
    Script script;
    script.setAddress(_pkh);
    return script;
}


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


bool Solver(const KeyStore& keystore, const Script& scriptPubKey, uint256 hash, int nHashType, Script& scriptSigRet)
{
    scriptSigRet.clear();
    
    std::vector<pair<opcodetype, valtype> > vSolution;
    if (!Solver(scriptPubKey, vSolution))
        return false;
    
    // Compile solution
    BOOST_FOREACH(PAIRTYPE(opcodetype, valtype)& item, vSolution)
    {
    if (item.first == OP_PUBKEY)
        {
        // Sign
        const valtype& vchPubKey = item.second;
        CKey key;
        if (!keystore.getKey(toPubKeyHash(vchPubKey), key))
            return false;
        if (key.GetPubKey() != vchPubKey)
            return false;
        if (hash != 0)
            {
            vector<unsigned char> vchSig;
            if (!key.Sign(hash, vchSig))
                return false;
            vchSig.push_back((unsigned char)nHashType);
            scriptSigRet << vchSig;
            }
        }
    else if (item.first == OP_PUBKEYHASH)
        {
        // Sign and give pubkey
        CKey key;
        if (!keystore.getKey(uint160(item.second), key))
            return false;
        if (hash != 0)
            {
            vector<unsigned char> vchSig;
            if (!key.Sign(hash, vchSig))
                return false;
            vchSig.push_back((unsigned char)nHashType);
            scriptSigRet << vchSig << key.GetPubKey();
            }
        }
    else
        {
        return false;
        }
    }
    
    return true;
}

//
// Return public keys or hashes from scriptPubKey, for 'standard' transaction types.
//
bool Solver(const Script& scriptPubKey, txnouttype& typeRet, vector<vector<unsigned char> >& vSolutionsRet)
{
    // Templates
    static map<txnouttype, Script> mTemplates;
    if (mTemplates.empty()) {
        // Standard tx, sender provides pubkey, receiver adds signature
        mTemplates.insert(make_pair(TX_PUBKEY, Script() << OP_PUBKEY << OP_CHECKSIG));
        
        // Bitcoin address tx, sender provides hash of pubkey, receiver provides signature and pubkey
        mTemplates.insert(make_pair(TX_PUBKEYHASH, Script() << OP_DUP << OP_HASH160 << OP_PUBKEYHASH << OP_EQUALVERIFY << OP_CHECKSIG));
        
        // Sender provides N pubkeys, receivers provides M signatures
        mTemplates.insert(make_pair(TX_MULTISIG, Script() << OP_SMALLINTEGER << OP_PUBKEYS << OP_SMALLINTEGER << OP_CHECKMULTISIG));
    }
    
    // Shortcut for pay-to-script-hash, which are more constrained than the other types:
    // it is always OP_HASH160 20 [20 byte hash] OP_EQUAL
    if (scriptPubKey.isPayToScriptHash()) {
        typeRet = TX_SCRIPTHASH;
        vector<unsigned char> hashBytes(scriptPubKey.begin()+2, scriptPubKey.begin()+22);
        vSolutionsRet.push_back(hashBytes);
        return true;
    }
    
    // Scan templates
    const Script& script1 = scriptPubKey;
    BOOST_FOREACH(const PAIRTYPE(txnouttype, Script)& tplate, mTemplates) {
        const Script& script2 = tplate.second;
        vSolutionsRet.clear();
        
        opcodetype opcode1, opcode2;
        vector<unsigned char> vch1, vch2;
        
        // Compare
        Script::const_iterator pc1 = script1.begin();
        Script::const_iterator pc2 = script2.begin();
        loop {
            if (pc1 == script1.end() && pc2 == script2.end()) {
                // Found a match
                typeRet = tplate.first;
                if (typeRet == TX_MULTISIG) {
                    // Additional checks for TX_MULTISIG:
                    unsigned char m = vSolutionsRet.front()[0];
                    unsigned char n = vSolutionsRet.back()[0];
                    if (m < 1 || n < 1 || m > n || vSolutionsRet.size()-2 != n)
                        return false;
                }
                return true;
            }
            if (!script1.getOp(pc1, opcode1, vch1))
                break;
            if (!script2.getOp(pc2, opcode2, vch2))
                break;
            
            // Template matching opcodes:
            if (opcode2 == OP_PUBKEYS) {
                while (vch1.size() >= 33 && vch1.size() <= 120) {
                    vSolutionsRet.push_back(vch1);
                    if (!script1.getOp(pc1, opcode1, vch1))
                        break;
                }
                if (!script2.getOp(pc2, opcode2, vch2))
                    break;
                // Normal situation is to fall through
                // to other if/else statments
            }
            
            if (opcode2 == OP_PUBKEY) {
                if (vch1.size() < 33 || vch1.size() > 120)
                    break;
                vSolutionsRet.push_back(vch1);
            }
            else if (opcode2 == OP_PUBKEYHASH) {
                if (vch1.size() != sizeof(uint160))
                    break;
                vSolutionsRet.push_back(vch1);
            }
            else if (opcode2 == OP_SMALLINTEGER) {   // Single-byte small integer pushed onto vSolutions
                if (opcode1 == OP_0 || (opcode1 >= OP_1 && opcode1 <= OP_16)) {
                    char n = (char)Script::decodeOP_N(opcode1);
                    vSolutionsRet.push_back(valtype(1, n));
                }
                else
                    break;
            }
            else if (opcode1 != opcode2 || vch1 != vch2) {
                // Others must match exactly
                break;
            }
        }
    }
    
    vSolutionsRet.clear();
    typeRet = TX_NONSTANDARD;
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

