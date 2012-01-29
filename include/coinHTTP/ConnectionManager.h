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

#ifndef HTTP_CONNECTION_MANAGER_H
#define HTTP_CONNECTION_MANAGER_H

#include <set>
#include <boost/noncopyable.hpp>
#include "coinHTTP/Connection.h"

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
