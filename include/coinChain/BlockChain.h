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

#include <coinChain/db.h>

#include <coinChain/BlockIndex.h>
#include <coinChain/BlockFile.h>
#include <coinChain/Chain.h>

#include <boost/noncopyable.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>
#include <list>

class Transaction;

class DiskTxPos
{
public:
    
    DiskTxPos()
    {
    setNull();
    }
    
    DiskTxPos(unsigned int file, unsigned int blockPos, unsigned int txPos) : _file(file), _blockPos(blockPos), _txPos(txPos) {}
    
    IMPLEMENT_SERIALIZE( READWRITE(FLATDATA(*this)); )
    
    void setNull() { _file = -1; _blockPos = 0; _txPos = 0; }
    bool isNull() const { return (_file == -1); }
    
    const unsigned int getFile() const { return _file; }
    const unsigned int getBlockPos() const { return _blockPos; }
    const unsigned int getTxPos() const { return _txPos; }
    
    friend bool operator==(const DiskTxPos& a, const DiskTxPos& b)
    {
    return (a._file     == b._file &&
            a._blockPos == b._blockPos &&
            a._txPos    == b._txPos);
    }
    
    friend bool operator!=(const DiskTxPos& a, const DiskTxPos& b)
    {
    return !(a == b);
    }
    
    std::string toString() const
    {
    if (isNull())
        return strprintf("null");
    else
        return strprintf("(nFile=%d, nBlockPos=%d, nTxPos=%d)", _file, _blockPos, _txPos);
    }
    
    void print() const
    {
    printf("%s", toString().c_str());
    }
private:
    unsigned int _file;
    unsigned int _blockPos;
    unsigned int _txPos;
};

/// A txdb record that contains the disk location of a transaction and the
/// locations of transactions that spend its outputs.  vSpent is really only
/// used as a flag, but having the location is very helpful for debugging.

class TxIndex
{
public:
    
    TxIndex()
    {
    setNull();
    }
    
    TxIndex(const DiskTxPos& pos, unsigned int outputs) : _pos(pos)
    {
    _spents.resize(outputs);
    }
    
    IMPLEMENT_SERIALIZE
    (
     if (!(nType & SER_GETHASH))
     READWRITE(nVersion);
     READWRITE(_pos);
     READWRITE(_spents);
     )
    
    void setNull()
    {
    _pos.setNull();
    _spents.clear();
    }
    
    bool isNull()
    {
    return _pos.isNull();
    }
    
    const DiskTxPos& getPos() const { return _pos; }
    
    const size_t getNumSpents() const { return _spents.size(); }
    const DiskTxPos& getSpent(unsigned int n) { return _spents[n]; }
    void resizeSpents(size_t size) { _spents.resize(size); }
    void setSpent(const unsigned int n, const DiskTxPos& pos) { _spents[n] = pos; };
    void setNotSpent(const unsigned int n) { _spents[n].setNull(); }
    
    friend bool operator==(const TxIndex& a, const TxIndex& b)
    {
    return (a._pos    == b._pos &&
            a._spents == b._spents);
    }
    
    friend bool operator!=(const TxIndex& a, const TxIndex& b)
    {
    return !(a == b);
    }
private:
    DiskTxPos _pos;
    std::vector<DiskTxPos> _spents;
};

/// BlockChain encapsulates the BlockChain and provides a const interface for querying properties for blocks and transactions. 
/// BlockChain also provides an interface for adding new blocks and transactions. (non-const)
/// BlockChain automatically handles adding new transactions to a memorypool and erasing them again when a block containing the transaction is added.

/// BlockChain inhierits from CDB, has a Chain definition reference and has a BlockFile. In fact BlockChain is the only class to access the block index db and the block file. BlockChain is hence thread safe for all const querying and for adding blocks and transactions, the user is responsible for not calling these methods at the same time from multiple threads.

typedef std::map<uint256, CBlockIndex*> BlockChainIndex;
typedef std::map<uint256, Transaction> TransactionIndex;
typedef std::map<uint160, Coins> AssetIndex;
typedef std::vector<Transaction> Transactions;
typedef std::vector<Block> Blocks;

