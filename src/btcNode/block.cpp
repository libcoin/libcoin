
#include "btcNode/Block.h"

#include "btcNode/db.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

using namespace std;
using namespace boost;

uint256 Block::getHash() const
{
    return Hash(BEGIN(_version), END(_nonce));
}

int Block::GetSigOpCount() const
{
    int n = 0;
    BOOST_FOREACH(const Transaction& tx, _transactions)
    n += tx.GetSigOpCount();
    return n;
}

uint256 Block::buildMerkleTree(bool genesisBlock)
{
    _merkleTree.clear();
    BOOST_FOREACH(const Transaction& tx, _transactions)
    _merkleTree.push_back(tx.GetHash());
    int j = 0;
    for (int size = _transactions.size(); size > 1; size = (size + 1) / 2) {
        for (int i = 0; i < size; i += 2) {
            int i2 = std::min(i+1, size-1);
            _merkleTree.push_back(Hash(BEGIN(_merkleTree[j+i]),  END(_merkleTree[j+i]),
                                       BEGIN(_merkleTree[j+i2]), END(_merkleTree[j+i2])));
        }
        j += size;
    }
    uint256 merkleRoot = (_merkleTree.empty() ? 0 : _merkleTree.back());
    if (genesisBlock) _merkleRoot = merkleRoot;
    return merkleRoot;
}

std::vector<uint256> Block::getMerkleBranch(int index)
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
        return 0;
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
    printf("Block(hash=%s, ver=%d, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%d)\n",
           getHash().toString().substr(0,20).c_str(),
           _version,
           _prevBlock.toString().substr(0,20).c_str(),
           _merkleRoot.toString().substr(0,10).c_str(),
           _time, _bits, _nonce,
           _transactions.size());
    for (int i = 0; i < _transactions.size(); i++)
        {
        printf("  ");
        _transactions[i].print();
        }
    printf("  vMerkleTree: ");
    for (int i = 0; i < _merkleTree.size(); i++)
        printf("%s ", _merkleTree[i].toString().substr(0,10).c_str());
    printf("\n");
}

bool Block::checkBlock(const CBigNum& proofOfWorkLimit)
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
    if (_transactions.empty() || !_transactions[0].IsCoinBase())
        return error("CheckBlock() : first tx is not coinbase");
    for (int i = 1; i < _transactions.size(); i++)
        if (_transactions[i].IsCoinBase())
            return error("CheckBlock() : more than one coinbase");
    
    // Check transactions
    BOOST_FOREACH(const Transaction& tx, _transactions)
    if (!tx.CheckTransaction())
        return error("CheckBlock() : CheckTransaction failed");
    
    // Check that it's not full of nonstandard transactions
    if (GetSigOpCount() > MAX_BLOCK_SIGOPS)
        return error("CheckBlock() : too many nonstandard transactions");
    
    // Check merkleroot
    if (_merkleRoot != buildMerkleTree())
        return error("CheckBlock() : hashMerkleRoot mismatch");
    
    return true;
}

