
#include "btcNode/BlockFilter.h"
#include "btcNode/BlockChain.h"
#include "btcNode/Block.h"

#include "btcNode/net.h"

using namespace std;
using namespace boost;


bool BlockFilter::operator()(CNode* origin, Message& msg) {
    if (origin->nVersion == 0) {
        throw OriginNotReady();
    }
    if (msg.command() == "block") {
        Block block;
        CDataStream data(msg.payload());
        data >> block;
        
        printf("received block %s\n", block.getHash().toString().substr(0,20).c_str());
        // block.print();
        
        Inventory inv(MSG_BLOCK, block.getHash());
        origin->AddInventoryKnown(inv);
        
        if (processBlock(origin, block))
            _alreadyAskedFor.erase(inv);
    }
    else if (msg.command() == "getblocks") {
        CBlockLocator locator;
        uint256 hashStop;
        CDataStream data(msg.payload());

        data >> locator >> hashStop;
        
        // Find the last block the caller has in the main chain
        CBlockIndex* pindex = _blockChain.getBlockIndex(locator);
        
        // Send the rest of the chain
        if (pindex)
            pindex = pindex->pnext;
        int nLimit = 500 + _blockChain.getDistanceBack(locator);
        unsigned int nBytes = 0;
        printf("getblocks %d to %s limit %d\n", (pindex ? pindex->nHeight : -1), hashStop.toString().substr(0,20).c_str(), nLimit);
        for (; pindex; pindex = pindex->pnext) {
            if (pindex->GetBlockHash() == hashStop) {
                printf("  getblocks stopping at %d %s (%u bytes)\n", pindex->nHeight, pindex->GetBlockHash().toString().substr(0,20).c_str(), nBytes);
                break;
            }
            origin->PushInventory(Inventory(MSG_BLOCK, pindex->GetBlockHash()));
            Block block;
            _blockChain.getBlock(pindex->GetBlockHash(), block);
            nBytes += block.GetSerializeSize(SER_NETWORK);
            if (--nLimit <= 0 || nBytes >= SendBufferSize()/2) {
                // When this block is requested, we'll send an inv that'll make them
                // getblocks the next batch of inventory.
                printf("  getblocks stopping at limit %d %s (%u bytes)\n", pindex->nHeight, pindex->GetBlockHash().toString().substr(0,20).c_str(), nBytes);
                origin->hashContinue = pindex->GetBlockHash();
                break;
            }
        }
    }
    else if (msg.command() == "getheaders") {
        CBlockLocator locator;
        uint256 hashStop;
        CDataStream data(msg.payload());
        data >> locator >> hashStop;
        
        CBlockIndex* pindex = NULL;
        if (locator.IsNull()) {
            // If locator is null, return the hashStop block
            pindex = _blockChain.getHashStopIndex(hashStop);
            if (pindex == NULL) return true;
        }
        else {
            // Find the last block the caller has in the main chain
            pindex = _blockChain.getBlockIndex(locator);
            if (pindex)
                pindex = pindex->pnext;
        }
        
        vector<Block> vHeaders;
        int nLimit = 2000 + _blockChain.getDistanceBack(locator);
        printf("getheaders %d to %s limit %d\n", (pindex ? pindex->nHeight : -1), hashStop.toString().substr(0,20).c_str(), nLimit);
        for (; pindex; pindex = pindex->pnext) {
            vHeaders.push_back(pindex->GetBlockHeader());
            if (--nLimit <= 0 || pindex->GetBlockHash() == hashStop)
                break;
        }
        origin->PushMessage("headers", vHeaders);
    }
    else if (msg.command() == "getdata") { // server, or whatever instance that holds the inv map...
        vector<Inventory> vInv;
        CDataStream data(msg.payload());
        data >> vInv;
        if (vInv.size() > 50000)
            return error("message getdata size() = %d", vInv.size());
        
        BOOST_FOREACH(const Inventory& inv, vInv) {
            if (fShutdown)
                return true;
            printf("received getdata for: %s\n", inv.toString().c_str());
            
            if (inv.getType() == MSG_BLOCK) {
                // Send block from disk
                Block block;
                _blockChain.getBlock(inv.getHash(), block);
                if (!block.isNull()) {
                    origin->PushMessage("block", block);
                    
                    // Trigger them to send a getblocks request for the next batch of inventory
                    if (inv.getHash() == origin->hashContinue) {
                        // Bypass PushInventory, this must send even if redundant,
                        // and we want it right after the last block so they don't
                        // wait for other stuff first.
                        vector<Inventory> vInv;
                        vInv.push_back(Inventory(MSG_BLOCK, _blockChain.getBestChain()));
                        origin->PushMessage("inv", vInv);
                        origin->hashContinue = 0;
                    }
                }
            }
            // Track requests for our stuff
            //            Inventory(inv.hash);
        }
    }
    else if (msg.command() == "inv") {
        vector<Inventory> inventories;
        CDataStream data(msg.payload());
        data >> inventories;
        if (inventories.size() > 50000)
            return error("message inv size() = %d", inventories.size());
        
        BOOST_FOREACH(const Inventory& inv, inventories) {
            if (fShutdown)
                return true;
            
            if (inv.getType() == MSG_BLOCK) {                
                origin->AddInventoryKnown(inv);
                
                bool fAlreadyHave = alreadyHave(inv);
                printf("  got inventory: %s  %s\n", inv.toString().c_str(), fAlreadyHave ? "have" : "new");
                
                if (!fAlreadyHave)
                    origin->AskFor(inv);
                else if (_orphanBlocks.count(inv.getHash()))
                    origin->PushGetBlocks(_blockChain.getBestLocator(), getOrphanRoot(_orphanBlocks[inv.getHash()]));
            }
    
        }
        
    }

    return true;
}

