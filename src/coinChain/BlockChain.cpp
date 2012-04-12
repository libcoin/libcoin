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

#include <coinChain/BlockChain.h>

#include <coin/Block.h>
#include <coinChain/BlockIndex.h>
#include <coinChain/MessageHeader.h>

#include <coinChain/Peer.h>

#include <coin/Script.h>

#include <boost/lexical_cast.hpp>

using namespace std;
using namespace boost;

//
// BlockChain
//

BlockChain::BlockChain(const Chain& chain, const string dataDir, const char* pszMode) : CDB(dataDir == "" ? CDB::dataDir(chain.dataDirSuffix()) : dataDir, "blkindex.dat", pszMode), _chain(chain), _blockFile(dataDir == "" ? CDB::dataDir(chain.dataDirSuffix()) : dataDir), _genesisBlockIndex(NULL), _bestChainWork(0), _bestInvalidWork(0), _bestChain(0), _bestIndex(NULL), _bestReceivedTime(0), _transactionsUpdated(0) {
    load();
    _acceptBlockTimer = 0;
    _connectInputsTimer = 0;
    _verifySignatureTimer = 0;
    _setBestChainTimer = 0;
    _addToBlockIndexTimer = 0;
}

void BlockChain::outputPerformanceTimings() const {
    printf("Performance timings: accept %d, addTo %.2f%%, setBest %.2f%%, connect %.2f%%, verify %.2f%%", _acceptBlockTimer/1000000, 100.*_addToBlockIndexTimer/_acceptBlockTimer, 100.*_setBestChainTimer/_acceptBlockTimer, 100.*_connectInputsTimer/_acceptBlockTimer, 100.*_verifySignatureTimer/_acceptBlockTimer );
}

bool BlockChain::load(bool allowNew)
{    
    //
    // Load block index
    //
    if (!LoadBlockIndex())
        return false;
    
    //
    // Init with genesis block
    //
    if (_blockChainIndex.empty()) {
        if (!allowNew)
            return false;
        
        // Start new block file
        
        unsigned int nFile;
        unsigned int nBlockPos;
        Block block(_chain.genesisBlock());
        if (!_blockFile.writeToDisk(_chain, block, nFile, nBlockPos, true))
            return error("LoadBlockIndex() : writing genesis block to disk failed");
        if (!addToBlockIndex(block, nFile, nBlockPos))
            return error("LoadBlockIndex() : genesis block not accepted");
        }
    
    return true;
}

void BlockChain::print()
{
    // precompute tree structure
    map<CBlockIndex*, vector<CBlockIndex*> > mapNext;

    for (BlockChainIndex::const_iterator mi = _blockChainIndex.begin(); mi != _blockChainIndex.end(); ++mi) {
        CBlockIndex* pindex = (*mi).second;
        mapNext[pindex->pprev].push_back(pindex);
    }
    
    vector<pair<int, CBlockIndex*> > vStack;
    vStack.push_back(make_pair(0, _genesisBlockIndex));
    
    int nPrevCol = 0;
    while (!vStack.empty())
        {
        int nCol = vStack.back().first;
        CBlockIndex* pindex = vStack.back().second;
        vStack.pop_back();
        
        // print split or gap
        if (nCol > nPrevCol)
            {
            for (int i = 0; i < nCol-1; i++)
                printf("| ");
            printf("|\\\n");
            }
        else if (nCol < nPrevCol)
            {
            for (int i = 0; i < nCol; i++)
                printf("| ");
            printf("|\n");
            }
        nPrevCol = nCol;
        
        // print columns
        for (int i = 0; i < nCol; i++)
            printf("| ");
        
        // print item
        Block block;
        _blockFile.readFromDisk(block, pindex);
        printf("%d (%u,%u) %s  %s  tx %d",
               pindex->nHeight,
               pindex->nFile,
               pindex->nBlockPos,
               block.getHash().toString().substr(0,20).c_str(),
               DateTimeStrFormat("%x %H:%M:%S", block.getBlockTime()).c_str(),
               block.getNumTransactions());
        
        //        PrintWallets(block);
        
        // put the main timechain first
        vector<CBlockIndex*>& vNext = mapNext[pindex];
        for (int i = 0; i < vNext.size(); i++)
            {
            if (vNext[i]->pnext)
                {
                swap(vNext[0], vNext[i]);
                break;
                }
            }
        
        // iterate children
        for (int i = 0; i < vNext.size(); i++)
            vStack.push_back(make_pair(nCol+i, vNext[i]));
        }
}

CBlockLocator BlockChain::getBestLocator() const {
    // lock the pool and chain for reading
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);

    CBlockLocator l;
    const CBlockIndex* pindex = getBestIndex();
    l.vHave.clear();
    int nStep = 1;
    while (pindex) {
        l.vHave.push_back(pindex->GetBlockHash());
        
        // Exponentially larger steps back
        for (int i = 0; pindex && i < nStep; i++)
            pindex = pindex->pprev;
        if (l.vHave.size() > 10)
            nStep *= 2;
        }
    l.vHave.push_back(getGenesisHash());
    return l;
}

bool BlockChain::isInitialBlockDownload() const
{
    // lock the pool and chain for reading
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);
    const int initialBlockThreshold = 120; // Regard blocks up until N-threshold as "initial download"

    if (_bestIndex == NULL || getBestHeight() < (getTotalBlocksEstimate()-initialBlockThreshold))
        return true;
    
    static int64 nLastUpdate;
    static CBlockIndex* pindexLastBest;
    if (_bestIndex != pindexLastBest) {
        pindexLastBest = _bestIndex;
        nLastUpdate = GetTime();
    }
    return (GetTime() - nLastUpdate < 10 &&
            _bestIndex->GetBlockTime() < GetTime() - 24 * 60 * 60);
}

int BlockChain::getDistanceBack(const CBlockLocator& locator) const
{
    // lock the pool and chain for reading
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);
    // Retrace how far back it was in the sender's branch
    int nDistance = 0;
    int nStep = 1;
    BOOST_FOREACH(const uint256& hash, locator.vHave) {
        BlockChainIndex::const_iterator mi = _blockChainIndex.find(hash);
        if (mi != _blockChainIndex.end()) {
            CBlockIndex* pindex = (*mi).second;
            if (isInMainChain(pindex))
                return nDistance;
        }
        nDistance += nStep;
        if (nDistance > 10)
            nStep *= 2;
    }
    return nDistance;
}

