
#include "btcNode/EndpointPool.h"
#include "btcNode/db.h"
#include "btcNode/irc.h"

#include "btc/util.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

using namespace std;
using namespace boost;

void EndpointPool::purge()
{    
    if (_lastPurgeTime == 0)
        _lastPurgeTime = GetTime();
    
    if (GetTime() - _lastPurgeTime > 10 * 60 && vNodes.size() >= 3) {
        _lastPurgeTime = GetTime();
        
        int64 since = GetAdjustedTime() - 14 * 24 * 60 * 60;
        for (EndpointMap::iterator mi = _endpoints.begin(); mi != _endpoints.end();) {
            const Endpoint& endpoint = (*mi).second;
            if (endpoint.getTime() < since) {
                if (_endpoints.size() < 1000 || GetTime() > _lastPurgeTime + 20)
                    break;

                eraseEndpoint(endpoint);
                _endpoints.erase(mi++);
            }
            else
                mi++;
        }
    }
}

set<Endpoint> EndpointPool::getRecent(int within, int rand_max)
{
    set<Endpoint> endpoints;
    int64 since = GetAdjustedTime() - within; // in the last 3 hours
    unsigned int nCount = 0;
    BOOST_FOREACH(const PAIRTYPE(vector<unsigned char>, Endpoint)& item, _endpoints)
        {
        const Endpoint& endpoint = item.second;
        if (endpoint.getTime() > since)
            nCount++;
        }
    BOOST_FOREACH(const PAIRTYPE(vector<unsigned char>, Endpoint)& item, _endpoints)
        {
        const Endpoint& endpoint = item.second;
        if (endpoint.getTime() > since && GetRand(nCount) < rand_max)
            endpoints.insert(endpoint);
        }
    return endpoints;
}

bool EndpointPool::addEndpoint(Endpoint endpoint, int64 penalty)
{
    if (!endpoint.isRoutable())
        return false;
    if (endpoint.getIP() == addrLocalHost.getIP())
        return false;
    endpoint.setTime(max((int64)0, (int64)endpoint.getTime() - penalty));
    bool updated = false;
    bool is_new = false;
    Endpoint found = endpoint;
    
    EndpointMap::iterator it = _endpoints.find(endpoint.getKey());
    if (it == _endpoints.end()) {
        // New address
        printf("AddAddress(%s)\n", endpoint.toString().c_str());
        _endpoints.insert(make_pair(endpoint.getKey(), endpoint));
        updated = true;
        is_new = true;
    }
    else {
        found = (*it).second;
        if ((found.getServices() | endpoint.getServices()) != found.getServices()) {
            // Services have been added
            uint64 services =  found.getServices() | endpoint.getServices();
            found.setServices(services);
            updated = true;
        }
        bool currentlyOnline = (GetAdjustedTime() - endpoint.getTime() < 24 * 60 * 60);
        int64 updateInterval = (currentlyOnline ? 60 * 60 : 24 * 60 * 60);
        if (found.getTime() < endpoint.getTime() - updateInterval) {
            // Periodically update most recently seen time
            found.setTime(endpoint.getTime());
            updated = true;
        }
    }
    
    // There is a nasty deadlock bug if this is done inside the cs_mapAddresses
    // CRITICAL_BLOCK:
    // Thread 1:  begin db transaction (locks inside-db-mutex)
    //            then AddAddress (locks cs_mapAddresses)
    // Thread 2:  AddAddress (locks cs_mapAddresses)
    //             ... then db operation hangs waiting for inside-db-mutex
    if (updated)
        writeEndpoint(found);

    return is_new;
}

void EndpointPool::currentlyConnected(const Endpoint& ep)
{
    // Only if it's been published already
    EndpointMap::iterator it = _endpoints.find(ep.getKey());
    if (it != _endpoints.end()) {
        Endpoint& found = (*it).second;
        int64 updateInterval = 20 * 60;
        if (found.getTime() < GetAdjustedTime() - updateInterval) {
            // Periodically update most recently seen time
            found.setTime(GetAdjustedTime());
            writeEndpoint(found);
        }
    }
}

