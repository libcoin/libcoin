
#ifndef ENDPOINT_H
#define ENDPOINT_H

#include <string>
#include <boost/asio.hpp>

#include "btc/uint256.h"
#include "btc/serialize.h"

enum
{
    NODE_NETWORK = (1 << 0),
};

/// Endpoint is a boost endpoint with a time of last use attached to it
/// The _services member is not used in any logic but seem to have indicated different node networks ? could be used for a esparate hashing scheme.
/// The _ipv6 has been left as is for reader/writer compatability, but is actually a part of the boost endpoint class.

class Endpoint : public boost::asio::ip::tcp::endpoint
{
    public:
    typedef std::vector<unsigned char> Key;
        Endpoint();
        Endpoint(boost::asio::ip::tcp::endpoint ep);
        Endpoint(unsigned int ipIn, unsigned short portIn=0, uint64 nServicesIn=NODE_NETWORK);
        explicit Endpoint(const struct sockaddr_in& sockaddr, uint64 nServicesIn=NODE_NETWORK);
        explicit Endpoint(std::string strIn, unsigned short portIn, bool fNameLookup = false, uint64 nServicesIn=NODE_NETWORK);
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
             if (fWrite) {
                 
                 unsigned int ip = ntohl(address().to_v4().to_ulong());
                 unsigned short p = ntohs(port());
                 READWRITE(ip);
                 READWRITE(p);
             }
             else if (fRead) {
                 unsigned int ip;
                 unsigned short p;
                 READWRITE(ip);
                 READWRITE(p);
                 const_cast<Endpoint*>(this)->address(boost::asio::ip::address(boost::asio::ip::address_v4(htonl(ip))));
                 const_cast<Endpoint*>(this)->port(htons(p));
             }
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
    
    const unsigned int getIP() const { return address().to_v4().to_ulong(); }
    void setIP(unsigned int ip) { address(boost::asio::ip::address(boost::asio::ip::address_v4(ip))); }

    const unsigned short getPort() const { return port(); }
    void setPort(unsigned short p) { port(p); }
    
    const unsigned int getTime() const  { return _time; }
    void setTime(unsigned int time) { _time = time; }
    
    const unsigned int getLastTry() const { return _lastTry; }
    void setLastTry(unsigned int lastTry) { _lastTry = lastTry; }

    const uint64 getServices() const { return _services; }
    void setServices(uint64 services) { _services = services; }
    
    private:
        uint64 _services;
        unsigned char _ipv6[12];
    //        unsigned int _ip;
    //        unsigned short _port;

        // disk and network only
        unsigned int _time;

        // memory only
        unsigned int _lastTry;
};

#endif // ENDPOINT_H
