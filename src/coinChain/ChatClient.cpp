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

#include <coinChain/ChatClient.h>
#include <coinChain/EndpointPool.h>

#include <coin/Address.h>
#include <coin/util.h>
#include <coin/Logger.h>
//#include <coinChain/Peer.h>

#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace boost;
using namespace boost::asio;
using namespace boost::asio::ip;

ChatClient::ChatClient(boost::asio::io_service& io_service, boost::function<void (void)> new_endpoint_notifier, const std::string& server, EndpointPool& endpointPool, string channel, unsigned int channels, tcp::endpoint proxy) : _resolver(io_service), _socket(io_service), _notifier(new_endpoint_notifier), _name_in_use(false), _server(server), _endpointPool(endpointPool), _channel(channel), _channels(channels), _proxy(proxy) {
    
    // Start an asynchronous resolve to translate the server and service names
    // into a list of endpoints.
    tcp::resolver::query query(server, "irc"); // should we remove irc as service type ?
    _resolver.async_resolve(query, boost::bind(&ChatClient::handle_resolve, this, asio::placeholders::error, asio::placeholders::iterator));
}

void ChatClient::handle_resolve(const system::error_code& err, tcp::resolver::iterator endpoint_iterator) {
    if (!err) {
        // Attempt a connection to the first endpoint in the list. Each endpoint
        // will be tried until we successfully establish a connection.
        tcp::endpoint endpoint = *endpoint_iterator;
        endpoint.port(6667);
        if (_proxy)
            _proxy(_socket).async_connect(endpoint, boost::bind(&ChatClient::handle_connect, this, asio::placeholders::error, ++endpoint_iterator));
        else
            _socket.async_connect(endpoint, boost::bind(&ChatClient::handle_connect, this, asio::placeholders::error, ++endpoint_iterator));
    }
    else {
        log_warn("Error: %s\n", err.message().c_str());
        _socket.close();
        _mode = wait_for_notice;
        tcp::resolver::query query(_server, "irc"); // should we remove irc as service type ?
        _resolver.async_resolve(query, boost::bind(&ChatClient::handle_resolve, this, asio::placeholders::error, asio::placeholders::iterator));
    }
}