void BlockChain::getBlock(const uint256 hash, Block& block) const
{
    // lock the pool and chain for reading
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);
    block.setNull();
    BlockChainIndex::const_iterator index = _blockChainIndex.find(hash);
    if (index != _blockChainIndex.end()) {
        _blockFile.readFromDisk(block, index->second);
    }
    // now try if the hash was for a transaction
    TxIndex txidx;
    if(ReadTxIndex(hash, txidx)) {
        _blockFile.readFromDisk(block, txidx.getPos().getFile(), txidx.getPos().getBlockPos());
    }
}

void BlockChain::getBlock(const CBlockIndex* index, Block& block) const {
    // lock the pool and chain for reading
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);
    _blockFile.readFromDisk(block, index);
}



const CBlockIndex* BlockChain::getBlockIndex(const CBlockLocator& locator) const
{
    // lock the pool and chain for reading
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);
    // Find the first block the caller has in the main chain
    BOOST_FOREACH(const uint256& hash, locator.vHave) {
        BlockChainIndex::const_iterator mi = _blockChainIndex.find(hash);
        if (mi != _blockChainIndex.end()) {
            CBlockIndex* pindex = (*mi).second;
            if (isInMainChain(pindex))
                return pindex;
        }
    }
    return _genesisBlockIndex;
}

const CBlockIndex* BlockChain::getBlockIndex(const uint256 hash) const
{
    // lock the pool and chain for reading
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);
    BlockChainIndex::const_iterator index = _blockChainIndex.find(hash);
    if (index != _blockChainIndex.end())
        return index->second;
    else
        return NULL;
}

double BlockChain::getDifficulty(const CBlockIndex* pindex) const {
    // Floating point number that is a multiple of the minimum difficulty,
    // minimum difficulty = 1.0.
    
    if(pindex == NULL) pindex = getBestIndex();
    
    int nShift = (pindex->nBits >> 24) & 0xff;
    
    double dDiff =
    (double)0x0000ffff / (double)(pindex->nBits & 0x00ffffff);
    
    while (nShift < 29) {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29) {
        dDiff /= 256.0;
        nShift--;
    }
    
    return dDiff;
}

uint256 BlockChain::getBlockHash(const CBlockLocator& locator)
{
    // lock the pool and chain for reading
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);
    // Find the first block the caller has in the main chain
    BOOST_FOREACH(const uint256& hash, locator.vHave) {
    BlockChainIndex::const_iterator mi = _blockChainIndex.find(hash);
    if (mi != _blockChainIndex.end())
        {
        CBlockIndex* pindex = (*mi).second;
        if (isInMainChain(pindex))
            return hash;
        }
    }
    return getGenesisHash();
}

CBlockIndex* BlockChain::getHashStopIndex(uint256 hashStop) const 
{
    // lock the pool and chain for reading
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);

    BlockChainIndex::const_iterator mi = _blockChainIndex.find(hashStop);
    if (mi == _blockChainIndex.end())
        return NULL;
    return (*mi).second;
}

bool BlockChain::connectInputs(const Transaction& tx, map<uint256, TxIndex>& mapTestPool, DiskTxPos posThisTx,
                   const CBlockIndex* pindexBlock, int64& nFees, bool fBlock, bool fMiner, int64 nMinFee) const
{    
    // lock the pool and chain for reading
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);

    // BIP0016 check - if the block is newer than the BIP0016 date enforce strictPayToScriptHash
    // Note that this will reject some transactions during block download, which should not be a problem.
    bool strictPayToScriptHash = (pindexBlock->nTime > _chain.timeStamp(Chain::BIP0016));
    
    int64 t0 = GetTimeMicros();

    // Take over previous transactions' spent pointers
    if (!tx.isCoinBase()) {
        int64 nValueIn = 0;
        for (int i = 0; i < tx.getNumInputs(); i++) {
            Coin prevout = tx.getInput(i).prevout();
            
            // Read txindex
            TxIndex txindex;
            bool fFound = true;
            if ((fBlock || fMiner) && mapTestPool.count(prevout.hash)) {
                // Get txindex from current proposed changes
                txindex = mapTestPool[prevout.hash];
            }
            else {
                // Read txindex from txdb
                fFound = ReadTxIndex(prevout.hash, txindex);
            }
            if (!fFound && (fBlock || fMiner))
                return fMiner ? false : error("ConnectInputs() : %s prev tx %s index entry not found", tx.getHash().toString().substr(0,10).c_str(),  prevout.hash.toString().substr(0,10).c_str());
            
            // Read txPrev
            Transaction txPrev;
            if (!fFound || txindex.getPos() == DiskTxPos(1,1,1)) {
                // Get prev tx from single transactions in memory
                TransactionIndex::const_iterator index = _transactionIndex.find(prevout.hash);
                if (index == _transactionIndex.end())
                    return error("ConnectInputs() : %s mapTransactions prev not found %s", tx.getHash().toString().substr(0,10).c_str(),  prevout.hash.toString().substr(0,10).c_str());
                txPrev = index->second;
                if (!fFound)
                    txindex.resizeSpents(txPrev.getNumOutputs());
            }
            else {
                // Get prev tx from disk
                if (!_blockFile.readFromDisk(txPrev, txindex.getPos()))
                    return error("ConnectInputs() : %s ReadFromDisk prev tx %s failed", tx.getHash().toString().substr(0,10).c_str(),  prevout.hash.toString().substr(0,10).c_str());
                }
            
            if (prevout.index >= txPrev.getNumOutputs() || prevout.index >= txindex.getNumSpents())
                return error("ConnectInputs() : %s prevout.n out of range %d %d %d prev tx %s\n%s", tx.getHash().toString().substr(0,10).c_str(), prevout.index, txPrev.getNumOutputs(), txindex.getNumSpents(), prevout.hash.toString().substr(0,10).c_str(), txPrev.toString().c_str());
            
            // If prev is coinbase, check that it's matured
            if (txPrev.isCoinBase())
                for (const CBlockIndex* pindex = pindexBlock; pindex && pindexBlock->nHeight - pindex->nHeight < COINBASE_MATURITY; pindex = pindex->pprev)
                    if (pindex->nBlockPos == txindex.getPos().getBlockPos() && pindex->nFile == txindex.getPos().getFile())
                        return error("ConnectInputs() : tried to spend coinbase at depth %d", pindexBlock->nHeight - pindex->nHeight);
            
            // Verify signature only if not downloading initial chain
            if (!(fBlock && (isInitialBlockDownload()))) {
                int64 t1 = GetTimeMicros();

                if (!VerifySignature(txPrev, tx, i, strictPayToScriptHash, 0))
                    return error("ConnectInputs() : %s VerifySignature failed", tx.getHash().toString().substr(0,10).c_str());
            
                _verifySignatureTimer += GetTimeMicros() - t1;
            }
            // Check for conflicts
            if (!txindex.getSpent(prevout.index).isNull())
                return fMiner ? false : error("ConnectInputs() : %s prev tx already used at %s", tx.getHash().toString().substr(0,10).c_str(), txindex.getSpent(prevout.index).toString().c_str());
            
            // Check for negative or overflow input values
            nValueIn += txPrev.getOutput(prevout.index).value();
            if (!MoneyRange(txPrev.getOutput(prevout.index).value()) || !MoneyRange(nValueIn))
                return error("ConnectInputs() : txin values out of range");
            
            // Mark outpoints as spent
            txindex.setSpent(prevout.index, posThisTx);
            
            // Write back - we add all changes to the testpool and leave it to the caller to update the db (as this method is const)
            if (fBlock || fMiner) {
                mapTestPool[prevout.hash] = txindex;
            }
        }
        
        if (nValueIn < tx.getValueOut())
            return error("ConnectInputs() : %s value in < value out", tx.getHash().toString().substr(0,10).c_str());
        
        // Tally transaction fees
        int64 nTxFee = nValueIn - tx.getValueOut();
        if (nTxFee < 0)
            return error("ConnectInputs() : %s nTxFee < 0", tx.getHash().toString().substr(0,10).c_str());
        if (nTxFee < nMinFee)
            return false;
        nFees += nTxFee;
        if (!MoneyRange(nFees))
            return error("ConnectInputs() : nFees out of range");
        }
    
    if (fBlock) {
        // Add transaction to changes
        mapTestPool[tx.getHash()] = TxIndex(posThisTx, tx.getNumOutputs());
    }
    else if (fMiner) {
        // Add transaction to test pool
        mapTestPool[tx.getHash()] = TxIndex(DiskTxPos(1,1,1), tx.getNumOutputs());
    }
    _connectInputsTimer += GetTimeMicros() - t0;

    return true;
}

