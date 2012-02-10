// Copyright (c) 2012 Michael Gronager
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef _WALLETRPC_H_
#define _WALLETRPC_H_

#include "coinHTTP/RPC.h"
#include "coin/util.h"

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
protected:
    int64 GetAccountBalance(CWalletDB& walletdb, const std::string& strAccount, int nMinDepth);
    int64 GetAccountBalance(const std::string& strAccount, int nMinDepth);
};

/// Return the unconfirmed balance or Held balance.
class GetHeldBalance : public GetBalance {
    GetHeldBalance(Wallet& wallet) : GetBalance(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
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
class ChainAddress;
class GetAccountAddress : public WalletMethod {
public:
    GetAccountAddress(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
protected:
    ChainAddress getAccountAddress(std::string strAccount, bool bForceNew=false);
};

/// Get address associated with account
class SetAccount : public GetAccountAddress { // inherit to reuse the getAccountAddress method
public:
    SetAccount(Wallet& wallet) : GetAccountAddress(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Get address associated with account
class GetAccount : public WalletMethod {
public:
    GetAccount(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Get addresses associated with account
class GetAddressesByAccount : public WalletMethod {
public:
    GetAddressesByAccount(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Set default tx fee.
class SetTxFee : public WalletMethod {
public:
    SetTxFee(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Get money received by address.
class GetReceivedByAddress : public WalletMethod {
public:
    GetReceivedByAddress(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Get money received by account.
class GetReceivedByAccount : public WalletMethod {
public:
    GetReceivedByAccount(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Move money from one account to the other.
class MoveCmd : public WalletMethod {
public:
    MoveCmd(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Send from a named account.
class SendFrom : public GetBalance {
public:
    SendFrom(Wallet& wallet) : GetBalance(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Send to many bitcoin addresses at once.
class CWalletTx;
class CAccountingEntry;
class SendMany : public GetBalance {
public:
    SendMany(Wallet& wallet) : GetBalance(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// List account related info base class.
class ListMethod : public WalletMethod {
public:
    ListMethod(Wallet& wallet) : WalletMethod(wallet) {}
protected:
    json_spirit::Value listReceived(const json_spirit::Array& params, bool fByAccounts);
    void listTransactions(const CWalletTx& wtx, const std::string& strAccount, int nMinDepth, bool fLong, json_spirit::Array& ret);    
    void acEntryToJSON(const CAccountingEntry& acentry, const std::string& strAccount, json_spirit::Array& ret);
    void walletTxToJSON(const CWalletTx& wtx, json_spirit::Object& entry);
};

/// Get money received by account.
class ListReceivedByAddress : public ListMethod {
public:
    ListReceivedByAddress(Wallet& wallet) : ListMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Get money received by account.
class ListReceivedByAccount : public ListMethod {
public:
    ListReceivedByAccount(Wallet& wallet) : ListMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Get money received by account.
class ListTransactions : public ListMethod {
public:
    ListTransactions(Wallet& wallet) : ListMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Get money received by account.
class ListAccounts : public ListMethod {
public:
    ListAccounts(Wallet& wallet) : ListMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Get wallet transactions.
class GetWalletTransaction : public ListMethod {
public:
    GetWalletTransaction(Wallet& wallet) : ListMethod(wallet) { setName("gettransaction"); }
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Backup the wallet.
class BackupWallet : public WalletMethod {
public:
    BackupWallet(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Refill the key pool.
class KeypoolRefill : public WalletMethod {
public:
    KeypoolRefill(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Enter the wallet passphrase and supply a timeout for when the wallet is locked again.
class WalletPassphrase : public WalletMethod {
public:
    WalletPassphrase(Wallet& wallet, boost::asio::io_service& io_service) : WalletMethod(wallet), _lock_timer(io_service) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
private:
    boost::asio::deadline_timer _lock_timer;
};

/// Change the wallet passphrase.
class WalletPassphraseChange : public WalletMethod {
public:
    WalletPassphraseChange(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Lock the wallet.
class WalletLock : public WalletMethod {
public:
    WalletLock(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Encrypt the wallet.
class EncryptWallet : public WalletMethod {
public:
    EncryptWallet(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Validate a bitcoin address.
class ValidateAddress : public WalletMethod {
public:
    ValidateAddress(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

#endif