Endpoint EndpointPool::getCandidate(const set<unsigned int>& not_in, int64 start_time)
{
    //
    // Choose an address to connect to based on most recently seen
    //
    Endpoint candidate;
    int64 best = INT64_MIN;
    
    BOOST_FOREACH(const PAIRTYPE(vector<unsigned char>, Endpoint)& item, _endpoints)
        {
        const Endpoint& ep = item.second;
        if (!ep.isIPv4() || !ep.isValid() || not_in.count(ep.getIP() & 0x0000ffff))
            continue;
        int64 sinceLastSeen = GetAdjustedTime() - ep.getTime();
        int64 sinceLastTry = GetAdjustedTime() - ep.getLastTry();
        
        // Randomize the order in a deterministic way, putting the standard port first
        int64 randomizer = (uint64)(start_time * 4951 + ep.getLastTry() * 9567851 + ep.getIP() * 7789) % (2 * 60 * 60);
        if (ep.getPort() != htons(getDefaultPort()))
            randomizer += 2 * 60 * 60;
        
        // Last seen  Base retry frequency
        //   <1 hour   10 min
        //    1 hour    1 hour
        //    4 hours   2 hours
        //   24 hours   5 hours
        //   48 hours   7 hours
        //    7 days   13 hours
        //   30 days   27 hours
        //   90 days   46 hours
        //  365 days   93 hours
        int64 delay = (int64)(3600.0 * sqrt(fabs((double)sinceLastSeen) / 3600.0) + randomizer);
        
        // Fast reconnect for one hour after last seen
        if (sinceLastSeen < 60 * 60)
            delay = 10 * 60;
        
        // Limit retry frequency
        if (sinceLastTry < delay)
            continue;
        
        // If we have IRC, we'll be notified when they first come online,
        // and again every 24 hours by the refresh broadcast.
        if (nGotIREndpointes > 0 && vNodes.size() >= 2 && sinceLastSeen > 24 * 60 * 60)
            continue;
        
        // Only try the old stuff if we don't have enough connections
        if (vNodes.size() >= 8 && sinceLastSeen > 24 * 60 * 60)
            continue;
        
        // If multiple addresses are ready, prioritize by time since
        // last seen and time since last tried.
        int64 score = min(sinceLastTry, (int64)24 * 60 * 60) - sinceLastSeen - randomizer;
        if (score > best) {
            best = score;
            candidate = ep;
        }
    }
    return candidate;
}

bool EndpointPool::writeEndpoint(const Endpoint& endpoint)
{
    return Write(make_pair(string("addr"), endpoint.getKey()), endpoint);
}

bool EndpointPool::eraseEndpoint(const Endpoint& endpoint)
{
    return Erase(make_pair(string("addr"), endpoint.getKey()));
}

bool EndpointPool::loadEndpoints()
{
    // Load user provided addresses
    CAutoFile filein = fopen((GetDataDir() + "/addr.txt").c_str(), "rt");
    if (filein) {
        try {
            char psz[1000];
            while (fgets(psz, sizeof(psz), filein))
                {
                Endpoint endpoint(psz, false, NODE_NETWORK);
                endpoint.setTime(0); // so it won't relay unless successfully connected
                if (endpoint.isValid())
                    addEndpoint(endpoint);
                }
        }
        catch (...) { }
    }
    
    // Get cursor
    Dbc* pcursor = GetCursor();
    if (!pcursor)
        return false;
    
    loop {
        // Read next record
        CDataStream ssKey;
        CDataStream ssValue;
        int ret = ReadAtCursor(pcursor, ssKey, ssValue);
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
            return false;
        
        // Unserialize
        string strType;
        ssKey >> strType;
        if (strType == "addr") {
            Endpoint endpoint;
            ssValue >> endpoint;
            _endpoints.insert(make_pair(endpoint.getKey(), endpoint));
        }
    }
    pcursor->close();
    
    printf("Loaded %d addresses\n", _endpoints.size());
    
    return true;
}
