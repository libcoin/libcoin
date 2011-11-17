// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
//#include "headers.h"
#include "btc/strlcpy.h"

#include "btcNode/db.h"
#include "btcNode/net.h"

#include "btcRPC/rpc.h"

#include "btcWallet/wallet.h"
#include "btcWallet/walletrpc.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/interprocess/sync/file_lock.hpp>

#include <boost/asio.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/algorithm/string.hpp>

#include <signal.h>

using namespace std;
using namespace boost;
using namespace boost::asio;
using namespace json_spirit;

typedef void wxWindow;
#define wxYES                   0x00000002
#define wxOK                    0x00000004
#define wxNO                    0x00000008
#define wxYES_NO                (wxYES|wxNO)
#define wxCANCEL                0x00000010
#define wxAPPLY                 0x00000020
#define wxCLOSE                 0x00000040
#define wxOK_DEFAULT            0x00000000
#define wxYES_DEFAULT           0x00000000
#define wxNO_DEFAULT            0x00000080
#define wxCANCEL_DEFAULT        0x80000000
#define wxICON_EXCLAMATION      0x00000100
#define wxICON_HAND             0x00000200
#define wxICON_WARNING          wxICON_EXCLAMATION
#define wxICON_ERROR            wxICON_HAND
#define wxICON_QUESTION         0x00000400
#define wxICON_INFORMATION      0x00000800
#define wxICON_STOP             wxICON_HAND
#define wxICON_ASTERISK         wxICON_INFORMATION
#define wxICON_MASK             (0x00000100|0x00000200|0x00000400|0x00000800)
#define wxFORWARD               0x00001000
#define wxBACKWARD              0x00002000
#define wxRESET                 0x00004000
#define wxHELP                  0x00008000
#define wxMORE                  0x00010000
#define wxSETUP                 0x00020000

inline int MyMessageBox(const std::string& message, const std::string& caption="Message", int style=wxOK, wxWindow* parent=NULL, int x=-1, int y=-1)
{
    printf("%s: %s\n", caption.c_str(), message.c_str());
    fprintf(stderr, "%s: %s\n", caption.c_str(), message.c_str());
    return 4;
}
#define wxMessageBox  MyMessageBox

#define DEFAULT "__default__"

/*
 if(asset_name.size() == 0)
 asset_name = DEFAULT;
 
 // browse the wallet folder for an address, move it to the asset folder and return it
 filesystem::path btc_path = "~/.bitcoin";
 filesystem::path wallet_path = "~/.bitcoin/wallet/";
 
 if(!filesystem::exists(btc_path))
 {
 // create .bitcoin
 filesystem::create_directory(btc_path);
 }
 
 if(!filesystem::exists(wallet_path))
 {
 // create .bitcoin
 filesystem::create_directory(wallet_path);
 }
 
 map<time_t, filesystem::path> new_keys;
 for (filesystem::path::iterator it = wallet_path.begin(); it != wallet_path.end(); it++) {
 if(filesystem::is_directory(*it))
 continue;
 new_keys.insert(make_pair(last_write_time(*it), *it));
 }
 
 if(new_keys.size() == 0)
 {
 // create new keys - top up with 100
 }
 
 */

Object callRPC(const string& strMethod, const Array& params);

class CRPCAssetSyncronizer : public CAssetSyncronizer
{
public:
    CRPCAssetSyncronizer(string server = "btc.ceptacle.com:8332") : _server(server) {}
    
    virtual void getCreditCoins(uint160 btc, Coins& coins)
    {
        Array p;
        p.push_back(CBitcoinAddress(btc).ToString());
        
        Object reply = callRPC("getcredit", p);
        
        Array txlist = find_value(reply, "result").get_array();
        
        for(Array::iterator it = txlist.begin(); it != txlist.end(); ++it)
            coins.insert(make_pair(uint256(find_value(it->get_obj(), "hash").get_str()), find_value(it->get_obj(), "n").get_int()));
    }
    
    virtual void getDebitCoins(uint160 btc, Coins& coins)
    {
        Array p;
        p.push_back(CBitcoinAddress(btc).ToString());
        
        Object reply = callRPC("getdebit", p);
        
        Array txlist = find_value(reply, "result").get_array();
        
        for(Array::iterator it = txlist.begin(); it != txlist.end(); ++it)
            coins.insert(make_pair(uint256(find_value(it->get_obj(), "hash").get_str()), find_value(it->get_obj(), "n").get_int()));
    }

