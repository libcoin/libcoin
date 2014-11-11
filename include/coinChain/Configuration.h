/* -*-c++-*- libcoin - Copyright (C) 2013 Michael Gronager
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

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <coinChain/Chain.h>
#include <coinChain/Node.h>

#include <boost/program_options.hpp>

typedef std::vector<std::string> strings;

class Configuration {
public:
    Configuration(int argc, char* argv[], const boost::program_options::options_description& extra);
    const Currency& currency() const {
        return *_chain;
    }
    const Chain& chain() const {
        return dynamic_cast<const Chain&>(*_chain);
    }
    std::string url() const {
        std::string url = "http://" + _rpc_connect + ":" + boost::lexical_cast<std::string>(_rpc_port);
        if(_ssl) url = "https://" + _rpc_connect + ":" + boost::lexical_cast<std::string>(_rpc_port);
        return url;
    }
    std::string user() const {
        return _rpc_user;
    }
    std::string pass() const {
        return _rpc_pass;
    }
    std::string bind() const {
        return _rpc_bind;
    }
    unsigned short port() const {
        return _rpc_port;
    }
    bool ssl() const {
        return _ssl;
    }
    std::string certchain() const {
        return _certchain;
    }
    std::string privkey() const {
        return _privkey;
    }
    std::string method() const {
        return _method;
    }
    std::string param(size_t i) const {
        if (_params.size())
            return _params[i];
        else
            return "";
    }
    const strings& params() const {
        return _params;
    }
    std::string data_dir() const {
        return _data_dir;
    }
    std::string notify() const {
        return _notify;
    }
    std::string listen() const {
        return _listen;
    }
    std::string irc() const {
        return _irc;
    }
    bool portmap() const {
        return _portmap;
    }
    unsigned short node_port() const {
        return _port;
    }
    unsigned int timeout() const {
        return _timeout;
    }
    std::string proxy() const {
        return _proxy;
    }
    bool generate() const {
        return _gen;
    }
    Node::Strictness verification() const {
        return _verification;
    }
    Node::Strictness validation() const {
        return _validation;
    }
    Node::Strictness persistence() const {
        return _persistence;
    }
    bool searchable() const {
        return _searchable;
    }
    bool help() const {
        return _help;
    }
    bool version() const {
        return _version;
    }
    friend std::ostream& operator<<(std::ostream& os, const Configuration&);
private:
    boost::program_options::options_description _visible;
    std::string _config_file;
    std::string _data_dir;
    std::string _log_file;
    std::string _notify;
    unsigned short _port, _rpc_port;
    std::string _rpc_bind, _rpc_connect, _rpc_user, _rpc_pass;
    std::string _method;
    strings _params;
    std::string _proxy;
    std::string _listen;
    std::string _irc;
    bool _portmap, _gen, _ssl;
    Node::Strictness _verification, _validation, _persistence;
    bool _searchable;
    bool _help, _version;
    unsigned int _timeout;
    std::string _certchain, _privkey;
    const Currency* _chain;
    std::ofstream _olog;
};

#endif // CONFIGURATION_H
