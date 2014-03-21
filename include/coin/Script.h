// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
#ifndef H_BITCOIN_SCRIPT
#define H_BITCOIN_SCRIPT

#include <coin/Export.h>
#include <coin/util.h>
#include <coin/BigNum.h>
#include <coin/uint256.h>
#include <coin/Serialization.h>

//#include <coin/Address.h>
//#include <coin/KeyStore.h>

#include <string>
#include <vector>

#include <boost/foreach.hpp>
#include <boost/logic/tribool.hpp>

class Transaction;
class Output;

enum
{
    SIGHASH_ALL = 1,
    SIGHASH_NONE = 2,
    SIGHASH_SINGLE = 3,
    SIGHASH_ANYONECANPAY = 0x80
};

enum txnouttype
{
    TX_NONSTANDARD,
    // 'standard' transaction types:
    TX_PUBKEY,
    TX_PUBKEYHASH,
    TX_SCRIPTHASH,
    TX_MULTISIG
};


enum opcodetype
{
    // push value
    OP_0=0,
    OP_FALSE=OP_0,

    OP_PUSHDATA1=76,
    OP_PUSHDATA2,
    OP_PUSHDATA4,
    OP_1NEGATE,
    OP_RESERVED,
    OP_1,
    OP_TRUE=OP_1,
    OP_2,
    OP_3,
    OP_4,
    OP_5,
    OP_6,
    OP_7,
    OP_8,
    OP_9,
    OP_10,
    OP_11,
    OP_12,
    OP_13,
    OP_14,
    OP_15,
    OP_16,
    
    // control
    OP_NOP,
    OP_VER,
    OP_IF,
    OP_NOTIF,
    OP_VERIF,
    OP_VERNOTIF,
    OP_ELSE,
    OP_ENDIF,
    OP_VERIFY,
    OP_RETURN,
    
    // stack ops
    OP_TOALTSTACK,
    OP_FROMALTSTACK,
    OP_2DROP,
    OP_2DUP,
    OP_3DUP,
    OP_2OVER,
    OP_2ROT,
    OP_2SWAP,
    OP_IFDUP,
    OP_DEPTH,
    OP_DROP,
    OP_DUP,
    OP_NIP,
    OP_OVER,
    OP_PICK,
    OP_ROLL,
    OP_ROT,
    OP_SWAP,
    OP_TUCK,
    
    // splice ops
    OP_CAT,
    OP_SUBSTR,
    OP_LEFT,
    OP_RIGHT,
    OP_SIZE,
    
    // bit logic
    OP_INVERT,
    OP_AND,
    OP_OR,
    OP_XOR,
    OP_EQUAL,
    OP_EQUALVERIFY,
    OP_RESERVED1,
    OP_RESERVED2,
    
    // numeric
    OP_1ADD,
    OP_1SUB,
    OP_2MUL,
    OP_2DIV,
    OP_NEGATE,
    OP_ABS,
    OP_NOT,
    OP_0NOTEQUAL,
    
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_LSHIFT,
    OP_RSHIFT,
    
    OP_BOOLAND,
    OP_BOOLOR,
    OP_NUMEQUAL,
    OP_NUMEQUALVERIFY,
    OP_NUMNOTEQUAL,
    OP_LESSTHAN,
    OP_GREATERTHAN,
    OP_LESSTHANOREQUAL,
    OP_GREATERTHANOREQUAL,
    OP_MIN,
    OP_MAX,
    
    OP_WITHIN,
    
    // crypto
    OP_RIPEMD160,
    OP_SHA1,
    OP_SHA256,
    OP_HASH160,
    OP_HASH256,
    OP_CODESEPARATOR,
    OP_CHECKSIG,
    OP_CHECKSIGVERIFY,
    OP_CHECKMULTISIG,
    OP_CHECKMULTISIGVERIFY,
    
