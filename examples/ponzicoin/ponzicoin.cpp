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
#include <coinHTTP/Client.h>

#include <coinWallet/Wallet.h>
#include <coinWallet/WalletRPC.h>

#include <coinMine/Miner.h>
#include <coinMine/MinerRPC.h>

#include <boost/thread.hpp>
#include <boost/program_options.hpp>

#include <fstream>

using namespace std;
using namespace boost;
using namespace boost::program_options;
using namespace json_spirit;

class PonziChain : public Chain
{
public:
    PonziChain();
    virtual const Block& genesisBlock() const ;
    virtual const uint256& genesisHash() const { return _genesis; }
    virtual const int64 subsidy(unsigned int height) const ;
    virtual bool isStandard(const Transaction& tx) const ;
    virtual const CBigNum proofOfWorkLimit() const { return CBigNum(~uint256(0) >> 20); }
    virtual unsigned int nextWorkRequired(const CBlockIndex* pindexLast) const ;
    virtual bool checkPoints(const unsigned int height, const uint256& hash) const { return true; }
    virtual unsigned int totalBlocksEstimate() const { return 0; }
    
    virtual const std::string dataDirSuffix() const { return "ponzicoin"; }
    
    //    virtual char networkId() const { return 0xff; } // 0x00, 0x6f, 0x34 (bitcoin, testnet, namecoin)
    virtual ChainAddress getAddress(PubKeyHash hash) const { return ChainAddress(0xff, hash); }
    virtual ChainAddress getAddress(ScriptHash hash) const { return ChainAddress(); }
    virtual ChainAddress getAddress(std::string str) const {
        ChainAddress addr(str);
        if(addr.version() == 0xff)
            addr.setType(ChainAddress::PUBKEYHASH);
        return addr;
    }

    virtual const MessageStart& messageStart() const { return _messageStart; };
    virtual short defaultPort() const { return 5247; }
    
    virtual std::string ircChannel() const { return "ponzicoin"; }
    virtual unsigned int ircChannels() const { return 1; } // number of groups to try (100 for bitcoin, 2 for litecoin)
    
private:
    Block _genesisBlock;
    uint256 _genesis;
    MessageStart _messageStart;
};

PonziChain::PonziChain() {
    _messageStart[0] = 0xbe; _messageStart[1] = 0xf9; _messageStart[2] = 0xd9; _messageStart[3] = 0xb4;
    const char* pszTimestamp = "If five-spots were snowflakes, Ponzi would be a three day blizzard";
    Transaction txNew;
    
    // 0x1d00ffff is difficulty 1 corresponding to a hex target of: 
    // 0x00000000FFFF0000000000000000000000000000000000000000000000000000
    // The formula is: target = b * 2^(8*(a-3)), where a = d/0x1000000 and b = d%0x1000000. 
    // So for the example above a=0x1d and b=0xffff. Note that b is signed and hence the largest b is 0x7fffff.
    // For ponzi we want a roughly 100kHash machine to use one minute for a hash. This is roughly 6*16^5 hashes pr minute so the target should be roughly 0x000006... or a bits of: b=6, a=0x20 or d=0x20000006
    Script signature = Script() << 0x20000006 << CBigNum(4) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.addInput(Input(Coin(), signature)); 
    Script script = Script() << OP_DUP << OP_HASH160 << ParseHex("5e5bd04fad1beadbc1dddb1f41c34dc3df527cd6") << OP_EQUALVERIFY << OP_CHECKSIG;
    txNew.addOutput(Output(50 * COIN, script)); 
    int Jan25th2012atnoon = 1327492800;
    _genesisBlock = Block(1, 0, 0, Jan25th2012atnoon, 0x20000006, 3600743);
    _genesisBlock.addTransaction(txNew);
    _genesisBlock.updateMerkleTree(); // genesisBlock

    CBigNum target;
    target.SetCompact(0x20000006);
    uint256 t = target.getuint256();

    // poor mans hasher - we do however already know the right nonce.
    while (_genesisBlock.getHash() > t) {
        _genesisBlock.setNonce(_genesisBlock.getNonce()+1);
    }
    
    _genesis = _genesisBlock.getHash();
    //    assert(_genesisBlock.getHash() == _genesis);
}

const Block& PonziChain::genesisBlock() const {
    return _genesisBlock;
}

const int64 PonziChain::subsidy(unsigned int height) const {
    int64 s = 50 * COIN;
    
    // Subsidy is cut in half every week
    s >>= (height / 10080);
    
    return s;
}

