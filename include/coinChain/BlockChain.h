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

#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <coin/BigNum.h>
#include <coin/Key.h>
#include <coin/Script.h>
#include <coin/Transaction.h>

#include <coinChain/Export.h>
#include <coinChain/Database.h>
#include <coinChain/Spendables.h>
#include <coinChain/Claims.h>
#include <coinChain/Chain.h>
#include <coinChain/BlockTree.h>
#include <coinChain/Verifier.h>

#include <boost/noncopyable.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>
#include <list>

class Transaction;
class NameOperation;

typedef std::vector<Transaction> Transactions;
typedef std::map<uint256, Block> Branches;

class Stats {
public:
    Stats() : _timer(0), _counter(0), _running(false) {}
    
    void start() { _running = true; ++_counter; _timer -= UnixTime::us(); }
    void stop() { _timer += UnixTime::us(); _running = false; }
    
    std::string str() const {
        int64_t timer = _timer + (_running ? UnixTime::us() : 0);
        double avg = 1./_counter*timer;
        std::string s = cformat("%9.3fs / #%6d = %6.3fus : \"%s\"", 0.000001*timer, _counter, avg).text();
        return s;
    }
private:
    int64_t _timer;
    int64_t _counter;
    bool _running;
};

class COINCHAIN_EXPORT BlockChain : private sqliterate::Database {
private: // noncopyable
    BlockChain(const BlockChain&);
    void operator=(const BlockChain&);

public:
    class Reject: public std::runtime_error {
    public:
        Reject(const std::string& s) : std::runtime_error(s.c_str()) {}
    };
    class Error: public std::runtime_error {
    public:
        Error(const std::string& s) : std::runtime_error(s.c_str()) {}
    };
    
    class Listener {
    public:
        virtual void operator()(const Transaction& txn, int conf) = 0;
        virtual void operator()(const Coin& coin, const Output& txn, int count) = 0;
    };
    
    /// The constructor - reference to a Chain definition is obligatory, if no dataDir is provided, the location for the db and the file is chosen from the Chain definition and the CDB::defaultDir method
    BlockChain(const Chain& chain = bitcoin, const std::string dataDir = "");

    /// The purge depth is the number of blocks kept as spendings and unspents. If purgedepth is set to 0 the client will be a full client
    /// Else it is the number of old blocks served to the bitcoin network.
    unsigned int purge_depth() const;
    void purge_depth(unsigned int purge_depth);
    
    bool lazy_purging() const { return _lazy_purging; }
    void lazy_purging(bool p) { _lazy_purging = p; }
    
    /// query and set index for fast script to unspent lookup - enabling can take some time
    bool script_to_unspents() const;
    void script_to_unspents(bool enable);
    
    /// Validation Depth: 0 - don't verify - use the database for verification
    /// if > 0 use Trie for validation as long as block count is below, use MerkleTrie when blockcount is equal or above
    /// e.g. a value of 1 will keep the merkle trie switched on all the time and a value of UINT_MAX will ensure Trie all the time
    /// Default is _chain.totalBlocksEstimate()
    unsigned int validation_depth() const { return _validation_depth; }
    void validation_depth(unsigned int v);
    
    /// Verification Depth: 0 - don't verify x (>0) verify anything later than this
    /// default is x = _chain.totalBlocksEstimate()
    /// Note: verification depth changes does not affect already accpeted blocks!
    unsigned int verification_depth() const { return _verification_depth; }
    void verification_depth(unsigned int v) { _verification_depth = v; }
    
    /// T R A N S A C T I O N S    
    
    void subscribe(Listener& listener) {
        _listeners.push_back(boost::ref(listener));
    }
    
    /// Get transactions from db or memory.
    void getTransaction(const int64_t cnf, Transaction &txn) const;
    void getTransaction(const uint256& hash, Transaction& tx) const;
    void getTransaction(const int64_t cnf, Transaction& tx, int64_t& height, int64_t& time) const;
    void getTransaction(const uint256& hash, Transaction& tx, int64_t& height, int64_t& time) const;
    
    /// Query for existence of a Transaction.
    bool haveTx(uint256 hash, bool must_be_confirmed = false) const;
    
