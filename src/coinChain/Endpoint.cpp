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

#include <coinChain/Endpoint.h>
#include <coin/util.h>

#include <boost/lexical_cast.hpp>

#ifndef _WIN32
# include <arpa/inet.h>
#endif

using namespace std;
using namespace boost;
using namespace boost::asio::ip;

// portDefault is in host order
bool Lookup(const string name, vector<Endpoint>& vaddr, int nServices, int nMaxSolutions, bool fAllowLookup = false, unsigned short portDefault = 0, bool fAllowPort = false)
{
    vaddr.clear();
    if (name.size() == 0)
        return false;
    unsigned short port = portDefault;
    string host = name;
    if (fAllowPort) { // split the name in host and port - find the ":"
        size_t colon_pos = name.rfind(':');
        if (colon_pos != string::npos) {
            port = lexical_cast<unsigned short>(name.substr(colon_pos+1));
            host = name.substr(0, colon_pos);
        }
    }
    
    unsigned int addrIP = inet_addr(host.c_str());
    if (addrIP != INADDR_NONE) {
        // valid IP address passed
        vaddr.push_back(Endpoint(addrIP, port, nServices));
        return true;
    }
    
    if (!fAllowLookup)
        return false;
    
    struct hostent* phostent = gethostbyname(host.c_str());
    if (!phostent)
        return false;
    
    if (phostent->h_addrtype != AF_INET)
        return false;
    
    char** ppAddr = phostent->h_addr_list;
    while (*ppAddr != NULL && vaddr.size() != (unsigned int)nMaxSolutions) {
        Endpoint addr(((struct in_addr*)ppAddr[0])->s_addr, port, nServices);
        if (addr.isValid())
            vaddr.push_back(addr);
        ppAddr++;
        }
    
    return (vaddr.size() > 0);
}

// portDefault is in host order
bool Lookup(const string name, Endpoint& addr, int nServices, bool fAllowLookup = false, unsigned short portDefault = 0, bool fAllowPort = false)
{
    vector<Endpoint> vaddr;
    bool fRet = Lookup(name, vaddr, nServices, 1, fAllowLookup, portDefault, fAllowPort);
    if (fRet)
        addr = vaddr[0];
    return fRet;
}

Endpoint::Endpoint()
{
    init();
}

Endpoint::Endpoint(tcp::endpoint ep) : tcp::endpoint(ep) {
    _services = NODE_NETWORK;
//    memcpy(_ipv6, pchIPv4, sizeof(_ipv6));
    _time = UnixTime::s() - 3*24*60*60 - GetRand(4*24*60*60);
    _lastTry = 0;    
}

Endpoint::Endpoint(const Endpoint& ep) : tcp::endpoint(ep), _services(ep._services), _time(ep._time), _lastTry(ep._lastTry)  {
//    memcpy(_ipv6, ep._ipv6, sizeof(_ipv6));
}

Endpoint::Endpoint(unsigned int ip, unsigned short p, uint64_t services)
{
    init();
    //    address(address_v4(ip));
    //    port(htons(p == 0 ? getDefaultPort() : p));
    address(address_v4(ntohl(ip)));
    //    port(p == 0 ? getDefaultPort() : p);
    port(p);
    _services = services;
}

Endpoint::Endpoint(const struct sockaddr_in& sockaddr, uint64_t services)
{
    init();
    address(address_v4(sockaddr.sin_addr.s_addr));
    port(sockaddr.sin_port);
    _services = services;
}

Endpoint::Endpoint(std::string strIn, unsigned short portIn, bool fNameLookup, uint64_t nServicesIn)
{
    init();
    Lookup(strIn, *this, nServicesIn, fNameLookup, portIn);
}

Endpoint::Endpoint(std::string strIn, bool fNameLookup, uint64_t nServicesIn)
{
    init();
    Lookup(strIn, *this, nServicesIn, fNameLookup, 0, true);
}

Endpoint::Endpoint(unsigned int ip, unsigned short p, unsigned int time, unsigned int last_try) {
    init();
    address(address_v4(ip));
    port(p);
    _time = time;
    _lastTry = last_try;
}


void Endpoint::init()
{
    _services = NODE_NETWORK;
//    memcpy(_ipv6, pchIPv4, sizeof(_ipv6));
    //    address(address_v4(INADDR_NONE));
    //    port(htons(getDefaultPort()));
    address(address_v4(INADDR_NONE));
    //    port(getDefaultPort());
    port(0);
    _time = 100000000;
    _lastTry = 0;
}

