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

#include "coinWallet/WalletRPC.h"
#include "coinWallet/Wallet.h"
#include "coinWallet/WalletDB.h"

using namespace std;
using namespace boost;
using namespace json_spirit;

Value ValueFromAmount(int64 amount) {
    return (double)amount / (double)COIN;
}

int64 AmountFromValue(const Value& value)
{
    double dAmount;
    if(value.type() == str_type)
        dAmount = lexical_cast<double>(value.get_str());
    else
        dAmount = value.get_real();
    if (dAmount <= 0.0 || dAmount > 21000000.0)
        throw RPC::error(RPC::invalid_request, "Invalid amount");
    int64 nAmount = roundint64(dAmount * COIN);
    if (!MoneyRange(nAmount))
        throw RPC::error(RPC::invalid_request, "Invalid amount");
    return nAmount;
}

string AccountFromValue(const Value& value)
{
    string strAccount = value.get_str();
    if (strAccount == "*")
        throw RPC::error(RPC::invalid_params, "Invalid account name");
    return strAccount;
}

int64 GetBalance::GetAccountBalance(CWalletDB& walletdb, const string& strAccount, int nMinDepth)
{
    int64 nBalance = 0;
    
    // Tally wallet transactions
    for (map<uint256, CWalletTx>::const_iterator it = _wallet.mapWallet.begin(); it != _wallet.mapWallet.end(); ++it)
        {
        const CWalletTx& wtx = (*it).second;
        if (!_wallet.isFinal(wtx))
            continue;
        
        int64 nGenerated, nReceived, nSent, nFee;
        wtx.GetAccountAmounts(strAccount, nGenerated, nReceived, nSent, nFee);
        
        if (nReceived != 0 && _wallet.getDepthInMainChain(wtx.getHash()) >= nMinDepth)
            nBalance += nReceived;
        nBalance += nGenerated - nSent - nFee;
        }
    
    // Tally internal accounting entries
    nBalance += walletdb.GetAccountCreditDebit(strAccount);
    
    return nBalance;
}

int64 GetBalance::GetAccountBalance(const string& strAccount, int nMinDepth)
{
    CWalletDB walletdb(_wallet._dataDir, _wallet.strWalletFile);
    return GetAccountBalance(walletdb, strAccount, nMinDepth);
}

Value GetBalance::operator()(const Array& params, bool fHelp) {
    if (fHelp || params.size() > 2)
        throw RPC::error(RPC::invalid_params, "getbalance [account] [minconf=1]\n"
                         "If [account] is not specified, returns the server's total available balance.\n"
                         "If [account] is specified, returns the balance in the account.");
    
    int nMinDepth = 1;
    if (params.size() > 1) {
        if (params[1].type() != json_spirit::int_type) {
            if (params[1].type() == json_spirit::str_type)
                nMinDepth = lexical_cast<int>(params[1].get_str());
        }
        else
            nMinDepth = params[1].get_int();
    }
    
    if (params.size() == 0)
        return  ValueFromAmount(_wallet.GetBalance(nMinDepth>0));
    
    if (params[0].get_str() == "*") {
        // Calculate total balance a different way from GetBalance()
        // (GetBalance() sums up all unspent TxOuts)
        // getbalance and getbalance '*' should always return the same number.
        int64 nBalance = 0;
        for (map<uint256, CWalletTx>::const_iterator it = _wallet.mapWallet.begin(); it != _wallet.mapWallet.end(); ++it) {
            const CWalletTx& wtx = (*it).second;
            if (!_wallet.isFinal(wtx))
                continue;
            
            int64 allGeneratedImmature, allGeneratedMature, allFee;
            allGeneratedImmature = allGeneratedMature = allFee = 0;
            string strSentAccount;
            list<pair<ChainAddress, int64> > listReceived;
            list<pair<ChainAddress, int64> > listSent;
            wtx.GetAmounts(allGeneratedImmature, allGeneratedMature, listReceived, listSent, allFee, strSentAccount);
            if (_wallet.getDepthInMainChain(wtx.getHash()) >= nMinDepth)
                BOOST_FOREACH(const PAIRTYPE(ChainAddress,int64)& r, listReceived)
                nBalance += r.second;
            BOOST_FOREACH(const PAIRTYPE(ChainAddress,int64)& r, listSent)
            nBalance -= r.second;
            nBalance -= allFee;
            nBalance += allGeneratedMature;
        }
        return  ValueFromAmount(nBalance);
    }
    
    string strAccount = AccountFromValue(params[0]);
    
    int64 nBalance = GetAccountBalance(strAccount, nMinDepth);
    
    return ValueFromAmount(nBalance);
    
}

Value GetNewAddress::operator()(const Array& params, bool fHelp) {
    if (fHelp || params.size() > 1)
        throw RPC::error(RPC::invalid_params, "getnewaddress [account]\n"
                            "Returns a new bitcoin address for receiving payments.  "
                            "If [account] is specified (recommended), it is added to the address book "
                            "so payments received with the address will be credited to [account].");
    
    // Parse the account first so we don't generate a key if there's an error
    string strAccount;
    if (params.size() > 0)
        strAccount = AccountFromValue(params[0]);
    
    if (!_wallet.IsLocked())
        _wallet.TopUpKeyPool();
    
    // Generate a new key that is added to wallet
    std::vector<unsigned char> newKey;
    if (!_wallet.GetKeyFromPool(newKey, false))
        throw RPC::error(RPC::internal_error, "Error: Keypool ran out, please call keypoolrefill first");
    ChainAddress address(_wallet.chain().networkId(), newKey);
    
    _wallet.SetAddressBookName(address, strAccount);
    
    return address.toString();
}

