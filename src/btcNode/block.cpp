
#include "btcNode/Block.h"
#include "btcNode/BlockIndex.h"
#include "btcNode/BlockChain.h"

#include "btcNode/main.h"
#include "btcNode/db.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

using namespace std;
using namespace boost;

int64 static GetBlockValue(int nHeight, int64 nFees)
{
    int64 nSubsidy = 50 * COIN;

    // Subsidy is cut in half every 4 years
    nSubsidy >>= (nHeight / 210000);

    return nSubsidy + nFees;
}

unsigned int static GetNextWorkRequired(const CBlockIndex* pindexLast)
{
    const int64 nTargetTimespan = 14 * 24 * 60 * 60; // two weeks
    const int64 nTargetSpacing = 10 * 60;
    const int64 nInterval = nTargetTimespan / nTargetSpacing;

    // Genesis block
    if (pindexLast == NULL)
        return bnProofOfWorkLimit.GetCompact();

    // Only change once per interval
    if ((pindexLast->nHeight+1) % nInterval != 0)
        return pindexLast->nBits;

    // Go back by what we want to be 14 days worth of blocks
    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < nInterval-1; i++)
        pindexFirst = pindexFirst->pprev;
    assert(pindexFirst);

    // Limit adjustment step
    int64 nActualTimespan = pindexLast->GetBlockTime() - pindexFirst->GetBlockTime();
    printf("  nActualTimespan = %"PRI64d"  before bounds\n", nActualTimespan);
    if (nActualTimespan < nTargetTimespan/4)
        nActualTimespan = nTargetTimespan/4;
    if (nActualTimespan > nTargetTimespan*4)
        nActualTimespan = nTargetTimespan*4;

    // Retarget
    CBigNum bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;

    if (bnNew > bnProofOfWorkLimit)
        bnNew = bnProofOfWorkLimit;

    /// debug print
    printf("GetNextWorkRequired RETARGET\n");
    printf("nTargetTimespan = %"PRI64d"    nActualTimespan = %"PRI64d"\n", nTargetTimespan, nActualTimespan);
    printf("Before: %08x  %s\n", pindexLast->nBits, CBigNum().SetCompact(pindexLast->nBits).getuint256().toString().c_str());
    printf("After:  %08x  %s\n", bnNew.GetCompact(), bnNew.getuint256().toString().c_str());

    return bnNew.GetCompact();
}

void static InvalidChainFound(CBlockIndex* pindexNew)
{
    if (pindexNew->bnChainWork > bnBestInvalidWork)
    {
        bnBestInvalidWork = pindexNew->bnChainWork;
        CTxDB().WriteBestInvalidWork(bnBestInvalidWork);
        MainFrameRepaint();
    }
    printf("InvalidChainFound: invalid block=%s  height=%d  work=%s\n", pindexNew->GetBlockHash().toString().substr(0,20).c_str(), pindexNew->nHeight, pindexNew->bnChainWork.toString().c_str());
    printf("InvalidChainFound:  current best=%s  height=%d  work=%s\n", hashBestChain.toString().substr(0,20).c_str(), nBestHeight, bnBestChainWork.toString().c_str());
    if (pindexBest && bnBestInvalidWork > bnBestChainWork + pindexBest->GetBlockWork() * 6)
        printf("InvalidChainFound: WARNING: Displayed transactions may not be correct!  You may need to upgrade, or other nodes may need to upgrade.\n");
}

uint256 Block::getHash() const
{
    return Hash(BEGIN(_version), END(_nonce));
}

int Block::GetSigOpCount() const
{
    int n = 0;
    BOOST_FOREACH(const CTransaction& tx, _transactions)
    n += tx.GetSigOpCount();
    return n;
}

