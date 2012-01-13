// Copyright (c) 2011 Michael Gronager
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef _WALLETRPC_H_
#define _WALLETRPC_H_

#include "btcHTTP/RPC.h"
#include "btc/util.h"

class Wallet;
class CWalletDB;

/// Base class for all wallet rpc methods - they all need a handle to the wallet.
class WalletMethod : public Method {
public:
    WalletMethod(Wallet& wallet) : _wallet(wallet) {}
protected:
    Wallet& _wallet;
};

/// Return the balance of the wallet.
class GetBalance : public WalletMethod {
public:
    GetBalance(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
private:
    int64 GetAccountBalance(CWalletDB& walletdb, const string& strAccount, int nMinDepth);
    int64 GetAccountBalance(const string& strAccount, int nMinDepth);
};

/// Get a fresh address. This will reserve the address in the wallet and hence change the wallet state.
class GetNewAddress : public WalletMethod {
public:
    GetNewAddress(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Send money to an address. (If encrypted, Wallet need to by unlocked first).
class SendToAddress : public WalletMethod {
public:
    SendToAddress(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};


/// Get address associated with account
class CBitcoinAddress;
class GetAccountAddress : public WalletMethod {
public:
    GetAccountAddress(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
private:
    CBitcoinAddress getAccountAddress(std::string strAccount, bool bForceNew=false);
};


/*

json_spirit::Value getaccountaddress(const json_spirit::Array& params, bool fHelp);
json_spirit::Value setaccount(const json_spirit::Array& params, bool fHelp);
json_spirit::Value getaccount(const json_spirit::Array& params, bool fHelp);
json_spirit::Value getaddressesbyaccount(const json_spirit::Array& params, bool fHelp);
json_spirit::Value settxfee(const json_spirit::Array& params, bool fHelp);

json_spirit::Value getreceivedbyaddress(const json_spirit::Array& params, bool fHelp);
json_spirit::Value getreceivedbyaccount(const json_spirit::Array& params, bool fHelp);

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
*/

#endif
