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

#include <coinPool/GetBlockTemplate.h>

#include <coinHTTP/RPC.h>

using namespace std;
using namespace boost;
using namespace json_spirit;

void GetBlockTemplate::dispatch(const CompletionHandler& f, const Request& request) {
    RPC rpc(request);
    // parse to find longpollid
    Array params = rpc.params();
    if (params.size() == 0 || params[0].type() != obj_type) {
        PoolMethod::dispatch(f); // leave it to the call operator to throw the errors
        return;
    }
    
    Object obj = params[0].get_obj();
    Value longpollid = find_value(obj, "longpollid");
    
    if (longpollid.type() != str_type) { // first call (or error) just dispatch
        PoolMethod::dispatch(f); // leave it to the call operator to throw the errors
        return;
    }
    
    // parse the string / format is simple - only a count of claims
    size_t count = lexical_cast<size_t>(longpollid.get_str());
    
    // install the handler
    install_handler(f, count);
}

Value GetBlockTemplate::operator()(const Array& params, bool fHelp) {
    if (fHelp || params.size() == 0 || params[0].type() != obj_type)
        throw RPC::error(RPC::invalid_params, "getblocktemplate\n [{capabilities : []}]"
                         "Returns an object containing various state info.");
    
    Object obj = params[0].get_obj();
    Value capabilities = find_value(obj, "capabilities");
    
    if (capabilities.type() != array_type)
        throw RPC::error(RPC::parse_error, "Expected a capability array");
    
    bool cap_coinbasetxn = false;
    bool cap_workid = false;
    bool cap_coinbase_append = false;
    for (size_t i = 0; i < capabilities.get_array().size(); ++i) {
        if (capabilities.get_array()[i].type() != str_type)
            continue; // ignore non string capabilities
        string capability = capabilities.get_array()[i].get_str();
        if (capability == "coinbasetxn")
            cap_coinbasetxn = true;
        else if (capability == "workid")
            cap_workid = true;
        else if (capability == "coinbase/append")
            cap_coinbase_append = true;
    }
    
    // we need coinbasetxn capability otherwise throw an error
    if (!cap_coinbasetxn)
        throw RPC::error(RPC::invalid_request, "Need coinbasetxn capability");
    
    // now, we compose a block - a block consists of a coinbasetxn and a all transactions data
    
    Object result;
    
    // Update block
    Block block;
    uint256 target;
    boost::tie(block, target) = _pool.getWork();
    
    // The block template consists of the header values, a coinbasetxn, and an array of transactions (omitting the coinbasetxn)
    
    // various flags
    result.push_back(Pair("sigoplimit", (int64_t)MAX_BLOCK_SIGOPS));
    result.push_back(Pair("sizelimit", (int64_t)MAX_BLOCK_SIZE));
    result.push_back(Pair("target", target.GetHex()));
    result.push_back(Pair("expires", 120));
    
    Array mutables;
    mutables.push_back("coinbase/append");
    mutables.push_back("submit/coinbase");
    
    result.push_back(Pair("mutable", mutables));
    
    if (cap_workid)
        result.push_back(Pair("workid", block.getMerkleRoot().GetHex()));
    
    // generate the longpollid
    result.push_back(Pair("longpollid", longpollid())); // blocknumber where action should be taken and numbers of new claims
    
    // header stuff
    result.push_back(Pair("version", block.getVersion()));
    result.push_back(Pair("previousblockhash", block.getPrevBlock().GetHex()));
    result.push_back(Pair("curtime", (int64_t)block.getTime()));
    result.push_back(Pair("bits", hex_bits(block.getBits())));
    result.push_back(Pair("noncerange", "00000000ffffffff"));
    
    // coinbase stuff
    
    result.push_back(Pair("height", block.getHeight())); // this is possible only for version >= 2 blocks
    
    Object coinbasetxn;
    const Transaction& txn = block.getTransaction(0);
//    CDataStream ds(SER_NETWORK, PROTOCOL_VERSION);
//    ds << txn;
    ostringstream os;
    os << txn;
    string ds = os.str();
    coinbasetxn.push_back(Pair("data", HexStr(ds.begin(), ds.end())));
    
    result.push_back(Pair("coinbasetxn", coinbasetxn));
    
    // transaction stuff
    
    Array transactions;
    
    for (size_t i = 1; i < block.getNumTransactions(); ++i) {
        Object transaction;
        const Transaction& txn = block.getTransaction(i);
//        CDataStream ds(SER_NETWORK, PROTOCOL_VERSION);
//        ds << txn;
        ostringstream os;
        os << txn;
        string ds = os.str();
        transaction.push_back(Pair("data", HexStr(ds.begin(), ds.end())));
        transactions.push_back(transaction);
    }
    
    result.push_back(Pair("transactions", transactions));
    
    return result;
}

std::string GetBlockTemplate::hex_bits(unsigned int bits) const {
    union {
        int32_t bits;
        char cBits[4];
    } uBits;
    uBits.bits = htonl((int32_t)bits);
    return HexStr(BEGIN(uBits.cBits), END(uBits.cBits));
}

void GetBlockTemplate::install_handler(const CompletionHandler& f, size_t count) {
    boost::mutex::scoped_lock lock(_handlers_access);
    
    _handlers.insert(make_pair(count, f));
}

void GetBlockTemplate::erase(HandlerRange range) {
    boost::mutex::scoped_lock lock(_handlers_access);
    
    _handlers.erase(range.first, range.second);
}

GetBlockTemplate::HandlerRange GetBlockTemplate::current_handlers() {
    boost::mutex::scoped_lock lock(_handlers_access);
    
    size_t count = _pool.blockChain().claim_count();
    Handlers::iterator from = _handlers.begin();
    Handlers::iterator to = _handlers.upper_bound(count);
    return HandlerRange(from, to);
}

GetBlockTemplate::HandlerRange GetBlockTemplate::all_handlers() {
    boost::mutex::scoped_lock lock(_handlers_access);
    
    Handlers::iterator from = _handlers.begin();
    Handlers::iterator to = _handlers.end();
    return HandlerRange(from, to);
}

std::string GetBlockTemplate::longpollid() const {
    return lexical_cast<string>(_pool.blockChain().claim_count() + _claim_threshold);
}