bool BlockChain::disconnectInputs(const Transaction& tx)
{
    // Relinquish previous transactions' spent pointers
    if (!tx.isCoinBase()) {
        BOOST_FOREACH(const Input& txin, tx.getInputs()) {
            Coin prevout = txin.prevout();
            
            // Get prev txindex from disk
            TxIndex txindex;
            if (!ReadTxIndex(prevout.hash, txindex))
                return error("DisconnectInputs() : ReadTxIndex failed");
            
            if (prevout.index >= txindex.getNumSpents())
                return error("DisconnectInputs() : prevout.n out of range");
            
            // Mark outpoint as not spent
            txindex.setNotSpent(prevout.index);
            
            // Write back
            if (!UpdateTxIndex(prevout.hash, txindex))
                return error("DisconnectInputs() : UpdateTxIndex failed");
            }
        }
    
    // Remove transaction from index
    // This can fail if a duplicate of this transaction was in a chain that got
    // reorganized away. This is only possible if this transaction was completely
    // spent, so erasing it would be a no-op anway.
    EraseTxIndex(tx);    
    
    return true;
}

bool BlockChain::isInMainChain(const uint256 hash) const {
    // lock the pool and chain for reading
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);

    BlockChainIndex::const_iterator i = _blockChainIndex.find(hash);
    return i != _blockChainIndex.end();
}

int BlockChain::getHeight(const uint256 hash) const
{
    // lock the pool and chain for reading
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);

    // first check if this is a block hash
    BlockChainIndex::const_iterator i = _blockChainIndex.find(hash);
    if (i != _blockChainIndex.end()) {
        return i->second->nHeight;
    }
    
    // assume it is a tx
    
    TxIndex txindex;
    if(!ReadTxIndex(hash, txindex))
        return -1;
    
    // Read block header
    Block block;
    if (!_blockFile.readFromDisk(block, txindex.getPos().getFile(), txindex.getPos().getBlockPos(), false))
        return -1;
    // Find the block in the index
    BlockChainIndex::const_iterator mi = _blockChainIndex.find(block.getHash());
    if (mi == _blockChainIndex.end())
        return -1;
    CBlockIndex* pindex = (*mi).second;
    if (!pindex || !isInMainChain(pindex))
        return -1;
    return pindex->nHeight;
}

// Private interface

bool BlockChain::ReadTxIndex(uint256 hash, TxIndex& txindex) const
{
    txindex.setNull();
    return Read(make_pair(string("tx"), hash), txindex);
}

bool BlockChain::UpdateTxIndex(uint256 hash, const TxIndex& txindex)
{
    return Write(make_pair(string("tx"), hash), txindex);
}

bool BlockChain::AddTxIndex(const Transaction& tx, const DiskTxPos& pos, int nHeight)
{
    // Add to tx index
    uint256 hash = tx.getHash();
    TxIndex txindex(pos, tx.getNumOutputs());
    
    return UpdateTxIndex(hash, txindex);
    //    return Write(make_pair(string("tx"), hash), txindex);
}


bool BlockChain::EraseTxIndex(const Transaction& tx)
{
    uint256 hash = tx.getHash();
    return Erase(make_pair(string("tx"), hash));
}

bool BlockChain::haveTx(uint256 hash, bool must_be_confirmed) const
{
    if(Exists(make_pair(string("tx"), hash)))
        return true;
    else if(!must_be_confirmed && _transactionIndex.count(hash))
        return true;
    else
        return false;
}

bool BlockChain::isFinal(const Transaction& tx, int nBlockHeight, int64 nBlockTime) const
{
    // Time based nLockTime implemented in 0.1.6
    if (tx.lockTime() == 0)
        return true;
    if (nBlockHeight == 0)
        nBlockHeight = getBestHeight();
    if (nBlockTime == 0)
        nBlockTime = GetAdjustedTime();
    if ((int64)tx.lockTime() < (tx.lockTime() < LOCKTIME_THRESHOLD ? (int64)nBlockHeight : nBlockTime))
        return true;
    BOOST_FOREACH(const Input& txin, tx.getInputs())
        if (!txin.isFinal())
            return false;
    return true;
}

bool BlockChain::haveBlock(uint256 hash) const
{
    return _blockChainIndex.count(hash);
}

