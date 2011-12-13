// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "btcNode/db.h"
#include "btcNode/net.h"
#include "btcNode/Block.h"
#include "btcNode/BlockFile.h"
#include "btcNode/BlockChain.h"

//#include "init.h"
//#include "cryptopp/sha.h"
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

using namespace std;
using namespace boost;

//
// Global state
//

CCriticalSection cs_main;

map<uint256, CTransaction> mapTransactions;
map<uint160, set<pair<uint256, unsigned int> > > mapCredits;
map<uint160, set<pair<uint256, unsigned int> > > mapDebits;
CCriticalSection cs_mapTransactions;
unsigned int nTransactionsUpdated = 0;
map<COutPoint, CInPoint> mapNextTx;

CBigNum bnProofOfWorkLimit(~uint256(0) >> 32);

CBlockIndex* pindexGenesisBlock = NULL;

//BlockFile __blockFile;
BlockChain* __blockChain;

double dHashesPerSec;
int64 nHPSTimerStart;

// Settings
int fGenerateBitcoins = false;
int64 nTransactionFee = 0;
int fLimitProcessors = false;
int nLimitProcessors = 1;
int fMinimizeToTray = true;
int fMinimizeOnClose = true;
#if USE_UPNP
int fUseUPnP = true;
#else
int fUseUPnP = false;
#endif

void MainFrameRepaint()
{
    CMain::instance().repaint();
}

void Shutdown(void* ptr)
{
    CMain::instance().shutdown();
}




bool CTransaction::CheckTransaction() const
{
    // Basic checks that don't depend on any context
    if (vin.empty())
        return error("CTransaction::CheckTransaction() : vin empty");
    if (vout.empty())
        return error("CTransaction::CheckTransaction() : vout empty");
    // Size limits
    if (::GetSerializeSize(*this, SER_NETWORK) > MAX_BLOCK_SIZE)
        return error("CTransaction::CheckTransaction() : size limits failed");

    // Check for negative or overflow output values
    int64 nValueOut = 0;
    BOOST_FOREACH(const CTxOut& txout, vout)
    {
        if (txout.nValue < 0)
            return error("CTransaction::CheckTransaction() : txout.nValue negative");
        if (txout.nValue > MAX_MONEY)
            return error("CTransaction::CheckTransaction() : txout.nValue too high");
        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut))
            return error("CTransaction::CheckTransaction() : txout total out of range");
    }

    // Check for duplicate inputs
    set<COutPoint> vInOutPoints;
    BOOST_FOREACH(const CTxIn& txin, vin)
    {
        if (vInOutPoints.count(txin.prevout))
            return false;
        vInOutPoints.insert(txin.prevout);
    }

    if (IsCoinBase())
    {
        if (vin[0].scriptSig.size() < 2 || vin[0].scriptSig.size() > 100)
            return error("CTransaction::CheckTransaction() : coinbase script size");
    }
    else
    {
        BOOST_FOREACH(const CTxIn& txin, vin)
            if (txin.prevout.IsNull())
                return error("CTransaction::CheckTransaction() : prevout is null");
    }

    return true;
}