    // expansion
    OP_NOP1,
    OP_NOP2,
    OP_NOP3,
    OP_NOP4,
    OP_NOP5,
    OP_NOP6,
    OP_NOP7,
    OP_NOP8,
    OP_NOP9,
    OP_NOP10,
    
    // this is for handling hieraical wallets, as well as partly signed transactions
    OP_SIGHASH = 0xea, // this will generate a signature hash from 
    OP_RESOLVE = 0xeb,
    OP_SIGN = 0xec,
    OP_RESOLVEANDSIGN = 0xed,
    //    OP_VALUE = 0xee, // used for template matching - deprecating the stuff below
    
    // template matching params
    OP_SMALLINTEGER = 0xfa,
    OP_PUBKEYS = 0xfb,
    OP_PUBKEYHASH = 0xfd,
    OP_SCRIPTHASH = 0xfd,
    OP_PUBKEY = 0xfe,
    
    OP_INVALIDOPCODE = 0xff
};

typedef std::vector<unsigned char> valtype;

static const size_t MAX_SCRIPT_ELEMENT_SIZE = 520;


inline const char* GetOpName(opcodetype opcode)
{
    switch (opcode) {
            // push value
        case OP_0                      : return "0";
        case OP_PUSHDATA1              : return "OP_PUSHDATA1";
        case OP_PUSHDATA2              : return "OP_PUSHDATA2";
        case OP_PUSHDATA4              : return "OP_PUSHDATA4";
        case OP_1NEGATE                : return "-1";
        case OP_RESERVED               : return "OP_RESERVED";
        case OP_1                      : return "1";
        case OP_2                      : return "2";
        case OP_3                      : return "3";
        case OP_4                      : return "4";
        case OP_5                      : return "5";
        case OP_6                      : return "6";
        case OP_7                      : return "7";
        case OP_8                      : return "8";
        case OP_9                      : return "9";
        case OP_10                     : return "10";
        case OP_11                     : return "11";
        case OP_12                     : return "12";
        case OP_13                     : return "13";
        case OP_14                     : return "14";
        case OP_15                     : return "15";
        case OP_16                     : return "16";
            
            // control
        case OP_NOP                    : return "OP_NOP";
        case OP_VER                    : return "OP_VER";
        case OP_IF                     : return "OP_IF";
        case OP_NOTIF                  : return "OP_NOTIF";
        case OP_VERIF                  : return "OP_VERIF";
        case OP_VERNOTIF               : return "OP_VERNOTIF";
        case OP_ELSE                   : return "OP_ELSE";
        case OP_ENDIF                  : return "OP_ENDIF";
        case OP_VERIFY                 : return "OP_VERIFY";
        case OP_RETURN                 : return "OP_RETURN";
            
            // stack ops
        case OP_TOALTSTACK             : return "OP_TOALTSTACK";
        case OP_FROMALTSTACK           : return "OP_FROMALTSTACK";
        case OP_2DROP                  : return "OP_2DROP";
        case OP_2DUP                   : return "OP_2DUP";
        case OP_3DUP                   : return "OP_3DUP";
        case OP_2OVER                  : return "OP_2OVER";
        case OP_2ROT                   : return "OP_2ROT";
        case OP_2SWAP                  : return "OP_2SWAP";
        case OP_IFDUP                  : return "OP_IFDUP";
        case OP_DEPTH                  : return "OP_DEPTH";
        case OP_DROP                   : return "OP_DROP";
        case OP_DUP                    : return "OP_DUP";
        case OP_NIP                    : return "OP_NIP";
        case OP_OVER                   : return "OP_OVER";
        case OP_PICK                   : return "OP_PICK";
        case OP_ROLL                   : return "OP_ROLL";
        case OP_ROT                    : return "OP_ROT";
        case OP_SWAP                   : return "OP_SWAP";
        case OP_TUCK                   : return "OP_TUCK";
            
            // splice ops
        case OP_CAT                    : return "OP_CAT";
        case OP_SUBSTR                 : return "OP_SUBSTR";
        case OP_LEFT                   : return "OP_LEFT";
        case OP_RIGHT                  : return "OP_RIGHT";
        case OP_SIZE                   : return "OP_SIZE";
            
            // bit logic
        case OP_INVERT                 : return "OP_INVERT";
        case OP_AND                    : return "OP_AND";
        case OP_OR                     : return "OP_OR";
        case OP_XOR                    : return "OP_XOR";
        case OP_EQUAL                  : return "OP_EQUAL";
        case OP_EQUALVERIFY            : return "OP_EQUALVERIFY";
        case OP_RESERVED1              : return "OP_RESERVED1";
        case OP_RESERVED2              : return "OP_RESERVED2";
            
            // numeric
        case OP_1ADD                   : return "OP_1ADD";
        case OP_1SUB                   : return "OP_1SUB";
        case OP_2MUL                   : return "OP_2MUL";
        case OP_2DIV                   : return "OP_2DIV";
        case OP_NEGATE                 : return "OP_NEGATE";
        case OP_ABS                    : return "OP_ABS";
        case OP_NOT                    : return "OP_NOT";
        case OP_0NOTEQUAL              : return "OP_0NOTEQUAL";
        case OP_ADD                    : return "OP_ADD";
        case OP_SUB                    : return "OP_SUB";
        case OP_MUL                    : return "OP_MUL";
        case OP_DIV                    : return "OP_DIV";
        case OP_MOD                    : return "OP_MOD";
        case OP_LSHIFT                 : return "OP_LSHIFT";
        case OP_RSHIFT                 : return "OP_RSHIFT";
        case OP_BOOLAND                : return "OP_BOOLAND";
        case OP_BOOLOR                 : return "OP_BOOLOR";
        case OP_NUMEQUAL               : return "OP_NUMEQUAL";
        case OP_NUMEQUALVERIFY         : return "OP_NUMEQUALVERIFY";
        case OP_NUMNOTEQUAL            : return "OP_NUMNOTEQUAL";
        case OP_LESSTHAN               : return "OP_LESSTHAN";
        case OP_GREATERTHAN            : return "OP_GREATERTHAN";
        case OP_LESSTHANOREQUAL        : return "OP_LESSTHANOREQUAL";
        case OP_GREATERTHANOREQUAL     : return "OP_GREATERTHANOREQUAL";
        case OP_MIN                    : return "OP_MIN";
        case OP_MAX                    : return "OP_MAX";
        case OP_WITHIN                 : return "OP_WITHIN";
            
            // crypto
        case OP_RIPEMD160              : return "OP_RIPEMD160";
        case OP_SHA1                   : return "OP_SHA1";
        case OP_SHA256                 : return "OP_SHA256";
        case OP_HASH160                : return "OP_HASH160";
        case OP_HASH256                : return "OP_HASH256";
        case OP_CODESEPARATOR          : return "OP_CODESEPARATOR";
        case OP_CHECKSIG               : return "OP_CHECKSIG";
        case OP_CHECKSIGVERIFY         : return "OP_CHECKSIGVERIFY";
        case OP_CHECKMULTISIG          : return "OP_CHECKMULTISIG";
        case OP_CHECKMULTISIGVERIFY    : return "OP_CHECKMULTISIGVERIFY";
            
            // expanson
        case OP_NOP1                   : return "OP_NOP1";
        case OP_NOP2                   : return "OP_NOP2";
        case OP_NOP3                   : return "OP_NOP3";
        case OP_NOP4                   : return "OP_NOP4";
        case OP_NOP5                   : return "OP_NOP5";
        case OP_NOP6                   : return "OP_NOP6";
        case OP_NOP7                   : return "OP_NOP7";
        case OP_NOP8                   : return "OP_NOP8";
        case OP_NOP9                   : return "OP_NOP9";
        case OP_NOP10                  : return "OP_NOP10";
                        
            // ExtendedKey stuff
        case OP_SIGHASH                : return "OP_SIGHASH";
        case OP_RESOLVE                : return "OP_RESOLVE";
        case OP_SIGN                   : return "OP_SIGN";
        case OP_RESOLVEANDSIGN         : return "OP_RESOLVEANDSIGN";
            
            // template matching params
        case OP_SMALLINTEGER           : return "OP_SMALLINTEGER";
        case OP_PUBKEYS                : return "OP_PUBKEYS";
        case OP_PUBKEYHASH             : return "OP_PUBKEYHASH";
        case OP_PUBKEY                 : return "OP_PUBKEY";
            
        case OP_INVALIDOPCODE          : return "OP_INVALIDOPCODE";
        default:
            return "OP_UNKNOWN";
    }
}