bool BlockChain::ReadOwnerTxes(uint160 hash160, int nMinHeight, vector<Transaction>& vtx)
{
    vtx.clear();
    
    // Get cursor
    Dbc* pcursor = CDB::GetCursor();
    if (!pcursor)
        return false;
    
    unsigned int fFlags = DB_SET_RANGE;
    loop
    {
    // Read next record
    CDataStream ssKey;
    if (fFlags == DB_SET_RANGE)
        ssKey << string("owner") << hash160 << DiskTxPos(0, 0, 0);
    CDataStream ssValue;
    int ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
    fFlags = DB_NEXT;
    if (ret == DB_NOTFOUND)
        break;
    else if (ret != 0)
        {
        pcursor->close();
        return false;
        }
    
    // Unserialize
    string strType;
    uint160 hashItem;
    DiskTxPos pos;
    ssKey >> strType >> hashItem >> pos;
    int nItemHeight;
    ssValue >> nItemHeight;
    
    // Read transaction
    if (strType != "owner" || hashItem != hash160)
        break;
    if (nItemHeight >= nMinHeight)
        {
        vtx.resize(vtx.size()+1);
        if (!_blockFile.readFromDisk(vtx.back(), pos))
            {
            pcursor->close();
            return false;
            }
        }
    }
    
    pcursor->close();
    return true;
}

bool BlockChain::ReadDiskTx(uint256 hash, Transaction& tx, TxIndex& txindex) const
{
    tx.setNull();
    if (!ReadTxIndex(hash, txindex))
        return false;
    return (_blockFile.readFromDisk(tx, txindex.getPos()));
}

bool BlockChain::readDiskTx(uint256 hash, Transaction& tx) const
{
    TxIndex txindex;
    return ReadDiskTx(hash, tx, txindex);
}

bool BlockChain::readDiskTx(uint256 hash, Transaction& tx, int64& height, int64& time) const
{
    TxIndex txindex;
    if(!ReadDiskTx(hash, tx, txindex)) return false;
    
    // Read block header
    Block block;
    if (!_blockFile.readFromDisk(block, txindex.getPos().getFile(), txindex.getPos().getBlockPos(), false))
        return false;
    // Find the block in the index
    BlockChainIndex::const_iterator mi = _blockChainIndex.find(block.getHash());
    if (mi == _blockChainIndex.end())
        return false;
    CBlockIndex* pindex = (*mi).second;
    if (!pindex || !isInMainChain(pindex))
        return false;

    height = pindex->nHeight;
    time = pindex->nTime;
    
    return true;
}


bool BlockChain::ReadDiskTx(Coin outpoint, Transaction& tx, TxIndex& txindex) const
{
    return ReadDiskTx(outpoint.hash, tx, txindex);
}

bool BlockChain::ReadDiskTx(Coin outpoint, Transaction& tx) const
{
    TxIndex txindex;
    return ReadDiskTx(outpoint.hash, tx, txindex);
}

bool BlockChain::WriteBlockIndex(const CDiskBlockIndex& blockindex)
{
    return Write(make_pair(string("blockindex"), blockindex.GetBlockHash()), blockindex);
}

bool BlockChain::EraseBlockIndex(uint256 hash)
{
    return Erase(make_pair(string("blockindex"), hash));
}

bool BlockChain::ReadHashBestChain()
{
    return Read(string("hashBestChain"), _bestChain);
}

bool BlockChain::WriteHashBestChain(const uint256 bestChain)
{
    return Write(string("hashBestChain"), bestChain);
}

bool BlockChain::ReadBestInvalidWork()
{
    return Read(string("bnBestInvalidWork"), _bestInvalidWork);
}

bool BlockChain::WriteBestInvalidWork()
{
    return Write(string("bnBestInvalidWork"), _bestInvalidWork);
}

CBlockIndex* BlockChain::InsertBlockIndex(uint256 hash)
{
    if (hash == 0)
        return NULL;
    
    // Return existing
    BlockChainIndex::const_iterator mi = _blockChainIndex.find(hash);
    if (mi != _blockChainIndex.end())
        return (*mi).second;
    
    // Create new
    CBlockIndex* pindexNew = new CBlockIndex();
    if (!pindexNew)
        throw runtime_error("LoadBlockIndex() : new CBlockIndex failed");
    mi = _blockChainIndex.insert(make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);
    
    return pindexNew;
}

bool BlockChain::LoadBlockIndex()
{
    // Get database cursor
    Dbc* pcursor = CDB::GetCursor();
    if (!pcursor)
        return false;
    
    // Load _blockChainIndex
    unsigned int fFlags = DB_SET_RANGE;
    loop {
        // Read next record
        CDataStream ssKey;
        if (fFlags == DB_SET_RANGE)
            ssKey << make_pair(string("blockindex"), uint256(0));
        CDataStream ssValue;
        int ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
            return false;
        
        // Unserialize
        string strType;
        ssKey >> strType;
        if (strType == "blockindex") {
            CDiskBlockIndex diskindex;
            ssValue >> diskindex;
            
            // Construct block index object
            CBlockIndex* pindexNew = InsertBlockIndex(diskindex.GetBlockHash());
            pindexNew->pprev          = InsertBlockIndex(diskindex.hashPrev);
            pindexNew->pnext          = InsertBlockIndex(diskindex.hashNext);
            pindexNew->nFile          = diskindex.nFile;
            pindexNew->nBlockPos      = diskindex.nBlockPos;
            pindexNew->nHeight        = diskindex.nHeight;
            pindexNew->nVersion       = diskindex.nVersion;
            pindexNew->hashMerkleRoot = diskindex.hashMerkleRoot;
            pindexNew->nTime          = diskindex.nTime;
            pindexNew->nBits          = diskindex.nBits;
            pindexNew->nNonce         = diskindex.nNonce;
            
            // Watch for genesis block
            if (_genesisBlockIndex == NULL && diskindex.GetBlockHash() == getGenesisHash())
                _genesisBlockIndex = pindexNew;
            
            if (!pindexNew->checkIndex(_chain.proofOfWorkLimit()))
                throw runtime_error(("LoadBlockIndex() : CheckIndex failed at " + lexical_cast<string>(pindexNew->nHeight)).c_str());
            //            if (!pindexNew->CheckIndex())
            //                return error("LoadBlockIndex() : CheckIndex failed at %d", pindexNew->nHeight);
        }
        else {
            break;
        }
    }
    pcursor->close();
    
    // Calculate bnChainWork
    vector<pair<int, CBlockIndex*> > vSortedByHeight;
    vSortedByHeight.reserve(_blockChainIndex.size());
    for(BlockChainIndex::const_iterator item = _blockChainIndex.begin(); item != _blockChainIndex.end(); ++item)
    {
    CBlockIndex* pindex = item->second;
    vSortedByHeight.push_back(make_pair(pindex->nHeight, pindex));
    }

    sort(vSortedByHeight.begin(), vSortedByHeight.end());
    BOOST_FOREACH(const PAIRTYPE(int, CBlockIndex*)& item, vSortedByHeight)
    {
    CBlockIndex* pindex = item.second;
    pindex->bnChainWork = (pindex->pprev ? pindex->pprev->bnChainWork : 0) + pindex->GetBlockWork();
    }
    
    // Load hashBestChain pointer to end of best chain
    if (!ReadHashBestChain()) {
        if (_genesisBlockIndex == NULL)
            return true;
        return error("BlockChain::LoadBlockIndex() : hashBestChain not loaded");
    }
    if (!_blockChainIndex.count(_bestChain))
        return error("BlockChain::LoadBlockIndex() : hashBestChain not found in the block index");
    _bestIndex = _blockChainIndex[_bestChain];
    _bestChainWork = _bestIndex->bnChainWork;
    printf("LoadBlockIndex(): hashBestChain=%s  height=%d\n", _bestChain.toString().substr(0,20).c_str(), getBestHeight());
    
    // Load _bestChainWork, OK if it doesn't exist
    ReadBestInvalidWork();
    
    // Verify blocks in the best chain
    CBlockIndex* pindexFork = NULL;
    for (CBlockIndex* pindex = _bestIndex; pindex && pindex->pprev; pindex = pindex->pprev)
        {
        //        if (pindex->nHeight < getBestHeight()-2500 && !mapArgs.count("-checkblocks"))
        if (pindex->nHeight < getBestHeight()-2500)
            break;
        Block block;
        if (!_blockFile.readFromDisk(block, pindex))
            return error("LoadBlockIndex() : block.ReadFromDisk failed");
        if (!block.checkBlock(_chain.proofOfWorkLimit()))
            {
            printf("LoadBlockIndex() : *** found bad block at %d, hash=%s\n", pindex->nHeight, pindex->GetBlockHash().toString().c_str());
            pindexFork = pindex->pprev;
            }
        }
    if (pindexFork) {
        // Reorg back to the fork
        printf("LoadBlockIndex() : *** moving best chain pointer back to block %d\n", pindexFork->nHeight);
        Block block;
        if (!_blockFile.readFromDisk(block, pindexFork))
            return error("LoadBlockIndex() : block.ReadFromDisk failed");
        setBestChain(block, pindexFork);
    }
    
    return true;
}

