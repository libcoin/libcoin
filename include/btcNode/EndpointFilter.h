
#ifndef ENDPOINTFILTER_H
#define ENDPOINTFILTER_H

#include "Filter.h"

#include <string>

class EndpointPool;

class EndpointFilter : public Filter
{
public:
    EndpointFilter(EndpointPool& epp) : _endpointPool(epp) {}
    
    virtual bool operator()(Message& msg);
    
    virtual std::vector<std::string> commands() {
        std::vector<std::string> c; 
        c.push_back("addr");
        c.push_back("getaddr");
        // these are only handled to update their last activity...
        c.push_back("version");
        c.push_back("inv");
        c.push_back("getdata");
        c.push_back("ping");
        return c;
    }
    
private:
    EndpointPool& _endpointPool;    
};

#endif // BLOCKFILTER_H
