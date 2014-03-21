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

#include <coinChain/MessageHeader.h>
#include <coin/util.h>
#include <coin/Logger.h>

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

MessageHeader::MessageHeader()
{
    memset(_messageStart.elems, 0, sizeof(_messageStart.elems));
    memset(&pchCommand, 0, sizeof(pchCommand));
    pchCommand[1] = 1;
    nMessageSize = -1;
    nChecksum = 0;
}

MessageHeader::MessageHeader(const Chain& chain, const char* pszCommand, unsigned int nMessageSizeIn) : _messageStart(chain.messageStart())
{
    //    memcpy(pchMessageStart, ::pchMessageStart, sizeof(pchMessageStart));
    strncpy(&pchCommand[0], pszCommand, COMMAND_SIZE);
    nMessageSize = nMessageSizeIn;
    nChecksum = 0;
}

std::string MessageHeader::GetCommand() const
{
    size_t len = COMMAND_SIZE;
    while (pchCommand[--len] == 0 && len);
    return std::string(&pchCommand[0], &pchCommand[0] + len + 1);
/*
    if (pchCommand[COMMAND_SIZE-1] == 0)
        return std::string(&pchCommand[0], &pchCommand[0] + strlen(pchCommand));
    else
        return std::string(pchCommand, pchCommand + COMMAND_SIZE);
*/
}

bool MessageHeader::IsValid(const Chain& chain) const
{
    // Check start string
    if (memcmp(_messageStart.elems, chain.messageStart().elems, sizeof(_messageStart.elems)) != 0)
        return false;

    // Check the command string for errors
    for (const char* p1 = &pchCommand[0]; p1 < &pchCommand[0] + COMMAND_SIZE; p1++)
    {
        if (*p1 == 0)
        {
            // Must be all zeros after the first zero
            for (; p1 < &pchCommand[0] + COMMAND_SIZE; p1++)
                if (*p1 != 0)
                    return false;
        }
        else if (*p1 < ' ' || *p1 > 0x7E)
            return false;
    }

    // Message size
    if (nMessageSize > MAX_SIZE)
    {
        log_debug("MessageHeader::IsValid() : (%s, %u bytes) nMessageSize > MAX_SIZE\n", GetCommand().c_str(), nMessageSize);
        return false;
    }

    return true;
}