class BlockChain : private CDB
{
private: // noncopyable
    BlockChain(const BlockChain&);
    void operator=(const BlockChain&);

public:
    /// The constructor - reference to a Chain definition i obligatory, if no dataDir is provided, the location for the db and the file is chosen from the Chain definition and the CDB::defaultDir method 
    BlockChain(const Chain& chain = bitcoin, const std::string dataDir = "", const char* pszMode="cr+");
    
    /// T R A N S A C T I O N S    
    
    /// Get transactions from db or memory.
    void getTransaction(const uint256& hash, Transaction& tx) const;
    void getTransaction(const uint256& hash, Transaction& tx, int64& height, int64& time) const;
    
    /// Get all unconfirmed transactions.
    Transactions unconfirmedTransactions() const {
        // lock the pool and chain for reading
        boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);

        Transactions txes;
        for(TransactionIndex::const_iterator i = _transactionIndex.begin(); i != _transactionIndex.end(); ++i)
            txes.push_back(i->second);
        return txes;
    }
    
    /// Query for existence of a Transaction.
    bool haveTx(uint256 hash, bool must_be_confirmed = false) const;
    
    /// A Transaction is final if the critreias set by it locktime are met.
    bool isFinal(const Transaction& tx, int nBlockHeight=0, int64 nBlockTime=0) const;
    
    /// Dry run version of the acceptTransaction method. Hence it is const. For probing transaction before they are scheduled for submission to the chain network.
    bool checkTransaction(const Transaction& tx) const {
        boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);
        return CheckForMemoryPool(tx);
    }
    
    /// Accept transaction (it will go into memory first)
    bool acceptTransaction(const Transaction& tx) { 
        return AcceptToMemoryPool(tx);
    }
    
    bool acceptTransaction(const Transaction& tx, bool& missing_inputs) { 
        bool mi = false;
        bool ret = AcceptToMemoryPool(tx, true, &mi);
        missing_inputs = mi;
        return ret;
    }

    /// C O I N S
    
    bool isSpent(Coin coin) const;
    int getNumSpent(uint256 hash) const ;
    uint256 spentIn(Coin coin) const;

    int64 value(Coin coin) const;
    
    /// B L O C K S
    
    /// Load the BlockIndex into memory and prepare the BlockFile.
    bool load(bool allowNew = true);
    
    /// Print the Block tree.
    void print();
    
    /// Query for existence of Block.
    bool haveBlock(uint256 hash) const;
    
    /// Accept a block (Note: this could lead to a reorganization of the block that is often quite time consuming).
    bool acceptBlock(const Block& block);
    
    /// Locate blocks in the block chain
    //CBlockLocator blockLocator(uint256 hashBlock);
    
    int getDistanceBack(const CBlockLocator& locator) const;

    const CBlockIndex* getBlockIndex(const CBlockLocator& locator) const;

    const CBlockIndex* getBlockIndex(const uint256 hash) const;

    double getDifficulty(const CBlockIndex* pindex = NULL) const; 
    
    /// getBlock will first try to locate the block by its hash through the block index, if this fails it will assume that the hash for a tx and check the database to get the disk pos and then return the block as read from the block file
    void getBlock(const uint256 hash, Block& block) const;
    
    void getBlock(const CBlockIndex* index, Block& block) const;

    CBlockIndex* getHashStopIndex(uint256 hashStop) const;

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
    int getBestHeight() const
    { 
        // lock the pool and chain for reading
        boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);
        return _bestIndex->nHeight;
    }
    
    /// Get the locator for the best index
    CBlockLocator getBestLocator() const;
    
    bool isInitialBlockDownload() const;
    
    const uint256& getGenesisHash() const { return _chain.genesisHash(); }
    const CBlockIndex* getBestIndex() const { return _bestIndex; }
    const uint256& getBestChain() const { return _bestChain; }
    const int64& getBestReceivedTime() const { return _bestReceivedTime; }

    size_t getBlockChainIndexSize() const { return _blockChainIndex.size(); }
    
    const Chain& chain() const { return _chain; }
    
    bool connectInputs(const Transaction& tx, std::map<uint256, TxIndex>& mapTestPool, DiskTxPos posThisTx, const CBlockIndex* pindexBlock, int64& nFees, bool fBlock, bool fMiner, int64 nMinFee = 0) const;
    
    void outputPerformanceTimings() const;

    int getTotalBlocksEstimate() const { return _chain.totalBlocksEstimate(); }    
    
