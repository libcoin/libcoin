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

#include <coinChain/NodeRPC.h>
#include <coinHTTP/RPC.h>

using namespace std;
using namespace boost;
using namespace json_spirit;

Value GetBlockHash::operator()(const Array& params, bool fHelp) {
    if (fHelp || params.size() != 1)
        throw RPC::error(RPC::invalid_params, "getblockhash <index>\n"
                            "Returns hash of block in best-block-chain at <index>.");
    
    int height = params[0].get_int();
    if (height < 0 || height > _node.blockChain().getBestHeight())
        throw RPC::error(RPC::invalid_request, "Block number out of range.");

    BlockIterator blk = _node.blockChain().iterator(height);
    return blk->hash.GetHex();;
}

Value GetBlock::operator()(const Array& params, bool fHelp) {
    if (fHelp || params.size() != 1)
        throw RPC::error(RPC::invalid_params, "getblock <hash>\n"
                            "Returns details of a block with given block-hash.");
    
    std::string strHash = params[0].get_str();
    uint256 hash(strHash);
    
    BlockIterator blk = _node.blockChain().iterator(hash);
    
    Block block;
    _node.blockChain().getBlock(hash, block);
    
    if (block.isNull())
        throw RPC::error(RPC::invalid_request,  "Block not found");
        
    Object result;
    result.push_back(Pair("hash", blk->hash.GetHex()));
    result.push_back(Pair("blockcount", blk.height()));
    result.push_back(Pair("version", block.getVersion()));
    result.push_back(Pair("merkleroot", block.getMerkleRoot().GetHex()));
    result.push_back(Pair("time", (boost::int64_t)block.getBlockTime()));
    result.push_back(Pair("nonce", (boost::uint64_t)block.getNonce()));
    result.push_back(Pair("difficulty", _node.blockChain().getDifficulty(blk)));
    Array txhashes;
    BOOST_FOREACH (const Transaction&tx, block.getTransactions())
    txhashes.push_back(tx.getHash().GetHex());
    
    result.push_back(Pair("tx", txhashes));

    BlockIterator prev = blk + 1;
    BlockIterator next = blk - 1;
    if (!!prev)
        result.push_back(Pair("hashprevious", prev->hash.GetHex()));
    if (!!next )
        result.push_back(Pair("hashnext", next->hash.GetHex()));
    
    return result;
}        

Object tx2json(Transaction &tx, int64_t timestamp, int64_t blockheight)
{
    Object entry;
    
    // scheme follows the scheme of blockexplorer:
    // "hash" : hash in hex
    // "ver" : vernum
    uint256 hash = tx.getHash();
    entry.push_back(Pair("timestamp", (boost::int64_t)timestamp));
    entry.push_back(Pair("blockheight", (boost::int64_t)blockheight));
    entry.push_back(Pair("hash", hash.toString()));
    entry.push_back(Pair("ver", (int)tx.version()));
    entry.push_back(Pair("vin_sz", boost::uint64_t(tx.getNumInputs())));
    entry.push_back(Pair("vout_sz", boost::uint64_t(tx.getNumOutputs())));
    entry.push_back(Pair("lock_time", boost::uint64_t(tx.lockTime())));
    entry.push_back(Pair("size", boost::uint64_t(serialize_size(tx))));
    
    // now loop over the txins
    Array txins;
    BOOST_FOREACH(const Input& txin, tx.getInputs()) {
        Object inentry;
        inentry.clear();
        Object prevout;
        prevout.clear();
        prevout.push_back(Pair("hash", txin.prevout().hash.toString()));
        prevout.push_back(Pair("n", boost::uint64_t(txin.prevout().index)));
        inentry.push_back(Pair("prev_out", prevout));
        if(tx.isCoinBase())            
            inentry.push_back(Pair("coinbase", HexStr(txin.signature())));
        else
            inentry.push_back(Pair("scriptSig", txin.signature().toString()));
        txins.push_back(inentry);
    }
    entry.push_back(Pair("in", txins));
    
    // now loop over the txouts
    Array txouts;
    BOOST_FOREACH(const Output& txout, tx.getOutputs()) {
        Object outentry;
        outentry.clear();
        outentry.push_back(Pair("value", strprintf("%"PRI64d".%08"PRI64d"",txout.value()/COIN, txout.value()%COIN))); // format correctly
        outentry.push_back(Pair("scriptPubKey", txout.script().toString()));
        txouts.push_back(outentry);
    }
    entry.push_back(Pair("out", txouts));
    
    return entry;
}

