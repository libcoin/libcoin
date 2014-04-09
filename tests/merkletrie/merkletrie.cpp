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
#include <coinChain/MerkleTrie.h>

#include <coin/uint256.h>
#include <coin/util.h>

#include <string>
#include <iostream>

#include <boost/lexical_cast.hpp>

#include <stdint.h>

//Portable algorithms from http://aggregate.org/MAGIC/
static inline uint32_t ones32(const uint32_t x)
{
    /* 32-bit recursive reduction using SWAR...
       but first step is mapping 2-bit values
       into sum of 2 1-bit values in sneaky way
	  */
    uint32_t y = x;
    y -= ((y >> 1) & 0x55555555);
    y = (((y >> 2) & 0x33333333) + (y & 0x33333333));
    y = (((y >> 4) + y) & 0x0f0f0f0f);
    y += (y >> 8);
    y += (y >> 16);
    return(y & 0x0000003f);
}

static inline uint32_t log2(const uint32_t x) {
    uint32_t y = x;
    y |= (y >> 1);
    y |= (y >> 2);
    y |= (y >> 4);
    y |= (y >> 8);
    y |= (y >> 16);
    return(ones32(y >> 1));
}

using namespace std;
using namespace boost;

struct IntElem {
    typedef uint32_t Key;
    typedef uint256 Hash;
    typedef unsigned char KeyWidth;
    
    IntElem(uint32_t i = 0) {
        if (i)
            key = i;
        else
            key = rand() % 1234567890;
        ostringstream ss;
        ss << hex << key;
        value = ss.str();
    }
    
    // output the key as a binary string "000101010101011010001"
    static string key_str(const Key& k, size_t a = 0, size_t b = 0) {
        if (a == 0 && b == 0) b = 8*sizeof(Key);
        unsigned int* data = (unsigned int*)&k;
        string bin;
        for (size_t i = 0; i < sizeof(Key)/4; ++i) {
            for (size_t j = 0; j < 32; ++j) {
                bin += (data[i]&(1<<j))?'1':'0';
            }
        }
        return bin.substr(a, b-a);
    }
    
    static int compare(const Key& a, const Key& b) {
        // treat keys as bitfields
        // delete the first depth bits
        // compare bits until there is a difference
        // return log2 of the difference + if a>b, - if a<b
        if (a > b)
            return -log2(a^b) - 1;
        else if (a < b)
            return +log2(a^b) + 1;
        else
            return 0;
    }
    
    std::string value;
    Key key;
    Hash hash() const { return ::Hash(value.begin(), value.end()); }
    static Hash hash(Hash a, Hash b) { return ::Hash(a.begin(), a.end(), b.begin(), b.end()); }
};

string bin2dec(string bin) {
    size_t val = 0;
    size_t radix = bin.size();
    while (radix) {
        --radix;
        val += (bin[radix]=='1'?1:0)*(1<<radix);
    }
    return lexical_cast<string>(val);
}


inline int64_t time_microseconds() {
    return (boost::posix_time::ptime(boost::posix_time::microsec_clock::universal_time()) -
            boost::posix_time::ptime(boost::gregorian::date(1970,1,1))).total_microseconds();
}

int main(int argc, char* argv[])
{
    /*
    uint256 hash = bitcoin.genesisBlock().getHash();
    uint256 halfHash = bitcoin.genesisBlock().getHalfHash();
    
    cout << hash.toString() << endl;
    cout << halfHash.toString() << endl;

    uint256 hash(1);
    cout << hash.toString() << endl;
    unsigned int* pn = (unsigned int*)hash.begin();
    cout << pn[0] << " " << pn[7] << endl;

    return 0;
    */
    
    try {
        cout << "Checking that the MerkleTrie works - first with IntElem" << endl;
        typedef MerkleTrie<IntElem> IntTrie;
        IntTrie trie;
        IntTrie trie_rnd;

        const int N = 100;
        vector<IntElem> elems;
        vector<IntElem::Key> keys;
        for (size_t i = 0; i < N; ++i) {
            elems.push_back(IntElem());
            if (i%3==0)
                keys.push_back(elems.back().key);
        }

        cout << "Inserting " << N << " elements" << endl;
        int64_t t_insert = -time_microseconds();
        for (size_t i = 0; i < N; ++i)
            trie.insert(elems[i]);
        for (size_t i = 0; i < keys.size(); ++i)
            trie.remove(keys[i]);
        t_insert += time_microseconds();
        
        cout << "Took " << 0.001*t_insert << "ms" << endl;
        cout << "Root hash is: " << trie.root()->hash().toString() << endl;
        
        cout << "Shuffeling the elements and inserting them in another trie (hashing off)" << endl;
        random_shuffle ( elems.begin(), elems.end() );

        trie_rnd.authenticated(true);
        int64_t t_rnd_insert = -time_microseconds();
        for (size_t i = 0; i < N; ++i) {
            trie_rnd.insert(elems[i]);
            if (i%13)
                for (size_t i = 0; i < keys.size(); ++i)
                    trie_rnd.remove(keys[i]);
        }
        for (size_t i = 0; i < keys.size(); ++i)
            trie_rnd.remove(keys[i]);
        //        trie_rnd.authenticated(true);
        t_rnd_insert += time_microseconds();

        cout << "Took " << 0.001*t_rnd_insert << "ms" << endl;
        cout << "Root hash is: " << trie_rnd.root()->hash().toString() << endl;
        
        if (trie.root()->hash() != trie_rnd.root()->hash())
            throw runtime_error("Hashes did not match - something is wrong!");
        
        IntTrie trie_clone(trie_rnd);

        trie_rnd.insert(IntElem());
        
        cout << "Root hash is: " << trie_rnd.root()->hash().toString() << endl;
        cout << "Clone hash is: " << trie_clone.root()->hash().toString() << endl;
        
        cout << "Performing " << N << " searches " << endl;

        int64_t t_search = -time_microseconds();
        
        size_t removed = 0;
        for (size_t i = 0; i < N; ++i) {
            IntTrie::Iterator it = trie_rnd.find(elems[i].key);
        
            if (!it) {
                removed++;
                // get lower and upper bounds!
                //                IntTrie::Branch lower = (-it).branch();
                //                IntTrie::Branch upper = (+it).branch();

                //uint32_t lower_key = (-it)->key;
                //uint32_t upper_key = (+it)->key;
                
//                uint32_t key = elems[i].key;

                //                cout << key-lower_key << " " << key << " " << upper_key-key << endl;
                
                //                if (!(lower_key < key && key < upper_key))
                //                    throw runtime_error("Key not between lower and upper !");
                
                //                if (!IntTrie::Branch::consecutive(lower, upper))
                //                    throw runtime_error("Returned lower, upper brannches not consecutive!");
            }
            else {
                IntTrie::Branch branch = it.branch();
                if (branch.root() != trie_rnd.root()->hash())
                    throw runtime_error("MerkleBranch Root Hash mishmash!");
                if (!branch.validate(elems[i]))
                    throw runtime_error("MerkleBranch Validation failed!");
            }
        }
        cout << "Removed: " << removed << endl;
        cout << "Validated Hash check algorithm: Success" << endl;
        
        t_search += time_microseconds();
        cout << "Took " << 0.001*t_search << "ms" << endl;
        
        
        
        cout << trie.graph() << endl;
        
        //    cout << trie_rnd.graph() << endl;

        return 0;
    }
    catch (std::exception &e) {
        cout << "Exception: " << e.what() << endl;
        return 1;
    }
}
