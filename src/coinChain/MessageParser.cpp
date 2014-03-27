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

#include <sstream>
#include <coinChain/Filter.h>
#include <coinChain/MessageParser.h>
#include <coinChain/MessageHeader.h>

#include <coin/util.h>

using namespace std;
using namespace boost;

MessageParser::MessageParser() : _state(start_1) {
}

void MessageParser::reset() {
    _state = start_1;
}

tribool MessageParser::consume(const Chain& chain, Message& msg, char input)
{
    switch (_state) {
        case start_1:
            if (input != (char)chain.magic()[0]) {
                log_warn("\n\nPROCESSMESSAGE MESSAGESTART NOT FOUND, got: %d\n\n", input);
                return false;
            }
            else {
                _state = start_2;
                msg.header()._magic[0] = input;
                return indeterminate;
            }
        case start_2:
            if (input != (char)chain.magic()[1]) {
                log_warn("\n\nPROCESSMESSAGE MESSAGESTART NOT FOUND\n\n");
                reset();
                return false;
            }
            else {
                _state = start_3;
                msg.header()._magic[1] = input;
                return indeterminate;
            }
        case start_3:
            if (input != (char)chain.magic()[2]) {
                log_warn("\n\nPROCESSMESSAGE MESSAGESTART NOT FOUND\n\n");
                reset();
                return false;
            }
            else {
                _state = start_4;
                msg.header()._magic[2] = input;
                return indeterminate;
            }
        case start_4:
            if (input != (char)chain.magic()[3]) {
                log_warn("\n\nPROCESSMESSAGE MESSAGESTART NOT FOUND, got: %d\n\n", input);
                reset();
                return false;
            }
            else {
                _state = command;
                _counter = 0;
                msg.header()._magic[3] = input;
                return indeterminate;
            }
        case command:
            msg.command(_counter++, input);
            if(_counter < 12)
                return indeterminate;
            else {
                // Version 0.2 obsoletes 20 Feb 2012
                if (UnixTime::s() > 1329696000)
                    _checksum = true;
                else
                    _checksum = (msg.command() != "version" && msg.command() != "verack");
                _state = messagesize;
                _counter = 0;
                return indeterminate;
            }
        case messagesize:
            msg.messageSize(_counter++, input);
            if(_counter < 4)
                return indeterminate;
            else {
                if(_checksum)
                    _state = checksum;
                else {
                    if (!msg.header().IsValid(chain)) {
                        log_warn("\n\nPROCESSMESSAGE: ERRORS IN HEADER %s\n\n\n", msg.command().c_str());
                        return false;
                    }
                    if (msg.header().nMessageSize) {
                        _state = payload;
                        msg.payload().clear();
                    }
                    else {
                        reset();
                        return true;
                    }
                }
                _counter = 0;
                return indeterminate;
            }
        case checksum:
            msg.checksum(_counter++, input);
            if(_counter < 4)
                return indeterminate;
            else {
                if (!msg.header().IsValid(chain)) {
                    log_warn("\n\nPROCESSMESSAGE: ERRORS IN HEADER %s\n\n\n", msg.command().c_str());
                    return false;
                }
                if (msg.header().nMessageSize) {
                    _state = payload;
                    msg.payload().clear();
                }
                else {
                    reset();
                    return true;
                }
            }
            _counter = 0;
            return indeterminate;
        case payload:
            msg.payload().push_back(input);
            if(msg.payload().size() < msg.header().nMessageSize)
                return indeterminate;
            else {
                if (_checksum) {
                    //                    cout << "payload size: " << msg.payload().size() << " " << msg.payload().end() - msg.payload().begin() << endl; 
                    uint256 hash = Hash(msg.payload().begin(), msg.payload().end());
                    //                    uint256 hash = Hash(msg.payload().begin(), msg.payload().begin() + msg.header().nMessageSize);
                    unsigned int nChecksum = 0;
                    memcpy(&nChecksum, &hash, sizeof(nChecksum));
                    if (nChecksum != msg.header().nChecksum) {
                        log_warn("ProcessMessage(%s, %u bytes) : CHECKSUM ERROR nChecksum=%08x hdr.nChecksum=%08x\n",
                               msg.command().c_str(), msg.header().nMessageSize, nChecksum, msg.header().nChecksum);
                        reset();
                        return false;
                    }
                }
                reset();
                return true;
            }
        default:
            return false;
    }
}
