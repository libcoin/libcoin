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

#ifndef ALERTFILTER_H
#define ALERTFILTER_H

#include <coinChain/Filter.h>

#include <coinChain/Export.h>

#include <coin/Key.h>

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include <string>
#include <set>

class Alert;

class COINCHAIN_EXPORT AlertHandler : boost::noncopyable
{
    virtual void operator()(const Alert& alert) = 0;
};

typedef boost::shared_ptr<AlertHandler> alerthandler_ptr;

class COINCHAIN_EXPORT AlertFilter : public Filter {
public:
    AlertFilter(const PubKey& pubkey, int version, std::string subversion) : _pub_key(pubkey), _version(version), _sub_version(subversion) {}
    
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
    PubKey _pub_key;
    int _version;
    std::string _sub_version;
};

#endif // ALERTFILTER_H
