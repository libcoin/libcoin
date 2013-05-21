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

#include <coinPool/GetWork.h>
#include <coinPool/GetBlockTemplate.h>
#include <coinPool/SubmitBlock.h>

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

class SpendablesPool : public Pool {
public:
    SpendablesPool(Node& node, Payee& payee) : Pool(node, payee) {
    }
    Block getBlockTemplate() {
        BlockChain::Payees payees;
        payees.push_back(_payee.current_script());
        Block block = _blockChain.getBlockTemplate(payees);
        // add the block to the list of work
        return block;
    }
};


class TxWait : public NodeMethod {
public:
    class Listener : public TransactionFilter::Listener {
    public:
        Listener() : TransactionFilter::Listener(), _counter(0) {}
        
        virtual void operator()(const Transaction& txn) {
            ++_counter;
            cout << "Got transaction with hash: " << txn.getHash().toString() << endl;
            // search for a handler
            HandlerRange range = _handlers.equal_range(_counter);
            for (Handlers::const_iterator h = range.first; h != range.second; ++h) {
                (h->second)();
            }
            _handlers.erase(_counter);
        }
        virtual ~Listener() {}

        void install(const CompletionHandler& handler, size_t N = 5) {
            // should add some thread guard!
            _handlers.insert(make_pair(_counter + N, handler));
        }

    private:
        size_t _counter;

        typedef std::multimap<size_t, CompletionHandler> Handlers;
        typedef pair<Handlers::const_iterator, Handlers::const_iterator> HandlerRange;
        Handlers _handlers;        
    };
    typedef boost::shared_ptr<Listener> listener_ptr;

    TxWait(Node& node) : NodeMethod(node), _listener(new Listener) {
        _node.subscribe(_listener);
    }
    
    /// dispatch installs a transaction filter that, after 5 transactions will call the handler
    virtual void dispatch(const CompletionHandler& f, const Request& request) {
        RPC rpc(request);
        Array params = rpc.params();
        if (params.size() > 0 && params[0].type() == int_type && params[0].get_int() > 0)
            _listener->install(f, params[0].get_int());
        else
            _listener->install(f);
    }
    
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp) {
        return "DONE!\n";
    }
private:
    listener_ptr _listener;

/*
    static string description =
    json::obj(pair("name", "txwait"), pair("summery", "Waits for N (default 5) transactions"), pair("idempotent", false), pair("params", json::array( ) )
        \"name\"    : \"txwait\",\
        "summary" : "Waits for N (default 5) transactions.",\
        "help"    : "http://www.example.com/service/sum.html",\
        "idempotent" : false,\
        "params"  : [\
            { "name" : "txes", "type" : "num", "default" : 5 },\
        "return" : {\
            "type" : "nil"\
        }\
    "
*/
};

// a pure
class Share : public Block {
public:
    Share() : Block() {}
    
    /// Basic checks to test if this block is (could be) a share
    bool check() const {
        if (getVersion() < 3) // it has to be at least a version 3 block
            return false;
        
        // get the share target and the previous share
        int target = getShareBits();
        uint256 prev = getPrevShare();
        
        if (target == 0 || prev == uint256(0))
            return false;
        
        return true;
    }
    
        
    uint256 getPrevShare() const {
        try {
            Script coinbase = getTransaction(0).getInput(0).signature();
            Script::const_iterator cbi = coinbase.begin();
            opcodetype opcode;
            std::vector<unsigned char> data;
            // We simply ignore the first opcode and data, however, it is the height...
            coinbase.getOp(cbi, opcode, data);
            
            // this should be an opcode for a uint256 (i.e. number of 32 bytes)
            coinbase.getOp(cbi, opcode, data);
            
            // and the prev share (also 32 bytes)
            uint256 hash_from_coinbase(data);
            return hash_from_coinbase;
        } catch (...) {
        }
        return uint256(0);
    }
    
    Script getRewardee(size_t i = 0) const {
        const Transaction& txn = getTransaction(0);
        return txn.getOutput(0).script();
    }
    
};

