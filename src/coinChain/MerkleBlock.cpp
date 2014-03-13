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

#include <coinChain/MerkleBlock.h>
#include <coinChain/BloomFilter.h>

using namespace std;

uint256 MerkleBlock::calcHash(int height, unsigned int pos, const std::vector<uint256> &vTxid) {
    if (height == 0) {
        // hash at height 0 is the txids themself
        return vTxid[pos];
    } else {
        // calculate left hash
        uint256 left = calcHash(height-1, pos*2, vTxid), right;
        // calculate right hash if not beyong the end of the array - copy left hash otherwise1
        if (pos*2+1 < calcTreeWidth(height-1))
            right = calcHash(height-1, pos*2+1, vTxid);
        else
            right = left;
        // combine subhashes
        return Hash(BEGIN(left), END(left), BEGIN(right), END(right));
    }
}

void MerkleBlock::traverseAndBuild(int height, unsigned int pos, const std::vector<uint256> &vTxid, const std::vector<bool> &vMatch) {
    // determine whether this node is the parent of at least one matched txid
    bool fParentOfMatch = false;
    for (unsigned int p = pos << height; p < (pos+1) << height && p < _transactions; p++)
        fParentOfMatch |= vMatch[p];
    // store as flag bit
    _matches.push_back(fParentOfMatch);
    if (height==0 || !fParentOfMatch) {
        // if at height 0, or nothing interesting below, store hash and stop
        _hashes.push_back(calcHash(height, pos, vTxid));
    } else {
        // otherwise, don't store any hash, but descend into the subtrees
        traverseAndBuild(height-1, pos*2, vTxid, vMatch);
        if (pos*2+1 < calcTreeWidth(height-1))
            traverseAndBuild(height-1, pos*2+1, vTxid, vMatch);
    }
}

uint256 MerkleBlock::traverseAndExtract(int height, unsigned int pos, unsigned int &nBitsUsed, unsigned int &nHashUsed, std::vector<uint256> &vMatch) {
    if (nBitsUsed >= _matches.size()) {
        // overflowed the bits array - failure
        _bad = true;
        return uint256(0);
    }
    bool fParentOfMatch = _matches[nBitsUsed++];
    if (height==0 || !fParentOfMatch) {
        // if at height 0, or nothing interesting below, use stored hash and do not descend
        if (nHashUsed >= _hashes.size()) {
            // overflowed the hash array - failure
            _bad = true;
            return uint256(0);
        }
        const uint256 &hash = _hashes[nHashUsed++];
        if (height==0 && fParentOfMatch) // in case of height 0, we have a matched txid
            vMatch.push_back(hash);
        return hash;
    } else {
        // otherwise, descend into the subtrees to extract matched txids and hashes
        uint256 left = traverseAndExtract(height-1, pos*2, nBitsUsed, nHashUsed, vMatch), right;
        if (pos*2+1 < calcTreeWidth(height-1))
            right = traverseAndExtract(height-1, pos*2+1, nBitsUsed, nHashUsed, vMatch);
        else
            right = left;
        // and combine them before returning
        return Hash(BEGIN(left), END(left), BEGIN(right), END(right));
    }
}

MerkleBlock::MerkleBlock(const Block& block, BloomFilter& filter) : BlockHeader(block), _bad(false) {
    
    vector<bool> vMatch;
    vector<uint256> vHashes;
    
    vMatch.reserve(block.getNumTransactions());
    vHashes.reserve(block.getNumTransactions());
    
    for (unsigned int i = 0; i < block.getNumTransactions(); i++) {
        const Transaction& txn = block.getTransaction(i);
        uint256 hash = txn.getHash();
        if (filter.isRelevantAndUpdate(txn)) {
            vMatch.push_back(true);
            _matched_txes.push_back(make_pair(i, hash));
        }
        else
            vMatch.push_back(false);
        vHashes.push_back(hash);
    }
    
    _transactions = vHashes.size();

    _matches.clear();
    _hashes.clear();
    
    // calculate height of tree
    int nHeight = 0;
    while (calcTreeWidth(nHeight) > 1)
        nHeight++;
    
    // traverse the partial tree
    traverseAndBuild(nHeight, 0, vHashes, vMatch);
}

//MerkleBlock::MerkleBlock() : BlockHeader(), _transactions(0), _bad(true) {}

uint256 MerkleBlock::extractMatches(std::vector<uint256> &vMatch) {
    vMatch.clear();
    // An empty set will not work
    if (_transactions == 0)
        return uint256(0);
    // check for excessively high numbers of transactions
    if (_transactions > MAX_BLOCK_SIZE / 60) // 60 is the lower bound for the size of a serialized CTransaction
        return uint256(0);
    // there can never be more hashes provided than one for every txid
    if (_hashes.size() > _transactions)
        return uint256(0);
    // there must be at least one bit per node in the partial tree, and at least one node per hash
    if (_matches.size() < _hashes.size())
        return uint256(0);
    // calculate height of tree
    int nHeight = 0;
    while (calcTreeWidth(nHeight) > 1)
        nHeight++;
    // traverse the partial tree
    unsigned int nBitsUsed = 0, nHashUsed = 0;
    uint256 hashMerkleRoot = traverseAndExtract(nHeight, 0, nBitsUsed, nHashUsed, vMatch);
    // verify that no problems occured during the tree traversal
    if (_bad)
        return uint256(0);
    // verify that all bits were consumed (except for the padding caused by serializing it as a byte sequence)
    if ((nBitsUsed+7)/8 != (_matches.size()+7)/8)
        return uint256(0);
    // verify that all hashes were consumed
    if (nHashUsed != _hashes.size())
        return uint256(0);
    return hashMerkleRoot;
}