bool BlockChain::disconnectBlock(const Block& block, CBlockIndex* pindex)
{
    // Disconnect in reverse order
    for (int i = block.getNumTransactions()-1; i >= 0; i--)
        if (!disconnectInputs(block.getTransaction(i)))
            return false;
    
    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    if (pindex->pprev) {
        CDiskBlockIndex blockindexPrev(pindex->pprev);
        blockindexPrev.hashNext = 0;
        if (!WriteBlockIndex(blockindexPrev))
            return error("DisconnectBlock() : WriteBlockIndex failed");
    }
    
    return true;
}

bool BlockChain::connectBlock(const Block& block, CBlockIndex* pindex)
{
    // Check it again in case a previous version let a bad block in
    if (!block.checkBlock(_chain.proofOfWorkLimit()))
        return false;
    
    // Do not allow blocks that contain transactions which 'overwrite' older transactions,
    // unless those are already completely spent.
    // If such overwrites are allowed, coinbases and transactions depending upon those
    // can be duplicated to remove the ability to spend the first instance -- even after
    // being sent to another address.
    // See BIP30 and http://r6.ca/blog/20120206T005236Z.html for more information.
    // This logic is not necessary for memory pool transactions, as AcceptToMemoryPool
    // already refuses previously-known transaction id's entirely.
    // This rule applies to all blocks whose timestamp is after March 15, 2012, 0:00 UTC.
    // On testnet it is enabled as of februari 20, 2012, 0:00 UTC.
    if (pindex->nTime > _chain.timeStamp(Chain::BIP0030))
        BOOST_FOREACH(const Transaction& tx, block.getTransactions()) {
        TxIndex txindexOld;
            if (ReadTxIndex(tx.getHash(), txindexOld))
                BOOST_FOREACH(const DiskTxPos &pos, txindexOld.getSpents())
                if (pos.isNull())
                    return false;
        }

    
    //// issue here: it doesn't know the version
    unsigned int nTxPos = pindex->nBlockPos + ::GetSerializeSize(Block(), SER_DISK) - 1 + GetSizeOfCompactSize(block.getNumTransactions());
    
    map<uint256, TxIndex> queuedChanges;
    int64 fees = 0;
    for(int i = 0; i < block.getNumTransactions(); ++i) {
        const Transaction& tx = block.getTransaction(i);
        DiskTxPos posThisTx(pindex->nFile, pindex->nBlockPos, nTxPos);
        nTxPos += ::GetSerializeSize(tx, SER_DISK);
        
        if (!connectInputs(tx, queuedChanges, posThisTx, pindex, fees, true, false))
            return false;
    }
    // Write queued txindex changes
    for (map<uint256, TxIndex>::iterator mi = queuedChanges.begin(); mi != queuedChanges.end(); ++mi) {
        if (!UpdateTxIndex((*mi).first, (*mi).second))
            return error("ConnectBlock() : UpdateTxIndex failed");
    }
    
    if (block.getTransaction(0).getValueOut() > _chain.subsidy(pindex->nHeight) + fees)
        return false;
    
    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    if (pindex->pprev) {
        CDiskBlockIndex blockindexPrev(pindex->pprev);
        blockindexPrev.hashNext = pindex->GetBlockHash();
        if (!WriteBlockIndex(blockindexPrev))
            return error("ConnectBlock() : WriteBlockIndex failed");
    }
    
    // Watch for transactions paying to me
    //    BOOST_FOREACH(Transaction& tx, vtx)
    //        SyncWithWallets(tx, this, true);
    
    return true;
}

