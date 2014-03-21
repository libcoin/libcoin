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

#include <coinPool/SubmitBlock.h>
#include <coinHTTP/RPC.h>

using namespace std;
using namespace boost;
using namespace json_spirit;

json_spirit::Value SubmitBlock::operator()(const json_spirit::Array& params, bool fHelp) {
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw RPC::error(RPC::invalid_params, "submitblock <hex data> [<workid>]\n"
                         "Attempts to submit new block to network.\n"
                         "The block can contain a header and coinbase (followed by a workid) or a full block"
                         "See https://en.bitcoin.it/wiki/BIP_0022 for full specification.");
    
    if (params[0].type() != str_type)
        throw RPC::error(RPC::parse_error, "Expect a string, hex encoded data, as the first param");
    
    string workid;
    if (params.size() > 1 && params[1].type() == obj_type) {
        Object obj = params[1].get_obj();
        Value workid_value = find_value(obj, "workid");
        if (workid_value.type() == str_type)
            workid = workid_value.get_str();
    }
    
    vector<unsigned char> block_data(ParseHex(params[0].get_str()));
    istringstream block_stream(string(block_data.begin(), block_data.end()));
//    CDataStream block_stream(block_data, SER_NETWORK, PROTOCOL_VERSION);
    Block block;
    try {
        block_stream >> block;
    }
    catch (std::ios_base::failure& f) { // in case of read errors we assume that it is due to a trunkated block, i.e. we just got the coinbase
        block.keepOnlyCoinbase();
    }
    catch (std::exception &e) {
        throw RPC::error(RPC::parse_error, "Block decode failed");
    }
    
    string result = _pool.submitBlock(block, workid);
    
    if (result.size())
        return result;
    else
        return Value::null;
}
