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

#include <coinChain/BlockFilter.h>
#include <coinChain/BlockChain.h>
#include <coin/Block.h>
#include <coin/Logger.h>
#include <coinChain/Peer.h>
#include <coinChain/Inventory.h>

using namespace std;
using namespace boost;


size_t SendBufferSize() { return 10*1000*1000; }

bool BlockFilter::operator()(Peer* origin, Message& msg) {
    if (origin->nVersion == 0) {
        throw OriginNotReady();
    }
    if (msg.command() == "block") {
        Block block;
        CDataStream data(msg.payload());
        data >> block;
        
        log_debug("received block %s\n", block.getHash().toString().substr(0,20).c_str());
        
        Inventory inv(MSG_BLOCK, block.getHash());
        origin->AddInventoryKnown(inv);
        
        // Check for duplicate
        uint256 hash = block.getHash();
        if (_blockChain.haveBlock(hash))
            return error("ProcessBlock() : already have block %d %s", _blockChain.getHeight(hash), hash.toString().substr(0,20).c_str());
    
        if (_orphanBlocks.count(hash))
            return error("ProcessBlock() : already have block (orphan) %s", hash.toString().substr(0,20).c_str());
        
        // Preliminary checks
        //        if (!block.checkBlock(_blockChain.chain().proofOfWorkLimit()))
        if (!block.checkBlock())
            return error("ProcessBlock() : CheckBlock FAILED");
        
        if (!_blockChain.chain().checkProofOfWork(block))
            return error("ProcessBlock() : CheckProofOfWork FAILED");
        
        // If don't already have its previous block, shunt it off to holding area until we get it
        if (!_blockChain.haveBlock(block.getPrevBlock())) {
            log_info("ProcessBlock: ORPHAN BLOCK, prev=%s\n", block.getPrevBlock().toString().substr(0,20).c_str());
            Block* block_copy = new Block(block);
            _orphanBlocks.insert(make_pair(hash, block_copy));
            _orphanBlocksByPrev.insert(make_pair(block_copy->getPrevBlock(), block_copy));
            
            // Ask this guy to fill in what we're missing
            if (origin)
                origin->PushGetBlocks(_blockChain.getBestLocator(), getOrphanRoot(block_copy));            
        }
        
        if (process(block, origin->getAllPeers()))
            _alreadyAskedFor.erase(inv);
    }
    else if (msg.command() == "getblocks") {
        BlockLocator locator;
        uint256 hashStop;
        CDataStream data(msg.payload());

        data >> locator >> hashStop;
        
        // Find the last block the caller has in the main chain
        BlockIterator blk = _blockChain.iterator(locator);
        
        // Send the rest of the chain
        int nLimit = 500 + _blockChain.getDistanceBack(locator);
        unsigned int nBytes = 0;
        int height = blk.height() + 1;
        log_debug("getblocks %d to %s limit %d\n", height, hashStop.toString().substr(0,20).c_str(), nLimit);
        for (++blk; !!blk; ++blk) {
            uint256 hash = blk->hash;
            height = blk.height();
            if (hash == hashStop) {
                log_debug("  getblocks stopping at %d %s (%u bytes)\n", height, hash.toString().substr(0,20).c_str(), nBytes);
                break;
            }
            origin->PushInventory(Inventory(MSG_BLOCK, hash));
            Block block;
            _blockChain.getBlock(blk, block);
            nBytes += block.GetSerializeSize(SER_NETWORK);
            if (--nLimit <= 0 || nBytes >= SendBufferSize()/2) {
                // When this block is requested, we'll send an inv that'll make them
                // getblocks the next batch of inventory.
                log_debug("  getblocks stopping at limit %d %s (%u bytes)\n", height, hash.toString().substr(0,20).c_str(), nBytes);
                origin->hashContinue = hash;
                break;
            }
        }
    }
    else if (msg.command() == "getheaders") {
        BlockLocator locator;
        uint256 hashStop;
        CDataStream data(msg.payload());
        data >> locator >> hashStop;
        
        BlockIterator blk;
        if (locator.IsNull()) {
            // If locator is null, return the hashStop block
            blk = _blockChain.iterator(hashStop);
            if (!blk) return true;
        }
        else {
            // Find the last block the caller has in the main chain
            blk = _blockChain.iterator(locator);
            if (!!blk)
                ++blk;
        }
        
        vector<Block> vHeaders;
        int nLimit = 2000 + _blockChain.getDistanceBack(locator);
        int height = blk.height();
        log_debug("getheaders %d to %s limit %d\n", (!!blk ? height : -1), hashStop.toString().substr(0,20).c_str(), nLimit);
        for (; !!blk; ++blk) {
            Block block;
            _blockChain.getBlockHeader(blk, block);
            vHeaders.push_back(block);
            if (--nLimit <= 0 || blk->hash == hashStop)
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
            log_debug("received getdata for: %s\n", inv.toString().c_str());
            
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
            if (inv.getType() == MSG_BLOCK) {                
                origin->AddInventoryKnown(inv);
                
                bool fAlreadyHave = alreadyHave(inv);
                log_debug("  got inventory: %s  %s\n", inv.toString().c_str(), fAlreadyHave ? "have" : "new");
                
                if (!fAlreadyHave)
                    origin->AskFor(inv);
                else if (_orphanBlocks.count(inv.getHash()))
                    origin->PushGetBlocks(_blockChain.getBestLocator(), getOrphanRoot(_orphanBlocks[inv.getHash()]));
            }
    
        }
    }
    else if (msg.command() == "version") {
        // Ask the first connected node for block updates
        static int nAskedForBlocks = 0;
        if (!origin->fClient && (origin->nVersion < 32000 || origin->nVersion >= 32400) && (nAskedForBlocks < 1 || origin->getAllPeers().size() <= 1)) {
            nAskedForBlocks++;
            origin->PushGetBlocks(_blockChain.getBestLocator(), uint256(0));
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
bool BlockFilter::process(const Block& block, Peers peers) {
    uint256 hash = block.getHash();
    // Store in DB
    try {
        _blockChain.append(block);
    }
    catch (std::exception &e) {
        return error((string("append(Block) failed: ") + e.what()).c_str());
    }
    // notify all listeners
    for(Listeners::iterator listener = _listeners.begin(); listener != _listeners.end(); ++listener)
        (*listener->get())(block);

    // Relay inventory, but don't relay old inventory during initial block download
    uint256 bestChain = _blockChain.getBestChain();
    if (bestChain == hash) {
        for(Peers::iterator peer = peers.begin(); peer != peers.end(); ++peer)
            if (_blockChain.getBestHeight() > ((*peer)->getStartingHeight() != -1 ? (*peer)->getStartingHeight() - 2000 : _blockChain.getTotalBlocksEstimate()))
                (*peer)->PushInventory(Inventory(MSG_BLOCK, hash));
    }
    
    // Recursively process any orphan blocks that depended on this one
    vector<uint256> workQueue;
    workQueue.push_back(hash);
    for (int i = 0; i < workQueue.size(); i++) {
        uint256 hashPrev = workQueue[i];
        for (multimap<uint256, Block*>::iterator mi = _orphanBlocksByPrev.lower_bound(hashPrev);
             mi != _orphanBlocksByPrev.upper_bound(hashPrev);
             ++mi) {
            Block* orphan = (*mi).second;
            try {
                _blockChain.append(*orphan);
                // notify all listeners
                for(Listeners::iterator listener = _listeners.begin(); listener != _listeners.end(); ++listener)
                    (*listener->get())(block);
                
                workQueue.push_back(orphan->getHash());
                // Relay inventory, but don't relay old inventory during initial block download
                uint256 bestChain = _blockChain.getBestChain();
                uint256 blockHash = block.getHash();
                if (bestChain == blockHash) {
                    for(Peers::iterator peer = peers.begin(); peer != peers.end(); ++peer)
                        if (_blockChain.getBestHeight() > ((*peer)->getStartingHeight() != -1 ? (*peer)->getStartingHeight() - 2000 : _blockChain.getTotalBlocksEstimate()))
                            (*peer)->PushInventory(Inventory(MSG_BLOCK, blockHash));
                }
            }
            catch (std::exception &e) {
                log_warn((string("append(Block) orphan failed: ") + e.what()).c_str());
            }
            _orphanBlocks.erase(orphan->getHash());
            delete orphan;
        }
        _orphanBlocksByPrev.erase(hashPrev);
    }
    //    _blockChain.outputPerformanceTimings();
    log_debug("ProcessBlock: ACCEPTED\n");
    return true;
}

/*
void BlockFilter::pushBlock(Block& block)
{
    _block_queue.push_back(block);
    _blockProcessor
    
}
*/
 
bool BlockFilter::alreadyHave(const Inventory& inv) {
    if (inv.getType() == MSG_BLOCK)
        return _orphanBlocks.count(inv.getHash()) || _blockChain.haveBlock(inv.getHash());
    else // Don't know what it is, just say we already got one
        return true;
}


bool ShareFilter::process(const Block& block, Peers peers) {
    uint256 hash = block.getHash();
    try {
        _blockChain.append(block);
    }
    catch (std::exception &e) {
        return error((string("append(Block) failed: ") + e.what()).c_str());
    }
    // notify all listeners
    for(Listeners::iterator listener = _listeners.begin(); listener != _listeners.end(); ++listener)
        (*listener->get())(block);
    
    log_debug("ProcessBlock: ACCEPTED\n");
    return true;
}

