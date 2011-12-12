// Copyright (c) 2011 Michael Gronager
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "btcNode/db.h"
#include "btcNode/BlockChain.h"

#include "btc/asset.h"

#include "btcHTTP/RPC.h"

#include "proxy.h"

using namespace std;
using namespace boost;
using namespace json_spirit;

#define BTCBROKER "19bvWMvxddxbDrrN6kXZxqhZsApfVFDxB6"

double reliability(uint256 hash) {
    unsigned int confirmations, known_in_nodes, n_nodes;
    CTxDB txdb("r");
    
    TxIndex txindex;
    if(!txdb.ReadTxIndex(hash, txindex)) { // confirmation is 0, check the other maturity
        confirmations = 0;
        Inventory inv(MSG_TX, hash);
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
    else {
        n_nodes = vNodes.size();
        known_in_nodes = 0;
        // now get # block the tx is in.
        confirmations = txindex.getDepthInMainChain();
    }
    
    if(confirmations > 0)
        return confirmations;
    else if(n_nodes > 50) // require at least 50 nodes!
        return 1./n_nodes * known_in_nodes;
    else
        return 0.;        
}

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
    
    TxIndex txindex;
    if(!txdb.ReadTxIndex(hash, txindex)) // confirmation is 0, check the other maturity
    {
        confirmations = 0;
        Inventory inv(MSG_TX, hash);
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
        confirmations = txindex.getDepthInMainChain();
    }

    Object entry;
    entry.push_back(Pair("confirmations", uint64_t(confirmations)));
    entry.push_back(Pair("known_in_nodes", uint64_t(known_in_nodes)));
    entry.push_back(Pair("of_nodes", uint64_t(n_nodes)));
    
    return entry;
}

Object tx2json(CTx &tx, int64 timestamp = 0, int64 blockheight = 0)
{
    Object entry;
    
    // scheme follows the scheme of blockexplorer:
    // "hash" : hash in hex
    // "ver" : vernum
    uint256 hash = tx.GetHash();
    entry.push_back(Pair("timestamp", timestamp));
    entry.push_back(Pair("blockheight", blockheight));
    entry.push_back(Pair("hash", hash.toString()));
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
        prevout.push_back(Pair("hash", txin.prevout.hash.toString()));
        prevout.push_back(Pair("n", uint64_t(txin.prevout.n)));
        inentry.push_back(Pair("prev_out", prevout));
        if(tx.IsCoinBase())            
            inentry.push_back(Pair("coinbase", txin.scriptSig.toString()));
        else
            inentry.push_back(Pair("scriptSig", txin.scriptSig.toString()));
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
        outentry.push_back(Pair("scriptPubKey", txout.scriptPubKey.toString()));
        txouts.push_back(outentry);
    }
    entry.push_back(Pair("out", txouts));

    return entry;
}

Value GetTxDetails::operator()(const Array& params, bool fHelp) {

    if (fHelp || params.size() != 1)
        throw runtime_error(
                            "gettxdetails <txhash>\n"
                            "Get transaction details for transaction with hash <txhash>");
    
    uint256 hash;
    hash.SetHex(params[0].get_str());
    
    CTxDB txdb("r");

    int64 timestamp = 0;
    int64 blockheight = 0;
    
    TxIndex txindex;
    if(txdb.ReadTxIndex(hash, txindex)) { // Read block header
        blockheight = 1 + nBestHeight - txindex.getDepthInMainChain();

        Block block;
        if (__blockFile.readFromDisk(block, txindex.getPos().getFile(), txindex.getPos().getBlockPos(), false)) {
        // Find the block in the index
            timestamp = block.getBlockTime();
        }
    }

    CTransaction tx;
    if(!txdb.ReadDiskTx(hash, tx))
    {
        CRITICAL_BLOCK(cs_mapTransactions)
        {
            if(mapTransactions.count(hash))
                tx = mapTransactions[hash];
            else
                throw RPC::error(RPC::invalid_params, "Invalid transaction id");        
        }
    }
/*    
    CDataStream ss(SER_GETHASH, VERSION);
    ss.reserve(10000);
    ss << tx;

    for(int i = 0; i < ss.size(); i++)
        cout << (int)(unsigned char)ss[i] << ",";
    cout << endl;
*/    
    
    Object entry = tx2json(tx, timestamp, blockheight);
    
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
    asset.syncronize(sync, true);
    int64 balance = asset.balance();
    
    Value val(balance);
    
    return val;
}

