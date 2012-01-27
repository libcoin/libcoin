
#ifndef BLOCKFILE_H
#define BLOCKFILE_H

#include "coin/BigNum.h"
#include "coin/Key.h"
#include "coin/Script.h"
#include "coin/Transaction.h"

#include <boost/noncopyable.hpp>
#include <list>

/// BlockFile encapsulates the Block file on the disk. It supports different queries to the block file.

class Chain;
class Block;
class CBlockIndex;
class Transaction;
class DiskTxPos;

class BlockFile : private boost::noncopyable
{
public:
    BlockFile(const std::string dataDir) : _dataDir(dataDir), _currentBlockFile(1) {} /// load the block chain index from file

    bool writeToDisk(const Chain& chain, const Block& block, unsigned int& nFileRet, unsigned int& nBlockPosRet, bool commit = false);
    bool readFromDisk(Transaction& tx, DiskTxPos pos, FILE** pfileRet);
    
    bool readFromDisk(Transaction& tx, DiskTxPos pos) const;    
    bool readFromDisk(Block& block, const CBlockIndex* pindex, bool fReadTransactions=true) const;
    bool readFromDisk(Block& block, unsigned int nFile, unsigned int nBlockPos, bool fReadTransactions=true) const;
    
    bool eraseBlockFromDisk(CBlockIndex bindex);

    bool checkDiskSpace(uint64 nAdditionalBytes=0);

protected:
    FILE* openBlockFile(unsigned int nFile, unsigned int nBlockPos) const;
    FILE* openBlockFile(unsigned int nFile, unsigned int nBlockPos, const char* pszMode);
    FILE* appendBlockFile(unsigned int& nFileRet);
    //    bool loadBlockIndex(bool fAllowNew=true);
    
private:
    std::string _dataDir;
    FILE* _blockFile;    
    unsigned int _currentBlockFile;
};


#endif // BLOCKFILE_H