uint256 Block::buildMerkleTree() const
{
    _merkleTree.clear();
    BOOST_FOREACH(const CTransaction& tx, _transactions)
    _merkleTree.push_back(tx.GetHash());
    int j = 0;
    for (int size = _transactions.size(); size > 1; size = (size + 1) / 2) {
        for (int i = 0; i < size; i += 2) {
            int i2 = std::min(i+1, size-1);
            _merkleTree.push_back(Hash(BEGIN(_merkleTree[j+i]),  END(_merkleTree[j+i]),
                                       BEGIN(_merkleTree[j+i2]), END(_merkleTree[j+i2])));
        }
        j += size;
    }
    return (_merkleTree.empty() ? 0 : _merkleTree.back());
}

std::vector<uint256> Block::getMerkleBranch(int index) const
{
    if (_merkleTree.empty())
        buildMerkleTree();
    std::vector<uint256> merkleBranch;
    int j = 0;
    for (int size = _transactions.size(); size > 1; size = (size + 1) / 2) {
        int i = std::min(index^1, size-1);
        merkleBranch.push_back(_merkleTree[j+i]);
        index >>= 1;
        j += size;
    }
    return merkleBranch;
}

uint256 Block::checkMerkleBranch(uint256 hash, const std::vector<uint256>& merkleBranch, int index)
{
    if (index == -1)
        return 0;
    BOOST_FOREACH(const uint256& otherside, merkleBranch)
    {
    if (index & 1)
        hash = Hash(BEGIN(otherside), END(otherside), BEGIN(hash), END(hash));
    else
        hash = Hash(BEGIN(hash), END(hash), BEGIN(otherside), END(otherside));
    index >>= 1;
    }
    return hash;
}

void Block::print() const
{
    printf("Block(hash=%s, ver=%d, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%d)\n",
           getHash().toString().substr(0,20).c_str(),
           _version,
           _prevBlock.toString().substr(0,20).c_str(),
           _merkleRoot.toString().substr(0,10).c_str(),
           _time, _bits, _nonce,
           _transactions.size());
    for (int i = 0; i < _transactions.size(); i++)
        {
        printf("  ");
        _transactions[i].print();
        }
    printf("  vMerkleTree: ");
    for (int i = 0; i < _merkleTree.size(); i++)
        printf("%s ", _merkleTree[i].toString().substr(0,10).c_str());
    printf("\n");
}




bool Block::disconnectBlock(CTxDB& txdb, CBlockIndex* pindex)
{
    // Disconnect in reverse order
    for (int i = _transactions.size()-1; i >= 0; i--)
        if (!_transactions[i].DisconnectInputs(txdb))
            return false;

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    if (pindex->pprev) {
        CDiskBlockIndex blockindexPrev(pindex->pprev);
        blockindexPrev.hashNext = 0;
        if (!txdb.WriteBlockIndex(blockindexPrev))
            return error("DisconnectBlock() : WriteBlockIndex failed");
    }

    return true;
}

bool Block::connectBlock(CTxDB& txdb, CBlockIndex* pindex)
{
    // Check it again in case a previous version let a bad block in
    if (!checkBlock())
        return false;

    //// issue here: it doesn't know the version
    unsigned int nTxPos = pindex->nBlockPos + ::GetSerializeSize(Block(), SER_DISK) - 1 + GetSizeOfCompactSize(_transactions.size());

    map<uint256, TxIndex> queuedChanges;
    int64 fees = 0;
    BOOST_FOREACH(CTransaction& tx, _transactions) {
        DiskTxPos posThisTx(pindex->nFile, pindex->nBlockPos, nTxPos);
        nTxPos += ::GetSerializeSize(tx, SER_DISK);

        if (!tx.ConnectInputs(txdb, queuedChanges, posThisTx, pindex, fees, true, false))
            return false;
    }
    // Write queued txindex changes
    for (map<uint256, TxIndex>::iterator mi = queuedChanges.begin(); mi != queuedChanges.end(); ++mi) {
        if (!txdb.UpdateTxIndex((*mi).first, (*mi).second))
            return error("ConnectBlock() : UpdateTxIndex failed");
    }

    if (_transactions[0].GetValueOut() > GetBlockValue(pindex->nHeight, fees))
        return false;

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    if (pindex->pprev)
    {
        CDiskBlockIndex blockindexPrev(pindex->pprev);
        blockindexPrev.hashNext = pindex->GetBlockHash();
        if (!txdb.WriteBlockIndex(blockindexPrev))
            return error("ConnectBlock() : WriteBlockIndex failed");
    }

    // Watch for transactions paying to me
//    BOOST_FOREACH(CTransaction& tx, vtx)
//        SyncWithWallets(tx, this, true);

    return true;
}

