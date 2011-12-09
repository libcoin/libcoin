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
    Init();
}

Endpoint::Endpoint(unsigned int ipIn, unsigned short portIn, uint64 nServicesIn)
{
    Init();
    ip = ipIn;
    port = htons(portIn == 0 ? GetDefaultPort() : portIn);
    nServices = nServicesIn;
}

Endpoint::Endpoint(const struct sockaddr_in& sockaddr, uint64 nServicesIn)
{
    Init();
    ip = sockaddr.sin_addr.s_addr;
    port = sockaddr.sin_port;
    nServices = nServicesIn;
}

Endpoint::Endpoint(const char* pszIn, int portIn, bool fNameLookup, uint64 nServicesIn)
{
    Init();
    Lookup(pszIn, *this, nServicesIn, fNameLookup, portIn);
}

Endpoint::Endpoint(const char* pszIn, bool fNameLookup, uint64 nServicesIn)
{
    Init();
    Lookup(pszIn, *this, nServicesIn, fNameLookup, 0, true);
}

Endpoint::Endpoint(std::string strIn, int portIn, bool fNameLookup, uint64 nServicesIn)
{
    Init();
    Lookup(strIn.c_str(), *this, nServicesIn, fNameLookup, portIn);
}

Endpoint::Endpoint(std::string strIn, bool fNameLookup, uint64 nServicesIn)
{
    Init();
    Lookup(strIn.c_str(), *this, nServicesIn, fNameLookup, 0, true);
}

void Endpoint::Init()
{
    nServices = NODE_NETWORK;
    memcpy(pchReserved, pchIPv4, sizeof(pchReserved));
    ip = INADDR_NONE;
    port = htons(GetDefaultPort());
    nTime = 100000000;
    nLastTry = 0;
}

bool operator==(const Endpoint& a, const Endpoint& b)
{
    return (memcmp(a.pchReserved, b.pchReserved, sizeof(a.pchReserved)) == 0 &&
            a.ip   == b.ip &&
            a.port == b.port);
}

bool operator!=(const Endpoint& a, const Endpoint& b)
{
    return (!(a == b));
}

bool operator<(const Endpoint& a, const Endpoint& b)
{
    int ret = memcmp(a.pchReserved, b.pchReserved, sizeof(a.pchReserved));
    if (ret < 0)
        return true;
    else if (ret == 0)
    {
        if (ntohl(a.ip) < ntohl(b.ip))
            return true;
        else if (a.ip == b.ip)
            return ntohs(a.port) < ntohs(b.port);
    }
    return false;
}

std::vector<unsigned char> Endpoint::GetKey() const
{
    CDataStream ss;
    ss.reserve(18);
    ss << FLATDATA(pchReserved) << ip << port;

    #if defined(_MSC_VER) && _MSC_VER < 1300
    return std::vector<unsigned char>((unsigned char*)&ss.begin()[0], (unsigned char*)&ss.end()[0]);
    #else
    return std::vector<unsigned char>(ss.begin(), ss.end());
    #endif
}

struct sockaddr_in Endpoint::GetSockAddr() const
{
    struct sockaddr_in sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = ip;
    sockaddr.sin_port = port;
    return sockaddr;
}

bool Endpoint::IsIPv4() const
{
    return (memcmp(pchReserved, pchIPv4, sizeof(pchIPv4)) == 0);
}

bool Endpoint::IsRFC1918() const
{
  return IsIPv4() && (GetByte(3) == 10 ||
    (GetByte(3) == 192 && GetByte(2) == 168) ||
    (GetByte(3) == 172 &&
      (GetByte(2) >= 16 && GetByte(2) <= 31)));
}

bool Endpoint::IsRFC3927() const
{
  return IsIPv4() && (GetByte(3) == 169 && GetByte(2) == 254);
}

bool Endpoint::IsLocal() const
{
  return IsIPv4() && (GetByte(3) == 127 ||
      GetByte(3) == 0);
}

bool Endpoint::IsRoutable() const
{
    return IsValid() &&
        !(IsRFC1918() || IsRFC3927() || IsLocal());
}

bool Endpoint::IsValid() const
{
    // Clean up 3-byte shifted addresses caused by garbage in size field
    // of addr messages from versions before 0.2.9 checksum.
    // Two consecutive addr messages look like this:
    // header20 vectorlen3 addr26 addr26 addr26 header20 vectorlen3 addr26 addr26 addr26...
    // so if the first length field is garbled, it reads the second batch
    // of addr misaligned by 3 bytes.
    if (memcmp(pchReserved, pchIPv4+3, sizeof(pchIPv4)-3) == 0)
        return false;

    return (ip != 0 && ip != INADDR_NONE && port != htons(USHRT_MAX));
}

unsigned char Endpoint::GetByte(int n) const
{
    return ((unsigned char*)&ip)[3-n];
}

std::string Endpoint::ToStringIPPort() const
{
    return strprintf("%u.%u.%u.%u:%u", GetByte(3), GetByte(2), GetByte(1), GetByte(0), ntohs(port));
}

std::string Endpoint::ToStringIP() const
{
    return strprintf("%u.%u.%u.%u", GetByte(3), GetByte(2), GetByte(1), GetByte(0));
}

std::string Endpoint::ToStringPort() const
{
    return strprintf("%u", ntohs(port));
}

std::string Endpoint::toString() const
{
    return strprintf("%u.%u.%u.%u:%u", GetByte(3), GetByte(2), GetByte(1), GetByte(0), ntohs(port));
}

void Endpoint::print() const
{
    printf("Endpoint(%s)\n", toString().c_str());
}