bool PonziChain::isStandard(const Transaction& tx) const {
    BOOST_FOREACH(const Input& txin, tx.getInputs())
    if (!txin.signature().isPushOnly())
        return ::error("nonstandard txin: %s", txin.signature().toString().c_str());
    BOOST_FOREACH(const Output& txout, tx.getOutputs()) {
        vector<pair<opcodetype, valtype> > solution;
        if (!Solver(txout.script(), solution))
            return ::error("nonstandard txout: %s", txout.script().toString().c_str());        
    }
    return true;
}

unsigned int PonziChain::nextWorkRequired(const CBlockIndex* pindexLast) const {
    const int64 nTargetTimespan = 2 * 60 * 60; // two hours
    const int64 nTargetSpacing = 1 * 60; // one block a minute !
    const int64 nInterval = nTargetTimespan / nTargetSpacing;
    
    // Genesis block
    if (pindexLast == NULL)
        return proofOfWorkLimit().GetCompact();
    
    // Only change once per interval
    if ((pindexLast->nHeight+1) % nInterval != 0)
        return pindexLast->nBits;
    
    // Go back by what we want to be 14 days worth of blocks
    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < nInterval-1; i++)
        pindexFirst = pindexFirst->pprev;
    assert(pindexFirst);
    
    // Limit adjustment step
    int64 nActualTimespan = pindexLast->GetBlockTime() - pindexFirst->GetBlockTime();
    printf("  nActualTimespan = %"PRI64d"  before bounds\n", nActualTimespan);
    if (nActualTimespan < nTargetTimespan/4)
        nActualTimespan = nTargetTimespan/4;
    if (nActualTimespan > nTargetTimespan*4)
        nActualTimespan = nTargetTimespan*4;
    
    // Retarget
    CBigNum bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;
    
    if (bnNew > proofOfWorkLimit())
        bnNew = proofOfWorkLimit();
    
    /// debug print
    printf("GetNextWorkRequired RETARGET\n");
    printf("nTargetTimespan = %"PRI64d"    nActualTimespan = %"PRI64d"\n", nTargetTimespan, nActualTimespan);
    printf("Before: %08x  %s\n", pindexLast->nBits, CBigNum().SetCompact(pindexLast->nBits).getuint256().toString().c_str());
    printf("After:  %08x  %s\n", bnNew.GetCompact(), bnNew.getuint256().toString().c_str());
    
    return bnNew.GetCompact();
}

const PonziChain ponzicoin;


/// ponzicoin is almost identical to the bitcoin client the only differences are the lack of
/// the testnet option and the definition of a new Chain that for this example call ponzicoin.
/// For digital payments the bitcoin chain is by far the most superior, but defining a new
/// chain can help you investigate interesting aspect of the bitcoin p2p network and can hence 
/// be valuable for educational purposes.