Value checkvalue(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 4)
        throw runtime_error(
                            "checkvalue <btcaddr> [[[<within>] <amount>] <confirmation>]\n"
                            "check the value of a bitcoin address, <within> seconds, get confirmations of specific <amount> or get a true/false of a minimum confirmation level");

    // parse the input
    CBitcoinAddress addr = CBitcoinAddress(params[0].get_str());
    if(!addr.IsValid())
        throw runtime_error("getdebit <btcaddr>\n"
                            "btcaddr invalid!");
    uint160 hash160 = addr.GetHash160();
    
    int within = 0;
    if(params.size() > 1)
        within = params[1].get_int();
        
    uint64 amount = 0;
    if(params.size() > 2)
        amount = params[2].get_uint64();
    
    double confirmation = 0;
    if(params.size() > 3)
        confirmation = params[3].get_real();
    
    // now we have a all the params - we need to do some querying

    CAsset asset;
    asset.addAddress(hash160); // only one address in this asset
    CTxDB txdb;
    CDBAssetSyncronizer sync(txdb);
    asset.syncronize(sync, true); // we now have all tx'es (incl those in memory! - we need to sort them according to time)
    
    Coins coins = asset.getCoins();
    Coins coins_within; // this is the set of coins within <within> seconds
    int64 now = GetTime();
    if(within > 0) { // we only want a subset
        for(Coins::iterator coin = coins.begin(); coin != coins.end(); ++coin) {
            // now get the txes belonging to this coin!
            TxIndex txindex;
            if(txdb.ReadTxIndex(coin->first, txindex)) { // Read block header
                int blockheight = 1 + nBestHeight - txindex.getDepthInMainChain();
                int64 timestamp = 0;
                Block block;
                if (__blockFile.readFromDisk(block, txindex.getPos().getFile(), txindex.getPos().getBlockPos(), false)) {
                    // Find the block in the index
                    timestamp = block.getBlockTime();
                }
                else
                    continue;
                cout << now-timestamp << endl;
                if(now-timestamp < within)
                    coins_within.insert(*coin);
                else
                    continue;
            }
            else { // this is a memory tx
                   // does memory txes have a timestamp ? // nope - until we have a proper interface we will assume that all keys in memory are within the timeinterval (which should hence be larger than 10 minutes...)
                if(mapTransactions.count(coin->first)) {
                    coins_within.insert(*coin);
                }
                else
                    continue;
                
            }
        }
    }
    else {
        for(Coins::iterator coin = coins.begin(); coin != coins.end(); ++coin)
            coins_within.insert(*coin);
    }
    
    // now find the balance for the set of coins_within
    int64 balance = 0;
    double min_reliability = HUGE;// coins_within.size() ? reliability(coins_within.begin()->first) : 0;
    for(Coins::iterator coin = coins_within.begin(); coin != coins_within.end(); ++coin) {
        balance += asset.value(*coin);
        min_reliability = min(min_reliability, reliability(coin->first));
    }

    if(balance < amount) min_reliability = 0;
    
    Object obj;
    Value reliable;
    switch (params.size()) {
        case 1: // implicitly the same as within "forever"
        case 2:
            obj.push_back(Pair("balance", balance));
        case 3:
            obj.push_back(Pair("reliability", min_reliability));
            return obj;
            break;
        case 4:
            reliable = (bool) (min_reliability > confirmation);
            return reliable;
    }
}

Value GetDebit::operator()(const Array& params, bool fHelp) {

    if (fHelp || params.size() != 1)
        throw runtime_error(
                            "getdebit <btcaddr>\n"
                            "Get debit coins of <btcaddr>");
    CBitcoinAddress addr = CBitcoinAddress(params[0].get_str());
    if(!addr.IsValid())
        throw runtime_error("getdebit <btcaddr>\n"
                            "btcaddr invalid!");
    
    uint160 hash160 = CBitcoinAddress(params[0].get_str()).GetHash160();
    
    CTxDB txdb("r");
    
    set<Coin> debit;
    
    txdb.ReadDrIndex(hash160, debit);
    
    CRITICAL_BLOCK(cs_mapTransactions)
        if(mapDebits.count(hash160))
            debit.insert(mapDebits[hash160].begin(), mapDebits[hash160].end());

    Array list;
    
    for(set<Coin>::iterator coin = debit.begin(); coin != debit.end(); ++coin)
    {
        Object obj;
        obj.clear();
        obj.push_back(Pair("hash", coin->first.toString()));
        obj.push_back(Pair("n", uint64_t(coin->second)));
        list.push_back(obj);
    }
    
    return list;
}

