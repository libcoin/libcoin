
#ifndef ALERTFILTER_H
#define ALERTFILTER_H

#include "btcNode/Filter.h"
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include <string>
#include <set>

class Alert;

class AlertHandler : boost::noncopyable
{
    virtual void operator()(const Alert& alert) = 0;
};

typedef boost::shared_ptr<AlertHandler> alerthandler_ptr;

class AlertFilter : public Filter
{
public:
    AlertFilter() {}
    
    /// Inhierit from AlertHandler and add it to receive alert notifications.
    void addHandler(alerthandler_ptr h) { _alertHandlers.insert(h); }
    
    virtual bool operator()(Message& msg);
    
    virtual std::vector<std::string> commands() {
        std::vector<std::string> c; 
        c.push_back("alert");
        return c;
    }
private:
    std::set<alerthandler_ptr> _alertHandlers;
};

#endif // ALERTFILTER_H