    virtual void getCoins(uint160 btc, Coins& coins)
    {
        Array p;
        p.push_back(CBitcoinAddress(btc).ToString());
        
        Object reply = callRPC("getcoins", p);
        
        Array txlist = find_value(reply, "result").get_array();
        
        for(Array::iterator it = txlist.begin(); it != txlist.end(); ++it)
            coins.insert(make_pair(uint256(find_value(it->get_obj(), "hash").get_str()), find_value(it->get_obj(), "n").get_int()));
    }
    
    virtual void getTransaction(const Coin& coin, CTx& tx)
    {
        Array p;
        p.push_back(coin.first.ToString());
        Object reply = callRPC("gettxdetails", p);
        
        Object txdetails = find_value(reply, "result").get_obj();
        
        // find first the version
        tx.nVersion = find_value(txdetails, "ver").get_int();
        // then the lock_time
        tx.nLockTime = find_value(txdetails, "lock_time").get_int();
        
        // then find the vins and outs
        Array txins = find_value(txdetails, "in").get_array();
        Array txouts = find_value(txdetails, "out").get_array();
        
        map<string, opcodetype> opcodemap; // the following will create a map opcodeName -> opCode, the opcodeName='OP_UNKNOWN' = 0xfc (last code with no name)
        for(unsigned char c = 0; c < UCHAR_MAX; c++) opcodemap[string(GetOpName(opcodetype(c)))] = (opcodetype)c;
        
        for(Array::iterator itin = txins.begin(); itin != txins.end(); ++itin)
        {
            Object in = itin->get_obj();
            Object prev_out = find_value(in, "prev_out").get_obj();
            uint256 hash(find_value(prev_out, "hash").get_str());
            unsigned int n = find_value(prev_out, "n").get_int();
            CScript scriptSig;
            string sscript = find_value(in, "scriptSig").get_str();
            // traverse through the vector and pushback opcodes and values
            istringstream iss(sscript);
            string token;
            while(iss >> token)
            {
                map<string, opcodetype>::iterator opcode = opcodemap.find(token);
                if(opcode != opcodemap.end()) // opcode read
                    scriptSig << opcode->second;
                else // value read
                    scriptSig << ParseHex(token);
            }
            CTxIn txin(hash, n, scriptSig);
            tx.vin.push_back(txin);
        }
        
        for(Array::iterator itout = txouts.begin(); itout != txouts.end(); ++itout)
        {
            Object out = itout->get_obj();
            string strvalue = find_value(out, "value").get_str();
            //parse string value...
            istringstream ivs(strvalue);
            double dvalue;
            ivs >> dvalue;
            dvalue *= COIN;
            int64 value = dvalue;
            CScript scriptPubkey;
            string sscript = find_value(out, "scriptPubKey").get_str();
            // traverse through the vector and pushback opcodes and values
            istringstream iss(sscript);
            string token;
            while(iss >> token)
            {
                map<string, opcodetype>::iterator opcode = opcodemap.find(token);
                if(opcode != opcodemap.end()) // opcode read
                    scriptPubkey << opcode->second;
                else // value read
                    scriptPubkey << ParseHex(token);
            }
            CTxOut txout(value, scriptPubkey);
            tx.vout.push_back(txout);
        }
        
    }
 
private:
    string _server;
};
 
uint160 getNewAddress(string asset_name)
{
    
    if (!pwalletMain->IsLocked())
        pwalletMain->TopUpKeyPool();
    
    // Generate a new key that is added to wallet
    std::vector<unsigned char> newKey;
    if (!pwalletMain->GetKeyFromPool(newKey, false)) // Error: Keypool ran out, please call keypoolrefill first
        return 0;
    CBitcoinAddress address(newKey);
    
    pwalletMain->SetAddressBookName(address, asset_name);
    
    return address.GetHash160();
}

