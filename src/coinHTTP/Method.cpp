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

#include "coinHTTP/Method.h"
#include "coinHTTP/RPC.h"
#include "coinHTTP/Server.h"

#include <string>
#include <algorithm>

using namespace std;
using namespace boost;
using namespace json_spirit;

Value Stop::operator()(const Array& params, bool fHelp) {
    if (fHelp || params.size() != 0)
        throw RPC::error(RPC::invalid_params, "stop\n"
                         "Stop bitcoin server.");
    
    _server.shutdown();
    return "Node and Server is stopping";
}    

const std::string Method::extractName() const {
    string n = typeid(*this).name();
    // remove trailing numbers from the typeid
    while (n.find_first_of("0123456789") == 0)
        n.erase(0, 1);
    transform(n.begin(), n.end(), n.begin(), ::tolower);
	// now remove a possible "class "
	if (n.find(" ") != string::npos) {
		n.erase(0, n.find(" ")+1);
	}
    return n;
}