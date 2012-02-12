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

#ifndef EXPLORER_H
#define EXPLORER_H

#include <coinStat/Export.h>

#include <coinChain/Node.h>

#include <fstream>

class Explorer;

/// Explorer collects address to coin mappings from the block chain.
/// This enables lookup in a database of coin owned by a spicific Address.
/// On first startup it scans the block file for all transactions, and 
/// stores it in a file: "explorer.dat" This file is tehn updated every 1000
/// blocks. The first scan can take several minutes, after that the penalty
/// of running the explorer is neglible.

class COINSTAT_EXPORT Explorer {
public:
    
    /// The TransactionListener scans each new transaction through
    /// the transaction filter.
    class TransactionListener : public TransactionFilter::Listener {
    public:
        TransactionListener(Explorer& explorer) : _explorer(explorer) {}
        virtual void operator()(const Transaction& tx);
    private:
        Explorer& _explorer;
    };
    
    /// The BlockListener scans each new transaction through
    /// the block filter.
    class BlockListener : public BlockFilter::Listener {
    public:
        BlockListener(Explorer& explorer) : _explorer(explorer) {}
        virtual void operator()(const Block& blk);
    private:
        Explorer& _explorer;
    };
    
public:
    /// Construct the Explorer.
    /// The Explorer monitors a Node for new transactions and blocks.
    /// Use the include_spend flag to minimize memory usage by only keeping
    /// a record of non spent coins - currently they are 10% of everything.
    Explorer(Node& node, bool include_spend = true, std::string file = "", std::string data_dir = "") : _include_spend(include_spend), _blockChain(node.blockChain()), _height(0) {
        data_dir = (data_dir == "") ? CDB::dataDir(_blockChain.chain().dataDirSuffix()) : data_dir;
        file = (file == "") ? file = "explorer.dat" : file;
        
        // load the wallet if it is there
        load(data_dir + "/" + file);
        unsigned int h = _height;
        scan();
        if (_height - h > 1000)
            save(data_dir + "/" + file);
        
        // install callbacks to get notified about new tx'es and blocks
        node.subscribe(TransactionFilter::listener_ptr(new TransactionListener(*this)));
        node.subscribe(BlockFilter::listener_ptr(new BlockListener(*this)));
    }
    
    /// Handle to the BlockChain
    const BlockChain& blockChain() const { return _blockChain; }
    
    /// Retreive coins credited from an address
    void getCredit(const Address& btc, Coins& coins) const;
    
    /// Retreive coins debited to an address
    void getDebit(const Address& btc, Coins& coins) const;
    
    /// Retreive spendable coins of address
    void getCoins(const Address& btc, Coins& coins) const;
    
    /// Load the database from a file
    void load(std::string filename);
    /// Take a snapshot of the database and store it in a file
    void save(std::string filename);
    
    /// Scan for address to coin mappings in the BlockChain. - This will take some time!
    void scan();
    
    /// add credit and debit:
    void insertDebit(const Address& address, const Coin& coin);
    void insertCredit(const Address& address, const Coin& coin);
    void markSpent(const Address& address, const Coin& coin);

    /// Set the height to the last block checked.
    void setHeight(int height) { _height = height; }
    
private:
    bool _include_spend;
    const BlockChain& _blockChain;
    /// Multimap, mapping Address to Coin relationship. One Address can be asset for multiple coins
    typedef std::multimap<Address, Coin> Ledger;
    Ledger _debits;
    Ledger _credits;
    Ledger _coins;
    int _height;
};

#endif // EXPLORER_H
