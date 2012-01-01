
#ifndef TRANSACTIONFILTER_H
#define TRANSACTIONFILTER_H

#include "btcNode/Filter.h"

#include "btc/serialize.h" // for CDataStream
#include "btc/util.h" // for CCriticalSection definition

#include <string>
#include <map>
#include <deque>

class BlockChain;
class Inventory;
class CDataStream;

class TransactionFilter : public Filter
{
public:
    TransactionFilter(BlockChain& bc) : _blockChain(bc) {}
    
    virtual bool operator()(Peer* origin, Message& msg);
    
    virtual std::vector<std::string> commands() {
        std::vector<std::string> c; 
        c.push_back("tx");
        c.push_back("inv");
        c.push_back("getdata");
        return c;
    }
    
private:
    std::map<uint256, CDataStream*> _orphanTransactions;
    std::multimap<uint256, CDataStream*> _orphanTransactionsByPrev;
    
    void addOrphanTx(const CDataStream& payload);
    
    void eraseOrphanTx(uint256 hash);
    
    bool alreadyHave(const Inventory& inv);
    
    std::map<Inventory, int64> _alreadyAskedFor;
    
    /// The Relay system is only used for Transactions - hence we put it here.
    
    std::map<Inventory, CDataStream> _relay;
    std::deque<std::pair<int64, Inventory> > _relayExpiration;
    CCriticalSection cs_mapRelay;
    
    inline void relayInventory(const Inventory& inv);

    template<typename T> void relayMessage(const Inventory& inv, const T& a);
    
    
    BlockChain& _blockChain;
};

template<> inline void TransactionFilter::relayMessage<>(const Inventory& inv, const CDataStream& ss);


#endif // TRANSACTIONFILTER_H
