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
typedef std::vector<uint256> MerkleBranch;

class COIN_EXPORT Block {
public:
    Block() {
        setNull();
    }

    Block(const int version, const uint256 prevBlock, const uint256 merkleRoot, const int time, const int bits, const int nonce) : _version(version), _prevBlock(prevBlock), _merkleRoot(merkleRoot), _time(time), _bits(bits), _nonce(nonce) {
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

    void setNull() {
        _version = 1;
        _prevBlock = 0;
        _merkleRoot = 0;
        _time = 0;
        _bits = 0;
        _nonce = 0;
        _transactions.clear();
        _merkleTree.clear();
    }

    bool isNull() const {
        return (_bits == 0);
    }

    uint256 getHash() const;

    uint256 getHalfHash() const;
    
    int64_t getBlockTime() const {
        return (int64_t)_time;
    }

    int GetSigOpCount() const;

    void addTransaction(const Transaction& tx) { _transactions.push_back(tx); }
    size_t getNumTransactions() const { return _transactions.size(); }
    const TransactionList& getTransactions() const { return _transactions; }
    TransactionList& getTransactions() { return _transactions; }
    Transaction& getTransaction(size_t i) { return _transactions[i]; }
    const Transaction& getTransaction(size_t i) const { return _transactions[i]; }
    void keepOnlyCoinbase() { _transactions.resize(1); }

    uint256 buildMerkleTree() const;

    void updateMerkleTree() {
        _merkleRoot = buildMerkleTree();
    }
    
    MerkleBranch getMerkleBranch(int index) const;

    static uint256 checkMerkleBranch(uint256 hash, const std::vector<uint256>& merkleBranch, int index);

    void print() const;

    //    bool checkBlock(const CBigNum& proofOfWorkLimit) const;
    bool checkBlock() const;
    
    /// Version 2 check according to BIP 0034: Height should be in the coinbase
    bool checkHeightInCoinbase(int height) const;
    int getHeight() const;

    /// Version 3 check (no BIP): Root hash of all Spendables should be in the coinbase (following the height)
    bool checkSpendablesRootInCoinbase(uint256 hash) const;
    uint256 getSpendablesRoot() const;
    
    const int getVersion() const { return _version; }
    
    const uint256 getPrevBlock() const { return _prevBlock; }

    const uint256 getMerkleRoot() const { return _merkleRoot; }

    const int getTime() const { return _time; }
    
    void setTime(int time) { _time = time; }
    
    const int getBits() const { return _bits; }
    
    const uint256 getTarget() const {
        return CBigNum().SetCompact(getBits()).getuint256();
    }
    
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

    
    const int getNonce() const { return _nonce; }

    void setNonce(unsigned int nonce) { _nonce = nonce; }
    
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
    mutable MerkleBranch _merkleTree;
};

#endif // BLOCK_H