inline std::string ValueString(const std::vector<unsigned char>& vch)
{
    //    if (vch.size() <= 4)
    //        return strprintf("%d", CBigNum(vch).getint());
    //    else
        return HexStr(vch);
}

inline std::string StackString(const std::vector<std::vector<unsigned char> >& vStack)
{
    std::string str;
    BOOST_FOREACH(const std::vector<unsigned char>& vch, vStack)
    {
        if (!str.empty())
            str += " ";
        str += ValueString(vch);
    }
    return str;
}


// This is the raw PubKeyHash - the BitcoinAddress or more correctly the ChainAddress includes the netword Id and is hence better suited for communicating with the outside. For backwards compatability we do, however, need to use the ChainAddress in e.g. the wallet.

typedef std::vector<unsigned char> PubKey;

class COIN_EXPORT PubKeyHash : public uint160 {
public:
    PubKeyHash(const uint160& b = uint160(0)) : uint160(b) {}
    PubKeyHash(uint64_t b) : uint160(b) {}
    
    PubKeyHash& operator=(const uint160& b) {
        uint160::operator=(b);
        return *this;
    }
    PubKeyHash& operator=(uint64_t b) {
        uint160::operator=(b);
        return *this;
    }
    
    explicit PubKeyHash(const std::string& str) : uint160(str) {}
    