bool static Reorganize(CTxDB& txdb, CBlockIndex* pindexNew)
{
    printf("REORGANIZE\n");
    
    // Find the fork
    CBlockIndex* pfork = pindexBest;
    CBlockIndex* plonger = pindexNew;
    while (pfork != plonger)
        {
        while (plonger->nHeight > pfork->nHeight)
            if (!(plonger = plonger->pprev))
                return error("Reorganize() : plonger->pprev is null");
        if (pfork == plonger)
            break;
        if (!(pfork = pfork->pprev))
            return error("Reorganize() : pfork->pprev is null");
        }
    
    // List of what to disconnect
    vector<CBlockIndex*> vDisconnect;
    for (CBlockIndex* pindex = pindexBest; pindex != pfork; pindex = pindex->pprev)
        vDisconnect.push_back(pindex);
    
    // List of what to connect
    vector<CBlockIndex*> vConnect;
    for (CBlockIndex* pindex = pindexNew; pindex != pfork; pindex = pindex->pprev)
        vConnect.push_back(pindex);
    reverse(vConnect.begin(), vConnect.end());
    
    // Disconnect shorter branch
    vector<CTransaction> vResurrect;
    BOOST_FOREACH(CBlockIndex* pindex, vDisconnect)
    {
    Block block;
    if (!__blockFile.readFromDisk(block, pindex))
        return error("Reorganize() : ReadFromDisk for disconnect failed");
    if (!block.disconnectBlock(txdb, pindex))
        return error("Reorganize() : DisconnectBlock failed");
    
    // Queue memory transactions to resurrect
    BOOST_FOREACH(const CTransaction& tx, block.getTransactions())
    if (!tx.IsCoinBase())
        vResurrect.push_back(tx);
    }
    
    // Connect longer branch
    vector<CTransaction> vDelete;
    for (int i = 0; i < vConnect.size(); i++)
        {
        CBlockIndex* pindex = vConnect[i];
        Block block;
        if (!__blockFile.readFromDisk(block,pindex))
            return error("Reorganize() : ReadFromDisk for connect failed");
        if (!block.connectBlock(txdb, pindex))
            {
            // Invalid block
            txdb.TxnAbort();
            return error("Reorganize() : ConnectBlock failed");
            }
        
        // Queue memory transactions to delete
        BOOST_FOREACH(const CTransaction& tx, block.getTransactions())
        vDelete.push_back(tx);
        }
    if (!txdb.WriteHashBestChain(pindexNew->GetBlockHash()))
        return error("Reorganize() : WriteHashBestChain failed");
    
    // Make sure it's successfully written to disk before changing memory structure
    if (!txdb.TxnCommit())
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
    BOOST_FOREACH(CTransaction& tx, vResurrect)
    tx.AcceptToMemoryPool(txdb, false);
    
    // Delete redundant memory transactions that are in the connected branch
    BOOST_FOREACH(CTransaction& tx, vDelete)
    tx.RemoveFromMemoryPool();
    
    return true;
}

