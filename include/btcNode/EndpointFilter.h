
#ifndef ENDPOINTFILTER_H
#define ENDPOINTFILTER_H

#include "Filter.h"

#include <string>

class EndpointPool;

class EndpointFilter : public Filter
{
public:
    EndpointFilter(EndpointPool& epp) : _endpointPool(epp) {}
    
    virtual bool operator()(Peer* origin, Message& msg);
    
    virtual std::set<std::string> commands() {
        std::set<std::string> c; 
        c.insert("addr");
        c.insert("getaddr");
        c.insert("version");
        // these are only handled to update their last activity...
        c.insert("inv");
        c.insert("getdata");
        c.insert("ping");
        return c;
    }
    
private:
    EndpointPool& _endpointPool;    
};

#endif // BLOCKFILTER_H
