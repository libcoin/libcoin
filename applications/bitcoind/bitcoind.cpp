
#include "btcNode/Node.h"

#include "btcHTTP/Server.h"

#include "btcWallet/wallet.h"
#include "btcWallet/walletrpc.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/thread.hpp>

#include <exception>
#include <signal.h>

using namespace std;
using namespace boost;

int main(int argc, char* argv[])
{
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
    //    server.registerMethod(method_ptr(new WalletPassphrase(wallet)), auth);
    
    server.run();

    // getting here means that we have exited from the server (e.g. by the quit method)
    node.shutdown();
    nodeThread.join();
    
    return 0;    
}

