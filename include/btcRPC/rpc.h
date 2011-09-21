// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPC_H
#define BITCOIN_RPC_H

#include "json/json_spirit.h"

#include <set>
#include <map>
#include <string>

typedef json_spirit::Value(*rpcfn_type)(const json_spirit::Array& params, bool fHelp);

extern std::set<std::string> setAllowInSafeMode;
extern std::map<std::string, rpcfn_type> mapCallTable;

json_spirit::Object JSONRPCError(int code, const std::string& message);

void ThreadRPCServer(void* parg);
int CommandLineRPC(int argc, char *argv[]);

#endif