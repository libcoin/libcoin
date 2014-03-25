// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include <coin/MerkleTx.h>
#include <coin/Logger.h>

using namespace std;

ostream& operator<<(ostream& os, const MerkleTx& mtx) {
    return os << (const Transaction&)mtx << mtx._blockHash << mtx._merkleBranch << const_binary<int>(mtx._index);
}

istream& operator>>(istream& is, MerkleTx& mtx) {
    return is >> (Transaction&)mtx >> mtx._blockHash >> mtx._merkleBranch >> binary<int>(mtx._index);
}


/*
int CMerkleTx::setMerkleBranch(const Block& block, const BlockChain& blockChain)
{
    // Update the tx's hashBlock
    _blockHash = block.getHash();
    
    // Locate the transaction
    TransactionList txes = block.getTransactions();
    _index = 0;
    for (TransactionList::const_iterator tx = txes.begin(); tx != txes.end(); ++tx, ++_index) {
        if (*tx == *(Transaction*)this)
            break;        
    }
    
    if (_index == txes.size()) {
        _merkleBranch.clear();
        _index = -1;
        log_debug("ERROR: SetMerkleBranch() : couldn't find tx in block\n");
        return 0;
    }
    
    // Fill in merkle branch
    _merkleBranch = block.getMerkleBranch(_index);
    
    int height = blockChain.getHeight(_blockHash);
    
    return height < 0 ? 0 : height;
}

int CMerkleTx::GetDepthInMainChain(int& nHeightRet) const
{
    if (hashBlock == 0 || nIndex == -1)
        return 0;
    
    // Find the block it claims to be in
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = (*mi).second;
    if (!pindex || !pindex->IsInMainChain())
        return 0;
    
    // Make sure the merkle branch connects to this block
    if (!fMerkleVerified)
        {
        if (Block::checkMerkleBranch(GetHash(), vMerkleBranch, nIndex) != pindex->hashMerkleRoot)
            return 0;
        fMerkleVerified = true;
        }
    
    nHeightRet = pindex->nHeight;
    return pindexBest->nHeight - pindex->nHeight + 1;
}


int CMerkleTx::GetBlocksToMaturity() const
{
    if (!IsCoinBase())
        return 0;
    return max(0, (COINBASE_MATURITY+20) - GetDepthInMainChain());
}


bool CMerkleTx::AcceptToMemoryPool(CTxDB& txdb, bool fCheckInputs)
{
    if (fClient)
        {
        if (!IsInMainChain() && !ClientConnectInputs())
            return false;
        return Transaction::AcceptToMemoryPool(txdb, false);
        }
    else
        {
        return Transaction::AcceptToMemoryPool(txdb, fCheckInputs);
        }
}

bool CMerkleTx::AcceptToMemoryPool()
{
    CTxDB txdb("r");
    return AcceptToMemoryPool(txdb);
}

int CMerkleTx::SetMerkleBranch(const Block* pblock)
{
    if (fClient)
        {
        if (hashBlock == 0)
            return 0;
        }
    else
        {
        Block blockTmp;
        if (pblock == NULL)
            {
            // Load the block this tx is in
            TxIndex txindex;
            if (!CTxDB("r").ReadTxIndex(GetHash(), txindex))
                return 0;
            if (!blockTmp.ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos))
                return 0;
            pblock = &blockTmp;
            }
        
        // Update the tx's hashBlock
        hashBlock = pblock->GetHash();
        
        // Locate the transaction
        for (nIndex = 0; nIndex < pblock->vtx.size(); nIndex++)
            if (pblock->vtx[nIndex] == *(Transaction*)this)
                break;
        if (nIndex == pblock->vtx.size())
            {
            vMerkleBranch.clear();
            nIndex = -1;
            printf("ERROR: SetMerkleBranch() : couldn't find tx in block\n");
            return 0;
            }
        
        // Fill in merkle branch
        vMerkleBranch = pblock->GetMerkleBranch(nIndex);
        }
    
    // Is the tx in a block that's in the main chain
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = (*mi).second;
    if (!pindex || !pindex->IsInMainChain())
        return 0;
    
    return pindexBest->nHeight - pindex->nHeight + 1;
}

*/
