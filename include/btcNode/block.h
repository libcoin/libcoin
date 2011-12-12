
#ifndef BLOCK_H
#define BLOCK_H

#include "btc/bignum.h"
#include "btc/key.h"
#include "btc/script.h"
#include "btc/tx.h"

#include "btcNode/main.h"

#include <list>

class Block;
class CBlockIndex;

class CKeyItem;

class Endpoint;
class Inventory;
class CRequestTracker;
class CNode;
class CBlockIndex;

class CTransaction;

class CTxDB;
class CTxIndex;


//
// Nodes collect new transactions into a block, hash them into a hash tree,
// and scan through nonce values to make the block's hash satisfy proof-of-work
// requirements.  When they solve the proof-of-work, they broadcast the block
// to everyone and the block is added to the block chain.  The first transaction
// in the block is a special one that creates a new coin owned by the creator
// of the block.
//
// Blocks are appended to blk0001.dat files on disk.  Their location on disk
// is indexed by CBlockIndex objects in memory.
//

typedef std::vector<CTransaction> TransactionList;
typedef std::vector<uint256> MerkleTree;

class Block
{
public:
    Block()
    {
        setNull();
    }

    Block(const int version, const uint256 prevBlock, const uint256 merkleRoot, const int time, const int bits, const int nonce) : _version(version), _prevBlock(prevBlock), _merkleRoot(merkleRoot), _time(time), _bits(bits), _nonce(nonce)
    {
        _transactions.clear();
        _merkleTree.clear();
    }

    
    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->_version);
        nVersion = this->_version;
        READWRITE(_prevBlock);
        READWRITE(_merkleRoot);
        READWRITE(_time);
        READWRITE(_bits);
        READWRITE(_nonce);

        // ConnectBlock depends on vtx being last so it can calculate offset
        if (!(nType & (SER_GETHASH|SER_BLOCKHEADERONLY)))
            READWRITE(_transactions);
        else if (fRead)
            const_cast<Block*>(this)->_transactions.clear();
    )

    void setNull()
    {
        _version = 1;
        _prevBlock = 0;
        _merkleRoot = 0;
        _time = 0;
        _bits = 0;
        _nonce = 0;
        _transactions.clear();
        _merkleTree.clear();
    }

    bool isNull() const
    {
        return (_bits == 0);
    }

    uint256 getHash() const
    {
        return Hash(BEGIN(_version), END(_nonce));
    }

    int64 getBlockTime() const
    {
        return (int64)_time;
    }

    int GetSigOpCount() const
    {
        int n = 0;
        BOOST_FOREACH(const CTransaction& tx, _transactions)
            n += tx.GetSigOpCount();
        return n;
    }

    void addTransaction(const CTransaction& tx) { _transactions.push_back(tx); }
    size_t getNumTransactions() { return _transactions.size(); }
    const TransactionList getTransactions() const { return _transactions; }
    
    uint256 buildMerkleTree() const
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

    std::vector<uint256> getMerkleBranch(int index) const
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

    static uint256 checkMerkleBranch(uint256 hash, const std::vector<uint256>& merkleBranch, int index)
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

    void print() const
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

    bool disconnectBlock(CTxDB& txdb, CBlockIndex* pindex);
    bool connectBlock(CTxDB& txdb, CBlockIndex* pindex);
    bool setBestChain(CTxDB& txdb, CBlockIndex* pindexNew);
    bool addToBlockIndex(unsigned int nFile, unsigned int nBlockPos);
    bool checkBlock() const;
    bool acceptBlock();
    
    const int getVersion() const { return _version; }
    //    void setVersion(const int version) { _version = version; }
    
    const uint256 getPrevBlock() const { return _prevBlock; }
    //    void setPrevBlock(const uint256 pb) { _prevBlock = pb; }

    const uint256 getMerkleRoot() const { return _merkleRoot; }
    //    void getMerkleRoot(const uint256 mr) { _merkleRoot = mr; }

    const int getTime() const { return _time; }
    //    void setTime(const int time) { _time = time; }
    
    const int getBits() const { return _bits; }
    //    void setBits(const int bits) { _bits = bits; }
    
    const int getNonce() const { return _nonce; }
    //    void setNonce(const int nonce) { _nonce = nonce; }
    
    
