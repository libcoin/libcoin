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
    virtual const int protocol_version() const { return 00000001; } // 0.0.0.1
    virtual const Block& genesisBlock() const ;
    virtual const uint256& genesisHash() const { return _genesis; }
    virtual const int64_t subsidy(unsigned int height, uint256 prev = uint256(0)) const ;
    virtual bool isStandard(const Transaction& tx) const ;
    virtual const CBigNum proofOfWorkLimit() const { return CBigNum(~uint256(0) >> 20); }
    virtual int nextWorkRequired(BlockIterator blk) const;
    virtual const bool checkProofOfWork(const Block& block) const;
    virtual bool checkPoints(const unsigned int height, const uint256& hash) const { return true; }
    virtual unsigned int totalBlocksEstimate() const { return 0; }
    
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

PonziChain::PonziChain() : Chain("ponzicoin", "PZC", 8) {
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
    _genesisBlock = Block(1, uint256(0), uint256(0), Jan25th2012atnoon, 0x20000006, 3600743);

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

const int64_t PonziChain::subsidy(unsigned int height, uint256 prev) const {
    int64_t s = 50 * COIN;
    
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

int PonziChain::nextWorkRequired(BlockIterator blk) const {
    const int64_t nTargetTimespan = 2 * 60 * 60; // two hours
    const int64_t nTargetSpacing = 1 * 60; // one block a minute !
    const int64_t nInterval = nTargetTimespan / nTargetSpacing;
    
    // Genesis block
    int h = blk.height();
    if (h == 0) // trick to test that it is asking for the genesis block
        return proofOfWorkLimit().GetCompact();
    
    // Only change once per interval
    if ((h + 1) % nInterval != 0)
        return blk->bits;
    
    // Go back by what we want to be 14 days worth of blocks
    BlockIterator former = blk - (nInterval-1);
    
    // Limit adjustment step
    int nActualTimespan = blk->time - former->time;
    log_debug("  nActualTimespan = %"PRI64d"  before bounds", nActualTimespan);
    if (nActualTimespan < nTargetTimespan/4)
        nActualTimespan = nTargetTimespan/4;
    if (nActualTimespan > nTargetTimespan*4)
        nActualTimespan = nTargetTimespan*4;
    
    // Retarget
    CBigNum bnNew;
    bnNew.SetCompact(blk->bits);
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;
    
    if (bnNew > proofOfWorkLimit())
        bnNew = proofOfWorkLimit();
    
    /// debug print
    log_debug("GetNextWorkRequired RETARGET");
    log_debug("nTargetTimespan = %"PRI64d"    nActualTimespan = %"PRI64d"", nTargetTimespan, nActualTimespan);
    log_debug("Before: %08x  %s", blk->bits, CBigNum().SetCompact(blk->bits).getuint256().toString().c_str());
    log_debug("After:  %08x  %s", bnNew.GetCompact(), bnNew.getuint256().toString().c_str());
    
    return bnNew.GetCompact();

}

const bool PonziChain::checkProofOfWork(const Block& block) const {
    uint256 hash = block.getHash();
    unsigned int nBits = block.getBits();
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);
    
    // Check range
    if (proofOfWorkLimit() != 0 && (bnTarget <= 0 || bnTarget > proofOfWorkLimit()))
        return ::error("CheckProofOfWork() : nBits below minimum work");
    
    // Check proof of work matches claimed amount
    if (hash > bnTarget.getuint256())
        return ::error("CheckProofOfWork() : hash doesn't match nBits");
    
    return true;
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
        bool gen;
        strings add_peers, connect_peers;
        boost::program_options::options_description extra("Extra options");
        extra.add_options()
        ("generate", boost::program_options::value<bool>(&gen)->default_value(true), "Generate block using the poor mans miner")
        ("addnode", boost::program_options::value<strings>(&add_peers), "Add a node to connect to")
        ("connect", boost::program_options::value<strings>(&connect_peers), "Connect only to the specified node")
        ;
        
        Configuration conf(argc, argv, extra);
        
        Auth auth(conf.user(), conf.pass()); // if rpc_user and rpc_pass are not set all authenticated methods becomes disallowed.
        
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
        string data_dir = default_data_dir(ponzicoin.name());
        Node node(ponzicoin, data_dir, conf.listen()); // it is also here we specify the use of a proxy!
        node.setClientVersion("libcoin/bitcoind", vector<string>());
        node.verification(conf.verification());
        node.validation(Node::MINIMAL);
        node.persistence(conf.persistance());
        node.searchable(conf.searchable());
        
        // use the connect and addnode options to restrict and supplement the irc and endpoint db.
        for(strings::iterator ep = add_peers.begin(); ep != add_peers.end(); ++ep) node.addPeer(*ep);
        for(strings::iterator ep = connect_peers.begin(); ep != connect_peers.end(); ++ep) node.connectPeer(*ep);
        
        Wallet wallet(node); // this will also register the needed callbacks
                
        thread nodeThread(&Node::run, &node); // run this as a background thread
        
        CReserveKey reservekey(&wallet);
        
        Server server(conf.bind(), lexical_cast<string>(conf.port()), filesystem::initial_path().string());
        
        // Register Server methods.
        server.registerMethod(method_ptr(new Stop(server)), auth);
        
        // Register Node methods.
        server.registerMethod(method_ptr(new GetBlockCount(node)));
        server.registerMethod(method_ptr(new GetConnectionCount(node)));
        server.registerMethod(method_ptr(new GetDifficulty(node)));
        server.registerMethod(method_ptr(new GetInfo(node)));
        
        /// The Pool enables you to run a backend for a miner, i.e. your private pool, it also enables you to participate in the "Name of Pool"
        // We need a list of blocks for the shared mining
        ChainAddress address("17uY6a7zUidQrJVvpnFchD1MX1untuAufd");
        StaticPayee payee(address.getPubKeyHash());
        Pool pool(node, payee);
        
        server.registerMethod(method_ptr(new GetWork(pool)));
        server.registerMethod(method_ptr(new GetBlockTemplate(pool)));
        server.registerMethod(method_ptr(new SubmitBlock(pool)));
        
        Miner miner(pool);
        miner.setGenerate(false);
        miner.setGenerate(gen);
        thread miningThread(&Miner::run, &miner);
        
        server.run();
        
        log_info("Server exitted, shutting down Node and Miner...\n");
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