Value GetTransaction::operator()(const Array& params, bool fHelp) {
    
    if (fHelp || params.size() != 1)
        throw RPC::error(RPC::invalid_params, "gettxdetails <txhash>\n"
                            "Get transaction details for transaction with hash <txhash>");
    
    uint256 hash;
    hash.SetHex(params[0].get_str());
    
    int64_t timestamp = 0;
    int64_t blockheight = 0;
    
    Transaction tx;
    _node.blockChain().getTransaction(hash, tx, blockheight, timestamp);
    if(tx.isNull())
        throw RPC::error(RPC::invalid_params, "Invalid transaction id");        
    
    Object entry = tx2json(tx, timestamp, blockheight);
    
    return entry;    
}        

Value GetPenetration::operator()(const Array& params, bool fHelp) {
    if (fHelp || params.size() < 1)
        throw RPC::error(RPC::invalid_params, "getpenetration <txhash> <penetration fraction> \n"
                         "Get how many peers that have announced this transaction with hash <txhash> \n"
                         "Optionally if <pene...> is present, getpenetration will not exit until this req is met, or it is timed out");

    uint256 hash;
    hash.SetHex(params[0].get_str());
    
    int64_t timestamp = 0;
    int64_t blockheight = 0;
    
    Transaction tx;
    _node.blockChain().getTransaction(hash, tx, blockheight, timestamp);
    int bestheight = _node.blockChain().getBestHeight();

    size_t total = _node.getConnectionCount();
    size_t known = total;
    
    if(tx.isNull()) { // transaction not yet confirmed - get penetration
        known = _node.peerPenetration(hash);
    }
    
    if (params.size() == 2 ) { // client is requesting a blocking wait
        
    }
    
    // now build the answer : { confirmations, known, total } - 0, 0, X is unknown, n, X, X is confirmed up to n confirmations, 0, x, X is a x/X penetration
    Object obj;
    obj.push_back(Pair("confirmations", (int)(bestheight - blockheight + 1)));
    obj.push_back(Pair("known", (int)known));
    obj.push_back(Pair("total", (int)total));
    
    // register callback for a this specific tx to be confirmed up to certain level
    // NOT FINISHED!!!
    return Value::null;
}
    
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
    
    return _node.blockChain().getDifficulty();
}


Value GetInfo::operator()(const Array& params, bool fHelp) {
    if (fHelp || params.size() != 0)
        throw RPC::error(RPC::invalid_params, "getinfo\n"
                         "Returns an object containing various state info.");
    
    Object obj;
    obj.push_back(Pair("version",       (int)_node.getClientVersion()));
    obj.push_back(Pair("protocolversion", (int)_node.blockChain().chain().protocol_version()));
    //        obj.push_back(Pair("balance",       ValueFromAmount(pwalletMain->GetBalance())));
    obj.push_back(Pair("blocks",        (int)_node.blockChain().getBestHeight()));
    obj.push_back(Pair("connections",   (int)_node.getConnectionCount()));
    //        obj.push_back(Pair("proxy",         (fUseProxy ? addrProxy.ToStringIPPort() : string())));
    //        obj.push_back(Pair("generate",      (bool)fGenerateBitcoins));
    //        obj.push_back(Pair("genproclimit",  (int)(fLimitProcessors ? nLimitProcessors : -1)));
    obj.push_back(Pair("difficulty",    (double)_node.blockChain().getDifficulty()));
    //        obj.push_back(Pair("hashespersec",  gethashespersec(params, false)));
    obj.push_back(Pair("testnet",       (&_node.blockChain().chain() == &testnet3)));
    //        obj.push_back(Pair("keypoololdest", (boost::int64_t)pwalletMain->GetOldestKeyPoolTime()));
    //        obj.push_back(Pair("paytxfee",      ValueFromAmount(nTransactionFee)));
    //        obj.push_back(Pair("errors",        GetWarnings("statusbar")));
    return obj;
}

