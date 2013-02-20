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

//#include <coinMine/Miner.h>
//#include <coinMine/MinerRPC.h>

#include <coinNAT/PortMapper.h>

#include <coin/Logger.h>

#include <boost/thread.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string/split.hpp>

#include <fstream>

using namespace std;
using namespace boost;
using namespace boost::program_options;
using namespace json_spirit;

/// Pool is an interface for running a pool. It is a basis for the pool rpc commands: 'getwork', 'getblocktemplate' and 'submitblock'
/// The Pool requests a template from the blockchain - i.e. composes a block candidate from the unconfirmed transactions. It stores the
/// candidate mapped by its merkletree root.
/// A solved block submitted either using submitblock or getwork looks up the block and replaces the nonce/time/coinbase
class Pool {
public:
    /// Initialize Pool with a node and a payee, i.e. an abstraction of a payee address generator
    Pool(Node& node, Payee& payee) : _node(node), _blockChain(node.blockChain()), _payee(payee) {
        // install a blockfilter to be notified each time a new block arrives invalidating the current mining effort
    }

    // map of merkletree hash to block templates
    typedef std::map<uint256, Block> BlockTemplates;

    virtual Block getBlockTemplate() {
        BlockChain::Payees payees;
        payees.push_back(_payee.current_script());
        Block block = _blockChain.getBlockTemplate(payees);
        // add the block to the list of work
        return block;
    }
    
    virtual bool submitBlock(const Block& header, const Script coinbase = Script()) {
        // Get saved block
        Pool::BlockTemplates::iterator templ = _templates.find(header.getMerkleRoot());
        if (templ == _templates.end())
            return false;
        Block& block = templ->second;
        
        block.setTime(header.getTime());
        block.setNonce(header.getNonce());

        if (coinbase.size()) {
            Input coinbase_input = block.getTransaction(0).getInput(0);
            coinbase_input.signature(coinbase);
            block.getTransaction(0).replaceInput(0, coinbase_input);
            block.updateMerkleTree();
        }
        
        // check that the work is indeed valid
        uint256 hash = block.getHash();
        
        if (hash > target())
            return false;
        
        cout << "Block with hash: " << hash.toString() << " mined" << endl;
        
        // else ... we got a valid block!
        if (hash > CBigNum().SetCompact(block.getBits()).getuint256() )
            _node.post(block);

        return true;
    }
    
    typedef pair<Block, uint256> Work;

    virtual Work getWork() {
        unsigned int now = GetTime();
        uint256 prev = _blockChain.getBestChain();
        if (_templates.empty() || prev != _latest_work->second.getPrevBlock() || _latest_work->second.getTime() + 30 > now) {
            if (_templates.empty() || prev != _latest_work->second.getPrevBlock()) {
                _templates.clear();
                _latest_work = _templates.end(); // reset _latest_work
            }
            BlockChain::Payees payees;
            payees.push_back(_payee.current_script());
            Block block = _blockChain.getBlockTemplate(payees);
            _latest_work = _templates.insert(make_pair(block.getMerkleRoot(), block)).first;
        }
        // note that if called more often that each second there will be dublicate searches
        _latest_work->second.setTime(now);
        _latest_work->second.setNonce(0);
        
        return Work(_latest_work->second, target());
    }
    
    uint256 target() const {
        CBigNum t = CBigNum().SetCompact(_latest_work->second.getBits());
        t *= 10000000;
        return t.getuint256();
    }
    
    boost::asio::io_service& get_io_service() { return _node.get_io_service(); }
    
private:
    Node& _node;
    const BlockChain& _blockChain;
    Payee& _payee;
    BlockTemplates _templates;
    BlockTemplates::iterator _latest_work;
};

class PoolMethod : public Method {
public:
    PoolMethod(Pool& pool) : _pool(pool) {}

    virtual void dispatch(const CompletionHandler& f) {
        _pool.get_io_service().dispatch(f);
    }
    
    void formatHashBuffers(Block& block, char* pmidstate, char* pdata, char* phash1);
    int formatHashBlocks(void* pbuffer, unsigned int len);
    void sha256Transform(void* pstate, void* pinput, const void* pinit);
    inline uint32_t byteReverse(uint32_t value) {
        value = ((value & 0xFF00FF00) >> 8) | ((value & 0x00FF00FF) << 8);
        return (value<<16) | (value>>16);
    }

protected:
    Pool& _pool;
};

