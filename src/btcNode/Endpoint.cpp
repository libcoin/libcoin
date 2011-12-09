// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "btcNode/Endpoint.h"
#include "btc/util.h"

#ifndef _WIN32
# include <arpa/inet.h>
#endif

// Prototypes from net.h, but that header (currently) stinks, can't #include it without breaking things
bool Lookup(const char *pszName, std::vector<Endpoint>& vaddr, int nServices, int nMaxSolutions, bool fAllowLookup = false, int portDefault = 0, bool fAllowPort = false);
bool Lookup(const char *pszName, Endpoint& addr, int nServices, bool fAllowLookup = false, int portDefault = 0, bool fAllowPort = false);

static const unsigned char pchIPv4[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff };

Endpoint::Endpoint()
{
    init();
}

Endpoint::Endpoint(unsigned int ip, unsigned short port, uint64 services)
{
    init();
    _ip = ip;
    _port = htons(port == 0 ? getDefaultPort() : port);
    _services = services;
}

Endpoint::Endpoint(const struct sockaddr_in& sockaddr, uint64 services)
{
    init();
    _ip = sockaddr.sin_addr.s_addr;
    _port = sockaddr.sin_port;
    _services = services;
}

Endpoint::Endpoint(const char* pszIn, int portIn, bool fNameLookup, uint64 nServicesIn)
{
    init();
    Lookup(pszIn, *this, nServicesIn, fNameLookup, portIn);
}

Endpoint::Endpoint(const char* pszIn, bool fNameLookup, uint64 nServicesIn)
{
    init();
    Lookup(pszIn, *this, nServicesIn, fNameLookup, 0, true);
}

Endpoint::Endpoint(std::string strIn, int portIn, bool fNameLookup, uint64 nServicesIn)
{
    init();
    Lookup(strIn.c_str(), *this, nServicesIn, fNameLookup, portIn);
}

Endpoint::Endpoint(std::string strIn, bool fNameLookup, uint64 nServicesIn)
{
    init();
    Lookup(strIn.c_str(), *this, nServicesIn, fNameLookup, 0, true);
}

void Endpoint::init()
{
    _services = NODE_NETWORK;
    memcpy(_ipv6, pchIPv4, sizeof(_ipv6));
    _ip = INADDR_NONE;
    _port = htons(getDefaultPort());
    _time = 100000000;
    _lastTry = 0;
}

bool operator==(const Endpoint& a, const Endpoint& b)
{
    return (memcmp(a._ipv6, b._ipv6, sizeof(a._ipv6)) == 0 &&
            a._ip   == b._ip &&
            a._port == b._port);
}

bool operator!=(const Endpoint& a, const Endpoint& b)
{
    return (!(a == b));
}

bool operator<(const Endpoint& a, const Endpoint& b)
{
    int ret = memcmp(a._ipv6, b._ipv6, sizeof(a._ipv6));
    if (ret < 0)
        return true;
    else if (ret == 0)
    {
        if (ntohl(a._ip) < ntohl(b._ip))
            return true;
        else if (a._ip == b._ip)
            return ntohs(a._port) < ntohs(b._port);
    }
    return false;
}

std::vector<unsigned char> Endpoint::getKey() const
{
    CDataStream ss;
    ss.reserve(18);
    ss << FLATDATA(_ipv6) << _ip << _port;

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
    sockaddr.sin_addr.s_addr = _ip;
    sockaddr.sin_port = _port;
    return sockaddr;
}

bool Endpoint::isIPv4() const
{
    return (memcmp(_ipv6, pchIPv4, sizeof(pchIPv4)) == 0);
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
    if (memcmp(_ipv6, pchIPv4+3, sizeof(pchIPv4)-3) == 0)
        return false;

    return (_ip != 0 && _ip != INADDR_NONE && _port != htons(USHRT_MAX));
}

unsigned char Endpoint::getByte(int n) const
{
    return ((unsigned char*)&_ip)[3-n];
}

std::string Endpoint::toStringIPPort() const
{
    return strprintf("%u.%u.%u.%u:%u", getByte(3), getByte(2), getByte(1), getByte(0), ntohs(_port));
}

std::string Endpoint::toStringIP() const
{
    return strprintf("%u.%u.%u.%u", getByte(3), getByte(2), getByte(1), getByte(0));
}

std::string Endpoint::toStringPort() const
{
    return strprintf("%u", ntohs(_port));
}

std::string Endpoint::toString() const
{
    return strprintf("%u.%u.%u.%u:%u", getByte(3), getByte(2), getByte(1), getByte(0), ntohs(_port));
}

void Endpoint::print() const
{
    printf("Endpoint(%s)\n", toString().c_str());
}
