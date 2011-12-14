// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "btcNode/main.h"

#include "btc/util.h"

#include "btcNode/BlockChain.h"

using namespace std;
using namespace boost;

//
// Global state
//

CCriticalSection cs_main;

BlockChain* __blockChain;

// Settings
int fGenerateBitcoins = false;
int64 nTransactionFee = 0;
int fLimitProcessors = false;
int nLimitProcessors = 1;
int fMinimizeToTray = true;
int fMinimizeOnClose = true;

#if USE_UPNP
int fUseUPnP = true;
#else
int fUseUPnP = false;
#endif

void MainFrameRepaint()
{
    CMain::instance().repaint();
}

void Shutdown(void* ptr)
{
    CMain::instance().shutdown();
}
