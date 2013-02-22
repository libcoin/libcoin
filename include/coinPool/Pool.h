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

#ifndef COINPOOL_POOL_H
#define COINPOOL_POOL_H

#include <coinPool/Export.h>

#include <coin/Block.h>

#include <coinChain/Node.h>

#include <coinHTTP/Method.h>

#include <map>

class Payee;

/// Pool is an interface for running a pool. It is a basis for the pool rpc commands: 'getwork', 'getblocktemplate' and 'submitblock'
/// The Pool requests a template from the blockchain - i.e. composes a block candidate from the unconfirmed transactions. It stores the
/// candidate mapped by its merkletree root.
/// A solved block submitted either using submitblock or getwork looks up the block and replaces the nonce/time/coinbase
class COINPOOL_EXPORT Pool {
public:
    /// Initialize Pool with a node and a payee, i.e. an abstraction of a payee address generator
    Pool(Node& node, Payee& payee) : _node(node), _blockChain(node.blockChain()), _payee(payee) {
        // install a blockfilter to be notified each time a new block arrives invalidating the current mining effort
    }
    
    // map of merkletree hash to block templates
    typedef std::map<uint256, Block> BlockTemplates;
    
    virtual Block getBlockTemplate();
    
    virtual bool submitBlock(const Block& stub, std::string workid = "");
    
    typedef std::pair<Block, uint256> Work;
    
    virtual Work getWork();
    
    uint256 target() const;
    
    boost::asio::io_service& get_io_service() { return _node.get_io_service(); }
    
    const BlockChain& blockChain() { return _blockChain; }
    
    Node& node() { return _node; }
private:
    Node& _node;
    const BlockChain& _blockChain;
    Payee& _payee;
    BlockTemplates _templates;
    BlockTemplates::iterator _latest_work;
};

class COINPOOL_EXPORT PoolMethod : public Method {
public:
    PoolMethod(Pool& pool) : _pool(pool) {}
    
    virtual void dispatch(const CompletionHandler& f) {
        _pool.get_io_service().dispatch(f);
    }
    
    virtual void dispatch(const CompletionHandler& f, const Request& request) {
        dispatch(f);
    }
    
    void formatHashBuffers(Block& block, char* pmidstate, char* pdata, char* phash1);
    int formatHashBlocks(void* pbuffer, unsigned int len);
    void sha256Transform(void* pstate, void* pinput, const void* pinit);
    inline uint32_t byteReverse(uint32_t value) {
        value = ((value & 0xFF00FF00) >> 8) | ((value & 0x00FF00FF) << 8);
        return (value<<16) | (value>>16);
    }
    
protected:
    Pool& _pool;
};

#endif // COINPOOL_POOL_H