bool CTransaction::AcceptToMemoryPool(bool fCheckInputs, bool* pfMissingInputs)
{
    if (pfMissingInputs)
        *pfMissingInputs = false;

    if (!CheckTransaction())
        return error("AcceptToMemoryPool() : CheckTransaction failed");

    // Coinbase is only valid in a block, not as a loose transaction
    if (IsCoinBase())
        return error("AcceptToMemoryPool() : coinbase as individual tx");

    // To help v0.1.5 clients who would see it as a negative number
    if ((int64)nLockTime > INT_MAX)
        return error("AcceptToMemoryPool() : not accepting nLockTime beyond 2038 yet");

    // Safety limits
    unsigned int nSize = ::GetSerializeSize(*this, SER_NETWORK);
    // Checking ECDSA signatures is a CPU bottleneck, so to avoid denial-of-service
    // attacks disallow transactions with more than one SigOp per 34 bytes.
    // 34 bytes because a TxOut is:
    //   20-byte address + 8 byte bitcoin amount + 5 bytes of ops + 1 byte script length
    if (GetSigOpCount() > nSize / 34 || nSize < 100)
        return error("AcceptToMemoryPool() : nonstandard transaction");

    // Rather not work on nonstandard transactions (unless -testnet)
    if (!fTestNet && !IsStandard())
        return error("AcceptToMemoryPool() : nonstandard transaction type");

    // Do we already have it?
    uint256 hash = GetHash();
    CRITICAL_BLOCK(cs_mapTransactions)
        if (mapTransactions.count(hash))
            return false;
    if (fCheckInputs)
        if (__blockChain->containsTx(hash))
            return false;

    // Check for conflicts with in-memory transactions
    CTransaction* ptxOld = NULL;
    for (int i = 0; i < vin.size(); i++)
    {
        COutPoint outpoint = vin[i].prevout;
        if (mapNextTx.count(outpoint))
        {
            // Disable replacement feature for now
            return false;

            // Allow replacing with a newer version of the same transaction
            if (i != 0)
                return false;
            ptxOld = (CTransaction*)mapNextTx[outpoint].ptx;
            if (__blockChain->isFinal(*ptxOld))
                return false;
            if (!IsNewerThan(*ptxOld))
                return false;
            for (int i = 0; i < vin.size(); i++)
            {
                COutPoint outpoint = vin[i].prevout;
                if (!mapNextTx.count(outpoint) || mapNextTx[outpoint].ptx != ptxOld)
                    return false;
            }
            break;
        }
    }

    if (fCheckInputs)
    {
        // Check against previous transactions
        map<uint256, TxIndex> mapUnused;
        int64 nFees = 0;
        if (!__blockChain->connectInputs(*this, mapUnused, DiskTxPos(1,1,1), __blockChain->getBestIndex(), nFees, false, false))
        {
            if (pfMissingInputs)
                *pfMissingInputs = true;
            return error("AcceptToMemoryPool() : ConnectInputs failed %s", hash.toString().substr(0,10).c_str());
        }

        // Don't accept it if it can't get into a block
        if (nFees < GetMinFee(1000, true, true))
            return error("AcceptToMemoryPool() : not enough fees");

        // Continuously rate-limit free transactions
        // This mitigates 'penny-flooding' -- sending thousands of free transactions just to
        // be annoying or make other's transactions take longer to confirm.
        if (nFees < MIN_RELAY_TX_FEE)
        {
            static CCriticalSection cs;
            static double dFreeCount;
            static int64 nLastTime;
            int64 nNow = GetTime();

            CRITICAL_BLOCK(cs)
            {
                // Use an exponentially decaying ~10-minute window:
                dFreeCount *= pow(1.0 - 1.0/600.0, (double)(nNow - nLastTime));
                nLastTime = nNow;
                // -limitfreerelay unit is thousand-bytes-per-minute
                // At default rate it would take over a month to fill 1GB
                if (dFreeCount > GetArg("-limitfreerelay", 15)*10*1000 /*&& !IsFromMe(*this)*/)
                    return error("AcceptToMemoryPool() : free transaction rejected by rate limiter");
                if (fDebug)
                    printf("Rate limit dFreeCount: %g => %g\n", dFreeCount, dFreeCount+nSize);
                dFreeCount += nSize;
            }
        }
    }

    // Store transaction in memory
    CRITICAL_BLOCK(cs_mapTransactions)
    {
        if (ptxOld)
        {
            printf("AcceptToMemoryPool() : replacing tx %s with new version\n", ptxOld->GetHash().toString().c_str());
            ptxOld->RemoveFromMemoryPool();
        }
        AddToMemoryPoolUnchecked();
    }

    ///// are we sure this is ok when loading transactions or restoring block txes
    // If updated, erase old tx from wallet
//    if (ptxOld)
//        EraseFromWallets(ptxOld->GetHash());

    printf("AcceptToMemoryPool(): accepted %s\n", hash.toString().substr(0,10).c_str());
    return true;
}

