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

#ifndef TRANSACTIONFILTER_H
#define TRANSACTIONFILTER_H

#include <coinChain/Export.h>
#include <coinChain/PeerManager.h>
#include <coinChain/Filter.h>
#include <coinChain/Inventory.h>

#include <coin/serialize.h> // for CDataStream
#include <coin/util.h> // for CCriticalSection definition

#include <string>
#include <map>
#include <deque>

class BlockChain;
class Inventory;
//class CDataStream;

class COINCHAIN_EXPORT TransactionFilter : public Filter
{
public:

    TransactionFilter(BlockChain& bc) : _blockChain(bc) {}
    
    class Listener : private boost::noncopyable {
    public:
        virtual void operator()(const Transaction&) = 0;
        virtual ~Listener() {}
    };
    typedef boost::shared_ptr<Listener> listener_ptr;
    typedef std::set<listener_ptr> Listeners;
    
    void subscribe(listener_ptr listener) { _listeners.insert(listener); }

    class Reminder : private boost::noncopyable {
    public:
        virtual void operator()(std::set<uint256>&) = 0;
    };
    typedef boost::shared_ptr<Reminder> reminder_ptr;
    typedef std::set<reminder_ptr> Reminders;

    void subscribe(reminder_ptr reminder) { _reminders.insert(reminder); }
        
    virtual bool operator()(Peer* origin, Message& msg);
    
    virtual std::set<std::string> commands() {
        std::set<std::string> c; 
        c.insert("tx");
        c.insert("inv");
        c.insert("getdata");
        c.insert("mempool");
        return c;
    }
    
    /// Call process to get a hook into the transaction processing, e.g. if for injecting wallet generated txes
    void process(Transaction& tx, Peers peers);
    
private:
    BlockChain& _blockChain;
    Listeners _listeners;
    Reminders _reminders;

    typedef std::map<uint256, Transaction> Orphans;
    Orphans _orphans;
    typedef std::multimap<uint256, Orphans::iterator> OrphansByPrev;
//    std::multimap<uint256, CDataStream*> _orphanTransactionsByPrev;
    OrphansByPrev _orphansByPrev;
    
    void addOrphan(const Transaction& tx);
    
    void eraseOrphan(uint256 hash);
    
    bool have(const Inventory& inv);
    
    /// The Relay system is only used for Transactions - hence we put it here.
    typedef std::map<Inventory, Transaction> Relay;
    Relay _relay;
    typedef std::deque<std::pair<int64_t, Inventory> > Expiration;
    Expiration _relayExpiration;
    
//    inline void relayInventory(const Peers& peers, const Inventory& inv);
    void relay(const Peers& peers, const Transaction& txn);

//    template<typename T> void relayMessage(const Peers& peers, const Inventory& inv, const T& a);
};

//template<> inline void TransactionFilter::relayMessage<>(const Peers& peers, const Inventory& inv, const CDataStream& ss);


#endif // TRANSACTIONFILTER_H