Value SendToAddress::operator()(const Array& params, bool fHelp) {
    if (_wallet.IsCrypted() && (fHelp || params.size() < 2 || params.size() > 4))
        throw RPC::error(RPC::invalid_request, "sendtoaddress <bitcoinaddress> <amount> [comment] [comment-to]\n"
                            "<amount> is a real and is rounded to the nearest 0.00000001\n"
                            "requires wallet passphrase to be set with walletpassphrase first");
    
    if (!_wallet.IsCrypted() && (fHelp || params.size() < 2 || params.size() > 4))
        throw RPC::error(RPC::invalid_params, "sendtoaddress <bitcoinaddress> <amount> [comment] [comment-to]\n"
                            "<amount> is a real and is rounded to the nearest 0.00000001");
    
    ChainAddress address(params[0].get_str());
    if (!address.isValid(_wallet.chain().networkId()))
        throw RPC::error(RPC::invalid_params, "Invalid bitcoin address");
    
    // Amount
    int64 nAmount = AmountFromValue(params[1]);
    
    // Wallet comments
    CWalletTx wtx;
    if (params.size() > 2 && params[2].type() != null_type && !params[2].get_str().empty())
        wtx.mapValue["comment"] = params[2].get_str();
    if (params.size() > 3 && params[3].type() != null_type && !params[3].get_str().empty())
        wtx.mapValue["to"]      = params[3].get_str();
    
    if (_wallet.IsLocked())
        throw RPC::error(RPC::invalid_request, "Error: Please enter the wallet passphrase with walletpassphrase first.");
    
    string strError = _wallet.SendMoneyToBitcoinAddress(address, nAmount, wtx);
    if (strError != "")
        throw RPC::error(RPC::invalid_request, strError);
    
    return wtx.getHash().GetHex();
}

ChainAddress GetAccountAddress::getAccountAddress(string strAccount, bool bForceNew)
{
    CWalletDB walletdb(_wallet._dataDir, _wallet.strWalletFile);
    
    CAccount account;
    walletdb.ReadAccount(strAccount, account);
    
    bool bKeyUsed = false;
    
    // Check if the current key has been used
    if (!account.vchPubKey.empty()) {
        Script scriptPubKey;
        scriptPubKey.setChainAddress(_wallet.chain().networkId(), account.vchPubKey);
        for (map<uint256, CWalletTx>::iterator it = _wallet.mapWallet.begin();
             it != _wallet.mapWallet.end() && !account.vchPubKey.empty();
             ++it) {
            const CWalletTx& wtx = (*it).second;
            BOOST_FOREACH(const Output& txout, wtx.getOutputs())
            if (txout.script() == scriptPubKey)
                bKeyUsed = true;
        }
    }
    
    // Generate a new key
    if (account.vchPubKey.empty() || bForceNew || bKeyUsed) {
        if (!_wallet.GetKeyFromPool(account.vchPubKey, false))
            throw RPC::error(RPC::internal_error, "Error: Keypool ran out, please call keypoolrefill first");
        
        _wallet.SetAddressBookName(ChainAddress(_wallet.chain().networkId(), account.vchPubKey), strAccount);
        walletdb.WriteAccount(strAccount, account);
    }
    
    return ChainAddress(_wallet.chain().networkId(), account.vchPubKey);
}

Value GetAccountAddress::operator()(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw RPC::error(RPC::invalid_params, "getaccountaddress <account>\n"
                            "Returns the current bitcoin address for receiving payments to this account.");
    
    // Parse the account first so we don't generate a key if there's an error
    string strAccount = AccountFromValue(params[0]);
    
    Value ret;
    
    ret = getAccountAddress(strAccount).toString();
    
    return ret;
}

Value SetAccount::operator()(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw RPC::error(RPC::invalid_params, "setaccount <bitcoinaddress> <account>\n"
                            "Sets the account associated with the given address.");
    
    ChainAddress address(params[0].get_str());
    if (!address.isValid(_wallet.chain().networkId()))
        throw RPC::error(RPC::invalid_params, "Invalid bitcoin address");
    
    
    string strAccount;
    if (params.size() > 1)
        strAccount = AccountFromValue(params[1]);
    
    // Detect when changing the account of an address that is the 'unused current key' of another account:
    if (_wallet.mapAddressBook.count(address)) {
        string strOldAccount = _wallet.mapAddressBook[address];
        if (address == getAccountAddress(strOldAccount))
            getAccountAddress(strOldAccount, true);
    }
    
    _wallet.SetAddressBookName(address, strAccount);
    
    return Value::null;
}


Value GetAccount::operator()(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw RPC::error(RPC::invalid_params, "getaccount <bitcoinaddress>\n"
                            "Returns the account associated with the given address.");
    
    ChainAddress address(params[0].get_str());
    if (!address.isValid(_wallet.chain().networkId()))
        throw RPC::error(RPC::invalid_params, "Invalid bitcoin address");
    
    string strAccount;
    map<ChainAddress, string>::iterator mi = _wallet.mapAddressBook.find(address);
    if (mi != _wallet.mapAddressBook.end() && !(*mi).second.empty())
        strAccount = (*mi).second;
    return strAccount;
}

Value GetAddressesByAccount::operator()(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw RPC::error(RPC::invalid_params, "getaddressesbyaccount <account>\n"
                            "Returns the list of addresses for the given account.");
    
    string strAccount = AccountFromValue(params[0]);
    
    // Find all addresses that have the given account
    Array ret;
    BOOST_FOREACH(const PAIRTYPE(ChainAddress, string)& item, _wallet.mapAddressBook) {
        const ChainAddress& address = item.first;
        const string& strName = item.second;
        if (strName == strAccount)
            ret.push_back(address.toString());
    }
    return ret;
}