bool Block::setBestChain(CTxDB& txdb, CBlockIndex* pindexNew)
{
    uint256 hash = getHash();

    txdb.TxnBegin();
    if (pindexGenesisBlock == NULL && hash == hashGenesisBlock) {
        txdb.WriteHashBestChain(hash);
        if (!txdb.TxnCommit())
            return error("SetBestChain() : TxnCommit failed");
        pindexGenesisBlock = pindexNew;
    }
    else if (_prevBlock == hashBestChain) {
        // Adding to current best branch
        if (!connectBlock(txdb, pindexNew) || !txdb.WriteHashBestChain(hash))
        {
            txdb.TxnAbort();
            InvalidChainFound(pindexNew);
            return error("SetBestChain() : ConnectBlock failed");
        }
        if (!txdb.TxnCommit())
            return error("SetBestChain() : TxnCommit failed");

        // Add to current best branch
        pindexNew->pprev->pnext = pindexNew;

        // Delete redundant memory transactions
        BOOST_FOREACH(CTransaction& tx, _transactions)
            tx.RemoveFromMemoryPool();
    }
    else
    {
        // New best branch
        if (!Reorganize(txdb, pindexNew))
        {
            txdb.TxnAbort();
            InvalidChainFound(pindexNew);
            return error("SetBestChain() : Reorganize failed");
        }
    }
/*
    // Update best block in wallet (so we can detect restored wallets)
    if (!IsInitialBlockDownload())
    {
        const CBlockLocator locator(pindexNew);
        ::SetBestChain(locator);
    }
*/
    // New best block
    hashBestChain = hash;
    pindexBest = pindexNew;
    nBestHeight = pindexBest->nHeight;
    bnBestChainWork = pindexNew->bnChainWork;
    nTimeBestReceived = GetTime();
    nTransactionsUpdated++;
    printf("SetBestChain: new best=%s  height=%d  work=%s\n", hashBestChain.toString().substr(0,20).c_str(), nBestHeight, bnBestChainWork.toString().c_str());

    return true;
}


bool Block::addToBlockIndex(unsigned int nFile, unsigned int nBlockPos)
{
    // Check for duplicate
    uint256 hash = getHash();
    if (mapBlockIndex.count(hash))
        return error("AddToBlockIndex() : %s already exists", hash.toString().substr(0,20).c_str());

    // Construct new block index object
    CBlockIndex* pindexNew = new CBlockIndex(nFile, nBlockPos, *this);
    if (!pindexNew)
        return error("AddToBlockIndex() : new CBlockIndex failed");
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);
    map<uint256, CBlockIndex*>::iterator miPrev = mapBlockIndex.find(_prevBlock);
    if (miPrev != mapBlockIndex.end())
    {
        pindexNew->pprev = (*miPrev).second;
        pindexNew->nHeight = pindexNew->pprev->nHeight + 1;
    }
    pindexNew->bnChainWork = (pindexNew->pprev ? pindexNew->pprev->bnChainWork : 0) + pindexNew->GetBlockWork();

    CTxDB txdb;
    txdb.TxnBegin();
    txdb.WriteBlockIndex(CDiskBlockIndex(pindexNew));
    if (!txdb.TxnCommit())
        return false;

    // New best
    if (pindexNew->bnChainWork > bnBestChainWork)
        if (!setBestChain(txdb, pindexNew))
            return false;

    txdb.Close();

    if (pindexNew == pindexBest)
    {
        // Notify UI to display prev block's coinbase if it was ours
        static uint256 hashPrevBestCoinBase;
//        UpdatedTransaction(hashPrevBestCoinBase);
        hashPrevBestCoinBase = _transactions[0].GetHash();
    }

    MainFrameRepaint();
    return true;
}

bool Block::checkBlock() const
{
    // These are checks that are independent of context
    // that can be verified before saving an orphan block.

    // Size limits
    if (_transactions.empty() || _transactions.size() > MAX_BLOCK_SIZE || ::GetSerializeSize(*this, SER_NETWORK) > MAX_BLOCK_SIZE)
        return error("CheckBlock() : size limits failed");

    // Check proof of work matches claimed amount
    if (!CheckProofOfWork(getHash(), _bits))
        return error("CheckBlock() : proof of work failed");

    // Check timestamp
    if (getBlockTime() > GetAdjustedTime() + 2 * 60 * 60)
        return error("CheckBlock() : block timestamp too far in the future");

    // First transaction must be coinbase, the rest must not be
    if (_transactions.empty() || !_transactions[0].IsCoinBase())
        return error("CheckBlock() : first tx is not coinbase");
    for (int i = 1; i < _transactions.size(); i++)
        if (_transactions[i].IsCoinBase())
            return error("CheckBlock() : more than one coinbase");

    // Check transactions
    BOOST_FOREACH(const CTransaction& tx, _transactions)
        if (!tx.CheckTransaction())
            return error("CheckBlock() : CheckTransaction failed");

    // Check that it's not full of nonstandard transactions
    if (GetSigOpCount() > MAX_BLOCK_SIGOPS)
        return error("CheckBlock() : too many nonstandard transactions");

    // Check merkleroot
    if (_merkleRoot != buildMerkleTree())
        return error("CheckBlock() : hashMerkleRoot mismatch");

    return true;
}

