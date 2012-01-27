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