    /// A Transaction is final if the critreias set by it locktime are met.
    bool isFinal(const Transaction& tx, int nBlockHeight=0, int64_t nBlockTime=0) const;
    
    /// Dry run version of claim.
    bool checkTransaction(const Transaction& txn) const {
        boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);
        try {
            try_claim(txn, true);
            return true;
        }
        catch (...) {
            return false;
        }
    }
    
    std::pair<Claims::Spents, int64_t> try_claim(const Transaction& txn, bool verify) const;

    void claim(const Transaction& txn, bool verify = true);
    
    size_t claim_count() const {
        return _claims.count();
    }
    
    std::vector<uint256> claims(BloomFilter& filter) const {
        return _claims.claims(filter);
    }
    
    int setMerkleBranch(MerkleTx& mtxn) const;
    
    /// S C R I P T S
    
    /// Get the balance of a script up to block <height> - will throw if <height> < pruned height
    int64_t balance(const Script& script, int height = -1) const;
    
    /// C O I N S
    
    bool isSpent(Coin coin, int confirmations = 1) const;
    
    /// getUnspents return unspent coins and values before a certain timestamp.
    /// If the timestamp is less than 500.000.000 it is assumed that it refers to a certain block,
    /// else it referes to a certain (posix)time, and all confirmed unspents will be included plus those claimed before the timestamp.
    /// If the timestamp is 0 (default) everything is included
    void getUnspents(const Script& script, Unspents& unspents, unsigned int before = 0) const;
    
    int getHeight(Coin coin) const;
    
    //    int64_t value(Coin coin) const;
    
    /// B L O C K S
        
    /// Query for existence of Block.
    bool haveBlock(uint256 hash) const;
    
    /// Accept a block (Note: this could lead to a reorganization of the block that is often quite time consuming).
    void append(const Block &block);

    int getDistanceBack(const BlockLocator& locator) const;

    BlockIterator iterator(const BlockLocator& locator) const;
    BlockIterator iterator(const uint256 hash) const;
    BlockIterator iterator(const size_t height) const { return _tree.begin() + height; }

    double getDifficulty(BlockIterator blk = BlockIterator()) const;
    
    /// getBlock will first try to locate the block by its hash through the block index, if this fails it will assume that the hash for a tx and check the database to get the disk pos and then return the block as read from the block file
    void getBlock(const uint256 hash, Block& block) const;
    
    void getBlock(BlockIterator blk, Block& block) const;

    /// getBlockHeader will only make one query to the DB and return an empty block without the transactions
    void getBlockHeader(int count, Block& block) const;
    void getBlockHeader(BlockIterator blk, Block& block) const { getBlockHeader(blk.count(), block); }

    void getBlock(int count, Block& block) const;
    
    /// S H A R E S
    bool checkShare(const Block& block) const;
    
    ShareTree::Dividend getDividend() const {
        return _share_tree.dividend();
    }
    
    /// Get height of block of transaction by its hash
    int getHeight(const uint256 hash) const;
    
    /// Get the maturity / depth of a block or a tx by its hash
    int getDepthInMainChain(const uint256 hash) const { int height = getHeight(hash); return height < 0 ? 0 : getBestHeight() - height + 1; }
    
    /// Get number of blocks to maturity - only relevant for Coin base transactions.
    int getBlocksToMaturity(const Transaction& tx) const {
        if (!tx.isCoinBase())
            return 0;
        return std::max(0, (_chain.maturity(getBestHeight())+20) - getDepthInMainChain(tx.getHash()));
    }
    
    /// Check if the hash of a block belongs to a block in the main chain:
    bool isInMainChain(const uint256 hash) const;
    
    /// Get the best height
    int getBestHeight() const {
        return _tree.height();
    }
    
    /// Get the deepest block height, that have not been purged
    int getDeepestDepth() const;
    
    BlockIterator getBest() const {
        return _tree.best();
    }
    
    BlockIterator getBestInvalid() const {
        return _tree.best_invalid();
    }
    
    /// NameCoin: getNameValue returns the most recent value for a name
    int getNameAge(const std::string& name) const;
    
    /// NameCoin: getCoinName returns the name, if any for an unspent output
    std::string getCoinName(int64_t coin) const;
    
    /// Get the locator for the best index
    const BlockLocator& getBestLocator() const;
    
    const uint256& getGenesisHash() const { return _chain.genesisHash(); }
    const uint256& getBestChain() const { return _tree.best()->hash; }
    const int64_t& getBestReceivedTime() const { return _bestReceivedTime; }

    typedef std::vector<Script> Payees;
    typedef std::vector<unsigned int> Fractions;
    Block getBlockTemplate(BlockIterator blk, Payees scripts, Fractions fractions = Fractions(), Fractions fee_fractions = Fractions()) const;
    
    const Chain& chain() const { return _chain; }
    
    void outputPerformanceTimings() const;

    int getTotalBlocksEstimate() const { return _chain.totalBlocksEstimate(); }    
    
    enum { _medianTimeSpan = 11 };
    unsigned int getMedianTimePast(BlockIterator blk) const {
        std::vector<unsigned int> samples;
        
        for (size_t i = 0; i < _medianTimeSpan && !!blk; ++i, --blk)
            samples.push_back(blk->time);
        
        std::sort(samples.begin(), samples.end());
        return samples[samples.size()/2];
    }    
    
