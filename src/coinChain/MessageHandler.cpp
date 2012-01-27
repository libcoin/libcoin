
#include "coinChain/MessageHandler.h"
#include "coin/serialize.h"

#include <string>

using namespace std;
using namespace boost;

MessageHandler::MessageHandler() {
}

void MessageHandler::installFilter(filter_ptr filter) {
        _filters.push_back(filter);
}

bool MessageHandler::handleMessage(Peer* origin, Message& msg) {

    try {
        bool ret = false;
        for(Filters::iterator filter = _filters.begin(); filter != _filters.end(); ++filter) {
            if ((*filter)->commands().count(msg.command())) {
                // copy the string
                string payload(msg.payload());
                //                Message message(msg.command(), payload);
                Message message(msg);
                // We need only one successfull command to return true
                if ( (**filter)(origin, message) ) ret = true;
            }
        }
        return ret;
    } catch (OriginNotReady e) { // Must have a version message before anything else
        return false;        
    }
}