bool CTransaction::AddToMemoryPoolUnchecked()
{
    // Add to memory pool without checking anything.  Don't call this directly,
    // call AcceptToMemoryPool to properly check the transaction first.
    
    uint256 hash = GetHash();
    
    typedef pair<uint160, unsigned int> AssetPair;
    vector<AssetPair> debits;
    vector<AssetPair> credits;
    
    // for each tx out in the newly added tx check for a pubkey or a pubkeyhash in the script
    for(unsigned int n = 0; n < vout.size(); n++)
        debits.push_back(AssetPair(vout[n].getAsset(), n));
    
    if(!IsCoinBase())
    {
        for(unsigned int n = 0; n < vin.size(); n++)
        {
            const CTxIn& txin = vin[n];
            CTransaction prevtx;
            if(!__blockChain->readDiskTx(txin.prevout.hash, prevtx))
                continue; // OK ???
            CTxOut txout = prevtx.vout[txin.prevout.n];        
            
            credits.push_back(AssetPair(txout.getAsset(), n));
        }
    }
    
    CRITICAL_BLOCK(cs_mapTransactions)
    {
        for(vector<AssetPair>::iterator assetpair = debits.begin(); assetpair != debits.end(); ++assetpair)
            mapDebits[assetpair->first].insert(pair<uint256, unsigned int>(hash, assetpair->second));
        
        for(vector<AssetPair>::iterator assetpair = credits.begin(); assetpair != credits.end(); ++assetpair)
            mapCredits[assetpair->first].insert(pair<uint256, unsigned int>(hash, assetpair->second));
        
        mapTransactions[hash] = *this;
        for (int i = 0; i < vin.size(); i++)
            mapNextTx[vin[i].prevout] = CInPoint(&mapTransactions[hash], i);
        nTransactionsUpdated++;
    }
    
    return true;
}


bool CTransaction::RemoveFromMemoryPool()
{
    // Remove transaction from memory pool
    
    uint256 hash = GetHash();
    
    typedef pair<uint160, unsigned int> AssetPair;
    vector<AssetPair> debits;
    vector<AssetPair> credits;
    
    // for each tx out in the newly added tx check for a pubkey or a pubkeyhash in the script
    for(unsigned int n = 0; n < vout.size(); n++)
        debits.push_back(AssetPair(vout[n].getAsset(), n));
    
    if(!IsCoinBase())
    {
        for(unsigned int n = 0; n < vin.size(); n++)
        {
            const CTxIn& txin = vin[n];
            CTransaction prevtx;
            if(!__blockChain->readDiskTx(txin.prevout.hash, prevtx))
                continue; // OK ???
            CTxOut txout = prevtx.vout[txin.prevout.n];        
            
            credits.push_back(AssetPair(txout.getAsset(), n));
        }
    }
    
    CRITICAL_BLOCK(cs_mapTransactions)
    {
        for(vector<AssetPair>::iterator assetpair = debits.begin(); assetpair != debits.end(); ++assetpair)
        {
            mapDebits[assetpair->first].erase(pair<uint256, unsigned int>(hash, assetpair->second));
            if(mapDebits[assetpair->first].size() == 0)
                mapDebits.erase(assetpair->first); 
        }
        
        for(vector<AssetPair>::iterator assetpair = credits.begin(); assetpair != credits.end(); ++assetpair)
        {
            mapCredits[assetpair->first].erase(pair<uint256, unsigned int>(hash, assetpair->second));
            if(mapCredits[assetpair->first].size() == 0)
                mapCredits.erase(assetpair->first); 
        }
        
        BOOST_FOREACH(const CTxIn& txin, vin)
            mapNextTx.erase(txin.prevout);
        mapTransactions.erase(GetHash());
        nTransactionsUpdated++;
    }
    return true;
}

