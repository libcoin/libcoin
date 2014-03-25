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

#ifndef BLOCK_H
#define BLOCK_H

#include <coin/Export.h>
#include <coin/BlockHeader.h>
#include <coin/AuxPow.h>
#include <coin/Transaction.h>
#include <coin/uint256.h>

#include <vector>

class Block;

//
// Nodes collect new transactions into a block, hash them into a hash tree,
// and scan through nonce values to make the block's hash satisfy proof-of-work
// requirements.  When they solve the proof-of-work, they broadcast the block
// to everyone and the block is added to the block chain.  The first transaction
// in the block is a special one that creates a new coin owned by the creator
// of the block.
//

typedef std::vector<Transaction> TransactionList;

class COIN_EXPORT Block : public BlockHeader {
public:
    Block() {
        setNull();
    }

    Block(const int version, const uint256 prevBlock, const uint256 merkleRoot, const int time, const int bits, const int nonce) : BlockHeader(version, prevBlock,merkleRoot, time, bits, nonce), _ignore_aux_pow(true) {
        _transactions.clear();
        _merkleTree.clear();
    }
    
    void adhere_aux_pow() {
        _ignore_aux_pow = false;
    }
    
    friend std::ostream& operator<<(std::ostream& os, const Block& b);
    
    friend std::istream& operator>>(std::istream& is, Block& b);

    void setNull() {
        BlockHeader::setNull();
        _transactions.clear();
        _merkleTree.clear();
        _ignore_aux_pow = true;
    }

    int GetSigOpCount() const;

    void addTransaction(const Transaction& tx) { _transactions.push_back(tx); }
    size_t getNumTransactions() const { return _transactions.size(); }
    const TransactionList& getTransactions() const { return _transactions; }
    TransactionList& getTransactions() { return _transactions; }
    Transaction& getTransaction(size_t i) { return _transactions[i]; }
    const Transaction& getTransaction(size_t i) const { return _transactions[i]; }
    void keepOnlyCoinbase() { _transactions.resize(1); }
    void stripTransactions() { _transactions.clear(); }

    const AuxPow& getAuxPoW() const {
        return _aux_pow;
    }
    
    void setAuxPoW(const AuxPow& auxpow) {
        _aux_pow = auxpow;
    }
    
    uint256 buildMerkleTree() const;

    void updateMerkleTree() {
        _merkleRoot = buildMerkleTree();
    }
    
    MerkleBranch getMerkleBranch(int index) const;

    static uint256 checkMerkleBranch(uint256 hash, const MerkleBranch& merkleBranch, int index);

    void print() const;
    
    /// Version 2 check according to BIP 0034: Height should be in the coinbase
    bool checkHeightInCoinbase(int height) const;
    int getHeight() const;

    /// Version 3 check (no BIP): Root hash of all Spendables should be in the coinbase (following the height)
    bool checkSpendablesRootInCoinbase(uint256 hash) const;
    uint256 getSpendablesRoot() const;
        
    int getShareBits() const {
        try {
            Script coinbase = getTransaction(0).getInput(0).signature();
            Script::const_iterator cbi = coinbase.begin();
            opcodetype opcode;
            std::vector<unsigned char> data;
            // We simply ignore the first opcode and data, however, it is the height...
            coinbase.getOp(cbi, opcode, data);
            
            // this should be an opcode for a uint256 (i.e. number of 32 bytes)
            coinbase.getOp(cbi, opcode, data);
            
            // now parse the prev share
            coinbase.getOp(cbi, opcode, data);
            
            // now parse the target
            coinbase.getOp(cbi, opcode, data);
            
            if (opcode < OP_PUSHDATA1) {
                CBigNum height;
                height.setvch(data);
                return height.getint();
            }
        } catch (...) {
        }
        return 0;
    }

    const uint256 getShareTarget() const {
        return CBigNum().SetCompact(getShareBits()).getuint256();
    }
    
    uint256 getPrevShare() const {
        try {
            Script coinbase = getTransaction(0).getInput(0).signature();
            Script::const_iterator cbi = coinbase.begin();
            opcodetype opcode;
            std::vector<unsigned char> data;
            // We simply ignore the first opcode and data, however, it is the height...
            coinbase.getOp(cbi, opcode, data);
            
            // this should be an opcode for a uint256 (i.e. number of 32 bytes)
            coinbase.getOp(cbi, opcode, data);
            
            // and the prev share (also 32 bytes)
            uint256 hash_from_coinbase(data);
            return hash_from_coinbase;
        } catch (...) {
        }
        return uint256(0);
    }
    
    Script getRewardee(size_t i = 0) const {
        const Transaction& txn = getTransaction(0);
        return txn.getOutput(0).script();
    }
    
private:
    // will only be used for merged miners
    AuxPow _aux_pow;
    
    // network and disk
    TransactionList _transactions;
    
    // memory only
    mutable MerkleBranch _merkleTree;
    
    bool _ignore_aux_pow;
};

#endif // BLOCK_H