uint160 getAccountAddress(string asset_name, bool bForceNew=false)
{
    CWalletDB walletdb(pwalletMain->strWalletFile);
    
    CAccount account;
    walletdb.ReadAccount(asset_name, account);
    
    bool bKeyUsed = false;
    
    // Check if the current key has been used
    if (!account.vchPubKey.empty())
    {
        CScript scriptPubKey;
        scriptPubKey.SetBitcoinAddress(account.vchPubKey);
        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin();
             it != pwalletMain->mapWallet.end() && !account.vchPubKey.empty();
             ++it)
        {
            const CWalletTx& wtx = (*it).second;
            BOOST_FOREACH(const CTxOut& txout, wtx.vout)
            if (txout.scriptPubKey == scriptPubKey)
                bKeyUsed = true;
        }
    }
    
    // Generate a new key
    if (account.vchPubKey.empty() || bForceNew || bKeyUsed)
    {
        if (!pwalletMain->GetKeyFromPool(account.vchPubKey, false)) // Error: Keypool ran out, please call keypoolrefill first
            return 0;
        
        pwalletMain->SetAddressBookName(CBitcoinAddress(account.vchPubKey), asset_name);
        walletdb.WriteAccount(asset_name, account);
    }
    
    return CBitcoinAddress(account.vchPubKey).GetHash160();
}

void setAccount(string btcaddr, string asset_name)
{
    CBitcoinAddress address(btcaddr);
    if (!address.IsValid())
        return; // Invalid bitcoin address
    
    // Detect when changing the account of an address that is the 'unused current key' of another account:
    if (pwalletMain->mapAddressBook.count(address))
    {
        string old_asset_name = pwalletMain->mapAddressBook[address];
        if (address.GetHash160() == getAccountAddress(old_asset_name))
            getAccountAddress(old_asset_name, true);
    }
    
    pwalletMain->SetAddressBookName(address, asset_name);
}

string getAccount(string btcaddr)
{
    CBitcoinAddress address(btcaddr);
    if (!address.IsValid())
        return ""; // Invalid bitcoin address
    
    string asset_name;
    map<CBitcoinAddress, string>::iterator mi = pwalletMain->mapAddressBook.find(address);
    if (mi != pwalletMain->mapAddressBook.end() && !(*mi).second.empty())
        asset_name = (*mi).second;
    return asset_name;
}

double getReceivedByAddress(uint160 btcaddr)
{
    // getcredit(btcaddr)
    // gettxdetails(tx'es);
}

double getReceivedByAccount(string account)
{
    // getcredit(btcaddr)
    // gettxdetails(tx'es);
}

Value getrecvbyaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
                            "getreceivedbyaddress <bitcoinaddress> [minconf=1]\n"
                            "Returns the total amount received by <bitcoinaddress> in transactions with at least [minconf] confirmations.");
    
    // Bitcoin address
    CBitcoinAddress address = CBitcoinAddress(params[0].get_str());
    CScript scriptPubKey;
    if (!address.IsValid())
        throw JSONRPCError(-5, "Invalid bitcoin address");
    scriptPubKey.SetBitcoinAddress(address);
    if (!IsMine(*pwalletMain,scriptPubKey))
        return (double)0.0;
    
    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();
    
    // now we need to call the server... - first create the input
    Array p;
    p.push_back(address.ToString());

    Object reply = callRPC("getdebit", p);
    
    Array txlist = find_value(reply, "result").get_array();

    // now iterate over the tx-es to get tx details from each
    double sum = 0;
    for(Array::iterator it = txlist.begin(); it != txlist.end(); ++it)
    {
        Array p;
        p.push_back(find_value(it->get_obj(), "hash"));
        Object reply = callRPC("gettxdetails", p);

        Object txdetails = find_value(reply, "result").get_obj();
        
        Array txouts = find_value(txdetails, "out").get_array();
        Object txout = txouts[find_value(it->get_obj(), "n").get_int()].get_obj();

        double amount = 0;
        istringstream(find_value(txout, "value").get_str()) >> amount;
        
        sum += amount;
    }
    
    return sum;
}

void getAccountAddresses(string strAccount, set<CBitcoinAddress>& setAddress)
{
    BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, string)& item, pwalletMain->mapAddressBook)
    {
        const CBitcoinAddress& address = item.first;
        const string& strName = item.second;
        if (strName == strAccount)
            setAddress.insert(address);
    }
}


