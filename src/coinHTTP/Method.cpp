
#include "coinHTTP/Method.h"
#include "coinHTTP/RPC.h"
#include "coinHTTP/Server.h"

#include <string>
#include <algorithm>

using namespace std;
using namespace boost;
using namespace json_spirit;

const string Method::name() const {
    string n = typeid(*this).name();
    // remove trailing numbers from the typeid
    while (n.find_first_of("0123456789") == 0)
        n.erase(0, 1);
    transform(n.begin(), n.end(), n.begin(), ::tolower);
    return n;
};

Value Stop::operator()(const Array& params, bool fHelp) {
    if (fHelp || params.size() != 0)
        throw RPC::error(RPC::invalid_params, "stop\n"
                         "Stop bitcoin server.");
    
    _server.shutdown();
    return "Node and Server is stopping";
}    

