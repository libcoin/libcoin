
#include "coinHTTP/RPC.h"
#include "coinHTTP/Method.h"

#include <sstream>
#include <string>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/join.hpp>

#include "coinHTTP/json/json_spirit.h"

using namespace std;
using namespace boost;
using namespace json_spirit;


Object RPC::error(RPC::Error e, const string message) {
    Object err;
    err.push_back(Pair("code", (int)e));
    if (message.size()) {
        err.push_back(Pair("message", message));
        return err;
    }
    switch (e) {
        case invalid_request:
            err.push_back(Pair("message", "Invalid Request."));                
            return err;                
        case method_not_found:
            err.push_back(Pair("message", "Method not found."));                
            return err;                
        case invalid_params:
            err.push_back(Pair("message", "Invalid params."));                
            return err;                
        case internal_error:
            err.push_back(Pair("message", "Internal error."));                
            return err;                
        case parse_error:
            err.push_back(Pair("message", "Parse error."));                
            return err;                
        default:
            err.push_back(Pair("message", "Server error."));                
            return err;
    }
}

string RPC::content(string method, vector<string> params) {
    //    '{"jsonrpc": "1.0", "id":"curledhair", "method": "getblockcount", "params": [] }'
    stringstream ss;
    ss << "{\"jsonrpc\":\"2.0\", \"id\":\"libbtc\", \"method\":\"" << method << "\", \"params\":[";
    if (params.size())
        ss << "\"" << algorithm::join(params, "\", \"") << "\"";
    ss << "] }";
    return ss.str();
}

Object RPC::reply(string content) {
    Value val;
    if (!read(content, val))
        throw runtime_error("couldn't parse reply from server");
    return val.get_obj();
}

RPC::RPC() : _id(Value::null), _error(Value::null) {};

void RPC::parse(string payload) {
    // Parse request
    Value parsed_req;
    if (!json_spirit::read(payload, parsed_req) || parsed_req.type() != obj_type)
        throw error(parse_error);
    const Object& request = parsed_req.get_obj();
    
    // Parse id now so errors from here on will have the id
    _id = find_value(request, "id");
    
    // Parse method
    Value method = find_value(request, "method");
    if (method.type() == null_type)
        throw error(parse_error, "Missing method.");
    if (method.type() != str_type)
        throw error(parse_error, "Method must be a string");
    _method = method.get_str();
    
    // Parse params
    Value parse_params = find_value(request, "params");
    if (parse_params.type() == array_type)
        _params = parse_params.get_array();
    else if (parse_params.type() == null_type)
        _params = Array();
    else
        throw error(parse_error, "Params must be an array");        
}

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
    reply.push_back(Pair("jsonrpc", "2.0"));
    reply.push_back(Pair("result", _result));
    reply.push_back(Pair("error", _error));
    reply.push_back(Pair("id", _id));
    _content = write(Value(reply)) + "\n";
    return _content;
}

void RPC::setError(const Value& error) {
    _error = error;
}

const Reply::status_type RPC::getStatus() {
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

void RPC::execute(Method& method) {
    _result = method(_params, false);
}
