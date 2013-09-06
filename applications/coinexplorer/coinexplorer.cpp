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


#include <coinChain/Node.h>
#include <coinChain/NodeRPC.h>

#include <coinHTTP/Server.h>
#include <coinHTTP/RPC.h>
#include <coinHTTP/Client.h>

#include <coinNAT/PortMapper.h>

#include <boost/thread.hpp>
#include <boost/program_options.hpp>

#include <fstream>


/// Get unspent coins belonging to an PubKeyHash
class GetCoins : public NodeMethod {
public:
    GetCoins(Node& node) : NodeMethod(node) {}
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);
};

/// Get the balance based on the unspent coins of an PubKeyHash
class GetAddressBalance : public NodeMethod {
public:
    GetAddressBalance(Node& node) : NodeMethod(node) { setName("getbalance"); }
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);
};

/// Do a search in the blockchain and in the explorer database.
class Search : public NodeMethod {
public:
    Search(Node& node) : NodeMethod(node) {}
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);
};

using namespace std;
using namespace boost;
using namespace boost::program_options;
using namespace json_spirit;

Value GetCoins::operator()(const Array& params, bool fHelp) {
    if (fHelp || params.size() != 1)
        throw RPC::error(RPC::invalid_params, "getcoins <btcaddr>/<PubKeyHash>\n"
                         "Get un spent coins of <btcaddr>/<PubKeyHash>");
    
    string param = params[0].get_str();
    
    PubKeyHash address;
    ChainAddress addr = _node.blockChain().chain().getAddress(param);
    if (!addr.isValid()) { // not an address - try with a PubKeyHash - base 16 encoded:
        if(param.size() == 40 && all(param, is_from_range('a','f') || is_digit()))
            address = toPubKeyHash(param); // for some reason the string is byte reversed as compared to the internal representation ?!?!?
        else
            throw RPC::error(RPC::invalid_params, "getcoins <btcaddr>/<PubKeyHash>\n"
                             "invalid argument!");
    }
    else
        address = addr.getPubKeyHash();
    
    Unspents coins;
    
    Script script;
    script.setAddress(address);
    _node.blockChain().getUnspents(script, coins);
    
    Array list;
    
    for(Unspents::const_iterator coin = coins.begin(); coin != coins.end(); ++coin) {
        Object obj;
        obj.clear();
        obj.push_back(Pair("hash", coin->key.hash.toString()));
        obj.push_back(Pair("n", boost::uint64_t(coin->key.index)));
        list.push_back(obj);
    }
    
    return list;
}

Value GetAddressBalance::operator()(const Array& params, bool fHelp) {
    if (fHelp || params.size() != 1)
        throw RPC::error(RPC::invalid_params, "getbalance <address>\n"
                         "Get the balance of a single chain address");
    
    ChainAddress addr = _node.blockChain().chain().getAddress(params[0].get_str());
    if (!addr.isValid())
        throw RPC::error(RPC::invalid_params, "getbalance <address>\n"
                         "address invalid!");
    
    PubKeyHash address = addr.getPubKeyHash();
    
    Unspents coins;
    
    Script script;
    script.setAddress(address);
    _node.blockChain().getUnspents(script, coins);
    
    // Now we have the coins - now get the value of these
    boost::int64_t balance = 0;
    for(Unspents::const_iterator coin = coins.begin(); coin != coins.end(); ++coin)
        balance += coin->output.value();
    
    Value val(balance);
    
    return val;
}