protected:        

    /// Read a Transaction from Disk.
    bool readDrIndex(uint160 hash160, std::set<Coin>& debit) const;
    bool readCrIndex(uint160 hash160, std::set<Coin>& credit) const;
    bool readDiskTx(uint256 hash, Transaction& tx) const;
    bool readDiskTx(uint256 hash, Transaction& tx, int64& height, int64& time) const;
    
    uint256 getBlockHash(const CBlockLocator& locator);
    
    bool isInMainChain(CBlockIndex* bi) const {
        return (bi->pnext || bi == _bestIndex);
    }    
    
    bool disconnectInputs(const Transaction& tx);    
    
    /// Block stuff
    bool disconnectBlock(const Block& block, CBlockIndex* pindex);
    bool connectBlock(const Block& block, CBlockIndex* pindex);
    bool setBestChain(const Block& block, CBlockIndex* pindexNew);
    bool addToBlockIndex(const Block& block, unsigned int nFile, unsigned int nBlockPos);

    bool ReadTxIndex(uint256 hash, TxIndex& txindex) const;
    bool UpdateTxIndex(uint256 hash, const TxIndex& txindex);
    bool AddTxIndex(const Transaction& tx, const DiskTxPos& pos, int nHeight);
    bool EraseTxIndex(const Transaction& tx);

    bool ReadOwnerTxes(uint160 hash160, int nHeight, std::vector<Transaction>& vtx);
    bool ReadDiskTx(uint256 hash, Transaction& tx, TxIndex& txindex) const;
    bool ReadDiskTx(Coin outpoint, Transaction& tx, TxIndex& txindex) const;
    bool ReadDiskTx(Coin outpoint, Transaction& tx) const;
    bool WriteBlockIndex(const CDiskBlockIndex& blockindex);
    bool EraseBlockIndex(uint256 hash);
    bool ReadHashBestChain();
    bool WriteHashBestChain();
    bool ReadBestInvalidWork();
    bool WriteBestInvalidWork();
    bool LoadBlockIndex();
    
    CBlockIndex* InsertBlockIndex(uint256 hash);
    
    void InvalidChainFound(CBlockIndex* pindexNew);
    
    bool reorganize(const Block& block, CBlockIndex* pindexNew);
    
    bool CheckForMemoryPool(const Transaction& tx) const { Transaction* ptxOld = NULL; return CheckForMemoryPool(tx, ptxOld); }
    bool CheckForMemoryPool(const Transaction& tx, Transaction*& ptxOld, bool fCheckInputs=true, bool* pfMissingInputs=NULL) const;

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
    BlockFile _blockFile; // this is ONLY interface to the block file!

    CBlockIndex* _genesisBlockIndex;
    
    BlockChainIndex _blockChainIndex;
    
    CBigNum _bestChainWork;
    CBigNum _bestInvalidWork;
    
    uint256 _bestChain;
    CBlockIndex* _bestIndex;
    int64 _bestReceivedTime;

    TransactionIndex _transactionIndex;    
    
    AssetIndex _creditIndex;
    AssetIndex _debitIndex;
    typedef std::map<Coin, CoinRef> TransactionConnections;
    TransactionConnections _transactionConnections;
    unsigned int _transactionsUpdated;
    
    mutable boost::shared_mutex _chain_and_pool_access;

    mutable int64 _acceptBlockTimer;
    mutable int64 _connectInputsTimer;
    mutable int64 _verifySignatureTimer;
    mutable int64 _setBestChainTimer;
    mutable int64 _addToBlockIndexTimer;
};

#endif // BLOCKCHAIN_H
