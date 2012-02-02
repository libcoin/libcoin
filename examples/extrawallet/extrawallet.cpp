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

#include "coinWallet/Wallet.h"
#include "coinWallet/WalletRPC.h"

#include <boost/thread.hpp>

using namespace std;
using namespace boost;

// The derived classes below are only to get other class names (using the auto rpc name feature)
// I will put adding a "setName" method to the Method class on the todo. 
class ExtraGetBalance : public GetBalance {
public:
    ExtraGetBalance(Wallet& wallet) : GetBalance(wallet) {}
};

class ExtraSendToAddress : public GetBalance {
public:
    ExtraSendToAddress(Wallet& wallet) : GetBalance(wallet) {}
};

/// extrawallet shows how to create a client with two wallets. 
/// Once running, you can access your extra wallet using the RPC interface:
///     ./extrawallet extragetbalance
/// And youy normal wallet by:
///     ./extrawallet getbalance
/// I'll leave the generalization to an n-wallet gui application to the reader ;)

int main(int argc, char* argv[])
{    
    logfile = CDB::dataDir(bitcoin.dataDirSuffix()) + "/debug.log";

    Node node; // deafult chain is bitcoin
    
    Wallet wallet(node, "wallet.dat"); // add the wallet
    Wallet extra_wallet(node, "extra_wallet.dat"); // add the extra wallet
    
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
    server.registerMethod(method_ptr(new ExtraGetBalance(wallet)));
    server.registerMethod(method_ptr(new ExtraSendToAddress(wallet)), Auth("username","password"));
    server.run();

    node.shutdown();
    nodeThread.join();
}

