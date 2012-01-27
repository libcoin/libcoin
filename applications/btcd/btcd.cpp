
#include "coinChain/Node.h"

#include "coinHTTP/Server.h"

#include "proxy.h"

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
    logfile = CDB::dataDir(bitcoin.dataDirSuffix()) + "/debug.log";
    Node node(bitcoin); // it is also here we specify the use of a proxy!
    
    // Wallet wallet(node); // the blockChain is a source for tx'es and if they are spent
    // establish a connection node.registerTxAcceptObserver(wallet) - Wallet is a functor: wallet(transaction);
    // establish a connection node.registerBlkAcceptObserver(wallet) - Wallet is a functor: wallet(block);
    // now we need to establish a connection from the wallet to the node to post txes and blocks:
    // node.post(Transaction) - post a transaction using io_service.post
    // node.post(Block) - do with block
    // optionally we can register a completionhandler that will be called on success...
    // The first two, registerObservers, can be done in the constructor. The latter, the node.post can be done either as a post object: return from node a reference to an object that when called posts the stuff to node - would be nice to ensure the mediation from one thread to another...

    
    // Explorer explorer(node.blockChain()); // this will register callbacks into the blockchain like:
    //  _blockChain.registerAcceptBlockNotifier(bind(&Explorer::updateBlock, this, placeholder::block));
    //  _blockChain.registerAcceptTransactionNotifier(bind(&Explorer::updateTransaction, this, placeholder::transaction));
    
    thread nodeThread(&Node::run, &node); // run this as a background thread
            
    Server server("0.0.0.0", "9332", filesystem::initial_path().string());
    server.registerMethod(method_ptr(new GetDebit(node.blockChain())));
    server.registerMethod(method_ptr(new GetCredit(node.blockChain())));
    server.registerMethod(method_ptr(new GetTxDetails(node.blockChain())));
    // Auth auth("rpcuser", "coined");
    // server.registerMethod(method_ptr(new GetBalance(wallet)), auth);
    // server.registerMethod(method_ptr(new GetBalance(wallet)), auth);
    
    server.run();

    // getting here means that we have exited from the server (e.g. by the quit method)
    node.shutdown();
    nodeThread.join();
    
    return 0;    
}

