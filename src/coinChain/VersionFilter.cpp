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

#include <coinChain/VersionFilter.h>
#include <coinChain/Alert.h>

#include <coin/util.h>
#include <coin/Logger.h>
#include <coinChain/Peer.h>
#include <coinChain/BlockChain.h>

using namespace std;
using namespace boost;

bool VersionFilter::operator() (Peer* origin, Message& msg) {
    if (msg.command() == "version") {
        // Each connection can only send one version message
        if (origin->initialized())
            return false;
            
        istringstream is(msg.payload());
        
        is >> (*origin);
        
        if (origin->disconnecting()) {
            log_debug("connected to self at %s, disconnecting\n", origin->addr.toString().c_str());
            return true;
        }

        if (!origin->initialized())
            return false;
  
        if (origin->disconnecting()) {
            log_debug("connected to self at %s, disconnecting\n", origin->addr.toString().c_str());
            return true;
        }
        
        // Be shy and don't send version until we hear
        if (origin->fInbound)
            origin->PushVersion();

        // We need to wait till after we have sent our data setting the nStartingHeight for the remote. Otherwise the remote will only get its onw height back.
        
        // Change version
        if (origin->nVersion >= 209)
            origin->PushMessage("verack");
        
        origin->successfullyConnected();
        
        return true;
    }
    else if (msg.command() == "verack") {
        if (!origin->initialized())
            throw OriginNotReady();
        return true;
    }  
    return false;
}