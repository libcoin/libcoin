// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_MAIN_H
#define BITCOIN_MAIN_H

#include "btcNode/BlockChain.h"

#ifdef USE_UPNP
static const int fHaveUPnP = true;
#else
static const int fHaveUPnP = false;
#endif

extern CCriticalSection cs_main;

extern BlockChain* __blockChain;
// Settings
extern int fGenerateBitcoins;
extern int64 nTransactionFee;
extern int fLimitProcessors;
extern int nLimitProcessors;
extern int fMinimizeToTray;
extern int fMinimizeOnClose;
extern int fUseUPnP;

class CHandler
{
public:
    virtual void operator()()
    {
    }
};

class CMain
{
private:
    CMain() {}
        
private:
    CHandler _shutdown;
    CHandler _repaint;
    
public:
    static CMain& instance()
    {
        static CMain _instance;
        return _instance;
    }
    
    void registerShutdownHandler(const CHandler& handler)
    {
        _shutdown = handler;
    }
    
    void shutdown()
    {
        _shutdown();
    }
    
    void registerRepaintHandler(const CHandler& handler)
    {
        _repaint = handler;
    }
    void repaint()
    {
        _repaint();
    }    
};

void MainFrameRepaint();
void Shutdown(void* ptr);

#endif
