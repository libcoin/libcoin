// Copyright (c) 2011 Michael Gronager
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef _BITCOIN_WALLETRPC_H_
#define _BITCOIN_WALLETRPC_H_

#include "btcRPC/rpc.h"

class CWallet;
extern CWallet* pwalletMain;

json_spirit::Value syncwallet(const json_spirit::Array& params, bool fHelp);
json_spirit::Value getnewaddress(const json_spirit::Array& params, bool fHelp);
json_spirit::Value getaccountaddress(const json_spirit::Array& params, bool fHelp);
json_spirit::Value setaccount(const json_spirit::Array& params, bool fHelp);
json_spirit::Value getaccount(const json_spirit::Array& params, bool fHelp);
json_spirit::Value getaddressesbyaccount(const json_spirit::Array& params, bool fHelp);
json_spirit::Value settxfee(const json_spirit::Array& params, bool fHelp);
json_spirit::Value sendtoaddress(const json_spirit::Array& params, bool fHelp);
json_spirit::Value getreceivedbyaddress(const json_spirit::Array& params, bool fHelp);
json_spirit::Value getreceivedbyaccount(const json_spirit::Array& params, bool fHelp);
json_spirit::Value getbalance(const json_spirit::Array& params, bool fHelp);
json_spirit::Value movecmd(const json_spirit::Array& params, bool fHelp);
json_spirit::Value sendfrom(const json_spirit::Array& params, bool fHelp);
json_spirit::Value sendmany(const json_spirit::Array& params, bool fHelp);
json_spirit::Value listreceivedbyaddress(const json_spirit::Array& params, bool fHelp);
json_spirit::Value listreceivedbyaccount(const json_spirit::Array& params, bool fHelp);
json_spirit::Value listtransactions(const json_spirit::Array& params, bool fHelp);
json_spirit::Value listaccounts(const json_spirit::Array& params, bool fHelp);
json_spirit::Value gettransaction(const json_spirit::Array& params, bool fHelp);
json_spirit::Value backupwallet(const json_spirit::Array& params, bool fHelp);
json_spirit::Value keypoolrefill(const json_spirit::Array& params, bool fHelp);
json_spirit::Value walletpassphrase(const json_spirit::Array& params, bool fHelp);
json_spirit::Value walletpassphrasechange(const json_spirit::Array& params, bool fHelp);
json_spirit::Value walletlock(const json_spirit::Array& params, bool fHelp);
json_spirit::Value encryptwallet(const json_spirit::Array& params, bool fHelp);
json_spirit::Value validateaddress(const json_spirit::Array& params, bool fHelp);


#endif
