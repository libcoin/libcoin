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

#include <coinChain/BlockFile.h>

#include <coin/Block.h>
#include <coinChain/BlockIndex.h>
#include <coinChain/BlockChain.h>
#include <coinChain/MessageHeader.h>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#ifdef _WIN32
#include <io.h>
#endif

using namespace std;
using namespace boost;

bool BlockFile::writeToDisk(const Chain& chain, const Block& block, unsigned int& nFileRet, unsigned int& nBlockPosRet, bool commit)
{
    // Open history file to append
    CAutoFile fileout = appendBlockFile(nFileRet);
    if (!fileout)
        return error("Block::WriteToDisk() : AppendBlockFile failed");
    
    // Write index header
    unsigned int nSize = fileout.GetSerializeSize(block);
    fileout << FLATDATA(chain.messageStart().elems) << nSize;
    
    // Write block
    nBlockPosRet = ftell(fileout);
    if (nBlockPosRet == -1)
        return error("Block::WriteToDisk() : ftell failed");
    fileout << block;
    
    // Flush stdio buffers and commit to disk before returning
    fflush(fileout);
    if (commit) {
#ifdef _WIN32
        _commit(_fileno(fileout));
#else
        fsync(fileno(fileout));
#endif
    }
    
    return true;
}

bool BlockFile::readFromDisk(Block& block, unsigned int nFile, unsigned int nBlockPos, bool fReadTransactions) const
{
    block.setNull();
    
    // Open history file to read
    CAutoFile filein = openBlockFile(nFile, nBlockPos);
    if (!filein)
        return error("Block::ReadFromDisk() : OpenBlockFile failed");
    if (!fReadTransactions)
        filein.nType |= SER_BLOCKHEADERONLY;
    
    // Read block
    filein >> block;
    
    // Check the header (this is only a light check to ensure the header is sane - no limit check as it is chain specific)
    if (!block.checkProofOfWork())
        return error("Block::ReadFromDisk() : errors in block header");
    
    return true;
}

bool BlockFile::readFromDisk(Block& block, const CBlockIndex* pindex, bool fReadTransactions) const
{
    if (!fReadTransactions)
        {
        block = pindex->GetBlockHeader();
        return true;
        }
    if (!readFromDisk(block, pindex->nFile, pindex->nBlockPos, fReadTransactions))
        return false;
    if (block.getHash() != pindex->GetBlockHash())
        return error("Block::ReadFromDisk() : GetHash() doesn't match index");
    return true;
}

bool BlockFile::readFromDisk(Transaction& tx, DiskTxPos pos) const
{
    CAutoFile filein = openBlockFile(pos.getFile(), 0);
    if (!filein)
        return error("Transaction::ReadFromDisk() : OpenBlockFile failed");
    
    // Read transaction
    if (fseek(filein, pos.getTxPos(), SEEK_SET) != 0)
        return error("Transaction::ReadFromDisk() : fseek failed");
    filein >> tx;
    
    return true;
}

bool BlockFile::readFromDisk(Transaction& tx, DiskTxPos pos, FILE** pfileRet)
{
    CAutoFile filein = openBlockFile(pos.getFile(), 0, "rb+");
    if (!filein)
        return error("Transaction::ReadFromDisk() : OpenBlockFile failed");
    
    // Read transaction
    if (fseek(filein, pos.getTxPos(), SEEK_SET) != 0)
        return error("Transaction::ReadFromDisk() : fseek failed");
    filein >> tx;
    
    // Return file pointer
    if (pfileRet) {
        if (fseek(filein, pos.getTxPos(), SEEK_SET) != 0)
            return error("Transaction::ReadFromDisk() : second fseek failed");
        *pfileRet = filein.release();
    }
    return true;
}

bool BlockFile::eraseBlockFromDisk(CBlockIndex bindex)
{
    // Open history file
    CAutoFile fileout = openBlockFile(bindex.nFile, bindex.nBlockPos, "rb+");
    if (!fileout)
        return false;
    
    // Overwrite with empty null block
    Block block;
    block.setNull();
    fileout << block;
    
    return true;
}

bool BlockFile::checkDiskSpace(uint64 nAdditionalBytes)
{
    uint64 nFreeBytesAvailable = filesystem::space(_dataDir).available;

    // Check for 15MB because database could create another 10MB log file at any time
    if (nFreeBytesAvailable < (uint64)15000000 + nAdditionalBytes) {
        string strMessage = "Warning: Disk space is low  ";
        strMiscWarning = strMessage;
        printf("*** %s\n", strMessage.c_str());
        throw runtime_error(strMessage);
        return false;
    }
    return true;
}

FILE* BlockFile::openBlockFile(unsigned int nFile, unsigned int nBlockPos) const {
    const char* pszMode = "rb";
    if (nFile == -1)
        return NULL;
    FILE* file = fopen(strprintf("%s/blk%04d.dat", _dataDir.c_str(), nFile).c_str(), pszMode);
    if (!file)
        return NULL;
    if (nBlockPos != 0 && !strchr(pszMode, 'a') && !strchr(pszMode, 'w')) {
        if (fseek(file, nBlockPos, SEEK_SET) != 0) {
            fclose(file);
            return NULL;
        }
    }
    return file;
}

FILE* BlockFile::openBlockFile(unsigned int nFile, unsigned int nBlockPos, const char* pszMode)
{
    if (nFile == -1)
        return NULL;
    FILE* file = fopen(strprintf("%s/blk%04d.dat", _dataDir.c_str(), nFile).c_str(), pszMode);
    if (!file)
        return NULL;
    if (nBlockPos != 0 && !strchr(pszMode, 'a') && !strchr(pszMode, 'w')) {
        if (fseek(file, nBlockPos, SEEK_SET) != 0) {
            fclose(file);
            return NULL;
        }
    }
    return file;
}

FILE* BlockFile::appendBlockFile(unsigned int& nFileRet)
{
    nFileRet = 0;
    loop {
        FILE* file = openBlockFile(_currentBlockFile, 0, "ab");
        if (!file)
            return NULL;
        if (fseek(file, 0, SEEK_END) != 0)
            return NULL;
        // FAT32 filesize max 4GB, fseek and ftell max 2GB, so we must stay under 2GB
        if (ftell(file) < 0x7F000000 - MAX_SIZE) {
            nFileRet = _currentBlockFile;
            return file;
        }
        fclose(file);
        _currentBlockFile++;
    }
}