    explicit PubKeyHash(const std::vector<unsigned char>& vch) : uint160(vch) {}
};

PubKeyHash toPubKeyHash(const PubKey& pubkey);
PubKeyHash toPubKeyHash(const std::string& str);

class COIN_EXPORT ScriptHash : public uint160 {
public:
    ScriptHash(const uint160& b = 0) : uint160(b) {}
    ScriptHash(uint64_t b) : uint160(b) {}
    
    ScriptHash& operator=(const uint160& b) {
        uint160::operator=(b);
        return *this;
    }
    ScriptHash& operator=(uint64_t b) {
        uint160::operator=(b);
        return *this;
    }
    
    explicit ScriptHash(const std::string& str) : uint160(str) {}
    
    explicit ScriptHash(const std::vector<unsigned char>& vch) : uint160(vch) {}
};

class Script;
ScriptHash toScriptHash(const Script& script);


class Script : public std::vector<unsigned char>
{
protected:
    Script& push_int64(int64_t n)
    {
        if (n == -1 || (n >= 1 && n <= 16))
        {
            push_back(n + (OP_1 - 1));
        }
        else
        {
            CBigNum bn(n);
            *this << bn.getvch();
        }
        return *this;
    }

    Script& push_uint64(uint64_t n)
    {
        if (n >= 1 && n <= 16)
        {
            push_back(n + (OP_1 - 1));
        }
        else
        {
            CBigNum bn(n);
            *this << bn.getvch();
        }
        return *this;
    }

public:
    Script() { }
    Script(const Script& b) : std::vector<unsigned char>(b.begin(), b.end()) { }
    Script(const_iterator pbegin, const_iterator pend) : std::vector<unsigned char>(pbegin, pend) { }
#ifndef _MSC_VER
    Script(const unsigned char* pbegin, const unsigned char* pend) : std::vector<unsigned char>(pbegin, pend) { }
#endif