private:
    // header
    int _version;
    uint256 _prevBlock;
    uint256 _merkleRoot;
    unsigned int _time;
    unsigned int _bits;
    unsigned int _nonce;
    
    // network and disk
    TransactionList _transactions;
    
    // memory only
    mutable MerkleTree _merkleTree;
};






//
// The block chain is a tree shaped structure starting with the
// genesis block at the root, with each block potentially having multiple
// candidates to be the next block.  pprev and pnext link a path through the
// main/longest chain.  A blockindex may have multiple pprev pointing back
// to it, but pnext will only point forward to the longest branch, or will
// be null if the block is not part of the longest chain.
//
class CBlockIndex
{
public:
    const uint256* phashBlock;
    CBlockIndex* pprev;
    CBlockIndex* pnext;
    unsigned int nFile;
    unsigned int nBlockPos;
    int nHeight;
    CBigNum bnChainWork;

    // block header
    int nVersion;
    uint256 hashMerkleRoot;
    unsigned int nTime;
    unsigned int nBits;
    unsigned int nNonce;


    CBlockIndex()
    {
        phashBlock = NULL;
        pprev = NULL;
        pnext = NULL;
        nFile = 0;
        nBlockPos = 0;
        nHeight = 0;
        bnChainWork = 0;

        nVersion       = 0;
        hashMerkleRoot = 0;
        nTime          = 0;
        nBits          = 0;
        nNonce         = 0;
    }

    CBlockIndex(unsigned int nFileIn, unsigned int nBlockPosIn, Block& block)
    {
        phashBlock = NULL;
        pprev = NULL;
        pnext = NULL;
        nFile = nFileIn;
        nBlockPos = nBlockPosIn;
        nHeight = 0;
        bnChainWork = 0;

        nVersion       = block.getVersion();
        hashMerkleRoot = block.getMerkleRoot();
        nTime          = block.getTime();
        nBits          = block.getBits();
        nNonce         = block.getNonce();
    }

    Block GetBlockHeader() const
    {
        uint256 prevBlock;
        if (pprev)
            prevBlock = pprev->GetBlockHash();
        Block block(nVersion, prevBlock, hashMerkleRoot, nTime, nBits, nNonce);
        return block;
    }

    uint256 GetBlockHash() const
    {
        return *phashBlock;
    }

    int64 GetBlockTime() const
    {
        return (int64)nTime;
    }

    CBigNum GetBlockWork() const
    {
        CBigNum bnTarget;
        bnTarget.SetCompact(nBits);
        if (bnTarget <= 0)
            return 0;
        return (CBigNum(1)<<256) / (bnTarget+1);
    }

    bool IsInMainChain() const
    {
        return (pnext || this == pindexBest);
    }

    bool CheckIndex() const
    {
        return CheckProofOfWork(GetBlockHash(), nBits);
    }

    enum { nMedianTimeSpan=11 };

    int64 GetMedianTimePast() const
    {
        int64 pmedian[nMedianTimeSpan];
        int64* pbegin = &pmedian[nMedianTimeSpan];
        int64* pend = &pmedian[nMedianTimeSpan];

        const CBlockIndex* pindex = this;
        for (int i = 0; i < nMedianTimeSpan && pindex; i++, pindex = pindex->pprev)
            *(--pbegin) = pindex->GetBlockTime();

        std::sort(pbegin, pend);
        return pbegin[(pend - pbegin)/2];
    }

    int64 GetMedianTime() const
    {
        const CBlockIndex* pindex = this;
        for (int i = 0; i < nMedianTimeSpan/2; i++)
        {
            if (!pindex->pnext)
                return GetBlockTime();
            pindex = pindex->pnext;
        }
        return pindex->GetMedianTimePast();
    }



    std::string toString() const
    {
        return strprintf("CBlockIndex(nprev=%08x, pnext=%08x, nFile=%d, nBlockPos=%-6d nHeight=%d, merkle=%s, hashBlock=%s)",
            pprev, pnext, nFile, nBlockPos, nHeight,
            hashMerkleRoot.toString().substr(0,10).c_str(),
            GetBlockHash().toString().substr(0,20).c_str());
    }

    void print() const
    {
        printf("%s\n", toString().c_str());
    }
};



