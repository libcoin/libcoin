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

#ifndef FILTER_H
#define FILTER_H

#include <coinChain/MessageHeader.h>

#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>

class Peer;

class Message
{
public:
    Message() : _header() {}
    //    Message(std::string& p) : _header(), _payload(p) {}
    Message(const Message& message) : _header(message._header), _payload(message._payload) { }
    //    Message(std::string c, std::string& p) : _header(c.c_str(), p.size()), _payload(p) { }
    
    const std::string command() const { return _header.GetCommand(); }
    
    void command(int i, char c) { ((char*)(&_header.pchCommand))[i] = c; }
    void messageSize(int i, char c) { ((char*)(&_header.nMessageSize))[i] = c; }
    void checksum(int i, char c) { ((char*)(&_header.nChecksum))[i] = c; }
    
    const std::string& payload() const { return _payload; }
    std::string& payload() { return _payload; }
    
    const MessageHeader& header() const { return _header; }
    MessageHeader& header() { return _header; }

    unsigned char* header_ptr() {return (unsigned char*)&_header; }
    
private:
    MessageHeader _header;
    std::string _payload;
};

/// Throw this if the origin has nVersion == 0.
class OriginNotReady: public std::exception
{
    virtual const char* what() const throw() {
        return "Origin must announce it's version before we want to chat.";
    }
};

typedef std::set<std::string> Commands;

class Filter
{
public:
    /// The actual filter call - you need to overload this to implement a filter
    virtual bool operator()(Peer* origin, Message& msg) = 0;
    
    /// Returns a list of commands that are processed by this filter
    virtual Commands commands() { std::set<std::string> c; return c; }
    
    virtual ~Filter() {}
};

typedef boost::shared_ptr<Filter> filter_ptr;

#endif // FILTER_H
