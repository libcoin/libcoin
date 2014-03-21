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

#include <coinChain/FilterHandler.h>
#include <coinChain/Peer.h>
#include <coinChain/BloomFilter.h>

#include <vector>

using namespace std;

bool FilterHandler::operator()(Peer* origin, Message& msg) {
    log_trace("args: origin: %s, msg: %s", origin->addr.toString(), msg.command());
    if (origin->nVersion == 0) {
        throw OriginNotReady();
    }
    if (msg.command() == "filterload") {
        BloomFilter filter;
        
        istringstream is(msg.payload());
        is >> filter;
        
        // CDataStream data(msg.payload());
        //data >> filter;
        
        if (filter.isWithinSizeConstraints()) {
            origin->filter = filter;
            origin->filter.updateEmptyFull();
        }
        origin->relayTxes = true;
        return true;
    }
    else if (msg.command() == "filteradd") {
//        CDataStream data(msg.payload());
        vector<unsigned char> bloom_data;
        
        istringstream is(msg.payload());
        is >> bloom_data;
        
        //        data >> bloom_data;
        
        // Nodes must NEVER send a data item > 520 bytes (the max size for a script data object,
        // and thus, the maximum size any matched object can have) in a filteradd message
        if (bloom_data.size() <= MAX_SCRIPT_ELEMENT_SIZE) {
            origin->filter.insert(bloom_data);
        }
        return true;
    }
    else if (msg.command() == "filterclear") {
        origin->filter = BloomFilter();
        origin->relayTxes = true;
        return true;
    }
    return false;
}
