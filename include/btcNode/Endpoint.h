
#ifndef ENDPOINT_H
#define ENDPOINT_H

#include <string>

#include "btc/uint256.h"
#include "btc/serialize.h"

extern bool fTestNet;
static inline unsigned short GetDefaultPort(const bool testnet = fTestNet)
{
    return testnet ? 18333 : 8333;
}

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
        std::string toString() const;
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

#endif // ENDPOINT_H
