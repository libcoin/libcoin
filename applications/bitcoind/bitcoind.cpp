
#include "btcNode/Node.h"

#include "btcHTTP/Server.h"

#include "btcWallet/wallet.h"
#include "btcWallet/walletrpc.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/thread.hpp>
#include <boost/program_options.hpp>

using namespace std;
using namespace boost;
using namespace boost::program_options;

int main(int argc, char* argv[])
{
    string config_file;
    string data_dir;
    unsigned short rpc_port;
    string rpc_connect;
    // Commandline options
    options_description generic("Generic options");
    generic.add_options()
        ("help,?", "Show help messages")
        ("version,v", "print version string")
        ("conf,c", value<string>(&config_file)->default_value("bitcoin.conf"), "Specify configuration file (default: bitcoin.conf)")
        ("datadir", value<string>(&data_dir)->default_value(""), "Specify data directory")
    ;
    
    options_description config("Config options");
    config.add_options()
        ("pid", value<string>(), "Specify pid file (default: bitcoind.pid)")
        ("nolisten", "Don't accept connections from outside")
        ("testnet", "Use the test network")
        ("rpcuser", value<string>(), "Username for JSON-RPC connections")
        ("rpcpassword", value<string>(), "Password for JSON-RPC connections")
        ("rpcport", value<unsigned short>(&rpc_port)->default_value(8332), "Listen for JSON-RPC connections on <port> (default: 8332)")
        ("rpcallowip", value<string>(), "Allow JSON-RPC connections from specified IP address")
        ("rpcconnect", value<string>(&rpc_connect)->default_value("127.0.0.1"), "Send commands to node running on <ip> (default: 127.0.0.1)")
        ("keypool", value<unsigned short>(), "Set key pool size to <n> (default: 100)")
        ("rescan", "Rescan the block chain for missing wallet transactions")
    ;
    variables_map args;
    store(parse_command_line(argc, argv, config), args);
    
    bool useTestnet = false;
    const Chain* chain_chooser;
    if(useTestnet)
        chain_chooser = &testnet;
    else
        chain_chooser = &bitcoin;
    const Chain& chain(*chain_chooser);
    
    logfile = CDB::dataDir(chain.dataDirSuffix()) + "/debug.log";
    Node node(chain); // it is also here we specify the use of a proxy!
    
    Wallet wallet(node); // this will also register the needed callbacks
    
    thread nodeThread(&Node::run, &node); // run this as a background thread
            
    Server server("0.0.0.0", "9332", filesystem::initial_path().string());
    //    server.registerMethod(method_ptr(new GetTxDetails(node.blockChain())));
    Auth auth("rpcuser", "coined");
    server.registerMethod(method_ptr(new GetBalance(wallet)), auth);
    server.registerMethod(method_ptr(new GetNewAddress(wallet)), auth);
    server.registerMethod(method_ptr(new GetAccountAddress(wallet)), auth);
    server.registerMethod(method_ptr(new SendToAddress(wallet)), auth);
    server.registerMethod(method_ptr(new WalletPassphrase(wallet, server.get_io_service())), auth);
    
    server.run();

    // getting here means that we have exited from the server (e.g. by the quit method)
    node.shutdown();
    nodeThread.join();
    
    return 0;    
}

