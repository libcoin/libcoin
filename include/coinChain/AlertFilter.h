
#ifndef ALERTFILTER_H
#define ALERTFILTER_H

#include "coinChain/Filter.h"
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
    
    virtual bool operator()(Peer* origin, Message& msg);
    
    virtual std::set<std::string> commands() {
        std::set<std::string> c; 
        c.insert("alert");
        c.insert("version");
        return c;
    }
    
private:
    std::set<alerthandler_ptr> _alertHandlers;
};

#endif // ALERTFILTER_H
