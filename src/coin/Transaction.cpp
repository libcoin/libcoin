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

#include <coin/util.h>
#include <coin/Transaction.h>
#include <coin/KeyStore.h>

using namespace std;
using namespace boost;

//////////////////////////////////////////////////////////////////////////////
//
// Output
//

uint160 Output::getAddress() const
{
    vector<pair<opcodetype, vector<unsigned char> > > vSolution;
    if (!Solver(_script, vSolution))
        return 0;
    
    BOOST_FOREACH(PAIRTYPE(opcodetype, vector<unsigned char>)& item, vSolution)
    {
        vector<unsigned char> vchPubKey;
        if (item.first == OP_PUBKEY)
            // encode the pubkey into a hash160
            return toPubKeyHash(item.second);
        else if (item.first == OP_PUBKEYHASH)
            return uint160(item.second);                
    }
    
    return 0;
}


//////////////////////////////////////////////////////////////////////////////
//
// Transaction
//

bool Transaction::isNewerThan(const Transaction& old) const {
    if (_inputs.size() != old._inputs.size())
        return false;
    for (unsigned int i = 0; i < _inputs.size(); i++)
        if (_inputs[i].prevout() != old._inputs[i].prevout())
            return false;
    
    bool newer = false;
    unsigned int lowest = UINT_MAX;
    for (unsigned int i = 0; i < _inputs.size(); i++) {
        if (_inputs[i].sequence() != old._inputs[i].sequence()) {
            if (_inputs[i].sequence() <= lowest) {
                newer = false;
                lowest = _inputs[i].sequence();
            }
            if (old._inputs[i].sequence() < lowest) {
                newer = true;
                lowest = old._inputs[i].sequence();
            }
        }
    }
    return newer;
}

int Transaction::getSigOpCount() const {
    int n = 0;
    BOOST_FOREACH(const Input& input, _inputs)
    n += input.signature().getSigOpCount();
    BOOST_FOREACH(const Output& output, _outputs)
    n += output.script().getSigOpCount();
    return n;
}

int64_t Transaction::getValueOut() const
{
    int64_t valueOut = 0;
    BOOST_FOREACH(const Output& output, _outputs) {
        valueOut += output.value();
    }
    return valueOut;
}

int64_t Transaction::paymentTo(PubKeyHash address) const {
    int64_t value = 0;
    BOOST_FOREACH(const Output& output, _outputs)
    {
    if(output.getAddress() == address)
        value += output.value();
    }
    
    return value;
}

bool Transaction::isSufficient(map<PubKeyHash, int64_t> payments) const {
    for (map<PubKeyHash, int64_t>::const_iterator payment = payments.begin(); payment != payments.end(); ++payment) {
        if (paymentTo(payment->first) < payment->second)
            return false;
    }
    return true;
}

int64_t Transaction::getMinFee(unsigned int nBlockSize, bool fAllowFree, bool fForRelay) const {
    // Base fee is either MIN_TX_FEE or MIN_RELAY_TX_FEE
    int64_t nBaseFee = fForRelay ? MIN_RELAY_TX_FEE : MIN_TX_FEE;
    
    unsigned int nBytes = serialize_size(*this);
    unsigned int nNewBlockSize = nBlockSize + nBytes;
    int64_t nMinFee = (1 + (int64_t)nBytes / 1000) * nBaseFee;
    
    if (fAllowFree) {
        if (nBlockSize == 1) {
            // Transactions under 10K are free
            // (about 4500bc if made of 50bc inputs)
            if (nBytes < 10000)
                nMinFee = 0;
        }
        else {
            // Free transaction area
            if (nNewBlockSize < 27000)
                nMinFee = 0;
        }
    }
    
    // To limit dust spam, require MIN_TX_FEE/MIN_RELAY_TX_FEE if any output is less than 0.01
    if (nMinFee < nBaseFee) {
        BOOST_FOREACH(const Output& output, _outputs) {
            if (output.value() < CENT)
                nMinFee = nBaseFee;
        }
    }
    
    // Raise the price as the block approaches full
    if (nBlockSize != 1 && nNewBlockSize >= MAX_BLOCK_SIZE_GEN/2) {
        if (nNewBlockSize >= MAX_BLOCK_SIZE_GEN)
            return MAX_MONEY;
        nMinFee *= MAX_BLOCK_SIZE_GEN / (MAX_BLOCK_SIZE_GEN - nNewBlockSize);
    }
    
    if (!MoneyRange(nMinFee))
        nMinFee = MAX_MONEY;
    return nMinFee;
}