Value getrecvbyaccount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
                            "getreceivedbyaccount <account> [minconf=1]\n"
                            "Returns the total amount received by addresses with <account> in transactions with at least [minconf] confirmations.");
    
    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();
    
    // Get the set of pub keys that have the label
    string account = AccountFromValue(params[0]);
    set<CBitcoinAddress> addresses;
    getAccountAddresses(account, addresses);
    
    // now we need to call the server... - first create the input
    Array txlists;        
    for(set<CBitcoinAddress>::iterator it = addresses.begin(); it!= addresses.end(); ++it)
    {
        Array p;
        p.push_back(it->ToString());
        
        Object reply = callRPC("getdebit", p);
        
        Array txlist = find_value(reply, "result").get_array();
        for (Array::iterator ai = txlist.begin(); ai != txlist.end(); ++ai) {
            txlists.push_back(*ai);
        }
    }
    
    // Tally
    // now iterate over the tx-es to get tx details from each
    double sum = 0;
    for(Array::iterator it = txlists.begin(); it != txlists.end(); ++it)
    {
        Array p;
        p.push_back(find_value(it->get_obj(), "hash"));
        Object reply = callRPC("gettxdetails", p);
        
        Object txdetails = find_value(reply, "result").get_obj();
        
        Array txouts = find_value(txdetails, "out").get_array();
        Object txout = txouts[find_value(it->get_obj(), "n").get_int()].get_obj();
        
        double amount = 0;
        istringstream(find_value(txout, "value").get_str()) >> amount;
        
        sum += amount;
    }
    
    return sum;
}

Value getaccountbalance(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
                            "getbalance [account] [minconf=1]\n"
                            "If [account] is not specified, returns the server's total available balance.\n"
                            "If [account] is specified, returns the balance in the account.");
    
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();

    set<CBitcoinAddress> addresses;
    if (params.size() == 0 || params[0].get_str() == "*") {
        // Calculate total balance a different way from GetBalance()
        // (GetBalance() sums up all unspent TxOuts)
        // getbalance and getbalance '*' should always return the same number.
        BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, string)& item, pwalletMain->mapAddressBook)
        {
            const CBitcoinAddress& address = item.first;
            CScript scriptPubKey;
            scriptPubKey.SetBitcoinAddress(address);
            if (!IsMine(*pwalletMain,scriptPubKey))
                continue;

            addresses.insert(address);
        }
    }
    else
    {
        string account = params[0].get_str();
        BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, string)& item, pwalletMain->mapAddressBook)
        {
            const CBitcoinAddress& address = item.first;
            CScript scriptPubKey;
            scriptPubKey.SetBitcoinAddress(address);
            if (!IsMine(*pwalletMain,scriptPubKey))
                continue;
            const string& label = item.second;
            if (label == account)
                addresses.insert(address);
        }

    }

    // trying to use the CAssetSyncronizer and CAsset instead...
    CAsset asset;
    for(set<CBitcoinAddress>::iterator it = addresses.begin(); it!= addresses.end(); ++it)
        asset.addAddress(it->GetHash160());

    CRPCAssetSyncronizer rpc;
    asset.syncronize(rpc);
    
    // now just ask for the balance...
    int64 balance = asset.balance();
    return balance*1.0/COIN;
    
    int64 sum = 0;
    for(set<CBitcoinAddress>::iterator it = addresses.begin(); it!= addresses.end(); ++it)
    {
        Array p;
        p.push_back(it->ToString());
        
        Object reply = callRPC("getvalue", p);
        
        int64 amount = find_value(reply, "result").get_int64();
        sum += amount;
    }
    
    return ValueFromAmount(sum);
}

Value listaccnts(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                            "listaccounts [minconf=1]\n"
                            "Returns Object that has account names as keys, account balances as values.");
    
    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();
    
    map<string, int64> mapAccountBalances;
    BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, string)& entry, pwalletMain->mapAddressBook) {
        if (pwalletMain->HaveKey(entry.first)) // This address belongs to me
        {
            Array p;
            p.push_back(entry.first.ToString());
            
            Object reply = callRPC("getvalue", p);
            
            int64 amount = find_value(reply, "result").get_int64();

            if(mapAccountBalances.count(entry.second))
                mapAccountBalances[entry.second] = mapAccountBalances[entry.second] + amount;
            else
                mapAccountBalances[entry.second] = amount;
        }
    }
    
    list<CAccountingEntry> acentries;
    CWalletDB(pwalletMain->strWalletFile).ListAccountCreditDebit("*", acentries);
    BOOST_FOREACH(const CAccountingEntry& entry, acentries)
    mapAccountBalances[entry.strAccount] += entry.nCreditDebit;
    
    Object ret;
    BOOST_FOREACH(const PAIRTYPE(string, int64)& accountBalance, mapAccountBalances) {
        ret.push_back(Pair(accountBalance.first, ValueFromAmount(accountBalance.second)));
    }
    return ret;
}

