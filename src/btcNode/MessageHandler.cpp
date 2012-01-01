
#include "btcNode/MessageHandler.h"
#include "btc/serialize.h"

#include <string>

using namespace std;
using namespace boost;

MessageHandler::MessageHandler() {
}

void MessageHandler::installFilter(filter_ptr filter) {
    Commands commands = filter->commands();
    for (Commands::iterator cmd = commands.begin(); cmd != commands.end(); ++cmd) {
        _filters.insert(make_pair(*cmd,filter));
    }
}

bool MessageHandler::handleMessage(Peer* origin, Message& msg) {

    try {
        if(_filters.count(msg.command())) { // do something
            bool ret = false;
            pair<Filters::iterator, Filters::iterator> cmds = _filters.equal_range(msg.command());
            multimap<string, int>::iterator it; //Iterator to be used along with ii
            for(Filters::iterator cmd = cmds.first; cmd != cmds.second; ++cmd) {
                // copy the string
                string payload(msg.payload());
                Message message(msg.command(), payload);
                // We need only one successfull command to return true
                if ( (*cmd->second)(origin, message) ) ret = true;
            }
            return ret;
        }
        else
            return false;
    } catch (OriginNotReady e) { // Must have a version message before anything else
        return false;        
    }
}