    Script& operator+=(const Script& b)
    {
        insert(end(), b.begin(), b.end());
        return *this;
    }

    friend Script operator+(const Script& a, const Script& b)
    {
        Script ret = a;
        ret += b;
        return ret;
    }


    explicit Script(char b)           { operator<<(b); }
    explicit Script(int16_t b)          { operator<<(b); }
    //explicit Script(int b)            { operator<<(b); }
    explicit Script(int32_t b)           { operator<<(b); }
    explicit Script(int64_t b)          { operator<<(b); }
    explicit Script(unsigned char b)  { operator<<(b); }
    //explicit Script(unsigned int b)   { operator<<(b); }
    explicit Script(uint16_t b) { operator<<(b); }
    explicit Script(uint32_t b)       { operator<<(b); }
    explicit Script(uint64_t b)         { operator<<(b); }

    explicit Script(opcodetype b)     { operator<<(b); }
    explicit Script(const uint256& b) { operator<<(b); }
    explicit Script(const CBigNum& b) { operator<<(b); }
    explicit Script(const std::vector<unsigned char>& b) { operator<<(b); }


    Script& operator<<(char b)           { return push_int64(b); }
    Script& operator<<(int16_t b)          { return push_int64(b); }
    Script& operator<<(int32_t b)            { return push_int64(b); }
    //    Script& operator<<(long b)           { return push_int64(b); }
    Script& operator<<(int64_t b)          { return push_int64(b); }
    Script& operator<<(unsigned char b)  { return push_uint64(b); }
    Script& operator<<(uint32_t b)   { return push_uint64(b); }
    Script& operator<<(uint16_t b) { return push_uint64(b); }
    //    Script& operator<<(unsigned long b)  { return push_uint64(b); }
    Script& operator<<(uint64_t b)         { return push_uint64(b); }

    Script& operator<<(opcodetype opcode)
    {
        if (opcode < 0 || opcode > 0xff)
            throw std::runtime_error("Script::operator<<() : invalid opcode");
        insert(end(), (unsigned char)opcode);
        return *this;
    }

    Script& operator<<(const uint160& b)
    {
        insert(end(), sizeof(b));
        insert(end(), (unsigned char*)&b, (unsigned char*)&b + sizeof(b));
        return *this;
    }

    Script& operator<<(const uint256& b)
    {
        insert(end(), sizeof(b));
        insert(end(), (unsigned char*)&b, (unsigned char*)&b + sizeof(b));
        return *this;
    }

    Script& operator<<(const CBigNum& b)
    {
        *this << b.getvch();
        return *this;
    }

    Script& operator<<(const std::vector<unsigned char>& b)
    {
        if (b.size() < OP_PUSHDATA1)
        {
            insert(end(), (unsigned char)b.size());
        }
        else if (b.size() <= 0xff)
        {
            insert(end(), OP_PUSHDATA1);
            insert(end(), (unsigned char)b.size());
        }
        else if (b.size() <= 0xffff)
        {
            insert(end(), OP_PUSHDATA2);
            unsigned short nSize = b.size();
            insert(end(), (unsigned char*)&nSize, (unsigned char*)&nSize + sizeof(nSize));
        }
        else
        {
            insert(end(), OP_PUSHDATA4);
            unsigned int nSize = b.size();
            insert(end(), (unsigned char*)&nSize, (unsigned char*)&nSize + sizeof(nSize));
        }
        insert(end(), b.begin(), b.end());
        return *this;
    }

    Script& operator<<(const Script& b)
    {
        // I'm not sure if this should push the script or concatenate scripts.
        // If there's ever a use for pushing a script onto a script, delete this member fn
        assert(!"warning: pushing a Script onto a Script with << is probably not intended, use + to concatenate");
        return *this;
    }