uint256 Transaction::getSignatureHash(Script scriptCode, unsigned int n, int type) const {
    if (n >= _inputs.size()) {
        printf("ERROR: SignatureHash() : nIn=%d out of range\n", n);
        return uint256(1);
    }
    Transaction txn(*this);
    
    // In case concatenating two scripts ends up with two codeseparators,
    // or an extra one at the end, this prevents all those possible incompatibilities.
    scriptCode.findAndDelete(Script(OP_CODESEPARATOR));
    
    // Blank out other inputs' signatures
    for (unsigned int i = 0; i < txn._inputs.size(); i++)
        txn._inputs[i].signature(Script());
    txn._inputs[n].signature(scriptCode);
    
    // Blank out some of the outputs
    if ((type & 0x1f) == SIGHASH_NONE) {
        // Wildcard payee
        txn._outputs.clear();
        
        // Let the others update at will
        for (unsigned int i = 0; i < txn._inputs.size(); i++)
            if (i != n)
                txn._inputs[i].sequence(0);
    }
    else if ((type & 0x1f) == SIGHASH_SINGLE) {
        // Only lock-in the txout payee at same index as txin
        if (n >= txn._outputs.size()) {
            printf("ERROR: SignatureHash() : nOut=%d out of range\n", n);
            return uint256(1);
        }
        txn._outputs.resize(n + 1);
        for (unsigned int i = 0; i < n; i++)
            txn._outputs[i].setNull();
        
        // Let the others update at will
        for (unsigned int i = 0; i < txn._inputs.size(); i++)
            if (i != n)
                txn._inputs[i].sequence(0);
    }
    
    // Blank out other inputs completely, not recommended for open transactions
    if (type & SIGHASH_ANYONECANPAY) {
        txn._inputs[0] = txn._inputs[n];
        txn._inputs.resize(1);
    }
    
    // Serialize and hash
    ostringstream os;
    os << txn << const_binary<int>(type);
    string ss = os.str();
    
    return Hash(ss.begin(), ss.end());
}

static bool CastToBool(const valtype& vch) {
    for (unsigned int i = 0; i < vch.size(); i++)
        {
        if (vch[i] != 0)
            {
            // Can be negative zero
            if (i == vch.size()-1 && vch[i] == 0x80)
                return false;
            return true;
            }
        }
    return false;
}


bool Transaction::verify(unsigned int n, Script script, int type, bool strictPayToScriptHash) const {
    Evaluator::Stack stackCopy;
    
    const Input& input = getInput(n);
    const Script& scriptSig = input.signature();
    
    TransactionEvaluator eval(*this, n, type);
    
    if (!eval(scriptSig) || eval.stack().empty())
        return false;
    if (strictPayToScriptHash)
        stackCopy = eval.stack();
    if (!eval(script) || eval.stack().empty())
        return false;
    
    if (CastToBool(eval.stack().back()) == false)
        return false;
    
    // Additional validation for spend-to-script-hash transactions:
    if (strictPayToScriptHash && script.isPayToScriptHash()) {
        if (!scriptSig.isPushOnly()) // scriptSig must be literals-only
            return false;            // or validation fails
        
        const Evaluator::Value& pubKeySerialized = stackCopy.back();
        Script pubKey2(pubKeySerialized.begin(), pubKeySerialized.end());
        stackCopy.pop_back();
        
        eval.stack(stackCopy);
        
        if (!eval(pubKey2) || eval.stack().empty())
            return false;
        return CastToBool(eval.stack().back());
    }
    
    return true;
}

static const size_t nMaxNumSize = 4;
static CBigNum CastToBigNum(const valtype& vch)
{
    if (vch.size() > nMaxNumSize)
        throw runtime_error("CastToBigNum() : overflow");
    // Get rid of extra leading zeros
    return CBigNum(CBigNum(vch).getvch());
}

