
#ifndef RPC_H
#define RPC_H

#include <string>
#include <boost/lexical_cast.hpp>
#include "coinHTTP/Method.h"
#include "coinHTTP/Reply.h"

#include "json/json_spirit.h"

class RPC
{
public:
    enum Error {
        unknown_error = -1,
        invalid_request = -32600,
        method_not_found = -36001,
        invalid_params = -36002,
        internal_error = -36003,
        parse_error = -32700
    };
    
    static json_spirit::Object error(Error e, const std::string message = "");
    
    static std::string content(std::string method, std::vector<std::string> params);

    static json_spirit::Object reply(std::string content);
    
    RPC();
    
    void parse(std::string payload);

    //    void setContent(string& content);
    
    std::string& getContent();
    
    void setError(const json_spirit::Value& error);

    const Reply::status_type getStatus();
    
    const std::string& method();
    
    void execute(Method& method);
    
private:
    std::string _method;
    std::string _content;
    json_spirit::Value _id;
    json_spirit::Value _error;
    json_spirit::Array _params;
    json_spirit::Value _result;
};

#endif