Value signhashwithkey(const Array& params, bool fHelp)
{
    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 2))
        throw runtime_error(
                            "signhashwithkey <hash> <key>\n"
                            "<amount> is a real and is rounded to the nearest 0.00000001\n"
                            "requires wallet passphrase to be set with walletpassphrase first");
    Array result;

    uint256 hash(params[0].get_str());
    vector<unsigned char> secret = ParseHex(params[1].get_str());
    CSecret real_secret;
    for(vector<unsigned char>::iterator i = secret.begin(); i != secret.end(); ++i)
        real_secret.push_back(*i);
    CKey key;
    key.SetSecret(real_secret);

    vector<unsigned char> signature;
    key.Sign(hash, signature);
    reverse(signature.begin(), signature.end());
    signature.push_back(0);
    CBigNum bn = CBigNum(signature);
    
    result.push_back(bn.GetHex());

    result.push_back(key.GetAddress().ToString());
    
    return result;    
}

Value getprivatekeys(const Array& params, bool fHelp)
{
    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 0))
        throw runtime_error(
                            "sendtoaddress <bitcoinaddress> <amount> [comment] [comment-to]\n"
                            "<amount> is a real and is rounded to the nearest 0.00000001\n"
                            "requires wallet passphrase to be set with walletpassphrase first");
    Array result;
    
    BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, string)& item, pwalletMain->mapAddressBook)
    {
        const CBitcoinAddress& address = item.first;
        CScript scriptPubKey;
        scriptPubKey.SetBitcoinAddress(address);
        if (!IsMine(*pwalletMain,scriptPubKey))
            continue;
        const string& label = item.second;
    
        CKey key;
        pwalletMain->GetKey(address, key);
        CSecret secret= key.GetSecret();
        vector<unsigned char> whistle;
        for (CSecret::iterator it = secret.begin(); it != secret.end(); ++it) {
            whistle.push_back(*it); 
        }
        reverse(whistle.begin(),whistle.end());
        if(whistle.size())
            whistle.push_back(0);
        CBigNum bn = CBigNum(whistle);
        result.push_back(bn.GetHex());
    }

    return result;
}

Value sendtoaddr(const Array& params, bool fHelp)
{
    if (pwalletMain->IsCrypted() && (fHelp || params.size() < 2 || params.size() > 4))
        throw runtime_error(
                            "sendtoaddress <bitcoinaddress> <amount> [comment] [comment-to]\n"
                            "<amount> is a real and is rounded to the nearest 0.00000001\n"
                            "requires wallet passphrase to be set with walletpassphrase first");
    if (!pwalletMain->IsCrypted() && (fHelp || params.size() < 2 || params.size() > 4))
        throw runtime_error(
                            "sendtoaddress <bitcoinaddress> <amount> [comment] [comment-to]\n"
                            "<amount> is a real and is rounded to the nearest 0.00000001");
    
    CBitcoinAddress to_address(params[0].get_str());
    if (!to_address.IsValid())
        throw JSONRPCError(-5, "Invalid bitcoin address");
    
    // Amount
    int64 amount = AmountFromValue(params[1]);
    
    if (pwalletMain->IsLocked())
        throw JSONRPCError(-13, "Error: Please enter the wallet passphrase with walletpassphrase first.");

    // first we need to load an asset with the right addresses - my addresses, from the wallet, further, we need to ensure that the addresses are all loaded with a private key (making them spendable)
    
    set<CBitcoinAddress> addresses;
    string account;
//    if (params.size() == 0 || params[0].get_str() == "*")
        account.clear();
//    else
//        account = params[0].get_str();
        
    CAsset asset;
    
    BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, string)& item, pwalletMain->mapAddressBook)
    {
        const CBitcoinAddress& address = item.first;
        CScript scriptPubKey;
        scriptPubKey.SetBitcoinAddress(address);
        if (!IsMine(*pwalletMain,scriptPubKey))
            continue;
        const string& label = item.second;
        if (account.size() == 0 || label == account)
        {
            CKey key;
            pwalletMain->GetKey(address, key);
            asset.addKey(key);
        }
    }

    // load the asset with coins
    CRPCAssetSyncronizer rpc;
    asset.syncronize(rpc);

    CTx tx;    
    set<CAsset::Payment> payments;
    payments.insert(CAsset::Payment(to_address.GetHash160(), amount));
    CBitcoinAddress btcbroker("19bvWMvxddxbDrrN6kXZxqhZsApfVFDxB6");
    payments.insert(CAsset::Payment(btcbroker.GetHash160(), amount/10)); // 10% in brokerfee!
