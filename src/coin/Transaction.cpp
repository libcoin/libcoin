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

int Transaction::getSigOpCount() const {
    int n = 0;
    BOOST_FOREACH(const Input& input, _inputs)
    n += input.signature().getSigOpCount();
    BOOST_FOREACH(const Output& output, _outputs)
    n += output.script().getSigOpCount();
    return n;
}

int64 Transaction::getValueOut() const
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

int64 Transaction::paymentTo(PubKeyHash address) const {
    int64 value = 0;
    BOOST_FOREACH(const Output& output, _outputs)
    {
    if(output.getAddress() == address)
        value += output.value();
    }
    
    return value;
}

bool Transaction::isSufficient(map<PubKeyHash, int64> payments) const {
    for (map<PubKeyHash, int64>::const_iterator payment = payments.begin(); payment != payments.end(); ++payment) {
        if (paymentTo(payment->first) < payment->second)
            return false;
    }
    return true;
}

int64 Transaction::getMinFee(unsigned int nBlockSize, bool fAllowFree, bool fForRelay) const {
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


bool Transaction::checkTransaction() const
{
    // Basic checks that don't depend on any context
    if (_inputs.empty())
        return error("Transaction::CheckTransaction() : vin empty");
    if (_outputs.empty())
        return error("Transaction::CheckTransaction() : vout empty");
    // Size limits
    if (::GetSerializeSize(*this, SER_NETWORK) > MAX_BLOCK_SIZE)
        return error("Transaction::CheckTransaction() : size limits failed");

    // Check for negative or overflow output values
    int64 nValueOut = 0;
    BOOST_FOREACH(const Output& output, _outputs)
    {
        if (output.value() < 0)
            return error("Transaction::CheckTransaction() : txout.nValue negative");
        if (output.value() > MAX_MONEY)
            return error("Transaction::CheckTransaction() : txout.nValue too high");
        nValueOut += output.value();
        if (!MoneyRange(nValueOut))
            return error("Transaction::CheckTransaction() : txout total out of range");
    }

    // Check for duplicate inputs
    set<Coin> vInOutPoints;
    BOOST_FOREACH(const Input& input, _inputs)
    {
        if (vInOutPoints.count(input.prevout()))
            return false;
        vInOutPoints.insert(input.prevout());
    }

    if (isCoinBase())
    {
        if (_inputs[0].signature().size() < 2 || _inputs[0].signature().size() > 100)
            return error("Transaction::CheckTransaction() : coinbase script size");
    }
    else
    {
        BOOST_FOREACH(const Input& input, _inputs)
            if (input.prevout().isNull())
                return error("Transaction::CheckTransaction() : prevout is null");
    }

    return true;
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
    CDataStream ss(SER_GETHASH);
    ss.reserve(10000);
    ss << txn << type;
    
    return Hash(ss.begin(), ss.end());
}

