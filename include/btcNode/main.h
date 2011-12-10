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

#include "btcNode/net.h"
//#include "btcNode/db.h"

#include <list>

class CBlock;
class CBlockIndex;

class CKeyItem;

class Endpoint;
class Inventory;
class CRequestTracker;
class CNode;
class CBlockIndex;

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

extern std::map<uint256, CBlockIndex*> mapBlockIndex;
extern uint256 hashGenesisBlock;
extern CBlockIndex* pindexGenesisBlock;
extern CBigNum bnBestChainWork;
extern CBigNum bnBestInvalidWork;
extern uint256 hashBestChain;
extern CBlockIndex* pindexBest;
extern unsigned int nTransactionsUpdated;
extern double dHashesPerSec;
extern int64 nHPSTimerStart;
extern int64 nTimeBestReceived;
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
class CTxDB;
class CTxIndex;

//void RegisterWallet(CWallet* pwalletIn);
//void UnregisterWallet(CWallet* pwalletIn);
bool CheckDiskSpace(uint64 nAdditionalBytes=0);
FILE* OpenBlockFile(unsigned int nFile, unsigned int nBlockPos, const char* pszMode="rb");
FILE* AppendBlockFile(unsigned int& nFileRet);
bool LoadBlockIndex(bool fAllowNew=true);
void PrintBlockTree();
//bool ProcessMessages(CNode* pfrom);
//bool SendMessages(CNode* pto, bool fSendTrickle);
//void GenerateBitcoins(bool fGenerate, CWallet* pwallet);
CBlock* CreateNewBlock(CReserveKey& reservekey);
void IncrementExtraNonce(CBlock* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce);
void FormatHashBuffers(CBlock* pblock, char* pmidstate, char* pdata, char* phash1);
//bool CheckWork(CBlock* pblock, CWallet& wallet, CReserveKey& reservekey);
bool CheckProofOfWork(uint256 hash, unsigned int nBits);
int GetTotalBlocksEstimate();
bool IsInitialBlockDownload();
std::string GetWarnings(std::string strFor);

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

class CDiskTxPos
{
public:
    unsigned int nFile;
    unsigned int nBlockPos;
    unsigned int nTxPos;

    CDiskTxPos()
    {
        SetNull();
    }

    CDiskTxPos(unsigned int nFileIn, unsigned int nBlockPosIn, unsigned int nTxPosIn)
    {
        nFile = nFileIn;
        nBlockPos = nBlockPosIn;
        nTxPos = nTxPosIn;
    }

    IMPLEMENT_SERIALIZE( READWRITE(FLATDATA(*this)); )
    void SetNull() { nFile = -1; nBlockPos = 0; nTxPos = 0; }
    bool IsNull() const { return (nFile == -1); }

    friend bool operator==(const CDiskTxPos& a, const CDiskTxPos& b)
    {
        return (a.nFile     == b.nFile &&
                a.nBlockPos == b.nBlockPos &&
                a.nTxPos    == b.nTxPos);
    }

    friend bool operator!=(const CDiskTxPos& a, const CDiskTxPos& b)
    {
        return !(a == b);
    }

    std::string toString() const
    {
        if (IsNull())
            return strprintf("null");
        else
            return strprintf("(nFile=%d, nBlockPos=%d, nTxPos=%d)", nFile, nBlockPos, nTxPos);
    }

    void print() const
    {
        printf("%s", toString().c_str());
    }
};


//
// The basic transaction that is broadcasted on the network and contained in
// blocks.  A transaction can contain multiple inputs and outputs.
//
class CTransaction : public CTx
{
public:
    
    CTransaction() : CTx() {}
    CTransaction(const CTx& tx) : CTx(tx) {} // nothing to be initialized in CTransaction
    
    bool ReadFromDisk(CDiskTxPos pos, FILE** pfileRet=NULL)
    {
        CAutoFile filein = OpenBlockFile(pos.nFile, 0, pfileRet ? "rb+" : "rb");
        if (!filein)
            return error("CTransaction::ReadFromDisk() : OpenBlockFile failed");

        // Read transaction
        if (fseek(filein, pos.nTxPos, SEEK_SET) != 0)
            return error("CTransaction::ReadFromDisk() : fseek failed");
        filein >> *this;

        // Return file pointer
        if (pfileRet)
        {
            if (fseek(filein, pos.nTxPos, SEEK_SET) != 0)
                return error("CTransaction::ReadFromDisk() : second fseek failed");
            *pfileRet = filein.release();
        }
        return true;
    }

    bool ReadFromDisk(CTxDB& txdb, COutPoint prevout, CTxIndex& txindexRet);
    bool ReadFromDisk(CTxDB& txdb, COutPoint prevout);
    bool ReadFromDisk(COutPoint prevout);
    bool DisconnectInputs(CTxDB& txdb);
    bool ConnectInputs(CTxDB& txdb, std::map<uint256, CTxIndex>& mapTestPool, CDiskTxPos posThisTx,
                       CBlockIndex* pindexBlock, int64& nFees, bool fBlock, bool fMiner, int64 nMinFee=0);
    bool ClientConnectInputs();
    bool CheckTransaction() const;
    bool AcceptToMemoryPool(CTxDB& txdb, bool fCheckInputs=true, bool* pfMissingInputs=NULL);
    bool AcceptToMemoryPool(bool fCheckInputs=true, bool* pfMissingInputs=NULL);
protected:
    bool AddToMemoryPoolUnchecked();
public:
    bool RemoveFromMemoryPool();
};


//
// A txdb record that contains the disk location of a transaction and the
// locations of transactions that spend its outputs.  vSpent is really only
// used as a flag, but having the location is very helpful for debugging.
//
class CTxIndex
{
public:
    CDiskTxPos pos;
    std::vector<CDiskTxPos> vSpent;

    CTxIndex()
    {
        SetNull();
    }

    CTxIndex(const CDiskTxPos& posIn, unsigned int nOutputs)
    {
        pos = posIn;
        vSpent.resize(nOutputs);
    }

    IMPLEMENT_SERIALIZE
    (
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(pos);
        READWRITE(vSpent);
    )

    void SetNull()
    {
        pos.SetNull();
        vSpent.clear();
    }

    bool IsNull()
    {
        return pos.IsNull();
    }

    friend bool operator==(const CTxIndex& a, const CTxIndex& b)
    {
        return (a.pos    == b.pos &&
                a.vSpent == b.vSpent);
    }

    friend bool operator!=(const CTxIndex& a, const CTxIndex& b)
    {
        return !(a == b);
    }
    int GetDepthInMainChain() const;
};


#endif
