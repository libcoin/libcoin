
#include "btcNode/Node.h"
#include "btcNode/main.h"

#include "btcHTTP/Server.h"

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
    Node node("0.0.0.0", "8333"); // it is also here we specify the use of a proxy!
    
    // Wallet wallet(node.blockChain()); // this will register callbacks into the blockchain like:
    //  _blockChain.registerAcceptBlockNotifier(bind(&Wallet::syncWithWallet, this, placeholder::block));
    //  _blockChain.registerAcceptTransactionNotifier(bind(&Wallet::syncWithWallet, this, placeholder::transaction));
    //  _blockChain.registerResendTransactionNotifier(bind(
    
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

