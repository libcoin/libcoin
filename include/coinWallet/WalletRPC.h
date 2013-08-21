// Copyright (c) 2012 Michael Gronager
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef _WALLETRPC_H_
#define _WALLETRPC_H_

#include <coinWallet/Export.h>

#include <coinHTTP/RPC.h>
#include <coin/util.h>

class Wallet;
class CWalletDB;

/// Base class for all wallet rpc methods - they all need a handle to the wallet.
class COINWALLET_EXPORT WalletMethod : public Method {
public:
    WalletMethod(Wallet& wallet) : _wallet(wallet) {}
protected:
    Wallet& _wallet;
};

/// Return the balance of the wallet.
class COINWALLET_EXPORT GetBalance : public WalletMethod {
public:
    GetBalance(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
protected:
    int64_t GetAccountBalance(CWalletDB& walletdb, const std::string& strAccount, int nMinDepth);
    int64_t GetAccountBalance(const std::string& strAccount, int nMinDepth);
};

/// Return the unconfirmed balance or Held balance.
class COINWALLET_EXPORT GetHeldBalance : public GetBalance {
    GetHeldBalance(Wallet& wallet) : GetBalance(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Get a fresh address. This will reserve the address in the wallet and hence change the wallet state.
class COINWALLET_EXPORT GetNewAddress : public WalletMethod {
public:
    GetNewAddress(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Send money to an address. (If encrypted, Wallet need to by unlocked first).
class COINWALLET_EXPORT SendToAddress : public WalletMethod {
public:
    SendToAddress(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Get address associated with account
class ChainAddress;
class COINWALLET_EXPORT GetAccountAddress : public WalletMethod {
public:
    GetAccountAddress(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
protected:
    ChainAddress getAccountAddress(std::string strAccount, bool bForceNew=false);
};

/// Get address associated with account
class COINWALLET_EXPORT SetAccount : public GetAccountAddress { // inherit to reuse the getAccountAddress method
public:
    SetAccount(Wallet& wallet) : GetAccountAddress(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Get address associated with account
class COINWALLET_EXPORT GetAccount : public WalletMethod {
public:
    GetAccount(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Get addresses associated with account
class COINWALLET_EXPORT GetAddressesByAccount : public WalletMethod {
public:
    GetAddressesByAccount(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Set default tx fee.
class COINWALLET_EXPORT SetTxFee : public WalletMethod {
public:
    SetTxFee(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Get money received by address.
class COINWALLET_EXPORT GetReceivedByAddress : public WalletMethod {
public:
    GetReceivedByAddress(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Get money received by account.
class COINWALLET_EXPORT GetReceivedByAccount : public WalletMethod {
public:
    GetReceivedByAccount(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Move money from one account to the other.
class COINWALLET_EXPORT MoveCmd : public WalletMethod {
public:
    MoveCmd(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Send from a named account.
class COINWALLET_EXPORT SendFrom : public GetBalance {
public:
    SendFrom(Wallet& wallet) : GetBalance(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Send to many bitcoin addresses at once.
class CWalletTx;
class CAccountingEntry;
class COINWALLET_EXPORT SendMany : public GetBalance {
public:
    SendMany(Wallet& wallet) : GetBalance(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// List account related info base class.
class COINWALLET_EXPORT ListMethod : public WalletMethod {
public:
    ListMethod(Wallet& wallet) : WalletMethod(wallet) {}
protected:
    json_spirit::Value listReceived(const json_spirit::Array& params, bool fByAccounts);
    void listTransactions(const CWalletTx& wtx, const std::string& strAccount, int nMinDepth, bool fLong, json_spirit::Array& ret);    
    void acEntryToJSON(const CAccountingEntry& acentry, const std::string& strAccount, json_spirit::Array& ret);
    void walletTxToJSON(const CWalletTx& wtx, json_spirit::Object& entry);
};

/// Get money received by account.
class COINWALLET_EXPORT ListReceivedByAddress : public ListMethod {
public:
    ListReceivedByAddress(Wallet& wallet) : ListMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Get money received by account.
class COINWALLET_EXPORT ListReceivedByAccount : public ListMethod {
public:
    ListReceivedByAccount(Wallet& wallet) : ListMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Get money received by account.
class COINWALLET_EXPORT ListTransactions : public ListMethod {
public:
    ListTransactions(Wallet& wallet) : ListMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Get money received by account.
class COINWALLET_EXPORT ListAccounts : public ListMethod {
public:
    ListAccounts(Wallet& wallet) : ListMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Get wallet transactions.
class COINWALLET_EXPORT GetWalletTransaction : public ListMethod {
public:
    GetWalletTransaction(Wallet& wallet) : ListMethod(wallet) { setName("gettransaction"); }
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Backup the wallet.
class COINWALLET_EXPORT BackupWallet : public WalletMethod {
public:
    BackupWallet(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Refill the key pool.
class COINWALLET_EXPORT KeypoolRefill : public WalletMethod {
public:
    KeypoolRefill(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Enter the wallet passphrase and supply a timeout for when the wallet is locked again.
class COINWALLET_EXPORT WalletPassphrase : public WalletMethod {
public:
    WalletPassphrase(Wallet& wallet, boost::asio::io_service& io_service) : WalletMethod(wallet), _lock_timer(io_service) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
private:
    boost::asio::deadline_timer _lock_timer;
};

/// Change the wallet passphrase.
class COINWALLET_EXPORT WalletPassphraseChange : public WalletMethod {
public:
    WalletPassphraseChange(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Lock the wallet.
class COINWALLET_EXPORT WalletLock : public WalletMethod {
public:
    WalletLock(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Encrypt the wallet.
class COINWALLET_EXPORT EncryptWallet : public WalletMethod {
public:
    EncryptWallet(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

/// Validate a bitcoin address.
class COINWALLET_EXPORT ValidateAddress : public WalletMethod {
public:
    ValidateAddress(Wallet& wallet) : WalletMethod(wallet) {}
    virtual json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);
};

#endif
