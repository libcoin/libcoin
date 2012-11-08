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
#include <coinChain/db.h>

#include <coinChain/Chain.h>
#include <coinChain/BlockTree.h>

#include <boost/noncopyable.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>
#include <list>

class Transaction;

typedef std::vector<Transaction> Transactions;
//typedef std::vector<Block> Blocks;
typedef std::map<uint256, Block> Branches;

class Branch;

class COINCHAIN_EXPORT BlockChain : private Database
{
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
    
    /// The constructor - reference to a Chain definition i obligatory, if no dataDir is provided, the location for the db and the file is chosen from the Chain definition and the CDB::defaultDir method
    BlockChain(const Chain& chain = bitcoin, const std::string dataDir = "");

    /// T R A N S A C T I O N S    
    
    /// Get transactions from db or memory.
    void getTransaction(const int64 tx, Transaction &txn) const;
    void getTransaction(const uint256& hash, Transaction& tx) const;
    void getTransaction(const uint256& hash, Transaction& tx, int64& height, int64& time) const;
    
    /// Get all unconfirmed transactions.
    Transactions unconfirmedTransactions() const {
        // lock the pool and chain for reading
        boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);
        
        Transactions txns;
        std::vector<int64> txs = _findBlockTransactions(0);

        for (std::vector<int64>::const_iterator tx = txs.begin(); tx != txs.end(); ++tx) {
            Transaction txn;
            getTransaction(*tx, txn);
            txns.push_back(txn);
        }
        
        return txns;
    }
    
    /// Query for existence of a Transaction.
    bool haveTx(uint256 hash, bool must_be_confirmed = false) const;
    
    /// A Transaction is final if the critreias set by it locktime are met.
    bool isFinal(const Transaction& tx, int nBlockHeight=0, int64 nBlockTime=0) const;
    
    /// Dry run version of the acceptTransaction method. Hence it is const. For probing transaction before they are scheduled for submission to the chain network.
    bool checkTransaction(const Transaction& tx) const {
        boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);
        //        return CheckForMemoryPool(tx);
    }
    
    /// Accept transaction (it will go into memory first)
    //    void acceptTransaction(const Transaction& txn, int64& fees, int64 min_fee);
    void acceptTransaction(const Transaction& txn, bool verify = true);
    void acceptBlockTransaction(const Transaction txn, int64& fees, int64 min_fee, BlockIterator blk = BlockIterator(), int64 idx = 0, bool verify = true);
    void reacceptTransaction(int64 tx);
    
    /// C O I N S
    
    bool isSpent(Coin coin) const;
    /// This rather strange name refers to this coin included in a transaction in the memorypool
    bool beingSpent(Coin coin) const { return _beingSpent(coin.hash, (int64)coin.index); }

    int64 value(Coin coin) const;
    
    /// B L O C K S
        
    /// Query for existence of Block.
    bool haveBlock(uint256 hash) const;
    
    /// Accept a block (Note: this could lead to a reorganization of the block that is often quite time consuming).
    void acceptBlock(const Block &block);

    int getDistanceBack(const BlockLocator& locator) const;

    BlockIterator iterator(const BlockLocator& locator) const;
    BlockIterator iterator(const uint256 hash) const;
    BlockIterator iterator(const size_t height) const { return _tree.begin() + height; }

    double getDifficulty(BlockIterator blk = BlockIterator()) const;
    
    /// getBlock will first try to locate the block by its hash through the block index, if this fails it will assume that the hash for a tx and check the database to get the disk pos and then return the block as read from the block file
    void getBlock(const uint256 hash, Block& block) const;
    
    void getBlock(BlockIterator blk, Block& block) const;

    /// getBlockHeader will only make one query to the DB and return an empty block without the transactions
    void getBlockHeader(BlockIterator blk, Block& block) const { block = _getBlock(blk->hash); }
    
    /// Get height of block of transaction by its hash
    int getHeight(const uint256 hash) const;
    
    /// Get the maturity / depth of a block or a tx by its hash
    int getDepthInMainChain(const uint256 hash) const { int height = getHeight(hash); return height < 0 ? 0 : getBestHeight() - height + 1; }
    
    /// Get number of blocks to maturity - only relevant for Coin base transactions.
    int getBlocksToMaturity(const Transaction& tx) const {
        if (!tx.isCoinBase())
            return 0;
        return std::max(0, (COINBASE_MATURITY+20) - getDepthInMainChain(tx.getHash()));
    }
    
    /// Check if the hash of a block belongs to a block in the main chain:
    bool isInMainChain(const uint256 hash) const;
    
    /// Get the best height
    int getBestHeight() const {
        return _tree.height();
    }
    
    /// Get the locator for the best index
    const BlockLocator& getBestLocator() const;
    
    const uint256& getGenesisHash() const { return _chain.genesisHash(); }
    const uint256& getBestChain() const { return _tree.best()->hash; }
    const int64& getBestReceivedTime() const { return _bestReceivedTime; }

    const Chain& chain() const { return _chain; }
    
    bool connectInputs(const Transaction& txn, const int64 blk, int idx, int64& fees, int64 min_fee = 0) const;

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
    void updateBestLocator();

    uint256 getBlockHash(const BlockLocator& locator) const;

    bool disconnectInputs(const Transaction& tx);    
    
    void deleteTransaction(const int64 tx, Transaction &txn);
    
    /// Block stuff
    
    void deleteBlock(BlockIterator blk, Block& block);
    
    void InvalidChainFound(int64 blk_new);
    
    bool reorganize(const Block& block, int64 blk_new);
    
    /// This is to accept
    bool AcceptToMemoryPool(const Transaction& tx) {
        bool mi;
        return AcceptToMemoryPool(tx, true, &mi);
    }

    /// This is to accept transactions from blocks (no checks)
    bool AcceptToMemoryPool(const Transaction& tx, bool fCheckInputs);
    bool AcceptToMemoryPool(const Transaction& tx, bool fCheckInputs, bool* pfMissingInputs);

    bool AddToMemoryPoolUnchecked(const Transaction& tx);
    bool RemoveFromMemoryPool(const Transaction& tx);

