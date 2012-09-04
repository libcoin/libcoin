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

#include <coinChain/EndpointPool.h>
#include <coinChain/db.h>

#include <coin/util.h>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

using namespace std;
using namespace boost;

EndpointPool::EndpointPool(short defaultPort, const std::string dataDir, const char* pszMode) :
        Database(dataDir+"/endpoints.sqlite3") /*CDB(dataDir, "addr.dat", pszMode)*/ ,
        _defaultPort(defaultPort),
        _localhost("0.0.0.0", defaultPort, false, NODE_NETWORK),
_lastPurgeTime(0) {
    
    execute("CREATE TABLE IF NOT EXISTS Endpoints(ip INTEGER NOT NULL, port INTEGER NOT NULL, time INTEGER, lasttry INTEGER, PRIMARY KEY (ip, port))");
    
    delete_older_than = prepare("DELETE FROM Endpoints WHERE time < ?");
    select_recent = prepare("SELECT ip, port, time, lasttry FROM Endpoints WHERE time > ? ORDER BY RANDOM() LIMIT ?");
    update_endpoint = prepare("INSERT OR REPLACE INTO Endpoints VALUES (?, ?, ?, ?)");
    update_time = prepare("UPDATE Endpoints SET time = ? WHERE ip = ? AND port = ?");
    update_lasttry = prepare("UPDATE Endpoints SET lasttry = ? WHERE ip = ? AND port = ?");
    candidates = prepare("SELECT ip, port, time, lasttry FROM Endpoints WHERE lasttry < ? ORDER BY time DESC");

            //    create_table();
    //        Statement& create_index = prepare("CREATE INDEX IF NOT EXISTS AddressPort ON Endpoints(address, port)");
    //        create_index();
}


void EndpointPool::purge()
{    
    if (_lastPurgeTime == 0)
        _lastPurgeTime = GetTime();
    
    if (GetTime() - _lastPurgeTime > 10 * 60) {
        _lastPurgeTime = GetTime();

        // remove endpoints older than 14 days:
        int64 since = GetAdjustedTime() - 14 * 24 * 60 * 60;
        delete_older_than(since);
        /*
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
        */
    }
}

set<Endpoint> EndpointPool::getRecent(int within, int rand_max)
{
    int64 since = GetAdjustedTime() - within; // in the last 3 hours
    
    vector<Endpoint> endpoint_list= select_recent(since, rand_max); // rows will now contain a list of endpoint newer than 'since'
                                                // C++11:   select_recent([&](Row& row) { endpoints.insert(Endpoint(row[0].get_int64(), row[1].get_int64(), row[2].get_int64(), row[3].get_int64())); },since, rand_max); // rows will now contain a list of endpoint newer than 'since'

    set<Endpoint> endpoints(endpoint_list.begin(), endpoint_list.end());
    // now generate loop over the rows and create a set of endpoints:
    
/*
    unsigned int nCount = 0;
    BOOST_FOREACH(const PAIRTYPE(vector<unsigned char>, Endpoint)& item, _endpoints)
        {
        const Endpoint& endpoint = item.second;
        if (endpoint.getTime() > since)
            nCount++; // will return the number of endpoints newer than since
        }
    BOOST_FOREACH(const PAIRTYPE(vector<unsigned char>, Endpoint)& item, _endpoints)
        {
        const Endpoint& endpoint = item.second;
        if (endpoint.getTime() > since && GetRand(nCount) < rand_max)
            endpoints.insert(endpoint); // will return roughly less than rand_max at random
        }
 */
    return endpoints;
}

bool EndpointPool::addEndpoint(Endpoint endpoint, int64 penalty)
{
    if (!endpoint.isRoutable())
        return false;
    if (endpoint.getIP() == _localhost.getIP())
        return false;
    endpoint.setTime(max((int64)0, (int64)endpoint.getTime() - penalty));
    bool updated = false;
    bool is_new = false;
    Endpoint found = endpoint;
    
    update_endpoint((int64)endpoint.getIP(), (int64)endpoint.getPort(), (int64)endpoint.getTime(), 0);
    
    return true;
/*
    // add the endpoint to the database - first check if it is new
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
    
    if (updated)
        writeEndpoint(found);

    return is_new;
*/
}

void EndpointPool::currentlyConnected(const Endpoint& ep)
{
    int64 now = GetAdjustedTime();
    update_time(now, (int64)ep.getIP(), ep.getPort());
/*
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
*/
}

void EndpointPool::setLastTry(const Endpoint& ep) {
    int64 now = GetAdjustedTime();
    update_lasttry(now, (int64)ep.getIP(), ep.getPort());
}

Endpoint EndpointPool::getCandidate(const set<unsigned int>& not_in, int64 start_time)
{
    //
    // Choose an address to connect to based on most recently seen
    //
    Endpoint candidate;
    int64 best = INT64_MIN;
    
    int64 now = GetAdjustedTime();
    vector<Endpoint> endpoints = candidates(now-60*60);
    
    // iterate until we get an address not in not_in or in the same class-B network:
    for (vector<Endpoint>::const_iterator e = endpoints.begin(); e != endpoints.end(); ++e) {
        bool skip = false;
        for (set<unsigned int>::const_iterator ep = not_in.begin(); ep != not_in.end(); ++ep) {
            if (e->getIP() == (*ep)) {
                skip = true;
                break;
            }
        }
        if (skip) continue;
        
        Endpoint candidate = *e;
        if (!candidate.isIPv4() || !candidate.isValid())
            continue;
        
        return candidate;
    }
    
    return Endpoint();
/*
    BOOST_FOREACH(const PAIRTYPE(vector<unsigned char>, Endpoint)& item, _endpoints)
        {
        const Endpoint& ep = item.second;
        if (!ep.isIPv4() || !ep.isValid() || not_in.count(ep.getIP() & 0x0000ffff))
            continue;
        int64 sinceLastSeen = GetAdjustedTime() - ep.getTime();
        int64 sinceLastTry = GetAdjustedTime() - ep.getLastTry();
        
        // Randomize the order in a deterministic way, putting the standard port first
        int64 randomizer = (uint64)(start_time * 4951 + ep.getLastTry() * 9567851 + ep.getIP() * 7789) % (2 * 60 * 60);
        if (ep.getPort() != htons(_defaultPort))
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
        //        if (vNodes.size() >= 2 && sinceLastSeen > 24 * 60 * 60)
        //  continue;
        
        // Only try the old stuff if we don't have enough connections
        //if (vNodes.size() >= 8 && sinceLastSeen > 24 * 60 * 60)
        //    continue;
        
        if (start_time > 0 && sinceLastSeen > 24 * 60 * 60)
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
 */
}
/*
bool EndpointPool::writeEndpoint(const Endpoint& endpoint)
{
    return Write(make_pair(string("addr"), endpoint.getKey()), endpoint);
}

bool EndpointPool::eraseEndpoint(const Endpoint& endpoint)
{
    return Erase(make_pair(string("addr"), endpoint.getKey()));
}

bool EndpointPool::loadEndpoints(const string dataDir)
{
    // Load user provided addresses
    CAutoFile filein = fopen((dataDir + "/addr.txt").c_str(), "rt");
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
    Dbc* pcursor = CDB::GetCursor();
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
*/