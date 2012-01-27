// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "coinChain/MessageHeader.h"
#include "coin/util.h"

#ifndef _WIN32
# include <arpa/inet.h>
#endif

static const unsigned char pchIPv4[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff };
static const char* ppszTypeName[] =
{
    "ERROR",
    "tx",
    "block",
};
//unsigned char pchMessageStart[4] = { 0xf9, 0xbe, 0xb4, 0xd9 };

MessageHeader::MessageHeader()
{
    memset(_messageStart.elems, 0, sizeof(_messageStart.elems));
    memset(pchCommand, 0, sizeof(pchCommand));
    pchCommand[1] = 1;
    nMessageSize = -1;
    nChecksum = 0;
}
/*
MessageHeader::MessageHeader(const Chain& chain) : _messageStart(chain.messageStart())
{
    //    memcpy(pchMessageStart, ::pchMessageStart, sizeof(pchMessageStart));
    memset(pchCommand, 0, sizeof(pchCommand));
    pchCommand[1] = 1;
    nMessageSize = -1;
    nChecksum = 0;
}

MessageHeader::MessageHeader(const char* pszCommand, unsigned int nMessageSizeIn) {
    memset(_messageStart.elems, 0, sizeof(_messageStart.elems));
    strncpy(pchCommand, pszCommand, COMMAND_SIZE);
    nMessageSize = nMessageSizeIn;
    nChecksum = 0;
}
*/
MessageHeader::MessageHeader(const Chain& chain, const char* pszCommand, unsigned int nMessageSizeIn) : _messageStart(chain.messageStart())
{
    //    memcpy(pchMessageStart, ::pchMessageStart, sizeof(pchMessageStart));
    strncpy(pchCommand, pszCommand, COMMAND_SIZE);
    nMessageSize = nMessageSizeIn;
    nChecksum = 0;
}

std::string MessageHeader::GetCommand() const
{
    if (pchCommand[COMMAND_SIZE-1] == 0)
        return std::string(pchCommand, pchCommand + strlen(pchCommand));
    else
        return std::string(pchCommand, pchCommand + COMMAND_SIZE);
}

bool MessageHeader::IsValid(const Chain& chain) const
{
    // Check start string
    if (memcmp(_messageStart.elems, chain.messageStart().elems, sizeof(_messageStart.elems)) != 0)
        return false;

    // Check the command string for errors
    for (const char* p1 = pchCommand; p1 < pchCommand + COMMAND_SIZE; p1++)
    {
        if (*p1 == 0)
        {
            // Must be all zeros after the first zero
            for (; p1 < pchCommand + COMMAND_SIZE; p1++)
                if (*p1 != 0)
                    return false;
        }
        else if (*p1 < ' ' || *p1 > 0x7E)
            return false;
    }

    // Message size
    if (nMessageSize > MAX_SIZE)
    {
        printf("MessageHeader::IsValid() : (%s, %u bytes) nMessageSize > MAX_SIZE\n", GetCommand().c_str(), nMessageSize);
        return false;
    }

    return true;
}
