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
/// This enables lookup in a database of coin owned by a spicific PubKeyHash.
/// On first startup it scans the block file for all transactions, and 
/// stores it in a file: "explorer.dat" This file is tehn updated every 1000
/// blocks. The first scan can take several minutes, after that the penalty
/// of running the explorer is neglible.

class COINSTAT_EXPORT Explorer {
public:
    /// Construct the Explorer.
    /// The Explorer monitors a Node for new transactions and blocks.
    /// Use the include_spend flag to minimize memory usage by only keeping
    /// a record of non spent coins - currently they are 10% of everything.
    Explorer(Node& node) : _blockChain(node.blockChain()) {
    }
    
    /// Handle to the BlockChain
    const BlockChain& blockChain() const { return _blockChain; }
    
    /// Retreive coins credited from an address
    //    void getCredit(const PubKeyHash& btc, Coins& coins) const;
    
    /// Retreive coins debited to an address
    //    void getDebit(const PubKeyHash& btc, Coins& coins) const;
    
    /// Retreive spendable coins of address
    //void getCoins(const PubKeyHash& btc, BlockChain::Unspents& coins) const;
    
    /// Load the database from a file
    //    void load(std::string filename);
    /// Take a snapshot of the database and store it in a file
    //    void save(std::string filename);
    
    /// Scan for address to coin mappings in the BlockChain. - This will take some time!
    //    void scan();
    
    /// add credit and debit:
    //    void insertDebit(const PubKeyHash& address, const Coin& coin);
    //    void insertCredit(const PubKeyHash& address, const Coin& coin);
    //    void markSpent(const PubKeyHash& address, const Coin& coin);

    /// Set the height to the last block checked.
    //    void setHeight(int height) { _height = height; }
    
private:
    const BlockChain& _blockChain;
};

#endif // EXPLORER_H
