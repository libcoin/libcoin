
#ifndef HTTP_CONNECTION_MANAGER_H
#define HTTP_CONNECTION_MANAGER_H

#include <set>
#include <boost/noncopyable.hpp>
#include "btcHTTP/Connection.h"

/// Manages open connections so that they may be cleanly stopped when the server
/// needs to shut down.
class ConnectionManager : private boost::noncopyable
{
public:
    /// Add the specified connection to the manager and start it.
    void start(connection_ptr c);
    
    /// Stop the specified connection.
    void stop(connection_ptr c);
    
    /// Stop all connections.
    void stop_all();
    
private:
    /// The managed connections.
    std::set<connection_ptr> _connections;
};

#endif // HTTP_CONNECTION_MANAGER_H
