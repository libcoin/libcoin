
#ifndef MESSAGE_HEADER_H
#define MESSAGE_HEADER_H

#include "btc/uint256.h"
#include "btc/serialize.h"
#include "btcNode/Chain.h"

#include <string>

#include <boost/array.hpp>

//
// Message header
//  (4) message start
//  (12) command
//  (4) size
//  (4) checksum

//extern unsigned char pchMessageStart[4];

class MessageHeader
{
public:
    MessageHeader();
    //    MessageHeader(const Chain& chain);
    //    MessageHeader(const char* pszCommand, unsigned int nMessageSizeIn);
    MessageHeader(const Chain& chain, const char* pszCommand, unsigned int nMessageSizeIn);
    
    std::string GetCommand() const;
    bool IsValid(const Chain& chain) const;
    
    IMPLEMENT_SERIALIZE
    (
     READWRITE(FLATDATA(_messageStart));
     READWRITE(FLATDATA(pchCommand));
     READWRITE(nMessageSize);
     if (nVersion >= 209)
     READWRITE(nChecksum);
     )
    
    // TODO: make private (improves encapsulation)
public:
    enum { COMMAND_SIZE=12 };
    MessageStart _messageStart;
    char pchCommand[COMMAND_SIZE];
    unsigned int nMessageSize;
    unsigned int nChecksum;
};

#endif // MESSAGE_HEADER_H
