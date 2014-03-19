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

#include <coin/uint256.h>
#include <coin/util.h>

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

int main(int argc, char* argv[])
{
    cout << "==== Testing serialization ====\n" << endl;
    
    try {
        cout << "Checking Coin" << endl;
        return 0;
    }
    catch (std::exception &e) {
        cout << "Exception: " << e.what() << endl;
        return 1;
    }
}