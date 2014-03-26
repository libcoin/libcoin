/* -*-c++-*- libcoin - Copyright (C) 2012-14 Michael Gronager
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

/// serialization tests - to check that the serialization follows the same scheme that the old serialization.

#include <coin/Transaction.h>
#include <coinWallet/serialize.h>
#include <coinWallet/WalletTx.h>
#include <coin/Serialization.h>
#include <coin/uint256.h>
#include <coin/util.h>

#include <coinChain/Chain.h>
#include <coinChain/Endpoint.h>

#include <string>
#include <iostream>

#include <boost/lexical_cast.hpp>

#include <stdint.h>

using namespace std;
using namespace boost;

inline int64_t time_microseconds() {
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

vector<unsigned char> rand_data(size_t size = 20) {
    vector<unsigned char> data;
    for (size_t i = 0; i < size; ++i)
        data.push_back(rand()%256);
    return data;
}

template <typename T>
void serialization_test(string name, T t) {
    try {
        ostringstream oss;
        oss << t;
        
        CDataStream ds(SER_NETWORK, PROTOCOL_VERSION);
        ds << t;
        
        string s1 = oss.str();
        string s2 = ds.str();
        
        if (s1 != s2)
            throw runtime_error(" output serialization error: length of DataStream: " + lexical_cast<string>(s2.size())+ " length of stringstream: "+ lexical_cast<string>(s1.size()));
        
        T t1;
        T t2;
        
        istringstream is1(s1);
        CDataStream is2(s2);
        
        is1 >> t1;
        is2 >> t2;
        
        if (t1 != t2)
            throw runtime_error(" input serialization error");
        
        cout << name << " Passed" << endl;
    }
    catch (std::exception& e) {
        throw runtime_error(name + " : " + e.what());
    }
}

int main(int argc, char* argv[])
{
    cout << "==== Testing serialization ====\n" << endl;

    try {
        const char* alphabet = "abcdefghijklmnopqrstuvwxyz";
        vector<char> vec(alphabet, alphabet+26);
        serialization_test("vector<char>", vec);

        serialization_test("Script", (Script)rand_data(36));
/*
 //     This has been tested and is now considered deprecated
        serialization_test("Coin", Coin(rand_hash(), 2));
 
        serialization_test("Input", Input(Coin(rand_hash(), 10),(Script)rand_data(36)));
        
        serialization_test("Output", Output(time_microseconds(), (Script)rand_data(25)));

        Transaction txn;
        txn.addInput(Input(Coin(rand_hash(), 1), (Script)rand_data(37)));
        txn.addInput(Input(Coin(rand_hash(), 10),(Script)rand_data(36)));
        txn.addInput(Input(Coin(rand_hash(), 100), (Script)rand_data(35)));
        txn.addInput(Input(Coin(rand_hash(), 1000), (Script)rand_data(39)));
        
        txn.addOutput(Output(time_microseconds(), (Script)rand_data(25)));
        txn.addOutput(Output(time_microseconds(), (Script)rand_data(25)));
        txn.addOutput(Output(time_microseconds(), (Script)rand_data(25)));
        
        serialization_test("Transaction", txn);
        
        serialization_test("BlockHeader", (BlockHeader)bitcoin.genesisBlock());
        
        serialization_test("Block", bitcoin.genesisBlock());

        serialization_test("Endpoint", Endpoint("1.2.3.4:5678"));
        serialization_test("Endpoint IPv6", Endpoint("fe80::6203:8ff:fe90:f4ce:5678"));
        serialization_test("Endpoint IPv6 enc IPv4", Endpoint("::ffff:1:2:3:4:5678"));
*/
        return 0;
    }
    catch (std::exception &e) {
        cout << "Exception: " << e.what() << endl;
        return 1;
    }
}