Value SetTxFee::operator()(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1)
        throw RPC::error(RPC::invalid_params, "settxfee <amount>\n"
                            "<amount> is a real and is rounded to the nearest 0.00000001");
    
    // Amount
    int64 nAmount = 0;
    if (params[0].get_real() != 0.0)
        nAmount = AmountFromValue(params[0]);        // rejects 0.0 amounts
    
    _wallet.nTransactionFee = nAmount;
    return true;
}

Value GetReceivedByAddress::operator()(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw RPC::error(RPC::invalid_params, "getreceivedbyaddress <bitcoinaddress> [minconf=1]\n"
                            "Returns the total amount received by <bitcoinaddress> in transactions with at least [minconf] confirmations.");
    
    // Bitcoin address
    ChainAddress address = ChainAddress(params[0].get_str());
    Script scriptPubKey;
    if (!address.isValid(_wallet.chain().networkId()))
        throw RPC::error(RPC::invalid_params, "Invalid bitcoin address");
    scriptPubKey.setChainAddress(address);
    if (!IsMine(_wallet,scriptPubKey))
        return (double)0.0;
    
    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();
    
    // Tally
    int64 nAmount = 0;
    for (map<uint256, CWalletTx>::iterator it = _wallet.mapWallet.begin(); it != _wallet.mapWallet.end(); ++it) {
        const CWalletTx& wtx = (*it).second;
        if (wtx.isCoinBase() || !_wallet.isFinal(wtx))
            continue;
        
        BOOST_FOREACH(const Output& txout, wtx.getOutputs())
        if (txout.script() == scriptPubKey)
            if (_wallet.getDepthInMainChain(wtx.getHash()) >= nMinDepth)
                nAmount += txout.value();
    }
    
    return  ValueFromAmount(nAmount);
}

Value GetReceivedByAccount::operator()(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw RPC::error(RPC::invalid_params, "getreceivedbyaccount <account> [minconf=1]\n"
                            "Returns the total amount received by addresses with <account> in transactions with at least [minconf] confirmations.");
    
    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();
    
    // Get the set of pub keys that have the label
    string strAccount = AccountFromValue(params[0]);
    set<ChainAddress> setAddress;
    BOOST_FOREACH(const PAIRTYPE(ChainAddress, string)& item, _wallet.mapAddressBook) {
        const ChainAddress& address = item.first;
        const string& strName = item.second;
        if (strName == strAccount)
            setAddress.insert(address);
    }
    
    // Tally
    int64 nAmount = 0;
    for (map<uint256, CWalletTx>::iterator it = _wallet.mapWallet.begin(); it != _wallet.mapWallet.end(); ++it) {
        const CWalletTx& wtx = (*it).second;
        if (wtx.isCoinBase() || !_wallet.isFinal(wtx))
            continue;
        
        BOOST_FOREACH(const Output& txout, wtx.getOutputs()) {
            Address address;
            if (ExtractAddress(txout.script(), &_wallet, address) && setAddress.count(ChainAddress(_wallet.chain().networkId(), address)))
                if (_wallet.getDepthInMainChain(wtx.getHash()) >= nMinDepth)
                    nAmount += txout.value();
        }
    }
    
    return (double)nAmount / (double)COIN;
}

Value MoveCmd::operator()(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 5)
        throw RPC::error(RPC::invalid_params, "move <fromaccount> <toaccount> <amount> [minconf=1] [comment]\n"
                            "Move from one account in your wallet to another.");
    
    string strFrom = AccountFromValue(params[0]);
    string strTo = AccountFromValue(params[1]);
    int64 nAmount = AmountFromValue(params[2]);
    if (params.size() > 3)
        // unused parameter, used to be nMinDepth, keep type-checking it though
        (void)params[3].get_int();
    string strComment;
    if (params.size() > 4)
        strComment = params[4].get_str();
    
    CWalletDB walletdb(_wallet._dataDir, _wallet.strWalletFile);
    walletdb.TxnBegin();
    
    int64 nNow = GetAdjustedTime();
    
    // Debit
    CAccountingEntry debit;
    debit.strAccount = strFrom;
    debit.nCreditDebit = -nAmount;
    debit.nTime = nNow;
    debit.strOtherAccount = strTo;
    debit.strComment = strComment;
    walletdb.WriteAccountingEntry(debit);
    
    // Credit
    CAccountingEntry credit;
    credit.strAccount = strTo;
    credit.nCreditDebit = nAmount;
    credit.nTime = nNow;
    credit.strOtherAccount = strFrom;
    credit.strComment = strComment;
    walletdb.WriteAccountingEntry(credit);
    
    walletdb.TxnCommit();
    
    return true;
}

