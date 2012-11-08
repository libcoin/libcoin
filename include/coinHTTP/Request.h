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

#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <coinHTTP/Export.h>
#include <coinHTTP/Header.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio/ip/address.hpp>

#include <string>

/// A request received from a client.
struct Request {
    Request() : pending(false) {}
    
    std::string method;
    std::string uri;
    int http_version_major;
    int http_version_minor;
    Headers headers;
    std::string payload;
    
    /// Extra info - the remote IP
    boost::asio::ip::address remote;    

    /// Extra info - the request receive timestamp
    boost::posix_time::ptime timestamp;
    
    /// Extra info - pending: indicates that the procesing of the request have been postponed pending yet unresolved information
    mutable bool pending;
    
    /// reset the request (used for keep_alive)
    void reset() {
        pending = false; // requests are per default not pending.
        method.clear();
        uri.clear();
        headers.clear();
        payload.clear();
    }
};

#endif // HTTP_REQUEST_H
