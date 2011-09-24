// Copyright (c) 2011 Michael Gronager
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef _BITCOIN_PROXY_H_
#define _BITCOIN_PROXY_H_

#include "btcRPC/rpc.h"

json_spirit::Value gettxmaturity(const json_spirit::Array& params, bool fHelp);
json_spirit::Value gettxdetails(const json_spirit::Array& params, bool fHelp);
json_spirit::Value getvalue(const json_spirit::Array& params, bool fHelp);
json_spirit::Value getdebit(const json_spirit::Array& params, bool fHelp);
json_spirit::Value getcredit(const json_spirit::Array& params, bool fHelp);
json_spirit::Value getcoins(const json_spirit::Array& params, bool fHelp);
json_spirit::Value posttx(const json_spirit::Array& params, bool fHelp);

#endif
