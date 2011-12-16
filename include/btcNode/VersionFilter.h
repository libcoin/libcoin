
#ifndef VERSIONFILTER_H
#define VERSIONFILTER_H

#include "Filter.h"

#include <string>

class VersionFilter : public Filter
{
public:
    VersionFilter() {}
    
    virtual bool operator()(Message& msg);

    virtual std::vector<std::string> commands() {
        std::vector<std::string> c; 
        c.push_back("version");
        c.push_back("verack");
        return c;
    }
};

#endif // VERSIONFILTER_H
