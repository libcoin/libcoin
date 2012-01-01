
#ifndef FILTER_H
#define FILTER_H

#include "btcNode/MessageHeader.h"

#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>

class Peer;

class Message
{
public:
    Message() : _header() {}
    Message(std::string& p) : _header(), _payload(p) {}
    Message(std::string c, std::string& p) : _header(c.c_str(), p.size()), _payload(p) { }
    
    const std::string command() const { return _header.GetCommand(); }
    
    void command(int i, char c) { ((char*)(&_header.pchCommand))[i] = c; }
    void messageSize(int i, char c) { ((char*)(&_header.nMessageSize))[i] = c; }
    void checksum(int i, char c) { ((char*)(&_header.nChecksum))[i] = c; }
    
    const std::string& payload() const { return _payload; }
    std::string& payload() { return _payload; }
    
    const MessageHeader& header() const { return _header; }
    MessageHeader& header() { return _header; }

    unsigned char* header_ptr() {return (unsigned char*)&_header; }
    
    static const char start(int i) { return pchMessageStart[i]; }
    
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

typedef std::vector<std::string> Commands;

class Filter
{
public:
    /// The actual filter call - you need to overload this to implement a filter
    virtual bool operator()(Peer* origin, Message& msg) = 0;
    
    /// Returns a list of commands that are processed by this filter
    virtual Commands commands() { std::vector<std::string> c; return c; }
};

typedef boost::shared_ptr<Filter> filter_ptr;

#endif // FILTER_H
