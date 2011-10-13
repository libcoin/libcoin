// Copyright (c) 2011 Michael Gronager
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "btcNode/db.h"
#include "btcRPC/rpc.h"

#include "asset.h"

using namespace std;
using namespace boost;
using namespace json_spirit;

Value gettxmaturity(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                            "gettxmaturity <txhash>\n"
                            "Get transaction maturity as 3 numbers: confirmations, known inventory, in nodes");

    unsigned int confirmations, known_in_nodes, n_nodes;
    
    uint256 hash;
    hash.SetHex(params[0].get_str());
    
    CTxDB txdb("r");
    
    CTxIndex txindex;
    if(!txdb.ReadTxIndex(hash, txindex)) // confirmation is 0, check the other maturity
    {
        confirmations = 0;
        CInv inv(MSG_TX, hash);
        CRITICAL_BLOCK(cs_vNodes)
        {
            n_nodes = vNodes.size();
            known_in_nodes = 0;
            for(int n = 0; n < n_nodes; n++)
            {
                CRITICAL_BLOCK(vNodes[n]->cs_inventory)
                    if(vNodes[n]->setInventoryKnown.count(inv))
                        known_in_nodes++;
            }
        }
    }
    else
    {
        n_nodes = vNodes.size();
        known_in_nodes = 0;
// now get # block the tx is in.
        confirmations = txindex.GetDepthInMainChain();
    }

    Object entry;
    entry.push_back(Pair("confirmations", uint64_t(confirmations)));
    entry.push_back(Pair("known_in_nodes", uint64_t(known_in_nodes)));
    entry.push_back(Pair("of_nodes", uint64_t(n_nodes)));
    
    return entry;
}

Value gettxdetails(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                            "gettxdetails <txhash>\n"
                            "Get transaction details for transaction with hash <txhash>");
    
    uint256 hash;
    hash.SetHex(params[0].get_str());
    
    CTxDB txdb("r");
    
    CTransaction tx;
    if(!txdb.ReadDiskTx(hash, tx))
        throw JSONRPCError(-5, "Invalid transaction id");
    
    Object entry;
    
    // scheme follows the scheme of blockexplorer:
    // "hash" : hash in hex
    // "ver" : vernum
    entry.push_back(Pair("hash", hash.ToString()));
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
    
    return entry;    
}

Value getvalue(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                            "getvalue <btcaddr>\n"
                            "Get value of <btcaddr>");
    
    uint160 hash160 = CBitcoinAddress(params[0].get_str()).GetHash160();
    
    CAsset asset;
    asset.addAddress(hash160);
    CTxDB txdb;
    CDBAssetSyncronizer sync(txdb);
    asset.syncronize(sync);
    int64 balance = asset.balance();
    
    Value val(balance);
    
    return val;
}

Value getdebit(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                            "getdebit <btcaddr>\n"
                            "Get debit coins of <btcaddr>");
    uint160 hash160 = CBitcoinAddress(params[0].get_str()).GetHash160();
    
    CTxDB txdb("r");
    
    set<Coin> debit;
    
    txdb.ReadDrIndex(hash160, debit);
    
    Array list;
    
    for(set<Coin>::iterator coin = debit.begin(); coin != debit.end(); ++coin)
    {
        Object obj;
        obj.clear();
        obj.push_back(Pair("hash", coin->first.ToString()));
        obj.push_back(Pair("n", uint64_t(coin->second)));
        list.push_back(obj);
    }
    
    return list;
}

Value getcredit(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                            "getcredit <btcaddr>\n"
                            "Get credit coins of <btcaddr>");
    uint160 hash160 = CBitcoinAddress(params[0].get_str()).GetHash160();
    
    CTxDB txdb("r");
    
    set<Coin> credit;
    
    txdb.ReadCrIndex(hash160, credit);
    
    Array list;
    
    for(set<Coin>::iterator coin = credit.begin(); coin != credit.end(); ++coin)
    {
        Object obj;
        obj.clear();
        obj.push_back(Pair("hash", coin->first.ToString()));
        obj.push_back(Pair("n", uint64_t(coin->second)));
        list.push_back(obj);
    }
    
    return list;
}

Value getcoins(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                            "getcoins <btcaddr>\n"
                            "Get non spend coins of <btcaddr>");
    
    uint160 hash160 = CBitcoinAddress(params[0].get_str()).GetHash160();
    
    CAsset asset;
    asset.addAddress(hash160);
    CTxDB txdb;
    CDBAssetSyncronizer sync(txdb);
    asset.syncronize(sync);
    set<Coin> coins = asset.getCoins();
    
    Array list;
    
    for(set<Coin>::iterator coin = coins.begin(); coin != coins.end(); ++coin)
    {
        Object obj;
        obj.clear();
        obj.push_back(Pair("hash", coin->first.ToString()));
        obj.push_back(Pair("n", uint64_t(coin->second)));
        list.push_back(obj);
    }
    
    return list;
}

Value posttx(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                            "posttx <tx>\n"
                            "Post a signed transaction json"); // note this is a new transaction and hence it has no hash!
    
    Object entry = params[0].get_obj();
    
    CTransaction tx;
    
    // find first the version
    tx.nVersion = find_value(entry, "ver").get_int();
    // then the lock_time
    tx.nLockTime = find_value(entry, "lock_time").get_int();
    
    // then find the vins and outs
    Array txins = find_value(entry, "in").get_array();
    Array txouts = find_value(entry, "out").get_array();
    
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
    
    // now we have read the transaction - we need verify and possibly post it to the p2p network
    // We create a fake node and pretend we got this from the network - this ensures that this is handled by the right thread...
    CNode* pnode = new CNode(INVALID_SOCKET, CAddress("localhost"));
    
    CDataStream ss;
    ss << tx;
    CMessageHeader header("tx", ss.size());
    uint256 hash = Hash(ss.begin(), ss.begin() + ss.size());
    unsigned int nChecksum;
    memcpy(&nChecksum, &hash, sizeof(nChecksum));    
    header.nChecksum = nChecksum;
    pnode->vRecv << header;
    pnode->vRecv << tx;
    pnode->nVersion = VERSION;
    
    CRITICAL_BLOCK(cs_vNodes)
    {
        pnode->nTimeConnected = GetTime();
        pnode->nLastRecv = GetTime();
        pnode->AddRef();
        vNodes.push_back(pnode); // add the node to the node list - it will then get polled for its content and purged at the next occation
    }
    
    return Value::null;
}
