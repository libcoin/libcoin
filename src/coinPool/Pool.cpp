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

#include <coinPool/Pool.h>

#include <coinHTTP/RPC.h>

using namespace std;
using namespace boost;
using namespace json_spirit;

Block Pool::getBlockTemplate(bool invalid) {
    BlockIterator blk;
    if (invalid) {
        blk = _blockChain.getBestInvalid();
        if (!blk) {
            blk = _blockChain.getBest();
            --blk;
        }
    }
    else
        blk = _blockChain.getBest();

    BlockChain::Payees payees;
    payees.push_back(_payee.current_script());
    Block block = _blockChain.getBlockTemplate(blk, payees);
    // add the block to the list of work
    return block;
}

std::string Pool::submitBlock(const Block& stub, std::string workid) {
    try {
        Block block;
        // check type of submission
        switch (stub.getNumTransactions()) {
            case 0: { // pure header, probably from GetWork
                Pool::BlockTemplates::iterator templ = _templates.find(stub.getMerkleRoot());
                if (templ == _templates.end())
                    return "block template not found";
                block = templ->second;
                
                block.setTime(stub.getTime());
                block.setNonce(stub.getNonce());
                break;
            }
            case 1: { // header and coinbase, we need a workid !
                uint256 merkleroot(workid);
                Pool::BlockTemplates::iterator templ = _templates.find(merkleroot);
                if (templ == _templates.end())
                    return "block template not found";
                block = templ->second;
                
                block.setTime(stub.getTime()); // most likely not changed
                block.setNonce(stub.getNonce());
                block.getTransaction(0) = stub.getTransaction(0);
                break;
            }
            default: { // assume full block
                block = stub;
                break;
            }
        }
        // Get saved block
        
        block.updateMerkleTree();
        
        // check that the work is indeed valid
        uint256 hash = block.getHash();
        
        if (hash > target())
            return "block hash did not meet target";
        
        cout << "Block with hash: " << hash.toString() << " mined" << endl;
        
        // else ... we got a valid block!
        if (_node.blockChain().chain().checkProofOfWork(block) )
            _node.post(block); // _node.processBlock(block);
        else if (hash < block.getShareTarget() )
            _node.post(block); // will call the ShareFilter
        else
            throw;
        
        return "";
    }
    catch (...) {
        return "rejected";
    }
}

Pool::Work Pool::getWork(bool invalid) {
    int now = UnixTime::s();
    uint256 prev = _blockChain.getBestChain();
    if (_templates.empty() || prev != _latest_work->second.getPrevBlock() || _latest_work->second.getTime() + 30 > now) {
        if (_templates.empty() || prev != _latest_work->second.getPrevBlock()) {
            _templates.clear();
            _latest_work = _templates.end(); // reset _latest_work
        }
        Block block = getBlockTemplate(invalid);
        _latest_work = _templates.insert(make_pair(block.getMerkleRoot(), block)).first;
    }
    // note that if called more often that each second there will be dublicate searches
    _latest_work->second.setTime(now);
    _latest_work->second.setNonce(0);
    
    return Work(_latest_work->second, target());
}

uint256 Pool::target() const {
    CBigNum t = 0xffffffff;
    t = t*t*t*t*t*t*t;
    if (_templates.size())
        t = CBigNum().SetCompact(_latest_work->second.getBits());
    t *= 0xfffffff;
    return t.getuint256();
}

static const unsigned int pSHA256InitState[8] =
{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

void PoolMethod::formatHashBuffers(Block& block, char* pmidstate, char* pdata, char* phash1)
{
    //
    // Pre-build hash buffers
    //
    struct {
        struct unnamed2 {
            int nVersion;
            uint256 hashPrevBlock;
            uint256 hashMerkleRoot;
            unsigned int nTime;
            unsigned int nBits;
            unsigned int nNonce;
        }
        block;
        unsigned char pchPadding0[64];
        uint256 hash1;
        unsigned char pchPadding1[64];
    }
    tmp;
    memset(&tmp, 0, sizeof(tmp));
    
    tmp.block.nVersion       = block.getVersion();
    tmp.block.hashPrevBlock  = block.getPrevBlock();
    tmp.block.hashMerkleRoot = block.getMerkleRoot();
    tmp.block.nTime          = block.getTime();
    tmp.block.nBits          = block.getBits();
    tmp.block.nNonce         = block.getNonce();
    
    formatHashBlocks(&tmp.block, sizeof(tmp.block));
    formatHashBlocks(&tmp.hash1, sizeof(tmp.hash1));
    
    // Byte swap all the input buffer
    for (unsigned int i = 0; i < sizeof(tmp)/4; i++)
        ((unsigned int*)&tmp)[i] = byteReverse(((unsigned int*)&tmp)[i]);
    
    // Precalc the first half of the first hash, which stays constant
    sha256Transform(pmidstate, &tmp.block, pSHA256InitState);
    
    memcpy(pdata, &tmp.block, 128);
    memcpy(phash1, &tmp.hash1, 64);
}

int PoolMethod::formatHashBlocks(void* pbuffer, unsigned int len) {
    unsigned char* pdata = (unsigned char*)pbuffer;
    unsigned int blocks = 1 + ((len + 8) / 64);
    unsigned char* pend = pdata + 64 * blocks;
    memset(pdata + len, 0, 64 * blocks - len);
    pdata[len] = 0x80;
    unsigned int bits = len * 8;
    pend[-1] = (bits >> 0) & 0xff;
    pend[-2] = (bits >> 8) & 0xff;
    pend[-3] = (bits >> 16) & 0xff;
    pend[-4] = (bits >> 24) & 0xff;
    return blocks;
}

void PoolMethod::sha256Transform(void* pstate, void* pinput, const void* pinit) {
    SHA256_CTX ctx;
    unsigned char data[64];
    
    SHA256_Init(&ctx);
    
    for (int i = 0; i < 16; i++)
        ((uint32_t*)data)[i] = byteReverse(((uint32_t*)pinput)[i]);
    
    for (int i = 0; i < 8; i++)
        ctx.h[i] = ((uint32_t*)pinit)[i];
    
    SHA256_Update(&ctx, data, sizeof(data));
    for (int i = 0; i < 8; i++)
        ((uint32_t*)pstate)[i] = ctx.h[i];
}
