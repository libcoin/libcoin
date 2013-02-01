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

uint256 Block::getHash() const
{
    return Hash(BEGIN(_version), END(_nonce));
}

template<typename T1>
inline uint256 HalfHash(const T1 pbegin, const T1 pend)
{
    static unsigned char pblank[1];
    uint256 hash1;
    SHA256((pbegin == pend ? pblank : (unsigned char*)&pbegin[0]), (pend - pbegin) * sizeof(pbegin[0]), (unsigned char*)&hash1);
    return hash1;
}

uint256 Block::getHalfHash() const
{
    return HalfHash(BEGIN(_version), END(_nonce));
}


int Block::GetSigOpCount() const
{
    int n = 0;
    BOOST_FOREACH(const Transaction& tx, _transactions)
    n += tx.getSigOpCount();
    return n;
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

std::vector<uint256> Block::getMerkleBranch(int index) const
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

uint256 Block::checkMerkleBranch(uint256 hash, const std::vector<uint256>& merkleBranch, int index)
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

bool Block::checkBlock(const CBigNum& proofOfWorkLimit) const
{
    // These are checks that are independent of context
    // that can be verified before saving an orphan block.
    
    // Size limits
    if (_transactions.empty() || _transactions.size() > MAX_BLOCK_SIZE || ::GetSerializeSize(*this, SER_NETWORK) > MAX_BLOCK_SIZE)
        return error("CheckBlock() : size limits failed");
    
    // Check proof of work matches claimed amount
    if (!checkProofOfWork(proofOfWorkLimit))
        return error("CheckBlock() : proof of work failed");
    
    // Check timestamp
    if (getBlockTime() > GetAdjustedTime() + 2 * 60 * 60)
        return error("CheckBlock() : block timestamp too far in the future");
    
    // First transaction must be coinbase, the rest must not be
    if (_transactions.empty() || !_transactions[0].isCoinBase())
        return error("CheckBlock() : first tx is not coinbase");
    for (int i = 1; i < _transactions.size(); i++)
        if (_transactions[i].isCoinBase())
            return error("CheckBlock() : more than one coinbase");
    
    // Check transactions
    BOOST_FOREACH(const Transaction& tx, _transactions)
    if (!tx.checkTransaction())
        return error("CheckBlock() : checkTransaction failed");
    
    // Check that it's not full of nonstandard transactions
    if (GetSigOpCount() > MAX_BLOCK_SIGOPS)
        return error("CheckBlock() : too many nonstandard transactions");
    
    // Check merkleroot
    if (_merkleRoot != buildMerkleTree())
        return error("CheckBlock() : hashMerkleRoot mismatch");
    
    return true;
}

bool Block::checkHeightInCoinbase(int height) const {
    Script coinbase = getTransaction(0).getInput(0).signature();
    Script expect = Script() << height;
    return std::equal(expect.begin(), expect.end(), coinbase.begin());
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