bool BlockChain::reorganize(const Block& block, CBlockIndex* pindexNew)
{
    printf("REORGANIZE\n");
    
    // Find the fork
    CBlockIndex* pfork = _bestIndex;
    CBlockIndex* plonger = pindexNew;
    while (pfork != plonger) {
        while (plonger->nHeight > pfork->nHeight)
            if (!(plonger = plonger->pprev))
                return error("Reorganize() : plonger->pprev is null");
        if (pfork == plonger)
            break;
        if (!(pfork = pfork->pprev))
            return error("Reorganize() : pfork->pprev is null");
    }

    printf("REORG heights: current %d, fork %d, longer %d)\n", _bestIndex->nHeight, pfork->nHeight, plonger->nHeight);
    
    // List of what to disconnect
    vector<CBlockIndex*> vDisconnect;
    for (CBlockIndex* pindex = _bestIndex; pindex != pfork; pindex = pindex->pprev)
        vDisconnect.push_back(pindex);
    
    printf("REORG disconnect length: %d\n", vDisconnect.size());
           
    // List of what to connect
    vector<CBlockIndex*> vConnect;
    for (CBlockIndex* pindex = pindexNew; pindex != pfork; pindex = pindex->pprev)
        vConnect.push_back(pindex);
    reverse(vConnect.begin(), vConnect.end());

    printf("REORG connect length: %d\n", vConnect.size());
    
    // Disconnect shorter branch
    vector<Transaction> vResurrect;
    BOOST_FOREACH(CBlockIndex* pindex, vDisconnect) {
        Block block;
        if (!_blockFile.readFromDisk(block, pindex))
            return error("Reorganize() : ReadFromDisk for disconnect failed");
        if (!disconnectBlock(block, pindex))
            return error("Reorganize() : DisconnectBlock failed");
        
        // Queue memory transactions to resurrect
        BOOST_FOREACH(const Transaction& tx, block.getTransactions())
        if (!tx.isCoinBase()) {
            vResurrect.push_back(tx);
            printf("REORG resurrect: %s\n", tx.getHash().toString().c_str());
        }
    }
    
    // Connect longer branch
    vector<Transaction> vDelete;
    for (int i = 0; i < vConnect.size(); i++) {
        CBlockIndex* pindex = vConnect[i];
        Block block;
        if (!_blockFile.readFromDisk(block,pindex))
            return error("Reorganize() : ReadFromDisk for connect failed");
        if (!connectBlock(block, pindex)) {
            // Invalid block
            TxnAbort();
            return error("Reorganize() : ConnectBlock failed");
        }
        
        // Queue memory transactions to delete
        BOOST_FOREACH(const Transaction& tx, block.getTransactions())
        vDelete.push_back(tx);
    }
    // At this point we have finalized building the transaction and are ready to commit it and 
    // update the transaction pool. We hence lock the chain and pool mutex:
    
    boost::unique_lock< boost::shared_mutex > lock(_chain_and_pool_access);
    //    _bestChain = pindexNew->GetBlockHash();
    if (!WriteHashBestChain(pindexNew->GetBlockHash()))
        return error("Reorganize() : WriteHashBestChain failed");
    
    
    // Make sure it's successfully written to disk before changing memory structure
    if (!TxnCommit())
        return error("Reorganize() : TxnCommit failed");
    
    // Disconnect shorter branch
    BOOST_FOREACH(CBlockIndex* pindex, vDisconnect)
    if (pindex->pprev)
        pindex->pprev->pnext = NULL;
    
    // Connect longer branch
    BOOST_FOREACH(CBlockIndex* pindex, vConnect)
    if (pindex->pprev)
        pindex->pprev->pnext = pindex;
    
    // Resurrect memory transactions that were in the disconnected branch
    BOOST_FOREACH(Transaction& tx, vResurrect)
    AcceptToMemoryPool(tx, false);
    
    // Delete redundant memory transactions that are in the connected branch
    BOOST_FOREACH(Transaction& tx, vDelete)
    RemoveFromMemoryPool(tx);
    
    return true;
}

void BlockChain::InvalidChainFound(CBlockIndex* pindexNew)
{
    if (pindexNew->bnChainWork > _bestInvalidWork) {
        _bestInvalidWork = pindexNew->bnChainWork;
        WriteBestInvalidWork();
    }
    printf("InvalidChainFound: invalid block=%s  height=%d  work=%s\n", pindexNew->GetBlockHash().toString().substr(0,20).c_str(), pindexNew->nHeight, pindexNew->bnChainWork.toString().c_str());
    printf("InvalidChainFound:  current best=%s  height=%d  work=%s\n", _bestChain.toString().substr(0,20).c_str(), getBestHeight(), _bestChainWork.toString().c_str());
    if (_bestIndex && _bestInvalidWork > _bestChainWork + _bestIndex->GetBlockWork() * 6)
        printf("InvalidChainFound: WARNING: Displayed transactions may not be correct!  You may need to upgrade, or other nodes may need to upgrade.\n");
}

bool BlockChain::setBestChain(const Block& block, CBlockIndex* pindexNew)
{
    int64 t0 = GetTimeMicros();

    uint256 hash = block.getHash();
    
    uint256 oldBestChain = _bestChain;
    TxnBegin();
    if (_genesisBlockIndex == NULL && hash == getGenesisHash()) {
        //        _bestChain = hash;
        WriteHashBestChain(hash);
        _bestChain = oldBestChain;
        if (!TxnCommit())
            return error("SetBestChain() : TxnCommit failed");
        _genesisBlockIndex = pindexNew;
    }
    else if (block.getPrevBlock() == _bestChain) {
        // Adding to current best branch
        //        _bestChain = hash;
        if (!connectBlock(block, pindexNew) || !WriteHashBestChain(hash)) {
            _bestChain = oldBestChain;
            TxnAbort();
            InvalidChainFound(pindexNew);
            return error("SetBestChain() : ConnectBlock failed");
        }
        
        // We are now ready to commit the changes to the DB and update the transaction pool
        // Hence we lock the mutex.
        boost::unique_lock< boost::shared_mutex > lock(_chain_and_pool_access);

        if (!TxnCommit()) {
            _bestChain = oldBestChain;
            return error("SetBestChain() : TxnCommit failed");
        }
        
        // Add to current best branch
        pindexNew->pprev->pnext = pindexNew;
        
        // Delete redundant memory transactions
        for(int i = 0; i < block.getNumTransactions(); ++i)
            RemoveFromMemoryPool(block.getTransaction(i));
    }
    else {
        // New best branch
        if (!reorganize(block, pindexNew)) {
            TxnAbort();
            InvalidChainFound(pindexNew);
            _bestChain = oldBestChain;
            return error("SetBestChain() : Reorganize failed");
        }
    }
    /*
     // Update best block in wallet (so we can detect restored wallets)
     if (!isInitialBlockDownload())
     {
     const CBlockLocator locator(pindexNew);
     ::SetBestChain(locator);
     }
     */
    // New best block
    _bestChain = hash;
    _bestIndex = pindexNew;
    _bestChainWork = pindexNew->bnChainWork;
    _bestReceivedTime = GetTime();
    _transactionsUpdated++;
    printf("SetBestChain: new best=%s  height=%d  work=%s\n", _bestChain.toString().substr(0,20).c_str(), getBestHeight(), _bestChainWork.toString().c_str());
    _setBestChainTimer += GetTimeMicros() - t0;

    return true;
}


