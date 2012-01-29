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

#include "coin/Block.h"
#include "coinChain/BlockIndex.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

using namespace std;
using namespace boost;

CBlockIndex::CBlockIndex(unsigned int nFileIn, unsigned int nBlockPosIn, const Block& block)
{
    phashBlock = NULL;
    pprev = NULL;
    pnext = NULL;
    nFile = nFileIn;
    nBlockPos = nBlockPosIn;
    nHeight = 0;
    bnChainWork = 0;
    
    nVersion       = block.getVersion();
    hashMerkleRoot = block.getMerkleRoot();
    nTime          = block.getTime();
    nBits          = block.getBits();
    nNonce         = block.getNonce();
}

bool CBlockIndex::checkIndex(const CBigNum& proofOfWorkLimit) const {
    Block block(nVersion, pprev ? pprev->GetBlockHash() : 0, hashMerkleRoot, nTime, nBits, nNonce);
    return block.checkProofOfWork(proofOfWorkLimit);
}

Block CBlockIndex::GetBlockHeader() const
{
    uint256 prevBlock;
    if (pprev)
        prevBlock = pprev->GetBlockHash();
    Block block(nVersion, prevBlock, hashMerkleRoot, nTime, nBits, nNonce);
    return block;
}


uint256 CDiskBlockIndex::GetBlockHash() const
{
    Block block(nVersion, hashPrev, hashMerkleRoot, nTime, nBits, nNonce);
    return block.getHash();
}

bool operator==(const CBlockLocator& a, const CBlockLocator& b) {
    return a.vHave == b.vHave;
}