bool CTransaction::ClientConnectInputs()
{
    if (IsCoinBase())
        return false;

    // Take over previous transactions' spent pointers
    CRITICAL_BLOCK(cs_mapTransactions)
    {
        int64 nValueIn = 0;
        for (int i = 0; i < vin.size(); i++)
        {
            // Get prev tx from single transactions in memory
            COutPoint prevout = vin[i].prevout;
            if (!mapTransactions.count(prevout.hash))
                return false;
            CTransaction& txPrev = mapTransactions[prevout.hash];

            if (prevout.n >= txPrev.vout.size())
                return false;

            // Verify signature
            if (!VerifySignature(txPrev, *this, i))
                return error("ConnectInputs() : VerifySignature failed");

            ///// this is redundant with the mapNextTx stuff, not sure which I want to get rid of
            ///// this has to go away now that posNext is gone
            // // Check for conflicts
            // if (!txPrev.vout[prevout.n].posNext.IsNull())
            //     return error("ConnectInputs() : prev tx already used");
            //
            // // Flag outpoints as used
            // txPrev.vout[prevout.n].posNext = posThisTx;

            nValueIn += txPrev.vout[prevout.n].nValue;

            if (!MoneyRange(txPrev.vout[prevout.n].nValue) || !MoneyRange(nValueIn))
                return error("ClientConnectInputs() : txin values out of range");
        }
        if (GetValueOut() > nValueIn)
            return false;
    }

    return true;
}