bool operator==(const Endpoint& a, const Endpoint& b)
{
//    return (memcmp(a._ipv6, b._ipv6, sizeof(a._ipv6)) == 0 &&
      return ((const boost::asio::ip::tcp::endpoint&)a == (const boost::asio::ip::tcp::endpoint&)b &&
              a._services == b._services &&
              a._time == b._time);
}

bool operator!=(const Endpoint& a, const Endpoint& b)
{
    return (!(a == b));
}

bool operator<(const Endpoint& a, const Endpoint& b)
{
//    int ret = memcmp(a._ipv6, b._ipv6, sizeof(a._ipv6));
//    if (ret < 0)
//        return true;
//    else if (ret == 0)
    int ret = a.address() == b.address();
    if (!ret)
        return a.address() < b.address();
    else
        return ntohs(a.port()) < ntohs(b.port());
    /*
    {
        if (ntohl(a.address().to_v4().to_ulong()) < ntohl(b.address().to_v4().to_ulong()))
            return true;
        else if (a.address() == b.address())
            return ntohs(a.port()) < ntohs(b.port());
    }
     */
//    return false;
}

std::vector<unsigned char> Endpoint::getKey() const
{
    boost::asio::ip::address_v6::bytes_type ipv6;
    if (address().is_v6())
        ipv6 = address().to_v6().to_bytes();
    else
        ipv6 = boost::asio::ip::address_v6::v4_mapped(address().to_v4()).to_bytes();
    
    ostringstream os;
    os << const_binary<boost::asio::ip::address_v6::bytes_type>(ipv6) << const_binary<unsigned short>(port());
    
    string ss = os.str();

    #if defined(_MSC_VER) && _MSC_VER < 1300
    return std::vector<unsigned char>((unsigned char*)&ss.begin()[0], (unsigned char*)&ss.end()[0]);
    #else
    return std::vector<unsigned char>(ss.begin(), ss.end());
    #endif
}

struct sockaddr_in Endpoint::getSockAddr() const
{
    struct sockaddr_in sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = address().to_v4().to_ulong();
    sockaddr.sin_port = port();
    return sockaddr;
}

bool Endpoint::isIPv4() const
{
    return address().is_v4();
//    return (memcmp(_ipv6, pchIPv4, sizeof(pchIPv4)) == 0);
}

bool Endpoint::isRFC1918() const
{
  return isIPv4() && (getByte(3) == 10 ||
    (getByte(3) == 192 && getByte(2) == 168) ||
    (getByte(3) == 172 &&
      (getByte(2) >= 16 && getByte(2) <= 31)));
}

bool Endpoint::isRFC3927() const
{
  return isIPv4() && (getByte(3) == 169 && getByte(2) == 254);
}

bool Endpoint::isLocal() const
{
  return isIPv4() && (getByte(3) == 127 ||
      getByte(3) == 0);
}

bool Endpoint::isRoutable() const
{
    return isValid() &&
        !(isRFC1918() || isRFC3927() || isLocal());
}

bool Endpoint::isValid() const
{
    // Clean up 3-byte shifted addresses caused by garbage in size field
    // of addr messages from versions before 0.2.9 checksum.
    // Two consecutive addr messages look like this:
    // header20 vectorlen3 addr26 addr26 addr26 header20 vectorlen3 addr26 addr26 addr26...
    // so if the first length field is garbled, it reads the second batch
    // of addr misaligned by 3 bytes.
//    if (memcmp(_ipv6, pchIPv4+3, sizeof(pchIPv4)-3) == 0)
//        return false;

    return (address().to_v4().to_ulong() != 0 && address().to_v4().to_ulong() != INADDR_NONE && port() != htons(USHRT_MAX));
}

unsigned char Endpoint::getByte(int n) const
{
    unsigned long ip = address().to_v4().to_ulong();
    return ((unsigned char*)&ip)[n];
}

std::string Endpoint::toStringIPPort() const
{
    return toString();
//    return strprintf("%u.%u.%u.%u:%u", getByte(3), getByte(2), getByte(1), getByte(0), port());
}

std::string Endpoint::toStringIP() const
{
    ostringstream os;
    os << address();
    return os.str();
//    return strprintf("%u.%u.%u.%u", getByte(3), getByte(2), getByte(1), getByte(0));
}

std::string Endpoint::toStringPort() const
{
    return strprintf("%u", port());
}

std::string Endpoint::toString() const
{
    ostringstream os;
    os << (const boost::asio::ip::tcp::endpoint&)(*this);
    return os.str();
//    return strprintf("%u.%u.%u.%u:%u", getByte(3), getByte(2), getByte(1), getByte(0), port());
}

void Endpoint::print() const
{
    log_info("Endpoint(%s)\n", toString().c_str());
}
