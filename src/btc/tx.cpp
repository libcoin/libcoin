// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
//#include "headers.h"
//#include "db.h"
//#include "net.h"
//#include "init.h"
//#include "cryptopp/sha.h"
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include "btc/util.h"
#include "btc/tx.h"

using namespace std;
using namespace boost;

//
// Global state
//

int nBestHeight = -1;
const int nTotalBlocksEstimate = 140700; // Conservative estimate of total nr of blocks on main chain

//////////////////////////////////////////////////////////////////////////////
//
// CTx
//

bool CTx::CheckTransaction() const
{
    // Basic checks that don't depend on any context
    if (vin.empty())
        return error("CTransaction::CheckTransaction() : vin empty");
    if (vout.empty())
        return error("CTransaction::CheckTransaction() : vout empty");
    // Size limits
    if (::GetSerializeSize(*this, SER_NETWORK) > MAX_BLOCK_SIZE)
        return error("CTransaction::CheckTransaction() : size limits failed");

    // Check for negative or overflow output values
    int64 nValueOut = 0;
    BOOST_FOREACH(const CTxOut& txout, vout)
    {
        if (txout.nValue < 0)
            return error("CTransaction::CheckTransaction() : txout.nValue negative");
        if (txout.nValue > MAX_MONEY)
            return error("CTransaction::CheckTransaction() : txout.nValue too high");
        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut))
            return error("CTransaction::CheckTransaction() : txout total out of range");
    }

    // Check for duplicate inputs
    set<COutPoint> vInOutPoints;
    BOOST_FOREACH(const CTxIn& txin, vin)
    {
        if (vInOutPoints.count(txin.prevout))
            return false;
        vInOutPoints.insert(txin.prevout);
    }

    if (IsCoinBase())
    {
        if (vin[0].scriptSig.size() < 2 || vin[0].scriptSig.size() > 100)
            return error("CTransaction::CheckTransaction() : coinbase script size");
    }
    else
    {
        BOOST_FOREACH(const CTxIn& txin, vin)
            if (txin.prevout.IsNull())
                return error("CTransaction::CheckTransaction() : prevout is null");
    }

    return true;
}

// Return conservative estimate of total number of blocks, 0 if unknown
int GetTotalBlocksEstimate()
{
    if(fTestNet)
    {
        return 0;
    }
    else
    {
        return nTotalBlocksEstimate;
    }
}
