#include "btcNode/BlockChain.h"

#include "btcNode/Block.h"
#include "btcNode/BlockIndex.h"

#include "btcNode/main.h"

using namespace std;
using namespace boost;

int TxIndex::getDepthInMainChain() const
{
    // Read block header
    Block block;
    if (!__blockFile.readFromDisk(block, _pos.getFile(), _pos.getBlockPos(), false))
        return 0;
    // Find the block in the index
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(block.getHash());
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = (*mi).second;
    if (!pindex || !pindex->IsInMainChain())
        return 0;
    return 1 + nBestHeight - pindex->nHeight;
}