int main(int argc, char* argv[])
{
    try {
        string config_file, data_dir;
        unsigned short rpc_port;
        string rpc_bind, rpc_connect, rpc_user, rpc_pass;
        typedef vector<string> strings;
        strings rpc_params;
        strings connect_peers;
        strings add_peers;
        bool gen;
        
        // Commandline options
        options_description generic("Generic options");
        generic.add_options()
            ("help,?", "Show help messages")
            ("version,v", "print version string")
            ("conf,c", value<string>(&config_file)->default_value("ponzi.conf"), "Specify configuration file")
            ("datadir", "Specify non default data directory")
            ;
            
            options_description config("Config options");
            config.add_options()
            ("pid", value<string>(), "Specify pid file (default: bitcoind.pid)")
            ("nolisten", "Don't accept connections from outside")
            ("addnode", value<strings>(&add_peers), "Add a node to connect to")
            ("connect", value<strings>(&connect_peers), "Connect only to the specified node")
            ("rpcuser", value<string>(&rpc_user), "Username for JSON-RPC connections")
            ("rpcpassword", value<string>(&rpc_pass), "Password for JSON-RPC connections")
            ("rpcport", value<unsigned short>(&rpc_port)->default_value(5246), "Listen for JSON-RPC connections on <arg>")
            ("rpcallowip", value<string>(&rpc_bind)->default_value(asio::ip::address_v4::loopback().to_string()), "Allow JSON-RPC connections from specified IP address")
            ("rpcconnect", value<string>(&rpc_connect)->default_value(asio::ip::address_v4::loopback().to_string()), "Send commands to node running on <arg>")
            ("keypool", value<unsigned short>(), "Set key pool size to <arg>")
            ("rescan", "Rescan the block chain for missing wallet transactions")
            ("gen", value<bool>(&gen)->default_value(false), "Generate coins")
       ;
        
        options_description hidden("Hidden options");
        hidden.add_options()("params", value<strings>(&rpc_params), "Run JSON RPC command");
        
        options_description cmdline_options;
        cmdline_options.add(generic).add(config).add(hidden);
        
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
            cout << "Usage: " << argv[0] << " [options] [rpc-command param1 param2 ...]\n";
            cout << visible << "\n";
            cout << "If no rpc-command is specified, " << argv[0] << " start up as a daemon, otherwise it offers commandline access to a running instance\n";
            return 1;
        }
        
        if (args.count("version")) {
            cout << argv[0] << " version is: " << FormatVersion(PROTOCOL_VERSION) << "\n";
            return 1;        
        }
        
        if(!args.count("datadir"))
            data_dir = CDB::dataDir(ponzicoin.dataDirSuffix());
        
        // if present, parse the config file - if no data dir is specified we always assume bitcoin chain at this stage 
        string config_path = data_dir + "/" + config_file;
        ifstream ifs(config_path.c_str());
        if(ifs) {
            store(parse_config_file(ifs, config_file_options, true), args);
            notify(args);
        }
        
        Auth auth(rpc_user, rpc_pass); // if rpc_user and rpc_pass are not set all authenticated methods becomes disallowed.
        
        // If we have params on the cmdline we run as a command line client contacting a server
        if (args.count("params")) {
            string rpc_method = rpc_params[0];
            rpc_params.erase(rpc_params.begin());
            // create URL
            string url = "http://" + rpc_connect + ":" + lexical_cast<string>(rpc_port);
            Client client;
            // this is a blocking post!
            Reply reply = client.post(url, RPC::content(rpc_method, rpc_params), auth.headers());
            if(reply.status == Reply::ok) {
                Object rpc_reply = RPC::reply(reply.content);
                Value result = find_value(rpc_reply, "result");
                cout << write_formatted(result) << "\n";
                return 0;
            }
            else {
                cout << "HTTP error code: " << reply.status << "\n";
                Object rpc_reply = RPC::reply(reply.content);
                Object rpc_error = find_value(rpc_reply, "error").get_obj();
                Value code = find_value(rpc_error, "code");
                Value message = find_value(rpc_error, "message");
                cout << "JSON RPC Error code: " << code.get_int() << "\n";
                cout <<  message.get_str() << "\n";
                return 1;
            }
        }
        
        // Else we start the bitcoin node and server!
        
        if(!args.count("datadir"))
            data_dir = CDB::dataDir(ponzicoin.dataDirSuffix());
        
        logfile = data_dir + "/debug.log";
        
        Node node(ponzicoin, data_dir, args.count("nolisten") ? "" : "0.0.0.0"); // it is also here we specify the use of a proxy!
        
        // use the connect and addnode options to restrict and supplement the irc and endpoint db.
        for(strings::iterator ep = add_peers.begin(); ep != add_peers.end(); ++ep) node.addPeer(*ep);
        for(strings::iterator ep = connect_peers.begin(); ep != connect_peers.end(); ++ep) node.connectPeer(*ep);
        
        Wallet wallet(node); // this will also register the needed callbacks
        
        if(args.count("rescan")) {
            wallet.ScanForWalletTransactions();
            printf("Scanned for wallet transactions");
        }
        
        thread nodeThread(&Node::run, &node); // run this as a background thread
        
        CReserveKey reservekey(&wallet);
        
        Miner miner(node, reservekey);
        miner.setGenerate(gen);
        thread miningThread(&Miner::run, &miner);
        
        Server server(rpc_bind, lexical_cast<string>(rpc_port), filesystem::initial_path().string());
        
        // Register Server methods.
        server.registerMethod(method_ptr(new Stop(server)), auth);
        
        // Register Node methods.
        server.registerMethod(method_ptr(new GetBlockCount(node)));
        server.registerMethod(method_ptr(new GetConnectionCount(node)));
        server.registerMethod(method_ptr(new GetDifficulty(node)));
        server.registerMethod(method_ptr(new GetInfo(node)));
        
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
        
        // Register Mining methods.
        server.registerMethod(method_ptr(new SetGenerate(miner)), auth);    
        server.registerMethod(method_ptr(new GetGenerate(miner)), auth);    
        server.registerMethod(method_ptr(new GetHashesPerSec(miner)), auth);    
        
        
        server.run();
        
        printf("Server exitted, shutting down Node and Miner...\n");
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