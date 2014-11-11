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

#ifndef CHATCLIENT_H
#define CHATCLIENT_H

#include <coinChain/Export.h>

#include <coinChain/Proxy.h>

#include <string>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>

class Endpoint;
class EndpointPool;

/// The Chatclient handles the communication with the IRC server <server>. It connect to the channel <channel>,
/// if <channels> is more than one, one is picked at random. ChatClient resides in the same run loop as the Node,
/// and notifies Node through the notify function. The first notification is send once the public IP is known.
/// The next follows every time a new address is added. This facilittates the bootstrap of the Node if started 
/// with no known addresses. 

class COINCHAIN_EXPORT ChatClient
{
public:
    ChatClient(boost::asio::io_service& io_service, boost::function<void (void)> new_endpoint_notifier, const std::string& server, EndpointPool& endpointPool, std::string channel, unsigned int channels, boost::asio::ip::tcp::endpoint proxy = boost::asio::ip::tcp::endpoint());
    
    ~ChatClient() {
        boost::system::error_code ec;
        _socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    }
private:
    void handle_resolve(const boost::system::error_code& err, boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
    void handle_connect(const boost::system::error_code& err, boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
    void handle_read_line(const boost::system::error_code& err, std::size_t bytes_transferred);    
    void handle_write_request(const boost::system::error_code& err);
    
    std::string encodeAddress(const Endpoint& addr);
    bool decodeAddress(std::string str, Endpoint& addr);
    
#pragma pack(push, 1)
    struct ircaddr {
        int ip;
        short port;
    };
#pragma pack(pop)
    
private:
    boost::asio::ip::tcp::resolver _resolver;
    boost::asio::ip::tcp::socket _socket;
    boost::asio::streambuf _send;
    boost::asio::streambuf _recv;
    boost::function<void (void)> _notifier;
    
    enum {
        wait_for_notice,
        wait_for_motd,
        wait_for_ip,
        in_chat_room
    } _mode;
    
    std::string _my_name;
    bool _name_in_use;
    const std::string _server;
    EndpointPool& _endpointPool;
    std::string _channel;
    unsigned int _channels;
    Proxy _proxy;
};

#endif // CHATCLIENT_H