//    tx = asset.generateTx(to_address.GetHash160(), amount);
    tx = asset.generateTx(payments);
    
    Object entry;
    
    // scheme follows the scheme of blockexplorer:
    // "hash" : hash in hex
    // "ver" : vernum
    entry.push_back(Pair("hash", tx.GetHash().ToString()));
    entry.push_back(Pair("ver", tx.nVersion));
    entry.push_back(Pair("vin_sz", uint64_t(tx.vin.size())));
    entry.push_back(Pair("vout_sz", uint64_t(tx.vout.size())));
    entry.push_back(Pair("lock_time", uint64_t(tx.nLockTime)));
    entry.push_back(Pair("size", uint64_t(::GetSerializeSize(tx, SER_NETWORK))));
    
    // now loop over the txins
    Array txins;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        Object inentry;
        inentry.clear();
        Object prevout;
        prevout.clear();
        prevout.push_back(Pair("hash", txin.prevout.hash.ToString()));
        prevout.push_back(Pair("n", uint64_t(txin.prevout.n)));
        inentry.push_back(Pair("prev_out", prevout));
        if(tx.IsCoinBase())            
            inentry.push_back(Pair("coinbase", txin.scriptSig.ToString()));
        else
            inentry.push_back(Pair("scriptSig", txin.scriptSig.ToString()));
        txins.push_back(inentry);
    }
    entry.push_back(Pair("in", txins));
    
    // now loop over the txouts
    Array txouts;
    BOOST_FOREACH(const CTxOut& txout, tx.vout)
    {
        Object outentry;
        outentry.clear();
        outentry.push_back(Pair("value", strprintf("%"PRI64d".%08"PRI64d"",txout.nValue/COIN, txout.nValue%COIN))); // format correctly
        outentry.push_back(Pair("scriptPubKey", txout.scriptPubKey.ToString()));
        txouts.push_back(outentry);
    }
    entry.push_back(Pair("out", txouts));
    
    Array p;
    p.push_back(entry);
    Object reply = callRPC("posttx", p);
    
    cout << write_formatted(entry) << endl;
    
    return tx.GetHash().GetHex();
}


Value ExecRPC(const string& strMethod, const Array& params)
{
    map<string, rpcfn_type>::iterator mi = mapCallTable.find(strMethod);
    rpcfn_type pfn = mi->second;
    Value ret = (*pfn)(params, false);
    return ret;
}


Object callRPC(const string& strMethod, const Array& params)
{
    // Connect to localhost
    bool fUseSSL = false;//GetBoolArg("-rpcssl");
#ifdef USE_SSL
    asio::io_service io_service;
    ssl::context context(io_service, ssl::context::sslv23);
    context.set_options(ssl::context::no_sslv2);
    SSLStream sslStream(io_service, context);
    SSLIOStreamDevice d(sslStream, fUseSSL);
    iostreams::stream<SSLIOStreamDevice> stream(d);
//    if (!d.connect("btc.ceptacle.com", "8332"))
    if (!d.connect("localhost", "9332"))
        throw runtime_error("couldn't connect to server");
#else
    if (fUseSSL)
        throw runtime_error("-rpcssl=1, but bitcoin compiled without full openssl libraries.");
    
//    ip::tcp::iostream stream("btc.ceptacle.com", "8332");
    ip::tcp::iostream stream("localhost", "9332");
    if (stream.fail())
        throw runtime_error("couldn't connect to server");
#endif
    
    
    // HTTP basic authentication
    string strUserPass64 = EncodeBase64("rpcuser:coined");
    map<string, string> mapRequestHeaders;
    mapRequestHeaders["Authorization"] = string("Basic ") + strUserPass64;
    
    // Send request
    string strRequest = JSONRPCRequest(strMethod, params, 1);
    string strPost = HTTPPost(strRequest, mapRequestHeaders);
    stream << strPost << std::flush;
    
    // Receive reply
    map<string, string> mapHeaders;
    string strReply;
    int nStatus = ReadHTTP(stream, mapHeaders, strReply);
    if (nStatus == 401)
        throw runtime_error("incorrect rpcuser or rpcpassword (authorization failed)");
    else if (nStatus >= 400 && nStatus != 400 && nStatus != 404 && nStatus != 500)
        throw runtime_error(strprintf("server returned HTTP error %d", nStatus));
    else if (strReply.empty())
        throw runtime_error("no response from server");
    
    // Parse reply
    Value valReply;
    if (!json_spirit::read(strReply, valReply))
        throw runtime_error("couldn't parse reply from server");
    const Object& reply = valReply.get_obj();
    if (reply.empty())
        throw runtime_error("expected reply to have result, error and id properties");
    
    return reply;
}



