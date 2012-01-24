
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


/// An input of a transaction.  It contains the location of the previous
/// transaction's output that it claims and a signature that matches the
/// output's public key.

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

typedef std::vector<Input> Inputs;


/// An output of a transaction.  It contains the public key that the next input
/// must be able to sign with to claim it.

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

typedef std::vector<Output> Outputs;

//
// The basic transaction that is broadcasted on the network and contained in
// blocks.  A transaction can contain multiple inputs and outputs.
//
class Transaction
{
public:
    Transaction() {
        setNull();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->_version);
        nVersion = this->_version;
        READWRITE(_inputs);
        READWRITE(_outputs);
        READWRITE(_lockTime);
    )

    void setNull() {
        _version = 1;
        _inputs.clear();
        _outputs.clear();
        _lockTime = 0;
    }

    bool isNull() const {
        return (_inputs.empty() && _outputs.empty());
    }

    /// Get the transaction hash. Note it is computed on call, so it is recommended to cache it.
    uint256 getHash() const {
        return SerializeHash(*this);
    }
    
    /// Get version.
    unsigned int version() const { return _version; }
    
    /// Get lock time
    unsigned int lockTime() const { return _lockTime; }
    

    /// Add, replace and remove inputs. pos denotes the position.
    void addInput(const Input& input) { _inputs.push_back(input); }
    void replaceInput(unsigned int pos, const Input& input) { _inputs[pos] = input; }
    void removeInputs() { _inputs.clear(); }

    /// Const getters for total number of inputs, single input and the set of inputs for iteration. 
    unsigned int getNumInputs() const { return _inputs.size(); }
    const Input& getInput(unsigned int i) const { return _inputs[i]; }
    const Inputs& getInputs() const { return _inputs; } 
    
    /// Add, replace, insert and remove outputs. pos denotes the position.
    void addOutput(const Output& output) { _outputs.push_back(output); }
    void replaceOutput(unsigned int pos, const Output& output) { _outputs[pos] = output; }
    void insertOutput(unsigned int pos, const Output& output) { _outputs.insert(_outputs.begin() + pos, output); }
    void removeOutputs() { _outputs.clear(); }

    /// Const getters for total number of outputs, single output and the set of outputs for iteration. 
    unsigned int getNumOutputs() const { return _outputs.size(); }
    const Output& getOutput(unsigned int i) const { return _outputs[i]; }
    const Outputs& getOutputs() const { return _outputs; } 
    
    /// Compare two transaction that only differ in sequence number to enable transaction replacements
    bool isNewerThan(const Transaction& old) const {
        if (_inputs.size() != old._inputs.size())
            return false;
        for (int i = 0; i < _inputs.size(); i++)
            if (_inputs[i].prevout() != old._inputs[i].prevout())
                return false;

        bool newer = false;
        unsigned int lowest = UINT_MAX;
        for (int i = 0; i < _inputs.size(); i++) {
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

    /// Check if the transaction is a coin base transaction.
    bool isCoinBase() const {
        return (_inputs.size() == 1 && _inputs[0].isSubsidy());
    }

    /// Count the number of operations in the scripts. We need to monitor for huge (in bytes) transactions
    /// and demand a suitable extra fee.
    int getSigOpCount() const {
        int n = 0;
        BOOST_FOREACH(const Input& input, _inputs)
            n += input.signature().GetSigOpCount();
        BOOST_FOREACH(const Output& output, _outputs)
            n += output.script().GetSigOpCount();
        return n;
    }

    /// Get the total value of the transaction (excluding the fee).
    int64 getValueOut() const
    {
        int64 valueOut = 0;
        BOOST_FOREACH(const Output& output, _outputs)
        {
            valueOut += output.value();
            if (!MoneyRange(output.value()) || !MoneyRange(valueOut))
                throw std::runtime_error("Transaction::GetValueOut() : value out of range");
        }
        return valueOut;
    }

    /// Calculate the amount payed to a spicific address by this transaction. If the address is not part of the
    /// transaction, 0 will be returned. 
    int64 paymentTo(Address address) const {
        int64 value = 0;
        BOOST_FOREACH(const Output& output, _outputs)
        {
            if(output.getAddress() == address)
                value += output.value();
        }
        
        return value;
    }
    
    /// Check if a transaction with a given priority can be completed with no fee.
    static bool allowFree(double dPriority) {
        // Large (in bytes) low-priority (new, small-coin) transactions
        // need a fee.
        return dPriority > COIN * 144 / 250;
    }

    /// Calculate minimum transaction fee.
    int64 getMinFee(unsigned int nBlockSize=1, bool fAllowFree=true, bool fForRelay=false) const {
        // Base fee is either MIN_TX_FEE or MIN_RELAY_TX_FEE
        int64 nBaseFee = fForRelay ? MIN_RELAY_TX_FEE : MIN_TX_FEE;

        unsigned int nBytes = ::GetSerializeSize(*this, SER_NETWORK);
        unsigned int nNewBlockSize = nBlockSize + nBytes;
        int64 nMinFee = (1 + (int64)nBytes / 1000) * nBaseFee;

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
        if (nMinFee < nBaseFee)
            BOOST_FOREACH(const Output& output, _outputs)
                if (output.value() < CENT)
                    nMinFee = nBaseFee;

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

    /// Compare two transactions.
    friend bool operator==(const Transaction& a, const Transaction& b) {
        return (a._version  == b._version &&
                a._inputs   == b._inputs &&
                a._outputs  == b._outputs &&
                a._lockTime == b._lockTime);
    }

    friend bool operator!=(const Transaction& a, const Transaction& b) {
        return !(a == b);
    }


    /// toString and print - for debugging.
    std::string toString() const {
        std::string str;
        str += strprintf("Transaction(hash=%s, ver=%d, vin.size=%d, vout.size=%d, nLockTime=%d)\n",
            getHash().toString().substr(0,10).c_str(),
            _version,
            _inputs.size(),
            _outputs.size(),
            _lockTime);
        for (int i = 0; i < _inputs.size(); i++)
            str += "    " + _inputs[i].toString() + "\n";
        for (int i = 0; i < _outputs.size(); i++)
            str += "    " + _outputs[i].toString() + "\n";
        return str;
    }

    void print() const {
        printf("%s", toString().c_str());
    }

    /// Check if all internal settings of a transaction are valid. This is not a check if e.g. the inputs are spent.
    bool checkTransaction() const;
    
protected:
    int _version;
    Inputs _inputs;
    Outputs _outputs;
    unsigned int _lockTime;
};

#endif
