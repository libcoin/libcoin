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

#include <coinChain/Database.h>

#include <string>
#include <iostream>

#include <boost/lexical_cast.hpp>

using namespace std;
using namespace boost;

int main(int argc, char* argv[])
{
    try {
        return 0;
    }
    catch (std::exception &e) {
        cout << "Exception: " << e.what() << endl;
        return 1;
    }
}