static const unsigned int pSHA256InitState[8] =
{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

void PoolMethod::formatHashBuffers(Block& block, char* pmidstate, char* pdata, char* phash1)
{
    //
    // Pre-build hash buffers
    //
    struct {
        struct unnamed2 {
            int nVersion;
            uint256 hashPrevBlock;
            uint256 hashMerkleRoot;
            unsigned int nTime;
            unsigned int nBits;
            unsigned int nNonce;
        }
        block;
        unsigned char pchPadding0[64];
        uint256 hash1;
        unsigned char pchPadding1[64];
    }
    tmp;
    memset(&tmp, 0, sizeof(tmp));
    
    tmp.block.nVersion       = block.getVersion();
    tmp.block.hashPrevBlock  = block.getPrevBlock();
    tmp.block.hashMerkleRoot = block.getMerkleRoot();
    tmp.block.nTime          = block.getTime();
    tmp.block.nBits          = block.getBits();
    tmp.block.nNonce         = block.getNonce();
    
    formatHashBlocks(&tmp.block, sizeof(tmp.block));
    formatHashBlocks(&tmp.hash1, sizeof(tmp.hash1));
    
    // Byte swap all the input buffer
    for (unsigned int i = 0; i < sizeof(tmp)/4; i++)
        ((unsigned int*)&tmp)[i] = byteReverse(((unsigned int*)&tmp)[i]);
    
    // Precalc the first half of the first hash, which stays constant
    sha256Transform(pmidstate, &tmp.block, pSHA256InitState);
    
    memcpy(pdata, &tmp.block, 128);
    memcpy(phash1, &tmp.hash1, 64);
}

int PoolMethod::formatHashBlocks(void* pbuffer, unsigned int len) {
    unsigned char* pdata = (unsigned char*)pbuffer;
    unsigned int blocks = 1 + ((len + 8) / 64);
    unsigned char* pend = pdata + 64 * blocks;
    memset(pdata + len, 0, 64 * blocks - len);
    pdata[len] = 0x80;
    unsigned int bits = len * 8;
    pend[-1] = (bits >> 0) & 0xff;
    pend[-2] = (bits >> 8) & 0xff;
    pend[-3] = (bits >> 16) & 0xff;
    pend[-4] = (bits >> 24) & 0xff;
    return blocks;
}

void PoolMethod::sha256Transform(void* pstate, void* pinput, const void* pinit) {
    SHA256_CTX ctx;
    unsigned char data[64];
    
    SHA256_Init(&ctx);
    
    for (int i = 0; i < 16; i++)
        ((uint32_t*)data)[i] = byteReverse(((uint32_t*)pinput)[i]);
    
    for (int i = 0; i < 8; i++)
        ctx.h[i] = ((uint32_t*)pinit)[i];
    
    SHA256_Update(&ctx, data, sizeof(data));
    for (int i = 0; i < 8; i++)
        ((uint32_t*)pstate)[i] = ctx.h[i];
}


class GetWork : public PoolMethod {
public:
    GetWork(Pool& pool) : PoolMethod(pool) {}
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);
};

Value GetWork::operator()(const Array& params, bool fHelp) {
    if (fHelp || params.size() > 1)
        throw RPC::error(RPC::invalid_params, "getwork [data]\n"
                         "If [data] is not specified, returns formatted hash data to work on:\n"
                         "  \"midstate\" : precomputed hash state after hashing the first half of the data (DEPRECATED)\n" // deprecated
                         "  \"data\" : block data\n"
                         "  \"hash1\" : formatted hash buffer for second hash (DEPRECATED)\n" // deprecated
                         "  \"target\" : little endian hash target\n"
                         "If [data] is specified, tries to solve the block and returns true if it was successful.");

    if (params.size() == 0) {
        // Update block
        Block block;
        uint256 target;
        tie(block, target) = _pool.getWork();
        
        // Pre-build hash buffers
        char pmidstate[32];
        char pdata[128];
        char phash1[64];
        formatHashBuffers(block, pmidstate, pdata, phash1);
        
        Object result;
        result.push_back(Pair("midstate", HexStr(BEGIN(pmidstate), END(pmidstate)))); // deprecated
        result.push_back(Pair("data",     HexStr(BEGIN(pdata), END(pdata))));
        result.push_back(Pair("hash1",    HexStr(BEGIN(phash1), END(phash1)))); // deprecated
        result.push_back(Pair("target",   HexStr(BEGIN(target), END(target))));
        return result;
    }
    else {
        // Parse parameters
        vector<unsigned char> data = ParseHex(params[0].get_str());
        if (data.size() != 128)
            throw RPC::error(RPC::invalid_params, "Invalid parameter");
        
        Block* header = (Block*)&data[0];

        // Byte reverse
        for (int i = 0; i < 128/4; i++)
            ((unsigned int*)header)[i] = byteReverse(((unsigned int*)header)[i]);
        
        return _pool.submitBlock(*header);
    }
}


class GetBlockTemplate : public PoolMethod {
public:
    enum Mode {
        Single,
        Shared
    };
    GetBlockTemplate(Pool& pool, Mode mode) : PoolMethod(pool), _mode(mode) {}
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);
private:
    Mode _mode;
};