protected:
    int getMinAcceptedBlockVersion() const;
    int getMinEnforcedBlockVersion() const;
    
    void rollbackConfirmation(int64_t cnf);
    void rollbackBlock(int count);
    
    void updateBestLocator();

    uint256 getBlockHash(const BlockLocator& locator) const;

    bool disconnectInputs(const Transaction& tx);    
    
    void deleteTransaction(const int64_t tx, Transaction &txn);
    
private:
    typedef std::map<uint256, Transaction> Txns;
    typedef std::set<uint256> Hashes;
    
    void attach(BlockIterator &blk, Txns& unconfirmed, Hashes& confirmed);
    void detach(BlockIterator &blk, Txns& unconfirmed);

    void postTransaction(const Transaction txn, int64_t& fees, int64_t min_fee, BlockIterator blk, int64_t idx, bool verify);
    void postSubsidy(const Transaction txn, BlockIterator blk, int64_t fees);
    
    void insertBlockHeader(int64_t count, const Block& block);

    /// Mark a spendable spent - throw if already spent (or immature in case of database mode)
    Unspent redeem(const Input& input, int iidx, Confirmation conf);
    
    /// Issue a new spendable
    int64_t issue(const Output& output, uint256 hash, unsigned int out_idx, Confirmation conf, bool unique = true);
    
    /// Maturate the coinbase from block with count # - throw if not unique ?
    void maturate(int64_t count);

    /// update the name in the name database - specifically for NameCoin
    void updateName(const NameOperation& name_op, int64_t coin, int count);

private:
    const Chain& _chain;
    
    Verifier _verifier;

    unsigned int _validation_depth;

    bool _lazy_purging; 
    
    unsigned int _purge_depth;

    unsigned int _verification_depth;
    
    BlockLocator _bestLocator;
    
    BlockTree _tree;
    BlockTree _invalid_tree;
    
    ShareTree _share_tree;
    
    Branches _branches;

    // Collections to speed up chekking for dublicate hashes (BIP0030)
    // And if coins are spendable
    // Further, Spendables can be a Merkle Trie enabling querying for non-spending proofs
    
    Spendables _spendables;
    Spendables _immature_coinbases;
    
    Spendables _share_spendables;
    
    Claims _claims;

    typedef boost::reference_wrapper<Listener> ListenerRef;
    typedef std::vector<ListenerRef> Listeners;
    
    Listeners _listeners;
    
    int64_t _bestReceivedTime;

    mutable boost::shared_mutex _chain_and_pool_access;

    mutable Stats _redeemStats;
    mutable Stats _issueStats;
    
    mutable int64_t _acceptBlockTimer;
    mutable int64_t _connectInputsTimer;
    mutable int64_t _verifySignatureTimer;
    mutable int64_t _setBestChainTimer;
    mutable int64_t _addToBlockIndexTimer;
};

#endif // BLOCKCHAIN_H
