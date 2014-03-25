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

#ifndef MESSAGE_PARSER_H
#define MESSAGE_PARSER_H

#include <string>
#include <boost/logic/tribool.hpp>
#include <boost/tuple/tuple.hpp>

#include <coinChain/Export.h>

class Message;

/// Parser for incoming requests.
class COINCHAIN_EXPORT MessageParser
{
public:
    /// Construct ready to parse the message command.
    MessageParser();
    
    /// Reset to initial parser state.
    void reset();
    
    /// Parse some data. The tribool return value is true when a complete request
    /// has been parsed, false if the data is invalid, indeterminate when more
    /// data is required. The InputIterator return value indicates how much of the
    /// input has been consumed.
    template <typename InputIterator>
    boost::tuple<boost::tribool, InputIterator> parse(const Chain& chain, Message& msg, InputIterator begin, InputIterator end) {
        while (begin != end) {
            boost::tribool result = consume(chain, msg, *begin++);
            if (result || !result)
                return boost::make_tuple(result, begin);
        }
        boost::tribool result = boost::indeterminate;
        return boost::make_tuple(result, begin);
    }
    
    template <typename StreamBuf>
    boost::tuple<boost::tribool, std::streamsize> parse(const Chain& chain, Message& msg, StreamBuf& stream_buf) {
        std::streamsize counter = 0;
        while (stream_buf.sgetc() != EOF) {
            boost::tribool result = consume(chain, msg, stream_buf.sbumpc());
            counter++;
            if (result || !result)
                return boost::make_tuple(result, counter);
        }
        boost::tribool result = boost::indeterminate;
        return boost::make_tuple(result, counter);
    }
    
private:
    /// Handle the next character of input.
    boost::tribool consume(const Chain& chain, Message& msg, char input);
        
    /// The current state of the parser.
    enum state {
        start_1,
        start_2,
        start_3,
        start_4, // switch to command and reset size counter
        command, // read the command - increment the counter by one for each read char at 12, then switch to messagesize
        messagesize, // read the message size - increment the counter by one for each read char at 4, then, if version/verack switch to payload otherwise switch to checksum
        checksum, // read the command - increment the counter by one for each read char at 4, then switch to payload
        payload // increment until reaching size, then validate checksum and reset to start_1 and return true
    } _state;
    
    int _counter;
    bool _checksum; // read the checksum - not the case if we are reading version or verack.
};

#endif // MESSAGE_PARSER_H
