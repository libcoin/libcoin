// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_IRC_H
#define BITCOIN_IRC_H

#include <string>
#include "btc/util.h"

bool RecvLine(SOCKET hSocket, std::string& strLine);
void ThreadIRCSeed(void* parg);

extern int nGotIREndpointes;
extern bool fGotExternalIP;

#endif