    bool getOp(iterator& pc, opcodetype& opcodeRet, std::vector<unsigned char>& vchRet)
    {
         // Wrapper so it can be called with either iterator or const_iterator
         const_iterator pc2 = pc;
         bool fRet = getOp2(pc2, opcodeRet, &vchRet);
         pc = begin() + (pc2 - begin());
         return fRet;
    }

    bool getOp(iterator& pc, opcodetype& opcodeRet)
    {
         const_iterator pc2 = pc;
         bool fRet = getOp2(pc2, opcodeRet, NULL);
         pc = begin() + (pc2 - begin());
         return fRet;
    }

    bool getOp(const_iterator& pc, opcodetype& opcodeRet, std::vector<unsigned char>& vchRet) const
    {
        return getOp2(pc, opcodeRet, &vchRet);
    }

    bool getOp(const_iterator& pc, opcodetype& opcodeRet) const
    {
        return getOp2(pc, opcodeRet, NULL);
    }

    bool getOp2(const_iterator& pc, opcodetype& opcodeRet, std::vector<unsigned char>* pvchRet) const
    {
        opcodeRet = OP_INVALIDOPCODE;
        if (pvchRet)
            pvchRet->clear();
        if (pc >= end())
            return false;

        // Read instruction
        if (end() - pc < 1)
            return false;
        unsigned int opcode = *pc++;

        // Immediate operand
        if (opcode <= OP_PUSHDATA4)
        {
            unsigned int nSize = 0;
            if (opcode < OP_PUSHDATA1)
            {
                nSize = opcode;
            }
            else if (opcode == OP_PUSHDATA1)
            {
                if (end() - pc < 1)
                    return false;
                nSize = *pc++;
            }
            else if (opcode == OP_PUSHDATA2)
            {
                if (end() - pc < 2)
                    return false;
                nSize = 0;
                memcpy(&nSize, &pc[0], 2);
                pc += 2;
            }
            else if (opcode == OP_PUSHDATA4)
            {
                if (end() - pc < 4)
                    return false;
                memcpy(&nSize, &pc[0], 4);
                pc += 4;
            }
            if (end() - pc < nSize)
                return false;
            if (pvchRet)
                pvchRet->assign(pc, pc + nSize);
            pc += nSize;
        }

        opcodeRet = (opcodetype)opcode;
        return true;
    }


    void findAndDelete(const Script& b)
    {
        if (b.empty())
            return;
        iterator pc = begin();
        opcodetype opcode;
        do
        {
            while (end() - pc >= b.size() && memcmp(&pc[0], &b[0], b.size()) == 0)
                erase(pc, pc + b.size());
        }
        while (getOp(pc, opcode));
    }


    int getSigOpCount() const
    {
        int n = 0;
        const_iterator pc = begin();
        while (pc < end())
        {
            opcodetype opcode;
            if (!getOp(pc, opcode))
                break;
            if (opcode == OP_CHECKSIG || opcode == OP_CHECKSIGVERIFY)
                n++;
            else if (opcode == OP_CHECKMULTISIG || opcode == OP_CHECKMULTISIGVERIFY)
                n += 20;
        }
        return n;
    }

    // Encode/decode small integers:
    static int decodeOP_N(opcodetype opcode) {
        if (opcode == OP_0)
            return 0;
        assert(opcode >= OP_1 && opcode <= OP_16);
        return (int)opcode - (int)(OP_1 - 1);
    }
    
    static opcodetype encodeOP_N(int n) {
        assert(n >= 0 && n <= 16);
        if (n == 0)
            return OP_0;
        return (opcodetype)(OP_1+n-1);
    }

    bool isPayToScriptHash() const;

    bool isPushOnly() const
    {
        const_iterator pc = begin();
        while (pc < end())
        {
            opcodetype opcode;
            if (!getOp(pc, opcode))
                return false;
            if (opcode > OP_16)
                return false;
        }
        return true;
    }


