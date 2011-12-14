
#ifndef BLOCKFILE_H
#define BLOCKFILE_H

#include "btc/bignum.h"
#include "btc/key.h"
#include "btc/script.h"
#include "btc/tx.h"

#include <boost/noncopyable.hpp>
#include <list>

/// BlockFile encapsulates the Block file on the disk. It supports different queries to the block file.

class Block;
class CBlockIndex;
class Transaction;
class DiskTxPos;

class BlockFile : private boost::noncopyable
{
public:
    BlockFile() : _currentBlockFile(1) {} /// load the block chain index from file

    bool writeToDisk(const Block& block, unsigned int& nFileRet, unsigned int& nBlockPosRet);
    bool readFromDisk(Block& block, unsigned int nFile, unsigned int nBlockPos, bool fReadTransactions=true);
    bool readFromDisk(Block& block, const CBlockIndex* pindex, bool fReadTransactions=true);
    
    bool readFromDisk(Transaction& tx, DiskTxPos pos, FILE** pfileRet=NULL);    
    
    bool eraseBlockFromDisk(CBlockIndex bindex);

    static bool checkDiskSpace(uint64 nAdditionalBytes=0);

protected:
    FILE* openBlockFile(unsigned int nFile, unsigned int nBlockPos, const char* pszMode="rb");
    FILE* appendBlockFile(unsigned int& nFileRet);
    //    bool loadBlockIndex(bool fAllowNew=true);
    
private:
    FILE* _blockFile;    
    unsigned int _currentBlockFile;
};


#endif // BLOCKFILE_H
