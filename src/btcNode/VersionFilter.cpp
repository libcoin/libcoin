
#include "btcNode/VersionFilter.h"
#include "btcNode/Alert.h"

#include "btc/util.h"
#include "btcNode/net.h"

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
        
        int64 nTime;
        Endpoint addrMe;
        Endpoint addrFrom;
        uint64 nNonce = 1;
        CDataStream data(msg.payload(), SER_NETWORK, 0); // this is to ensure we dont reread the time and version for the endpoints (see Endpoint serializer)

        data >> origin->nVersion >> origin->nServices >> nTime >> addrMe;
        if (origin->nVersion == 10300)
            origin->nVersion = 300;
        if (origin->nVersion >= 106 && !msg.payload().empty())
            data >> addrFrom >> nNonce;
        if (origin->nVersion >= 106 && !msg.payload().empty())
            data >> origin->strSubVer;
        if (origin->nVersion >= 209 && !msg.payload().empty())
            data >> origin->nStartingHeight;
        
        if (origin->nVersion == 0)
            return false;
        
        // Disconnect if we connected to ourself
        if (nNonce == nLocalHostNonce && nNonce > 1) {
            printf("connected to self at %s, disconnecting\n", origin->addr.toString().c_str());
            origin->fDisconnect = true;
            return true;
        }
        
        // Be shy and don't send version until we hear
        if (origin->fInbound)
            origin->PushVersion();
        
        origin->fClient = !(origin->nServices & NODE_NETWORK);
        
        AddTimeData(origin->addr.getIP(), nTime);
        
        // Change version
        if (origin->nVersion >= 209)
            origin->PushMessage("verack");
        origin->vSend.SetVersion(min(origin->nVersion, VERSION));
        if (origin->nVersion < 209)
            origin->vRecv.SetVersion(min(origin->nVersion, VERSION));
        
        if (!origin->fInbound) {
            // Advertise our address
            if (_endpointPool->getLocal().isRoutable() && !fUseProxy) {
                Endpoint addr(_endpointPool->getLocal());
                addr.setTime(GetAdjustedTime());
                origin->PushAddress(addr);
            }
            
            // Get recent addresses
            if (origin->nVersion >= 31402 || _endpointPool->getPoolSize() < 1000) {
                origin->PushMessage("getaddr");
                origin->fGetAddr = true;
            }
        }
        
        // Ask the first connected node for block updates
        static int nAskedForBlocks;
        if (!origin->fClient &&
            (origin->nVersion < 32000 || origin->nVersion >= 32400) &&
            (nAskedForBlocks < 1 || vNodes.size() <= 1)) {
            nAskedForBlocks++;
            origin->PushGetBlocks(__blockChain->getBestLocator(), uint256(0));
        }
        
        // Relay alerts
        CRITICAL_BLOCK(cs_mapAlerts)
            BOOST_FOREACH(PAIRTYPE(const uint256, Alert)& item, mapAlerts)
                item.second.relayTo(origin);
        
        origin->fSuccessfullyConnected = true;
        
        printf("version message: version %d, blocks=%d\n", origin->nVersion, origin->nStartingHeight);
        return true;
    }
    else if (msg.command() == "verack") {
        if (origin->nVersion == 0) {
            throw OriginNotReady();
        }
        origin->vRecv.SetVersion(min(origin->nVersion, VERSION)); // this is where the version is - this triggers the parser to expect checksums too! (UGLY!)
        return true;
    }  
    return false;
}