
#ifndef VERSIONFILTER_H
#define VERSIONFILTER_H

#include "Filter.h"

#include <string>

class VersionFilter : public Filter
{
public:
    VersionFilter() {}
    
    virtual bool operator()(Peer* origin, Message& msg);

    virtual std::set<std::string> commands() {
        std::set<std::string> c; 
        c.insert("version");
        c.insert("verack");
        return c;
    }
};

#endif // VERSIONFILTER_H
