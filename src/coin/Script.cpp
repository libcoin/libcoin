// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
//#include "headers.h>

#include <vector>
#include <boost/foreach.hpp>

#include <coin/BigNum.h>
#include <coin/NameOperation.h>
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

void MakeSameSize(valtype& vch1, valtype& vch2)
{
    // Lengthen the shorter one
    if (vch1.size() < vch2.size())
        vch1.resize(vch2.size(), 0);
    if (vch2.size() < vch1.size())
        vch2.resize(vch1.size(), 0);
}

bool Script::skipNamePrefix (const_iterator& pc) const
{
    opcodetype opcode;
    if (!getOp(pc, opcode))
        return false;
    if (opcode < OP_1 || opcode > OP_16)
        return false;

    int op = opcode - OP_1 + 1;

    unsigned argCnt = 0;
    for (;;) {
        vector<unsigned char> vch;
        if (!getOp(pc, opcode, vch))
            return false;
        if (opcode == OP_DROP || opcode == OP_2DROP || opcode == OP_NOP)
            break;
        if (!(opcode >= 0 && opcode <= OP_PUSHDATA4))
            return false;
        ++argCnt;
    }

    // move the pc to after any DROP or NOP
    while (opcode == OP_DROP || opcode == OP_2DROP || opcode == OP_NOP)
    {
        if (!getOp(pc, opcode))
            break;
    }

    pc--;

    if ((op == NameOperation::OP_NAME_NEW && argCnt == 1) ||
            (op == NameOperation::OP_NAME_FIRSTUPDATE && argCnt == 3) ||
            (op == NameOperation::OP_NAME_UPDATE && argCnt == 2))
        return true;
    return false;
}

bool Script::isPayToScriptHash() const
{
    // Extra-fast test for pay-to-script-hash CScripts:
    return (this->size() == 23 &&
            this->at(0) == OP_HASH160 &&
            this->at(1) == 0x14 &&
            this->at(22) == OP_EQUAL);
}

Script Script::getDropped() const {
    DropEvaluator eval;
    eval(*this);
    return eval.dropped();
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
            if (value.size() > MAX_SCRIPT_ELEMENT_SIZE)
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
            if (n < 0 || (unsigned int)n >= _stack.size())
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
            if (top(-1).size() > MAX_SCRIPT_ELEMENT_SIZE)
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
            if ((unsigned int)nBegin > vch.size())
                nBegin = vch.size();
            if ((unsigned int)nEnd > vch.size())
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
            if ((unsigned int)nSize > vch.size())
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
            for (unsigned int i = 0; i < vch.size(); i++)
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
                for (unsigned int i = 0; i < vch1.size(); i++)
                    vch1[i] &= vch2[i];
            }
            else if (opcode == OP_OR) {
                for (unsigned int i = 0; i < vch1.size(); i++)
                    vch1[i] |= vch2[i];
            }
            else if (opcode == OP_XOR) {
                for (unsigned int i = 0; i < vch1.size(); i++)
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


// Canonical tests

void checkCanonicalPubKey(const valtype &vchPubKey) {
    if (vchPubKey.size() < 33)
        throw runtime_error("Non-canonical public key: too short");
    if (vchPubKey[0] == 0x04) {
        if (vchPubKey.size() != 65)
            throw runtime_error("Non-canonical public key: invalid length for uncompressed key");
    } else if (vchPubKey[0] == 0x02 || vchPubKey[0] == 0x03) {
        if (vchPubKey.size() != 33)
            throw runtime_error("Non-canonical public key: invalid length for compressed key");
    } else {
        throw runtime_error("Non-canonical public key: compressed nor uncompressed");
    }
}

void checkCanonicalSignature(const valtype &vchSig) {
    // See https://bitcointalk.org/index.php?topic=8392.msg127623#msg127623
    // A canonical signature exists of: <30> <total len> <02> <len R> <R> <02> <len S> <S> <hashtype>
    // Where R and S are not negative (their first byte has its highest bit not set), and not
    // excessively padded (do not start with a 0 byte, unless an otherwise negative number follows,
    // in which case a single 0 byte is necessary and even required).
    if (vchSig.size() < 9)
        throw runtime_error("Non-canonical signature: too short");
    if (vchSig.size() > 73)
        throw runtime_error("Non-canonical signature: too long");
    unsigned char nHashType = vchSig[vchSig.size() - 1] & (~(SIGHASH_ANYONECANPAY));
    if (nHashType < SIGHASH_ALL || nHashType > SIGHASH_SINGLE)
        throw runtime_error("Non-canonical signature: unknown hashtype byte");
    if (vchSig[0] != 0x30)
        throw runtime_error("Non-canonical signature: wrong type");
    if (vchSig[1] != vchSig.size()-3)
        throw runtime_error("Non-canonical signature: wrong length marker");
    unsigned int nLenR = vchSig[3];
    if (5 + nLenR >= vchSig.size())
        throw runtime_error("Non-canonical signature: S length misplaced");
    unsigned int nLenS = vchSig[5+nLenR];
    if ((unsigned long)(nLenR+nLenS+7) != vchSig.size())
        throw runtime_error("Non-canonical signature: R+S length mismatch");
    
    const unsigned char *R = &vchSig[4];
    if (R[-2] != 0x02)
        throw runtime_error("Non-canonical signature: R value type mismatch");
    if (nLenR == 0)
        throw runtime_error("Non-canonical signature: R length is zero");
    if (R[0] & 0x80)
        throw runtime_error("Non-canonical signature: R value negative");
    if (nLenR > 1 && (R[0] == 0x00) && !(R[1] & 0x80))
        throw runtime_error("Non-canonical signature: R value excessively padded");
    
    const unsigned char *S = &vchSig[6+nLenR];
    if (S[-2] != 0x02)
        throw runtime_error("Non-canonical signature: S value type mismatch");
    if (nLenS == 0)
        throw runtime_error("Non-canonical signature: S length is zero");
    if (S[0] & 0x80)
        throw runtime_error("Non-canonical signature: S value negative");
    if (nLenS > 1 && (S[0] == 0x00) && !(S[1] & 0x80))
        throw runtime_error("Non-canonical signature: S value excessively padded");
}

