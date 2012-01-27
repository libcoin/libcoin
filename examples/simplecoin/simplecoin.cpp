
#include "coinChain/Node.h"
#include "coinChain/NodeRPC.h"

#include "coinHTTP/Server.h"

#include "coinWallet/wallet.h"
#include "coinWallet/walletrpc.h"

#include <boost/thread.hpp>

using namespace std;
using namespace boost;

int main(int argc, char* argv[])
{    
    logfile = CDB::dataDir(bitcoin.dataDirSuffix()) + "/debug.log";
    
    Node node; // deafult chain is bitcoin
    
    Wallet wallet(node); // add the wallet
    
    thread nodeThread(&Node::run, &node); // run this as a background thread
    
    Server server;
    
    // Register Server methods.
    server.registerMethod(method_ptr(new Stop(server)));
    
    // Register Node methods.
    server.registerMethod(method_ptr(new GetBlockCount(node)));
    server.registerMethod(method_ptr(new GetConnectionCount(node)));
    server.registerMethod(method_ptr(new GetDifficulty(node)));
    server.registerMethod(method_ptr(new GetInfo(node)));
    
    // Register Wallet methods. - note that we don't have any auth, so anyone (on localhost) can read your balance!
    server.registerMethod(method_ptr(new GetBalance(wallet)));
    server.registerMethod(method_ptr(new SendToAddress(wallet)), Auth("username","password"));

    server.run();

    node.shutdown();
    nodeThread.join();
    
    return 0;    
}