bool BlockChain::addToBlockIndex(const Block& block, unsigned int nFile, unsigned int nBlockPos)
{
    int64 t0 = GetTimeMicros();
    // Check for duplicate
    uint256 hash = block.getHash();
    if (_blockChainIndex.count(hash))
        return error("AddToBlockIndex() : %s already exists", hash.toString().substr(0,20).c_str());
    
    // --- BEGIN addBlock
    // Construct new block index object
    CBlockIndex* pindexNew = new CBlockIndex(nFile, nBlockPos, block);
    if (!pindexNew)
        return error("AddToBlockIndex() : new CBlockIndex failed");
    BlockChainIndex::const_iterator mi = _blockChainIndex.insert(make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);
    BlockChainIndex::const_iterator miPrev = _blockChainIndex.find(block.getPrevBlock());
    if (miPrev != _blockChainIndex.end()) {
        pindexNew->pprev = (*miPrev).second;
        pindexNew->nHeight = pindexNew->pprev->nHeight + 1;
    }
    pindexNew->bnChainWork = (pindexNew->pprev ? pindexNew->pprev->bnChainWork : 0) + pindexNew->GetBlockWork();
    
    TxnBegin();
    WriteBlockIndex(CDiskBlockIndex(pindexNew));
    if (!TxnCommit())
        return false;
    
    // --- END addBlock
    
    // New best
    if (pindexNew->bnChainWork > _bestChainWork)
        if (!setBestChain(block, pindexNew))
            return false;
    
    if (pindexNew == _bestIndex) {
        // Notify UI to display prev block's coinbase if it was ours
        static uint256 hashPrevBestCoinBase;
        //        UpdatedTransaction(hashPrevBestCoinBase);
        hashPrevBestCoinBase = block.getTransaction(0).getHash();
    }
    
    _addToBlockIndexTimer += GetTimeMicros() - t0;
    return true;
}

bool BlockChain::acceptBlock(const Block& block)
{
    int64 t0 = GetTimeMicros();
    // Check for duplicate
    uint256 hash = block.getHash();
    if (_blockChainIndex.count(hash))
        return error("AcceptBlock() : block already in _blockChainIndex");
    
    // Get prev block index
    BlockChainIndex::const_iterator mi = _blockChainIndex.find(block.getPrevBlock());
    if (mi == _blockChainIndex.end())
        return error("AcceptBlock() : prev block not found");
    CBlockIndex* pindexPrev = (*mi).second;
    int height = pindexPrev->nHeight+1;
    
    // Check proof of work
    if (block.getBits() != _chain.nextWorkRequired(pindexPrev))
        return error("AcceptBlock() : incorrect proof of work");
    //   if (block.getBits() != GetNextWorkRequired(pindexPrev))
    //        return error("AcceptBlock() : incorrect proof of work");
    
    // Check timestamp against prev
    if (block.getBlockTime() <= pindexPrev->GetMedianTimePast())
        return error("AcceptBlock() : block's timestamp is too early");
    
    // Check that all transactions are finalized
    for(int i = 0; i < block.getNumTransactions(); ++i)
        if(!isFinal(block.getTransaction(i), height, block.getBlockTime()))
        return error("AcceptBlock() : contains a non-final transaction");
    
    // Check that the block chain matches the known block chain up to a checkpoint
    if(!_chain.checkPoints(height, hash))
        return error("AcceptBlock() : rejected by checkpoint lockin at %d", height);

    // Write block to history file
    if (!_blockFile.checkDiskSpace(::GetSerializeSize(block, SER_DISK)))
        return error("AcceptBlock() : out of disk space");
    unsigned int nFile = -1;
    unsigned int nBlockPos = 0;
    bool commit = (!isInitialBlockDownload() || (getBestHeight()+1) % 500 == 0);
    if (!_blockFile.writeToDisk(_chain, block, nFile, nBlockPos, commit))
        return error("AcceptBlock() : WriteToDisk failed");
    if (!addToBlockIndex(block, nFile, nBlockPos))
        return error("AcceptBlock() : AddToBlockIndex failed");

    _acceptBlockTimer += GetTimeMicros()-t0;
    return true;
}

bool BlockChain::CheckForMemoryPool(const Transaction& tx, Transaction*& ptxOld, bool fCheckInputs, bool* pfMissingInputs) const {
    if (pfMissingInputs)
        *pfMissingInputs = false;
    
    if (!tx.checkTransaction())
        return error("AcceptToMemoryPool() : CheckTransaction failed");
    
    // Coinbase is only valid in a block, not as a loose transaction
    if (tx.isCoinBase())
        return error("AcceptToMemoryPool() : coinbase as individual tx");
    
    // To help v0.1.5 clients who would see it as a negative number
    if ((int64)tx.lockTime() > INT_MAX)
        return error("AcceptToMemoryPool() : not accepting nLockTime beyond 2038 yet");
    
    // Safety limits
    unsigned int nSize = ::GetSerializeSize(tx, SER_NETWORK);
    // Checking ECDSA signatures is a CPU bottleneck, so to avoid denial-of-service
    // attacks disallow transactions with more than one SigOp per 34 bytes.
    // 34 bytes because a TxOut is:
    //   20-byte address + 8 byte bitcoin amount + 5 bytes of ops + 1 byte script length
    if (tx.getSigOpCount() > nSize / 34 || nSize < 100)
        return error("AcceptToMemoryPool() : nonstandard transaction");
    
    // Rather not work on nonstandard transactions (unless -testnet)
    if (!_chain.isStandard(tx))
        return error("AcceptToMemoryPool() : nonstandard transaction type");
    
    // Do we already have it?
    uint256 hash = tx.getHash();
    if (_transactionIndex.count(hash))
        return false;
    if (fCheckInputs)
        if (haveTx(hash, true))
            return false;
    
    // Check for conflicts with in-memory transactions
    //    Transaction* ptxOld = NULL;
    for (int i = 0; i < tx.getNumInputs(); i++) {
        Coin outpoint = tx.getInput(i).prevout();
        TransactionConnections::const_iterator cnx = _transactionConnections.find(outpoint);
        if (cnx != _transactionConnections.end()) {
            // Disable replacement feature for now
            return false;
            
            // Allow replacing with a newer version of the same transaction
            if (i != 0)
                return false;
            ptxOld = (Transaction*)cnx->second.ptx;
            if (isFinal(*ptxOld))
                return false;
            if (!tx.isNewerThan(*ptxOld))
                return false;
            for (int i = 0; i < tx.getNumInputs(); i++) {
                Coin outpoint = tx.getInput(i).prevout();
                TransactionConnections::const_iterator cnx = _transactionConnections.find(outpoint);
                if (cnx == _transactionConnections.end() || cnx->second.ptx != ptxOld)
                    return false;
            }
            break;
        }
    }
    
    if (fCheckInputs) {
        // Check against previous transactions
        map<uint256, TxIndex> mapUnused;
        int64 nFees = 0;
        if (!connectInputs(tx, mapUnused, DiskTxPos(1,1,1), getBestIndex(), nFees, false, false)) {
            if (pfMissingInputs)
                *pfMissingInputs = true;
            return error("AcceptToMemoryPool() : ConnectInputs failed %s", hash.toString().substr(0,10).c_str());
        }
        
        // Don't accept it if it can't get into a block
        if (nFees < tx.getMinFee(1000, true, true))
            return error("AcceptToMemoryPool() : not enough fees");
        
        // Continuously rate-limit free transactions
        // This mitigates 'penny-flooding' -- sending thousands of free transactions just to
        // be annoying or make other's transactions take longer to confirm.
        if (nFees < MIN_RELAY_TX_FEE) {
            static CCriticalSection cs;
            static double dFreeCount;
            static int64 nLastTime;
            int64 nNow = GetTime();
            
            CRITICAL_BLOCK(cs) {
                // Use an exponentially decaying ~10-minute window:
                dFreeCount *= pow(1.0 - 1.0/600.0, (double)(nNow - nLastTime));
                nLastTime = nNow;
                // -limitfreerelay unit is thousand-bytes-per-minute
                // At default rate it would take over a month to fill 1GB
                if (dFreeCount > 15*10*1000 /*&& !IsFromMe(*this)*/)
                    //                    if (dFreeCount > GetArg("-limitfreerelay", 15)*10*1000 /*&& !IsFromMe(*this)*/)
                    return error("AcceptToMemoryPool() : free transaction rejected by rate limiter");
                if (fDebug)
                    printf("Rate limit dFreeCount: %g => %g\n", dFreeCount, dFreeCount+nSize);
                dFreeCount += nSize;
            }
        }
    }
    
    return true;
}

