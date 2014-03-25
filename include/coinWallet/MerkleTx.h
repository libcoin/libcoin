// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
#ifndef COINWALLET_MERKLETX_H
#define COINWALLET_MERKLETX_H

#include <coin/Transaction.h>
#include <coin/BigNum.h>
#include <coin/Key.h>
#include <coin/Script.h>

#include <coin/Block.h>
#include <coinChain/BlockChain.h>

#include <coinWallet/Export.h>
#include <coinWallet/serialize.h>

/// A transaction with a merkle branch linking it to the block chain.
class COINWALLET_EXPORT CMerkleTx : public Transaction
{
public:
    uint256 _blockHash;
    MerkleBranch _merkleBranch;
    int _index;
    
    // memory only
    mutable char _merkleVerified;
    
    
    CMerkleTx() {
        Init();
    }
    
    CMerkleTx(const Transaction& txIn) : Transaction(txIn) {
        Init();
    }
    
    void Init() {
        _blockHash = 0;
        _index = -1;
        _merkleVerified = false;
    }    
    
    IMPLEMENT_SERIALIZE
    (
     nSerSize += SerReadWrite(s, *(Transaction*)this, nType, nVersion, ser_action);
     nVersion = this->_version;
     READWRITE(_blockHash);
     READWRITE(_merkleBranch);
     READWRITE(_index);
     )
    
   
    int setMerkleBranch(const Block& block, const BlockChain& blockChain);
/*
    int GetDepthInMainChain(int& nHeightRet) const;
    int GetDepthInMainChain() const { int nHeight; return GetDepthInMainChain(nHeight); }
    bool IsInMainChain() const { return GetDepthInMainChain() > 0; }
    int GetBlocksToMaturity() const;
    bool AcceptToMemoryPool(CTxDB& txdb, bool fCheckInputs=true);
    bool AcceptToMemoryPool();
 */
};

#endif // COINWALLET_MERKLETX_H
