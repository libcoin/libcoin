// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include "btc/util.h"
#include "btc/tx.h"

using namespace std;
using namespace boost;

//
// Global state
//

const int nTotalBlocksEstimate = 140700; // Conservative estimate of total nr of blocks on main chain

//////////////////////////////////////////////////////////////////////////////
//
// CTxOut
//

uint160 CTxOut::getAsset() const
{
    vector<pair<opcodetype, vector<unsigned char> > > vSolution;
    if (!Solver(scriptPubKey, vSolution))
        return 0;
    
    BOOST_FOREACH(PAIRTYPE(opcodetype, vector<unsigned char>)& item, vSolution)
    {
        vector<unsigned char> vchPubKey;
        if (item.first == OP_PUBKEY)
            // encode the pubkey into a hash160
            return Hash160(item.second);
        else if (item.first == OP_PUBKEYHASH)
            return uint160(item.second);                
    }
    
    return 0;
}


//////////////////////////////////////////////////////////////////////////////
//
// Transaction
//

bool Transaction::CheckTransaction() const
{
    // Basic checks that don't depend on any context
    if (vin.empty())
        return error("Transaction::CheckTransaction() : vin empty");
    if (vout.empty())
        return error("Transaction::CheckTransaction() : vout empty");
    // Size limits
    if (::GetSerializeSize(*this, SER_NETWORK) > MAX_BLOCK_SIZE)
        return error("Transaction::CheckTransaction() : size limits failed");

    // Check for negative or overflow output values
    int64 nValueOut = 0;
    BOOST_FOREACH(const CTxOut& txout, vout)
    {
        if (txout.nValue < 0)
            return error("Transaction::CheckTransaction() : txout.nValue negative");
        if (txout.nValue > MAX_MONEY)
            return error("Transaction::CheckTransaction() : txout.nValue too high");
        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut))
            return error("Transaction::CheckTransaction() : txout total out of range");
    }

    // Check for duplicate inputs
    set<Coin> vInOutPoints;
    BOOST_FOREACH(const CTxIn& txin, vin)
    {
        if (vInOutPoints.count(txin.prevout))
            return false;
        vInOutPoints.insert(txin.prevout);
    }

    if (IsCoinBase())
    {
        if (vin[0].scriptSig.size() < 2 || vin[0].scriptSig.size() > 100)
            return error("Transaction::CheckTransaction() : coinbase script size");
    }
    else
    {
        BOOST_FOREACH(const CTxIn& txin, vin)
            if (txin.prevout.isNull())
                return error("Transaction::CheckTransaction() : prevout is null");
    }

    return true;
}
