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
#include <coinChain/Configuration.h>

#include <coinHTTP/Server.h>
#include <coinHTTP/RPC.h>
#include <coinHTTP/Client.h>

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
    strings add_peers, connect_peers;
    boost::program_options::options_description extra("Extra options");
    extra.add_options()
    ("addnode", boost::program_options::value<strings>(&add_peers), "Add a node to connect to")
    ("connect", boost::program_options::value<strings>(&connect_peers), "Connect only to the specified node")
    ;
    
    Configuration conf(argc, argv, extra);
    
    try {
        
        Auth auth(conf.user(), conf.pass()); // if rpc_user and rpc_pass are not set, all authenticated methods becomes disallowed.
        
        if (conf.method() != "") {
            if (conf.method() == "help") {
                cout << "Usage: " << argv[0] << " [options]\n";
                cout << conf << "\n";
                cout << argv[0] << " start up as a daemon\n";
                return 1;
            }
            
            if (conf.method() == "version") {
                cout << argv[0] << " version is: " << FormatVersion(LIBRARY_VERSION) << "\n";
                return 1;
            }
        }

        // Start the bitcoin node and server!
        
        asio::ip::tcp::endpoint proxy_server;
        if(conf.proxy().size()) {
            strings host_port;
            string proxy = conf.proxy();
            split(host_port, proxy, is_any_of(":"));
            split(host_port, proxy, is_any_of(":"));
            if(host_port.size() < 2) host_port.push_back("1080");
            proxy_server = asio::ip::tcp::endpoint(asio::ip::address_v4::from_string(host_port[0]), lexical_cast<short>(host_port[1]));
        }
        Node node(conf.chain(), conf.data_dir(), conf.listen(), lexical_cast<string>(conf.node_port()), proxy_server, conf.timeout()); // it is also here we specify the use of a proxy!
        node.setClientVersion("libcoin/bitcoind", vector<string>());
        node.verification(conf.verification());
        node.validation(conf.validation());
        node.persistence(conf.persistance());
        node.searchable(true); // always true - otherwise we cannot explorer the addresses!

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
        
        Server server(conf.bind(), lexical_cast<string>(conf.port()), filesystem::initial_path().string(), conf.data_dir());
        if(conf.ssl()) server.setCredentials(conf.data_dir(), conf.certchain(), conf.privkey());
        
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