// ShareRef is a essentially a block header plus some data extracted from the coinbase - namely:
//  * The merkletrie root hash
//  * The previous share hash
//  * The share target / replacing the normal target
//  * The redeemer script (this is the first tx out in the coinbase txn)
/*
struct ShareRef {
    typedef uint256 Hash;
    int version;
    Hash hash;
    Hash prev;
    Hash prev_share;
    Hash merkletrie;
    unsigned int time;
    unsigned int bits;
    Script script;
    
    CBigNum work() const {
        CBigNum target;
        target.SetCompact(bits);
        if (target <= 0)
            return 0;
        return (CBigNum(1)<<256) / (target+1);
    }
    
    ShareRef() : version(3), hash(0), prev(0), prev_share(0), merkletrie(0), time(0), bits(0), script(Script()) {}
    ShareRef(int version, Hash hash, Hash prev, Hash prev_share, Hash merkletrie, unsigned int time, unsigned int bits, const Script& script) : version(version), hash(hash), prev(prev), prev_share(prev_share), merkletrie(merkletrie), time(time), bits(bits), script(script) {}
};

class COINCHAIN_EXPORT ShareTree : public SparseTree<ShareRef> {
public:
    ShareTree() : SparseTree<ShareRef>(), _trimmed(0) {}
    virtual void assign(const Trunk& trunk);
    
    struct Changes {
        Hashes deleted;
        Hashes inserted;
    };
    
    struct Pruned {
        Hashes branches;
        Hashes trunk;
    };
    
    CBigNum accumulatedWork(BlockTree::Iterator blk) const;
    
    Changes insert(const BlockRef ref);
    
    void pop_back();
    
    Iterator find(const BlockRef::Hash hash) const {
        return SparseTree<ShareRef>::find(hash);
    }
    
    Iterator best() const {
        return Iterator(&_trunk.back(), this);
    }
    
    /// Trim the trunk, removing all branches ending below the trim_height
    Pruned trim(size_t trim_height) {
        // ignore branches for now...
        if (trim_height > height())
            trim_height = height();
        Pruned pruned;
        for (size_t h = _trimmed; h < trim_height; ++h)
            pruned.trunk.push_back(_trunk[h].hash);
        _trimmed = trim_height;
        return pruned;
    }
    
    
private:
    typedef std::vector<CBigNum> AccumulatedWork;
    AccumulatedWork _acc_work;
    
    size_t _trimmed;
};

typedef ShareTree::Iterator ShareIterator;


/// The Shares chain is for shared mining (like P2Pool) and for validating the blockchain by version 3 block.
/// It connects to the block chain through block and transaction listeners and to the websocket network to get shares
class ShareChain : public sqliterate::Database {
public:
    
    ShareChain(const Chain& chain, const string dataDir) :
    sqliterate::Database(dataDir == "" ? ":memory:" : dataDir + "/sharechain.sqlite3", getMemorySize()/4),
    _chain(chain) {
        size_t total_memory = getMemorySize();
        // use only 25% of the memory at max:
        const size_t page_size = 4096;
        size_t cache_size = total_memory/4/page_size;
        
        // we need to declare this static as the pointer to the c_str will live also outside this function
        static string pragma_page_size = "PRAGMA page_size = " + lexical_cast<string>(page_size);
        static string pragma_cache_size = "PRAGMA cache_size = " + lexical_cast<string>(cache_size);
        
        query("PRAGMA journal_mode=WAL");
        query("PRAGMA locking_mode=NORMAL");
        query("PRAGMA synchronous=OFF");
        query(pragma_page_size.c_str()); // this is 512MiB of cache with 4kiB page_size
        query(pragma_cache_size.c_str()); // this is 512MiB of cache with 4kiB page_size
        query("PRAGMA temp_store=MEMORY"); // use memory for temp tables
        
        query("CREATE TABLE IF NOT EXISTS Shares ("
              "count INTEGER PRIMARY KEY," // share count is height+1 - i.e. the genesis has count = 1
              "hash BINARY,"
              "version INTEGER,"
              "prev BINARY,"
              "mrkl BINARY,"
              "time INTEGER,"
              "bits INTEGER,"
              "nonce INTEGER"
              ")");

        // keep headers back to the last checkpoint
        // keep coinbase txns back to the last 
        
    }

    /// attach is a dry version of the BlockChain::attach - a spendables state is used as basis for a 
    void attach(ShareIterator shr) {
        Share share = _shares[shr->hash];
        int height = shr.height(); // height for non trunk blocks is negative
        
        for(size_t idx = 0; idx < block.getNumTransactions(); ++idx)
            if(!isFinal(share.getTransaction(idx), height, shr->time))
                throw Error("Contains a non-final transaction");
        
        // get a reference to the spendables from its previous block
        Spendables spendables = _blockChain.getSpendables(share.getPrevBlock());
        
        _verifier.reset();
        
        //insertBlockHeader(blk.count(), block);
        
        // commit transactions
        int64 fees = 0;
        int64 min_fee = 0;
        bool verify = _verification_depth && (height > _verification_depth);
        for(size_t idx = 1; idx < block.getNumTransactions(); ++idx) {
            Transaction txn = block.getTransaction(idx);
            uint256 hash = txn.getHash();
            if (unconfirmed.count(hash) || _claims.have(hash)) // if the transaction is already among the unconfirmed, we have verified it earlier
                verify = false;
            postTransaction(txn, fees, min_fee, blk, idx, verify);
            unconfirmed.erase(hash);
            confirmed.insert(hash);
        }
        // post subsidy - means adding the new coinbase to spendings and the matured coinbase (100 blocks old) to spendables
        Transaction txn = block.getTransaction(0);
        postSubsidy(txn, blk, fees);
        
        if (!_verifier.yield_success())
            throw Error("Verify Signature failed with: " + _verifier.reason());
    }

    void detach(ShareIterator shr) {

    }
    
    void append(Share& share) {
        // first check that the block is indeed a share:
        if (!share.check())
            throw Error("Block is not a valid share");
        
        uint256 hash = share.getHash();
        
        // check that it is not already committed
        ShareIterator shr = _tree.find(hash);
        if (shr != _tree.end())
            throw Error("Share already committed!");
        
        // check that it is connected to the share chain
        ShareIterator prev = _tree.find(share.getPrevShare());
        if (prev == _tree.end())
            throw Error("Cannot accept orphan share");
        
        // check the payout - each former share in the share chain need their share
        // at this stage we don't need to verify that the overall reward is correct, we only need the relative share
        int64 reward = 0;
        const Transaction& cb_txn = share.getTransaction(0);
        for (int i = 0; i < cb_txn.getNumOutputs(); ++i)
            reward += cb_txn.getOutput(i).value();
        int64 fraction = reward/360;
        int64 modulus = reward%360;
        
        // check that the shares are correct
        map<Script, unsigned int> fractions;
        
        unsigned int total = 0;
        for (int i = 1; i < cb_txn.getNumOutputs(); ++i) {
            const Script& script = cb_txn.getOutput(i).script();
            int64 value = cb_txn.getOutput(i).value();
            if (value%fraction)
                throw Error("Wrong fractions in coinbase transaction");
            fractions[script] = value/fraction;
            
            total += value/fraction;
        }
        // now handle the reward to the share creater
        const Script& script = cb_txn.getOutput(0).script();
        int64 value = cb_txn.getOutput(0).value();
        
        value -= modulus;
        if (value%fraction)
            throw Error("Wrong fractions in coinbase transaction 0");
        
        fractions[script] = value/fraction - 1;

        // now check that this matches with the numbers in the recent_fractions
        if (fractions != _fractions)
            throw Error("Fractions does not match the recent share fractions");
        
        // the coinbase_txn is validated - proceed to check if it is a new best in the share tree
        ShareTree::Changes changes = _tree.insert(BlockRef(share.getVersion(), hash, prev->hash, block.getBlockTime(), block.getBits()));
        
        _shares[hash] = share;
        
        if (changes.inserted.size() == 0)
            return;
        
        try {            
            // note that a change set is like a patch - it contains the blockrefs to remove and the blockrefs to add
            
            // loop over deleted shares and detach them
            for (BlockTree::Hashes::const_iterator h = changes.deleted.begin(); h != changes.deleted.end(); ++h) {
                ShareIterator shr = _tree.find(*h);
                detach(shr);
            }
            
            // loop over inserted blocks and attach them
            for (int i = changes.inserted.size() - 1; i >= 0; --i) {
                shr = _tree.find(changes.inserted[i]);
                attach(shr);
            }
            
            // we have a commit - also commit the transactions to the merkle trie, which happens automatically when spendables goes out of scope
            
            // delete inserted blocks from the _branches
            for (BlockTree::Hashes::const_iterator h = changes.inserted.begin(); h != changes.inserted.end(); ++h)
                _shares.erase(*h);
        }
        catch (std::exception& e) {
            query("ROLLBACK --BLOCK");
            _tree.pop_back();
            for (BlockTree::Hashes::const_iterator h = changes.deleted.begin(); h != changes.deleted.end(); ++h)
                _shares.erase(*h);
            
            _spendables = snapshot; // this will restore the Merkle Trie to its former state
            
            throw Error(string("append(Block): ") + e.what());
        }
        
    }
private:
    const Chain& _chain;
    ShareTree _tree;
    
    typedef std::map<uint256, Share> Shares;
    Shares _shares;
    
    // a multi index map to lookup and sort Shares by count and script
    typedef std::map<Script, unsigned int> Fractions;
    Fractions _fractions;
};

*/