Value GetCredit::operator()(const Array& params, bool fHelp) {

    if (fHelp || params.size() != 1)
        throw runtime_error("getcredit <btcaddr>\n"
                            "Get credit coins of <btcaddr>");
    
    CBitcoinAddress addr = CBitcoinAddress(params[0].get_str());
    if(!addr.IsValid())
        throw runtime_error("getcredit <btcaddr>\n"
                            "btcaddr invalid!");
    uint160 hash160 = addr.GetHash160();
        
    
    CTxDB txdb("r");
    
    set<Coin> credit;
    
    txdb.ReadCrIndex(hash160, credit);

    CRITICAL_BLOCK(cs_mapTransactions)
        if(mapCredits.count(hash160))
            credit.insert(mapCredits[hash160].begin(), mapCredits[hash160].end());
    
    Array list;
    
    for(set<Coin>::iterator coin = credit.begin(); coin != credit.end(); ++coin)
    {
        Object obj;
        obj.clear();
        obj.push_back(Pair("hash", coin->first.toString()));
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
    asset.syncronize(sync, true);
    set<Coin> coins = asset.getCoins();
    
    Array list;
    
    for(set<Coin>::iterator coin = coins.begin(); coin != coins.end(); ++coin)
    {
        Object obj;
        obj.clear();
        obj.push_back(Pair("hash", coin->first.toString()));
        obj.push_back(Pair("n", uint64_t(coin->second)));
        list.push_back(obj);
    }
    
    return list;
}

CTx json2tx(Object entry)
{
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
        int64 value = floor(dvalue+0.1);
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

    cout << tx.GetHash().toString() << endl;
    cout << find_value(entry, "hash").get_str() << endl;
    
    return tx;
}

int64 CalculateFee(CTx& tx)
{
    CTxDB txdb;
    // calculate the inputs:
    int64 value_in = 0;
    for(int i = 0; i < tx.vin.size(); i++)
    {
        CTransaction txin;
        txdb.ReadDiskTx(tx.vin[i].prevout.hash, txin); // OBS - you need to check also the MemoryPool...
        if (txin.IsNull()) throw runtime_error("Referred transaction not known : " + tx.vin[i].prevout.hash.toString());
        if (tx.vin[i].prevout.n < txin.vout.size())
            value_in += txin.vout[tx.vin[i].prevout.n].nValue;
        else
        {
            ostringstream err;
            err << "Referred index " << tx.vin[i].prevout.n << " too big. Transaction: " << tx.vin[i].prevout.hash.toString() << "has only " << txin.vout.size() << " txouts.";
            throw runtime_error(err.str());
        }
    }
    int64 value_out = tx.GetValueOut();
    return value_in-value_out;
}

Value posttx(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                            "posttx <tx>\n"
                            "Post a signed transaction json"); // note this is a new transaction and hence it has no hash!
    
    Object entry = params[0].get_obj();
    
    CTransaction tx = json2tx(entry);
    
    cout << write_formatted(tx2json(tx)) << endl;

    // check transaction - if not a valid tx return error
    if(!tx.CheckTransaction())
        throw runtime_error("Transaction invalid failed basic checks");
            
    // check fee size - if too small return error
    if(CalculateFee(tx) < tx.GetMinFee())
    {
        ostringstream err;
        err << "Transaction fee of: " << CalculateFee(tx)*(1./COIN) << " less than minimum fee of: " << tx.GetMinFee()*(1./COIN) << " Transaction aborted";
        cout << err.str() << endl;
        throw runtime_error(err.str());        
    }
    
    // check that the transaction contains a fee to me, the broker
    uint160 btcbroker = CBitcoinAddress(BTCBROKER).GetHash160();
    if(tx.paymentTo(btcbroker) < tx.GetMinFee())
    {
        ostringstream err;
        err << "Transaction fee of: " << tx.paymentTo(btcbroker)*(1./COIN) << " less than minimum fee of: " << tx.GetMinFee()*(1./COIN) << " Transaction aborted";
        cout << err.str() << endl;
        throw runtime_error(err.str());        
    }
        
    // accept to memory pool
    if(!tx.AcceptToMemoryPool())
    {
        ostringstream err;
        err << "Could not accept transaction - coins already spent?";
        cout << err.str() << endl;
        throw runtime_error(err.str());        
    }

    // post an inv telling about this tx
    // we don't need to block on cs_main as this routine will be called _only_ by the RPC caller that already blocks on cs_main
    if (!tx.IsCoinBase())
    {
        uint256 hash = tx.GetHash();
        RelayMessage(Inventory(MSG_TX, hash), tx);
    }
    
    // save it to the broker db, from here it will be reposted later if needed
    CBrokerDB brokerdb;
    brokerdb.WriteTx(tx);
    
    return Value::null;
    
    // now we have read the transaction - we need verify and possibly post it to the p2p network
    // We create a fake node and pretend we got this from the network - this ensures that this is handled by the right thread...
    CNode* pnode = new CNode(INVALID_SOCKET, Endpoint("localhost"));
    
    CDataStream ss;
    ss << tx;
    MessageHeader header("tx", ss.size());
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
