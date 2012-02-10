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

#ifndef BLOCKFILE_H
#define BLOCKFILE_H

#include <coin/BigNum.h>
#include <coin/Key.h>
#include <coin/Script.h>
#include <coin/Transaction.h>

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