int main(int argc, char* argv[])
{
//    mapCallTable.insert(make_pair("syncwallet",             &syncwallet));
    mapCallTable.insert(make_pair("getnewaddress",          &getnewaddress)); // OK
    mapCallTable.insert(make_pair("getaccountaddress",      &getaccountaddress)); // OK
    mapCallTable.insert(make_pair("setaccount",             &setaccount)); // OK
    mapCallTable.insert(make_pair("getaccount",             &getaccount)); // OK
    mapCallTable.insert(make_pair("getaddressesbyaccount",  &getaddressesbyaccount)); // OK
    mapCallTable.insert(make_pair("sendtoaddress",          &sendtoaddr));
    mapCallTable.insert(make_pair("getreceivedbyaddress",   &getrecvbyaddress));
    mapCallTable.insert(make_pair("getreceivedbyaccount",   &getrecvbyaccount));
    mapCallTable.insert(make_pair("listreceivedbyaddress",  &listreceivedbyaddress));
    mapCallTable.insert(make_pair("listreceivedbyaccount",  &listreceivedbyaccount));
    mapCallTable.insert(make_pair("backupwallet",           &backupwallet));
    mapCallTable.insert(make_pair("keypoolrefill",          &keypoolrefill));
    mapCallTable.insert(make_pair("walletpassphrase",       &walletpassphrase));
    mapCallTable.insert(make_pair("walletpassphrasechange", &walletpassphrasechange));
    mapCallTable.insert(make_pair("walletlock",             &walletlock));
    mapCallTable.insert(make_pair("encryptwallet",          &encryptwallet));
    mapCallTable.insert(make_pair("validateaddress",        &validateaddress));
    mapCallTable.insert(make_pair("getbalance",             &getaccountbalance));
    mapCallTable.insert(make_pair("move",                   &movecmd));
    mapCallTable.insert(make_pair("sendfrom",               &sendfrom));
    mapCallTable.insert(make_pair("sendmany",               &sendmany));
    mapCallTable.insert(make_pair("gettransaction",         &gettransaction));
    mapCallTable.insert(make_pair("listtransactions",       &listtransactions));
    mapCallTable.insert(make_pair("listaccounts",           &listaccnts));
    mapCallTable.insert(make_pair("settxfee",               &settxfee));
    mapCallTable.insert(make_pair("getkeys",               &getprivatekeys));
    mapCallTable.insert(make_pair("signhash",               &signhashwithkey));
    
    vector<string> args;
    do args.push_back(*(argv++)); while(--argc);
    
// open wallet and assign it to pwalletmain.
    bool fFirstRun;
    string strPrint, strErrors;
    pwalletMain = new CWallet("wallet.dat");
    int nLoadWalletRet = pwalletMain->LoadWallet(fFirstRun);
    if (nLoadWalletRet != DB_LOAD_OK)
    {
        if (nLoadWalletRet == DB_CORRUPT)
            strErrors += _("Error loading wallet.dat: Wallet corrupted      \n");
        else if (nLoadWalletRet == DB_TOO_NEW)
            strErrors += _("Error loading wallet.dat: Wallet requires newer version of Bitcoin      \n");
        else
            strErrors += _("Error loading wallet.dat      \n");
    }

// check if argument #1 is in the list of allowed arguments - else exit with error+help message
    if(args.size() < 2)
    {
        cerr << "No command given" << endl;
        return 1;
    }
        
    if(mapCallTable.find(args[1]) == mapCallTable.end())
    {
        cerr << "\"" << args[1] << "\" Not a valid command" << endl;
        return 1;
    }
    
    // run sync to sync with server
// if argument #1 is in the list, use ExecRPC (as opposed to CallRPC) to run the rpc call
    Array params;
    for (int i = 2; i < args.size(); i++)
        params.push_back(args[i]);
    
    // Execute
    const Value& result = ExecRPC(args[1], params);
    
    strPrint = write_formatted(result);
    
    cout << strPrint << endl;

    return 0;
}
