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

#include <coinWallet/Wallet.h>
#include <coinWallet/WalletRPC.h>

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