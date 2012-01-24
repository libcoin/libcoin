// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_TX_H
#define BITCOIN_TX_H

#include "btc/bignum.h"
#include "btc/key.h"
#include "btc/script.h"

static const unsigned int MAX_BLOCK_SIZE = 1000000;
static const unsigned int MAX_BLOCK_SIZE_GEN = MAX_BLOCK_SIZE/2;
static const int MAX_BLOCK_SIGOPS = MAX_BLOCK_SIZE/50;
static const int64 COIN = 100000000;
static const int64 CENT = 1000000;
static const int64 MIN_TX_FEE = 50000;
static const int64 MIN_RELAY_TX_FEE = 10000;
static const int64 MAX_MONEY = 21000000 * COIN;
inline bool MoneyRange(int64 nValue) { return (nValue >= 0 && nValue <= MAX_MONEY); }
static const int COINBASE_MATURITY = 100;
// Threshold for nLockTime: below this value it is interpreted as block number, otherwise as UNIX timestamp.
static const int LOCKTIME_THRESHOLD = 500000000; // Tue Nov  5 00:53:20 1985 UTC

class Transaction;

struct Coin // was COutPoint
{
    uint256 hash;
    unsigned int index;
    
    Coin() { setNull(); }
    Coin(uint256 hashIn, unsigned int nIn) { hash = hashIn; index = nIn; }

    IMPLEMENT_SERIALIZE( READWRITE(FLATDATA(*this)); )
    
    void setNull() { hash = 0; index = -1; }
    bool isNull() const { return (hash == 0 && index == -1); }
    
    friend bool operator<(const Coin& a, const Coin& b) {
        return (a.hash < b.hash || (a.hash == b.hash && a.index < b.index));
    }
    
    friend bool operator==(const Coin& a, const Coin& b) {
        return (a.hash == b.hash && a.index == b.index);
    }
    
    friend bool operator!=(const Coin& a, const Coin& b) {
        return !(a == b);
    }
    
    std::string toString() const {
        return strprintf("COutPoint(%s, %d)", hash.toString().substr(0,10).c_str(), index);
    }
    
    void print() const {
        printf("%s\n", toString().c_str());
    }
};

struct CoinRef // was CInPoint
{
    Transaction* ptx;
    unsigned int index;
    
    CoinRef() { setNull(); }
    CoinRef(Transaction* ptxIn, unsigned int nIn) { ptx = ptxIn; index = nIn; }
    
    void setNull() { ptx = NULL; index = -1; }
    bool isNull() const { return (ptx == NULL && index == -1); }
};

//
// An input of a transaction.  It contains the location of the previous
// transaction's output that it claims and a signature that matches the
// output's public key.
//
class Input // was CTxInput
{
public:
    Input() {
        _sequence = UINT_MAX;
    }

    explicit Input(Coin prevout, CScript signature=CScript(), unsigned int sequence=UINT_MAX) {
        _prevout = prevout;
        _signature = signature;
        _sequence = sequence;
    }

    Input(uint256 prev_hash, unsigned int prev_index, CScript signature=CScript(), unsigned int sequence=UINT_MAX) {
        _prevout = Coin(prev_hash, prev_index);
        _signature = signature;
        _sequence = sequence;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(_prevout);
        READWRITE(_signature);
        READWRITE(_sequence);
    )

    const Coin prevout() const { return _prevout; }
    const CScript signature() const { return _signature; }
    const unsigned int sequence() const { return _sequence; }
    
    
    bool isFinal() const {
        return (_sequence == UINT_MAX);
    }

    friend bool operator==(const Input& a, const Input& b) {
        return (a._prevout   == b._prevout &&
                a._signature == b._signature &&
                a._sequence == b._sequence);
    }

    friend bool operator!=(const Input& a, const Input& b) {
        return !(a == b);
    }

    bool isSubsidy() const {
        return _prevout.isNull();
    }
    
