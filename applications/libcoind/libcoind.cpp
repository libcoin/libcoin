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
#include <coinHTTP/Client.h>

#include <coinWallet/Wallet.h>
#include <coinWallet/WalletRPC.h>

#include <coinPool/Miner.h>
#include <coinPool/GetWork.h>
#include <coinPool/GetBlockTemplate.h>
#include <coinPool/SubmitBlock.h>

#include <coinNAT/PortMapper.h>

#include <coinName/NameGetRPC.h>

#include <coin/Logger.h>

#include <boost/thread.hpp>
#include <boost/algorithm/string/split.hpp>

#include <fstream>

using namespace std;
using namespace boost;
using namespace json_spirit;

/// Get unspent coins belonging to an PubKeyHash
class TestMessage : public NodeMethod {
public:
    TestMessage(Node& node) : NodeMethod(node) {}
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);
};

Value TestMessage::operator()(const Array& params, bool fHelp) {
    if (fHelp || params.size() != 1)
        throw RPC::error(RPC::invalid_params, "testmessage message\n"
                         "fake sending a message to the node");
    
    string param = params[0].get_str();
    
    _node.post("mempool", "");
    
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
        
        // If we have params on the cmdline we run as a command line client contacting a server
        if (conf.method() != "") {
            if (conf.method() == "help") {
                cout << "Usage: " << argv[0] << " [options] [rpc-command param1 param2 ...]\n";
                cout << conf << "\n";
                cout << "If no rpc-command is specified, " << argv[0] << " start up as a daemon, otherwise it offers commandline access to a running instance\n";
                return 1;
            }
            
            if (conf.method() == "version") {
                cout << argv[0] << " version is: " << FormatVersion(LIBRARY_VERSION) << "\n";
                return 1;
            }

            // create URL
            string url = conf.url();
            Client client;
            // this is a blocking post!
            Reply reply = client.post(url, RPC::content(conf.method(), conf.params()), auth.headers());
            if(reply.status() == Reply::ok) {
                Object rpc_reply = RPC::reply(reply.content());
                Value result = find_value(rpc_reply, "result");
                if (result.type() == str_type)
                    cout << result.get_str() << "\n";
                else
                    cout << write_formatted(result) << "\n";
                return 0;
            }
            else {
                cout << "HTTP error code: " << reply.status() << "\n";
                Object rpc_reply = RPC::reply(reply.content());
                Object rpc_error = find_value(rpc_reply, "error").get_obj();
                Value code = find_value(rpc_error, "code");
                Value message = find_value(rpc_error, "message");
                cout << "JSON RPC Error code: " << code.get_int() << "\n";
                cout <<  message.get_str() << "\n";
                return 1;
            }
        }
        
        // Else we start the bitcoin node and server!

        asio::ip::tcp::endpoint proxy_server;
        if(conf.proxy().size()) {
            strings host_port;
            string proxy = conf.proxy();
            split(host_port, proxy, is_any_of(":"));
            if(host_port.size() < 2) host_port.push_back("1080"); 
            proxy_server = asio::ip::tcp::endpoint(asio::ip::address_v4::from_string(host_port[0]), lexical_cast<short>(host_port[1]));
        }
        Node node(conf.chain(), conf.data_dir(), conf.listen(), lexical_cast<string>(conf.node_port()), proxy_server, conf.timeout()); // it is also here we specify the use of a proxy!
        node.setClientVersion("libcoin/bitcoind", vector<string>());
        node.verification(conf.verification());
        node.validation(conf.validation());
        node.persistence(conf.persistance());
        node.searchable(conf.searchable());

        PortMapper mapper(node.get_io_service(), conf.node_port()); // this will use the Node call
        if(conf.portmap()) mapper.start();
        
        // use the connect and addnode options to restrict and supplement the irc and endpoint db.
        for(strings::iterator ep = add_peers.begin(); ep != add_peers.end(); ++ep) node.addPeer(*ep);
        for(strings::iterator ep = connect_peers.begin(); ep != connect_peers.end(); ++ep) node.connectPeer(*ep);
        
        // connect the accounts - first assume there is only one - the wallet.dat
        
        
        Wallet wallet(node, "wallet.dat", conf.data_dir()); // this will also register the needed callbacks
        
        //
        //node.subscribe(wallet); // which will first sync the wallet, then install it as a callback to the blockchain
        // question was how to get 
        
        thread nodeThread(&Node::run, &node); // run this as a background thread

        CReserveKey reservekey(&wallet);
        
        Server server(conf.bind(), lexical_cast<string>(conf.port()), filesystem::initial_path().string());
        if(conf.ssl()) server.setCredentials(conf.data_dir(), conf.certchain(), conf.privkey());
        
        // Register Server methods.
        server.registerMethod(method_ptr(new Stop(server)), auth);
        
        // Register Node methods.
        server.registerMethod(method_ptr(new GetBlockHash(node)));
        server.registerMethod(method_ptr(new GetBlock(node)));
        server.registerMethod(method_ptr(new GetBlockCount(node)));
        server.registerMethod(method_ptr(new GetConnectionCount(node)));
        server.registerMethod(method_ptr(new GetDifficulty(node)));
        server.registerMethod(method_ptr(new GetInfo(node)));
        server.registerMethod(method_ptr(new TestMessage(node)));

        //server.registerMethod(method_ptr(new TxWait(node)));
        
        if (conf.chain().adhere_names()) {
            server.registerMethod(method_ptr(new NameShow(node)));
            server.registerMethod(method_ptr(new NameHistory(node)));
            server.registerMethod(method_ptr(new NameScan(node)));
        }
        
        /// The Pool enables you to run a backend for a miner, i.e. your private pool, it also enables you to participate in the "Name of Pool"
        // We need a list of blocks for the shared mining
        //
        ChainAddress address("17uY6a7zUidQrJVvpnFchD1MX1untuAufd");
        StaticPayee payee(address.getPubKeyHash());
        Pool pool(node, payee);
        
        server.registerMethod(method_ptr(new GetWork(pool)));
        server.registerMethod(method_ptr(new GetBlockTemplate(pool)));
        server.registerMethod(method_ptr(new SubmitBlock(pool)));

        Miner miner(pool);
        miner.setGenerate(conf.generate());
        thread miningThread(&Miner::run, &miner);
        
        // Register Wallet methods.
        server.registerMethod(method_ptr(new GetBalance(wallet)), auth);
        server.registerMethod(method_ptr(new GetNewAddress(wallet)), auth);
        server.registerMethod(method_ptr(new SendToAddress(wallet)), auth);
        server.registerMethod(method_ptr(new GetAccountAddress(wallet)), auth);
        server.registerMethod(method_ptr(new GetAccount(wallet)), auth);
        server.registerMethod(method_ptr(new SetAccount(wallet)), auth);
        server.registerMethod(method_ptr(new GetAddressesByAccount(wallet)), auth);
        server.registerMethod(method_ptr(new SetTxFee(wallet)), auth);
        server.registerMethod(method_ptr(new GetReceivedByAddress(wallet)), auth);
        server.registerMethod(method_ptr(new GetReceivedByAccount(wallet)), auth);
        server.registerMethod(method_ptr(new MoveCmd(wallet)), auth);
        server.registerMethod(method_ptr(new SendFrom(wallet)), auth);
        server.registerMethod(method_ptr(new SendMany(wallet)), auth);
        server.registerMethod(method_ptr(new ListReceivedByAddress(wallet)), auth);
        server.registerMethod(method_ptr(new ListReceivedByAccount(wallet)), auth);
        server.registerMethod(method_ptr(new ListTransactions(wallet)), auth);
        server.registerMethod(method_ptr(new ListAccounts(wallet)), auth);
        server.registerMethod(method_ptr(new GetWalletTransaction(wallet)), auth);
        server.registerMethod(method_ptr(new BackupWallet(wallet)), auth);
        server.registerMethod(method_ptr(new KeypoolRefill(wallet)), auth);
        server.registerMethod(method_ptr(new WalletPassphraseChange(wallet)), auth);
        server.registerMethod(method_ptr(new WalletLock(wallet)), auth);
        server.registerMethod(method_ptr(new EncryptWallet(wallet)), auth);
        server.registerMethod(method_ptr(new ValidateAddress(wallet)), auth);
        // this method also takes the io_service from the server to start a deadline timer locking the wallet again.
        server.registerMethod(method_ptr(new WalletPassphrase(wallet, server.get_io_service())), auth);

        try { // we keep the server in its own exception scope as we want the other threads to shut down properly if the server exits
            server.run();    
        } catch(std::exception& e) { 
            cerr << "Error: " << e.what() << endl; 
        }

        log_info("Server exited, shutting down Node and Miner...\n");
        // getting here means that we have exited from the server (e.g. by the quit method)
        
        miner.shutdown();
        miningThread.join();
        
        node.shutdown();
        nodeThread.join();
        
        return 0;    
    }
    catch(std::exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
}
