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

//#include <coinChain/Chain.h>
#include <coinChain/BlockTree.h>

#include <coin/uint256.h>
#include <coin/util.h>

#include <string>
#include <iostream>

#include <boost/lexical_cast.hpp>

#include <stdint.h>

using namespace std;
using namespace boost;


inline int64 time_microseconds() {
    return (boost::posix_time::ptime(boost::posix_time::microsec_clock::universal_time()) -
            boost::posix_time::ptime(boost::gregorian::date(1970,1,1))).total_microseconds();
}

uint256 rand_hash(string hex = "0x000000000019d668") {
    // 5 bytes:
    const char digits[] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
    
    for (size_t i = hex.size(); i < 64; ++i)
        hex += digits[rand()%16];
    return uint256(hex);
}

int main(int argc, char* argv[])
{
    /// We test the blocktree by inserting into it a list of BlockRefs (100000)
    /// We simulate a network with constant hashing power - i.e. bits = 0x1d00ffff
    /// We keep exactly 10 minutes between each block starting from 1234567890 (i.e. february 14th 2009 at 00.31.30)
    unsigned int epoch = 1234567890;
    unsigned int bits = 0x1d00ffff;
    uint256 prev(0);
    uint256 hash("0x000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f");
    vector<BlockRef> blockchain;
    size_t blocks = 100;
    for (size_t i = 0; i < blocks; ++i) {
        blockchain.push_back(BlockRef(i, hash, prev, epoch+i*10*60, bits));
        prev = hash;
        hash = rand_hash();
    }

    cout << "==== Testing class BlockTree ====\n" << endl;

    
    try {
        cout << "Checking that assignment works" << endl;
        
        BlockTree tree;
        tree.assign(blockchain);

        if (tree.height() != blocks-1)
            throw runtime_error("BlockTree assignment failed");

        cout << "BlockTree height is: " << tree.height() << endl;
        
        // now test a single block insertion:

        cout << "Checking that insertion works" << endl;
        for (size_t i = blocks; i < blocks+10; ++i) {
            BlockTree::Changes changes = tree.insert(BlockRef(i, hash, prev, epoch+i*10*60, bits));
            prev = hash;
            hash = rand_hash();
            if (changes.inserted.size() != 1 || changes.deleted.size() != 0)
                throw runtime_error("BlockTree insert failed");
            if (tree.height() != i)
                throw runtime_error("BlockTree height failed");
        }

        cout << "BlockTree height is: " << tree.height() << endl;

        const size_t reorganization_depth = 7;
        BlockIterator blk = tree.begin() + (blocks + 10 - reorganization_depth);
        prev = blk->hash;
        hash = rand_hash();
        
        cout << "Checking that reorganization works - insert 11 blocks from " << reorganization_depth << " blocks back" << endl;
        for (size_t i = blocks + 11 - reorganization_depth; i < blocks + 15; ++i) {
            BlockTree::Changes changes = tree.insert(BlockRef(1000+i, hash, prev, epoch+i*10*60, bits));
            prev = hash;
            hash = rand_hash();
            cout << "Modification table (inserted, deleted): " << changes.inserted.size() << ", " << changes.deleted.size() << endl;
            if (i < blocks + 10 && (changes.inserted.size() > 0 || changes.deleted.size() > 0))
                throw runtime_error("inserted and deleted should be 0 up to the point of reorganization");
            if (i == blocks + 10 && (changes.inserted.size() != reorganization_depth || changes.deleted.size() != reorganization_depth-1))
                throw runtime_error("inserted and deleted should should reflect the reorganization");
            if (i > blocks + 10 && (changes.inserted.size() != 1 || changes.deleted.size() != 0))
                throw runtime_error("inserted and deleted should should be 1 resp. 0 during normal insertion");
        }

        return 0;
    }
    catch (std::exception &e) {
        cout << "Exception: " << e.what() << endl;
        return 1;
    }
}