private:
    const Chain& _chain;
    /// database actions
    Statement<int64> _findBlock;
    StatementLastId _insertBlock;
    StatementLastId _insertGenesisBlock;
    StatementVoid _deleteBlock;
    
    StatementVec<int64, 1, int64> _findBlockTransactions;

    StatementVoid _deleteConfirmation;
    StatementLastId _insertConfirmation;
    Statement<int64> _confirmationIdx;
    
    Statement<int64> _findTransaction;
    StatementLastId _insertTransaction;
    StatementClass<Transaction, 2, int, unsigned int> _getTransaction;
    StatementVoid _deleteTransaction;
    
    StatementVec<int64, 1, int64> _getConfirmationBlocks;
    StatementVec<int64, 1, int64> _getInputConfirmationBlocks;
    
    Statement<int64> _nonfirmationTime;
    
    StatementClass<Block, 6, int, uint256, uint256, int, int, int> _getBlock;

    Statement<int64> _findOutput;
    StatementClass<Output, 2, int64, Script> _getOutput;
    StatementVec<Output, 2, int64, Script> _getOutputs;
    StatementLastId _insertOutput;
    StatementVoid _deleteOutputs;

    StatementLastId _insertInput;
    StatementVec<Input, 4, uint256, unsigned int, Script, unsigned int> _getInputs;
    StatementVoid _deleteInputs;

    Statement<int64> _findScript;
    StatementLastId _insertScript;

    StatementVec<int64, 1, int64> _spentInBlocks; // input is coin output is vector of blk

    StatementVec<int64, 1, int64> _prunedTransactions;
    
    StatementVoid _pruneOutputs;
    StatementVoid _pruneInputs;
    
    Statement<int64> _countOutputs;
    
    StatementVoid _pruneTransaction;
    StatementVoid _pruneConfirmation;
    
    Statement<int64> _beingSpent;
    
    BlockLocator _bestLocator;
    
    BlockTree _tree;
    
    Branches _branches;
    
    int64 _bestReceivedTime;

    mutable boost::shared_mutex _chain_and_pool_access;

    mutable int64 _acceptBlockTimer;
    mutable int64 _connectInputsTimer;
    mutable int64 _verifySignatureTimer;
    mutable int64 _setBestChainTimer;
    mutable int64 _addToBlockIndexTimer;
};

#endif // BLOCKCHAIN_H
