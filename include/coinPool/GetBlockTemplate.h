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

#ifndef COINPOOL_GETBLOCKTEMPLATE_H
#define COINPOOL_GETBLOCKTEMPLATE_H

/// GetBlockTemplate aims at supporting primarily cgminer. cgminer makes the following request:
/// {"id": 0, "method": "getblocktemplate", "params": [{"capabilities": ["coinbasetxn", "workid", "coinbase/append"]}]}
/// The important part is the capabilities part:
/// * coinbasetxn: only submit the coinbasetxn, not the rest of the block
/// * workid: provided by the server (we use the merkleroot for this) and to be returned by SubmitBlock
/// * coinbase/append: the miner can append to the coinbase
/// If the mining client chooses to use the longpollid returned by the server it will install a callback in the server for this longpollid
/// A longpollid composed as follows: "#claims", meaning will return if blocknumber changes, or if #txns claims-threshold is met

#include <coinPool/Export.h>

#include <coinPool/Pool.h>

class COINPOOL_EXPORT GetBlockTemplate : public PoolMethod {
public:
    class BlockListener : public BlockFilter::Listener {
    public:
        BlockListener(GetBlockTemplate& gbt) : BlockFilter::Listener(), _gbt(gbt) {}
        
        virtual void operator()(const Block& block) {
            // this forces a call to all handlers - a new block means we need to reinstantiate miners
            // search for a handler
            HandlerRange range = _gbt.all_handlers();
            for (Handlers::const_iterator h = range.first; h != range.second; ++h) {
                (h->second)();
            }
            _gbt.erase(range);
        }
        virtual ~BlockListener() {}
        
    private:
        GetBlockTemplate& _gbt;
    };
    typedef boost::shared_ptr<BlockListener> block_listener_ptr;
    
    class TransactionListener : public TransactionFilter::Listener {
    public:
        TransactionListener(GetBlockTemplate& gbt) : TransactionFilter::Listener(), _gbt(gbt)  {}
        
        virtual void operator()(const Transaction& txn) {
            // search for a handler
            HandlerRange range = _gbt.current_handlers();
            for (Handlers::const_iterator h = range.first; h != range.second; ++h) {
                (h->second)();
            }
            _gbt.erase(range);
        }
        virtual ~TransactionListener() {}
        
    private:
        GetBlockTemplate& _gbt;
    };
    typedef boost::shared_ptr<TransactionListener> transaction_listener_ptr;
    
    
    GetBlockTemplate(Pool& pool, size_t claim_threshold = 50) : PoolMethod(pool), _txn_listener(new TransactionListener(*this)), _block_listener(new BlockListener(*this)), _claim_threshold(claim_threshold)  {
        _pool.node().subscribe(_txn_listener);
        _pool.node().subscribe(_block_listener);
    }
    
    /// dispatch installs a transaction filter that, after 5 transactions will call the handler
    virtual void dispatch(const CompletionHandler& f, const Request& request);
    
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);
    
    typedef std::multimap<size_t, CompletionHandler> Handlers;
    typedef std::pair<Handlers::iterator, Handlers::iterator> HandlerRange;
    
    void install_handler(const CompletionHandler& f, size_t count);
    
    void erase(HandlerRange range);
    
    HandlerRange current_handlers();
    
    HandlerRange all_handlers();
    
    std::string longpollid() const;
    
private:
    std::string hex_bits(unsigned int bits) const;
private:
    transaction_listener_ptr _txn_listener;
    block_listener_ptr _block_listener;
    
    Handlers _handlers;
    
    mutable boost::mutex _handlers_access;
    
    size_t _claim_threshold;
};

#endif // COINPOOL_GETBLOCKTEMPLATE_H
