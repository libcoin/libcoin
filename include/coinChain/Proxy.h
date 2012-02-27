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

#ifndef PROXY_H
#define PROXY_H

#include <coinChain/Export.h>

#include <boost/asio.hpp>
#include <boost/function.hpp>

#include <string>
#include <boost/noncopyable.hpp>

namespace Socks4 {
    
    const unsigned char version = 0x04;
    
    class Request {
    public:
        enum CommandType {
            connect = 0x01,
            bind = 0x02
        };
        
        Request(CommandType cmd, const boost::asio::ip::tcp::endpoint& endpoint, const std::string& user_id) : _version(version), _command(cmd), _user_id(user_id), _null_byte(0) {
            // Only IPv4 is supported by the SOCKS 4 protocol.
            if (endpoint.protocol() != boost::asio::ip::tcp::v4()) {
                throw boost::system::system_error(boost::asio::error::address_family_not_supported);
            }
            
            // Convert port number to network byte order.
            unsigned short port = endpoint.port();
            _port_high_byte = (port >> 8) & 0xff;
            _port_low_byte = port & 0xff;
            
            // Save IP address in network byte order.
            _address = endpoint.address().to_v4().to_bytes();
        }
        
        boost::array<boost::asio::const_buffer, 7> buffers() const {
            boost::array<boost::asio::const_buffer, 7> bufs = {
                {
                boost::asio::buffer(&_version, 1),
                boost::asio::buffer(&_command, 1),
                boost::asio::buffer(&_port_high_byte, 1),
                boost::asio::buffer(&_port_low_byte, 1),
                boost::asio::buffer(_address),
                boost::asio::buffer(_user_id),
                boost::asio::buffer(&_null_byte, 1)
                }
            };
            return bufs;
        }
        
    private:
        unsigned char _version;
        unsigned char _command;
        unsigned char _port_high_byte;
        unsigned char _port_low_byte;
        boost::asio::ip::address_v4::bytes_type _address;
        std::string _user_id;
        unsigned char _null_byte;
    };
    
    class Reply {
    public:
        enum StatusType {
            request_granted = 0x5a,
            request_failed = 0x5b,
            request_failed_no_identd = 0x5c,
            request_failed_bad_user_id = 0x5d
        };
        
        Reply() : _null_byte(0), _status() { }
        
        boost::array<boost::asio::mutable_buffer, 5> buffers() {
            boost::array<boost::asio::mutable_buffer, 5> bufs = {
                {
                boost::asio::buffer(&_null_byte, 1),
                boost::asio::buffer(&_status, 1),
                boost::asio::buffer(&_port_high_byte, 1),
                boost::asio::buffer(&_port_low_byte, 1),
                boost::asio::buffer(_address)
                }
            };
            return bufs;
        }
        
        bool success() const {
            return _null_byte == 0 && _status == request_granted;
        }
        
        unsigned char status() const {
            return _status;
        }
        
        boost::asio::ip::tcp::endpoint endpoint() const {
            unsigned short port = _port_high_byte;
            port = (port << 8) & 0xff00;
            port = port | _port_low_byte;
            
            boost::asio::ip::address_v4 address(_address);
            
            return boost::asio::ip::tcp::endpoint(address, port);
        }
        
    private:
        unsigned char _null_byte;
        unsigned char _status;
        unsigned char _port_high_byte;
        unsigned char _port_low_byte;
        boost::asio::ip::address_v4::bytes_type _address;
    };
    
} // namespace socks4

/// Proxy enables connection via a proxy.
/// Proxy supports different socks proxies (currently only SOCKS4) and manages the initial 
/// connection by interceping the connection initiation. The connection callback can be reused.
class COINCHAIN_EXPORT Proxy : private boost::noncopyable
{
public:
    enum Version {
        SOCKS4 = 0x04,
        SOCKS5 = 0x05
    };

    /// Setup a <version> Proxy to the <server>
    Proxy(boost::asio::ip::tcp::endpoint server, Version version = SOCKS4) : _server(server), _version(version), _socket(NULL) {}
    
    /// Reset the proxy with new settings.
    void reset(boost::asio::ip::tcp::endpoint server, Version version = SOCKS4) { _server = server; _version = version; }
    
    /// Convenience operator to set the connection socket.
    Proxy& operator()(boost::asio::ip::tcp::socket& socket);
    
    /// Initiate a connection, setup the proxy and call the connection_handler. 
    void async_connect(boost::asio::ip::tcp::endpoint ep, boost::function<void (const boost::system::error_code&)> connection_handler);
    
    /// Cast to bool to enable proxy querying.
    operator bool() const {
        return _server.address().to_v4().to_ulong() != INADDR_ANY;
    }
    
private:
    /// write handler
    void handle_write(const boost::system::error_code& e, size_t bytes_transferred);
    
    /// Handle completion of an asynchronous connect to socks proxy server.
    void handle_proxy_connect(const boost::system::error_code& e);
    
    /// Handle reply from proxy server - next step will be normal handle_connect.
    void handle_proxy_reply(const boost::system::error_code& e, size_t bytes_transferred);
    
private:
    boost::asio::ip::tcp::endpoint _server;
    Version _version;

    /// Socket for the connection to the peer via the proxy server.
    boost::asio::ip::tcp::socket* _socket;
    /// Endpoint
    boost::asio::ip::tcp::endpoint _endpoint;
    
    boost::function<void (const boost::system::error_code&)> _connection_handler;
    
    Socks4::Reply _reply;
};

#endif // PROXY_H
