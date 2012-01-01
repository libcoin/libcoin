
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
    
    virtual bool operator()(Peer* origin, Message& msg);
    
    virtual std::vector<std::string> commands() {
        std::vector<std::string> c; 
        c.push_back("block");
        c.push_back("getblocks");
        c.push_back("getheaders");
        c.push_back("inv");
        c.push_back("getdata");
        return c;
    }

private:
    uint256 getOrphanRoot(const Block* pblock);
    bool processBlock(Peer* origin, Block& block);
    
    bool alreadyHave(const Inventory& inv);

private:
    BlockChain& _blockChain;
    
    std::map<uint256, Block*> _orphanBlocks;
    std::multimap<uint256, Block*> _orphanBlocksByPrev;
    
    std::map<Inventory, int64> _alreadyAskedFor;
};

#endif // BLOCKFILTER_H