Value SendFrom::operator()(const Array& params, bool fHelp)
{
    if (_wallet.IsCrypted() && (fHelp || params.size() < 3 || params.size() > 6))
        throw RPC::error(RPC::invalid_params, "sendfrom <fromaccount> <tobitcoinaddress> <amount> [minconf=1] [comment] [comment-to]\n"
                            "<amount> is a real and is rounded to the nearest 0.00000001\n"
                            "requires wallet passphrase to be set with walletpassphrase first");
    if (!_wallet.IsCrypted() && (fHelp || params.size() < 3 || params.size() > 6))
        throw RPC::error(RPC::invalid_params, "sendfrom <fromaccount> <tobitcoinaddress> <amount> [minconf=1] [comment] [comment-to]\n"
                            "<amount> is a real and is rounded to the nearest 0.00000001");
    
    string strAccount = AccountFromValue(params[0]);
    ChainAddress address(params[1].get_str());
    if (!address.isValid(_wallet.chain().networkId()))
        throw RPC::error(RPC::invalid_params, "Invalid bitcoin address");
    int64 nAmount = AmountFromValue(params[2]);
    int nMinDepth = 1;
    if (params.size() > 3)
        nMinDepth = params[3].get_int();
    
    CWalletTx wtx;
    wtx.strFromAccount = strAccount;
    if (params.size() > 4 && params[4].type() != null_type && !params[4].get_str().empty())
        wtx.mapValue["comment"] = params[4].get_str();
    if (params.size() > 5 && params[5].type() != null_type && !params[5].get_str().empty())
        wtx.mapValue["to"]      = params[5].get_str();
    
    if (_wallet.IsLocked())
        throw RPC::error(RPC::invalid_request, "Error: Please enter the wallet passphrase with walletpassphrase first.");
    
    // Check funds
    int64 nBalance = GetAccountBalance(strAccount, nMinDepth);
    if (nAmount > nBalance)
        throw RPC::error(RPC::invalid_request, "Account has insufficient funds");
    
    // Send
    string strError = _wallet.SendMoneyToBitcoinAddress(address, nAmount, wtx);
    if (strError != "")
        throw RPC::error(RPC::invalid_request, strError);
    
    return wtx.getHash().GetHex();
}


Value SendMany::operator()(const Array& params, bool fHelp)
{
    if (_wallet.IsCrypted() && (fHelp || params.size() < 2 || params.size() > 4))
        throw RPC::error(RPC::invalid_params, "sendmany <fromaccount> {address:amount,...} [minconf=1] [comment]\n"
                            "amounts are double-precision floating point numbers\n"
                            "requires wallet passphrase to be set with walletpassphrase first");
    if (!_wallet.IsCrypted() && (fHelp || params.size() < 2 || params.size() > 4))
        throw RPC::error(RPC::invalid_params, "sendmany <fromaccount> {address:amount,...} [minconf=1] [comment]\n"
                            "amounts are double-precision floating point numbers");
    
    string strAccount = AccountFromValue(params[0]);
    Object sendTo = params[1].get_obj();
    int nMinDepth = 1;
    if (params.size() > 2)
        nMinDepth = params[2].get_int();
    
    CWalletTx wtx;
    wtx.strFromAccount = strAccount;
    if (params.size() > 3 && params[3].type() != null_type && !params[3].get_str().empty())
        wtx.mapValue["comment"] = params[3].get_str();
    
    set<ChainAddress> setAddress;
    vector<pair<Script, int64> > vecSend;
    
    int64 totalAmount = 0;
    BOOST_FOREACH(const Pair& s, sendTo)
    {
    ChainAddress address(s.name_);
    if (!address.isValid(_wallet.chain().networkId()))
        throw RPC::error(RPC::invalid_params, string("Invalid bitcoin address:")+s.name_);
    
    if (setAddress.count(address))
        throw RPC::error(RPC::invalid_params, string("Invalid parameter, duplicated address: ")+s.name_);
    setAddress.insert(address);
    
    Script scriptPubKey;
    scriptPubKey.setChainAddress(address);
    int64 nAmount = AmountFromValue(s.value_); 
    totalAmount += nAmount;
    
    vecSend.push_back(make_pair(scriptPubKey, nAmount));
    }
    
    if (_wallet.IsLocked())
        throw RPC::error(RPC::invalid_request, "Error: Please enter the wallet passphrase with walletpassphrase first.");
    
    // Check funds
    int64 nBalance = GetAccountBalance(strAccount, nMinDepth);
    if (totalAmount > nBalance)
        throw RPC::error(RPC::invalid_request, "Account has insufficient funds");
    
    // Send
    CReserveKey keyChange(&_wallet);
    int64 nFeeRequired = 0;
    bool fCreated = _wallet.CreateTransaction(vecSend, wtx, keyChange, nFeeRequired);
    if (!fCreated)
        {
        if (totalAmount + nFeeRequired > _wallet.GetBalance())
            throw RPC::error(RPC::invalid_request, "Insufficient funds");
        throw RPC::error(RPC::internal_error, "Transaction creation failed");
        }
    if (!_wallet.CommitTransaction(wtx, keyChange))
        throw RPC::error(RPC::internal_error, "Transaction commit failed");
    
    return wtx.getHash().GetHex();
}

// Convenience class for the ListMethod base class
struct tallyitem
{
    int64 nAmount;
    int nConf;
    tallyitem()
    {
    nAmount = 0;
    nConf = INT_MAX;
    }
};

