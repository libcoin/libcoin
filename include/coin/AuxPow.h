// Copyright (c) 2009-2010 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
#ifndef COIN_AUXPOW_H
#define COIN_AUXPOW_H

#include <coin/MerkleTx.h>

#include <coin/BlockHeader.h>

// this is for merged mining support
enum {
    // primary version
    BLOCK_VERSION_DEFAULT        = (1 << 0),
    
    // modifiers
    BLOCK_VERSION_AUXPOW         = (1 << 8),
    
    // bits allocated for chain ID
    BLOCK_VERSION_CHAIN_START    = (1 << 16),
    BLOCK_VERSION_CHAIN_END      = (1 << 30)
};

class AuxPow : public MerkleTx {
public:
    AuxPow(const Transaction& txn) : MerkleTx(txn) {
    }

    AuxPow() : MerkleTx() {
    }

    // Merkle branch with root vchAux
    // root must be present inside the coinbase
    std::vector<uint256> vChainMerkleBranch;
    // Index of chain in chains merkle tree
    int nChainIndex;
    BlockHeader parentBlock;

    IMPLEMENT_SERIALIZE
    (
        nSerSize += SerReadWrite(s, *(MerkleTx*)this, nType, nVersion, ser_action);
        nVersion = this->version();
        READWRITE(vChainMerkleBranch);
        READWRITE(nChainIndex);

        // Always serialize the saved parent block as header so that the size of CAuxPow
        // is consistent.
        nSerSize += SerReadWrite(s, parentBlock, nType | SER_BLOCKHEADERONLY, nVersion, ser_action);
    )

    bool Check(uint256 hashAuxBlock, int nChainID) const;

    uint256 GetParentBlockHash() const {
        return parentBlock.getHash();
    }
};

template <typename Stream>
int ReadWriteAuxPow(Stream& s, const AuxPow& auxpow, int nType, int nVersion, CSerActionGetSerializeSize ser_action) {
    if (nVersion & BLOCK_VERSION_AUXPOW) {
        return ::GetSerializeSize(auxpow, nType, nVersion);
    }
    return 0;
}

template <typename Stream>
int ReadWriteAuxPow(Stream& s, const AuxPow& auxpow, int nType, int nVersion, CSerActionSerialize ser_action) {
    if (nVersion & BLOCK_VERSION_AUXPOW) {
        return SerReadWrite(s, auxpow, nType, nVersion, ser_action);
    }
    return 0;
}

template <typename Stream>
int ReadWriteAuxPow(Stream& s, AuxPow& auxpow, int nType, int nVersion, CSerActionUnserialize ser_action) {
    if (nVersion & BLOCK_VERSION_AUXPOW) {
        return SerReadWrite(s, auxpow, nType, nVersion, ser_action);
    }
    else {
        return 0;
    }
}

//extern void RemoveMergedMiningHeader(std::vector<unsigned char>& vchAux);
//extern CScript MakeCoinbaseWithAux(unsigned int nBits, unsigned int nExtraNonce, std::vector<unsigned char>& vchAux);

#endif // COIN_AUXPOW_H
