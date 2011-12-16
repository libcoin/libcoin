
#include "btcNode/VersionFilter.h"
#include "btcNode/Alert.h"

#include "btc/util.h"
#include "btcNode/net.h"

using namespace std;
using namespace boost;

bool VersionFilter::operator() (Message& msg) {
    if (msg.command == "version")
        {
        // Each connection can only send one version message
        if (msg.origin->nVersion != 0)
            return false;
        
        int64 nTime;
        Endpoint addrMe;
        Endpoint addrFrom;
        uint64 nNonce = 1;
        msg.payload >> msg.origin->nVersion >> msg.origin->nServices >> nTime >> addrMe;
        if (msg.origin->nVersion == 10300)
            msg.origin->nVersion = 300;
        if (msg.origin->nVersion >= 106 && !msg.payload.empty())
            msg.payload >> addrFrom >> nNonce;
        if (msg.origin->nVersion >= 106 && !msg.payload.empty())
            msg.payload >> msg.origin->strSubVer;
        if (msg.origin->nVersion >= 209 && !msg.payload.empty())
            msg.payload >> msg.origin->nStartingHeight;
        
        if (msg.origin->nVersion == 0)
            return false;
        
        // Disconnect if we connected to ourself
        if (nNonce == nLocalHostNonce && nNonce > 1) {
            printf("connected to self at %s, disconnecting\n", msg.origin->addr.toString().c_str());
            msg.origin->fDisconnect = true;
            return true;
        }
        
        // Be shy and don't send version until we hear
        if (msg.origin->fInbound)
            msg.origin->PushVersion();
        
        msg.origin->fClient = !(msg.origin->nServices & NODE_NETWORK);
        
        AddTimeData(msg.origin->addr.getIP(), nTime);
        
        // Change version
        if (msg.origin->nVersion >= 209)
            msg.origin->PushMessage("verack");
        msg.origin->vSend.SetVersion(min(msg.origin->nVersion, VERSION));
        if (msg.origin->nVersion < 209)
            msg.origin->vRecv.SetVersion(min(msg.origin->nVersion, VERSION));
        
        if (!msg.origin->fInbound) {
            // Advertise our address
            if (__localhost.isRoutable() && !fUseProxy) {
                Endpoint addr(__localhost);
                addr.setTime(GetAdjustedTime());
                msg.origin->PushAddress(addr);
            }
            
            // Get recent addresses
            if (msg.origin->nVersion >= 31402 || _endpointPool->getPoolSize() < 1000) {
                msg.origin->PushMessage("getaddr");
                msg.origin->fGetAddr = true;
            }
        }
        
        // Ask the first connected node for block updates
        static int nAskedForBlocks;
        if (!msg.origin->fClient &&
            (msg.origin->nVersion < 32000 || msg.origin->nVersion >= 32400) &&
            (nAskedForBlocks < 1 || vNodes.size() <= 1)) {
            nAskedForBlocks++;
            msg.origin->PushGetBlocks(__blockChain->getBestIndex(), uint256(0));
        }
        
        // Relay alerts
        CRITICAL_BLOCK(cs_mapAlerts)
            BOOST_FOREACH(PAIRTYPE(const uint256, Alert)& item, mapAlerts)
                item.second.relayTo(msg.origin);
        
        msg.origin->fSuccessfullyConnected = true;
        
        printf("version message: version %d, blocks=%d\n", msg.origin->nVersion, msg.origin->nStartingHeight);
        return true;
    }
    else if (msg.command == "verack") {
        if (msg.origin->nVersion == 0) {
            throw OriginNotReady();
        }
        msg.origin->vRecv.SetVersion(min(msg.origin->nVersion, VERSION));
    }    
}