/*



//////////////////////////////////////////////////////////////////////////////
//
// BitcoinMiner
//

int static FormatHashBlocks(void* pbuffer, unsigned int len)
{
    unsigned char* pdata = (unsigned char*)pbuffer;
    unsigned int blocks = 1 + ((len + 8) / 64);
    unsigned char* pend = pdata + 64 * blocks;
    memset(pdata + len, 0, 64 * blocks - len);
    pdata[len] = 0x80;
    unsigned int bits = len * 8;
    pend[-1] = (bits >> 0) & 0xff;
    pend[-2] = (bits >> 8) & 0xff;
    pend[-3] = (bits >> 16) & 0xff;
    pend[-4] = (bits >> 24) & 0xff;
    return blocks;
}

using CryptoPP::ByteReverse;

static const unsigned int pSHA256InitState[8] =
{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

inline void SHA256Transform(void* pstate, void* pinput, const void* pinit)
{
    memcpy(pstate, pinit, 32);
    CryptoPP::SHA256::Transform((CryptoPP::word32*)pstate, (CryptoPP::word32*)pinput);
}

//
// ScanHash scans nonces looking for a hash with at least some zero bits.
// It operates on big endian data.  Caller does the byte reversing.
// All input buffers are 16-byte aligned.  nNonce is usually preserved
// between calls, but periodically or if nNonce is 0xffff0000 or above,
// the block is rebuilt and nNonce starts over at zero.
//
unsigned int static ScanHash_CryptoPP(char* pmidstate, char* pdata, char* phash1, char* phash, unsigned int& nHashesDone)
{
    unsigned int& nNonce = *(unsigned int*)(pdata + 12);
    for (;;)
    {
        // Crypto++ SHA-256
        // Hash pdata using pmidstate as the starting state into
        // preformatted buffer phash1, then hash phash1 into phash
        nNonce++;
        SHA256Transform(phash1, pdata, pmidstate);
        SHA256Transform(phash, phash1, pSHA256InitState);

        // Return the nonce if the hash has at least some zero bits,
        // caller will check if it has enough to reach the target
        if (((unsigned short*)phash)[14] == 0)
            return nNonce;

        // If nothing found after trying for a while, return -1
        if ((nNonce & 0xffff) == 0)
        {
            nHashesDone = 0xffff+1;
            return -1;
        }
    }
}

// Some explaining would be appreciated
class COrphan
{
public:
    CTransaction* ptx;
    set<uint256> setDependsOn;
    double dPriority;

    COrphan(CTransaction* ptxIn)
    {
        ptx = ptxIn;
        dPriority = 0;
    }

    void print() const
    {
        printf("COrphan(hash=%s, dPriority=%.1f)\n", ptx->GetHash().toString().substr(0,10).c_str(), dPriority);
        BOOST_FOREACH(uint256 hash, setDependsOn)
            printf("   setDependsOn %s\n", hash.toString().substr(0,10).c_str());
    }
};

Block* CreateNewBlock(CReserveKey& reservekey)
{
    CBlockIndex* pindexPrev = _bestIndex;

    // Create new block
    auto_ptr<Block> pblock(new Block());
    if (!pblock.get())
        return NULL;

    // Create coinbase tx
    CTransaction txNew;
    txNew.vin.resize(1);
    txNew.vin[0].prevout.SetNull();
    txNew.vout.resize(1);
    txNew.vout[0].scriptPubKey << reservekey.GetReservedKey() << OP_CHECKSIG;

    // Add our coinbase tx as first transaction
    pblock->vtx.push_back(txNew);

    // Collect memory pool transactions into the block
    int64 nFees = 0;
    CRITICAL_BLOCK(cs_main)
    CRITICAL_BLOCK(cs_mapTransactions)
    {
        CTxDB txdb("r");

        // Priority order to process transactions
        list<COrphan> vOrphan; // list memory doesn't move
        map<uint256, vector<COrphan*> > mapDependers;
        multimap<double, CTransaction*> mapPriority;
        for (map<uint256, CTransaction>::iterator mi = mapTransactions.begin(); mi != mapTransactions.end(); ++mi)
        {
            CTransaction& tx = (*mi).second;
            if (tx.IsCoinBase() || !__blockChain->isFinal(tx))
                continue;

            COrphan* porphan = NULL;
            double dPriority = 0;
            BOOST_FOREACH(const CTxIn& txin, tx.vin)
            {
                // Read prev transaction
                CTransaction txPrev;
                TxIndex txindex;
                if (!txPrev.ReadFromDisk(txdb, txin.prevout, txindex))
                {
                    // Has to wait for dependencies
                    if (!porphan)
                    {
                        // Use list for automatic deletion
                        vOrphan.push_back(COrphan(&tx));
                        porphan = &vOrphan.back();
                    }
                    mapDependers[txin.prevout.hash].push_back(porphan);
                    porphan->setDependsOn.insert(txin.prevout.hash);
                    continue;
                }
                int64 nValueIn = txPrev.vout[txin.prevout.n].nValue;

                // Read block header
                int nConf = txindex.GetDepthInMainChain();

                dPriority += (double)nValueIn * nConf;

                if (fDebug && GetBoolArg("-printpriority"))
                    printf("priority     nValueIn=%-12I64d nConf=%-5d dPriority=%-20.1f\n", nValueIn, nConf, dPriority);
            }

            // Priority is sum(valuein * age) / txsize
            dPriority /= ::GetSerializeSize(tx, SER_NETWORK);

            if (porphan)
                porphan->dPriority = dPriority;
            else
                mapPriority.insert(make_pair(-dPriority, &(*mi).second));

            if (fDebug && GetBoolArg("-printpriority"))
            {
                printf("priority %-20.1f %s\n%s", dPriority, tx.GetHash().toString().substr(0,10).c_str(), tx.toString().c_str());
                if (porphan)
                    porphan->print();
                printf("\n");
            }
        }

        // Collect transactions into block
        map<uint256, TxIndex> mapTestPool;
        uint64 nBlockSize = 1000;
        int nBlockSigOps = 100;
        while (!mapPriority.empty())
        {
            // Take highest priority transaction off priority queue
            double dPriority = -(*mapPriority.begin()).first;
            CTransaction& tx = *(*mapPriority.begin()).second;
            mapPriority.erase(mapPriority.begin());

            // Size limits
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK);
            if (nBlockSize + nTxSize >= MAX_BLOCK_SIZE_GEN)
                continue;
            int nTxSigOps = tx.GetSigOpCount();
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
                continue;

            // Transaction fee required depends on block size
            bool fAllowFree = (nBlockSize + nTxSize < 4000 || CTransaction::AllowFree(dPriority));
            int64 nMinFee = tx.GetMinFee(nBlockSize, fAllowFree, true);

            // Connecting shouldn't fail due to dependency on other memory pool transactions
            // because we're already processing them in order of dependency
            map<uint256, TxIndex> mapTestPoolTmp(mapTestPool);
            if (!tx.ConnectInputs(txdb, mapTestPoolTmp, DiskTxPos(1,1,1), pindexPrev, nFees, false, true, nMinFee))
                continue;
            swap(mapTestPool, mapTestPoolTmp);

            // Added
            pblock->vtx.push_back(tx);
            nBlockSize += nTxSize;
            nBlockSigOps += nTxSigOps;

            // Add transactions that depend on this one to the priority queue
            uint256 hash = tx.GetHash();
            if (mapDependers.count(hash))
            {
                BOOST_FOREACH(COrphan* porphan, mapDependers[hash])
                {
                    if (!porphan->setDependsOn.empty())
                    {
                        porphan->setDependsOn.erase(hash);
                        if (porphan->setDependsOn.empty())
                            mapPriority.insert(make_pair(-porphan->dPriority, porphan->ptx));
                    }
                }
            }
        }
    }
    pblock->vtx[0].vout[0].nValue = GetBlockValue(pindexPrev->nHeight+1, nFees);

    // Fill in header
    pblock->hashPrevBlock  = pindexPrev->GetBlockHash();
    pblock->hashMerkleRoot = pblock->BuildMerkleTree();
    pblock->nTime          = max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());
    pblock->nBits          = GetNextWorkRequired(pindexPrev);
    pblock->nNonce         = 0;

    return pblock.release();
}


void IncrementExtraNonce(Block* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock)
    {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;
    pblock->vtx[0].vin[0].scriptSig = CScript() << pblock->nTime << CBigNum(nExtraNonce);
    pblock->hashMerkleRoot = pblock->BuildMerkleTree();
}


void FormatHashBuffers(Block* pblock, char* pmidstate, char* pdata, char* phash1)
{
    //
    // Prebuild hash buffers
    //
    struct
    {
        struct unnamed2
        {
            int nVersion;
            uint256 hashPrevBlock;
            uint256 hashMerkleRoot;
            unsigned int nTime;
            unsigned int nBits;
            unsigned int nNonce;
        }
        block;
        unsigned char pchPadding0[64];
        uint256 hash1;
        unsigned char pchPadding1[64];
    }
    tmp;
    memset(&tmp, 0, sizeof(tmp));

    tmp.block.nVersion       = pblock->nVersion;
    tmp.block.hashPrevBlock  = pblock->hashPrevBlock;
    tmp.block.hashMerkleRoot = pblock->hashMerkleRoot;
    tmp.block.nTime          = pblock->nTime;
    tmp.block.nBits          = pblock->nBits;
    tmp.block.nNonce         = pblock->nNonce;

    FormatHashBlocks(&tmp.block, sizeof(tmp.block));
    FormatHashBlocks(&tmp.hash1, sizeof(tmp.hash1));

    // Byte swap all the input buffer
    for (int i = 0; i < sizeof(tmp)/4; i++)
        ((unsigned int*)&tmp)[i] = ByteReverse(((unsigned int*)&tmp)[i]);

    // Precalc the first half of the first hash, which stays constant
    SHA256Transform(pmidstate, &tmp.block, pSHA256InitState);

    memcpy(pdata, &tmp.block, 128);
    memcpy(phash1, &tmp.hash1, 64);
}


bool CheckWork(Block* pblock, CWallet& wallet, CReserveKey& reservekey)
{
    uint256 hash = pblock->GetHash();
    uint256 hashTarget = CBigNum().SetCompact(pblock->nBits).getuint256();

    if (hash > hashTarget)
        return false;

    //// debug print
    printf("BitcoinMiner:\n");
    printf("proof-of-work found  \n  hash: %s  \ntarget: %s\n", hash.GetHex().c_str(), hashTarget.GetHex().c_str());
    pblock->print();
    printf("%s ", DateTimeStrFormat("%x %H:%M", GetTime()).c_str());
    printf("generated %s\n", FormatMoney(pblock->vtx[0].vout[0].nValue).c_str());

    // Found a solution
    CRITICAL_BLOCK(cs_main)
    {
        if (pblock->hashPrevBlock != hashBestChain)
            return error("BitcoinMiner : generated block is stale");

        // Remove key from key pool
        reservekey.KeepKey();

        // Track how many getdata requests this block gets
        CRITICAL_BLOCK(wallet.cs_wallet)
            wallet.mapRequestCount[pblock->GetHash()] = 0;

        // Process this block the same as if we had received it from another node
        if (!ProcessBlock(NULL, pblock))
            return error("BitcoinMiner : ProcessBlock, block not accepted");
    }

    Sleep(2000);
    return true;
}

void static ThreadBitcoinMiner(void* parg);

void static BitcoinMiner(CWallet *pwallet)
{
    printf("BitcoinMiner started\n");
    SetThreadPriority(THREAD_PRIORITY_LOWEST);

    // Each thread has its own key and counter
    CReserveKey reservekey(pwallet);
    unsigned int nExtraNonce = 0;

    while (fGenerateBitcoins)
    {
        if (AffinityBugWorkaround(ThreadBitcoinMiner))
            return;
        if (fShutdown)
            return;
        while (vNodes.empty() || __blockChain->isInitialBlockDownload())
        {
            Sleep(1000);
            if (fShutdown)
                return;
            if (!fGenerateBitcoins)
                return;
        }


        //
        // Create new block
        //
        unsigned int nTransactionsUpdatedLast = nTransactionsUpdated;
        CBlockIndex* pindexPrev = _bestIndex;

        auto_ptr<Block> pblock(CreateNewBlock(reservekey));
        if (!pblock.get())
            return;
        IncrementExtraNonce(pblock.get(), pindexPrev, nExtraNonce);

        printf("Running BitcoinMiner with %d transactions in block\n", pblock->vtx.size());


        //
        // Prebuild hash buffers
        //
        char pmidstatebuf[32+16]; char* pmidstate = alignup<16>(pmidstatebuf);
        char pdatabuf[128+16];    char* pdata     = alignup<16>(pdatabuf);
        char phash1buf[64+16];    char* phash1    = alignup<16>(phash1buf);

        FormatHashBuffers(pblock.get(), pmidstate, pdata, phash1);

        unsigned int& nBlockTime = *(unsigned int*)(pdata + 64 + 4);
        unsigned int& nBlockNonce = *(unsigned int*)(pdata + 64 + 12);


        //
        // Search
        //
        int64 nStart = GetTime();
        uint256 hashTarget = CBigNum().SetCompact(pblock->nBits).getuint256();
        uint256 hashbuf[2];
        uint256& hash = *alignup<16>(hashbuf);
        loop
        {
            unsigned int nHashesDone = 0;
            unsigned int nNonceFound;

            // Crypto++ SHA-256
            nNonceFound = ScanHash_CryptoPP(pmidstate, pdata + 64, phash1,
                                            (char*)&hash, nHashesDone);

            // Check if something found
            if (nNonceFound != -1)
            {
                for (int i = 0; i < sizeof(hash)/4; i++)
                    ((unsigned int*)&hash)[i] = ByteReverse(((unsigned int*)&hash)[i]);

                if (hash <= hashTarget)
                {
                    // Found a solution
                    pblock->nNonce = ByteReverse(nNonceFound);
                    assert(hash == pblock->GetHash());

                    SetThreadPriority(THREAD_PRIORITY_NORMAL);
                    CheckWork(pblock.get(), *pwalletMain, reservekey);
                    SetThreadPriority(THREAD_PRIORITY_LOWEST);
                    break;
                }
            }

            // Meter hashes/sec
            static int64 nHashCounter;
            if (nHPSTimerStart == 0)
            {
                nHPSTimerStart = GetTimeMillis();
                nHashCounter = 0;
            }
            else
                nHashCounter += nHashesDone;
            if (GetTimeMillis() - nHPSTimerStart > 4000)
            {
                static CCriticalSection cs;
                CRITICAL_BLOCK(cs)
                {
                    if (GetTimeMillis() - nHPSTimerStart > 4000)
                    {
                        dHashesPerSec = 1000.0 * nHashCounter / (GetTimeMillis() - nHPSTimerStart);
                        nHPSTimerStart = GetTimeMillis();
                        nHashCounter = 0;
                        string strStatus = strprintf("    %.0f khash/s", dHashesPerSec/1000.0);
                        UIThreadCall(boost::bind(CalledSetStatusBar, strStatus, 0));
                        static int64 nLogTime;
                        if (GetTime() - nLogTime > 30 * 60)
                        {
                            nLogTime = GetTime();
                            printf("%s ", DateTimeStrFormat("%x %H:%M", GetTime()).c_str());
                            printf("hashmeter %3d CPUs %6.0f khash/s\n", vnThreadsRunning[3], dHashesPerSec/1000.0);
                        }
                    }
                }
            }

            // Check for stop or if block needs to be rebuilt
            if (fShutdown)
                return;
            if (!fGenerateBitcoins)
                return;
            if (fLimitProcessors && vnThreadsRunning[3] > nLimitProcessors)
                return;
            if (vNodes.empty())
                break;
            if (nBlockNonce >= 0xffff0000)
                break;
            if (nTransactionsUpdated != nTransactionsUpdatedLast && GetTime() - nStart > 60)
                break;
            if (pindexPrev != _bestIndex)
                break;

            // Update nTime every few seconds
            pblock->nTime = max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());
            nBlockTime = ByteReverse(pblock->nTime);
        }
    }
}

void static ThreadBitcoinMiner(void* parg)
{
    CWallet* pwallet = (CWallet*)parg;
    try
    {
        vnThreadsRunning[3]++;
        BitcoinMiner(pwallet);
        vnThreadsRunning[3]--;
    }
    catch (std::exception& e) {
        vnThreadsRunning[3]--;
        PrintException(&e, "ThreadBitcoinMiner()");
    } catch (...) {
        vnThreadsRunning[3]--;
        PrintException(NULL, "ThreadBitcoinMiner()");
    }
    UIThreadCall(boost::bind(CalledSetStatusBar, "", 0));
    nHPSTimerStart = 0;
    if (vnThreadsRunning[3] == 0)
        dHashesPerSec = 0;
    printf("ThreadBitcoinMiner exiting, %d threads remaining\n", vnThreadsRunning[3]);
}


void GenerateBitcoins(bool fGenerate, CWallet* pwallet)
{
    if (fGenerateBitcoins != fGenerate)
    {
        fGenerateBitcoins = fGenerate;
        WriteSetting("fGenerateBitcoins", fGenerateBitcoins);
        MainFrameRepaint();
    }
    if (fGenerateBitcoins)
    {
        int nProcessors = boost::thread::hardware_concurrency();
        printf("%d processors\n", nProcessors);
        if (nProcessors < 1)
            nProcessors = 1;
        if (fLimitProcessors && nProcessors > nLimitProcessors)
            nProcessors = nLimitProcessors;
        int nAddThreads = nProcessors - vnThreadsRunning[3];
        printf("Starting %d BitcoinMiner threads\n", nAddThreads);
        for (int i = 0; i < nAddThreads; i++)
        {
            if (!CreateThread(ThreadBitcoinMiner, pwallet))
                printf("Error: CreateThread(ThreadBitcoinMiner) failed\n");
            Sleep(10);
        }
    }
}
*/