    PubKeyHash getAddress() const
    {
        opcodetype opcode;
        std::vector<unsigned char> vch;
        Script::const_iterator pc = begin();
        if (!getOp(pc, opcode, vch) || opcode != OP_DUP) return PubKeyHash();
        if (!getOp(pc, opcode, vch) || opcode != OP_HASH160) return PubKeyHash();
        if (!getOp(pc, opcode, vch) || vch.size() != sizeof(uint160)) return PubKeyHash();
        PubKeyHash address(vch);
        if (!getOp(pc, opcode, vch) || opcode != OP_EQUALVERIFY) return PubKeyHash();
        if (!getOp(pc, opcode, vch) || opcode != OP_CHECKSIG) return PubKeyHash();
        if (pc != end()) return PubKeyHash();
        return address;
    }

    void setAddress(const PubKeyHash& address)
    {
        this->clear();
        *this << OP_DUP << OP_HASH160 << address << OP_EQUALVERIFY << OP_CHECKSIG;
    }

    void setAddress(const PubKey& vchPubKey)
    {
        setAddress(toPubKeyHash(vchPubKey));
    }

    Script getDropped() const;
    
    void printHex() const
    {
        log_info("Script(%s)\n", HexStr(begin(), end(), true).c_str());
    }

    std::string toString() const
    {
        std::string str;
        opcodetype opcode;
        std::vector<unsigned char> vch;
        const_iterator pc = begin();
        while (pc < end())
        {
            if (!str.empty())
                str += " ";
            if (!getOp(pc, opcode, vch))
            {
                str += "[error]";
                return str;
            }
            if (0 <= opcode && opcode <= OP_PUSHDATA4)
                str += ValueString(vch);
            else
                str += GetOpName(opcode);
        }
        return str;
    }

    void print() const
    {
        log_info("%s\n", toString().c_str());
    }
};

/// Evaluator is the base functor for script evaluation
class Evaluator {
public:
    typedef std::vector<unsigned char> Value;
    typedef std::vector<Value> Stack;
    
    /// Evaluate a script
    bool operator()(const Script& script);

    /// Evaluate a script against a template - returns true if matching, and false otherwise.
    /// If matching the extracted values are pushed to the stack
    // bool operator()(const Script& script, const Script& tmpl);

    void stack(const Stack& s) {
        _stack.insert(_stack.end(), s.begin(), s.end());
    }
    
    const Stack& stack() const { return _stack; }
    
    Script stack_as_script() const {
        Script script;
        for (Stack::const_iterator val = _stack.begin(); val != _stack.end(); ++val)
            script << *val;
        return script;
    }
    
protected:
    /// Subclass Evaluator to implement eval, enbaling evaluation of context dependent operations
    virtual boost::tribool eval(opcodetype opcode);
    
    Value& top(int i = -1) {
        return _stack.at(_stack.size() + i);
    }
    
    Value& alttop(int i = -1) {
        return _alt_stack.at(_alt_stack.size()+i);
    }
    
    const Value& top(int i = -1) const {
        return _stack.at(_stack.size() + i);
    }
    
    const Value& alttop(int i = -1) const {
        return _alt_stack.at(_alt_stack.size()+i);
    }
    
    static inline void pop(Stack& stack) {
        if (stack.empty())
            throw std::runtime_error("pop() : stack empty");
        stack.pop_back();
    }

protected:
    Script::const_iterator _begin;
    Script::const_iterator _current;
    Script::const_iterator _end;
    
    int _op_count;

    bool _exec;
    std::vector<bool> _exec_stack;
    
    Stack _stack;
    Stack _alt_stack;
};

/// DropEvaluator - returns the dropped part of a script
class DropEvaluator : public Evaluator {
public:
    DropEvaluator() : Evaluator() {}

