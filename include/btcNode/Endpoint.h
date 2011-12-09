
#ifndef ENDPOINT_H
#define ENDPOINT_H

#include <string>

#include "btc/uint256.h"
#include "btc/serialize.h"

extern bool fTestNet;
static inline unsigned short getDefaultPort(const bool testnet = fTestNet)
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
    typedef std::vector<unsigned char> Key;
        Endpoint();
        Endpoint(unsigned int ipIn, unsigned short portIn=0, uint64 nServicesIn=NODE_NETWORK);
        explicit Endpoint(const struct sockaddr_in& sockaddr, uint64 nServicesIn=NODE_NETWORK);
        explicit Endpoint(const char* pszIn, int portIn, bool fNameLookup = false, uint64 nServicesIn=NODE_NETWORK);
        explicit Endpoint(const char* pszIn, bool fNameLookup = false, uint64 nServicesIn=NODE_NETWORK);
        explicit Endpoint(std::string strIn, int portIn, bool fNameLookup = false, uint64 nServicesIn=NODE_NETWORK);
        explicit Endpoint(std::string strIn, bool fNameLookup = false, uint64 nServicesIn=NODE_NETWORK);

        void init();

        IMPLEMENT_SERIALIZE
            (
             if (fRead)
             const_cast<Endpoint*>(this)->init();
             if (nType & SER_DISK)
             READWRITE(nVersion);
             if ((nType & SER_DISK) || (nVersion >= 31402 && !(nType & SER_GETHASH)))
             READWRITE(_time);
             READWRITE(_services);
             READWRITE(FLATDATA(_ipv6)); // for IPv6
             READWRITE(_ip);
             READWRITE(_port);
            )

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
    
    const unsigned int getIP() const { return _ip; }
    void setIP(unsigned int ip) { _ip = ip; }

    const unsigned short getPort() const { return _port; }
    void setPort(unsigned short port) { _port = port; }
    
    const unsigned int getTime() const  { return _time; }
    void setTime(unsigned int time) { _time = time; }
    
    const unsigned int getLastTry() const { return _lastTry; }
    void setLastTry(unsigned int lastTry) { _lastTry = lastTry; }

    const uint64 getServices() const { return _services; }
    void setServices(uint64 services) { _services = services; }
    
    private:
        uint64 _services;
        unsigned char _ipv6[12];
        unsigned int _ip;
        unsigned short _port;

        // disk and network only
        unsigned int _time;

        // memory only
        unsigned int _lastTry;
};

#endif // ENDPOINT_H
