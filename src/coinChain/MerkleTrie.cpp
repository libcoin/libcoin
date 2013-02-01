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

#include <coinChain/MerkleTrie.h>

#include <coin/Logger.h>

#include <boost/date_time/posix_time/posix_time_types.hpp>

#include <iostream>

using namespace std;
using namespace boost;

int64_t MerkleTrieBase::time_microseconds() const {
    return (boost::posix_time::ptime(boost::posix_time::microsec_clock::universal_time()) -
            boost::posix_time::ptime(boost::gregorian::date(1970,1,1))).total_microseconds();
}

std::string  MerkleTrieBase::statistics() const {
    int64_t total_time = _insert_timer + _remove_timer + _find_timer + _hashing_timer;

    stringstream oss;
    oss << "MerkleTrie Stats:";
    oss << cformat("\tFind time: %9.3fs \n", 0.000001*_find_timer).text();
    oss << cformat("\tInsert time: %9.3fs \n", 0.000001*_insert_timer).text();
    oss << cformat("\tRemove time: %9.3fs \n", 0.000001*_remove_timer).text();
    oss << cformat("\tHashing time: %9.3fs \n", 0.000001*_hashing_timer).text();
    oss << cformat("Total time: %9.3fs \n", 0.000001*total_time).text();
    oss << std::endl;
    return oss.str();
}