void ChatClient::handle_connect(const boost::system::error_code& err, tcp::resolver::iterator endpoint_iterator)
{
    if (!err) {
        // Register handle for a read line handler
        _mode = wait_for_notice;
        async_read_until(_socket, _recv, "\r\n", boost::bind(&ChatClient::handle_read_line, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
    }
    else if (endpoint_iterator != tcp::resolver::iterator()) {
        // The connection failed. Try the next endpoint in the list.
        _socket.close();
        tcp::endpoint endpoint = *endpoint_iterator;
        endpoint.port(6667);
        _socket.async_connect(endpoint, boost::bind(&ChatClient::handle_connect, this, asio::placeholders::error, ++endpoint_iterator));
    }
    else {
        log_warn("Error: %s\n", err.message().c_str());
        _socket.close();
        _mode = wait_for_notice;
        tcp::resolver::query query(_server, "irc"); // should we remove irc as service type ?
        _resolver.async_resolve(query, boost::bind(&ChatClient::handle_resolve, this, asio::placeholders::error, asio::placeholders::iterator));
    }
}

void ChatClient::handle_read_line(const boost::system::error_code& err, size_t bytes_transferred) {
    if (!err) {
        istream is(&_recv);
        string rx;
        do {
            char c;
            is >> noskipws >> c;
            if(c == '\r' || c == '\n') 
                bytes_transferred--;
            else
                rx.push_back(c);
        } while (rx.size() < bytes_transferred);
        //        cout << rx << endl;
        //        _recv.consume(bytes_transferred);
        switch (_mode) {
            case wait_for_notice: {
                // look for "Found your hostname", "using your IP address instead", "Couldn't look up your hostname", "ignoring hostname" in the line
                if (rx.find("Found your hostname") != string::npos ||
                    rx.find("using your IP address instead") != string::npos ||
                    rx.find("Couldn't look up your hostname") != string::npos ||
                    rx.find("ignoring hostname") != string::npos) {
                    _mode = wait_for_motd;
                    
                    if (_endpointPool.getLocal().isRoutable() && !_proxy && !_name_in_use)
                        _my_name = encodeAddress(_endpointPool.getLocal());
                    else
                        _my_name = strprintf("x%u", GetRand(1000000000));
                    
                    std::ostream txstream(&_send);
                    txstream << "NICK " << _my_name << "\r";
                    txstream << "USER " << _my_name << " 8 * : " << _my_name << "\r";

                    async_write(_socket, _send, boost::bind(&ChatClient::handle_write_request, this, boost::asio::placeholders::error));
                }
                break;
            }
            case wait_for_motd: {
                if (rx.find(" 004 ") != string::npos) {
                    _mode = wait_for_ip;
                    
                    std::ostream txstream(&_send);
                    txstream << "USERHOST " << _my_name << "\r";

                    async_write(_socket, _send, boost::bind(&ChatClient::handle_write_request, this, boost::asio::placeholders::error));
                }
                else if (rx.find(" 433 ") != string::npos) { // 
                    log_debug("IRC name already in use\n");
                    _name_in_use = true;
                    // close and rejoin
                    _socket.close();
                    _mode = wait_for_notice;
                    tcp::resolver::query query(_server, "irc"); // should we remove irc as service type ?
                    _resolver.async_resolve(query, boost::bind(&ChatClient::handle_resolve, this, asio::placeholders::error, asio::placeholders::iterator));
                    return;
                }
                break;
            }
            case wait_for_ip: {
                if (rx.find(" 302 ") != string::npos) {
                    _mode = in_chat_room;
                    std::ostream txstream(&_send);                
                    // set my IP:
                    vector<string> words;
                    split(words, rx, is_any_of(" "));
                    if (words.size() > 3) {
                        string str = words[3];
                        if (str.rfind("@") != string::npos) {
                            string host = str.substr(str.rfind("@")+1);
                            
                            // Hybrid IRC used by lfnet always returns IP when you userhost yourself,
                            // but in case another IRC is ever used this should work.
                            log_debug("GetIPFromIRC() got userhost %s\n", host.c_str());
                            Endpoint endpoint(host, 0, true);
                            if (endpoint.isValid()) {
                                _endpointPool.setLocal(endpoint);
                                _my_name = encodeAddress(endpoint);
                                _notifier(); // this will start the Node...
                            }
                            if (_proxy)
                                // re nick
                                txstream << "NICK " << _my_name << "\r";
                            // now join the chat room
                            if (_channels > 1) {
                                // randomly join e.g. #bitcoin00-#bitcoin99
                                int channel_number = GetRandInt(_channels);
                                txstream << setfill('0') << setw(2);
                                txstream << "JOIN #" << _channel << channel_number << "\r";
                                txstream <<  "WHO #" << _channel << channel_number <<  "\r";
                            }
                            else {
                                txstream << "JOIN #" << _channel << "00\r";
                                txstream <<  "WHO #" << _channel << "00\r";
                            }                
                            async_write(_socket, _send, boost::bind(&ChatClient::handle_write_request, this, asio::placeholders::error));
                            break;
                        }
                    }
                }
            }    
            case in_chat_room: {
                if (rx.empty() || rx.size() > 900)
                    break;
                
                vector<string> words;
                split(words, rx, is_any_of(" "));
                if (words.size() < 2)
                    break;
                
                string name;
                
                if (words[0] == "PING") {
                    string tx = rx;
                    ostream txstream(&_send);
                    tx[1] = 'O'; // change line to PONG
                    tx += '\r';
                    txstream << tx;
                    async_write(_socket, _send, boost::bind(&ChatClient::handle_write_request, this, asio::placeholders::error));
                    break;
                }
                
                if (words[1] == "352" && words.size() >= 8) {
                    // index 7 is limited to 16 characters
                    // could get full length name at index 10, but would be different from join messages
                    name = words[7];
                    log_debug("IRC got who\n");
                }
                
                if (words[1] == "JOIN" && words[0].size() > 1) {
                    // :username!username@50000007.F000000B.90000002.IP JOIN :#channelname
                    name = words[0].substr(1);
                    size_t exclamation_pos = words[0].find("!");
                    if(exclamation_pos != string::npos)
                        name = words[0].substr(1, exclamation_pos-1); // the 1, -1 is due to the colon
                                                                      //                    if (strchr(pszName, '!'))
                                                                      //                        *strchr(pszName, '!') = '\0';
                    log_debug("IRC got join\n");
                }
                
                if (name[0] == 'u') {
                    Endpoint endpoint;
                    if (decodeAddress(name, endpoint)) {
                        endpoint.setTime(GetAdjustedTime());
                        if (_endpointPool.addEndpoint(endpoint, 51 * 60)) {
                            log_debug("IRC got new address: %s\n", endpoint.toString().c_str());
                            _notifier();
                        }
                        //                        nGotIREndpointes++;
                    }
                    else {
                        log_debug("IRC decode failed\n");
                    }
                }
                break;
            }
                
            default:
                break;
        }
        async_read_until(_socket, _recv, "\r\n", boost::bind(&ChatClient::handle_read_line, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
    }
    else {
        // check if we were kicked out, then rejoin
        std::cout << "Error: " << err.message() << "\n";
        // close and rejoin
        _socket.close();
        _mode = wait_for_notice;
        tcp::resolver::query query(_server, "irc"); // should we remove irc as service type ?
        _resolver.async_resolve(query, boost::bind(&ChatClient::handle_resolve, this, asio::placeholders::error, asio::placeholders::iterator));
    }
}

void ChatClient::handle_write_request(const boost::system::error_code& err) {
    if (err) {
        // check if we were kicked out, then rejoin
        log_debug(err.message());
        // close and rejoin - we should add a postpone to this...
        _socket.close();
        _mode = wait_for_notice;
        tcp::resolver::query query(_server, "irc"); // should we remove irc as service type ?
        _resolver.async_resolve(query, boost::bind(&ChatClient::handle_resolve, this, asio::placeholders::error, asio::placeholders::iterator));
    }
}

std::string ChatClient::encodeAddress(const Endpoint& addr) {
    struct ircaddr tmp;
    tmp.ip    = addr.getIP();
    tmp.port  = addr.getPort();
    
    vector<unsigned char> vch(UBEGIN(tmp), UEND(tmp));
    return string("u") + EncodeBase58Check(vch);
}

bool ChatClient::decodeAddress(string str, Endpoint& addr) {
    vector<unsigned char> vch;
    if (!DecodeBase58Check(str.substr(1), vch))
        return false;
    
    struct ircaddr tmp;
    if (vch.size() != sizeof(tmp))
        return false;
    memcpy(&tmp, &vch[0], sizeof(tmp));
    
    addr = Endpoint(tmp.ip, ntohs(tmp.port), NODE_NETWORK);
    return true;
}