boost::tribool TransactionEvaluator::eval(opcodetype opcode) {
    boost::tribool result = Evaluator::eval(opcode);
    if (result || !result)
        return result;
    
    if (needContext())
        std::runtime_error("Trying to use TransactionEvaluator without a Transaction context!");
    
    const Value False(0);
    const Value True(1, 1);
    
    switch (opcode) {
        case OP_CODESEPARATOR: {
            // Hash starts after the code separator
            _codehash = _current;
            _code_separator = true;
            break;
        }
        case OP_CHECKSIG:
        case OP_CHECKSIGVERIFY: {
            // (sig pubkey -- bool)
            if (_stack.size() < 2)
                return false;
            
            Value& vchSig    = top(-2);
            Value& vchPubKey = top(-1);
            
            ////// debug print
            //PrintHex(vchSig.begin(), vchSig.end(), "sig: %s\n");
            //PrintHex(vchPubKey.begin(), vchPubKey.end(), "pubkey: %s\n");
            
            // Subset of script starting at the most recent codeseparator
            if (!_code_separator)
                _codehash = _begin;
            Script scriptCode(_codehash, _end);
            
            // Drop the signature, since there's no way for a signature to sign itself
            scriptCode.findAndDelete(Script(vchSig));
            
            bool fSuccess = checkSig(vchSig, vchPubKey, scriptCode);
            
            pop(_stack);
            pop(_stack);
            _stack.push_back(fSuccess ? True : False);
            if (opcode == OP_CHECKSIGVERIFY) {
                if (fSuccess)
                    pop(_stack);
                else
                    return false;
            }
            break;
        }
            
        case OP_CHECKMULTISIG:
        case OP_CHECKMULTISIGVERIFY: {
            // ([sig ...] num_of_signatures [pubkey ...] num_of_pubkeys -- bool)
            
            unsigned int i = 1;
            if (_stack.size() < i)
                return false;
            
            int nKeysCount = CastToBigNum(top(-i)).getint();
            if (nKeysCount < 0 || nKeysCount > 20)
                return false;
            _op_count += nKeysCount;
            if (_op_count > 201)
                return false;
            int ikey = ++i;
            i += nKeysCount;
            if (_stack.size() < i)
                return false;
            
            int nSigsCount = CastToBigNum(top(-i)).getint();
            if (nSigsCount < 0 || nSigsCount > nKeysCount)
                return false;
            int isig = ++i;
            i += nSigsCount;
            if (_stack.size() < i)
                return false;
            
            // Subset of script starting at the most recent codeseparator
            if (!_code_separator)
                _codehash = _begin;
            Script scriptCode(_codehash, _end);
            
            // Drop the signatures, since there's no way for a signature to sign itself
            for (int k = 0; k < nSigsCount; k++) {
                Value& vchSig = top(-isig-k);
                scriptCode.findAndDelete(Script(vchSig));
            }
            
            bool fSuccess = true;
            while (fSuccess && nSigsCount > 0) {
                Value& vchSig    = top(-isig);
                Value& vchPubKey = top(-ikey);
                
                // Check signature
                if (checkSig(vchSig, vchPubKey, scriptCode)) {
                    isig++;
                    nSigsCount--;
                }
                ikey++;
                nKeysCount--;
                
                // If there are more signatures left than keys left,
                // then too many signatures have failed
                if (nSigsCount > nKeysCount)
                    fSuccess = false;
            }
            
            while (i-- > 0)
                pop(_stack);
            _stack.push_back(fSuccess ? True : False);
            
            if (opcode == OP_CHECKMULTISIGVERIFY) {
                if (fSuccess)
                    pop(_stack);
                else
                    return false;
            }
            break;
        }
        default:
            break;
    }
    return indeterminate;
}

bool TransactionEvaluator::checkSig(std::vector<unsigned char> vchSig, std::vector<unsigned char> vchPubKey, Script scriptCode)
{
    CKey key;
    int hashtype = _hash_type;

    if (!key.SetPubKey(vchPubKey))
        return false;
    
    // Hash type is one byte tacked on to the end of the signature
    if (vchSig.empty())
        return false;
    if (hashtype == 0)
        hashtype = vchSig.back();
    else if (hashtype != vchSig.back())
        return false;
    vchSig.pop_back();
    
    return key.Verify(_txn.getSignatureHash(scriptCode, _in, hashtype), vchSig);
}

