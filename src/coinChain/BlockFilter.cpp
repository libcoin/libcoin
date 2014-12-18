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
        if (_blockChain.haveBlock(hash)) {
            log_error("ProcessBlock() : already have block %d %s", _blockChain.getHeight(hash), hash.toString().substr(0,20).c_str());
            return false;
        }
    
        if (_orphans.count(hash)) {
            log_error("ProcessBlock() : already have block (orphan) %s", hash.toString().substr(0,20).c_str());
            return false;
        }
        
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
        
        if (!_blockChain.chain().checkProofOfWork(block)) {
            log_error("ProcessBlock() : CheckProofOfWork FAILED");
            return false;
        }
        
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
        
        // check if the block belongs to the invalid chain - if so, we should try to reapply the invalid chain again - might seem strange, but in case of random DB errors it does make sense - we warn as this should not happen and also we should warn in case of failure again - as it could indicate that we are on the wrong chain.
        /*
        if (_blockChain.isInvalid(block.getPrevBlock())) {
            log_warn("The invalid chain is growing!");
            // attempt to recommit the entire invalid chain - if it fails announce!
            // iterate backwards until we find the main chain
            _branches.find(block.getPrevBlock());
        }
        */
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
                origin->setContinue(hash);
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

        // Find the last block the caller has in the main chain
        BlockIterator blk = _blockChain.iterator(locator);

        vector<BlockHeader> headers;
        int limit = 2000 + _blockChain.getDistanceBack(locator);
        int height = blk.height() + 1;
        log_debug("getheaders %d to %s limit %d", height, hashStop.toString().substr(0,20).c_str(), limit);
        for (++blk; !!blk; ++blk) {
            uint256 hash = blk->hash;
            height = blk.height();
            if (height < _blockChain.getDeepestDepth()) {
                log_debug("  getheaders stopping at %d - no more blocks", height);
                break;
            }
            if (hash == hashStop) {
                log_debug("  getheaders stopping at %d %s", height, hash.toString().substr(0,20).c_str());
                break;
            }
            BlockHeader header;

            _blockChain.getBlockHeader(blk, header);
            headers.push_back(header);
            if (--limit <= 0 || blk->hash == hashStop)
                break;
        }
        origin->PushMessage("headers", headers);
    }
    else if (msg.command() == "getdata") { // server, or whatever instance that holds the inv map...
        vector<Inventory> vInv;
        istringstream is(msg.payload());
        is >> vInv;
        if (vInv.size() > 50000) {
            log_error("message getdata size() = %d", vInv.size());
            return false;
        }
        
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
                            if (!origin->known(inv))
                                origin->push(txn);
                        }
                    }
                    // tickle:
                    // Trigger them to send a getblocks request for the next batch of inventory
                    if (inv.getHash() == origin->getContinue()) {
                        // Bypass PushInventory, this must send even if redundant,
                        // and we want it right after the last block so they don't
                        // wait for other stuff first.
                        vector<Inventory> vInv;
                        vInv.push_back(Inventory(MSG_BLOCK, _blockChain.getBestChain()));
                        origin->PushMessage("inv", vInv);
                        origin->setContinue(uint256(0));
                    }
                }
            }
            else if (_support_normalized && inv.getType() == MSG_NORMALIZED_BLOCK) {
                Block block;
                BlockChain::Redeemed redeemed;
                _blockChain.getBlock(inv.getHash(), block, redeemed);
                ostringstream os;
                os << inv.getHash();
                os << (BlockHeader)block;
                os << const_varint(block.getNumTransactions()); // the is the number of transaction
                // now iterate over transactions and the redeemed outputs
                os << block.getTransaction(0).getHash();
                os << const_varint((int64_t)0); // this is an empty redeemed output
                os << block.getTransaction(0); // nothing to redeem for a coinbase
                for (size_t i = 1; i < block.getNumTransactions(); ++i) {
                    os << block.getTransaction(i).getHash();
                    os << redeemed[i-1];
                    os << block.getTransaction(i);
                }
                origin->push("normblock", os.str());
            }
            // tickle:
            // Trigger them to send a getblocks request for the next batch of inventory
            if (inv.getHash() == origin->getContinue()) {
                // Bypass PushInventory, this must send even if redundant,
                // and we want it right after the last block so they don't
                // wait for other stuff first.
                vector<Inventory> vInv;
                vInv.push_back(Inventory(MSG_BLOCK, _blockChain.getBestChain()));
                origin->PushMessage("inv", vInv);
                origin->setContinue(uint256(0));
            }
        }
    }
    else if (msg.command() == "inv") {
        vector<Inventory> inventories;
        istringstream is(msg.payload());
        is >> inventories;
        if (inventories.size() > 50000) {
            log_error("message inv size() = %d", inventories.size());
            return false;
        }
        
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
        int height = _blockChain.getBestHeight();
        static int nAskedForBlocks = 0;
        if (!origin->client() && (origin->version() < 32000 || origin->version() >= 32400) && (nAskedForBlocks < 1 || origin->getAllPeers().size() <= 1) && (height -100 < origin->getStartingHeight())) {
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
        // check if we are getting behind the invalid chain:
        int invalid = _blockChain.getInvalidHeight();
        int best = _blockChain.getBestHeight();

        if (_blockChain.getInvalidHeight() - _blockChain.getBestHeight() > 2) {
            ostringstream oss;
            oss << "Invalid " << _blockChain.chain().name() << " chain is longer than our best chain : " << invalid << " vs. " << best;
            notify(oss.str());
        }
        log_error("append(Block) failed: %s", e.what());
        if (_blockChain.getInvalidHeight() - _blockChain.getBestHeight() > 3) {
            log_error("Invalid chain is longer than ours - exiting");
            exit(1); // we don't want to
        }

    }

    // Relay inventory, but don't relay old inventory during initial block download
    uint256 bestChain = _blockChain.getBestChain();
    if (bestChain == hash) {
        for(Peers::iterator peer = peers.begin(); peer != peers.end(); ++peer)
            if (_blockChain.getBestHeight() > ((*peer)->getStartingHeight() != -1 ? (*peer)->getStartingHeight() - 2000 : _blockChain.getTotalBlocksEstimate()))
                (*peer)->push(Inventory(MSG_BLOCK, hash), true);
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
        log_error((string("append(Block) failed: ") + e.what()).c_str());
        return false;
    }
    // notify all listeners
    for(Listeners::iterator listener = _listeners.begin(); listener != _listeners.end(); ++listener)
        (*listener->get())(block);
    
    log_debug("ProcessShare: ACCEPTED");
    return true;
}