class Miner : private boost::noncopyable {
public:
    Miner(Pool& pool) : _pool(pool), _io_service(), _generate(true) {}
    
    // run the miner
    void run() {
        // get a block candidate
        _io_service.run();
    }
    
    void setGenerate(bool gen) {
        if (gen^_generate) {
            _generate = gen;
            if (_generate)
                _io_service.post(boost::bind(&Miner::mine, this));
        }
    }
    
    bool getGenerate() const {
        return _generate;
    }
    
    /// Shutdown the Node.
    void shutdown() { _io_service.dispatch(boost::bind(&Miner::handle_stop, this)); }
    
private:
    void handle_stop() {
        _io_service.stop();
    }
    
    // return true on success
    bool mine() {
        int tries = 65536;
        
        Block block = _pool.getBlockTemplate();

        uint256 target = _pool.target();
        
        // mine - i.e. adjust the nonce and calculate the hash - note this is a totally crappy mining implementation! Don't used except for testing
        while (block.getHash() > target && tries--)
            block.setNonce(block.getNonce() + 1);

        if (block.getHash() <= target)
            _pool.submitBlock(block);

        if (_generate)
            _io_service.post(boost::bind(&Miner::mine, this));
    }
    
protected:
    Pool& _pool;
    
    boost::asio::io_service _io_service;
            
