
#include "btcNode/Block.h"

#include "btcNode/main.h"
#include "btcNode/db.h"

#include "btcNode/BlockChain.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

using namespace std;
using namespace boost;

CBlockIndex::CBlockIndex(unsigned int nFileIn, unsigned int nBlockPosIn, Block& block)
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

bool CBlockIndex::CheckIndex() const {
    return Block::CheckProofOfWork(GetBlockHash(), nBits);
}

Block CBlockIndex::GetBlockHeader() const
{
    uint256 prevBlock;
    if (pprev)
        prevBlock = pprev->GetBlockHash();
    Block block(nVersion, prevBlock, hashMerkleRoot, nTime, nBits, nNonce);
    return block;
}


uint256 CDiskBlockIndex::GetBlockHash() const
{
    Block block(nVersion, hashPrev, hashMerkleRoot, nTime, nBits, nNonce);
    return block.getHash();
}



void CBlockLocator::Set(const CBlockIndex* pindex)
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
    vHave.push_back(__blockChain->getGenesisHash());
}

int CBlockLocator::GetHeight()
{
    CBlockIndex* pindex = __blockChain->getBlockIndex(*this);
    if (!pindex)
        return 0;
    return pindex->nHeight;
}
