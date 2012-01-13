
#ifndef BLOCKFILTER_H
#define BLOCKFILTER_H

#include "Filter.h"

#include "btc/uint256.h"

#include <string>
#include <map>

class Block;
class BlockChain;
class Inventory;

class BlockFilter : public Filter
{
public:    
    BlockFilter(BlockChain& bc) : _blockChain(bc) {}
    
    class Listener : private boost::noncopyable {
    public:
        virtual void operator()(const Block&) = 0;
    };
    typedef boost::shared_ptr<Listener> listener_ptr;
    typedef std::set<listener_ptr> Listeners;
    
    void subscribe(listener_ptr listener) { _listeners.insert(listener); }
    
    virtual bool operator()(Peer* origin, Message& msg);
    
    virtual std::set<std::string> commands() {
        std::set<std::string> c; 
        c.insert("block");
        c.insert("getblocks");
        c.insert("getheaders");
        c.insert("inv");
        c.insert("getdata");
        c.insert("version");
        return c;
    }

private:
    uint256 getOrphanRoot(const Block* pblock);
    bool processBlock(Peer* origin, Block& block);
    
    bool alreadyHave(const Inventory& inv);

private:
    BlockChain& _blockChain;
    Listeners _listeners;
    
    std::map<uint256, Block*> _orphanBlocks;
    std::multimap<uint256, Block*> _orphanBlocksByPrev;
    
    std::map<Inventory, int64> _alreadyAskedFor;
};

#endif // BLOCKFILTER_H
