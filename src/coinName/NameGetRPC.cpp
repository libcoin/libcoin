/* -*-c++-*- libcoin - Copyright (C) 2014 Daniel Kraft
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


#include <coinName/NameGetRPC.h>

#include <coinChain/NodeRPC.h>

#include <coinHTTP/Server.h>
#include <coinHTTP/RPC.h>

/*
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
*/
