
#ifndef MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H

#include "Filter.h"

#include "boost/noncopyable.hpp"

#include <string>
#include <exception>
#include <map>

typedef std::multimap<std::string, filter_ptr> Filters;

/// The common handler for all incoming messages.
class MessageHandler : private boost::noncopyable
{
public:
    /// Construct
    explicit MessageHandler();
    
    /// Register a command filter
    void installFilter(filter_ptr filter);
    
    /// Handle the message using the installed filters
    bool handleMessage(Message& msg);
    
private:
    Filters _filters;
};

#endif // MESSAGE_HANDLER_HPP
