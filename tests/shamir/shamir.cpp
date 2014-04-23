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


#include <coin/ExtendedKey.h>
#include <coin/BigNum.h>
#include <coin/Shamir.h>

using namespace std;
using namespace boost;

int main(int argc, char* argv[]) {

    try {
        cout << "TESTING: Shamir class" << endl << endl;

        Key k;
        k.reset();
        Shamir shamir(6, 3);
        
        uint256 s(k.number().getuint256());
        Shamir::Shares shares = shamir.split(s);
        
        shares.erase(shares.begin());
        shares.erase(shares.begin());
        shares.erase(shares.begin());
        
        uint256 s1 = shamir.recover(shares);
        
        cout << s.toString() << endl;
        cout << s1.toString() << endl;
        
        if (s != s1)
            throw runtime_error("Could not recover secret!");
    }
    catch (std::exception &e) {
        cout << "Exception: " << e.what() << endl;
        cout << "FAILED: Shamir class" << endl;
        return 1;
    }
    cout << "\nPASSED: Shamir class" << endl;
    
    return 0;
}
