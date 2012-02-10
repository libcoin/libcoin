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

#ifndef MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H

#include <coinChain/Filter.h>

#include "boost/noncopyable.hpp"

#include <string>
#include <exception>
#include <map>

typedef std::vector<filter_ptr> Filters;

/// The common handler for all incoming messages.
class MessageHandler : private boost::noncopyable
{
public:
    /// Construct
    explicit MessageHandler();
    
    /// Register a command filter
    void installFilter(filter_ptr filter);
    
    /// Handle the message using the installed filters
    bool handleMessage(Peer* origin, Message& msg);
    
private:
    Filters _filters;
};

#endif // MESSAGE_HANDLER_HPP
