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

#include <coinChain/MessageHandler.h>
#include <coinChain/Peer.h>

//#include <coin/serialize.h>

#include <string>

using namespace std;
using namespace boost;

MessageHandler::MessageHandler() {
}

void MessageHandler::installFilter(filter_ptr filter) {
        _filters.push_back(filter);
}

bool MessageHandler::handleMessage(Peer* origin, Message& msg) {
    log_trace("args origin: %s, command: %s, size: %d", origin->addr.toString(), msg.command(), msg.payload().size());

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
        log_trace("exit: %s", ret ? "true" : "false");
        return ret;
    } 
    catch (OriginNotReady& e) { // Must have a version message before anything else
    }
    catch (std::ios_base::failure& e) {
        if (strstr(e.what(), "end of data")) {
            // Allow exceptions from underlength message on vRecv
            log_warn("ProcessMessage(%s, %u bytes) : Exception '%s' caught, normally caused by a message being shorter than its stated length\n", msg.command().c_str(), msg.payload().size(), e.what());
        }
        else if (strstr(e.what(), "size too large")) {
            // Allow exceptions from overlong size
            log_warn("ProcessMessage(%s, %u bytes) : Exception '%s' caught\n", msg.command().c_str(), msg.payload().size(), e.what());
        }
        else {
            PrintExceptionContinue(&e, "ProcessMessage()");
        }
    }
    //    catch (std::exception& e) {
    //        PrintExceptionContinue(&e, "ProcessMessage()");
    //    }
    //    catch (...) {
    //        PrintExceptionContinue(NULL, "ProcessMessage()");
    //    }
    log_trace("exit: false");
    return false;
}