// Private interface

uint256 BlockFilter::getOrphanRoot(const Block* pblock) {
    // Work back to the first block in the orphan chain
    while (_orphanBlocks.count(pblock->getPrevBlock()))
        pblock = _orphanBlocks[pblock->getPrevBlock()];
    return pblock->getHash();
}

/// Need access to mapOrphanBlocks/ByPrev and a call to GetOrphanRoot
bool BlockFilter::processBlock(CNode* origin, Block& block) {
    // Check for duplicate
    uint256 hash = block.getHash();
    if (_blockChain.haveBlock(hash))
        return error("ProcessBlock() : already have block %d %s", _blockChain.getBlockIndex(hash)->nHeight, hash.toString().substr(0,20).c_str());
    if (_orphanBlocks.count(hash))
        return error("ProcessBlock() : already have block (orphan) %s", hash.toString().substr(0,20).c_str());
    
    // Preliminary checks
    if (!block.checkBlock())
        return error("ProcessBlock() : CheckBlock FAILED");
    
    // If don't already have its previous block, shunt it off to holding area until we get it
    if (!_blockChain.haveBlock(block.getPrevBlock())) {
        printf("ProcessBlock: ORPHAN BLOCK, prev=%s\n", block.getPrevBlock().toString().substr(0,20).c_str());
        Block* block_copy = new Block(block);
        _orphanBlocks.insert(make_pair(hash, block_copy));
        _orphanBlocksByPrev.insert(make_pair(block_copy->getPrevBlock(), block_copy));
        
        // Ask this guy to fill in what we're missing
        if (origin)
            origin->PushGetBlocks(_blockChain.getBestLocator(), getOrphanRoot(block_copy));            
        return true;
    }
    
    // Store to disk
    if (!_blockChain.acceptBlock(block))
        return error("ProcessBlock() : AcceptBlock FAILED");
    
    // Recursively process any orphan blocks that depended on this one
    vector<uint256> workQueue;
    workQueue.push_back(hash);
    for (int i = 0; i < workQueue.size(); i++) {
        uint256 hashPrev = workQueue[i];
        for (multimap<uint256, Block*>::iterator mi = _orphanBlocksByPrev.lower_bound(hashPrev);
             mi != _orphanBlocksByPrev.upper_bound(hashPrev);
             ++mi) {
            Block* orphan = (*mi).second;
            if (_blockChain.acceptBlock(*orphan))
                workQueue.push_back(orphan->getHash());
            _orphanBlocks.erase(orphan->getHash());
            delete orphan;
        }
        _orphanBlocksByPrev.erase(hashPrev);
    }
    
    printf("ProcessBlock: ACCEPTED\n");
    return true;
}

bool BlockFilter::alreadyHave(const Inventory& inv) {
    if (inv.getType() == MSG_BLOCK)
        return _orphanBlocks.count(inv.getHash()) || _blockChain.haveBlock(inv.getHash());
    else // Don't know what it is, just say we already got one
        return true;
}