Value Search::operator()(const Array& params, bool fHelp) {
    if (fHelp || params.size() < 1)
        throw RPC::error(RPC::invalid_params, "search <address or part of hash160 or hash256>\n"
                         "Search the block chain for nearest matching address/transaction/block");
    
    // Is it an address ?
    ChainAddress addr = _node.blockChain().chain().getAddress(params[0].get_str());
    if (addr.isValid()) {
        // We assume we got an address - now produce info for this:
        // return value, credit coins, debit coins, and spendable coins depending on options
        
        Script script;
        script.setAddress(addr.getPubKeyHash());
        Unspents coins;
        
        if (params.size() < 2 || params[1].get_str() == "balance") {
            _node.blockChain().getUnspents(script, coins);
            // Now we have the coins - now get the value of these
            boost::int64_t balance = 0;
            for(Unspents::const_iterator coin = coins.begin(); coin != coins.end(); ++coin)
                balance += coin->output.value();
            
            Value val(balance);
            
            return val;
        }
        else if (params.size() == 2 && params[1].get_str() == "unspent") {
            Unspents coins;
            
            _node.blockChain().getUnspents(script, coins);
            
            Array list;
            
            for(Unspents::const_iterator coin = coins.begin(); coin != coins.end(); ++coin) {
                Object obj;
                obj.clear();
                obj.push_back(Pair("hash", coin->key.hash.toString()));
                obj.push_back(Pair("n", boost::uint64_t(coin->key.index)));
                list.push_back(obj);
            }
            
            return list;
        }
        else
            return Value::null;
    }
    else { // assume a block number, hash or hash fraction - search all possible hashes
           // hashes can be address hash, transaction hash or block hash - currently no fraction is supported
        if (params[0].get_str().size() < 40) { // assume block height
            unsigned int height = lexical_cast<unsigned int>(params[0].get_str());
            
            BlockIterator bi = _node.blockChain().iterator(height);
            
            if (!!bi) { // it was a block
                Block block;
                _node.blockChain().getBlock(bi, block);
                
                Object result;
                result.push_back(Pair("hash", block.getHash().GetHex()));
                result.push_back(Pair("blockcount", bi.height()));
                result.push_back(Pair("version", block.getVersion()));
                result.push_back(Pair("merkleroot", block.getMerkleRoot().GetHex()));
                result.push_back(Pair("time", (boost::int64_t)block.getBlockTime()));
                result.push_back(Pair("nonce", (boost::uint64_t)block.getNonce()));
                result.push_back(Pair("difficulty", _node.blockChain().getDifficulty(bi)));
                Array txhashes;
                BOOST_FOREACH (const Transaction&tx, block.getTransactions())
                txhashes.push_back(tx.getHash().GetHex());
                
                result.push_back(Pair("tx", txhashes));
                
                if (bi->prev != uint256(0))
                    result.push_back(Pair("hashprevious", bi->prev.toString()));
                //                if (blockindex->pnext)
                //                    result.push_back(Pair("hashnext", blockindex->pnext->GetBlockHash().GetHex()));
                
                return result;
            }
        }
        else if (params[0].get_str().size() < 64) { // assume uint160
            PubKeyHash address(params[0].get_str());
            Script script;
            script.setAddress(address);
            
            Unspents coins;
            _node.blockChain().getUnspents(script, coins);
            // Now we have the coins - now get the value of these
            boost::int64_t balance = 0;
            for(Unspents::const_iterator coin = coins.begin(); coin != coins.end(); ++coin)
                balance += coin->output.value();
            
            Value val(balance);
            
            return val;
        }
        else { // assume uint256
            uint256 hash(params[0].get_str());
            BlockIterator bi = _node.blockChain().iterator(hash);
            
            if (!!bi) { // it was a block
                Block block;
                _node.blockChain().getBlock(bi, block);
                
                Object result;
                result.push_back(Pair("hash", block.getHash().GetHex()));
                result.push_back(Pair("blockcount", bi.height()));
                result.push_back(Pair("version", block.getVersion()));
                result.push_back(Pair("merkleroot", block.getMerkleRoot().GetHex()));
                result.push_back(Pair("time", (boost::int64_t)block.getBlockTime()));
                result.push_back(Pair("nonce", (boost::uint64_t)block.getNonce()));
                result.push_back(Pair("difficulty", _node.blockChain().getDifficulty(bi)));
                Array txhashes;
                BOOST_FOREACH (const Transaction&tx, block.getTransactions())
                txhashes.push_back(tx.getHash().GetHex());
                
                result.push_back(Pair("tx", txhashes));
                
                if (bi->prev != uint256(0))
                    result.push_back(Pair("hashprevious", bi->prev.toString()));
                //                if (blockindex->pnext)
                //                    result.push_back(Pair("hashnext", blockindex->pnext->GetBlockHash().GetHex()));
                
                return result;
            }
            
            int64_t timestamp = 0;
            int64_t blockheight = 0;
            
            Transaction tx;
            _node.blockChain().getTransaction(hash, tx, blockheight, timestamp);
            
            if(!tx.isNull()) {
                Object entry = tx2json(tx, timestamp, blockheight);
                return entry;
            }
        }
    }
    return Value::null;
}


