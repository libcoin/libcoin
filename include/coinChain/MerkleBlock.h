/* -*-c++-*- libcoin - Copyright (C) 2012-14 Michael Gronager
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

#ifndef MERKLEBLOCK_H
#define MERKLEBLOCK_H

#include <coinChain/Export.h>

#include <coin/BlockHeader.h>


/** Data structure that represents a partial merkle tree.
 *
 * It respresents a subset of the txid's of a known block, in a way that
 * allows recovery of the list of txid's and the merkle root, in an
 * authenticated way.
 *
 * The encoding works as follows: we traverse the tree in depth-first order,
 * storing a bit for each traversed node, signifying whether the node is the
 * parent of at least one matched leaf txid (or a matched txid itself). In
 * case we are at the leaf level, or this bit is 0, its merkle node hash is
 * stored, and its children are not explorer further. Otherwise, no hash is
 * stored, but we recurse into both (or the only) child branch. During
 * decoding, the same depth-first traversal is performed, consuming bits and
 * hashes as they written during encoding.
 *
 * The serialization is fixed and provides a hard guarantee about the
 * encoded size:
 *
 *   SIZE <= 10 + ceil(32.25*N)
 *
 * Where N represents the number of leaf nodes of the partial tree. N itself
 * is bounded by:
 *
 *   N <= total_transactions
 *   N <= 1 + matched_transactions*tree_height
 *
 * The serialization format:
 *  - uint32     total_transactions (4 bytes)
 *  - varint     number of hashes   (1-3 bytes)
 *  - uint256[]  hashes in depth-first order (<= 32*N bytes)
 *  - varint     number of bytes of flag bits (1-3 bytes)
 *  - byte[]     flag bits, packed per 8 in a byte, least significant bit first (<= 2*N-1 bits)
 * The size constraints follow from this.
 */

#include <coin/Block.h>

class BloomFilter;
class Block;

class COINCHAIN_EXPORT MerkleBlock : public BlockHeader {
public:
    MerkleBlock(const Block& block, BloomFilter& filter);
    
    inline friend std::ostream& operator<<(std::ostream& os, const MerkleBlock& mb) {
        std::vector<unsigned char> bytes;
        bytes.resize((mb._matches.size()+7)/8);
        for (unsigned int p = 0; p < mb._matches.size(); p++)
            bytes[p / 8] |= mb._matches[p] << (p % 8);
        
        return os << (const BlockHeader&) mb << const_binary<unsigned int>(mb._transactions) << mb._hashes << bytes;
    }
    
    inline friend std::istream& operator>>(std::istream& is, MerkleBlock& mb) {
        std::vector<unsigned char> bytes;
        is >> (BlockHeader&) mb >> binary<unsigned int>(mb._transactions) >> mb._hashes >> bytes;
        mb._matches.resize(bytes.size() * 8);
        for (unsigned int p = 0; p < mb._matches.size(); p++)
            mb._matches[p] = (bytes[p / 8] & (1 << (p % 8))) != 0;
        mb._bad = false;
        return is;
    }
    
    size_t getNumTransactions() const {
        return _transactions;
    }

    size_t getNumFilteredTransactions() const {
        return _matched_txes.size();
    }
    
    size_t getTransactionBlockIndex(size_t i) const {
        if (i >= getNumFilteredTransactions())
            throw std::runtime_error("MerkleBlock::getTransactionBlockIndex() : i out of range");

        return _matched_txes[i].first;
    }
    
    uint256 getTransactionBlockHash(size_t i) const {
        if (i >= getNumFilteredTransactions())
            throw std::runtime_error("MerkleBlock::getTransactionBlockIndex() : i out of range");
        
        return _matched_txes[i].second;
    }

    // extract the matching txid's represented by this partial merkle tree.
    // returns the merkle root, or 0 in case of failure
    uint256 extractMatches(std::vector<uint256> &vMatch);
private:
    // helper function to efficiently calculate the number of nodes at given height in the merkle tree
    unsigned int calcTreeWidth(int height) {
        return (_transactions+(1 << height)-1) >> height;
    }
    
    // calculate the hash of a node in the merkle tree (at leaf level: the txid's themself)
    uint256 calcHash(int height, unsigned int pos, const std::vector<uint256> &vTxid);
    
    // recursive function that traverses tree nodes, storing the data as bits and hashes
    void traverseAndBuild(int height, unsigned int pos, const std::vector<uint256> &vTxid, const std::vector<bool> &vMatch);
    
    // recursive function that traverses tree nodes, consuming the bits and hashes produced by TraverseAndBuild.
    // it returns the hash of the respective node.
    uint256 traverseAndExtract(int height, unsigned int pos, unsigned int &nBitsUsed, unsigned int &nHashUsed, std::vector<uint256> &vMatch);
    
private:
    // the total number of transactions in the block
    unsigned int _transactions;
    
    // node-is-parent-of-matched-txid bits
    std::vector<bool> _matches;
    
    // txids and internal hashes
    std::vector<uint256> _hashes;
    
    // flag set when encountering invalid data
    bool _bad;
    
    typedef std::vector<std::pair<unsigned int, uint256> > MatchedTxes;
    MatchedTxes _matched_txes;
};

#endif // MERKLEBLOCK_H