void ListMethod::listTransactions(const CWalletTx& wtx, const string& strAccount, int nMinDepth, bool fLong, Array& ret)
{
    int64 nGeneratedImmature, nGeneratedMature, nFee;
    string strSentAccount;
    list<pair<ChainAddress, int64> > listReceived;
    list<pair<ChainAddress, int64> > listSent;
    wtx.GetAmounts(nGeneratedImmature, nGeneratedMature, listReceived, listSent, nFee, strSentAccount);
    
    bool fAllAccounts = (strAccount == string("*"));
    
    // Generated blocks assigned to account ""
    if ((nGeneratedMature+nGeneratedImmature) != 0 && (fAllAccounts || strAccount == ""))
        {
        Object entry;
        entry.push_back(Pair("account", string("")));
        if (nGeneratedImmature)
            {
            entry.push_back(Pair("category", _wallet.getDepthInMainChain(wtx.getHash()) ? "immature" : "orphan"));
            entry.push_back(Pair("amount", ValueFromAmount(nGeneratedImmature)));
            }
        else
            {
            entry.push_back(Pair("category", "generate"));
            entry.push_back(Pair("amount", ValueFromAmount(nGeneratedMature)));
            }
        if (fLong)
            walletTxToJSON(wtx, entry);
        ret.push_back(entry);
        }
    
    // Sent
    if ((!listSent.empty() || nFee != 0) && (fAllAccounts || strAccount == strSentAccount))
        {
        BOOST_FOREACH(const PAIRTYPE(ChainAddress, int64)& s, listSent)
            {
            Object entry;
            entry.push_back(Pair("account", strSentAccount));
            entry.push_back(Pair("address", s.first.toString()));
            entry.push_back(Pair("category", "send"));
            entry.push_back(Pair("amount", ValueFromAmount(-s.second)));
            entry.push_back(Pair("fee", ValueFromAmount(-nFee)));
            if (fLong)
                walletTxToJSON(wtx, entry);
            ret.push_back(entry);
            }
        }
    
    // Received
    if (listReceived.size() > 0 && _wallet.getDepthInMainChain(wtx.getHash()) >= nMinDepth)
        BOOST_FOREACH(const PAIRTYPE(ChainAddress, int64)& r, listReceived)
        {
        string account;
        if (_wallet.mapAddressBook.count(r.first))
            account = _wallet.mapAddressBook[r.first];
        if (fAllAccounts || (account == strAccount))
            {
            Object entry;
            entry.push_back(Pair("account", account));
            entry.push_back(Pair("address", r.first.toString()));
            entry.push_back(Pair("category", "receive"));
            entry.push_back(Pair("amount", ValueFromAmount(r.second)));
            if (fLong)
                walletTxToJSON(wtx, entry);
            ret.push_back(entry);
            }
        }
}

void ListMethod::acEntryToJSON(const CAccountingEntry& acentry, const string& strAccount, Array& ret)
{
    bool fAllAccounts = (strAccount == string("*"));
    
    if (fAllAccounts || acentry.strAccount == strAccount)
        {
        Object entry;
        entry.push_back(Pair("account", acentry.strAccount));
        entry.push_back(Pair("category", "move"));
        entry.push_back(Pair("time", (boost::int64_t)acentry.nTime));
        entry.push_back(Pair("amount", ValueFromAmount(acentry.nCreditDebit)));
        entry.push_back(Pair("otheraccount", acentry.strOtherAccount));
        entry.push_back(Pair("comment", acentry.strComment));
        ret.push_back(entry);
        }
}

void ListMethod::walletTxToJSON(const CWalletTx& wtx, Object& entry)
{
    entry.push_back(Pair("confirmations", _wallet.getDepthInMainChain(wtx.getHash())));
    entry.push_back(Pair("txid", wtx.getHash().GetHex()));
    entry.push_back(Pair("time", (boost::int64_t)wtx.GetTxTime()));
    BOOST_FOREACH(const PAIRTYPE(string,string)& item, wtx.mapValue)
    entry.push_back(Pair(item.first, item.second));
}

Value ListMethod::listReceived(const Array& params, bool fByAccounts)
{
    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();
    
    // Whether to include empty accounts
    bool fIncludeEmpty = false;
    if (params.size() > 1)
        fIncludeEmpty = params[1].get_bool();
    
    // Tally
    map<ChainAddress, tallyitem> mapTally;
    for (map<uint256, CWalletTx>::iterator it = _wallet.mapWallet.begin(); it != _wallet.mapWallet.end(); ++it)
        {
        const CWalletTx& wtx = (*it).second;
        if (wtx.isCoinBase() || !_wallet.isFinal(wtx))
            continue;
        
        int nDepth = _wallet.getDepthInMainChain(wtx.getHash());
        if (nDepth < nMinDepth)
            continue;
        
        BOOST_FOREACH(const Output& txout, wtx.getOutputs())
            {
            Address address;
            if (!ExtractAddress(txout.script(), &_wallet, address) || address != 0)
                continue;
            
            tallyitem& item = mapTally[ChainAddress(_wallet.chain().networkId(), address)];
            item.nAmount += txout.value();
            item.nConf = min(item.nConf, nDepth);
            }
        }
    
    // Reply
    Array ret;
    map<string, tallyitem> mapAccountTally;
    BOOST_FOREACH(const PAIRTYPE(ChainAddress, string)& item, _wallet.mapAddressBook) {
    const ChainAddress& address = item.first;
    const string& strAccount = item.second;
    map<ChainAddress, tallyitem>::iterator it = mapTally.find(address);
    if (it == mapTally.end() && !fIncludeEmpty)
        continue;
    
    int64 nAmount = 0;
    int nConf = INT_MAX;
    if (it != mapTally.end())
        {
        nAmount = (*it).second.nAmount;
        nConf = (*it).second.nConf;
        }
    
    if (fByAccounts)
        {
        tallyitem& item = mapAccountTally[strAccount];
        item.nAmount += nAmount;
        item.nConf = min(item.nConf, nConf);
        }
    else
        {
        Object obj;
        obj.push_back(Pair("address",       address.toString()));
        obj.push_back(Pair("account",       strAccount));
        obj.push_back(Pair("label",         strAccount)); // deprecated
        obj.push_back(Pair("amount",        ValueFromAmount(nAmount)));
        obj.push_back(Pair("confirmations", (nConf == INT_MAX ? 0 : nConf)));
        ret.push_back(obj);
        }
    }
    
    if (fByAccounts)
        {
        for (map<string, tallyitem>::iterator it = mapAccountTally.begin(); it != mapAccountTally.end(); ++it)
            {
            int64 nAmount = (*it).second.nAmount;
            int nConf = (*it).second.nConf;
            Object obj;
            obj.push_back(Pair("account",       (*it).first));
            obj.push_back(Pair("label",         (*it).first)); // deprecated
            obj.push_back(Pair("amount",        ValueFromAmount(nAmount)));
            obj.push_back(Pair("confirmations", (nConf == INT_MAX ? 0 : nConf)));
            ret.push_back(obj);
            }
        }
    
    return ret;
}

