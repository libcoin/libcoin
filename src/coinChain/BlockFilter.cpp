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
#include <coinChain/MerkleBlock.h>

#include <coin/Block.h>
#include <coin/Logger.h>
#include <coinChain/Peer.h>
#include <coinChain/Inventory.h>

using namespace std;
using namespace boost;


size_t SendBufferSize() { return 10*1000*1000; }

bool BlockFilter::operator()(Peer* origin, Message& msg) {
    if (!origin->initialized()) {
        throw OriginNotReady();
    }
    if (msg.command() == "block") {
        Block block;
        if (_blockChain.chain().adhere_aux_pow()) block.adhere_aux_pow();
        istringstream is(msg.payload());
        is >> block;
        
        log_debug("received block %s", block.getHash().toString().substr(0,20).c_str());
        
        Inventory inv(block);
        origin->AddInventoryKnown(inv);
        
        // Check for duplicate
        uint256 hash = inv.getHash();
        if (_blockChain.haveBlock(hash))
            return error("ProcessBlock() : already have block %d %s", _blockChain.getHeight(hash), hash.toString().substr(0,20).c_str());
    
        if (_orphans.count(hash))
            return error("ProcessBlock() : already have block (orphan) %s", hash.toString().substr(0,20).c_str());
        
        // Preliminary checks
        //        if (!block.checkBlock(_blockChain.chain().proofOfWorkLimit()))
        try {
            _blockChain.chain().check(block);
        }
        catch (std::exception& e ) {
            log_error("BlockFilter()(): %s", e.what());
            return false;
        }
        
        // as the block is valid we obviously got what we asked for and we can delete the request (no matter if the block was useful or not)
        origin->dequeue(inv);
        
        if (!_blockChain.chain().checkProofOfWork(block))
            return error("ProcessBlock() : CheckProofOfWork FAILED");
        
        // If don't already have its previous block, shunt it off to holding area until we get it
        if (!_blockChain.haveBlock(block.getPrevBlock())) {
            log_debug("ProcessBlock: ORPHAN BLOCK, prev=%s", block.getPrevBlock().toString().substr(0,20).c_str());
            pair<Orphans::iterator, bool> ret = _orphans.insert(make_pair(hash, block));
            _orphansByPrev.insert(make_pair(block.getPrevBlock(), ret.first));
            
            // Ask this guy to fill in what we're missing
            if (origin)
                origin->PushGetBlocks(_blockChain.getBestLocator(), getOrphanRoot(block));

            return false; // don't process orphan blocks
        }
        
        // the block is now part of the chain and has a valid proof of work
        
        process(block, origin->getAllPeers());
    }
    else if (msg.command() == "getblocks") { // here we should simply ignore requests below
        int version;
        BlockLocator locator;
        uint256 hashStop;
        istringstream is(msg.payload());
        is >> binary<int>(version) >> locator >> hashStop;
        
        // Find the last block the caller has in the main chain
        BlockIterator blk = _blockChain.iterator(locator);
        
        // Send the rest of the chain
        int nLimit = 500 + _blockChain.getDistanceBack(locator);
        unsigned int nBytes = 0;
        int height = blk.height() + 1;
        log_debug("getblocks %d to %s limit %d", height, hashStop.toString().substr(0,20).c_str(), nLimit);
        for (++blk; !!blk; ++blk) {
            uint256 hash = blk->hash;
            height = blk.height();
            if (height < _blockChain.getDeepestDepth()) {
                log_debug("  getblocks stopping at %d - no more blocks", height);
                break;
            }
            if (hash == hashStop) {
                log_debug("  getblocks stopping at %d %s (%u bytes)", height, hash.toString().substr(0,20).c_str(), nBytes);
                break;
            }
            origin->push(Inventory(MSG_BLOCK, hash));
            Block block;
            _blockChain.getBlock(blk, block);            
            nBytes += serialize_size(block);//block.GetSerializeSize(SER_NETWORK);
            if (--nLimit <= 0 || nBytes >= SendBufferSize()/2) {
                // When this block is requested, we'll send an inv that'll make them
                // getblocks the next batch of inventory.
                log_debug("  getblocks stopping at limit %d %s (%u bytes)", height, hash.toString().substr(0,20).c_str(), nBytes);
                origin->hashContinue = hash;
                break;
            }
        }
    }
    else if (msg.command() == "getheaders") {
        int version;
        BlockLocator locator;
        uint256 hashStop;

        istringstream is(msg.payload());
        is >> binary<int>(version) >> locator >> hashStop;

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
        log_debug("getheaders %d to %s limit %d", (!!blk ? height : -1), hashStop.toString().substr(0,20).c_str(), nLimit);
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
        istringstream is(msg.payload());
        is >> vInv;
        if (vInv.size() > 50000)
            return error("message getdata size() = %d", vInv.size());
        
        BOOST_FOREACH(const Inventory& inv, vInv) {
            log_debug("received getdata for: %s", inv.toString().c_str());
            
            if (inv.getType() == MSG_BLOCK || inv.getType() == MSG_FILTERED_BLOCK) {
                // Send block from disk
                Block block;
                _blockChain.getBlock(inv.getHash(), block);
                if (!block.isNull()) {
                    if (inv.getType() == MSG_BLOCK)
                        origin->PushMessage("block", block);
                    else {
                        MerkleBlock merkleBlock(block, origin->filter);
                        origin->PushMessage("merkleblock", merkleBlock);
                        // MerkleBlock just contains hashes, so also push any transactions in the block the client did not see
                        // This avoids hurting performance by pointlessly requiring a round-trip
                        // Note that there is currently no way for a node to request any single transactions we didnt send here -
                        // they must either disconnect and retry or request the full block.
                        // Thus, the protocol spec specified allows for us to provide duplicate txn here,
                        // however we MUST always provide at least what the remote peer needs
                        for (size_t i = 0; i < merkleBlock.getNumFilteredTransactions(); ++i) {
                            const Transaction& txn = block.getTransaction(merkleBlock.getTransactionBlockIndex(i));
                            Inventory inv(txn);
                            if (!origin->setInventoryKnown.count(inv))
                                origin->push(inv);
                        }
                    }
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
        istringstream is(msg.payload());
        is >> inventories;
        if (inventories.size() > 50000)
            return error("message inv size() = %d", inventories.size());
        
        BOOST_FOREACH(const Inventory& inv, inventories) {
            if (inv.getType() == MSG_BLOCK) {                
                origin->AddInventoryKnown(inv);
                
                bool fAlreadyHave = alreadyHave(inv);
                log_debug("  got inventory: %s  %s", inv.toString().c_str(), fAlreadyHave ? "have" : "new");
                
                if (!fAlreadyHave)
                    origin->queue(inv);
                else if (_orphans.count(inv.getHash()))
                    origin->PushGetBlocks(_blockChain.getBestLocator(), getOrphanRoot(_orphans[inv.getHash()]));
            }
    
        }
    }
    else if (msg.command() == "version") {
        // Ask the first connected node for block updates
        static int nAskedForBlocks = 0;
        if (!origin->client() && (origin->version() < 32000 || origin->version() >= 32400) && (nAskedForBlocks < 1 || origin->getAllPeers().size() <= 1)) {
            nAskedForBlocks++;
            origin->PushGetBlocks(_blockChain.getBestLocator(), uint256(0));
        }
    }

    return true;
}

// Private interface

uint256 BlockFilter::getOrphanRoot(const Block& block) {
    // Work back to the first block in the orphan chain
    Block b = block;
    while (_orphans.count(b.getPrevBlock()))
        b = _orphans[b.getPrevBlock()];
    return b.getHash();
}

/// Need access to mapOrphanBlocks/ByPrev and a call to GetOrphanRoot
void BlockFilter::process(const Block& block, Peers peers) {
    uint256 hash = block.getHash();
    // Store in DB
    try {
        _blockChain.append(block);
        // notify all listeners
        for(Listeners::iterator listener = _listeners.begin(); listener != _listeners.end(); ++listener)
            (*listener->get())(block);
        log_debug("ProcessBlock: ACCEPTED");
    }
    catch (std::exception &e) {
        log_error("append(Block) failed: %s", e.what());
    }

    // Relay inventory, but don't relay old inventory during initial block download
    uint256 bestChain = _blockChain.getBestChain();
    if (bestChain == hash) {
        for(Peers::iterator peer = peers.begin(); peer != peers.end(); ++peer)
            if (_blockChain.getBestHeight() > ((*peer)->getStartingHeight() != -1 ? (*peer)->getStartingHeight() - 2000 : _blockChain.getTotalBlocksEstimate()))
                (*peer)->push(Inventory(MSG_BLOCK, hash));
    }
    
    // Recursively process any orphan blocks that depended on this one
    vector<uint256> workQueue;
    workQueue.push_back(hash);
    for (unsigned int i = 0; i < workQueue.size(); i++) {
        uint256 hashPrev = workQueue[i];
        for (OrphansByPrev::iterator mi = _orphansByPrev.lower_bound(hashPrev);
             mi != _orphansByPrev.upper_bound(hashPrev);
             ++mi) {
            const Block& orphan = mi->second->second;
            try {
                _blockChain.append(orphan);
                // notify all listeners only if append does not throw
                for(Listeners::iterator listener = _listeners.begin(); listener != _listeners.end(); ++listener)
                    (*listener->get())(block);
                log_debug("ProcessBlock: ACCEPTED ORPHAN");
            }
            catch (std::exception &e) {
                log_warn("append(Block) orphan failed: %s", e.what());
            }
            
            workQueue.push_back(orphan.getHash());
            // Relay inventory, but don't relay old inventory during initial block download
            uint256 bestChain = _blockChain.getBestChain();
            uint256 blockHash = block.getHash();
            if (bestChain == blockHash) {
                for(Peers::iterator peer = peers.begin(); peer != peers.end(); ++peer)
                    if (_blockChain.getBestHeight() > ((*peer)->getStartingHeight() != -1 ? (*peer)->getStartingHeight() - 2000 : _blockChain.getTotalBlocksEstimate()))
                        (*peer)->push(Inventory(MSG_BLOCK, blockHash));
            }
            _orphans.erase(orphan.getHash());
        }
        _orphansByPrev.erase(hashPrev);
    }
    //    _blockChain.outputPerformanceTimings();
}

bool BlockFilter::alreadyHave(const Inventory& inv) {
    if (inv.getType() == MSG_BLOCK)
        return _orphans.count(inv.getHash()) || _blockChain.haveBlock(inv.getHash());
    else // Don't know what it is, just say we already got one
        return true;
}


bool ShareFilter::process(const Block& block, Peers peers) {
    //    uint256 hash = block.getHash();
    try {
        _blockChain.append(block);
    }
    catch (std::exception &e) {
        return error((string("append(Block) failed: ") + e.what()).c_str());
    }
    // notify all listeners
    for(Listeners::iterator listener = _listeners.begin(); listener != _listeners.end(); ++listener)
        (*listener->get())(block);
    
    log_debug("ProcessShare: ACCEPTED");
    return true;
}

