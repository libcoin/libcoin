
#ifndef FILTER_H
#define FILTER_H

#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>

class CNode;
class CDataStream;

struct Message
{
    Message(CNode* o, std::string c, CDataStream& p) : origin(o), command(c), payload(p) {}
    CNode* origin;
    std::string command;
    CDataStream& payload;    
};

/// Throw this if the origin has nVersion == 0.
class OriginNotReady: public std::exception
{
    virtual const char* what() const throw() {
        return "Origin must have a version before anything else";
    }
};

typedef std::vector<std::string> Commands;

class Filter
{
public:
    /// The actual filter call - you need to overload this to implement a filter
    virtual bool operator()(Message& msg) = 0;
    
    /// Returns a list of commands that are processed by this filter
    virtual Commands commands() { std::vector<std::string> c; return c; }
};

typedef boost::shared_ptr<Filter> filter_ptr;

#endif // FILTER_H