    bool _generate;
};

void calcDividend () {
    // test function
    // we maintain a sharetree for the last 144 blocks
    // the sharetree is up to 1440 blocks long (could be longer, but only 1440 is used)
    // the sharetree has two targets before and after the block%144
    // work is the accumulated work for the last 144 blocks
    // there can be several sharetrees - the longest is considered the "best"
    // the sharetrees can be multirooted!
    // each new share attaches to the share tree as insert(share), which might result in a reorg of the sharetree, but that dosn't matter for the transactions
    // we calc the dividend as: all, up to 1440 blocks, of the sharetree
    // The sharetree is only stored in memory
    // The sharetree can also use the sparse tree (keeping only the best chain indexed) ?
    
}

// A structure to hold the ShareChains - First a ShareTree:
class ShareTree {
public:
    typedef std::list<ShareRef> Container;
    typedef std::map<uint256, Container::const_iterator> Index;

    ShareTree(ShareRef share) : _work(0){
        append(share);
    }

    ShareTree(Container::const_iterator shr, Container::const_iterator last) {
        do {
            append(*shr);
        } while(shr++ != last);
    }
    
    Container::const_iterator find(const uint256 hash) {
        return _index.find(hash);
    }
    
    void append(ShareRef share) {
        _container.push_back(share);
        _index[share.hash] = _container.end() - 1;
        _work += share.work();
    }
    
    CBigNum work() const {
        return _work;
    }
    
    // delete all shares pointing the the old block and update the work
    void prune(uint256 old) {
        while (_container.front().prev == old) {
            _index.erase(_container.front().hash);
            _work -= _container.front().work();
            _container.pop_front();
        }
    }
    
    // to check that a new share is valid we need to compare it to the share chain it references
    // we can hence get a set of outputs transactions should adhere
    
    // further we need logic to do manage the target adjustment
    // target adjustment follows a rule that each 144 blocks an ajustment is made so that
    // if we had more than 360 shares we adjust the difficulty up, and down otherwise.
    // However, we never adjust by more than a factor of 4 and minimum diffiulty is that of
    // main chain.
    // will be called from the check share method of BlockChain, hence we have easy access to the blockchain - we need to figure out if work is same as before or should be recalculated - recalculate iff prev->height()%144 == 0 && current->height()%144 != 0
    unsigned int nextWork(uint256 prev, int height) const {
        // locate the hash
        Container::const_iterator prv = find(prev);
        if (prv != _container.end()) {
            CBigInt work = prv->work();
            // get height of share - if %144 +1 react!
            if (height%144 == 0 && prv->height%144 != 0) {
                // work needs adjustment - step backwards a day
                int shares = 0;
                while (--prv != _container.end() && prv->height != height - 145 && ++shares < 1440);
                if (prv != _container.end()) { // only adjust if we have enough blocks to do the adjustment
                    if (shares < 90) shares = 90;
                    work *= 360;
                    work /= shares; // shares == 360 nothing happens
                }
                return work.GetCompact();
            }
            else // previous work - no adjustment
                return prv->bits;
        }
        return 0;
    }
private:
    Container _container;
    Index _index;
    CBigNum _work;
};

