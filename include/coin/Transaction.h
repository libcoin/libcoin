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

#ifndef BITCOIN_TX_H
#define BITCOIN_TX_H

#include <coin/Export.h>
#include <coin/BigNum.h>
#include <coin/Key.h>
#include <coin/Script.h>
#include <coin/Serialization.h>

static const unsigned int MAX_BLOCK_SIZE = 1000000;
static const unsigned int MAX_BLOCK_SIZE_GEN = MAX_BLOCK_SIZE/2;
/** The maximum size for transactions we're willing to relay/mine */
static const unsigned int MAX_STANDARD_TX_SIZE = MAX_BLOCK_SIZE_GEN/5;
static const int MAX_BLOCK_SIGOPS = MAX_BLOCK_SIZE/50;
static const int64_t COIN = 100000000;
static const int64_t CENT = 1000000;
static const int64_t MIN_TX_FEE = 50000;
static const int64_t MIN_RELAY_TX_FEE = 10000;
static const int64_t MAX_MONEY = 21000000 * COIN;
inline bool MoneyRange(int64_t nValue) { return (nValue >= 0 && nValue <= MAX_MONEY); }
// Threshold for nLockTime: below this value it is interpreted as block number, otherwise as UNIX timestamp.
static const int LOCKTIME_THRESHOLD = 500000000; // Tue Nov  5 00:53:20 1985 UTC

class Transaction;

struct Coin { // was COutPoint

    uint256 hash;
    unsigned int index;
    
    Coin() { setNull(); }
    Coin(uint256 hashIn, unsigned int nIn) { hash = hashIn; index = nIn; }

    IMPLEMENT_SERIALIZE( READWRITE(FLATDATA(*this)); )
    
    inline friend std::ostream& operator<<(std::ostream& os, const Coin& coin) {
        return os << const_binary<Coin>(coin);
    }
    
    inline friend std::istream& operator>>(std::istream& is, Coin& coin) {
        return is >> binary<Coin>(coin);
    }

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
        log_info("%s\n", toString().c_str());
    }
};

/// An input of a transaction.  It contains the location of the previous
/// transaction's output that it claims and a signature that matches the
/// output's public key.

class COIN_EXPORT Input // was CTxInput
{
public:
    Input() {
        _sequence = UINT_MAX;
    }

    explicit Input(Coin prevout, Script signature=Script(), unsigned int sequence=UINT_MAX) {
        _prevout = prevout;
        _signature = signature;
        _sequence = sequence;
    }

    Input(uint256 prev_hash, unsigned int prev_index, Script signature=Script(), unsigned int sequence=UINT_MAX) {
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

    inline friend std::ostream& operator<<(std::ostream& os, const Input& i) {
        return os << i._prevout << i._signature << const_binary<unsigned int>(i._sequence);
    }
    
    inline friend std::istream& operator>>(std::istream& is, Input& i) {
        return is >> i._prevout >> i._signature >> binary<unsigned int>(i._sequence);
    }

    const Coin prevout() const { return _prevout; }

    const Script signature() const { return _signature; }
    void signature(Script script) { _signature = script; }

    const unsigned int sequence() const { return _sequence; }
    void sequence(unsigned int seq) { _sequence = seq; }
    
    
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
        log_info("%s\n", toString().c_str());
    }
    
private:
    Coin _prevout;
    Script _signature;
    unsigned int _sequence;
};

typedef std::vector<Input> Inputs;


/// An output of a transaction.  It contains the public key that the next input
/// must be able to sign with to claim it.

class COIN_EXPORT Output // was CTxOut
{
public:
    Output() {
        setNull();
    }

    Output(int64_t value, Script script) {
        _value = value;
        _script = script;
    }

    inline friend std::ostream& operator<<(std::ostream& os, const Output& o) {
        return os << const_binary<int64_t>(o._value) << o._script;
    }
    
    inline friend std::istream& operator>>(std::istream& is, Output& o) {
        return is >> binary<int64_t>(o._value) >> o._script;
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

    bool isNull() const {
        return (_value == -1);
    }

    const int64_t value() const { return _value; }
    
    const Script script() const { return _script; }
    
    bool isDust(int64_t min_fee = MIN_RELAY_TX_FEE) const {
        // "Dust" is defined in terms of CTransaction::nMinRelayTxFee,
        // which has units satoshis-per-kilobyte.
        // If you'd pay more than 1/3 in fees
        // to spend something, then we consider it dust.
        // A typical txout is 34 bytes big, and will
        // need a CTxIn of at least 148 bytes to spend,
        // so dust is a txout less than 54 uBTC
        // (5460 satoshis) with default nMinRelayTxFee
        return ((_value*1000)/(3*((int)GetSerializeSize(SER_DISK,0)+148)) < min_fee);
    }
    
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
        log_info("%s\n", toString().c_str());
    }
    
private:
    int64_t _value;
    Script _script;
};

typedef std::vector<Output> Outputs;

//
// The basic transaction that is broadcasted on the network and contained in
// blocks.  A transaction can contain multiple inputs and outputs.
//
class COIN_EXPORT Transaction
{
public:
    Transaction() {
        setNull();
    }

    Transaction(int version, unsigned int lockTime) {
        setNull();
        _version = version;
        _lockTime = lockTime;
    }
    
