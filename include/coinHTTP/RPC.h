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

#ifndef RPC_H
#define RPC_H

#include <coinHTTP/Export.h>
#include <coinHTTP/Method.h>
#include <coinHTTP/Reply.h>

#include "json/json_spirit.h"

#include <string>

#include <boost/lexical_cast.hpp>
#include <boost/system/error_code.hpp>

class COINHTTP_EXPORT RPC
{
public:
    enum Error {
        unknown_error = -1,
        name_not_found = -4,
        invalid_request = -32600,
        method_not_found = -32601,
        invalid_params = -32602,
        internal_error = -32603,
        parse_error = -32700
    };
    
    static json_spirit::Object error(Error e, const std::string message = "");
    json_spirit::Object response(Error e, const std::string message = "");
    
    json_spirit::Object error_response(const json_spirit::Object& error);
    json_spirit::Object result_response(const json_spirit::Value& result);
    
    static std::string content(std::string method, std::vector<std::string> params);

    static json_spirit::Object reply(std::string content);
    
    RPC(const Request& request);
    
    /// parse the content from a application/json - it is assumed that it is formatted according to the JSOC RPC 2.0 spec
    void parse(std::string payload);

    /// parse the content from a text/plain html form post - it is assumed that action = method, and payload is params=<params>
    void parse(std::string action, std::string payload);
    
    /// parse a get request with a query string
    void parse(std::string action, std::vector<std::string> args);

    /// Get content envelope in application/json formmatted for JSON RPC 2.0
    std::string& getContent();
    
    /// Get content envelope for text/plain.
    std::string& getPlainContent();
    
    void setError(const json_spirit::Value& error);

    const Reply::Status getStatus();
    
    const std::string& method();
    
    const json_spirit::Array& params() const {
        return _params;
    }
    
    //    void execute(Method& method);

    typedef boost::function<void (const boost::system::error_code&)> CompletionHandler;
    
    //    void dispatch(const CompletionHandler& handler);
    
    //    void async_execute(const CompletionHandler& handler);
    
    //    void setMethod(Method* method) { _call_method = method; }
    
private:
    std::string _method;
    std::string _content;
    json_spirit::Value _id;
    std::string _version;
    json_spirit::Value _error;
    json_spirit::Array _params;
    json_spirit::Value _result;
    
    const Request& _request;
    //    Reply& _reply;
    //    Method* _call_method;
};

#endif
