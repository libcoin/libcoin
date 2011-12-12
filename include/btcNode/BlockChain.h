
#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include "btc/bignum.h"
#include "btc/key.h"
#include "btc/script.h"
#include "btc/tx.h"

#include <boost/noncopyable.hpp>
#include <list>

class DiskTxPos;
class TxIndex;

/// BlockChain encapsulates the BlockChain and provides an interface for adding new blocks, storing in memory transactions and querying for transactions.
/// BlockChain inhierits from CDB and is in fact an extension of the former CTxDB. Further, most of the globals from main.cpp is now in this class.
/*
class CBlockIndex;

typedef std::map<uint256, CBlockIndex*> BlockChainIndex;

class BlockChain : private boost::noncopyable
{
public:
    BlockChain(); /// load the block chain index from file

    bool checkProofOfWork(uint256 hash, unsigned int nBits);
    int getTotalBlocksEstimate();
    bool isInitialBlockDownload();
    void printBlockTree();
    
    // To support mining...
    // CBlock* CreateNewBlock(CReserveKey& reservekey);
    
private:
    BlockChainIndex _blockChainIndex; // mapBlockIndex
    FILE* _blockFile;
    
    const uint256 _genesisBlock; //extern uint256 hashGenesisBlock;
    CBlockIndex* _GenesisBlockIndex; //extern CBlockIndex* pindexGenesisBlock;
    CBigNum _bestChainWork; //extern CBigNum bnBestChainWork;
    CBigNum _bestInvalidWork; //extern CBigNum bnBestInvalidWork;
    uint256 _bestChain; //extern uint256 hashBestChain;
    CBlockIndex* _bestIndex; //extern CBlockIndex* pindexBest;
    int64 _bestReceivedTime; // extern int64 nTimeBestReceived;
    const int _initialBlockThreshold; //const int nInitialBlockThreshold = 120; // Regard blocks up until N-threshold as "initial download"
    CBigNum _proofOfWorkLimit; //extern CBigNum bnProofOfWorkLimit;

};
*/

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
    int getDepthInMainChain() const;
private:
    DiskTxPos _pos;
    std::vector<DiskTxPos> _spents;
};


#endif // BLOCKCHAIN_H