Value GetBlockTemplate::operator()(const Array& params, bool fHelp) {
    if (fHelp || params.size() != 0)
        throw RPC::error(RPC::invalid_params, "getblocktemplate\n"
                         "Returns an object containing various state info.");
    
    Block block;
    // generate a block template - depending on the mode generate different coinbase output.
    if (_mode == Single) {
        BlockChain::Payees payees;
        //        payees.push_back(_payee.current_script());
        //        block = _pool.getBlockTemplate(payees);
    }
    else if (_mode == Shared) {
        // get the last 100 blocks
        //        _node.blockChain().;
    }
    
    
    
    Object obj;

    return obj;
}
/*
class Tx5Method : public NodeMethod {
public:
    class Listener : public TransactionFilter::Listener {
    public:
        Listener() : TransactionFilter::Listener(), _counter(0) {}
        
        virtual void operator()(const Transaction& txn) {
            
        }
        virtual ~Listener() {}
    private:
        size_t _counter;
    };

    
    Tx5Method(Node& node) : NodeMethod(node) {}
    
    
    
    /// dispatch installs a transaction filter that, after 5 transactions will call the handler
    virtual void dispatch(const CompletionHandler& f) {
        _pool.get_io_service().dispatch(f);
    }

    
};
*/
int main(int argc, char* argv[])
{
    try {
        string config_file, data_dir;
        unsigned short port, rpc_port;
        string rpc_bind, rpc_connect, rpc_user, rpc_pass;
        typedef vector<string> strings;
        strings rpc_params;
        string proxy;
        strings connect_peers;
        strings add_peers;
        bool portmap, gen, ssl;
        unsigned int timeout;
        string certchain, privkey;

        // Commandline options
        options_description generic("Generic options");
        generic.add_options()
            ("help,?", "Show help messages")
            ("version,v", "print version string")
            ("conf,c", value<string>(&config_file)->default_value("bitcoin.conf"), "Specify configuration file")
            ("datadir", value<string>(&data_dir), "Specify non default data directory")
        ;
        
        options_description config("Config options");
        config.add_options()
            ("pid", value<string>(), "Specify pid file (default: bitcoind.pid)")
            ("nolisten", "Don't accept connections from outside")
            ("portmap", value<bool>(&portmap)->default_value(true), "Use IGD-UPnP or NATPMP to map the listening port")
            ("upnp", value<bool>(&portmap), "Use UPnP to map the listening port - deprecated, use portmap")
            ("testnet", "Use the test network")
            ("proxy", value<string>(&proxy), "Connect through socks4 proxy")
            ("timeout", value<unsigned int>(&timeout)->default_value(5000), "Specify connection timeout (in milliseconds)")
            ("addnode", value<strings>(&add_peers), "Add a node to connect to")
            ("connect", value<strings>(&connect_peers), "Connect only to the specified node")
            ("port", value<unsigned short>(&port)->default_value(8333), "Listen on specified port for the p2p protocol")
            ("rpcuser", value<string>(&rpc_user), "Username for JSON-RPC connections")
            ("rpcpassword", value<string>(&rpc_pass), "Password for JSON-RPC connections")
            ("rpcport", value<unsigned short>(&rpc_port)->default_value(8332), "Listen for JSON-RPC connections on <arg>")
            ("rpcallowip", value<string>(&rpc_bind)->default_value(asio::ip::address_v4::loopback().to_string()), "Allow JSON-RPC connections from specified IP address")
            ("rpcconnect", value<string>(&rpc_connect)->default_value(asio::ip::address_v4::loopback().to_string()), "Send commands to node running on <arg>")
            ("keypool", value<unsigned short>(), "Set key pool size to <arg>")
            ("rescan", "Rescan the block chain for missing wallet transactions")
            ("gen", value<bool>(&gen)->default_value(false), "Generate coins")
            ("rpcssl", value<bool>(&ssl)->default_value(false), "Use OpenSSL (https) for JSON-RPC connections")
            ("rpcsslcertificatechainfile", value<string>(&certchain)->default_value("server.cert"), "Server certificate file")
            ("rpcsslprivatekeyfile", value<string>(&privkey)->default_value("server.pem"), "Server private key")
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
            data_dir = CDB::dataDir(bitcoin.dataDirSuffix());
        
        std::ofstream olog((data_dir + "/debug.log").c_str(), std::ios_base::out|std::ios_base::app);
        Logger::instantiate(olog);
        Logger::label_thread("main");
        
        // if present, parse the config file - if no data dir is specified we always assume bitcoin chain at this stage 
        string config_path = data_dir + "/" + config_file;
        ifstream ifs(config_path.c_str());
        if(ifs) {
            store(parse_config_file(ifs, config_file_options, true), args);
            notify(args);
        }

        Auth auth(rpc_user, rpc_pass); // if rpc_user and rpc_pass are not set, all authenticated methods becomes disallowed.
        
        // If we have params on the cmdline we run as a command line client contacting a server
        if (args.count("params")) {
            string rpc_method = rpc_params[0];
            rpc_params.erase(rpc_params.begin());
            // create URL
            string url = "http://" + rpc_connect + ":" + lexical_cast<string>(rpc_port);
            if(ssl) url = "https://" + rpc_connect + ":" + lexical_cast<string>(rpc_port); 
            Client client;
            // this is a blocking post!
            Reply reply = client.post(url, RPC::content(rpc_method, rpc_params), auth.headers());
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

        const Chain* chain_chooser;
        if(args.count("testnet"))
            chain_chooser = &testnet;
        else
            chain_chooser = &bitcoin;
        const Chain& chain(*chain_chooser);

        if(!args.count("datadir"))
            data_dir = CDB::dataDir(chain.dataDirSuffix());
        
        logfile = data_dir + "/debug.log";
        
        asio::ip::tcp::endpoint proxy_server;
        if(proxy.size()) {
            vector<string> host_port; split(host_port, proxy, is_any_of(":"));
            if(host_port.size() < 2) host_port.push_back("1080"); 
            proxy_server = asio::ip::tcp::endpoint(asio::ip::address_v4::from_string(host_port[0]), lexical_cast<short>(host_port[1]));
        }
        Node node(chain, data_dir, args.count("nolisten") ? "" : "0.0.0.0", lexical_cast<string>(port), proxy_server, timeout); // it is also here we specify the use of a proxy!
        node.setClientVersion("libcoin/bitcoind", vector<string>(), 59100);
        node.verification(Node::MINIMAL);
        node.validation(Node::NONE);
        node.persistence(Node::LAZY);
        //        node.readBlockFile("/Users/gronager/.microbits/");

        PortMapper mapper(node.get_io_service(), port); // this will use the Node call
        if(portmap) mapper.start();
        
        // use the connect and addnode options to restrict and supplement the irc and endpoint db.
        for(strings::iterator ep = add_peers.begin(); ep != add_peers.end(); ++ep) node.addPeer(*ep);
        for(strings::iterator ep = connect_peers.begin(); ep != connect_peers.end(); ++ep) node.connectPeer(*ep);
        
        Wallet wallet(node, "wallet.dat", data_dir); // this will also register the needed callbacks
        
        if(args.count("rescan")) {
            //            wallet.ScanForWalletTransactions();
            log_warn("Scanned for wallet transactions - need to be implemented!");
        }
        
        thread nodeThread(&Node::run, &node); // run this as a background thread

        CReserveKey reservekey(&wallet);
        
        //        Miner miner(node, reservekey);
        //        miner.setGenerate(gen);
        //        thread miningThread(&Miner::run, &miner);
        
        Server server(rpc_bind, lexical_cast<string>(rpc_port), filesystem::initial_path().string());
        if(ssl) server.setCredentials(data_dir, certchain, privkey);
        
        // Register Server methods.
        server.registerMethod(method_ptr(new Stop(server)), auth);
        
        // Register Node methods.
        server.registerMethod(method_ptr(new GetBlockHash(node)));
        server.registerMethod(method_ptr(new GetBlock(node)));
        server.registerMethod(method_ptr(new GetBlockCount(node)));
        server.registerMethod(method_ptr(new GetConnectionCount(node)));
        server.registerMethod(method_ptr(new GetDifficulty(node)));
        server.registerMethod(method_ptr(new GetInfo(node)));

        /// The Pool enables you to run a backend for a miner, i.e. your private pool, it also enables you to participate in the "Name of Pool"
        // We need a list of blocks for the shared mining
        //
        ChainAddress address("17uY6a7zUidQrJVvpnFchD1MX1untuAufd");
        StaticPayee payee(address.getPubKeyHash());
        Pool pool(node, payee);
        
        server.registerMethod(method_ptr(new GetWork(pool)));
        //server.registerMethod(method_ptr(new GetBlockTemplate(pool)));
        //server.registerMethod(method_ptr(new SubmitBlock(pool)));
        
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
        //server.registerMethod(method_ptr(new SetGenerate(miner)), auth);
        //server.registerMethod(method_ptr(new GetGenerate(miner)), auth);
        //server.registerMethod(method_ptr(new GetHashesPerSec(miner)), auth);
        
        try { // we keep the server in its own exception scope as we want the other threads to shut down properly if the server exits
            server.run();    
        } catch(std::exception& e) { 
            cerr << "Error: " << e.what() << endl; 
        }

        log_info("Server exited, shutting down Node and Miner...\n");
        // getting here means that we have exited from the server (e.g. by the quit method)
        
        //miner.shutdown();
        //miningThread.join();
        
        node.shutdown();
        nodeThread.join();
        
        return 0;    
    }
    catch(std::exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
}