Value ListReceivedByAddress::operator()(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw RPC::error(RPC::invalid_params, "listreceivedbyaddress [minconf=1] [includeempty=false]\n"
                         "[minconf] is the minimum number of confirmations before payments are included.\n"
                         "[includeempty] whether to include addresses that haven't received any payments.\n"
                         "Returns an array of objects containing:\n"
                         "  \"address\" : receiving address\n"
                         "  \"account\" : the account of the receiving address\n"
                         "  \"amount\" : total amount received by the address\n"
                         "  \"confirmations\" : number of confirmations of the most recent transaction included");
    
    return listReceived(params, false);
}

Value ListReceivedByAccount::operator()(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw RPC::error(RPC::invalid_params, "listreceivedbyaccount [minconf=1] [includeempty=false]\n"
                            "[minconf] is the minimum number of confirmations before payments are included.\n"
                            "[includeempty] whether to include accounts that haven't received any payments.\n"
                            "Returns an array of objects containing:\n"
                            "  \"account\" : the account of the receiving addresses\n"
                            "  \"amount\" : total amount received by addresses with this account\n"
                            "  \"confirmations\" : number of confirmations of the most recent transaction included");
    
    return listReceived(params, true);
}

Value ListTransactions::operator()(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw RPC::error(RPC::invalid_params, "listtransactions [account] [count=10] [from=0]\n"
                            "Returns up to [count] most recent transactions skipping the first [from] transactions for account [account].");
    
    string strAccount = "*";
    if (params.size() > 0)
        strAccount = params[0].get_str();
    int nCount = 10;
    if (params.size() > 1)
        nCount = params[1].get_int();
    int nFrom = 0;
    if (params.size() > 2)
        nFrom = params[2].get_int();
    
    Array ret;
    CWalletDB walletdb(_wallet._dataDir, _wallet.strWalletFile);
    
    // Firs: get all CWalletTx and CAccountingEntry into a sorted-by-time multimap:
    typedef pair<CWalletTx*, CAccountingEntry*> TxPair;
    typedef multimap<int64, TxPair > TxItems;
    TxItems txByTime;
    
    for (map<uint256, CWalletTx>::iterator it = _wallet.mapWallet.begin(); it != _wallet.mapWallet.end(); ++it)
        {
        CWalletTx* wtx = &((*it).second);
        txByTime.insert(make_pair(wtx->GetTxTime(), TxPair(wtx, (CAccountingEntry*)0)));
        }
    list<CAccountingEntry> acentries;
    walletdb.ListAccountCreditDebit(strAccount, acentries);
    BOOST_FOREACH(CAccountingEntry& entry, acentries)
    {
    txByTime.insert(make_pair(entry.nTime, TxPair((CWalletTx*)0, &entry)));
    }
    
    // Now: iterate backwards until we have nCount items to return:
    TxItems::reverse_iterator it = txByTime.rbegin();
    if (txByTime.size() > nFrom) std::advance(it, nFrom);
    for (; it != txByTime.rend(); ++it)
        {
        CWalletTx *const pwtx = (*it).second.first;
        if (pwtx != 0)
            listTransactions(*pwtx, strAccount, 0, true, ret);
        CAccountingEntry *const pacentry = (*it).second.second;
        if (pacentry != 0)
            acEntryToJSON(*pacentry, strAccount, ret);
        
        if (ret.size() >= nCount) break;
        }
    // ret is now newest to oldest
    
    // Make sure we return only last nCount items (sends-to-self might give us an extra):
    if (ret.size() > nCount)
        {
        Array::iterator last = ret.begin();
        std::advance(last, nCount);
        ret.erase(last, ret.end());
        }
    std::reverse(ret.begin(), ret.end()); // oldest to newest
    
    return ret;
}

