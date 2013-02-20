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

#include <coinHTTP/json/json_spirit.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio/ip/address.hpp>

#include <string>

/// Request received from a client.

class Request {
public:
    enum ErrorCode {
        unknown_error = -1,
        invalid_request = -32600,
        method_not_found = -36001,
        invalid_params = -36002,
        internal_error = -36003,
        parse_error = -32700
    };

    Request() { reset(); }

    void setMethod(const std::string method);

    void setURI(const std::string& uri);
    
    void setVersion(int major, int minor);
    
    void addHeader(const std::string& field, const std::string& value);
    
    /// Set the content of the request and parse it.
    void setContent(const std::string& content);
    
    bool is_get() const { return _method == "GET"; }
    bool is_post() const { return _method == "POST"; }
    
    const std::string& method() const {
        return _method;
    }
    
    const std::string& uri() const {
        return _uri;
    }
    
    const Headers& headers() const {
        return _headers;
    }
    
    const std::string& content() const {
        return _content;
    }
    
    std::string version() const {
        std::ostringstream is;
        is << _version_major << "." << _version_major;
        return is.str();
    }
    
    std::string path() const {
        size_t search_separator = _uri.find("?");
        if (search_separator != std::string::npos)
            return _uri.substr(0, search_separator);
        else
            return _uri;
    }
    
    std::string query() const {
        size_t search_separator = _uri.find("?");
        if (search_separator != std::string::npos)
            return _uri.substr(search_separator + 1, std::string::npos); // exclude the '?'
        else
            return "";
    }
    
    std::string basic_auth() const;

    json_spirit::Object error(ErrorCode e, const std::string message = "");
    
    /// Extra info - the remote IP
    boost::asio::ip::address remote;    

    /// Extra info - the request receive timestamp
    boost::posix_time::ptime timestamp;
    
    /// reset the request (used for keep_alive)
    void reset();

    std::string mime() const {
        if (_headers.count("content-type"))
            return _headers.find("content-type")->second;
            
        return "";
    }
    
private:
    std::string _method;
    std::string _uri;
    int _version_major;
    int _version_minor;
    Headers _headers;
    std::string _content;
};

#endif // HTTP_REQUEST_H
