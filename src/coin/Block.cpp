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

#include <coin/Block.h>
#include <coin/Logger.h>

using namespace std;
using namespace boost;

int Block::GetSigOpCount() const
{
    int n = 0;
    BOOST_FOREACH(const Transaction& tx, _transactions)
    n += tx.getSigOpCount();
    return n;
}

ostream& operator<<(ostream& os, const Block& b) {
    os << (BlockHeader)b;
    if (!b._ignore_aux_pow) os << b._aux_pow;
    return os << b._transactions;
}

istream& operator>>(istream& is, Block& b) {
    is >> (BlockHeader&)b;
    if (!b._ignore_aux_pow) is >> b._aux_pow;
    return is >> b._transactions;
}

uint256 Block::buildMerkleTree() const
{
    _merkleTree.clear();
    BOOST_FOREACH(const Transaction& tx, _transactions)
    _merkleTree.push_back(tx.getHash());
    int j = 0;
    for (int size = _transactions.size(); size > 1; size = (size + 1) / 2) {
        for (int i = 0; i < size; i += 2) {
            int i2 = std::min(i+1, size-1);
            _merkleTree.push_back(Hash(BEGIN(_merkleTree[j+i]),  END(_merkleTree[j+i]),
                                       BEGIN(_merkleTree[j+i2]), END(_merkleTree[j+i2])));
        }
        j += size;
    }
    uint256 merkleRoot = (_merkleTree.empty() ? uint256(0) : _merkleTree.back());
    return merkleRoot;
}

MerkleBranch Block::getMerkleBranch(int index) const
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

uint256 Block::checkMerkleBranch(uint256 hash, const MerkleBranch& merkleBranch, int index)
{
    if (index == -1)
        return uint256(0);
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

void Block::print() const
{
    log_info("Block(hash=%s, ver=%d, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%d)\n",
           getHash().toString().substr(0,20).c_str(),
           _version,
           _prevBlock.toString().substr(0,20).c_str(),
           _merkleRoot.toString().substr(0,10).c_str(),
           _time, _bits, _nonce,
           _transactions.size());
    for (int i = 0; i < _transactions.size(); i++)
        {
        log_info("  ");
        _transactions[i].print();
        }
    log_info("  vMerkleTree: ");
    for (int i = 0; i < _merkleTree.size(); i++)
        log_info("%s ", _merkleTree[i].toString().substr(0,10).c_str());
    log_info("\n");
}

//bool Block::checkBlock(const CBigNum& proofOfWorkLimit) const

bool Block::checkHeightInCoinbase(int height) const {
    Script coinbase = getTransaction(0).getInput(0).signature();
    Script expect = Script() << height;
    return std::equal(expect.begin(), expect.end(), coinbase.begin());
}

int Block::getHeight() const {
    Script coinbase = getTransaction(0).getInput(0).signature();
    Script::const_iterator cbi = coinbase.begin();
    opcodetype opcode;
    std::vector<unsigned char> data;
    // We simply ignore the first opcode and data, however, it is the height...
    coinbase.getOp(cbi, opcode, data);
    if (opcode < OP_PUSHDATA1) {
        CBigNum height;
        height.setvch(data);
        int h = height.getint();
        return h;
    }
    return 0;
}

bool Block::checkSpendablesRootInCoinbase(uint256 hash) const {
    try {
        Script coinbase = getTransaction(0).getInput(0).signature();
        Script::const_iterator cbi = coinbase.begin();
        opcodetype opcode;
        std::vector<unsigned char> data;
        // We simply ignore the first opcode and data, however, it is the height...
        coinbase.getOp(cbi, opcode, data);
        
        // this should be an opcode for a number
        coinbase.getOp(cbi, opcode, data);
        uint256 hash_from_coinbase(data);
        return (hash == hash_from_coinbase);
    } catch (...) {
        return false;
    }
}

uint256 Block::getSpendablesRoot() const {
    try {
        Script coinbase = getTransaction(0).getInput(0).signature();
        Script::const_iterator cbi = coinbase.begin();
        opcodetype opcode;
        std::vector<unsigned char> data;
        // We simply ignore the first opcode and data, however, it is the height...
        coinbase.getOp(cbi, opcode, data);
        
        // this should be an opcode for a number
        coinbase.getOp(cbi, opcode, data);
        uint256 hash_from_coinbase(data);
        return hash_from_coinbase;
    } catch (...) {
        return uint256(0);
    }
}