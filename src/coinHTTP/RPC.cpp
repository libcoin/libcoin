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

#include <coinHTTP/RPC.h>
#include <coinHTTP/Method.h>
#include <coinHTTP/Request.h>

#include <sstream>
#include <string>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/bind.hpp>

#include <coinHTTP/json/json_spirit.h>

using namespace std;
using namespace boost;
using namespace json_spirit;


Object RPC::error(RPC::Error e, const string message) {
    Object err;
    err.push_back(Pair("code", (int)e));
    if (message.size()) {
        err.push_back(Pair("message", message));
    }
    else {
        switch (e) {
            case invalid_request:
                err.push_back(Pair("message", "Invalid Request."));
                break;
            case method_not_found:
                err.push_back(Pair("message", "Method not found."));
                break;
            case invalid_params:
                err.push_back(Pair("message", "Invalid params."));
                break;
            case internal_error:
                err.push_back(Pair("message", "Internal error."));
                break;
            case parse_error:
                err.push_back(Pair("message", "Parse error."));
                break;
            default:
                err.push_back(Pair("message", "Server error."));
                break;
        }
    }
    
    return err;
}

Object RPC::response(RPC::Error e, const string message) {
    Object err = error(e, message);
    return error_response(err);
}

json_spirit::Object RPC::error_response(const json_spirit::Object& err) {
    Object response;

    if (_version.size())
        response.push_back(Pair("jsonrpc", _version));
    response.push_back(Pair("error", err));
    if (_version != "2.0") {
        response.push_back(Pair("result", Value::null));
        response.push_back(Pair("id", _id));
    }
    else if (!_id.is_null())
        response.push_back(Pair("id", _id));
    return response;
}

json_spirit::Object RPC::result_response(const json_spirit::Value& result) {
    Object response;
    
    if (_version.size())
        response.push_back(Pair("jsonrpc", _version));
    response.push_back(Pair("result", result));
    if (_version != "2.0") {
        response.push_back(Pair("error", Value::null));
        response.push_back(Pair("id", _id));
    }
    else if (!_id.is_null())
        response.push_back(Pair("id", _id));
    return response;
}


string RPC::content(string method, vector<string> params) {
    //    '{"jsonrpc": "1.0", "id":"curledhair", "method": "getblockcount", "params": [] }'
    stringstream ss;
    ss << "{\"jsonrpc\":\"2.0\", \"method\":\"" << method << "\", \"params\":[";
    Value val;
    // stringify nonparsable params - i.e. assume it is a string if nothing else. We use [ ] to ensure the entire string is read and not just the first few numbers of a longer string
    for (vector<string>::iterator param = params.begin(); param != params.end(); ++param)
        if (!read("[" + *param + "]", val)) *param = "\"" + *param + "\"";
    if (params.size())
        //        ss << "\"" << algorithm::join(params, "\", \"") << "\"";
        ss << algorithm::join(params, ",");
    ss << "] }";
    return ss.str();
}

Object RPC::reply(string content) {
    Value val;
    if (!read(content, val))
        throw runtime_error("couldn't parse reply from server");
    return val.get_obj();
}

//RPC::RPC(const Request& request, Reply& reply) : _id(Value::null), _error(Value::null), _request(request), _reply(reply) {}

RPC::RPC(const Request& request) : _id(Value::null), _error(Value::null), _request(request) {
    if (_request.is_post() && _request.mime() == "application/json")
        parse(_request.content());
    else if (_request.is_get())
        parse(_request.path(), _request.query());
    // alternatively everything will be void...
}


void RPC::parse(string payload) {
    // Parse request
    Value parsed_req;
    if (!json_spirit::read(payload, parsed_req) || parsed_req.type() != obj_type)
        throw response(parse_error);
    const Object& request = parsed_req.get_obj();
    
    // Parse the version - can be e.g. 1.0 or 2.0, or even empty (meaning 1.0)
    Value version = find_value(request, "jsonrpc");
    if (version.type() == str_type)
        _version = version.get_str();
    
    // Parse id now so errors from here on will have the id
    _id = find_value(request, "id");
    
    // Parse method
    Value method = find_value(request, "method");
    if (method.type() == null_type)
        throw response(parse_error, "Missing method.");
    if (method.type() != str_type)
        throw response(parse_error, "Method must be a string");
    _method = method.get_str();
    
    // Parse params
    Value parse_params = find_value(request, "params");
    if (parse_params.type() == array_type)
        _params = parse_params.get_array();
    else if (parse_params.type() == null_type)
        _params = Array();
    else
        throw response(parse_error, "Params must be an array");
}

void RPC::parse(std::string action, std::string payload) {
    _method = action;
    if (payload.find("params=") == 0) {
        string param_line = payload.substr(7);
        vector<string> params;
        split(params, param_line, is_any_of("+ ")); // html forms use "+" instead of " " !
        _params = Array();
        BOOST_FOREACH(const string& param, params) {
            _params.push_back(param);
        }
    }
}

void RPC::parse(std::string action, std::vector<std::string> args) {
    _method = action;
    _params = Array();
    BOOST_FOREACH(const string& arg, args) {
        _params.push_back(arg);
    }
}
/*
void RPC::parse(std::string path, std::string query) {
    // get the command
    _method = action;
    _params = Array();
    BOOST_FOREACH(const string& arg, args) {
        _params.push_back(arg);
    }
}
*/
/*
void RPC::setContent(string& content) {
    // Generate JSON RPC 2.0 reply
    Object reply;
    reply.push_back(Pair("jsonrpc", "2.0"));
    reply.push_back(Pair("result", _result));
    reply.push_back(Pair("error", _error));
    reply.push_back(Pair("id", _id));
    content = write(Value(reply)) + "\n";
}
*/

string& RPC::getContent() {
    // Generate JSON RPC 2.0 reply
    Object reply;
    if (_version.size())
        reply.push_back(Pair("jsonrpc", _version));
    if (_version == "2.0") {
        if (!_error.is_null())
            reply.push_back(Pair("error", _error));
        else // if no error, always return the result - also if null
            reply.push_back(Pair("result", _result));
    }
    else { // _version == 1.0 (or something else)
        reply.push_back(Pair("error", _error));
        reply.push_back(Pair("result", _result));
    }
    if (!_id.is_null())
        reply.push_back(Pair("id", _id));
    _content = write(Value(reply)) + "\n";
    return _content;
}

string& RPC::getPlainContent() {
    // Generate text/plain reply
    if(_error.is_null())
        _content = write_formatted(_result);
    else
        _content = write_formatted(_error);
    return _content;
}

void RPC::setError(const Value& error) {
    _error = error;
}

const Reply::Status RPC::getStatus() {
    if (_error == Value::null)
        return Reply::ok;
    
    int code = find_value(_error.get_obj(), "code").get_int();
    if (code == invalid_request)
        return Reply::bad_request;
    if (code == method_not_found)
        return Reply::not_found;
    return Reply::internal_server_error;
}

const string& RPC::method() { return _method; } 

/*
void RPC::execute(Method& method) {
    _result = method(_params, false, _request);
}

void RPC::dispatch(const CompletionHandler& handler) {
    _call_method->dispatch(boost::bind(&RPC::async_execute, this, handler));
}

void RPC::async_execute(const CompletionHandler& handler) {
    _result = (*_call_method)(_params, false, _request);
    _reply.content = getContent();
    _reply.headers["Content-Length"] = lexical_cast<string>(_reply.content.size());
    _reply.headers["Content-Type"] = "application/json";
    _reply.status = getStatus();
    boost::system::error_code ec (0, boost::system::system_category());
    handler(ec);
}
*/