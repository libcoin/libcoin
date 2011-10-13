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

int64 AmountFromValue(const json_spirit::Value& value);
json_spirit::Value ValueFromAmount(int64 amount);
std::string AccountFromValue(const json_spirit::Value& value);

std::string JSONRPCRequest(const std::string& strMethod, const json_spirit::Array& params, const json_spirit::Value& id);

json_spirit::Object JSONRPCError(int code, const std::string& message);

void ThreadRPCServer(void* parg);
int CommandLineRPC(int argc, char *argv[]);

std::string EncodeBase64(std::string s);
std::string DecodeBase64(std::string s);

int ReadHTTP(std::basic_istream<char>& stream, std::map<std::string, std::string>& mapHeadersRet, std::string& strMessageRet);
std::string HTTPPost(const std::string& strMsg, const std::map<std::string,std::string>& mapRequestHeaders);

#endif