    std::string toString() const {
        std::string str;
        str += strprintf("CTxIn(");
        str += _prevout.toString();
        if (_prevout.isNull())
            str += strprintf(", coinbase %s", HexStr(_signature).c_str());
        else
            str += strprintf(", scriptSig=%s", _signature.toString().substr(0,24).c_str());
        if (_sequence != UINT_MAX)
            str += strprintf(", nSequence=%u", _sequence);
        str += ")";
        return str;
    }

    void print() const {
        printf("%s\n", toString().c_str());
    }
    
private:
    Coin _prevout;
    CScript _signature;
    unsigned int _sequence;
};


//
// An output of a transaction.  It contains the public key that the next input
// must be able to sign with to claim it.
//

class Output // was CTxOut
{
public:
    Output() {
        setNull();
    }

    Output(int64 value, CScript script) {
        _value = value;
        _script = script;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(_value);
        READWRITE(_script);
    )

    void setNull() {
        _value = -1;
        _script.clear();
    }

    bool isNull() {
        return (_value == -1);
    }

    const int64 value() const { return _value; }
    
    const CScript script() const { return _script; }
    
    uint256 getHash() const {
        return SerializeHash(*this);
    }

    uint160 getAddress() const;
    
    friend bool operator==(const Output& a, const Output& b) {
        return (a._value       == b._value &&
                a._script == b._script);
    }

    friend bool operator!=(const Output& a, const Output& b) {
        return !(a == b);
    }

    std::string toString() const {
        if (_script.size() < 6)
            return "CTxOut(error)";
        return strprintf("CTxOut(nValue=%"PRI64d".%08"PRI64d", scriptPubKey=%s)", _value / COIN, _value % COIN, _script.toString().substr(0,30).c_str());
    }

    void print() const {
        printf("%s\n", toString().c_str());
    }
    
private:
    int64 _value;
    CScript _script;
};


//
// The basic transaction that is broadcasted on the network and contained in
// blocks.  A transaction can contain multiple inputs and outputs.
//
class Transaction
{
public:
    int nVersion;
    std::vector<Input> vin;
    std::vector<Output> vout;
    unsigned int nLockTime;


    Transaction()
    {
        SetNull();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(vin);
        READWRITE(vout);
        READWRITE(nLockTime);
    )

    void SetNull()
    {
        nVersion = 1;
        vin.clear();
        vout.clear();
        nLockTime = 0;
    }

    bool IsNull() const
    {
        return (vin.empty() && vout.empty());
    }

    uint256 GetHash() const
    {
        return SerializeHash(*this);
    }

    bool IsNewerThan(const Transaction& old) const
    {
        if (vin.size() != old.vin.size())
            return false;
        for (int i = 0; i < vin.size(); i++)
            if (vin[i].prevout() != old.vin[i].prevout())
                return false;

        bool newer = false;
        unsigned int lowest = UINT_MAX;
        for (int i = 0; i < vin.size(); i++) {
            if (vin[i].sequence() != old.vin[i].sequence()) {
                if (vin[i].sequence() <= lowest) {
                    newer = false;
                    lowest = vin[i].sequence();
                }
                if (old.vin[i].sequence() < lowest) {
                    newer = true;
                    lowest = old.vin[i].sequence();
                }
            }
        }
        return newer;
    }

    bool IsCoinBase() const
    {
        return (vin.size() == 1 && vin[0].isSubsidy());
    }

