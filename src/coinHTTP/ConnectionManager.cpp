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

#include "coinHTTP/ConnectionManager.h"
#include <algorithm>
#include <boost/bind.hpp>

void ConnectionManager::start(connection_ptr c) {
    _connections.insert(c);
    c->start();
}

void ConnectionManager::stop(connection_ptr c) {
    _connections.erase(c);
    c->stop();
}

void ConnectionManager::stop_all() {
    //    std::for_each(_connections.begin(), _connections.end(), boost::bind(&Connection::stop, _1));
    for(std::set<connection_ptr>::iterator c = _connections.begin(); c != _connections.end(); ++c)
        (*c)->stop();
    _connections.clear();
}
