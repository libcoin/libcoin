// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
//#include "headers.h>

#include <vector>
#include <boost/foreach.hpp>

#include <coin/BigNum.h>
#include <coin/Transaction.h>
#include <coin/Logger.h>

using namespace std;
using namespace boost;

//bool CheckSig(vector<unsigned char> vchSig, vector<unsigned char> vchPubKey, Script scriptCode, const Transaction& txTo, unsigned int nIn, int nHashType);



static const valtype vchFalse(0);
static const valtype vchZero(0);
static const valtype vchTrue(1, 1);
static const CBigNum bnZero(0);
static const CBigNum bnOne(1);
static const CBigNum bnFalse(0);
static const CBigNum bnTrue(1);
static const size_t nMaxNumSize = 4;


CBigNum CastToBigNum(const valtype& vch)
{
    if (vch.size() > nMaxNumSize)
        throw runtime_error("CastToBigNum() : overflow");
    // Get rid of extra leading zeros
    return CBigNum(CBigNum(vch).getvch());
}

bool CastToBool(const valtype& vch)
{
    for (int i = 0; i < vch.size(); i++)
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

void MakeSameSize(valtype& vch1, valtype& vch2)
{
    // Lengthen the shorter one
    if (vch1.size() < vch2.size())
        vch1.resize(vch2.size(), 0);
    if (vch2.size() < vch1.size())
        vch2.resize(vch1.size(), 0);
}

bool Script::isPayToScriptHash() const
{
    // Extra-fast test for pay-to-script-hash CScripts:
    return (this->size() == 23 &&
            this->at(0) == OP_HASH160 &&
            this->at(1) == 0x14 &&
            this->at(22) == OP_EQUAL);
}


bool Evaluator::operator()(const Script& script) {
    _begin = script.begin();
    _current = script.begin();
    _end = script.end();
    _op_count = 0;
    
    opcodetype opcode;
    Value value;
    if (script.size() > 10000)
        return false;
    //    int nOpCount = 0;
    
    try {
        while (_current < _end) {
            _exec = !count(_exec_stack.begin(), _exec_stack.end(), false);
            
            // Read instruction
            if (!script.getOp(_current, opcode, value))
                return false;
            if (value.size() > 520)
                return false;
            if (opcode > OP_16 && ++_op_count > 201)
                return false;
            
            if (opcode == OP_CAT ||
                opcode == OP_SUBSTR ||
                opcode == OP_LEFT ||
                opcode == OP_RIGHT ||
                opcode == OP_INVERT ||
                opcode == OP_AND ||
                opcode == OP_OR ||
                opcode == OP_XOR ||
                opcode == OP_2MUL ||
                opcode == OP_2DIV ||
                opcode == OP_MUL ||
                opcode == OP_DIV ||
                opcode == OP_MOD ||
                opcode == OP_LSHIFT ||
                opcode == OP_RSHIFT)
                return false;
            
            if (_exec && 0 <= opcode && opcode <= OP_PUSHDATA4)
                _stack.push_back(value);
            else if (_exec || (OP_IF <= opcode && opcode <= OP_ENDIF)) {
                boost::tribool result = eval(opcode);
                if (result || !result)
                    return result;
            }
            // Size limits
            if (_stack.size() + _alt_stack.size() > 1000)
                return false;
        }
    }
    catch (...) {
        return false;
    }
    
    if (!_exec_stack.empty())
        return false;
    
    return true;
}
/*
bool Evaluator::operator()(const Script& script, const Script& tmpl) {
    _begin = script.begin();
    _current = script.begin();
    _end = script.end();
    Script::const_iterator current = tmpl.begin();

    // step through the script and template -
}
*/
boost::tribool Evaluator::eval(opcodetype opcode) {
    switch (opcode) {
            // Push value
        case OP_1NEGATE:
        case OP_1:
        case OP_2:
        case OP_3:
        case OP_4:
        case OP_5:
        case OP_6:
        case OP_7:
        case OP_8:
        case OP_9:
        case OP_10:
        case OP_11:
        case OP_12:
        case OP_13:
        case OP_14:
        case OP_15:
        case OP_16: {
            // ( -- value)
            CBigNum bn((int)opcode - (int)(OP_1 - 1));
            _stack.push_back(bn.getvch());
            break;
        }
            
            // Control
        case OP_NOP:
        case OP_NOP1: case OP_NOP2: case OP_NOP3: case OP_NOP4: case OP_NOP5:
        case OP_NOP6: case OP_NOP7: case OP_NOP8: case OP_NOP9: case OP_NOP10:
            break;
            
        case OP_IF:
        case OP_NOTIF: {
            // <expression> if [statements] [else [statements]] endif
            bool fValue = false;
            if (_exec) {
                if (_stack.size() < 1)
                    return false;
                Value& vch = top(-1);
                fValue = CastToBool(vch);
                if (opcode == OP_NOTIF)
                    fValue = !fValue;
                pop(_stack);
            }
            _exec_stack.push_back(fValue);
            break;
        }
            
        case OP_ELSE: {
            if (_exec_stack.empty())
                return false;
            _exec_stack.back() = !_exec_stack.back();
            break;
        }
            
        case OP_ENDIF: {
            if (_exec_stack.empty())
                return false;
            _exec_stack.pop_back();
            break;
        }
            
        case OP_VERIFY: {
            // (true -- ) or
            // (false -- false) and return
            if (_stack.size() < 1)
                return false;
            bool fValue = CastToBool(top(-1));
            if (fValue)
                pop(_stack);
            else
                return false;
            break;
        }
            
        case OP_RETURN: {
            return false;
            break;
        }
            
            
            // Stack ops
        case OP_TOALTSTACK: {
            if (_stack.size() < 1)
                return false;
            _alt_stack.push_back(top(-1));
            pop(_stack);
            break;
        }
            
        case OP_FROMALTSTACK: {
            if (_alt_stack.size() < 1)
                return false;
            _stack.push_back(alttop(-1));
            pop(_alt_stack);
            break;
        }
            
        case OP_2DROP: {
            // (x1 x2 -- )
            if (_stack.size() < 2)
                return false;
            pop(_stack);
            pop(_stack);
            break;
        }
            
        case OP_2DUP: {
            // (x1 x2 -- x1 x2 x1 x2)
            if (_stack.size() < 2)
                return false;
            Value vch1 = top(-2);
            Value vch2 = top(-1);
            _stack.push_back(vch1);
            _stack.push_back(vch2);
            break;
        }
            
        case OP_3DUP: {
            // (x1 x2 x3 -- x1 x2 x3 x1 x2 x3)
            if (_stack.size() < 3)
                return false;
            Value vch1 = top(-3);
            Value vch2 = top(-2);
            Value vch3 = top(-1);
            _stack.push_back(vch1);
            _stack.push_back(vch2);
            _stack.push_back(vch3);
            break;
        }
            
        case OP_2OVER: {
            // (x1 x2 x3 x4 -- x1 x2 x3 x4 x1 x2)
            if (_stack.size() < 4)
                return false;
            Value vch1 = top(-4);
            Value vch2 = top(-3);
            _stack.push_back(vch1);
            _stack.push_back(vch2);
            break;
        }
            
        case OP_2ROT: {
            // (x1 x2 x3 x4 x5 x6 -- x3 x4 x5 x6 x1 x2)
            if (_stack.size() < 6)
                return false;
            Value vch1 = top(-6);
            Value vch2 = top(-5);
            _stack.erase(_stack.end()-6, _stack.end()-4);
            _stack.push_back(vch1);
            _stack.push_back(vch2);
            break;
        }
            
        case OP_2SWAP: {
            // (x1 x2 x3 x4 -- x3 x4 x1 x2)
            if (_stack.size() < 4)
                return false;
            swap(top(-4), top(-2));
            swap(top(-3), top(-1));
            break;
        }
            
        case OP_IFDUP: {
            // (x - 0 | x x)
            if (_stack.size() < 1)
                return false;
            Value vch = top(-1);
            if (CastToBool(vch))
                _stack.push_back(vch);
            break;
        }
            
        case OP_DEPTH: {
            // -- stacksize
            CBigNum bn((uint64_t)_stack.size());
            _stack.push_back(bn.getvch());
            break;
        }
            
        case OP_DROP: {
            // (x -- )
            if (_stack.size() < 1)
                return false;
            pop(_stack);
            break;
        }
            
        case OP_DUP: {
            // (x -- x x)
            if (_stack.size() < 1)
                return false;
            Value vch = top(-1);
            _stack.push_back(vch);
            break;
        }
            
        case OP_NIP: {
            // (x1 x2 -- x2)
            if (_stack.size() < 2)
                return false;
            _stack.erase(_stack.end() - 2);
            break;
        }
            
        case OP_OVER: {
            // (x1 x2 -- x1 x2 x1)
            if (_stack.size() < 2)
                return false;
            Value vch = top(-2);
            _stack.push_back(vch);
            break;
        }
            
        case OP_PICK:
        case OP_ROLL: {
            // (xn ... x2 x1 x0 n - xn ... x2 x1 x0 xn)
            // (xn ... x2 x1 x0 n - ... x2 x1 x0 xn)
            if (_stack.size() < 2)
                return false;
            int n = CastToBigNum(top(-1)).getint();
            pop(_stack);
            if (n < 0 || n >= _stack.size())
                return false;
            Value vch = top(-n-1);
            if (opcode == OP_ROLL)
                _stack.erase(_stack.end()-n-1);
            _stack.push_back(vch);
            break;
        }
            
        case OP_ROT: {
            // (x1 x2 x3 -- x2 x3 x1)
            //  x2 x1 x3  after first swap
            //  x2 x3 x1  after second swap
            if (_stack.size() < 3)
                return false;
            swap(top(-3), top(-2));
            swap(top(-2), top(-1));
            break;
        }
            
        case OP_SWAP: {
            // (x1 x2 -- x2 x1)
            if (_stack.size() < 2)
                return false;
            swap(top(-2), top(-1));
            break;
        }
            
        case OP_TUCK: {
            // (x1 x2 -- x2 x1 x2)
            if (_stack.size() < 2)
                return false;
            Value vch = top(-1);
            _stack.insert(_stack.end()-2, vch);
            break;
        }
            
            
            //
            // Splice ops
            //
        case OP_CAT: {
            // (x1 x2 -- out)
            if (_stack.size() < 2)
                return false;
            Value& vch1 = top(-2);
            Value& vch2 = top(-1);
            vch1.insert(vch1.end(), vch2.begin(), vch2.end());
            pop(_stack);
            if (top(-1).size() > 520)
                return false;
            break;
        }
            
        case OP_SUBSTR: {
            // (in begin size -- out)
            if (_stack.size() < 3)
                return false;
            Value& vch = top(-3);
            int nBegin = CastToBigNum(top(-2)).getint();
            int nEnd = nBegin + CastToBigNum(top(-1)).getint();
            if (nBegin < 0 || nEnd < nBegin)
                return false;
            if (nBegin > vch.size())
                nBegin = vch.size();
            if (nEnd > vch.size())
                nEnd = vch.size();
            vch.erase(vch.begin() + nEnd, vch.end());
            vch.erase(vch.begin(), vch.begin() + nBegin);
            pop(_stack);
            pop(_stack);
            break;
        }
            
        case OP_LEFT:
        case OP_RIGHT: {
            // (in size -- out)
            if (_stack.size() < 2)
                return false;
            Value& vch = top(-2);
            int nSize = CastToBigNum(top(-1)).getint();
            if (nSize < 0)
                return false;
            if (nSize > vch.size())
                nSize = vch.size();
            if (opcode == OP_LEFT)
                vch.erase(vch.begin() + nSize, vch.end());
            else
                vch.erase(vch.begin(), vch.end() - nSize);
            pop(_stack);
            break;
        }
            
        case OP_SIZE: {
            // (in -- in size)
            if (_stack.size() < 1)
                return false;
            CBigNum bn((uint64_t)top(-1).size());
            _stack.push_back(bn.getvch());
            break;
        }
            
            
            // Bitwise logic
        case OP_INVERT: {
            // (in - out)
            if (_stack.size() < 1)
                return false;
            Value& vch = top(-1);
            for (int i = 0; i < vch.size(); i++)
                vch[i] = ~vch[i];
            break;
        }
            
        case OP_AND:
        case OP_OR:
        case OP_XOR: {
            // (x1 x2 - out)
            if (_stack.size() < 2)
                return false;
            Value& vch1 = top(-2);
            Value& vch2 = top(-1);
            MakeSameSize(vch1, vch2);
            if (opcode == OP_AND) {
                for (int i = 0; i < vch1.size(); i++)
                    vch1[i] &= vch2[i];
            }
            else if (opcode == OP_OR) {
                for (int i = 0; i < vch1.size(); i++)
                    vch1[i] |= vch2[i];
            }
            else if (opcode == OP_XOR) {
                for (int i = 0; i < vch1.size(); i++)
                    vch1[i] ^= vch2[i];
            }
            pop(_stack);
            break;
        }
            
        case OP_EQUAL:
            //case OP_NOTEQUAL: // use OP_NUMNOTEQUAL
        case OP_EQUALVERIFY: {
            // (x1 x2 - bool)
            if (_stack.size() < 2)
                return false;
            Value& vch1 = top(-2);
            Value& vch2 = top(-1);
            bool fEqual = (vch1 == vch2);
            // OP_NOTEQUAL is disabled because it would be too easy to say
            // something like n != 1 and have some wiseguy pass in 1 with extra
            // zero bytes after it (numerically, 0x01 == 0x0001 == 0x000001)
            //if (opcode == OP_NOTEQUAL)
            //    fEqual = !fEqual;
            pop(_stack);
            pop(_stack);
            _stack.push_back(fEqual ? vchTrue : vchFalse);
            if (opcode == OP_EQUALVERIFY) {
                if (fEqual)
                    pop(_stack);
                else
                    return false;
            }
            break;
        }
            
            
            // Numeric
        case OP_1ADD:
        case OP_1SUB:
        case OP_2MUL:
        case OP_2DIV:
        case OP_NEGATE:
        case OP_ABS:
        case OP_NOT:
        case OP_0NOTEQUAL: {
            // (in -- out)
            if (_stack.size() < 1)
                return false;
            CBigNum bn = CastToBigNum(top(-1));
            switch (opcode) {
                case OP_1ADD:       bn += bnOne; break;
                case OP_1SUB:       bn -= bnOne; break;
                case OP_2MUL:       bn <<= 1; break;
                case OP_2DIV:       bn >>= 1; break;
                case OP_NEGATE:     bn = -bn; break;
                case OP_ABS:        if (bn < bnZero) bn = -bn; break;
                case OP_NOT:        bn = (bn == bnZero); break;
                case OP_0NOTEQUAL:  bn = (bn != bnZero); break;
                default:            assert(!"invalid opcode"); break;
            }
            pop(_stack);
            _stack.push_back(bn.getvch());
            break;
        }
            
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_MOD:
        case OP_LSHIFT:
        case OP_RSHIFT:
        case OP_BOOLAND:
        case OP_BOOLOR:
        case OP_NUMEQUAL:
        case OP_NUMEQUALVERIFY:
        case OP_NUMNOTEQUAL:
        case OP_LESSTHAN:
        case OP_GREATERTHAN:
        case OP_LESSTHANOREQUAL:
        case OP_GREATERTHANOREQUAL:
        case OP_MIN:
        case OP_MAX: {
            // (x1 x2 -- out)
            CAutoBN_CTX pctx;
            if (_stack.size() < 2)
                return false;
            CBigNum bn1 = CastToBigNum(top(-2));
            CBigNum bn2 = CastToBigNum(top(-1));
            CBigNum bn;
            switch (opcode) {
                case OP_ADD:
                    bn = bn1 + bn2;
                    break;
                    
                case OP_SUB:
                    bn = bn1 - bn2;
                    break;
                    
                case OP_MUL:
                    if (!BN_mul(&bn, &bn1, &bn2, pctx))
                        return false;
                    break;
                    
                case OP_DIV:
                    if (!BN_div(&bn, NULL, &bn1, &bn2, pctx))
                        return false;
                    break;
                    
                case OP_MOD:
                    if (!BN_mod(&bn, &bn1, &bn2, pctx))
                        return false;
                    break;
                    
                case OP_LSHIFT:
                    if (bn2 < bnZero || bn2 > CBigNum(2048))
                        return false;
                    bn = bn1 << bn2.getulong();
                    break;
                    
                case OP_RSHIFT:
                    if (bn2 < bnZero || bn2 > CBigNum(2048))
                        return false;
                    bn = bn1 >> bn2.getulong();
                    break;
                    
                case OP_BOOLAND:             bn = (bn1 != bnZero && bn2 != bnZero); break;
                case OP_BOOLOR:              bn = (bn1 != bnZero || bn2 != bnZero); break;
                case OP_NUMEQUAL:            bn = (bn1 == bn2); break;
                case OP_NUMEQUALVERIFY:      bn = (bn1 == bn2); break;
                case OP_NUMNOTEQUAL:         bn = (bn1 != bn2); break;
                case OP_LESSTHAN:            bn = (bn1 < bn2); break;
                case OP_GREATERTHAN:         bn = (bn1 > bn2); break;
                case OP_LESSTHANOREQUAL:     bn = (bn1 <= bn2); break;
                case OP_GREATERTHANOREQUAL:  bn = (bn1 >= bn2); break;
                case OP_MIN:                 bn = (bn1 < bn2 ? bn1 : bn2); break;
                case OP_MAX:                 bn = (bn1 > bn2 ? bn1 : bn2); break;
                default:                     assert(!"invalid opcode"); break;
            }
            pop(_stack);
            pop(_stack);
            _stack.push_back(bn.getvch());
            
            if (opcode == OP_NUMEQUALVERIFY) {
                if (CastToBool(top(-1)))
                    pop(_stack);
                else
                    return false;
            }
            break;
        }
            
        case OP_WITHIN: {
            // (x min max -- out)
            if (_stack.size() < 3)
                return false;
            CBigNum bn1 = CastToBigNum(top(-3));
            CBigNum bn2 = CastToBigNum(top(-2));
            CBigNum bn3 = CastToBigNum(top(-1));
            bool fValue = (bn2 <= bn1 && bn1 < bn3);
            pop(_stack);
            pop(_stack);
            pop(_stack);
            _stack.push_back(fValue ? vchTrue : vchFalse);
            break;
        }
            
            
            // Crypto
        case OP_RIPEMD160:
        case OP_SHA1:
        case OP_SHA256:
        case OP_HASH160:
        case OP_HASH256: {
            // (in -- hash)
            if (_stack.size() < 1)
                return false;
            Value& vch = top(-1);
            Value vchHash((opcode == OP_RIPEMD160 || opcode == OP_SHA1 || opcode == OP_HASH160) ? 20 : 32);
            if (opcode == OP_RIPEMD160)
                RIPEMD160(&vch[0], vch.size(), &vchHash[0]);
            else if (opcode == OP_SHA1)
                SHA1(&vch[0], vch.size(), &vchHash[0]);
            else if (opcode == OP_SHA256)
                SHA256(&vch[0], vch.size(), &vchHash[0]);
            else if (opcode == OP_HASH160) {
                uint160 hash160 = toPubKeyHash(vch);
                memcpy(&vchHash[0], &hash160, sizeof(hash160));
            }
            else if (opcode == OP_HASH256) {
                uint256 hash = Hash(vch.begin(), vch.end());
                memcpy(&vchHash[0], &hash, sizeof(hash));
            }
            pop(_stack);
            _stack.push_back(vchHash);
            break;
        }
            
        default:
            break;
    }
    return indeterminate;
}


bool Solver(const Script& scriptPubKey, vector<pair<opcodetype, valtype> >& vSolutionRet)
{
    // Templates
    static vector<Script> vTemplates;
    if (vTemplates.empty())
    {
        // Standard tx, sender provides pubkey, receiver adds signature
        vTemplates.push_back(Script() << OP_PUBKEY << OP_CHECKSIG);

        // Bitcoin address tx, sender provides hash of pubkey, receiver provides signature and pubkey
        vTemplates.push_back(Script() << OP_DUP << OP_HASH160 << OP_PUBKEYHASH << OP_EQUALVERIFY << OP_CHECKSIG);
    }

    // Scan templates
    const Script& script1 = scriptPubKey;
    BOOST_FOREACH(const Script& script2, vTemplates)
    {
        vSolutionRet.clear();
        opcodetype opcode1, opcode2;
        vector<unsigned char> vch1, vch2;

        // Compare
        Script::const_iterator pc1 = script1.begin();
        Script::const_iterator pc2 = script2.begin();
        loop
        {
            if (pc1 == script1.end() && pc2 == script2.end())
            {
                // Found a match
                reverse(vSolutionRet.begin(), vSolutionRet.end());
                return true;
            }
            if (!script1.getOp(pc1, opcode1, vch1))
                break;
            if (!script2.getOp(pc2, opcode2, vch2))
                break;
            if (opcode2 == OP_PUBKEY)
            {
                if (vch1.size() < 33 || vch1.size() > 120)
                    break;
                vSolutionRet.push_back(make_pair(opcode2, vch1));
            }
            else if (opcode2 == OP_PUBKEYHASH)
            {
                if (vch1.size() != sizeof(uint160))
                    break;
                vSolutionRet.push_back(make_pair(opcode2, vch1));
            }
            else if (opcode1 != opcode2 || vch1 != vch2)
            {
                break;
            }
        }
    }

    vSolutionRet.clear();
    return false;
}


bool Solver(const KeyStore& keystore, const Script& scriptPubKey, uint256 hash, int nHashType, Script& scriptSigRet)
{
    scriptSigRet.clear();

    vector<pair<opcodetype, valtype> > vSolution;
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

int ScriptSigArgsExpected(txnouttype t, const std::vector<std::vector<unsigned char> >& vSolutions)
{
    switch (t) {
        case TX_NONSTANDARD:
            return -1;
        case TX_PUBKEY:
            return 1;
        case TX_PUBKEYHASH:
            return 2;
        case TX_MULTISIG:
            if (vSolutions.size() < 1 || vSolutions[0].size() < 1)
                return -1;
            return vSolutions[0][0] + 1;
        case TX_SCRIPTHASH:
            return 1; // doesn't include args needed by the script
    }
    return -1;
}

int HaveKeys(const vector<valtype>& pubkeys, const KeyStore& keystore)
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
    vector<valtype> vSolutions;
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
            vector<valtype> keys(vSolutions.begin()+1, vSolutions.begin()+vSolutions.size()-1);
            return HaveKeys(keys, keystore) == keys.size();
        }
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
        for (int i = 1; i < vSolutions.size()-1; i++) {
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
