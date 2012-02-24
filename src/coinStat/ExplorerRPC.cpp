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


#include <coinStat/ExplorerRPC.h>

#include <coinChain/NodeRPC.h>

#include <coinHTTP/Server.h>
#include <coinHTTP/RPC.h>

using namespace std;
using namespace boost;
using namespace json_spirit;

Value GetDebit::operator()(const Array& params, bool fHelp) {
    if (fHelp || params.size() != 1)
        throw RPC::error(RPC::invalid_params, "getdebit <btcaddr>\n"
                         "Get debit coins of <btcaddr>");
    
    ChainAddress addr = _explorer.blockChain().chain().getAddress(params[0].get_str());
    if (!addr.isValid())
        throw RPC::error(RPC::invalid_params, "getdebit <btcaddr>\n"
                         "btcaddr invalid!");
    
    PubKeyHash address = addr.getPubKeyHash();
    
    Coins coins;
    
    _explorer.getDebit(address, coins);
    
    Array list;
    
    for(Coins::iterator coin = coins.begin(); coin != coins.end(); ++coin) {
        Object obj;
        obj.clear();
        obj.push_back(Pair("hash", coin->hash.toString()));
        obj.push_back(Pair("n", boost::uint64_t(coin->index)));
        list.push_back(obj);
    }
    
    return list;
}

Value GetCredit::operator()(const Array& params, bool fHelp) {        
    if (fHelp || params.size() != 1)
        throw RPC::error(RPC::invalid_params, "getcredit <btcaddr>\n"
                         "Get credit coins of <btcaddr>");
    
    ChainAddress addr = _explorer.blockChain().chain().getAddress(params[0].get_str());
    if (!addr.isValid())
        throw RPC::error(RPC::invalid_params, "getcredit <btcaddr>\n"
                         "btcaddr invalid!");
    
    PubKeyHash address = addr.getPubKeyHash();
    
    Coins coins;
    
    _explorer.getCredit(address, coins);
    
    Array list;
    
    for(Coins::iterator coin = coins.begin(); coin != coins.end(); ++coin) {
        Object obj;
        obj.clear();
        obj.push_back(Pair("hash", coin->hash.toString()));
        obj.push_back(Pair("n", boost::uint64_t(coin->index)));
        list.push_back(obj);
    }
    
    return list;
}

Value GetCoins::operator()(const Array& params, bool fHelp) {        
    if (fHelp || params.size() != 1)
        throw RPC::error(RPC::invalid_params, "getcoins <btcaddr>\n"
                         "Get un spent coins of <btcaddr>");
    
    ChainAddress addr = _explorer.blockChain().chain().getAddress(params[0].get_str());
    if (!addr.isValid())
        throw RPC::error(RPC::invalid_params, "getcoins <btcaddr>\n"
                         "btcaddr invalid!");
    
    PubKeyHash address = addr.getPubKeyHash();
    
    Coins coins;
    
    _explorer.getCoins(address, coins);
    
    Array list;
    
    for(Coins::iterator coin = coins.begin(); coin != coins.end(); ++coin) {
        Object obj;
        obj.clear();
        obj.push_back(Pair("hash", coin->hash.toString()));
        obj.push_back(Pair("n", boost::uint64_t(coin->index)));
        list.push_back(obj);
    }
    
    return list;
}

Value GetAddressBalance::operator()(const Array& params, bool fHelp) {        
    if (fHelp || params.size() != 1)
        throw RPC::error(RPC::invalid_params, "getbalance <address>\n"
                         "Get the balance of a single chain address");
    
    ChainAddress addr = _explorer.blockChain().chain().getAddress(params[0].get_str());
    if (!addr.isValid())
        throw RPC::error(RPC::invalid_params, "getbalance <address>\n"
                         "address invalid!");
    
    PubKeyHash address = addr.getPubKeyHash();
    
    Coins coins;
    
    _explorer.getCoins(address, coins);
    
    // Now we have the coins - now get the value of these
    boost::int64_t balance = 0;
    for(Coins::const_iterator coin = coins.begin(); coin != coins.end(); ++coin)
        balance += _explorer.blockChain().value(*coin);
    
    Value val(balance);
    
    return val;
}

