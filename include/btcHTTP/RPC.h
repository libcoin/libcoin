
#ifndef RPC_H
#define RPC_H

#include <string>
#include <boost/lexical_cast.hpp>
#include "btcHTTP/Method.h"
#include "btcHTTP/Reply.h"

#include "json/json_spirit.h"

using namespace std;
using namespace boost;
using namespace json_spirit;

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
    
    static Object error(Error e, const string message = "");
    
    RPC();
    
    void parse(string payload);

    //    void setContent(string& content);
    
    string& getContent();
    
    void setError(const Value& error);

    const Reply::status_type getStatus();
    
    const string& method();
    
    void execute(Method& method);
    
private:
    string _method;
    string _content;
    Value _id;
    Value _error;
    Array _params;
    Value _result;
};

#endif