    Script dropped() const {
        Script script;
        // return the script in reverse order - namecoin has these strange scripts that are reversed
        for (Stack::const_iterator val = _dropped.begin(); val != _dropped.end(); ++val) {
            if (val->size() == 1 && 0 < (*val)[0] && (*val)[0] < 16)
                script << (int) (*val)[0];
            else
                script << *val;
        }
        return script;
    }
private:
    Stack _dropped;

protected:
    /// Subclass Evaluator to implement eval, enbaling evaluation of context dependent operations
    virtual boost::tribool eval(opcodetype opcode) {
        switch (opcode) {
            case OP_2DROP: {
                // (x1 x2 -- )
                if (_stack.size() < 2)
                    return false;
                _dropped.push_back(_stack.back());
                pop(_stack);
                _dropped.push_back(_stack.back());
                pop(_stack);
                return boost::indeterminate;
            }
            case OP_DROP: {
                // (x -- )
                if (_stack.size() < 1)
                    return false;
                _dropped.push_back(_stack.front());
                pop(_stack);
                return boost::indeterminate;
            }
            default:
                return Evaluator::eval(opcode);
        }
    }
};

/// TemplateEvaluator matches a script against a template. The stack is then populated with the template values of the script.
/// True is returned on match, false if no match
class Template : public Evaluator {
public:
    Template(const Script& script) : Evaluator(), _template(script), _it(_template.begin()), _values(0) {}

public:
    /// pubKey does a match against a standard pubkey template and returns the pubkey
    static PubKey pubKey(const Script& script) {
        Template eval(Script() << OP_PUBKEY << OP_CHECKSIG);
        if (eval(script))
            return eval.top();
        else
            return PubKey();
    }
    
    /// pubKeyHash does a match against a standard pubkeyhash template and returns the pubkeyhash
    static PubKeyHash pubKeyHash(const Script& script) {
        return script.getAddress();
        Template eval(Script() << OP_DUP << OP_HASH160 << OP_PUBKEYHASH << OP_EQUALVERIFY << OP_CHECKSIG);
        if (eval(script))
            return uint160(eval.top());
        else
            return uint160(0);
    }
    
    /// pubKey does a match against a standard P2SH template and returns the script hash
    static ScriptHash scriptHash(const Script& script) {
        Template eval(Script() << OP_HASH160 << OP_SCRIPTHASH << OP_EQUAL);
        if (eval(script))
            return uint160(eval.top());
        else
            return uint160(0);
    }

protected:
    /// Subclass Evaluator to implement eval, enbaling evaluation of context dependent operations
    virtual boost::tribool eval(opcodetype opcode) {
        opcodetype tmpl_opcode;
        Value val;
        _template.getOp(_it, tmpl_opcode, val);
        if (tmpl_opcode == opcode)
            return boost::logic::indeterminate;
        else if (val.size()) {
            if (top() == val) {
                pop(_stack);
                return boost::logic::indeterminate;
            }
            else
                return false;
        }
        else {
            switch (tmpl_opcode) {
                case OP_PUBKEYHASH:
                case OP_PUBKEY:
                case OP_PUBKEYS:
                case OP_SMALLINTEGER: {
                    // check that the latest stuff on the stack matches
                    if(_stack.size() == ++_values) {
                        // do a check of the value
                        const Value& value = top();
                        switch (tmpl_opcode) {
                            case OP_PUBKEYHASH:
                                if (value.size() == sizeof(uint160))
                                    return boost::logic::indeterminate;
                                break;
                            case OP_PUBKEY:
                                if (value.size() >= 33 && value.size() <= 120)
                                    return boost::logic::indeterminate;
                                break;
                            case OP_PUBKEYS:
                            case OP_SMALLINTEGER:
                            default:
                                break;
                        }
                        return false;
                    }
                    else
                        return false;
                }
                default:
                    return false;
            }
        }
    }

private:
    Script _template;
    Script::const_iterator _it;
    size_t _values;
};

int ScriptSigArgsExpected(txnouttype t, const std::vector<std::vector<unsigned char> >& vSolutions);

void checkCanonicalPubKey(const std::vector<unsigned char> &vchPubKey);
void checkCanonicalSignature(const std::vector<unsigned char> &vchSig);

#endif
