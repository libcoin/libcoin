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

#ifndef VERSIONFILTER_H
#define VERSIONFILTER_H

#include <coinChain/Export.h>
#include <coinChain/Filter.h>

#include <string>

class BlockChain;

class COINCHAIN_EXPORT VersionFilter : public Filter
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