    /// We also need a copy constructor
    Transaction(const Transaction& tx) {
        setNull();
        _version = tx._version;
        _lockTime = tx._lockTime;
        for(Inputs::const_iterator i = tx._inputs.begin(); i != tx._inputs.end(); ++i)
            _inputs.push_back(*i);
        for(Outputs::const_iterator o = tx._outputs.begin(); o != tx._outputs.end(); ++o)
            _outputs.push_back(*o);
    }
    
    inline friend std::ostream& operator<<(std::ostream& os, const Transaction& txn) {
        return os << const_binary<int>(txn._version) << txn._inputs << txn._outputs << const_binary<unsigned int>(txn._lockTime);
    }
    
    inline friend std::istream& operator>>(std::istream& is, Transaction& txn) {
        return is >> binary<int>(txn._version) >> txn._inputs >> txn._outputs >> binary<unsigned int>(txn._lockTime);
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
    
    /// Get the transaction hash used for signatures
    uint256 getSignatureHash(Script scriptCode, unsigned int n, int type) const;
    
    /// Verify the signature of input n against script
    bool verify(unsigned int n, Script script, int type = 0, bool strictPayToScriptHash = true) const;
    
    /// Get version.
    unsigned int version() const { return _version; }
    
    /// Get lock time
    unsigned int lockTime() const { return _lockTime; }
    

    /// Add, replace and remove inputs. pos denotes the position.
    void addInput(const Input& input) { _inputs.push_back(input); }
    void replaceInput(unsigned int pos, const Input& input) { _inputs[pos] = input; }
    void removeInputs() { _inputs.clear(); }
    void setInputs(const Inputs& inputs) { _inputs = inputs; }

    /// Const getters for total number of inputs, single input and the set of inputs for iteration. 
    unsigned int getNumInputs() const { return _inputs.size(); }
    const Input& getInput(unsigned int i) const { return _inputs[i]; }
    const Inputs& getInputs() const { return _inputs; } 
    
    /// Add, replace, insert and remove outputs. pos denotes the position.
    void addOutput(const Output& output) { _outputs.push_back(output); }
    void replaceOutput(unsigned int pos, const Output& output) { _outputs[pos] = output; }
    void insertOutput(unsigned int pos, const Output& output) { _outputs.insert(_outputs.begin() + pos, output); }
    void removeOutputs() { _outputs.clear(); }
    void setOutputs(const Outputs& outputs) { _outputs = outputs; }

    /// Const getters for total number of outputs, single output and the set of outputs for iteration. 
    unsigned int getNumOutputs() const { return _outputs.size(); }
    const Output& getOutput(unsigned int i) const { return _outputs[i]; }
    const Outputs& getOutputs() const { return _outputs; } 
    
    /// Compare two transaction that only differ in sequence number to enable transaction replacements
    bool isNewerThan(const Transaction& old) const;

    /// Check if the transaction is a coin base transaction.
    bool isCoinBase() const {
        return (_inputs.size() == 1 && _inputs[0].isSubsidy());
    }

    /// Count the number of operations in the scripts. We need to monitor for huge (in bytes) transactions
    /// and demand a suitable extra fee.
    int getSigOpCount() const;

    /// Get the total value of the transaction (excluding the fee).
    int64_t getValueOut() const;

    /// Calculate the amount payed to a spicific address by this transaction. If the address is not part of the
    /// transaction, 0 will be returned. 
    int64_t paymentTo(PubKeyHash address) const;
    
    /// Calculate for a payment map of pubkeyhashes and amounts if a transaction is sufficient.
    bool isSufficient(std::map<PubKeyHash, int64_t> payments) const;
    
    /// Check if a transaction with a given priority can be completed with no fee.
    static bool allowFree(double dPriority) {
        // Large (in bytes) low-priority (new, small-coin) transactions
        // need a fee.
        return dPriority > COIN * 144 / 250;
    }

    /// Calculate minimum transaction fee.
    int64_t getMinFee(unsigned int nBlockSize=1, bool fAllowFree=true, bool fForRelay=false) const;

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

    std::string hexify() const {
        std::ostringstream os;
        os << *this;
        std::string ss = os.str();
//        CDataStream ss;
//        ss << *this;
        return HexStr(ss.begin(), ss.end());
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
        log_info("%s", toString().c_str());
    }

protected:
    int _version;
    Inputs _inputs;
    Outputs _outputs;
    unsigned int _lockTime;
};
    
class TransactionEvaluator : public Evaluator {
public:
    TransactionEvaluator() {}
    TransactionEvaluator(Transaction txn, unsigned int nIn, int nHashType = SIGHASH_ALL) : Evaluator(), _txn(txn), _in(nIn), _hash_type(nHashType), _code_separator(false) {}
    
    virtual bool needContext() { return _txn.getNumInputs() == 0; }
    void setTransactionContext(Transaction txn, unsigned int i, int hashtype = SIGHASH_ALL) {
        _txn = txn;
        _in = i;
        _hash_type = hashtype;
    }
    
protected:
    virtual boost::tribool eval(opcodetype opcode);
    
    bool checkSig(std::vector<unsigned char> vchSig, std::vector<unsigned char> vchPubKey, Script scriptCode);
    
protected:
    Transaction _txn;
    unsigned int _in;
    int _hash_type;
    bool _code_separator;
    Script::const_iterator _codehash;
};

#endif
