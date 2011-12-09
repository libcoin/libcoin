// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef __cplusplus
# error This header can only be compiled as C++.
#endif

#ifndef __INCLUDED_PROTOCOL_H__
#define __INCLUDED_PROTOCOL_H__

#include <string>

#include "btc/uint256.h"
#include "btc/serialize.h"

extern bool fTestNet;
static inline unsigned short GetDefaultPort(const bool testnet = fTestNet)
{
    return testnet ? 18333 : 8333;
}

//
// Message header
//  (4) message start
//  (12) command
//  (4) size
//  (4) checksum

extern unsigned char pchMessageStart[4];

class CMessageHeader
{
    public:
        CMessageHeader();
        CMessageHeader(const char* pszCommand, unsigned int nMessageSizeIn);

        std::string GetCommand() const;
        bool IsValid() const;

        IMPLEMENT_SERIALIZE
            (
             READWRITE(FLATDATA(pchMessageStart));
             READWRITE(FLATDATA(pchCommand));
             READWRITE(nMessageSize);
             if (nVersion >= 209)
             READWRITE(nChecksum);
            )

    // TODO: make private (improves encapsulation)
    public:
        enum { COMMAND_SIZE=12 };
        char pchMessageStart[sizeof(::pchMessageStart)];
        char pchCommand[COMMAND_SIZE];
        unsigned int nMessageSize;
        unsigned int nChecksum;
};

enum
{
    NODE_NETWORK = (1 << 0),
};

class Endpoint
{
    public:
        Endpoint();
        Endpoint(unsigned int ipIn, unsigned short portIn=0, uint64 nServicesIn=NODE_NETWORK);
        explicit Endpoint(const struct sockaddr_in& sockaddr, uint64 nServicesIn=NODE_NETWORK);
        explicit Endpoint(const char* pszIn, int portIn, bool fNameLookup = false, uint64 nServicesIn=NODE_NETWORK);
        explicit Endpoint(const char* pszIn, bool fNameLookup = false, uint64 nServicesIn=NODE_NETWORK);
        explicit Endpoint(std::string strIn, int portIn, bool fNameLookup = false, uint64 nServicesIn=NODE_NETWORK);
        explicit Endpoint(std::string strIn, bool fNameLookup = false, uint64 nServicesIn=NODE_NETWORK);

        void Init();

        IMPLEMENT_SERIALIZE
            (
             if (fRead)
             const_cast<Endpoint*>(this)->Init();
             if (nType & SER_DISK)
             READWRITE(nVersion);
             if ((nType & SER_DISK) || (nVersion >= 31402 && !(nType & SER_GETHASH)))
             READWRITE(nTime);
             READWRITE(nServices);
             READWRITE(FLATDATA(pchReserved)); // for IPv6
             READWRITE(ip);
             READWRITE(port);
            )

        friend bool operator==(const Endpoint& a, const Endpoint& b);
        friend bool operator!=(const Endpoint& a, const Endpoint& b);
        friend bool operator<(const Endpoint& a, const Endpoint& b);

        std::vector<unsigned char> GetKey() const;
        struct sockaddr_in GetSockAddr() const;
        bool IsIPv4() const;
        bool IsRFC1918() const;
        bool IsRFC3927() const;
        bool IsLocal() const;
        bool IsRoutable() const;
        bool IsValid() const;
        unsigned char GetByte(int n) const;
        std::string ToStringIPPort() const;
        std::string ToStringIP() const;
        std::string ToStringPort() const;
        std::string ToString() const;
        void print() const;

    // TODO: make private (improves encapsulation)
    public:
        uint64 nServices;
        unsigned char pchReserved[12];
        unsigned int ip;
        unsigned short port;

        // disk and network only
        unsigned int nTime;

        // memory only
        unsigned int nLastTry;
};

class Inventory
{
    public:
        Inventory();
        Inventory(int typeIn, const uint256& hashIn);
        Inventory(const std::string& strType, const uint256& hashIn);

        IMPLEMENT_SERIALIZE
        (
            READWRITE(type);
            READWRITE(hash);
        )

        friend bool operator<(const Inventory& a, const Inventory& b);

        bool IsKnownType() const;
        const char* GetCommand() const;
        std::string ToString() const;
        void print() const;

    // TODO: make private (improves encapsulation)
    public:
        int type;
        uint256 hash;
};

#endif // __INCLUDED_PROTOCOL_H__
