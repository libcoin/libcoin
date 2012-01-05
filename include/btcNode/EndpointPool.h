
#ifndef ENDPOINTPOOL_H
#define ENDPOINTPOOL_H

#include "btcNode/db.h"

#include "btcNode/Endpoint.h"

#include <map>
#include <string>
#include <vector>


typedef std::map<std::vector<unsigned char>, Endpoint> EndpointMap;
 
class EndpointPool : protected CDB
{
public:
    EndpointPool(const char* pszMode="cr+") : CDB("addr.dat", pszMode) , _localhost("0.0.0.0", 0, false, NODE_NETWORK), _lastPurgeTime(0) { loadEndpoints(); }
    
    /// Purge old addresses - meant to be called periodically to awoid to much work
    void purge();
    
    /// Add an endpoint with a time penalty in seconds. Returns true if the endpoint was not in the pool already.
    bool addEndpoint(Endpoint endpoint, int64 penalty = 0);

    /// Get a handle to an endpoint.
    Endpoint& getEndpoint(Endpoint::Key key) { return _endpoints[key]; }
    
    /// Get a set with the most recent endpoints.
    std::set<Endpoint> getRecent(int within, int rand_max);

    
    /// update the last seen time of an endpoint.
    void currentlyConnected(const Endpoint& ep);
    
    /// Returns the size of the pool
    const size_t getPoolSize() const { return _endpoints.size(); } 
    
    /// Get good candidate for a new peer
    Endpoint getCandidate(const std::set<unsigned int>& not_in, int64 start_time);
    
    /// Return the external endpoint of the local host
    const Endpoint getLocal() const { return _localhost; }
    
    /// Set the external endpoint of the local host (this is done by e.g. the Chat Client
    void setLocal(Endpoint ep) { _localhost = ep; }
    
private:
    EndpointPool(const EndpointPool&);
    void operator=(const EndpointPool&);

private:
    bool writeEndpoint(const Endpoint& endpoint);
    bool eraseEndpoint(const Endpoint& endpoint);
    bool loadEndpoints();
    
private:
    EndpointMap _endpoints;
    
    Endpoint _localhost;
    int64 _lastPurgeTime;

};

#endif // ENDPOINTPOOL_H