int main(int argc, char* argv[])
{
    try {
        string config_file, data_dir, log_file;
        unsigned short port, rpc_port;
        string rpc_bind, rpc_connect, rpc_user, rpc_pass;
        typedef vector<string> strings;
        strings rpc_params;
        string proxy, irc;
        strings connect_peers;
        strings add_peers;
        bool portmap, ssl;
        unsigned int timeout;
        string certchain, privkey;

        // Commandline options
        options_description generic("Generic options");
        generic.add_options()
            ("help,?", "Show help messages")
            ("version,v", "print version string")
            ("conf,c", value<string>(&config_file)->default_value("libcoin.conf"), "Specify configuration file")
            ("datadir", value<string>(&data_dir), "Specify non default data directory")
        ;
        
        options_description config("Config options");
        config.add_options()
            ("pid", value<string>(), "Specify pid file (default: bitcoind.pid)")
            ("nolisten", "Don't accept connections from outside")
            ("portmap", value<bool>(&portmap)->default_value(true), "Use IGD-UPnP or NATPMP to map the listening port")
            ("upnp", value<bool>(&portmap), "Use UPnP to map the listening port - deprecated, use portmap")
            ("testnet", "Use the test network")
            ("litecoin", "Run as a litecoin client")
            ("namecoin", "Run as a namecoin client")
            ("proxy", value<string>(&proxy), "Connect through socks4 proxy")
            ("irc", value<string>(&irc)->default_value("92.243.23.21"), "Specify other chat server, for no chatserver specify \"\"")
            ("timeout", value<unsigned int>(&timeout)->default_value(5000), "Specify connection timeout (in milliseconds)")
            ("addnode", value<strings>(&add_peers), "Add a node to connect to")
            ("connect", value<strings>(&connect_peers), "Connect only to the specified node")
            ("port", value<unsigned short>(&port)->default_value(8333), "Listen on specified port for the p2p protocol")
            ("rpcuser", value<string>(&rpc_user), "Username for JSON-RPC connections")
            ("rpcpassword", value<string>(&rpc_pass), "Password for JSON-RPC connections")
            ("rpcport", value<unsigned short>(&rpc_port)->default_value(8332), "Listen for JSON-RPC connections on <arg>")
            ("rpcallowip", value<string>(&rpc_bind)->default_value(asio::ip::address_v4::loopback().to_string()), "Allow JSON-RPC connections from specified IP address")
            ("rpcconnect", value<string>(&rpc_connect)->default_value(asio::ip::address_v4::loopback().to_string()), "Send commands to node running on <arg>")
            ("rpcssl", value<bool>(&ssl)->default_value(false), "Use OpenSSL (https) for JSON-RPC connections")
            ("rpcsslcertificatechainfile", value<string>(&certchain)->default_value("server.cert"), "Server certificate file")
            ("rpcsslprivatekeyfile", value<string>(&privkey)->default_value("server.pem"), "Server private key")
            ("debug", "Set logging output to debug")
            ("trace", "Set logging output to trace")
            ("log", value<string>(&log_file)->default_value("debug.log"), "Logfile name - if starting with / absolute path is assumed, otherwise relative to data directory, choose '-' for logging to stderr")
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
            cout << argv[0] << " version is: " << FormatVersion(PROTOCOL_VERSION) << "\n";
            return 1;        
        }

        if(!args.count("datadir"))
            data_dir = default_data_dir(bitcoin.dataDirSuffix());
        
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
        else if(args.count("litecoin"))
            chain_chooser = &litecoin;
        else if(args.count("namecoin"))
            chain_chooser = &namecoin;
        else
            chain_chooser = &bitcoin;
        const Chain& chain(*chain_chooser);

        if(!args.count("datadir"))
            data_dir = default_data_dir(chain.dataDirSuffix());
        

        ofstream olog;
        
        if (log_file.size() && log_file[0] != '-') {
            if (log_file.size() && log_file[0] != '/')
                log_file = data_dir + "/" + log_file;

            olog.open((log_file).c_str(), std::ios_base::out|std::ios_base::app);;
        }
        
        Logger::Level ll = Logger::info;
        if (args.count("trace"))
            ll = Logger::trace;
        else if (args.count("debug"))
            ll = Logger::debug;

        if (olog.is_open())
            Logger::instantiate(olog, ll);
        else
            Logger::instantiate(cerr, ll);

        Logger::label_thread("main");
        
        asio::ip::tcp::endpoint proxy_server;
        if(proxy.size()) {
            vector<string> host_port; split(host_port, proxy, is_any_of(":"));
            if(host_port.size() < 2) host_port.push_back("1080");
            proxy_server = asio::ip::tcp::endpoint(asio::ip::address_v4::from_string(host_port[0]), lexical_cast<short>(host_port[1]));
        }
        Node node(chain, data_dir, args.count("nolisten")||connect_peers.size() ? "" : "0.0.0.0", lexical_cast<string>(port), proxy_server, timeout, irc); // it is also here we specify the use of a proxy!
        node.setClientVersion("libcoin/coinexplorer", vector<string>());
        node.verification(Node::MINIMAL);
        node.validation(Node::NONE); // note set to MINIMAL to get validation (merkeltrie stuff)
        node.persistence(Node::LAZY); // set to FULL to get keep all blocks (history)
        node.searchable(true);
        PortMapper mapper(node.get_io_service(), port); // this will use the Node call
        if(portmap) mapper.start();

        // use the connect and addnode options to restrict and supplement the irc and endpoint db.
        for(strings::iterator ep = add_peers.begin(); ep != add_peers.end(); ++ep) node.addPeer(*ep);
        for(strings::iterator ep = connect_peers.begin(); ep != connect_peers.end(); ++ep) node.connectPeer(*ep);

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
        Server server(rpc_bind, lexical_cast<string>(rpc_port), (args.count("raphael") ? filesystem::initial_path().string() : search_page), data_dir);
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
        //        server.registerMethod(method_ptr(new GetDebit(explorer)));
        //        server.registerMethod(method_ptr(new GetCredit(explorer)));
        server.registerMethod(method_ptr(new GetCoins(node)));
        server.registerMethod(method_ptr(new GetAddressBalance(node)));
        server.registerMethod(method_ptr(new Search(node)));
                
        try { // we keep the server in its own exception scope as we want the other threads to shut down properly if the server exits
            server.run();    
        } catch(std::exception& e) { 
            cerr << "Error: " << e.what() << endl; 
        }

        log_info("Server exitted, shutting down Node...\n");
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