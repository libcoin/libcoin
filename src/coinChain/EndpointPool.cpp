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

    query("PRAGMA journal_mode=WAL");
    query("PRAGMA locking_mode=NORMAL");
    query("PRAGMA synchronous=OFF");
    query("PRAGMA temp_store=MEMORY");
    query("CREATE TABLE IF NOT EXISTS Endpoints(ip INTEGER NOT NULL, port INTEGER NOT NULL, time INTEGER, lasttry INTEGER, PRIMARY KEY (ip, port))");

}


void EndpointPool::purge()
{    
    if (_lastPurgeTime == 0)
        _lastPurgeTime = UnixTime::s();
    
    if (UnixTime::s() - _lastPurgeTime > 10 * 60) {
        _lastPurgeTime = UnixTime::s();

        // remove endpoints older than 14 days:
        int64_t since = GetAdjustedTime() - 14 * 24 * 60 * 60;
        query("DELETE FROM Endpoints WHERE time < ?", since);
    }
}

set<Endpoint> EndpointPool::getRecent(int within, int rand_max)
{
    int64_t since = GetAdjustedTime() - within; // in the last 3 hours
    
    vector<Endpoint> endpoint_list= queryColRow<Endpoint(unsigned int, unsigned short, unsigned int, unsigned int)>("SELECT ip, port, time, lasttry FROM Endpoints WHERE time > ? ORDER BY RANDOM() LIMIT ?", since, rand_max); // rows will now contain a list of endpoint newer than 'since'

    set<Endpoint> endpoints(endpoint_list.begin(), endpoint_list.end());
    // now generate loop over the rows and create a set of endpoints:
    
    return endpoints;
}

bool EndpointPool::addEndpoint(Endpoint endpoint, int64_t penalty)
{
    if (!endpoint.isRoutable())
        return false;
    if (endpoint.getIP() == _localhost.getIP())
        return false;
    endpoint.setTime(max((int64_t)0, (int64_t)endpoint.getTime() - penalty));
    Endpoint found = endpoint;
    
    query("INSERT OR REPLACE INTO Endpoints VALUES (?, ?, ?, ?)", (int64_t)endpoint.getIP(), (int64_t)endpoint.getPort(), (int64_t)endpoint.getTime(), 0);
    
    return true;
}

void EndpointPool::currentlyConnected(const Endpoint& ep)
{
    int64_t now = GetAdjustedTime();

    query("UPDATE Endpoints SET time = ? WHERE ip = ? AND port = ?", now, (int64_t)ep.getIP(), ep.getPort());
}

void EndpointPool::setLastTry(const Endpoint& ep) {
    int64_t now = GetAdjustedTime();
    query("UPDATE Endpoints SET lasttry = ? WHERE ip = ? AND port = ?", now, (int64_t)ep.getIP(), ep.getPort());
}

Endpoint EndpointPool::getCandidate(const set<unsigned int>& not_in, int64_t start_time)
{
    //
    // Choose an address to connect to based on most recently seen
    //
    Endpoint candidate;
    
    int64_t now = GetAdjustedTime();
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
}