bool Block::acceptBlock()
{
    // Check for duplicate
    uint256 hash = getHash();
    if (mapBlockIndex.count(hash))
        return error("AcceptBlock() : block already in mapBlockIndex");

    // Get prev block index
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(_prevBlock);
    if (mi == mapBlockIndex.end())
        return error("AcceptBlock() : prev block not found");
    CBlockIndex* pindexPrev = (*mi).second;
    int height = pindexPrev->nHeight+1;

    // Check proof of work
    if (_bits != GetNextWorkRequired(pindexPrev))
        return error("AcceptBlock() : incorrect proof of work");

    // Check timestamp against prev
    if (getBlockTime() <= pindexPrev->GetMedianTimePast())
        return error("AcceptBlock() : block's timestamp is too early");

    // Check that all transactions are finalized
    BOOST_FOREACH(const CTransaction& tx, _transactions)
        if (!tx.IsFinal(height, getBlockTime()))
            return error("AcceptBlock() : contains a non-final transaction");

    // Check that the block chain matches the known block chain up to a checkpoint
    if (!fTestNet)
        if ((height ==  11111 && hash != uint256("0x0000000069e244f73d78e8fd29ba2fd2ed618bd6fa2ee92559f542fdb26e7c1d")) ||
            (height ==  33333 && hash != uint256("0x000000002dd5588a74784eaa7ab0507a18ad16a236e7b1ce69f00d7ddfb5d0a6")) ||
            (height ==  68555 && hash != uint256("0x00000000001e1b4903550a0b96e9a9405c8a95f387162e4944e8d9fbe501cd6a")) ||
            (height ==  70567 && hash != uint256("0x00000000006a49b14bcf27462068f1264c961f11fa2e0eddd2be0791e1d4124a")) ||
            (height ==  74000 && hash != uint256("0x0000000000573993a3c9e41ce34471c079dcf5f52a0e824a81e7f953b8661a20")) ||
            (height == 105000 && hash != uint256("0x00000000000291ce28027faea320c8d2b054b2e0fe44a773f3eefb151d6bdc97")) ||
            (height == 118000 && hash != uint256("0x000000000000774a7f8a7a12dc906ddb9e17e75d684f15e00f8767f9e8f36553")) ||
            (height == 134444 && hash != uint256("0x00000000000005b12ffd4cd315cd34ffd4a594f430ac814c91184a0d42d2b0fe")) ||
            (height == 140700 && hash != uint256("0x000000000000033b512028abb90e1626d8b346fd0ed598ac0a3c371138dce2bd")))
            return error("AcceptBlock() : rejected by checkpoint lockin at %d", height);

    // Write block to history file
    if (!BlockFile::checkDiskSpace(::GetSerializeSize(*this, SER_DISK)))
        return error("AcceptBlock() : out of disk space");
    unsigned int nFile = -1;
    unsigned int nBlockPos = 0;
    if (!__blockFile.writeToDisk(*this, nFile, nBlockPos))
        return error("AcceptBlock() : WriteToDisk failed");
    if (!addToBlockIndex(nFile, nBlockPos))
        return error("AcceptBlock() : AddToBlockIndex failed");

    // Relay inventory, but don't relay old inventory during initial block download
    if (hashBestChain == hash)
        CRITICAL_BLOCK(cs_vNodes)
            BOOST_FOREACH(CNode* pnode, vNodes)
                if (nBestHeight > (pnode->nStartingHeight != -1 ? pnode->nStartingHeight - 2000 : 140700))
                    pnode->PushInventory(Inventory(MSG_BLOCK, hash));

    return true;
}
