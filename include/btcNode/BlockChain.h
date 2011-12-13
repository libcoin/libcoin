
#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include "btc/bignum.h"
#include "btc/key.h"
#include "btc/script.h"
#include "btc/tx.h"

#include "btcNode/db.h"

#include "btcNode/BlockIndex.h"
#include "btcNode/BlockFile.h"

#include <boost/noncopyable.hpp>
#include <list>

class CTransaction;

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

/// BlockChain encapsulates the BlockChain and provides an interface for adding new blocks, storing in memory transactions and querying for transactions.
/// BlockChain inhierits from CDB and is in fact an extension of the former CTxDB. Further, most of the globals from main.cpp is now in this class.

typedef std::map<uint256, CBlockIndex*> BlockChainIndex;

class BlockChain : public CDB
{
private: // noncopyable
    BlockChain(const BlockChain&);
    void operator=(const BlockChain&);

public:
    BlockChain(const char* pszMode="cr+") : CDB("blkindex.dat", pszMode), _genesisBlock("0x000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f"), _bestChainWork(0), _bestInvalidWork(0), _bestChain(0), _bestIndex(NULL), _bestReceivedTime(0) { }
    
    /// Load the BlockIndex into memory and prepare the BlockFile.
    bool load(bool allowNew = true);

    /// Print the Block tree.
    void print();
    
    /// Read a Transaction from Disk.
    
    bool readDrIndex(uint160 hash160, std::set<std::pair<uint256, unsigned int> >& debit);
    bool readCrIndex(uint160 hash160, std::set<std::pair<uint256, unsigned int> >& credit);
    bool readDiskTx(uint256 hash, CTransaction& tx);
    bool readDiskTx(uint256 hash, CTransaction& tx, int64& height, int64& time);

    /// Query for existence of Transaction.
    bool containsTx(uint256 hash);
    
    /// Query tx.
    bool isFinal(CTransaction& tx, int nBlockHeight=0, int64 nBlockTime=0) const;
    
    /// Query for existence of Block.
    bool containsBlock(uint256 hash);
    
    /// Locate blocks in the block chain
    CBlockLocator blockLocator(uint256 hashBlock);
    
    int getDistanceBack(const CBlockLocator& locator);

    CBlockIndex* getBlockIndex(const CBlockLocator& locator);

    CBlockIndex* getBlockIndex(const uint256 hash);

    void readBlock(const uint256 hash, Block& block);

    CBlockIndex* getHashStopIndex(uint256 hashStop);

    uint256 getBlockHash(const CBlockLocator& locator);
    
    /// Get height of block of transaction by its hash
    int getHeight(const uint256 hash);
    
    /// Get the maturity / depth of a block or a tx by its hash
    int getDepthInMainChain(const uint256 hash) { int height = getHeight(hash); return height < 0 ? 0 : getBestHeight() - height + 1; }
    
    /// Get the best height
    int getBestHeight() const { return _bestIndex->nHeight; }
    
    bool isInMainChain(CBlockIndex* bi) const
    {
        return (bi->pnext || bi == _bestIndex);
    }    

    bool isInitialBlockDownload();

    bool connectInputs(CTransaction& tx, std::map<uint256, TxIndex>& mapTestPool, DiskTxPos posThisTx, const CBlockIndex* pindexBlock, int64& nFees, bool fBlock, bool fMiner, int64 nMinFee = 0);
    
    bool disconnectInputs(CTransaction& tx);    

    /// Block stuff
    bool disconnectBlock(Block& block, CBlockIndex* pindex);
    bool connectBlock(Block& block, CBlockIndex* pindex);
    bool setBestChain(Block& block, CBlockIndex* pindexNew);
    bool addToBlockIndex(Block& block, unsigned int nFile, unsigned int nBlockPos);
    bool acceptBlock(Block& block);
    
    const uint256& getGenesisHash() const { return _genesisBlock; }
    const CBlockIndex* getBestIndex() const { return _bestIndex; }
    const uint256& getBestChain() const { return _bestChain; }
    const int64& getBestReceivedTime() const { return _bestReceivedTime; }

    const size_t getBlockChainIndexSize() const { return _blockChainIndex.size(); }
    
protected:
    bool ReadTxIndex(uint256 hash, TxIndex& txindex);
    bool UpdateTxIndex(uint256 hash, const TxIndex& txindex);
    bool AddTxIndex(const CTransaction& tx, const DiskTxPos& pos, int nHeight);
    bool EraseTxIndex(const CTransaction& tx);

    bool ReadOwnerTxes(uint160 hash160, int nHeight, std::vector<CTransaction>& vtx);
    bool ReadDiskTx(uint256 hash, CTransaction& tx, TxIndex& txindex);
    bool ReadDiskTx(COutPoint outpoint, CTransaction& tx, TxIndex& txindex);
    bool ReadDiskTx(COutPoint outpoint, CTransaction& tx);
    bool WriteBlockIndex(const CDiskBlockIndex& blockindex);
    bool EraseBlockIndex(uint256 hash);
    bool ReadHashBestChain();
    bool WriteHashBestChain();
    bool ReadBestInvalidWork();
    bool WriteBestInvalidWork();
    bool LoadBlockIndex();
    
    CBlockIndex* InsertBlockIndex(uint256 hash);
    
    void InvalidChainFound(CBlockIndex* pindexNew);
    
    bool reorganize(Block& block, CBlockIndex* pindexNew);
    
    int getTotalBlocksEstimate() const { if(fTestNet) return 0; else return _totalBlocksEstimate; }
    
private:
    uint256 _genesisBlock;
    
    BlockFile _blockFile; // this is ONLY interface to the block file!
    BlockChainIndex _blockChainIndex;
    
    CBigNum _bestChainWork;
    CBigNum _bestInvalidWork;
    uint256 _bestChain;
    CBlockIndex* _bestIndex;
    int64 _bestReceivedTime;
    
    static const int _totalBlocksEstimate = 150000;
};


class CDBAssetSyncronizer : public CAssetSyncronizer
{
public:
    CDBAssetSyncronizer(BlockChain& bc) : _blockChain(bc) {}
    virtual void getCreditCoins(uint160 btc, Coins& coins);
    virtual void getDebitCoins(uint160 btc, Coins& coins);
    virtual void getCoins(uint160 btc, Coins& coins);
    
    virtual void getTransaction(const Coin& coin, CTx&);
private:
    BlockChain& _blockChain;
};

#endif // BLOCKCHAIN_H