    int GetSigOpCount() const
    {
        int n = 0;
        BOOST_FOREACH(const Input& txin, vin)
            n += txin.signature().GetSigOpCount();
        BOOST_FOREACH(const Output& txout, vout)
            n += txout.script().GetSigOpCount();
        return n;
    }
/*
 // IsStandard is chain dependent - so it cannot be evaluated on a pr class basis.
    bool IsStandard() const
    {
        BOOST_FOREACH(const Input& txin, vin)
            if (!txin.scriptSig.IsPushOnly())
                return error("nonstandard txin: %s", txin.scriptSig.toString().c_str());
        BOOST_FOREACH(const Output& txout, vout)
            if (!::IsStandard(txout.scriptPubKey))
                return error("nonstandard txout: %s", txout.scriptPubKey.toString().c_str());
        return true;
    }
*/
    int64 GetValueOut() const
    {
        int64 nValueOut = 0;
        BOOST_FOREACH(const Output& txout, vout)
        {
            nValueOut += txout.value();
            if (!MoneyRange(txout.value()) || !MoneyRange(nValueOut))
                throw std::runtime_error("Transaction::GetValueOut() : value out of range");
        }
        return nValueOut;
    }

    int64 paymentTo(uint160 btc) const
    {
        int64 value = 0;
        BOOST_FOREACH(const Output& txout, vout)
        {
            if(txout.getAddress() == btc)
                value += txout.value();
        }
        
        return value;
    }
    
    static bool AllowFree(double dPriority)
    {
        // Large (in bytes) low-priority (new, small-coin) transactions
        // need a fee.
        return dPriority > COIN * 144 / 250;
    }

    int64 GetMinFee(unsigned int nBlockSize=1, bool fAllowFree=true, bool fForRelay=false) const
    {
        // Base fee is either MIN_TX_FEE or MIN_RELAY_TX_FEE
        int64 nBaseFee = fForRelay ? MIN_RELAY_TX_FEE : MIN_TX_FEE;

        unsigned int nBytes = ::GetSerializeSize(*this, SER_NETWORK);
        unsigned int nNewBlockSize = nBlockSize + nBytes;
        int64 nMinFee = (1 + (int64)nBytes / 1000) * nBaseFee;

        if (fAllowFree)
        {
            if (nBlockSize == 1)
            {
                // Transactions under 10K are free
                // (about 4500bc if made of 50bc inputs)
                if (nBytes < 10000)
                    nMinFee = 0;
            }
            else
            {
                // Free transaction area
                if (nNewBlockSize < 27000)
                    nMinFee = 0;
            }
        }

        // To limit dust spam, require MIN_TX_FEE/MIN_RELAY_TX_FEE if any output is less than 0.01
        if (nMinFee < nBaseFee)
            BOOST_FOREACH(const Output& txout, vout)
                if (txout.value() < CENT)
                    nMinFee = nBaseFee;

        // Raise the price as the block approaches full
        if (nBlockSize != 1 && nNewBlockSize >= MAX_BLOCK_SIZE_GEN/2)
        {
            if (nNewBlockSize >= MAX_BLOCK_SIZE_GEN)
                return MAX_MONEY;
            nMinFee *= MAX_BLOCK_SIZE_GEN / (MAX_BLOCK_SIZE_GEN - nNewBlockSize);
        }

        if (!MoneyRange(nMinFee))
            nMinFee = MAX_MONEY;
        return nMinFee;
    }

    friend bool operator==(const Transaction& a, const Transaction& b)
    {
        return (a.nVersion  == b.nVersion &&
                a.vin       == b.vin &&
                a.vout      == b.vout &&
                a.nLockTime == b.nLockTime);
    }

    friend bool operator!=(const Transaction& a, const Transaction& b)
    {
        return !(a == b);
    }


    std::string toString() const
    {
        std::string str;
        str += strprintf("Transaction(hash=%s, ver=%d, vin.size=%d, vout.size=%d, nLockTime=%d)\n",
            GetHash().toString().substr(0,10).c_str(),
            nVersion,
            vin.size(),
            vout.size(),
            nLockTime);
        for (int i = 0; i < vin.size(); i++)
            str += "    " + vin[i].toString() + "\n";
        for (int i = 0; i < vout.size(); i++)
            str += "    " + vout[i].toString() + "\n";
        return str;
    }

    void print() const
    {
        printf("%s", toString().c_str());
    }

    bool CheckTransaction() const;
};

#endif