// This AcceptToMemoryPool has no locks as it is only called from the block acceptor that does the locking.
bool BlockChain::AcceptToMemoryPool(const Transaction& tx, bool fCheckInputs) {
    Transaction* ptxOld = NULL;
    bool fMissingInputs;
    if(CheckForMemoryPool(tx, ptxOld, fCheckInputs, &fMissingInputs)) {
        
        // Store transaction in memory
        if (ptxOld) {
            printf("AcceptToMemoryPool() : replacing tx %s with new version\n", ptxOld->getHash().toString().c_str());
            RemoveFromMemoryPool(*ptxOld);
        }
        AddToMemoryPoolUnchecked(tx);
        
        ///// are we sure this is ok when loading transactions or restoring block txes
        // If updated, erase old tx from wallet
        //    if (ptxOld)
        //        EraseFromWallets(ptxOld->GetHash());
        
        printf("AcceptToMemoryPool(): accepted %s\n", tx.getHash().toString().substr(0,10).c_str());
        return true;
    }
    else
       return false;
}

// This AcceptToMemoryPool has locks as it is only called from the Transaction acceptor.
bool BlockChain::AcceptToMemoryPool(const Transaction& tx, bool fCheckInputs, bool* pfMissingInputs) {
    Transaction* ptxOld = NULL;
    if(CheckForMemoryPool(tx, ptxOld, fCheckInputs, pfMissingInputs)) {
        // The pool will be updated, so lock the chain and pool mutex.
        boost::unique_lock< boost::shared_mutex > lock(_chain_and_pool_access);
        // Store transaction in memory
        if (ptxOld) {
            printf("AcceptToMemoryPool() : replacing tx %s with new version\n", ptxOld->getHash().toString().c_str());
            RemoveFromMemoryPool(*ptxOld);
        }
        AddToMemoryPoolUnchecked(tx);
        
        ///// are we sure this is ok when loading transactions or restoring block txes
        // If updated, erase old tx from wallet
        //    if (ptxOld)
        //        EraseFromWallets(ptxOld->GetHash());
        
        printf("AcceptToMemoryPool(): accepted %s\n", tx.getHash().toString().substr(0,10).c_str());
        return true;
    }
    else
        return false;
}

bool BlockChain::AddToMemoryPoolUnchecked(const Transaction& tx)
{
    // Add to memory pool without checking anything.  Don't call this directly,
    // call AcceptToMemoryPool to properly check the transaction first.
   
    uint256 hash = tx.getHash();
    _transactionIndex[hash] = tx;
    for (int i = 0; i < tx.getNumInputs(); i++)
        _transactionConnections[tx.getInput(i).prevout()] = CoinRef(&_transactionIndex[hash], i);
    _transactionsUpdated++;
    
    return true;
}


bool BlockChain::RemoveFromMemoryPool(const Transaction& tx)
{
    // Remove transaction from memory pool
    
    BOOST_FOREACH(const Input& txin, tx.getInputs())
    _transactionConnections.erase(txin.prevout());
    _transactionIndex.erase(tx.getHash());
    _transactionsUpdated++;
    
    return true;
}

void BlockChain::getTransaction(const uint256& hash, Transaction& tx) const
{
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);

    tx.setNull();
    if(!readDiskTx(hash, tx)) {
        TransactionIndex::const_iterator hashtx = _transactionIndex.find(hash);
        if(hashtx != _transactionIndex.end())
            tx = hashtx->second;
    }
}

void BlockChain::getTransaction(const uint256& hash, Transaction& tx, int64& height, int64& time) const
{
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);

    tx.setNull();
    height = -1;
    time = -1;
    if(!readDiskTx(hash, tx, height, time)) {
        TransactionIndex::const_iterator hashtx = _transactionIndex.find(hash);
        if(hashtx != _transactionIndex.end())
            tx = hashtx->second;
    }
}

bool BlockChain::isSpent(Coin coin) const {
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);

    TxIndex index;
    if(ReadTxIndex(coin.hash, index))
        return !index.getSpent(coin.index).isNull();
    else
        return false;
}

int BlockChain::getNumSpent(uint256 hash) const {
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);

    TxIndex index;
    if(ReadTxIndex(hash, index))
        return index.getNumSpents();
    else
        return 0;
}

uint256 BlockChain::spentIn(Coin coin) const {
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);

    TxIndex index;
    if(ReadTxIndex(coin.hash, index)) {
        const DiskTxPos& diskpos = index.getSpent(coin.index);
        Transaction tx;
        _blockFile.readFromDisk(tx, diskpos);
        return tx.getHash();
    }
    else
        return 0;    
}

int64 BlockChain::value(Coin coin) const {
    // get the transaction, then get the Output of the prevout, then get the value
    Transaction tx;
    getTransaction(coin.hash, tx);
    if (!tx.isNull() && coin.index < tx.getNumOutputs()) {
        Output out = tx.getOutput(coin.index);
        return out.value();
    }
    return 0;
}