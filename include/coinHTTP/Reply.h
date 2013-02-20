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

#ifndef HTTP_REPLY_HPP
#define HTTP_REPLY_HPP

#include <coinHTTP/Export.h>
#include <coinHTTP/Header.h>
#include <coinHTTP/Method.h>

#include <string>

#include <boost/asio.hpp>

/// A reply to be sent to a client.
class Reply {
    Reply& operator= (const Reply& rep);
public:
    Reply(const Request& request) : _request(request), _method(NULL) {}
    
    /// The status of the reply.
    enum Status {
        ok = 200,
        created = 201,
        accepted = 202,
        no_content = 204,
        multiple_choices = 300,
        moved_permanently = 301,
        moved_temporarily = 302,
        not_modified = 304,
        bad_request = 400,
        unauthorized = 401,
        forbidden = 403,
        not_found = 404,
        internal_server_error = 500,
        not_implemented = 501,
        bad_gateway = 502,
        service_unavailable = 503,
        gateway_timeout = 504
    };

    void status(Status s);
    
    Status status() const { return _status; }
    
    std::string& content() {
        return _content;
    }
    
    size_t content_length() const {
        return _content.size();
    }
    
    Headers& headers() {
        return _headers;
    }

    void setContentAndMime(const std::string& content, const std::string& mime);
    
    void setMethod(Method* method) {
        _method = method;
    }
    
    /// dispatch
    typedef boost::function<void (void)> CompletionHandler;
    void dispatch(const CompletionHandler& handler);
    
    void exec(const CompletionHandler& handler);
    
    /// reset the reply (used for keep_alive)
    void reset() {
        _method = NULL;
        _headers.clear();
        _content.clear();
    }
    
    /// Convert the reply into a vector of buffers. The buffers do not own the
    /// underlying memory blocks, therefore the reply object must remain valid and
    /// not be changed until the write operation has completed.
    std::vector<boost::asio::const_buffer> to_buffers();
    
    /// Get a stock reply.
    // static Reply stock_reply(status_type status);
    
private:
    /// The status
    Status _status;
    
    /// The Request generating this reply
    const Request& _request;
    
    /// Optional method for creating the content of this reply
    Method* _method;
    
    /// The headers to be included in the reply.
    Headers _headers;

    /// The content to be sent in the reply.
    std::string _content;
};

#endif // HTTP_REPLY_H