class ShareTrees {
public:
    void insert(ShareRef share, bool accept_as_genesis) { // accept as genesis means that it points to block -144
        // first do a quick search to see if we are just appending
        Trees::iterator tr = _trees.find(share.prev_share);
        if (tr != _trees.end()) {
            tr->second->append(share);
            _trees[share.hash] = tr.second.release();
            _trees.erase(share.prev_share);
            if (_best == share.prev_share)
                return; // was best, is best
        }
        else {
            // do a lineary search over all trees and clone if found
            bool cloned = false;
            for (tr = _trees.begin(); tr != _trees.end(); ++tr) {
                Container::const_iterator shr = tr->second->find(share.prev_share);
                if (shr != tr->second->end()) { // clone, insert and exit
                    _trees[share.hash] = new ShareTree(tr->second->begin(), shr);
                    cloned = true;
                    break;
                }
            }
            if (!cloned && accept_as_genesis) {
                _trees[share.hash] = new ShareTree(share);
            }
            else if (!cloned)
                return;
        }
        // set new best!
        CBigInt best = 0;
        for (tr = _trees.begin(); tr != _trees.end(); ++tr) {
            if (tr->second->work() > best) {
                best = tr->second->work();
                _best = tr->second->first;
            }
        }
    }
    
    void prune(uint256 old) {
        CBigInt best = 0;
        for (tr = _trees.begin(); tr != _trees.end(); ++tr) {
            tr->second->prune(old);
            if (tr->second->work() > best) {
                best = tr->second->work();
                _best = tr->second->first;
            }
        }        
    }
    
    unsigned int nextWork(uint256 prev, int height) const {
        // first search for tree tops:
        Trees::const_iterator tr = _trees.find(prev);
        if (tr != _trees.end())
            return tr->nextWork(prev, height);
        
        // do a search in each tree:
        for (tr = _trees.begin(); tr != _trees.end(); ++tr) {
            unsigned int work = tr->nextWork();
            if (work) return work;
        }
        return 0;
    }
    
private:
    // keep a container of all the ShareTrees - could be a map of the top hashes
    // keep a ref to the best chain
    typedef std::map<uint256, auto_ptr<ShareTree> > Trees;;
    Trees _trees;
    uint256 _best;
};

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
            data_dir = default_data_dir(bitcoin.dataDirSuffix());
        
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
            data_dir = default_data_dir(chain.dataDirSuffix());
        
        logfile = data_dir + "/debug.log";
        
        asio::ip::tcp::endpoint proxy_server;
        if(proxy.size()) {
            vector<string> host_port; split(host_port, proxy, is_any_of(":"));
            if(host_port.size() < 2) host_port.push_back("1080"); 
            proxy_server = asio::ip::tcp::endpoint(asio::ip::address_v4::from_string(host_port[0]), lexical_cast<short>(host_port[1]));
        }
        Node node(chain, data_dir, args.count("nolisten") ? "" : "0.0.0.0", lexical_cast<string>(port), proxy_server, timeout); // it is also here we specify the use of a proxy!
        node.setClientVersion("libcoin/bitcoind", vector<string>());
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

        //server.registerMethod(method_ptr(new TxWait(node)));

        /// The Pool enables you to run a backend for a miner, i.e. your private pool, it also enables you to participate in the "Name of Pool"
        // We need a list of blocks for the shared mining
        //
        ChainAddress address("17uY6a7zUidQrJVvpnFchD1MX1untuAufd");
        StaticPayee payee(address.getPubKeyHash());
        Pool pool(node, payee);
        
        server.registerMethod(method_ptr(new GetWork(pool)));
        server.registerMethod(method_ptr(new GetBlockTemplate(pool)));
        server.registerMethod(method_ptr(new SubmitBlock(pool)));

        Miner miner(pool);
        miner.setGenerate(gen);
        thread miningThread(&Miner::run, &miner);
        
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