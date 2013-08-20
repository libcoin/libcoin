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

#include <coin/util.h>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

using namespace std;
using namespace boost;
using namespace sqliterate;

EndpointPool::EndpointPool(short defaultPort, const std::string dataDir, const char* pszMode) :
    Database(dataDir == "" ? ":memory:" : dataDir+"/endpoints.sqlite3") /*CDB(dataDir, "addr.dat", pszMode)*/ ,
        _defaultPort(defaultPort),
        _localhost("0.0.0.0", defaultPort, false, NODE_NETWORK),
    _lastPurgeTime(0) {

    query("CREATE TABLE IF NOT EXISTS Endpoints(ip INTEGER NOT NULL, port INTEGER NOT NULL, time INTEGER, lasttry INTEGER, PRIMARY KEY (ip, port))");

}


void EndpointPool::purge()
{    
    if (_lastPurgeTime == 0)
        _lastPurgeTime = UnixTime::s();
    
    if (UnixTime::s() - _lastPurgeTime > 10 * 60) {
        _lastPurgeTime = UnixTime::s();

        // remove endpoints older than 14 days:
        int64 since = GetAdjustedTime() - 14 * 24 * 60 * 60;
        query("DELETE FROM Endpoints WHERE time < ?", since);
    }
}

set<Endpoint> EndpointPool::getRecent(int within, int rand_max)
{
    int64 since = GetAdjustedTime() - within; // in the last 3 hours
    
    vector<Endpoint> endpoint_list= queryColRow<Endpoint(unsigned int, unsigned short, unsigned int, unsigned int)>("SELECT ip, port, time, lasttry FROM Endpoints WHERE time > ? ORDER BY RANDOM() LIMIT ?", since, rand_max); // rows will now contain a list of endpoint newer than 'since'

    set<Endpoint> endpoints(endpoint_list.begin(), endpoint_list.end());
    // now generate loop over the rows and create a set of endpoints:
    
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
    
    query("INSERT OR REPLACE INTO Endpoints VALUES (?, ?, ?, ?)", (int64)endpoint.getIP(), (int64)endpoint.getPort(), (int64)endpoint.getTime(), 0);
    
    return true;
}

void EndpointPool::currentlyConnected(const Endpoint& ep)
{
    int64 now = GetAdjustedTime();

    query("UPDATE Endpoints SET time = ? WHERE ip = ? AND port = ?", now, (int64)ep.getIP(), ep.getPort());
}

void EndpointPool::setLastTry(const Endpoint& ep) {
    int64 now = GetAdjustedTime();
    query("UPDATE Endpoints SET lasttry = ? WHERE ip = ? AND port = ?", now, (int64)ep.getIP(), ep.getPort());
}

Endpoint EndpointPool::getCandidate(const set<unsigned int>& not_in, int64 start_time)
{
    //
    // Choose an address to connect to based on most recently seen
    //
    Endpoint candidate;
    
    int64 now = GetAdjustedTime();
    vector<Endpoint> endpoints = queryColRow<Endpoint(unsigned int, unsigned short, unsigned int, unsigned int)>("SELECT ip, port, time, lasttry FROM Endpoints WHERE lasttry < ? ORDER BY time DESC", now-60*60);
    
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