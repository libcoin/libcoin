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


#include "coinChain/Node.h"
#include "coinChain/NodeRPC.h"

#include "coinHTTP/Server.h"
#include "coinHTTP/RPC.h"
#include "coinHTTP/Client.h"

#include "coinStat/Explorer.h"
#include "coinStat/ExplorerRPC.h"

#include <boost/thread.hpp>
#include <boost/program_options.hpp>

#include <fstream>

using namespace std;
using namespace boost;
using namespace boost::program_options;
using namespace json_spirit;

/*
class Explorer;

/// Explorer collects address to coin mappings from the block chain.
/// This enables lookup in a database of coin owned by a spicific Address.
/// On first startup it scans the block file for all transactions, and 
/// stores it in a file: "explorer.dat" This file is tehn updated every 1000
/// blocks. The first scan can take several minutes, after that the penalty
/// of running the explorer is neglible.

class Explorer {
public:
    class TransactionListener : public TransactionFilter::Listener {
    public:
        TransactionListener(Explorer& explorer) : _explorer(explorer) {}
        virtual void operator()(const Transaction& tx);
    private:
        Explorer& _explorer;
    };
    
    class BlockListener : public BlockFilter::Listener {
    public:
        BlockListener(Explorer& explorer) : _explorer(explorer) {}
        virtual void operator()(const Block& blk);
    private:
        Explorer& _explorer;
    };
    
public:
    Explorer(Node& node, bool include_spend = true, std::string file = "", std::string data_dir = "") : _include_spend(include_spend), _blockChain(node.blockChain()), _height(0) {
        data_dir = (data_dir == "") ? CDB::dataDir(_blockChain.chain().dataDirSuffix()) : data_dir;
        file = (file == "") ? file = "explorer.dat" : file;
        
        // load the wallet if it is there
        load(data_dir + "/" + file);
        unsigned int h = _height;
        scan();
        if (_height - h > 1000)
            save(data_dir + "/" + file);
        
        // install callbacks to get notified about new tx'es and blocks
        node.subscribe(TransactionFilter::listener_ptr(new TransactionListener(*this)));
        node.subscribe(BlockFilter::listener_ptr(new BlockListener(*this)));
    }
    
    /// Handle to the BlockChain
    const BlockChain& blockChain() const { return _blockChain; }
    
    /// Retreive coins credited from an address
    void getCredit(const Address& btc, Coins& coins) const;
    
    /// Retreive coins debited to an address
    void getDebit(const Address& btc, Coins& coins) const;
    
    /// Retreive spendable coins of address
    void getCoins(const Address& btc, Coins& coins) const;
    
    void load(std::string filename);
    void save(std::string filename);
    
    /// Scan for address to coin mappings in the BlockChain. - This will take some time!
    void scan();
    
    /// add credit and debit:
    void insertDebit(const Address& address, const Coin& coin);
    void insertCredit(const Address& address, const Coin& coin);
    void markSpent(const Address& address, const Coin& coin);
    
    void setHeight(int height) { _height = height; }
    
private:
    bool _include_spend;
    const BlockChain& _blockChain;
    /// Multimap, mapping Address to Coin relationship. One Address can be asset for multiple coins
    typedef std::multimap<Address, Coin> Ledger;
    Ledger _debits;
    Ledger _credits;
    Ledger _coins;
    unsigned int _height;
};
  
using namespace std;
using namespace boost;
using namespace boost::program_options;
using namespace json_spirit;

void Explorer::TransactionListener::operator()(const Transaction& tx) {
    uint256 hash = tx.getHash();
    
    // for each tx output in the tx check for a pubkey or a pubkeyhash in the script
    for(unsigned int n = 0; n < tx.getNumOutputs(); n++) {
        const Output& txout = tx.getOutput(n);
        Address address = txout.getAddress();
        _explorer.insertDebit(address, Coin(hash, n));
    }
    if(!tx.isCoinBase()) {
        for(unsigned int n = 0; n < tx.getNumInputs(); n++) {
            const Input& txin = tx.getInput(n);
            Transaction prevtx;
            _explorer.blockChain().getTransaction(txin.prevout().hash, prevtx);
            
            Output txout = prevtx.getOutput(txin.prevout().index);        
            
            Address address = txout.getAddress();
            _explorer.insertCredit(address, Coin(hash, n));
            _explorer.markSpent(address, Coin(txin.prevout().hash, txin.prevout().index));
        }
    }
}

void Explorer::BlockListener::operator()(const Block& block) {
    TransactionList txes = block.getTransactions();
    for(TransactionList::const_iterator tx = txes.begin(); tx != txes.end(); ++tx) {
        uint256 hash = tx->getHash();
        
        // for each tx output in the tx check for a pubkey or a pubkeyhash in the script
        for(unsigned int n = 0; n < tx->getNumOutputs(); n++) {
            const Output& txout = tx->getOutput(n);
            Address address = txout.getAddress();
            _explorer.insertDebit(address, Coin(hash, n));
        }
        if(!tx->isCoinBase()) {
            for(unsigned int n = 0; n < tx->getNumInputs(); n++) {
                const Input& txin = tx->getInput(n);
                Transaction prevtx;
                _explorer.blockChain().getTransaction(txin.prevout().hash, prevtx);
                
                Output txout = prevtx.getOutput(txin.prevout().index);        
                
                Address address = txout.getAddress();
                _explorer.insertCredit(address, Coin(hash, n));
                _explorer.markSpent(address, Coin(txin.prevout().hash, txin.prevout().index));
            }
        }
    }
    _explorer.setHeight(_explorer.blockChain().getBlockIndex(block.getHash())->nHeight);
}

void Explorer::getCredit(const Address& address, Coins& coins) const {
    pair<Ledger::const_iterator, Ledger::const_iterator> range = _credits.equal_range(address);
    for(Ledger::const_iterator i = range.first; i != range.second; ++i)
        coins.insert(i->second);
}

void Explorer::getDebit(const Address& address, Coins& coins) const {
    pair<Ledger::const_iterator, Ledger::const_iterator> range = _debits.equal_range(address);
    for(Ledger::const_iterator i = range.first; i != range.second; ++i)
        coins.insert(i->second);        
}

void Explorer::getCoins(const Address& address, Coins& coins) const {
    pair<Ledger::const_iterator, Ledger::const_iterator> range = _coins.equal_range(address);
    for(Ledger::const_iterator i = range.first; i != range.second; ++i)
        coins.insert(i->second);
}

void Explorer::save(std::string filename) {
    ofstream ofs(filename.c_str());
    
    CDataStream ds;
    ds << _height;
    ds << _debits;
    ds << _credits;
    ds << _coins;
    ofs.write(&ds[0], ds.size());
}

void Explorer::load(std::string filename) {
    ifstream ifs(filename.c_str());
    std::stringstream buffer;
    buffer << ifs.rdbuf();        
    CDataStream ds(buffer.str());
    ds >> _height;
    ds >> _debits;
    ds >> _credits;
    ds >> _coins;
}

void Explorer::scan() {
    const CBlockIndex* pindex = _blockChain.getBlockIndex(_blockChain.getGenesisHash());
    for(int h = 0; h < _height-250; ++h) if(!(pindex = pindex->pnext)) break; // rescan the last 250 entries
    
    while (pindex) {
        Block block;
        _blockChain.getBlock(pindex, block);
        
        TransactionList txes = block.getTransactions();
        for(TransactionList::const_iterator tx = txes.begin(); tx != txes.end(); ++tx) {
            uint256 hash = tx->getHash();
            
            // for each tx output in the tx check for a pubkey or a pubkeyhash in the script
            for(unsigned int n = 0; n < tx->getNumOutputs(); n++) {
                const Output& txout = tx->getOutput(n);
                Address address = txout.getAddress();
                insertDebit(address, Coin(hash, n));
            }
            if(!tx->isCoinBase()) {
                for(unsigned int n = 0; n < tx->getNumInputs(); n++) {
                    const Input& txin = tx->getInput(n);
                    Transaction prevtx;
                    _blockChain.getTransaction(txin.prevout().hash, prevtx);
                    
                    Output txout = prevtx.getOutput(txin.prevout().index);        
                    
                    Address address = txout.getAddress();
                    insertCredit(address, Coin(hash, n));
                    markSpent(address, Coin(txin.prevout().hash, txin.prevout().index));
                }
            }
        }
        
        _height = pindex->nHeight;
        if(pindex->nHeight%100 == 0)
            printf("[coins/block#: %d, %d, %d, %d]\n", pindex->nHeight, _coins.size(), _debits.size(), _credits.size());
        
        pindex = pindex->pnext;
    }
}

void Explorer::insertDebit(const Address& address, const Coin& coin) {
    if(_include_spend)
        _debits.insert(make_pair(address, coin));
    _coins.insert(make_pair(address, coin));
}

void Explorer::insertCredit(const Address& address, const Coin& coin) {
    if(_include_spend)
        _credits.insert(make_pair(address, coin));
}

void Explorer::markSpent(const Address& address, const Coin& coin) {
    pair<Ledger::iterator, Ledger::iterator> range = _coins.equal_range(address);
    for(Ledger::iterator i = range.first; i != range.second; ++i)
        if (i->second == coin) {
            _coins.erase(i);
            break;
        }
}    

/// Base class for all Node rpc methods - they all need a handle to the node.
class ExplorerMethod : public Method {
public:
    ExplorerMethod(Explorer& explorer) : _explorer(explorer) {}
protected:
    Explorer& _explorer;
};

class GetDebit : public ExplorerMethod {
public:
    GetDebit(Explorer& explorer) : ExplorerMethod(explorer) {}
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);    
};

class GetCredit : public ExplorerMethod {
public:
    GetCredit(Explorer& explorer) : ExplorerMethod(explorer) {}
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);    
};

class GetCoins : public ExplorerMethod {
public:
    GetCoins(Explorer& explorer) : ExplorerMethod(explorer) {}
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);    
};

class GetAddressBalance : public ExplorerMethod {
public:
    GetAddressBalance(Explorer& explorer) : ExplorerMethod(explorer) { setName("getbalance"); }
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);    
};

class Search : public ExplorerMethod {
public:
    Search(Explorer& explorer) : ExplorerMethod(explorer) {}
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);    
};



Value GetDebit::operator()(const Array& params, bool fHelp) {
    if (fHelp || params.size() != 1)
        throw RPC::error(RPC::invalid_params, "getdebit <btcaddr>\n"
                         "Get debit coins of <btcaddr>");
    
    ChainAddress addr = ChainAddress(params[0].get_str());
    if(!addr.isValid(_explorer.blockChain().chain().networkId()))
        throw RPC::error(RPC::invalid_params, "getdebit <btcaddr>\n"
                         "btcaddr invalid!");
    
    Address address = addr.getAddress();
    
    Coins coins;
    
    _explorer.getDebit(address, coins);
    
    Array list;
    
    for(Coins::iterator coin = coins.begin(); coin != coins.end(); ++coin) {
        Object obj;
        obj.clear();
        obj.push_back(Pair("hash", coin->hash.toString()));
        obj.push_back(Pair("n", uint64_t(coin->index)));
        list.push_back(obj);
    }
    
    return list;
}

Value GetCredit::operator()(const Array& params, bool fHelp) {        
    if (fHelp || params.size() != 1)
        throw RPC::error(RPC::invalid_params, "getcredit <btcaddr>\n"
                         "Get credit coins of <btcaddr>");
    
    ChainAddress addr = ChainAddress(params[0].get_str());
    if(!addr.isValid(_explorer.blockChain().chain().networkId()))
        throw RPC::error(RPC::invalid_params, "getcredit <btcaddr>\n"
                         "btcaddr invalid!");
    
    Address address = addr.getAddress();
    
    Coins coins;
    
    _explorer.getCredit(address, coins);
    
    Array list;
    
    for(Coins::iterator coin = coins.begin(); coin != coins.end(); ++coin) {
        Object obj;
        obj.clear();
        obj.push_back(Pair("hash", coin->hash.toString()));
        obj.push_back(Pair("n", uint64_t(coin->index)));
        list.push_back(obj);
    }
    
    return list;
}

Value GetCoins::operator()(const Array& params, bool fHelp) {        
    if (fHelp || params.size() != 1)
        throw RPC::error(RPC::invalid_params, "getcoins <btcaddr>\n"
                         "Get un spent coins of <btcaddr>");
    
    ChainAddress addr = ChainAddress(params[0].get_str());
    if(!addr.isValid(_explorer.blockChain().chain().networkId()))
        throw RPC::error(RPC::invalid_params, "getcoins <btcaddr>\n"
                         "btcaddr invalid!");
    
    Address address = addr.getAddress();
    
    Coins coins;
    
    _explorer.getCoins(address, coins);
    
    Array list;
    
    for(Coins::iterator coin = coins.begin(); coin != coins.end(); ++coin) {
        Object obj;
        obj.clear();
        obj.push_back(Pair("hash", coin->hash.toString()));
        obj.push_back(Pair("n", uint64_t(coin->index)));
        list.push_back(obj);
    }
    
    return list;
}

Value GetAddressBalance::operator()(const Array& params, bool fHelp) {        
    if (fHelp || params.size() != 1)
        throw RPC::error(RPC::invalid_params, "getbalance <address>\n"
                         "Get the balance of a single chain address");
    
    ChainAddress addr = ChainAddress(params[0].get_str());
    if(!addr.isValid(_explorer.blockChain().chain().networkId()))
        throw RPC::error(RPC::invalid_params, "getbalance <address>\n"
                         "address invalid!");
    
    Address address = addr.getAddress();
    
    Coins coins;
    
    _explorer.getCoins(address, coins);
    
    // Now we have the coins - now get the value of these
    int64 balance = 0;
    for(Coins::const_iterator coin = coins.begin(); coin != coins.end(); ++coin)
        balance += _explorer.blockChain().value(*coin);
    
    Value val(balance);
    
    return val;
}

Value Search::operator()(const Array& params, bool fHelp) {        
    if (fHelp || params.size() < 1)
        throw RPC::error(RPC::invalid_params, "search <address or part of hash160 or hash256>\n"
                         "Search the block chain for nearest matching address/transaction/block");
    
    // Is it an address ?
    ChainAddress addr = ChainAddress(params[0].get_str());
    if(addr.isValid(_explorer.blockChain().chain().networkId())) {
        // We assume we got an address - now produce info for this:
        // return value, credit coins, debit coins, and spendable coins depending on options
        
        Address address = addr.getAddress();
        Coins coins;
        
        if (params.size() < 2 || params[1].get_str() == "balance") {
            _explorer.getCoins(address, coins);
            // Now we have the coins - now get the value of these
            int64 balance = 0;
            for(Coins::const_iterator coin = coins.begin(); coin != coins.end(); ++coin)
                balance += _explorer.blockChain().value(*coin);
            
            Value val(balance);
            
            return val;            
        }
        else if (params.size() == 2 && params[1].get_str() == "unspent") {
            Coins coins;
            
            _explorer.getCoins(address, coins);
            
            Array list;
            
            for(Coins::iterator coin = coins.begin(); coin != coins.end(); ++coin) {
                Object obj;
                obj.clear();
                obj.push_back(Pair("hash", coin->hash.toString()));
                obj.push_back(Pair("n", uint64_t(coin->index)));
                list.push_back(obj);
            }
            
            return list;
        }
        else if (params.size() == 2 && params[1].get_str() == "credit") {
            Coins coins;
            
            _explorer.getCredit(address, coins);
            
            Array list;
            
            for(Coins::iterator coin = coins.begin(); coin != coins.end(); ++coin) {
                Object obj;
                obj.clear();
                obj.push_back(Pair("hash", coin->hash.toString()));
                obj.push_back(Pair("n", uint64_t(coin->index)));
                list.push_back(obj);
            }
            
            return list;
        }
        else if (params.size() == 2 && params[1].get_str() == "debit") {
            Coins coins;
            
            _explorer.getDebit(address, coins);
            
            Array list;
            
            for(Coins::iterator coin = coins.begin(); coin != coins.end(); ++coin) {
                Object obj;
                obj.clear();
                obj.push_back(Pair("hash", coin->hash.toString()));
                obj.push_back(Pair("n", uint64_t(coin->index)));
                list.push_back(obj);
            }
            
            return list;
        }
        else
            return Value::null;
    }
    else { // assume a hash or hash fraction - search all possible hashes
        // hashes can be address hash, transaction hash or block hash - currently no fraction is supported
        if (params[0].get_str().size() < 64) { // assume uint160
            Address address(params[0].get_str());
            Coins coins;
            _explorer.getCoins(address, coins);
            // Now we have the coins - now get the value of these
            int64 balance = 0;
            for(Coins::const_iterator coin = coins.begin(); coin != coins.end(); ++coin)
                balance += _explorer.blockChain().value(*coin);
            
            Value val(balance);
            
            return val;            
        }
        else { // assume uint256
            uint256 hash(params[0].get_str());
            const CBlockIndex* blockindex = _explorer.blockChain().getBlockIndex(hash);
            
            if (blockindex) { // it was a block
                Block block;
                _explorer.blockChain().getBlock(hash, block);
                
                Object result;
                result.push_back(Pair("hash", block.getHash().GetHex()));
                result.push_back(Pair("blockcount", blockindex->nHeight));
                result.push_back(Pair("version", block.getVersion()));
                result.push_back(Pair("merkleroot", block.getMerkleRoot().GetHex()));
                result.push_back(Pair("time", (boost::int64_t)block.getBlockTime()));
                result.push_back(Pair("nonce", (boost::uint64_t)block.getNonce()));
                result.push_back(Pair("difficulty", _explorer.blockChain().getDifficulty(blockindex)));
                Array txhashes;
                BOOST_FOREACH (const Transaction&tx, block.getTransactions())
                txhashes.push_back(tx.getHash().GetHex());
                
                result.push_back(Pair("tx", txhashes));
                
                if (blockindex->pprev)
                    result.push_back(Pair("hashprevious", blockindex->pprev->GetBlockHash().GetHex()));
                if (blockindex->pnext)
                    result.push_back(Pair("hashnext", blockindex->pnext->GetBlockHash().GetHex()));
                
                return result;
            }
            
            int64 timestamp = 0;
            int64 blockheight = 0;
            
            Transaction tx;
            _explorer.blockChain().getTransaction(hash, tx, blockheight, timestamp);
            
            if(!tx.isNull()) {                
                Object entry = tx2json(tx, timestamp, blockheight);
                return entry;
            }
        }
    }
    return Value::null;
}
*/
int main(int argc, char* argv[])
{
    try {
        string config_file, data_dir;
        unsigned short rpc_port;
        string rpc_bind, rpc_connect, rpc_user, rpc_pass;
        typedef vector<string> strings;
        strings rpc_params;
        strings connect_peers;
        strings add_peers;
        bool ssl;
        string certchain, privkey;

        // Commandline options
        options_description generic("Generic options");
        generic.add_options()
            ("help,?", "Show help messages")
            ("version,v", "print version string")
            ("conf,c", value<string>(&config_file)->default_value("bitcoin.conf"), "Specify configuration file")
            ("datadir", "Specify non default data directory")
        ;
        
        options_description config("Config options");
        config.add_options()
            ("pid", value<string>(), "Specify pid file (default: bitcoind.pid)")
            ("nolisten", "Don't accept connections from outside")
            ("testnet", "Use the test network")
            ("addnode", value<strings>(&add_peers), "Add a node to connect to")
            ("connect", value<strings>(&connect_peers), "Connect only to the specified node")
            ("rpcuser", value<string>(&rpc_user), "Username for JSON-RPC connections")
            ("rpcpassword", value<string>(&rpc_pass), "Password for JSON-RPC connections")
            ("rpcport", value<unsigned short>(&rpc_port)->default_value(8332), "Listen for JSON-RPC connections on <arg>")
            ("rpcallowip", value<string>(&rpc_bind)->default_value(asio::ip::address_v4::loopback().to_string()), "Allow JSON-RPC connections from specified IP address")
            ("rpcconnect", value<string>(&rpc_connect)->default_value(asio::ip::address_v4::loopback().to_string()), "Send commands to node running on <arg>")
            ("rpcssl", value<bool>(&ssl)->default_value(false), "Use OpenSSL (https) for JSON-RPC connections")
            ("rpcsslcertificatechainfile", value<string>(&certchain)->default_value("server.cert"), "Server certificate file")
            ("rpcsslprivatekeyfile", value<string>(&privkey)->default_value("server.pem"), "Server private key")
        ;
                
        options_description cmdline_options;
        cmdline_options.add(generic).add(config);
        
        options_description config_file_options;
        config_file_options.add(config);
        
        options_description visible;
        visible.add(generic).add(config);
        
        positional_options_description pos;
        pos.add("params", -1);
        
        
        // parse the command line
        variables_map args;
        store(command_line_parser(argc, argv).options(cmdline_options).positional(pos).run(), args);
        notify(args);
        
        if (args.count("help")) {
            cout << "Usage: " << argv[0] << " [options]\n";
            cout << visible << "\n";
            cout << argv[0] << " start up as a daemon\n";
            return 1;
        }
        
        if (args.count("version")) {
            cout << argv[0] << " version is: " << FormatFullVersion() << "\n";
            return 1;        
        }

        if(!args.count("datadir"))
            data_dir = CDB::dataDir(bitcoin.dataDirSuffix());
        
        // if present, parse the config file - if no data dir is specified we always assume bitcoin chain at this stage 
        string config_path = data_dir + "/" + config_file;
        ifstream ifs(config_path.c_str());
        if(ifs) {
            store(parse_config_file(ifs, config_file_options, true), args);
            notify(args);
        }

        Auth auth(rpc_user, rpc_pass); // if rpc_user and rpc_pass are not set, all authenticated methods becomes disallowed.
        
        // Start the bitcoin node and server!

        const Chain* chain_chooser;
        if(args.count("testnet"))
            chain_chooser = &testnet;
        else
            chain_chooser = &bitcoin;
        const Chain& chain(*chain_chooser);

        if(!args.count("datadir"))
            data_dir = CDB::dataDir(chain.dataDirSuffix());
        
        logfile = data_dir + "/debug.log";
        
        Node node(chain, data_dir, args.count("nolisten") ? "" : "0.0.0.0"); // it is also here we specify the use of a proxy!
//        PortMapper(node.get_io_service(), port); // this will use the Node call

        // use the connect and addnode options to restrict and supplement the irc and endpoint db.
        for(strings::iterator ep = add_peers.begin(); ep != add_peers.end(); ++ep) node.addPeer(*ep);
        for(strings::iterator ep = connect_peers.begin(); ep != connect_peers.end(); ++ep) node.connectPeer(*ep);

        Explorer explorer(node); // this will also register notification routines for new blocks and transactions
        
        thread nodeThread(&Node::run, &node); // run this as a background thread
        
        string search_page = 
        "<html>"
            "<head>"
                "<title>libcoin - Coin Explorer</title>"
            "</head>"
            "<body>"
                "<form action=\"/search\" method=\"post\" enctype=\"text/plain\">"
                    "<p>"
                        "<input type=\"text\" name=\"params\" size=\"50\">"
                        "<input type=\"submit\" value=\"Search\">"
                    "</p>"
                "</form>"
            "</body>"
        "</html>";
        Server server(rpc_bind, lexical_cast<string>(rpc_port), search_page);
        if(ssl) server.setCredentials(data_dir, certchain, privkey);
        
        // Register Server methods.
        server.registerMethod(method_ptr(new Stop(server)), auth);
        
        // Register Node methods.
        server.registerMethod(method_ptr(new GetBlockCount(node)));
        server.registerMethod(method_ptr(new GetConnectionCount(node)));
        server.registerMethod(method_ptr(new GetDifficulty(node)));
        server.registerMethod(method_ptr(new GetInfo(node)));
        
        // Node methods relevant for coin explorer
        server.registerMethod(method_ptr(new GetBlock(node)));
        server.registerMethod(method_ptr(new GetBlockHash(node)));
        server.registerMethod(method_ptr(new GetTransaction(node)));
        
        // Register Explorer methods.
        server.registerMethod(method_ptr(new GetDebit(explorer)));
        server.registerMethod(method_ptr(new GetCredit(explorer)));
        server.registerMethod(method_ptr(new GetCoins(explorer)));
        server.registerMethod(method_ptr(new GetAddressBalance(explorer)));
        server.registerMethod(method_ptr(new Search(explorer)));
                
        try { // we keep the server in its own exception scope as we want the other threads to shut down properly if the server exits
            server.run();    
        } catch(std::exception& e) { 
            cerr << "Error: " << e.what() << endl; 
        }

        printf("Server exitted, shutting down Node...\n");
        // getting here means that we have exited from the server (e.g. by the quit method)
        
        node.shutdown();
        nodeThread.join();
        
        return 0;    
    }
    catch(std::exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
}