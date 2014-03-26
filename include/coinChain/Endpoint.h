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

#ifndef ENDPOINT_H
#define ENDPOINT_H

#include <string>
#include <boost/asio.hpp>

#include <coin/uint256.h>
//#include <coin/serialize.h>

#include <coinChain/Export.h>

enum
{
    NODE_NETWORK = (1 << 0)
};

/// Endpoint is a boost endpoint with a time of last use attached to it
/// The _services member is not used in any logic but seem to have indicated different node networks ? could be used for a esparate hashing scheme.
/// The _ipv6 has been left as is for reader/writer compatability, but is actually a part of the boost endpoint class.

class COINCHAIN_EXPORT Endpoint : public boost::asio::ip::tcp::endpoint
{
    public:
    typedef std::vector<unsigned char> Key;
    Endpoint();
    Endpoint(boost::asio::ip::tcp::endpoint ep);
    Endpoint(const Endpoint& endpoint);
    Endpoint(unsigned int ipIn, unsigned short portIn=0, uint64_t nServicesIn=NODE_NETWORK);
    explicit Endpoint(const struct sockaddr_in& sockaddr, uint64_t nServicesIn=NODE_NETWORK);
    explicit Endpoint(std::string strIn, unsigned short portIn, bool fNameLookup = false, uint64_t nServicesIn=NODE_NETWORK);
    explicit Endpoint(std::string strIn, bool fNameLookup = false, uint64_t nServicesIn=NODE_NETWORK);
    explicit Endpoint(unsigned int ip, unsigned short port, unsigned int time, unsigned int last_try);
  
    void init();
    
    inline friend std::ostream& operator<<(std::ostream& os, const Endpoint& ep) {
        boost::asio::ip::address_v6::bytes_type ipv6;
        if (ep.address().is_v6())
            ipv6 = ep.address().to_v6().to_bytes();
        else
            ipv6 = boost::asio::ip::address_v6::v4_mapped(ep.address().to_v4()).to_bytes();
        if (ep._lastTry != UINT_MAX)
            os << const_binary<unsigned int>(ep._time);
        return  os << const_binary<uint64_t>(ep._services) << const_binary<boost::asio::ip::address_v6::bytes_type>(ipv6) << const_binary<unsigned short>(ntohs(ep.port()));
    }
    
    inline friend std::istream& operator>>(std::istream& is, Endpoint& ep) {
        bool skip_time = (ep._lastTry == UINT_MAX);
        ep.init();
        boost::asio::ip::address_v6::bytes_type ipv6;
        unsigned short port;
        if (!skip_time)
            is >> binary<unsigned int>(ep._time);
        is >> binary<uint64_t>(ep._services) >> binary<boost::asio::ip::address_v6::bytes_type>(ipv6) >> binary<unsigned short>(port);
        boost::asio::ip::address_v6 address = boost::asio::ip::address_v6(ipv6);
        if (address.is_v4_mapped())
            ep.address(address.to_v4());
        else
            ep.address(address);
        ep.port(htons(port));
        
        return is;
    }

    friend bool operator==(const Endpoint& a, const Endpoint& b);
        friend bool operator!=(const Endpoint& a, const Endpoint& b);
        friend bool operator<(const Endpoint& a, const Endpoint& b);

        Key getKey() const;
        struct sockaddr_in getSockAddr() const;
        bool isIPv4() const;
        bool isRFC1918() const;
        bool isRFC3927() const;
        bool isLocal() const;
        bool isRoutable() const;
        bool isValid() const;
        unsigned char getByte(int n) const;
        std::string toStringIPPort() const;
        std::string toStringIP() const;
        std::string toStringPort() const;
        std::string toString() const;
        void print() const;
    
    const unsigned int getIP() const { return address().to_v4().to_ulong(); }
    void setIP(unsigned int ip) { address(boost::asio::ip::address(boost::asio::ip::address_v4(ip))); }

    const unsigned short getPort() const { return port(); }
    void setPort(unsigned short p) { port(p); }
    
    const unsigned int getTime() const  { return _time; }
    void setTime(unsigned int time) { _time = time; }
    
    const unsigned int getLastTry() const { return _lastTry; }
    void setLastTry(unsigned int lastTry) { _lastTry = lastTry; }

    const uint64_t getServices() const { return _services; }
    void setServices(uint64_t services) { _services = services; }
    
    boost::asio::ip::address_v6::bytes_type getIPv6() const {
        if (address().is_v6())
            return address().to_v6().to_bytes();
        else
            return boost::asio::ip::address_v6::v4_mapped(address().to_v4()).to_bytes();
    }
    
    private:
        uint64_t _services;
//        unsigned char _ipv6[12];
    //        unsigned int _ip;
    //        unsigned short _port;

        // disk and network only
        unsigned int _time;

        // memory only
        unsigned int _lastTry;
};

#endif // ENDPOINT_H