Value ListAccounts::operator()(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw RPC::error(RPC::invalid_params, "listaccounts [minconf=1]\n"
                            "Returns Object that has account names as keys, account balances as values.");
    
    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();
    
    map<string, int64> mapAccountBalances;
    BOOST_FOREACH(const PAIRTYPE(ChainAddress, string)& entry, _wallet.mapAddressBook) {
        if (_wallet.HaveKey(entry.first)) // This address belongs to me
            mapAccountBalances[entry.second] = 0;
    }
    
    for (map<uint256, CWalletTx>::iterator it = _wallet.mapWallet.begin(); it != _wallet.mapWallet.end(); ++it) {
        const CWalletTx& wtx = (*it).second;
        int64 nGeneratedImmature, nGeneratedMature, nFee;
        string strSentAccount;
        list<pair<ChainAddress, int64> > listReceived;
        list<pair<ChainAddress, int64> > listSent;
        wtx.GetAmounts(nGeneratedImmature, nGeneratedMature, listReceived, listSent, nFee, strSentAccount);
        mapAccountBalances[strSentAccount] -= nFee;
        BOOST_FOREACH(const PAIRTYPE(ChainAddress, int64)& s, listSent)
        mapAccountBalances[strSentAccount] -= s.second;
        if (_wallet.getDepthInMainChain(wtx.getHash()) >= nMinDepth) {
            mapAccountBalances[""] += nGeneratedMature;
            BOOST_FOREACH(const PAIRTYPE(ChainAddress, int64)& r, listReceived)
            if (_wallet.mapAddressBook.count(r.first))
                mapAccountBalances[_wallet.mapAddressBook[r.first]] += r.second;
            else
                mapAccountBalances[""] += r.second;
        }
    }

    list<CAccountingEntry> acentries;
    CWalletDB(_wallet._dataDir, _wallet.strWalletFile).ListAccountCreditDebit("*", acentries);
    BOOST_FOREACH(const CAccountingEntry& entry, acentries)
    mapAccountBalances[entry.strAccount] += entry.nCreditDebit;
    
    Object ret;
    BOOST_FOREACH(const PAIRTYPE(string, int64)& accountBalance, mapAccountBalances) {
        ret.push_back(Pair(accountBalance.first, ValueFromAmount(accountBalance.second)));
    }
    return ret;
}

Value GetTransaction::operator()(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw RPC::error(RPC::invalid_params, "gettransaction <txid>\n"
                            "Get detailed information about <txid>");
    
    uint256 hash;
    hash.SetHex(params[0].get_str());
    
    Object entry;
    
    if (!_wallet.mapWallet.count(hash))
        throw RPC::error(RPC::invalid_params, "Invalid or non-wallet transaction id");
    const CWalletTx& wtx = _wallet.mapWallet[hash];
    
    int64 nCredit = wtx.GetCredit();
    int64 nDebit = wtx.GetDebit();
    int64 nNet = nCredit - nDebit;
    int64 nFee = (wtx.IsFromMe() ? wtx.getValueOut() - nDebit : 0);
    
    entry.push_back(Pair("amount", ValueFromAmount(nNet - nFee)));
    if (wtx.IsFromMe())
        entry.push_back(Pair("fee", ValueFromAmount(nFee)));
    
    walletTxToJSON(_wallet.mapWallet[hash], entry);
    
    Array details;
    listTransactions(_wallet.mapWallet[hash], "*", 0, false, details);
    entry.push_back(Pair("details", details));
    
    return entry;
}

Value BackupWallet::operator()(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw RPC::error(RPC::invalid_params, "backupwallet <destination>\n"
                            "Safely copies wallet.dat to destination, which can be a directory or a path with filename.");
    
    string strDest = params[0].get_str();
    ::BackupWallet(_wallet, strDest);
    
    return Value::null;
}

Value KeypoolRefill::operator()(const Array& params, bool fHelp)
{
    if (_wallet.IsCrypted() && (fHelp || params.size() > 0))
        throw RPC::error(RPC::internal_error, "keypoolrefill\n"
                            "Fills the keypool, requires wallet passphrase to be set.");
    if (!_wallet.IsCrypted() && (fHelp || params.size() > 0))
        throw RPC::error(RPC::invalid_params, "keypoolrefill\n"
                            "Fills the keypool.");
    
    if (_wallet.IsLocked())
        throw RPC::error(RPC::internal_error, "Error: Please enter the wallet passphrase with walletpassphrase first.");
    
    _wallet.TopUpKeyPool();
    
    //    if (_wallet.GetKeyPoolSize() < GetArg("-keypool", 100))
    //        throw RPC::error(RPC::internal_error, "Error refreshing keypool.");
    
    return Value::null;
}

Value WalletPassphrase::operator()(const Array& params, bool fHelp)
{
    if (_wallet.IsCrypted() && (fHelp || params.size() != 2))
        throw RPC::error(RPC::invalid_params, "walletpassphrase <passphrase> <timeout>\n"
                            "Stores the wallet decryption key in memory for <timeout> seconds.");
    if (fHelp)
        return true;
    if (!_wallet.IsCrypted())
        throw RPC::error(RPC::invalid_request, "Error: running with an unencrypted wallet, but walletpassphrase was called.");
    
    if (!_wallet.IsLocked())
        throw RPC::error(RPC::invalid_request, "Error: Wallet is already unlocked.");
    
    if (params[0].type() != json_spirit::str_type)
        throw RPC::error(RPC::invalid_params, "Error: expect a password formatted as string.");
        
    int timeout = 60;
    if (params[1].type() != json_spirit::int_type) {
        if (params[1].type() == json_spirit::str_type)
            timeout = lexical_cast<int>(params[1].get_str());
    }        
    else
        timeout = params[1].get_int();
    
    // Note that the walletpassphrase is stored in params[0] which is not mlock()ed
    string strWalletPass;
    strWalletPass.reserve(100);
    mlock(&strWalletPass[0], strWalletPass.capacity());
    strWalletPass = params[0].get_str();
    
    if (strWalletPass.length() > 0) {
        if (!_wallet.Unlock(strWalletPass)) {
            fill(strWalletPass.begin(), strWalletPass.end(), '\0');
            munlock(&strWalletPass[0], strWalletPass.capacity());
            throw RPC::error(RPC::invalid_request, "Error: The wallet passphrase entered was incorrect.");
        }
        fill(strWalletPass.begin(), strWalletPass.end(), '\0');
        munlock(&strWalletPass[0], strWalletPass.capacity());
    }
    else
        throw RPC::error(RPC::invalid_params, "walletpassphrase <passphrase> <timeout>\n"
                            "Stores the wallet decryption key in memory for <timeout> seconds.");
    
    //    CreateThread(ThreadTopUpKeyPool, NULL);
    //    int* pnSleepTime = new int(params[1].get_int());
    //    CreateThread(ThreadCleanWalletPassphrase, pnSleepTime);
    
    _lock_timer.expires_from_now(boost::posix_time::seconds(timeout));
    _lock_timer.async_wait(boost::bind(&Wallet::Lock, &_wallet));
    
    return Value::null;
}

