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

#include "coinChain/NodeRPC.h"
#include "coinHTTP/RPC.h"

using namespace std;
using namespace boost;
using namespace json_spirit;

Value GetBlockCount::operator()(const Array& params, bool fHelp) {
    if (fHelp || params.size() != 0)
        throw RPC::error(RPC::invalid_params, "getblockcount\n"
                         "Returns the number of blocks in the longest block chain.");
    
    return _node.blockChain().getBestHeight();
}        


Value GetConnectionCount::operator()(const Array& params, bool fHelp) {
    if (fHelp || params.size() != 0)
        throw RPC::error(RPC::invalid_params, "getconnectioncount\n"
                         "Returns the number of connections to other nodes.");
    
    return (int) _node.getConnectionCount();
}


Value GetDifficulty::operator()(const Array& params, bool fHelp) {
    if (fHelp || params.size() != 0)
        throw RPC::error(RPC::invalid_params, "getdifficulty\n"
                         "Returns the proof-of-work difficulty as a multiple of the minimum difficulty.");
    
    return getDifficulty();
}


double GetDifficulty::getDifficulty() {
    // Floating point number that is a multiple of the minimum difficulty,
    // minimum difficulty = 1.0.
    
    const CBlockIndex* pindexBest = _node.blockChain().getBestIndex();
    if (pindexBest == NULL)
        return 1.0;
    int nShift = (pindexBest->nBits >> 24) & 0xff;
    
    double dDiff =
    (double)0x0000ffff / (double)(pindexBest->nBits & 0x00ffffff);
    
    while (nShift < 29) {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29) {
        dDiff /= 256.0;
        nShift--;
    }
    
    return dDiff;
}

Value GetInfo::operator()(const Array& params, bool fHelp) {
    if (fHelp || params.size() != 0)
        throw RPC::error(RPC::invalid_params, "getinfo\n"
                         "Returns an object containing various state info.");
    
    Object obj;
    obj.push_back(Pair("version",       (int)VERSION));
    //        obj.push_back(Pair("balance",       ValueFromAmount(pwalletMain->GetBalance())));
    obj.push_back(Pair("blocks",        (int)_node.blockChain().getBestHeight()));
    obj.push_back(Pair("connections",   (int)_node.getConnectionCount()));
    //        obj.push_back(Pair("proxy",         (fUseProxy ? addrProxy.ToStringIPPort() : string())));
    //        obj.push_back(Pair("generate",      (bool)fGenerateBitcoins));
    //        obj.push_back(Pair("genproclimit",  (int)(fLimitProcessors ? nLimitProcessors : -1)));
    obj.push_back(Pair("difficulty",    (double)getDifficulty()));
    //        obj.push_back(Pair("hashespersec",  gethashespersec(params, false)));
    obj.push_back(Pair("testnet",       (&_node.blockChain().chain() == &testnet)));
    //        obj.push_back(Pair("keypoololdest", (boost::int64_t)pwalletMain->GetOldestKeyPoolTime()));
    //        obj.push_back(Pair("paytxfee",      ValueFromAmount(nTransactionFee)));
    //        obj.push_back(Pair("errors",        GetWarnings("statusbar")));
    return obj;
}

