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

#include "coinMine/MinerRPC.h"
#include "coinHTTP/RPC.h"

using namespace std;
using namespace boost;
using namespace json_spirit;



Value SetGenerate::operator()(const Array& params, bool fHelp) {
    if (fHelp || params.size() != 1)
        throw RPC::error(RPC::invalid_params, "setgenerate <generate>\n"
                            "<generate> is true or false to turn generation on or off.");
    
    bool gen = true;
    if (params.size() > 0) {
        if (params[0].type() != json_spirit::bool_type) {
            if (params[0].type() == json_spirit::str_type)
                gen = params[0].get_str() == "true";
        }
        else
            gen = params[0].get_bool();
    }
        
    // atomic - we don't need to create a lock here
    _miner.setGenerate(gen);
    return Value::null;

}        

Value GetGenerate::operator()(const Array& params, bool fHelp) {
    if (fHelp || params.size() != 0)
        throw RPC::error(RPC::invalid_params, "getgenerate\n"
                            "Returns true or false.");
    
    return (bool)_miner.getGenerate();
}        

Value GetHashesPerSec::operator()(const Array& params, bool fHelp) {
    if (fHelp || params.size() != 0)
        throw RPC::error(RPC::invalid_params, "gethashespersec\n"
                            "Returns a recent hashes per second performance measurement while generating.");

    if(_miner.getGenerate())
        return (boost::int64_t)_miner.hashesPerSecond();
    else
        return (boost::int64_t)0;
}        


/*
class signal_set {
public:
    typedef set<int> Signals;
    class Handler {
        virtual void operator()(const boost::system::error_code& ec, int signal_number) = 0;
    };
    
    signal_set(boost::asio::io_service& io_service) : _io_service(io_service) {}
    
    void add(int signal) {
        _signals.insert(signal);
    }
    void async_wait(handler h) {
        // use csignals to install handlers
        for(Signals::iterator s = _signals.begin(); s != s.end(); ++s)
            signal(*s, handler_adapter);
    }
    
private:
    boost::asio::io_service _io_service;
    Signals _signals;
    Handler _handler;
private:
    static void handler_adapter(int signal) {
        
    }
};
*/