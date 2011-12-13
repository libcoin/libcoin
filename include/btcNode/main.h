// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_MAIN_H
#define BITCOIN_MAIN_H

#include "btc/bignum.h"
#include "btc/key.h"
#include "btc/script.h"
#include "btc/tx.h"

#include "btcNode/BlockChain.h"
//#include "btcNode/net.h"
//#include "btcNode/db.h"

#include <list>

class Block;
class CBlockIndex;

class CKeyItem;

class Endpoint;
class Inventory;
class CRequestTracker;
class CNode;
class BlockChain;

#ifdef USE_UPNP
static const int fHaveUPnP = true;
#else
static const int fHaveUPnP = false;
#endif

extern CCriticalSection cs_main;

class CTransaction;
extern std::map<uint256, CTransaction> mapTransactions;
extern std::map<uint160, std::set<std::pair<uint256, unsigned int> > > mapCredits;
extern std::map<uint160, std::set<std::pair<uint256, unsigned int> > > mapDebits;
extern CCriticalSection cs_mapTransactions;

extern BlockChain* __blockChain;

//extern std::map<uint256, CBlockIndex*> mapBlockIndex;
extern CBlockIndex* pindexGenesisBlock;
extern unsigned int nTransactionsUpdated;
extern double dHashesPerSec;
extern int64 nHPSTimerStart;
extern CBigNum bnProofOfWorkLimit;

const int nInitialBlockThreshold = 120; // Regard blocks up until N-threshold as "initial download"

// Settings
extern int fGenerateBitcoins;
extern int64 nTransactionFee;
extern int fLimitProcessors;
extern int nLimitProcessors;
extern int fMinimizeToTray;
extern int fMinimizeOnClose;
extern int fUseUPnP;





class CReserveKey;
class TxIndex;

Block* CreateNewBlock(CReserveKey& reservekey);
void IncrementExtraNonce(Block* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce);
void FormatHashBuffers(Block* pblock, char* pmidstate, char* pdata, char* phash1);

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

//
// The basic transaction that is broadcasted on the network and contained in
// blocks.  A transaction can contain multiple inputs and outputs.
//
class CTransaction : public CTx
{
public:
    
    CTransaction() : CTx() {}
    CTransaction(const CTx& tx) : CTx(tx) {} // nothing to be initialized in CTransaction
    
    bool ClientConnectInputs();
    bool CheckTransaction() const;
    bool AcceptToMemoryPool(bool fCheckInputs=true, bool* pfMissingInputs=NULL);
protected:
    bool AddToMemoryPoolUnchecked();
public:
    bool RemoveFromMemoryPool();
};


#endif