Value Search::operator()(const Array& params, bool fHelp) {        
    if (fHelp || params.size() < 1)
        throw RPC::error(RPC::invalid_params, "search <address or part of hash160 or hash256>\n"
                         "Search the block chain for nearest matching address/transaction/block");
    
    // Is it an address ?
    ChainAddress addr = _explorer.blockChain().chain().getAddress(params[0].get_str());
    if (!addr.isValid()) {
        // We assume we got an address - now produce info for this:
        // return value, credit coins, debit coins, and spendable coins depending on options
        
        PubKeyHash address = addr.getPubKeyHash();
        Coins coins;
        
        if (params.size() < 2 || params[1].get_str() == "balance") {
            _explorer.getCoins(address, coins);
            // Now we have the coins - now get the value of these
            boost::int64_t balance = 0;
            for(Coins::const_iterator coin = coins.begin(); coin != coins.end(); ++coin)
                balance += _explorer.blockChain().value(*coin);
            
            Value val(balance);
            
            return val;            
        }
        else if (params.size() == 2 && params[1].get_str() == "unspent") {
            Coins coins;
            
            _explorer.getCoins(address, coins);
            
            Array list;
            
            for(Coins::iterator coin = coins.begin(); coin != coins.end(); ++coin) {
                Object obj;
                obj.clear();
                obj.push_back(Pair("hash", coin->hash.toString()));
                obj.push_back(Pair("n", boost::uint64_t(coin->index)));
                list.push_back(obj);
            }
            
            return list;
        }
        else if (params.size() == 2 && params[1].get_str() == "credit") {
            Coins coins;
            
            _explorer.getCredit(address, coins);
            
            Array list;
            
            for(Coins::iterator coin = coins.begin(); coin != coins.end(); ++coin) {
                Object obj;
                obj.clear();
                obj.push_back(Pair("hash", coin->hash.toString()));
                obj.push_back(Pair("n", boost::uint64_t(coin->index)));
                list.push_back(obj);
            }
            
            return list;
        }
        else if (params.size() == 2 && params[1].get_str() == "debit") {
            Coins coins;
            
            _explorer.getDebit(address, coins);
            
            Array list;
            
            for(Coins::iterator coin = coins.begin(); coin != coins.end(); ++coin) {
                Object obj;
                obj.clear();
                obj.push_back(Pair("hash", coin->hash.toString()));
                obj.push_back(Pair("n", boost::uint64_t(coin->index)));
                list.push_back(obj);
            }
            
            return list;
        }
        else
            return Value::null;
    }
    else { // assume a hash or hash fraction - search all possible hashes
        // hashes can be address hash, transaction hash or block hash - currently no fraction is supported
        if (params[0].get_str().size() < 64) { // assume uint160
            PubKeyHash address(params[0].get_str());
            Coins coins;
            _explorer.getCoins(address, coins);
            // Now we have the coins - now get the value of these
            boost::int64_t balance = 0;
            for(Coins::const_iterator coin = coins.begin(); coin != coins.end(); ++coin)
                balance += _explorer.blockChain().value(*coin);
            
            Value val(balance);
            
            return val;            
        }
        else { // assume uint256
            uint256 hash(params[0].get_str());
            const CBlockIndex* blockindex = _explorer.blockChain().getBlockIndex(hash);
            
            if (blockindex) { // it was a block
                Block block;
                _explorer.blockChain().getBlock(hash, block);
                
                Object result;
                result.push_back(Pair("hash", block.getHash().GetHex()));
                result.push_back(Pair("blockcount", blockindex->nHeight));
                result.push_back(Pair("version", block.getVersion()));
                result.push_back(Pair("merkleroot", block.getMerkleRoot().GetHex()));
                result.push_back(Pair("time", (boost::int64_t)block.getBlockTime()));
                result.push_back(Pair("nonce", (boost::uint64_t)block.getNonce()));
                result.push_back(Pair("difficulty", _explorer.blockChain().getDifficulty(blockindex)));
                Array txhashes;
                BOOST_FOREACH (const Transaction&tx, block.getTransactions())
                txhashes.push_back(tx.getHash().GetHex());
                
                result.push_back(Pair("tx", txhashes));
                
                if (blockindex->pprev)
                    result.push_back(Pair("hashprevious", blockindex->pprev->GetBlockHash().GetHex()));
                if (blockindex->pnext)
                    result.push_back(Pair("hashnext", blockindex->pnext->GetBlockHash().GetHex()));
                
                return result;
            }
            
            int64 timestamp = 0;
            int64 blockheight = 0;
            
            Transaction tx;
            _explorer.blockChain().getTransaction(hash, tx, blockheight, timestamp);
            
            if(!tx.isNull()) {                
                Object entry = tx2json(tx, timestamp, blockheight);
                return entry;
            }
        }
    }
    return Value::null;
}