Value WalletPassphraseChange::operator()(const Array& params, bool fHelp)
{
    if (_wallet.IsCrypted() && (fHelp || params.size() != 2))
        throw RPC::error(RPC::invalid_params, "walletpassphrasechange <oldpassphrase> <newpassphrase>\n"
                            "Changes the wallet passphrase from <oldpassphrase> to <newpassphrase>.");
    if (fHelp)
        return true;
    if (!_wallet.IsCrypted())
        throw RPC::error(RPC::invalid_request, "Error: running with an unencrypted wallet, but walletpassphrasechange was called.");
    
    string strOldWalletPass;
    strOldWalletPass.reserve(100);
    mlock(&strOldWalletPass[0], strOldWalletPass.capacity());
    strOldWalletPass = params[0].get_str();
    
    string strNewWalletPass;
    strNewWalletPass.reserve(100);
    mlock(&strNewWalletPass[0], strNewWalletPass.capacity());
    strNewWalletPass = params[1].get_str();
    
    if (strOldWalletPass.length() < 1 || strNewWalletPass.length() < 1)
        throw RPC::error(RPC::invalid_params, "walletpassphrasechange <oldpassphrase> <newpassphrase>\n"
                            "Changes the wallet passphrase from <oldpassphrase> to <newpassphrase>.");
    
    if (!_wallet.ChangeWalletPassphrase(strOldWalletPass, strNewWalletPass)) {
        fill(strOldWalletPass.begin(), strOldWalletPass.end(), '\0');
        fill(strNewWalletPass.begin(), strNewWalletPass.end(), '\0');
        munlock(&strOldWalletPass[0], strOldWalletPass.capacity());
        munlock(&strNewWalletPass[0], strNewWalletPass.capacity());
        throw RPC::error(RPC::invalid_request, "Error: The wallet passphrase entered was incorrect.");
    }
    fill(strNewWalletPass.begin(), strNewWalletPass.end(), '\0');
    fill(strOldWalletPass.begin(), strOldWalletPass.end(), '\0');
    munlock(&strOldWalletPass[0], strOldWalletPass.capacity());
    munlock(&strNewWalletPass[0], strNewWalletPass.capacity());
    
    return Value::null;
}

Value WalletLock::operator()(const Array& params, bool fHelp)
{
    if (_wallet.IsCrypted() && (fHelp || params.size() != 0))
        throw RPC::error(RPC::invalid_params, "walletlock\n"
                            "Removes the wallet encryption key from memory, locking the wallet.\n"
                            "After calling this method, you will need to call walletpassphrase again\n"
                            "before being able to call any methods which require the wallet to be unlocked.");
    if (fHelp)
        return true;
    if (!_wallet.IsCrypted())
        throw RPC::error(RPC::invalid_request, "Error: running with an unencrypted wallet, but walletlock was called.");
    
    _wallet.Lock();
    
    return Value::null;
}


Value EncryptWallet::operator()(const Array& params, bool fHelp)
{
    if (!_wallet.IsCrypted() && (fHelp || params.size() != 1))
        throw RPC::error(RPC::invalid_params, "encryptwallet <passphrase>\n"
                            "Encrypts the wallet with <passphrase>.");
    if (fHelp)
        return true;
    if (_wallet.IsCrypted())
        throw RPC::error(RPC::invalid_request, "Error: running with an encrypted wallet, but encryptwallet was called.");
    
    string strWalletPass;
    strWalletPass.reserve(100);
    mlock(&strWalletPass[0], strWalletPass.capacity());
    strWalletPass = params[0].get_str();
    
    if (strWalletPass.length() < 1)
        throw runtime_error(
                            "encryptwallet <passphrase>\n"
                            "Encrypts the wallet with <passphrase>.");
    
    if (!_wallet.EncryptWallet(strWalletPass)) {
        fill(strWalletPass.begin(), strWalletPass.end(), '\0');
        munlock(&strWalletPass[0], strWalletPass.capacity());
        throw RPC::error(RPC::internal_error, "Error: Failed to encrypt the wallet.");
    }
    fill(strWalletPass.begin(), strWalletPass.end(), '\0');
    munlock(&strWalletPass[0], strWalletPass.capacity());
    
    return Value::null;
}


Value ValidateAddress::operator()(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw RPC::error(RPC::invalid_params, "validateaddress <bitcoinaddress>\n"
                            "Return information about <bitcoinaddress>.");
    
    ChainAddress address(params[0].get_str());
    bool isValid = address.isValid(_wallet.chain().networkId());
    
    Object ret;
    ret.push_back(Pair("isvalid", isValid));
    if (isValid) {
        // Call Hash160ToAddress() so we always return current ADDRESSVERSION
        // version of the address:
        string currentAddress = address.toString();
        ret.push_back(Pair("address", currentAddress));
        ret.push_back(Pair("ismine", (_wallet.HaveKey(address) > 0)));
        if (_wallet.mapAddressBook.count(address))
            ret.push_back(Pair("account", _wallet.mapAddressBook[address]));
    }
    return ret;
}