//
// Used to marshal pointers into hashes for db storage.
//
class CDiskBlockIndex : public CBlockIndex
{
public:
    uint256 hashPrev;
    uint256 hashNext;

    CDiskBlockIndex()
    {
        hashPrev = 0;
        hashNext = 0;
    }

    explicit CDiskBlockIndex(CBlockIndex* pindex) : CBlockIndex(*pindex)
    {
        hashPrev = (pprev ? pprev->GetBlockHash() : 0);
        hashNext = (pnext ? pnext->GetBlockHash() : 0);
    }

    IMPLEMENT_SERIALIZE
    (
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);

        READWRITE(hashNext);
        READWRITE(nFile);
        READWRITE(nBlockPos);
        READWRITE(nHeight);

        // block header
        READWRITE(this->nVersion);
        READWRITE(hashPrev);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);
    )

    uint256 GetBlockHash() const
    {
        Block block(nVersion, hashPrev, hashMerkleRoot, nTime, nBits, nNonce);
        return block.getHash();
    }


    std::string toString() const
    {
        std::string str = "CDiskBlockIndex(";
        str += CBlockIndex::toString();
        str += strprintf("\n                hashBlock=%s, hashPrev=%s, hashNext=%s)",
            GetBlockHash().toString().c_str(),
            hashPrev.toString().substr(0,20).c_str(),
            hashNext.toString().substr(0,20).c_str());
        return str;
    }

    void print() const
    {
        printf("%s\n", toString().c_str());
    }
};








//
// Describes a place in the block chain to another node such that if the
// other node doesn't have the same branch, it can find a recent common trunk.
// The further back it is, the further before the fork it may be.
//
class CBlockLocator
{
protected:
    std::vector<uint256> vHave;
public:

    CBlockLocator()
    {
    }

    explicit CBlockLocator(const CBlockIndex* pindex)
    {
        Set(pindex);
    }

    explicit CBlockLocator(uint256 hashBlock)
    {
        std::map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashBlock);
        if (mi != mapBlockIndex.end())
            Set((*mi).second);
    }

    IMPLEMENT_SERIALIZE
    (
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vHave);
    )

    void SetNull()
    {
        vHave.clear();
    }

    bool IsNull()
    {
        return vHave.empty();
    }

    void Set(const CBlockIndex* pindex)
    {
        vHave.clear();
        int nStep = 1;
        while (pindex)
        {
            vHave.push_back(pindex->GetBlockHash());

            // Exponentially larger steps back
            for (int i = 0; pindex && i < nStep; i++)
                pindex = pindex->pprev;
            if (vHave.size() > 10)
                nStep *= 2;
        }
        vHave.push_back(hashGenesisBlock);
    }

    int GetDistanceBack()
    {
        // Retrace how far back it was in the sender's branch
        int nDistance = 0;
        int nStep = 1;
        BOOST_FOREACH(const uint256& hash, vHave)
        {
            std::map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hash);
            if (mi != mapBlockIndex.end())
            {
                CBlockIndex* pindex = (*mi).second;
                if (pindex->IsInMainChain())
                    return nDistance;
            }
            nDistance += nStep;
            if (nDistance > 10)
                nStep *= 2;
        }
        return nDistance;
    }

    CBlockIndex* GetBlockIndex()
    {
        // Find the first block the caller has in the main chain
        BOOST_FOREACH(const uint256& hash, vHave)
        {
            std::map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hash);
            if (mi != mapBlockIndex.end())
            {
                CBlockIndex* pindex = (*mi).second;
                if (pindex->IsInMainChain())
                    return pindex;
            }
        }
        return pindexGenesisBlock;
    }

    uint256 GetBlockHash()
    {
        // Find the first block the caller has in the main chain
        BOOST_FOREACH(const uint256& hash, vHave)
        {
            std::map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hash);
            if (mi != mapBlockIndex.end())
            {
                CBlockIndex* pindex = (*mi).second;
                if (pindex->IsInMainChain())
                    return hash;
            }
        }
        return hashGenesisBlock;
    }

    int GetHeight()
    {
        CBlockIndex* pindex = GetBlockIndex();
        if (!pindex)
            return 0;
        return pindex->nHeight;
    }
};

#endif // BLOCK_H
