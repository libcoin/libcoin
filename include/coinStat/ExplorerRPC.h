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

#ifndef EXPLORERRPC_H
#define EXPLORERRPC_H

#include <coinStat/Explorer.h>

#include <coinChain/Node.h>
#include <coinChain/NodeRPC.h>

#include <coinHTTP/Server.h>
#include <coinHTTP/RPC.h>

/// Base class for all Node rpc methods - they all need a handle to the node.
class ExplorerMethod : public Method {
public:
    ExplorerMethod(Explorer& explorer) : _explorer(explorer) {}
protected:
    Explorer& _explorer;
};

/// Get debit coins belonging to an Address 
class GetDebit : public ExplorerMethod {
public:
    GetDebit(Explorer& explorer) : ExplorerMethod(explorer) {}
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);    
};

/// Get credit coins belonging to an Address 
class GetCredit : public ExplorerMethod {
public:
    GetCredit(Explorer& explorer) : ExplorerMethod(explorer) {}
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);    
};

/// Get unspent coins belonging to an Address 
class GetCoins : public ExplorerMethod {
public:
    GetCoins(Explorer& explorer) : ExplorerMethod(explorer) {}
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);    
};

/// Get the balance based on the unspent coins of an Address 
class GetAddressBalance : public ExplorerMethod {
public:
    GetAddressBalance(Explorer& explorer) : ExplorerMethod(explorer) { setName("getbalance"); }
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);    
};

/// Do a search in the blockchain and in the explorer database.
class Search : public ExplorerMethod {
public:
    Search(Explorer& explorer) : ExplorerMethod(explorer) {}
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);    
};

#endif // EXPLORERRPC_H