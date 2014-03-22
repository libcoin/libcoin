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

using namespace std;
using namespace boost;

bool VersionFilter::operator() (Peer* origin, Message& msg) {
    if (msg.command() == "version") {
        // Each connection can only send one version message
        if (origin->nVersion != 0)
            return false;
        
        //        cout << "msg payload size: " << dec << msg.payload().size() << " and contents ";
        //        for ( unsigned int i = 0; i < msg.payload().size(); i++ ) {
        //            cout << hex << (unsigned int)(msg.payload()[i]) << " ";
        //        }
        //        cout << endl;
        
        int64_t nTime;
        Endpoint addrMe;
        Endpoint addrFrom;
        addrFrom.setLastTry(UINT_MAX);
        addrMe.setLastTry(UINT_MAX);
        uint64_t nNonce = 1;
        istringstream is(msg.payload());
        
        is >> binary<int>(origin->nVersion) >> binary<uint64_t>(origin->nServices) >> binary<int64_t>(nTime) >> addrMe;
        if (origin->nVersion == 10300)
            origin->nVersion = 300;
        if (origin->nVersion >= 106 && !is.eof())
            is >> addrFrom >> binary<uint64_t>(nNonce);
        if (origin->nVersion >= 106 && !is.eof())
            is >> varstr(origin->strSubVer);
        int bestHeight;
        if (origin->nVersion >= 209 && !is.eof())
            is >> bestHeight;
        
        if (!is.eof())
            is >> binary<bool>(origin->relayTxes); // set to true after we get the first filter* message
        else
            origin->relayTxes = true;

//        CDataStream data(msg.payload(), SER_NETWORK, 0); // this is to ensure we dont reread the time and version for the endpoints (see Endpoint serializer)
/*
        data >> origin->nVersion >> origin->nServices >> nTime >> addrMe;
        if (origin->nVersion == 10300)
            origin->nVersion = 300;
        if (origin->nVersion >= 106 && !data.empty())
            data >> addrFrom >> nNonce;
        if (origin->nVersion >= 106 && !data.empty())
            data >> origin->strSubVer;
        int bestHeight;
        if (origin->nVersion >= 209 && !data.empty())
            data >> bestHeight;
        
        if (!data.empty())
            data >> origin->relayTxes; // set to true after we get the first filter* message
        else
            origin->relayTxes = true;
*/
        if (origin->nVersion == 0)
            return false;
        
        // Disconnect if we connected to ourself
        if (nNonce == origin->getNonce() && nNonce > 1) {
            log_debug("connected to self at %s, disconnecting\n", origin->addr.toString().c_str());
            origin->fDisconnect = true;
            return true;
        }
        
        // Be shy and don't send version until we hear
        if (origin->fInbound)
            origin->PushVersion();

        // We need to wait till after we have sent our data setting the nStartingHeight for the remote. Otherwise the remote will only get its onw height back.
        
        origin->setStartingHeight(bestHeight);
        
        origin->fClient = !(origin->nServices & NODE_NETWORK);
        
        AddTimeData(origin->addr.getIP(), nTime);
        
        // Change version
        if (origin->nVersion >= 209)
            origin->PushMessage("verack");
        origin->vSend.SetVersion(min(origin->nVersion, PROTOCOL_VERSION));
        if (origin->nVersion < 209)
            origin->vRecv.SetVersion(min(origin->nVersion, PROTOCOL_VERSION));
                
        origin->successfullyConnected();
        
        return true;
    }
    else if (msg.command() == "verack") {
        if (origin->nVersion == 0) {
            throw OriginNotReady();
        }
        origin->vRecv.SetVersion(min(origin->nVersion, PROTOCOL_VERSION)); // this is where the version is - this triggers the parser to expect checksums too! (UGLY!)
        return true;
    }  
    return false;
}