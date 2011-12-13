// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "btcNode/main.h"
#include "btcNode/irc.h"
#include "btcNode/db.h"
#include "btcNode/net.h"

#include "btcNode/EndpointPool.h"
#include "btcNode/Alert.h"
#include "btcNode/BlockChain.h"
#include "btcNode/Block.h"

#include "btc/strlcpy.h"

#ifdef _WIN32
#include <string.h>
#endif

#include <boost/asio.hpp>

#ifdef USE_UPNP
#include <miniupnpc/miniwget.h>
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <mswsock.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <io.h>
#include <process.h>
#include <malloc.h>
#else
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <fcntl.h>
#include <signal.h>
#endif
#ifdef BSD
#include <netinet/in.h>
#endif

using namespace std;
using namespace boost;

static const int MAX_OUTBOUND_CONNECTIONS = 8;

void ThreadMessageHandler2(void* parg);
void ThreadSocketHandler2(void* parg);
void ThreadOpenConnections2(void* parg);
#ifdef USE_UPNP
void ThreadMapPort2(void* parg);
#endif
bool OpenNetworkConnection(const Endpoint& addrConnect);





//
// Global state variables
//
bool fClient = false;
bool fAllowDNS = false;
uint64 nLocalServices = (fClient ? 0 : NODE_NETWORK);
Endpoint __localhost("0.0.0.0", 0, false, nLocalServices);
uint64 nLocalHostNonce = 0;
array<int, 10> vnThreadsRunning;
static SOCKET hListenSocket = INVALID_SOCKET;

vector<CNode*> vNodes;
CCriticalSection cs_vNodes;
//map<vector<unsigned char>, Endpoint> mapAddresses;
EndpointPool* _endpointPool;
CCriticalSection cs_mapAddresses;
map<Inventory, CDataStream> mapRelay;
deque<pair<int64, Inventory> > vRelayExpiration;
CCriticalSection cs_mapRelay;
map<Inventory, int64> mapAlreadyAskedFor;

// Settings
int fUseProxy = false;
int nConnectTimeout = 5000;
Endpoint addrProxy("127.0.0.1",9050);




unsigned short GetListenPort()
{
    return (unsigned short)(GetArg("-port", getDefaultPort()));
}

//////////////////////////////////////////////////////////////////////////////
//
// mapOrphanTransactions
//

map<uint256, Block*> mapOrphanBlocks;
multimap<uint256, Block*> mapOrphanBlocksByPrev;

map<uint256, CDataStream*> mapOrphanTransactions;
multimap<uint256, CDataStream*> mapOrphanTransactionsByPrev;

void static AddOrphanTx(const CDataStream& vMsg)
{
    CTransaction tx;
    CDataStream(vMsg) >> tx;
    uint256 hash = tx.GetHash();
    if (mapOrphanTransactions.count(hash))
        return;
    CDataStream* pvMsg = mapOrphanTransactions[hash] = new CDataStream(vMsg);
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    mapOrphanTransactionsByPrev.insert(make_pair(txin.prevout.hash, pvMsg));
}

void static EraseOrphanTx(uint256 hash)
{
    if (!mapOrphanTransactions.count(hash))
        return;
    const CDataStream* pvMsg = mapOrphanTransactions[hash];
    CTransaction tx;
    CDataStream(*pvMsg) >> tx;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        for (multimap<uint256, CDataStream*>::iterator mi = mapOrphanTransactionsByPrev.lower_bound(txin.prevout.hash);
             mi != mapOrphanTransactionsByPrev.upper_bound(txin.prevout.hash);)
        {
            if ((*mi).second == pvMsg)
                mapOrphanTransactionsByPrev.erase(mi++);
            else
                mi++;
        }
    }
    delete pvMsg;
    mapOrphanTransactions.erase(hash);
}

uint256 static GetOrphanRoot(const Block* pblock)
{
    // Work back to the first block in the orphan chain
    while (mapOrphanBlocks.count(pblock->getPrevBlock()))
        pblock = mapOrphanBlocks[pblock->getPrevBlock()];
    return pblock->getHash();
}


bool static AlreadyHave(const Inventory& inv)
{
    switch (inv.getType())
    {
        case MSG_TX:    return mapTransactions.count(inv.getHash()) || mapOrphanTransactions.count(inv.getHash()) || __blockChain->containsTx(inv.getHash());
        case MSG_BLOCK: return __blockChain->containsBlock(inv.getHash()) || mapOrphanBlocks.count(inv.getHash());
    }
    // Don't know what it is, just say we already got one
    return true;
}

//////////////////////////////////////////////////////////////////////////////
//
// Messages
//

bool static ProcessBlock(CNode* pfrom, Block* pblock)
{
    // Check for duplicate
    uint256 hash = pblock->getHash();
    if (__blockChain->containsBlock(hash))
        return error("ProcessBlock() : already have block %d %s", __blockChain->getBlockIndex(hash)->nHeight, hash.toString().substr(0,20).c_str());
    if (mapOrphanBlocks.count(hash))
        return error("ProcessBlock() : already have block (orphan) %s", hash.toString().substr(0,20).c_str());
    
    // Preliminary checks
    if (!pblock->checkBlock())
        return error("ProcessBlock() : CheckBlock FAILED");
    
    // If don't already have its previous block, shunt it off to holding area until we get it
    if (!__blockChain->containsBlock(pblock->getPrevBlock()))
    {
        printf("ProcessBlock: ORPHAN BLOCK, prev=%s\n", pblock->getPrevBlock().toString().substr(0,20).c_str());
        Block* pblock2 = new Block(*pblock);
        mapOrphanBlocks.insert(make_pair(hash, pblock2));
        mapOrphanBlocksByPrev.insert(make_pair(pblock2->getPrevBlock(), pblock2));
        
        // Ask this guy to fill in what we're missing
        if (pfrom)
            pfrom->PushGetBlocks(__blockChain->getBestIndex(), GetOrphanRoot(pblock2));
        return true;
    }
    
    // Store to disk
    if (!__blockChain->acceptBlock(*pblock))
        return error("ProcessBlock() : AcceptBlock FAILED");
    
    // Recursively process any orphan blocks that depended on this one
    vector<uint256> vWorkQueue;
    vWorkQueue.push_back(hash);
    for (int i = 0; i < vWorkQueue.size(); i++)
    {
        uint256 hashPrev = vWorkQueue[i];
        for (multimap<uint256, Block*>::iterator mi = mapOrphanBlocksByPrev.lower_bound(hashPrev);
             mi != mapOrphanBlocksByPrev.upper_bound(hashPrev);
             ++mi)
        {
            Block* pblockOrphan = (*mi).second;
            if (__blockChain->acceptBlock(*pblockOrphan))
                vWorkQueue.push_back(pblockOrphan->getHash());
            mapOrphanBlocks.erase(pblockOrphan->getHash());
            delete pblockOrphan;
        }
        mapOrphanBlocksByPrev.erase(hashPrev);
    }
    
    printf("ProcessBlock: ACCEPTED\n");
    return true;
}

// The message start string is designed to be unlikely to occur in normal data.
// The characters are rarely used upper ascii, not valid as UTF-8, and produce
// a large 4-byte int at any alignment.
unsigned char pchMessageStart[4] = { 0xf9, 0xbe, 0xb4, 0xd9 };


bool CNode::ProcessMessage(string strCommand, CDataStream& vRecv)
{
    CNode* pfrom = this;
    static map<unsigned int, vector<unsigned char> > mapReuseKey;
    RandAddSeedPerfmon();
    if (fDebug)
        printf("%s ", DateTimeStrFormat("%x %H:%M:%S", GetTime()).c_str());
    printf("received: %s (%d bytes)\n", strCommand.c_str(), vRecv.size());
    if (mapArgs.count("-dropmessagestest") && GetRand(atoi(mapArgs["-dropmessagestest"])) == 0)
    {
        printf("dropmessagestest DROPPING RECV MESSAGE\n");
        return true;
    }
    
    if (strCommand == "version")
    {
        // Each connection can only send one version message
        if (pfrom->nVersion != 0)
            return false;
        
        int64 nTime;
        Endpoint addrMe;
        Endpoint addrFrom;
        uint64 nNonce = 1;
        vRecv >> pfrom->nVersion >> pfrom->nServices >> nTime >> addrMe;
        if (pfrom->nVersion == 10300)
            pfrom->nVersion = 300;
        if (pfrom->nVersion >= 106 && !vRecv.empty())
            vRecv >> addrFrom >> nNonce;
        if (pfrom->nVersion >= 106 && !vRecv.empty())
            vRecv >> pfrom->strSubVer;
        if (pfrom->nVersion >= 209 && !vRecv.empty())
            vRecv >> pfrom->nStartingHeight;
        
        if (pfrom->nVersion == 0)
            return false;
        
        // Disconnect if we connected to ourself
        if (nNonce == nLocalHostNonce && nNonce > 1)
        {
            printf("connected to self at %s, disconnecting\n", pfrom->addr.toString().c_str());
            pfrom->fDisconnect = true;
            return true;
        }
        
        // Be shy and don't send version until we hear
        if (pfrom->fInbound)
            pfrom->PushVersion();
        
        pfrom->fClient = !(pfrom->nServices & NODE_NETWORK);
        
        AddTimeData(pfrom->addr.getIP(), nTime);
        
        // Change version
        if (pfrom->nVersion >= 209)
            pfrom->PushMessage("verack");
        pfrom->vSend.SetVersion(min(pfrom->nVersion, VERSION));
        if (pfrom->nVersion < 209)
            pfrom->vRecv.SetVersion(min(pfrom->nVersion, VERSION));
        
        if (!pfrom->fInbound)
        {
            // Advertise our address
            if (__localhost.isRoutable() && !fUseProxy)
            {
                Endpoint addr(__localhost);
                addr.setTime(GetAdjustedTime());
                pfrom->PushAddress(addr);
            }
            
            // Get recent addresses
            if (pfrom->nVersion >= 31402 || _endpointPool->getPoolSize() < 1000)
            {
                pfrom->PushMessage("getaddr");
                pfrom->fGetAddr = true;
            }
        }
        
        // Ask the first connected node for block updates
        static int nAskedForBlocks;
        if (!pfrom->fClient &&
            (pfrom->nVersion < 32000 || pfrom->nVersion >= 32400) &&
            (nAskedForBlocks < 1 || vNodes.size() <= 1))
        {
            nAskedForBlocks++;
            pfrom->PushGetBlocks(__blockChain->getBestIndex(), uint256(0));
        }
        
        // Relay alerts
        CRITICAL_BLOCK(cs_mapAlerts)
        BOOST_FOREACH(PAIRTYPE(const uint256, Alert)& item, mapAlerts)
        item.second.relayTo(pfrom);
        
        pfrom->fSuccessfullyConnected = true;
        
        printf("version message: version %d, blocks=%d\n", pfrom->nVersion, pfrom->nStartingHeight);
    }
    
    
    else if (pfrom->nVersion == 0)
    {
        // Must have a version message before anything else
        return false;
    }
    
    
    else if (strCommand == "verack")
    {
        pfrom->vRecv.SetVersion(min(pfrom->nVersion, VERSION));
    }
    
    
    else if (strCommand == "addr")
    {
        vector<Endpoint> vAddr;
        vRecv >> vAddr;
        
        // Don't want addr from older versions unless seeding
        if (pfrom->nVersion < 209)
            return true;
        if (pfrom->nVersion < 31402 && _endpointPool->getPoolSize() > 1000)
            return true;
        if (vAddr.size() > 1000)
            return error("message addr size() = %d", vAddr.size());
        
        // Store the new addresses
        //        CAddrDB addrDB;
        //addrDB.TxnBegin();
        int64 nNow = GetAdjustedTime();
        int64 nSince = nNow - 10 * 60;
        BOOST_FOREACH(Endpoint& addr, vAddr)
        {
            if (fShutdown)
                return true;
            // ignore IPv6 for now, since it isn't implemented anyway
            if (!addr.isIPv4())
                continue;
            if (addr.getTime() <= 100000000 || addr.getTime() > nNow + 10 * 60)
                addr.setTime(nNow - 5 * 24 * 60 * 60);
            _endpointPool->addEndpoint(addr, 2 * 60 * 60);
            pfrom->AddAddressKnown(addr);
            if (addr.getTime() > nSince && !pfrom->fGetAddr && vAddr.size() <= 10 && addr.isRoutable())
            {
                // Relay to a limited number of other nodes
                CRITICAL_BLOCK(cs_vNodes)
                {
                    // Use deterministic randomness to send to the same nodes for 24 hours
                    // at a time so the setAddrKnowns of the chosen nodes prevent repeats
                    static uint256 hashSalt;
                    if (hashSalt == 0)
                        RAND_bytes((unsigned char*)&hashSalt, sizeof(hashSalt));
                    uint256 hashRand = hashSalt ^ (((int64)addr.getIP())<<32) ^ ((GetTime() + addr.getIP())/(24*60*60));
                    hashRand = Hash(BEGIN(hashRand), END(hashRand));
                    multimap<uint256, CNode*> mapMix;
                    BOOST_FOREACH(CNode* pnode, vNodes)
                    {
                        if (pnode->nVersion < 31402)
                            continue;
                        unsigned int nPointer;
                        memcpy(&nPointer, &pnode, sizeof(nPointer));
                        uint256 hashKey = hashRand ^ nPointer;
                        hashKey = Hash(BEGIN(hashKey), END(hashKey));
                        mapMix.insert(make_pair(hashKey, pnode));
                    }
                    int nRelayNodes = 2;
                    for (multimap<uint256, CNode*>::iterator mi = mapMix.begin(); mi != mapMix.end() && nRelayNodes-- > 0; ++mi)
                        ((*mi).second)->PushAddress(addr);
                }
            }
        }
    //        addrDB.TxnCommit();  // Save addresses (it's ok if this fails)
        if (vAddr.size() < 1000)
            pfrom->fGetAddr = false;
    }
    
    
    else if (strCommand == "inv")
    {
        vector<Inventory> vInv;
        vRecv >> vInv;
        if (vInv.size() > 50000)
            return error("message inv size() = %d", vInv.size());
        
    //        CTxDB txdb("r");
        BOOST_FOREACH(const Inventory& inv, vInv)
        {
            if (fShutdown)
                return true;
            pfrom->AddInventoryKnown(inv);
            
            bool fAlreadyHave = AlreadyHave(inv);
            printf("  got inventory: %s  %s\n", inv.toString().c_str(), fAlreadyHave ? "have" : "new");
            
            if (!fAlreadyHave)
                pfrom->AskFor(inv);
            else if (inv.getType() == MSG_BLOCK && mapOrphanBlocks.count(inv.getHash()))
                pfrom->PushGetBlocks(__blockChain->getBestIndex(), GetOrphanRoot(mapOrphanBlocks[inv.getHash()]));
            
            // Track requests for our stuff
            //            Inventory(inv.hash);
        }
    }
    
    
    else if (strCommand == "getdata")
    {
        vector<Inventory> vInv;
        vRecv >> vInv;
        if (vInv.size() > 50000)
            return error("message getdata size() = %d", vInv.size());
        
        BOOST_FOREACH(const Inventory& inv, vInv)
        {
            if (fShutdown)
                return true;
            printf("received getdata for: %s\n", inv.toString().c_str());
            
            if (inv.getType() == MSG_BLOCK) {
                // Send block from disk
                Block block;
                __blockChain->readBlock(inv.getHash(), block);
                if (!block.isNull()) {
                    pfrom->PushMessage("block", block);
                    
                    // Trigger them to send a getblocks request for the next batch of inventory
                    if (inv.getHash() == pfrom->hashContinue)
                    {
                        // Bypass PushInventory, this must send even if redundant,
                        // and we want it right after the last block so they don't
                        // wait for other stuff first.
                        vector<Inventory> vInv;
                        vInv.push_back(Inventory(MSG_BLOCK, __blockChain->getBestChain()));
                        pfrom->PushMessage("inv", vInv);
                        pfrom->hashContinue = 0;
                    }
                }
            }
            else if (inv.isKnownType())
            {
                // Send stream from relay memory
                CRITICAL_BLOCK(cs_mapRelay)
                {
                    map<Inventory, CDataStream>::iterator mi = mapRelay.find(inv);
                    if (mi != mapRelay.end())
                        pfrom->PushMessage(inv.getCommand(), (*mi).second);
                }
            }
            
            // Track requests for our stuff
            //            Inventory(inv.hash);
        }
    }
    
    
    else if (strCommand == "getblocks")
    {
        CBlockLocator locator;
        uint256 hashStop;
        vRecv >> locator >> hashStop;
        
        // Find the last block the caller has in the main chain
        CBlockIndex* pindex = __blockChain->getBlockIndex(locator);
        
        // Send the rest of the chain
        if (pindex)
            pindex = pindex->pnext;
        int nLimit = 500 + __blockChain->getDistanceBack(locator);
        unsigned int nBytes = 0;
        printf("getblocks %d to %s limit %d\n", (pindex ? pindex->nHeight : -1), hashStop.toString().substr(0,20).c_str(), nLimit);
        for (; pindex; pindex = pindex->pnext)
        {
            if (pindex->GetBlockHash() == hashStop)
            {
                printf("  getblocks stopping at %d %s (%u bytes)\n", pindex->nHeight, pindex->GetBlockHash().toString().substr(0,20).c_str(), nBytes);
                break;
            }
            pfrom->PushInventory(Inventory(MSG_BLOCK, pindex->GetBlockHash()));
            Block block;
            __blockChain->readBlock(pindex->GetBlockHash(), block);
            nBytes += block.GetSerializeSize(SER_NETWORK);
            if (--nLimit <= 0 || nBytes >= SendBufferSize()/2)
            {
                // When this block is requested, we'll send an inv that'll make them
                // getblocks the next batch of inventory.
                printf("  getblocks stopping at limit %d %s (%u bytes)\n", pindex->nHeight, pindex->GetBlockHash().toString().substr(0,20).c_str(), nBytes);
                pfrom->hashContinue = pindex->GetBlockHash();
                break;
            }
        }
    }
    
    
    else if (strCommand == "getheaders")
    {
        CBlockLocator locator;
        uint256 hashStop;
        vRecv >> locator >> hashStop;
        
        CBlockIndex* pindex = NULL;
        if (locator.IsNull()) {
            // If locator is null, return the hashStop block
            pindex = __blockChain->getHashStopIndex(hashStop);
            if (pindex == NULL) return true;
        }
        else
        {
            // Find the last block the caller has in the main chain
            pindex = __blockChain->getBlockIndex(locator);
            if (pindex)
                pindex = pindex->pnext;
        }
        
        vector<Block> vHeaders;
        int nLimit = 2000 + __blockChain->getDistanceBack(locator);
        printf("getheaders %d to %s limit %d\n", (pindex ? pindex->nHeight : -1), hashStop.toString().substr(0,20).c_str(), nLimit);
        for (; pindex; pindex = pindex->pnext)
        {
            vHeaders.push_back(pindex->GetBlockHeader());
            if (--nLimit <= 0 || pindex->GetBlockHash() == hashStop)
                break;
        }
        pfrom->PushMessage("headers", vHeaders);
    }
    
    
    else if (strCommand == "tx")
    {
        vector<uint256> vWorkQueue;
        CDataStream vMsg(vRecv);
        CTransaction tx;
        vRecv >> tx;
        
        Inventory inv(MSG_TX, tx.GetHash());
        pfrom->AddInventoryKnown(inv);
        
        bool fMissingInputs = false;
        if (tx.AcceptToMemoryPool(true, &fMissingInputs))
        {
            //            SyncWithWallets(tx, NULL, true);
            RelayMessage(inv, vMsg);
            mapAlreadyAskedFor.erase(inv);
            vWorkQueue.push_back(inv.getHash());
            
            // Recursively process any orphan transactions that depended on this one
            for (int i = 0; i < vWorkQueue.size(); i++)
            {
                uint256 hashPrev = vWorkQueue[i];
                for (multimap<uint256, CDataStream*>::iterator mi = mapOrphanTransactionsByPrev.lower_bound(hashPrev);
                     mi != mapOrphanTransactionsByPrev.upper_bound(hashPrev);
                     ++mi)
                {
                    const CDataStream& vMsg = *((*mi).second);
                    CTransaction tx;
                    CDataStream(vMsg) >> tx;
                    Inventory inv(MSG_TX, tx.GetHash());
                    
                    if (tx.AcceptToMemoryPool(true))
                    {
                        printf("   accepted orphan tx %s\n", inv.getHash().toString().substr(0,10).c_str());
                        //                        SyncWithWallets(tx, NULL, true);
                        RelayMessage(inv, vMsg);
                        mapAlreadyAskedFor.erase(inv);
                        vWorkQueue.push_back(inv.getHash());
                    }
                }
            }
            
            BOOST_FOREACH(uint256 hash, vWorkQueue)
            EraseOrphanTx(hash);
        }
        else if (fMissingInputs)
        {
            printf("storing orphan tx %s\n", inv.getHash().toString().substr(0,10).c_str());
            AddOrphanTx(vMsg);
        }
    }
    
    
    else if (strCommand == "block")
    {
        Block block;
        vRecv >> block;
        
        printf("received block %s\n", block.getHash().toString().substr(0,20).c_str());
        // block.print();
        
        Inventory inv(MSG_BLOCK, block.getHash());
        pfrom->AddInventoryKnown(inv);
        
        if (ProcessBlock(pfrom, &block))
            mapAlreadyAskedFor.erase(inv);
    }
    
    
    else if (strCommand == "getaddr") {
        // Nodes rebroadcast an addr every 24 hours
        pfrom->vAddrToSend.clear();
        set<Endpoint> endpoints = _endpointPool->getRecent(3*60*60, 2500);
        for (set<Endpoint>::iterator ep = endpoints.begin(); ep != endpoints.end(); ++ep)
            pfrom->PushAddress(*ep);
    }
    
    else if (strCommand == "ping")
    {
    }
    
    
    else if (strCommand == "alert")
    {
        Alert alert;
        vRecv >> alert;
        
        if (alert.processAlert())
        {
            // Relay
            pfrom->setKnown.insert(alert.getHash());
            CRITICAL_BLOCK(cs_vNodes)
            BOOST_FOREACH(CNode* pnode, vNodes)
            alert.relayTo(pnode);
        }
    }
    
    
    else
    {
        // Ignore unknown commands for extensibility
    }
    
    
    // Update the last seen time for this node's address
    if (pfrom->fNetworkNode)
        if (strCommand == "version" || strCommand == "addr" || strCommand == "inv" || strCommand == "getdata" || strCommand == "ping")
            _endpointPool->currentlyConnected(pfrom->addr);
    
    
    return true;
}

bool CNode::ProcessMessages()
{
    CNode* pfrom = this;
    CDataStream& vRecv = pfrom->vRecv;
    if (vRecv.empty())
        return true;
    //if (fDebug)
    //    printf("ProcessMessages(%u bytes)\n", vRecv.size());
    
    //
    // Message format
    //  (4) message start
    //  (12) command
    //  (4) size
    //  (4) checksum
    //  (x) data
    //
    
    loop
    {
        // Scan for message start
        CDataStream::iterator pstart = search(vRecv.begin(), vRecv.end(), BEGIN(pchMessageStart), END(pchMessageStart));
        int nHeaderSize = vRecv.GetSerializeSize(MessageHeader());
        if (vRecv.end() - pstart < nHeaderSize)
        {
            if (vRecv.size() > nHeaderSize)
            {
                printf("\n\nPROCESSMESSAGE MESSAGESTART NOT FOUND\n\n");
                vRecv.erase(vRecv.begin(), vRecv.end() - nHeaderSize);
            }
            break;
        }
        if (pstart - vRecv.begin() > 0)
            printf("\n\nPROCESSMESSAGE SKIPPED %d BYTES\n\n", pstart - vRecv.begin());
        vRecv.erase(vRecv.begin(), pstart);
        
        // Read header
        vector<char> vHeaderSave(vRecv.begin(), vRecv.begin() + nHeaderSize);
        MessageHeader hdr;
        vRecv >> hdr;
        if (!hdr.IsValid())
        {
            printf("\n\nPROCESSMESSAGE: ERRORS IN HEADER %s\n\n\n", hdr.GetCommand().c_str());
            continue;
        }
        string strCommand = hdr.GetCommand();
        
        // Message size
        unsigned int nMessageSize = hdr.nMessageSize;
        if (nMessageSize > MAX_SIZE)
        {
            printf("ProcessMessage(%s, %u bytes) : nMessageSize > MAX_SIZE\n", strCommand.c_str(), nMessageSize);
            continue;
        }
        if (nMessageSize > vRecv.size())
        {
            // Rewind and wait for rest of message
            vRecv.insert(vRecv.begin(), vHeaderSave.begin(), vHeaderSave.end());
            break;
        }
        
        // Checksum
        if (vRecv.GetVersion() >= 209)
        {
            uint256 hash = Hash(vRecv.begin(), vRecv.begin() + nMessageSize);
            unsigned int nChecksum = 0;
            memcpy(&nChecksum, &hash, sizeof(nChecksum));
            if (nChecksum != hdr.nChecksum)
            {
                printf("ProcessMessage(%s, %u bytes) : CHECKSUM ERROR nChecksum=%08x hdr.nChecksum=%08x\n",
                       strCommand.c_str(), nMessageSize, nChecksum, hdr.nChecksum);
                continue;
            }
        }
        
        // Copy message to its own buffer
        CDataStream vMsg(vRecv.begin(), vRecv.begin() + nMessageSize, vRecv.nType, vRecv.nVersion);
        vRecv.ignore(nMessageSize);
        
        // Process message
        bool fRet = false;
        try
        {
            CRITICAL_BLOCK(cs_main)
            fRet = pfrom->ProcessMessage(strCommand, vMsg);
            if (fShutdown)
                return true;
        }
        catch (std::ios_base::failure& e)
        {
            if (strstr(e.what(), "end of data"))
            {
                // Allow exceptions from underlength message on vRecv
                printf("ProcessMessage(%s, %u bytes) : Exception '%s' caught, normally caused by a message being shorter than its stated length\n", strCommand.c_str(), nMessageSize, e.what());
            }
            else if (strstr(e.what(), "size too large"))
            {
                // Allow exceptions from overlong size
                printf("ProcessMessage(%s, %u bytes) : Exception '%s' caught\n", strCommand.c_str(), nMessageSize, e.what());
            }
            else
            {
                PrintExceptionContinue(&e, "ProcessMessage()");
            }
        }
        catch (std::exception& e) {
            PrintExceptionContinue(&e, "ProcessMessage()");
        } catch (...) {
            PrintExceptionContinue(NULL, "ProcessMessage()");
        }
        
        if (!fRet)
            printf("ProcessMessage(%s, %u bytes) FAILED\n", strCommand.c_str(), nMessageSize);
    }
    
    vRecv.Compact();
    return true;
}

void ResendBrokerTransactions()
{
    // Do this infrequently and randomly to avoid giving away
    // that these are our transactions.
    static int64 nNextTime;
    if (GetTime() < nNextTime)
        return;
    bool fFirst = (nNextTime == 0);
    nNextTime = GetTime() + GetRand(30 * 60);
    if (fFirst)
        return;
    
    // Only do it if there's been a new block since last time
    static int64 nLastTime;
    if (__blockChain->getBestReceivedTime() < nLastTime)
        return;
    nLastTime = GetTime();
    
    // Rebroadcast any of our txes that aren't in a block yet, if we have reached 100 confirmations forget about it
    printf("ResendBrokerTransactions()\n");
    CBrokerDB brokerdb;
    map<uint256, CTx> txes;
    brokerdb.LoadTxes(txes);
    
    for(map<uint256, CTx>::iterator it = txes.begin(); it != txes.end(); ++it) {
        int depth = __blockChain->getDepthInMainChain(it->first);
        if (depth > 0) {
            if(depth > 100)
                brokerdb.EraseTx(it->second);
            continue;
        }

        // rebroadcast tx
        RelayMessage(Inventory(MSG_TX, it->first), it->second);
    }
}

bool CNode::SendMessages(bool fSendTrickle)
{
    CNode* pto = this;
    CRITICAL_BLOCK(cs_main)
    {
        // Don't send anything until we get their version message
        if (pto->nVersion == 0)
            return true;
        
        // Keep-alive ping
        if (pto->nLastSend && GetTime() - pto->nLastSend > 30 * 60 && pto->vSend.empty())
            pto->PushMessage("ping");
        
        // Resend wallet transactions that haven't gotten in a block yet
        //        ResendWalletTransactions();
        ResendBrokerTransactions();
        
        // Address refresh broadcast
        static int64 nLastRebroadcast;
        if (GetTime() - nLastRebroadcast > 24 * 60 * 60)
        {
            nLastRebroadcast = GetTime();
            CRITICAL_BLOCK(cs_vNodes)
            {
                BOOST_FOREACH(CNode* pnode, vNodes)
                {
                    // Periodically clear setAddrKnown to allow refresh broadcasts
                    pnode->setAddrKnown.clear();
                    
                    // Rebroadcast our address
                    if (__localhost.isRoutable() && !fUseProxy)
                    {
                        Endpoint addr(__localhost);
                        addr.setTime(GetAdjustedTime());
                        pnode->PushAddress(addr);
                    }
                }
            }
        }
        
        // Clear out old addresses periodically so it's not too much work at once
        _endpointPool->purge();
    
        //
        // Message: addr
        //
        if (fSendTrickle)
        {
            vector<Endpoint> vAddr;
            vAddr.reserve(pto->vAddrToSend.size());
            BOOST_FOREACH(const Endpoint& addr, pto->vAddrToSend)
            {
                // returns true if wasn't already contained in the set
                if (pto->setAddrKnown.insert(addr).second)
                {
                    vAddr.push_back(addr);
                    // receiver rejects addr messages larger than 1000
                    if (vAddr.size() >= 1000)
                    {
                        pto->PushMessage("addr", vAddr);
                        vAddr.clear();
                    }
                }
            }
            pto->vAddrToSend.clear();
            if (!vAddr.empty())
                pto->PushMessage("addr", vAddr);
        }
        
        
        //
        // Message: inventory
        //
        vector<Inventory> vInv;
        vector<Inventory> vInvWait;
        CRITICAL_BLOCK(pto->cs_inventory)
        {
            vInv.reserve(pto->vInventoryToSend.size());
            vInvWait.reserve(pto->vInventoryToSend.size());
            BOOST_FOREACH(const Inventory& inv, pto->vInventoryToSend)
            {
                if (pto->setInventoryKnown.count(inv))
                    continue;
                
                // trickle out tx inv to protect privacy
                if (inv.getType() == MSG_TX && !fSendTrickle)
                {
                    // 1/4 of tx invs blast to all immediately
                    static uint256 hashSalt;
                    if (hashSalt == 0)
                        RAND_bytes((unsigned char*)&hashSalt, sizeof(hashSalt));
                    uint256 hashRand = inv.getHash() ^ hashSalt;
                    hashRand = Hash(BEGIN(hashRand), END(hashRand));
                    bool fTrickleWait = ((hashRand & 3) != 0);
                    /*
                     // always trickle our own transactions
                     if (!fTrickleWait)
                     {
                     CWalletTx wtx;
                     if (GetTransaction(inv.hash, wtx))
                     if (wtx.fFromMe)
                     fTrickleWait = true;
                     }
                     */
                    if (fTrickleWait)
                    {
                        vInvWait.push_back(inv);
                        continue;
                    }
                }
                
                // returns true if wasn't already contained in the set
                if (pto->setInventoryKnown.insert(inv).second)
                {
                    vInv.push_back(inv);
                    if (vInv.size() >= 1000)
                    {
                        pto->PushMessage("inv", vInv);
                        vInv.clear();
                    }
                }
            }
            pto->vInventoryToSend = vInvWait;
        }
        if (!vInv.empty())
            pto->PushMessage("inv", vInv);
        
        
        //
        // Message: getdata
        //
        vector<Inventory> vGetData;
        int64 nNow = GetTime() * 1000000;

        while (!pto->mapAskFor.empty() && (*pto->mapAskFor.begin()).first <= nNow)
        {
            const Inventory& inv = (*pto->mapAskFor.begin()).second;
            if (!AlreadyHave(inv))
            {
                printf("sending getdata: %s\n", inv.toString().c_str());
                vGetData.push_back(inv);
                if (vGetData.size() >= 1000)
                {
                    pto->PushMessage("getdata", vGetData);
                    vGetData.clear();
                }
            }
            mapAlreadyAskedFor[inv] = nNow;
            pto->mapAskFor.erase(pto->mapAskFor.begin());
        }
        if (!vGetData.empty())
            pto->PushMessage("getdata", vGetData);
        
    }
    return true;
}



void CNode::PushGetBlocks(const CBlockIndex* pindexBegin, uint256 hashEnd)
{
    // Filter out duplicate requests
    if (pindexBegin == pindexLastGetBlocksBegin && hashEnd == hashLastGetBlocksEnd)
        return;
    pindexLastGetBlocksBegin = pindexBegin;
    hashLastGetBlocksEnd = hashEnd;

    PushMessage("getblocks", CBlockLocator(pindexBegin), hashEnd);
}





bool ConnectSocket(const Endpoint& addrConnect, SOCKET& hSocketRet, int nTimeout)
{
    hSocketRet = INVALID_SOCKET;

    SOCKET hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (hSocket == INVALID_SOCKET)
        return false;
#ifdef SO_NOSIGPIPE
    int set = 1;
    setsockopt(hSocket, SOL_SOCKET, SO_NOSIGPIPE, (void*)&set, sizeof(int));
#endif

    bool fProxy = (fUseProxy && addrConnect.isRoutable());
    struct sockaddr_in sockaddr = (fProxy ? addrProxy.getSockAddr() : addrConnect.getSockAddr());

#ifdef _WIN32
    u_long fNonblock = 1;
    if (ioctlsocket(hSocket, FIONBIO, &fNonblock) == SOCKET_ERROR)
#else
    int fFlags = fcntl(hSocket, F_GETFL, 0);
    if (fcntl(hSocket, F_SETFL, fFlags | O_NONBLOCK) == -1)
#endif
    {
        closesocket(hSocket);
        return false;
    }


    if (connect(hSocket, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) == SOCKET_ERROR)
    {
        // WSAEINVAL is here because some legacy version of winsock uses it
        if (WSAGetLastError() == WSAEINPROGRESS || WSAGetLastError() == WSAEWOULDBLOCK || WSAGetLastError() == WSAEINVAL)
        {
            struct timeval timeout;
            timeout.tv_sec  = nTimeout / 1000;
            timeout.tv_usec = (nTimeout % 1000) * 1000;

            fd_set fdset;
            FD_ZERO(&fdset);
            FD_SET(hSocket, &fdset);
            int nRet = select(hSocket + 1, NULL, &fdset, NULL, &timeout);
            if (nRet == 0)
            {
                printf("connection timeout\n");
                closesocket(hSocket);
                return false;
            }
            if (nRet == SOCKET_ERROR)
            {
                printf("select() for connection failed: %i\n",WSAGetLastError());
                closesocket(hSocket);
                return false;
            }
            socklen_t nRetSize = sizeof(nRet);
#ifdef _WIN32
            if (getsockopt(hSocket, SOL_SOCKET, SO_ERROR, (char*)(&nRet), &nRetSize) == SOCKET_ERROR)
#else
            if (getsockopt(hSocket, SOL_SOCKET, SO_ERROR, &nRet, &nRetSize) == SOCKET_ERROR)
#endif
            {
                printf("getsockopt() for connection failed: %i\n",WSAGetLastError());
                closesocket(hSocket);
                return false;
            }
            if (nRet != 0)
            {
                printf("connect() failed after select(): %s\n",strerror(nRet));
                closesocket(hSocket);
                return false;
            }
        }
#ifdef _WIN32
        else if (WSAGetLastError() != WSAEISCONN)
#else
        else
#endif
        {
            printf("connect() failed: %i\n",WSAGetLastError());
            closesocket(hSocket);
            return false;
        }
    }

    /*
    this isn't even strictly necessary
    CNode::ConnectNode immediately turns the socket back to non-blocking
    but we'll turn it back to blocking just in case
    */
#ifdef _WIN32
    fNonblock = 0;
    if (ioctlsocket(hSocket, FIONBIO, &fNonblock) == SOCKET_ERROR)
#else
    fFlags = fcntl(hSocket, F_GETFL, 0);
    if (fcntl(hSocket, F_SETFL, fFlags & !O_NONBLOCK) == SOCKET_ERROR)
#endif
    {
        closesocket(hSocket);
        return false;
    }

    if (fProxy)
    {
        printf("proxy connecting %s\n", addrConnect.toString().c_str());
        char pszSocks4IP[] = "\4\1\0\0\0\0\0\0user";
        unsigned short port = addrConnect.getPort();
        unsigned int ip = addrConnect.getIP();
        memcpy(pszSocks4IP + 2, &port, 2);
        memcpy(pszSocks4IP + 4, &ip, 4);
        char* pszSocks4 = pszSocks4IP;
        int nSize = sizeof(pszSocks4IP);

        int ret = send(hSocket, pszSocks4, nSize, MSG_NOSIGNAL);
        if (ret != nSize)
        {
            closesocket(hSocket);
            return error("Error sending to proxy");
        }
        char pchRet[8];
        if (recv(hSocket, pchRet, 8, 0) != 8)
        {
            closesocket(hSocket);
            return error("Error reading proxy response");
        }
        if (pchRet[1] != 0x5a)
        {
            closesocket(hSocket);
            if (pchRet[1] != 0x5b)
                printf("ERROR: Proxy returned error %d\n", pchRet[1]);
            return false;
        }
        printf("proxy connected %s\n", addrConnect.toString().c_str());
    }

    hSocketRet = hSocket;
    return true;
}

// portDefault is in host order
bool Lookup(const char *pszName, vector<Endpoint>& vaddr, int nServices, int nMaxSolutions, bool fAllowLookup, int portDefault, bool fAllowPort)
{
    vaddr.clear();
    if (pszName[0] == 0)
        return false;
    int port = portDefault;
    char psz[256];
    char *pszHost = psz;
    strlcpy(psz, pszName, sizeof(psz));
    if (fAllowPort)
    {
        char* pszColon = strrchr(psz+1,':');
        char *pszPortEnd = NULL;
        int portParsed = pszColon ? strtoul(pszColon+1, &pszPortEnd, 10) : 0;
        if (pszColon && pszPortEnd && pszPortEnd[0] == 0)
        {
            if (psz[0] == '[' && pszColon[-1] == ']')
            {
                // Future: enable IPv6 colon-notation inside []
                pszHost = psz+1;
                pszColon[-1] = 0;
            }
            else
                pszColon[0] = 0;
            port = portParsed;
            if (port < 0 || port > USHRT_MAX)
                port = USHRT_MAX;
        }
    }

    unsigned int addrIP = inet_addr(pszHost);
    if (addrIP != INADDR_NONE)
    {
        // valid IP address passed
        vaddr.push_back(Endpoint(addrIP, port, nServices));
        return true;
    }

    if (!fAllowLookup)
        return false;

    struct hostent* phostent = gethostbyname(pszHost);
    if (!phostent)
        return false;

    if (phostent->h_addrtype != AF_INET)
        return false;

    char** ppAddr = phostent->h_addr_list;
    while (*ppAddr != NULL && vaddr.size() != nMaxSolutions)
    {
        Endpoint addr(((struct in_addr*)ppAddr[0])->s_addr, port, nServices);
        if (addr.isValid())
            vaddr.push_back(addr);
        ppAddr++;
    }

    return (vaddr.size() > 0);
}

// portDefault is in host order
bool Lookup(const char *pszName, Endpoint& addr, int nServices, bool fAllowLookup, int portDefault, bool fAllowPort)
{
    vector<Endpoint> vaddr;
    bool fRet = Lookup(pszName, vaddr, nServices, 1, fAllowLookup, portDefault, fAllowPort);
    if (fRet)
        addr = vaddr[0];
    return fRet;
}

bool GetMyExternalIP2(const Endpoint& addrConnect, const char* pszGet, const char* pszKeyword, unsigned int& ipRet)
{
    SOCKET hSocket;
    if (!ConnectSocket(addrConnect, hSocket))
        return error("GetMyExternalIP() : connection to %s failed", addrConnect.toString().c_str());

    send(hSocket, pszGet, strlen(pszGet), MSG_NOSIGNAL);

    string strLine;
    while (RecvLine(hSocket, strLine))
    {
        if (strLine.empty()) // HTTP response is separated from headers by blank line
        {
            loop
            {
                if (!RecvLine(hSocket, strLine))
                {
                    closesocket(hSocket);
                    return false;
                }
                if (pszKeyword == NULL)
                    break;
                if (strLine.find(pszKeyword) != -1)
                {
                    strLine = strLine.substr(strLine.find(pszKeyword) + strlen(pszKeyword));
                    break;
                }
            }
            closesocket(hSocket);
            if (strLine.find("<") != -1)
                strLine = strLine.substr(0, strLine.find("<"));
            strLine = strLine.substr(strspn(strLine.c_str(), " \t\n\r"));
            while (strLine.size() > 0 && isspace(strLine[strLine.size()-1]))
                strLine.resize(strLine.size()-1);
            Endpoint addr(strLine,0,true);
            printf("GetMyExternalIP() received [%s] %s\n", strLine.c_str(), addr.toString().c_str());
            if (addr.getIP() == 0 || addr.getIP() == INADDR_NONE || !addr.isRoutable())
                return false;
            ipRet = addr.getIP();
            return true;
        }
    }
    closesocket(hSocket);
    return error("GetMyExternalIP() : connection closed");
}

// We now get our external IP from the IRC server first and only use this as a backup
bool GetMyExternalIP(unsigned int& ipRet)
{
    Endpoint addrConnect;
    const char* pszGet;
    const char* pszKeyword;

    if (fUseProxy)
        return false;

    for (int nLookup = 0; nLookup <= 1; nLookup++)
    for (int nHost = 1; nHost <= 2; nHost++)
    {
        // We should be phasing out our use of sites like these.  If we need
        // replacements, we should ask for volunteers to put this simple
        // php file on their webserver that prints the client IP:
        //  <?php echo $_SERVER["REMOTE_ADDR"]; ?>
        if (nHost == 1)
        {
            addrConnect = Endpoint("91.198.22.70",80); // checkip.dyndns.org

            if (nLookup == 1)
            {
                Endpoint addrIP("checkip.dyndns.org", 80, true);
                if (addrIP.isValid())
                    addrConnect = addrIP;
            }

            pszGet = "GET / HTTP/1.1\r\n"
                     "Host: checkip.dyndns.org\r\n"
                     "User-Agent: Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1)\r\n"
                     "Connection: close\r\n"
                     "\r\n";

            pszKeyword = "Address:";
        }
        else if (nHost == 2)
        {
            addrConnect = Endpoint("74.208.43.192", 80); // www.showmyip.com

            if (nLookup == 1)
            {
                Endpoint addrIP("www.showmyip.com", 80, true);
                if (addrIP.isValid())
                    addrConnect = addrIP;
            }

            pszGet = "GET /simple/ HTTP/1.1\r\n"
                     "Host: www.showmyip.com\r\n"
                     "User-Agent: Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1)\r\n"
                     "Connection: close\r\n"
                     "\r\n";

            pszKeyword = NULL; // Returns just IP address
        }

        if (GetMyExternalIP2(addrConnect, pszGet, pszKeyword, ipRet))
            return true;
    }

    return false;
}

void ThreadGetMyExternalIP(void* parg)
{
    // Wait for IRC to get it first
    if (!GetBoolArg("-noirc"))
    {
        for (int i = 0; i < 2 * 60; i++)
        {
            Sleep(1000);
            if (fGotExternalIP || fShutdown)
                return;
        }
    }

    // Fallback in case IRC fails to get it
    unsigned int ip;
    if (GetMyExternalIP(ip)) {
        __localhost.setIP(ip);
        printf("GetMyExternalIP() returned %s\n", __localhost.toStringIP().c_str());
        if (__localhost.isRoutable())
        {
            // If we already connected to a few before we had our IP, go back and addr them.
            // setAddrKnown automatically filters any duplicate sends.
            Endpoint addr(__localhost);
            addr.setTime(GetAdjustedTime());
            CRITICAL_BLOCK(cs_vNodes)
                BOOST_FOREACH(CNode* pnode, vNodes)
                    pnode->PushAddress(addr);
        }
    }
}

CNode* FindNode(unsigned int ip)
{
    CRITICAL_BLOCK(cs_vNodes)
    {
        BOOST_FOREACH(CNode* pnode, vNodes)
            if (pnode->addr.getIP() == ip)
                return (pnode);
    }
    return NULL;
}

CNode* FindNode(Endpoint addr)
{
    CRITICAL_BLOCK(cs_vNodes)
    {
        BOOST_FOREACH(CNode* pnode, vNodes)
            if (pnode->addr == addr)
                return (pnode);
    }
    return NULL;
}

CNode* ConnectNode(Endpoint addrConnect, int64 nTimeout)
{
    if (addrConnect.getIP() == __localhost.getIP())
        return NULL;

    // Look for an existing connection
    CNode* pnode = FindNode(addrConnect.getIP());
    if (pnode)
    {
        if (nTimeout != 0)
            pnode->AddRef(nTimeout);
        else
            pnode->AddRef();
        return pnode;
    }

    /// debug print
    printf("trying connection %s lastseen=%.1fhrs lasttry=%.1fhrs\n",
        addrConnect.toString().c_str(),
        (double)(addrConnect.getTime() - GetAdjustedTime())/3600.0,
        (double)(addrConnect.getLastTry() - GetAdjustedTime())/3600.0);

    _endpointPool->getEndpoint(addrConnect.getKey()).setLastTry(GetAdjustedTime());
    
    // Connect
    SOCKET hSocket;
    if (ConnectSocket(addrConnect, hSocket))
    {
        /// debug print
        printf("connected %s\n", addrConnect.toString().c_str());

        // Set to nonblocking
#ifdef _WIN32
        u_long nOne = 1;
        if (ioctlsocket(hSocket, FIONBIO, &nOne) == SOCKET_ERROR)
            printf("ConnectSocket() : ioctlsocket nonblocking setting failed, error %d\n", WSAGetLastError());
#else
        if (fcntl(hSocket, F_SETFL, O_NONBLOCK) == SOCKET_ERROR)
            printf("ConnectSocket() : fcntl nonblocking setting failed, error %d\n", errno);
#endif

        // Add node
        CNode* pnode = new CNode(hSocket, addrConnect, false);
        if (nTimeout != 0)
            pnode->AddRef(nTimeout);
        else
            pnode->AddRef();
        CRITICAL_BLOCK(cs_vNodes)
            vNodes.push_back(pnode);

        pnode->nTimeConnected = GetTime();
        return pnode;
    }
    else
    {
        return NULL;
    }
}

void CNode::CloseSocketDisconnect()
{
    fDisconnect = true;
    if (hSocket != INVALID_SOCKET)
    {
        if (fDebug)
            printf("%s ", DateTimeStrFormat("%x %H:%M:%S", GetTime()).c_str());
        printf("disconnecting node %s\n", addr.toString().c_str());
        closesocket(hSocket);
        hSocket = INVALID_SOCKET;
    }
}

void ThreadSocketHandler(void* parg)
{
    IMPLEMENT_RANDOMIZE_STACK(ThreadSocketHandler(parg));
    try
    {
        vnThreadsRunning[0]++;
        ThreadSocketHandler2(parg);
        vnThreadsRunning[0]--;
    }
    catch (std::exception& e) {
        vnThreadsRunning[0]--;
        PrintException(&e, "ThreadSocketHandler()");
    } catch (...) {
        vnThreadsRunning[0]--;
        throw; // support pthread_cancel()
    }
    printf("ThreadSocketHandler exiting\n");
}

void ThreadSocketHandler2(void* parg)
{
    printf("ThreadSocketHandler started\n");
    list<CNode*> vNodesDisconnected;
    int nPrevNodeCount = 0;

    loop
    {
        //
        // Disconnect nodes
        //
        CRITICAL_BLOCK(cs_vNodes)
        {
            // Disconnect unused nodes
            vector<CNode*> vNodesCopy = vNodes;
            BOOST_FOREACH(CNode* pnode, vNodesCopy)
            {
                if (pnode->fDisconnect ||
                    (pnode->GetRefCount() <= 0 && pnode->vRecv.empty() && pnode->vSend.empty()))
                {
                    // remove from vNodes
                    vNodes.erase(remove(vNodes.begin(), vNodes.end(), pnode), vNodes.end());

                    // close socket and cleanup
                    pnode->CloseSocketDisconnect();
                //                    pnode->Cleanup();

                    // hold in disconnected pool until all refs are released
                    pnode->nReleaseTime = max(pnode->nReleaseTime, GetTime() + 15 * 60);
                    if (pnode->fNetworkNode || pnode->fInbound)
                        pnode->Release();
                    vNodesDisconnected.push_back(pnode);
                }
            }

            // Delete disconnected nodes
            list<CNode*> vNodesDisconnectedCopy = vNodesDisconnected;
            BOOST_FOREACH(CNode* pnode, vNodesDisconnectedCopy)
            {
                // wait until threads are done using it
                if (pnode->GetRefCount() <= 0)
                {
                    bool fDelete = false;
                    TRY_CRITICAL_BLOCK(pnode->cs_vSend)
                     TRY_CRITICAL_BLOCK(pnode->cs_vRecv)
                      TRY_CRITICAL_BLOCK(pnode->cs_mapRequests)
                       TRY_CRITICAL_BLOCK(pnode->cs_inventory)
                        fDelete = true;
                    if (fDelete)
                    {
                        vNodesDisconnected.remove(pnode);
                        delete pnode;
                    }
                }
            }
        }
        if (vNodes.size() != nPrevNodeCount)
        {
            nPrevNodeCount = vNodes.size();
            MainFrameRepaint();
        }


        //
        // Find which sockets have data to receive
        //
        struct timeval timeout;
        timeout.tv_sec  = 0;
        timeout.tv_usec = 50000; // frequency to poll pnode->vSend

        fd_set fdsetRecv;
        fd_set fdsetSend;
        fd_set fdsetError;
        FD_ZERO(&fdsetRecv);
        FD_ZERO(&fdsetSend);
        FD_ZERO(&fdsetError);
        SOCKET hSocketMax = 0;

        if(hListenSocket != INVALID_SOCKET)
            FD_SET(hListenSocket, &fdsetRecv);
        hSocketMax = max(hSocketMax, hListenSocket);
        CRITICAL_BLOCK(cs_vNodes)
        {
            BOOST_FOREACH(CNode* pnode, vNodes)
            {
                if (pnode->hSocket == INVALID_SOCKET)
                    continue;
                FD_SET(pnode->hSocket, &fdsetRecv);
                FD_SET(pnode->hSocket, &fdsetError);
                hSocketMax = max(hSocketMax, pnode->hSocket);
                TRY_CRITICAL_BLOCK(pnode->cs_vSend)
                    if (!pnode->vSend.empty())
                        FD_SET(pnode->hSocket, &fdsetSend);
            }
        }

        vnThreadsRunning[0]--;
        int nSelect = select(hSocketMax + 1, &fdsetRecv, &fdsetSend, &fdsetError, &timeout);
        vnThreadsRunning[0]++;
        if (fShutdown)
            return;
        if (nSelect == SOCKET_ERROR)
        {
            int nErr = WSAGetLastError();
            if (hSocketMax > -1)
            {
                printf("socket select error %d\n", nErr);
                for (int i = 0; i <= hSocketMax; i++)
                    FD_SET(i, &fdsetRecv);
            }
            FD_ZERO(&fdsetSend);
            FD_ZERO(&fdsetError);
            Sleep(timeout.tv_usec/1000);
        }


        //
        // Accept new connections
        //
        if (hListenSocket != INVALID_SOCKET && FD_ISSET(hListenSocket, &fdsetRecv))
        {
            struct sockaddr_in sockaddr;
            socklen_t len = sizeof(sockaddr);
            SOCKET hSocket = accept(hListenSocket, (struct sockaddr*)&sockaddr, &len);
            Endpoint addr(sockaddr);
            int nInbound = 0;

            CRITICAL_BLOCK(cs_vNodes)
                BOOST_FOREACH(CNode* pnode, vNodes)
                if (pnode->fInbound)
                    nInbound++;
            if (hSocket == INVALID_SOCKET)
            {
                if (WSAGetLastError() != WSAEWOULDBLOCK)
                    printf("socket error accept failed: %d\n", WSAGetLastError());
            }
            else if (nInbound >= GetArg("-maxconnections", 125) - MAX_OUTBOUND_CONNECTIONS)
            {
                closesocket(hSocket);
            }
            else
            {
                printf("accepted connection %s\n", addr.toString().c_str());
                CNode* pnode = new CNode(hSocket, addr, true);
                pnode->AddRef();
                CRITICAL_BLOCK(cs_vNodes)
                    vNodes.push_back(pnode);
            }
        }


        //
        // Service each socket
        //
        vector<CNode*> vNodesCopy;
        CRITICAL_BLOCK(cs_vNodes)
        {
            vNodesCopy = vNodes;
            BOOST_FOREACH(CNode* pnode, vNodesCopy)
                pnode->AddRef();
        }
        BOOST_FOREACH(CNode* pnode, vNodesCopy)
        {
            if (fShutdown)
                return;

            //
            // Receive
            //
            if (pnode->hSocket == INVALID_SOCKET)
                continue;
            if (FD_ISSET(pnode->hSocket, &fdsetRecv) || FD_ISSET(pnode->hSocket, &fdsetError))
            {
                TRY_CRITICAL_BLOCK(pnode->cs_vRecv)
                {
                    CDataStream& vRecv = pnode->vRecv;
                    unsigned int nPos = vRecv.size();

                    if (nPos > ReceiveBufferSize()) {
                        if (!pnode->fDisconnect)
                            printf("socket recv flood control disconnect (%d bytes)\n", vRecv.size());
                        pnode->CloseSocketDisconnect();
                    }
                    else {
                        // typical socket buffer is 8K-64K
                        char pchBuf[0x10000];
                        int nBytes = recv(pnode->hSocket, pchBuf, sizeof(pchBuf), MSG_DONTWAIT);
                        if (nBytes > 0)
                        {
                            vRecv.resize(nPos + nBytes);
                            memcpy(&vRecv[nPos], pchBuf, nBytes);
                            pnode->nLastRecv = GetTime();
                        }
                        else if (nBytes == 0)
                        {
                            // socket closed gracefully
                            if (!pnode->fDisconnect)
                                printf("socket closed\n");
                            pnode->CloseSocketDisconnect();
                        }
                        else if (nBytes < 0)
                        {
                            // error
                            int nErr = WSAGetLastError();
                            if (nErr != WSAEWOULDBLOCK && nErr != WSAEMSGSIZE && nErr != WSAEINTR && nErr != WSAEINPROGRESS)
                            {
                                if (!pnode->fDisconnect)
                                    printf("socket recv error %d\n", nErr);
                                pnode->CloseSocketDisconnect();
                            }
                        }
                    }
                }
            }

            //
            // Send
            //
            if (pnode->hSocket == INVALID_SOCKET)
                continue;
            if (FD_ISSET(pnode->hSocket, &fdsetSend))
            {
                TRY_CRITICAL_BLOCK(pnode->cs_vSend)
                {
                    CDataStream& vSend = pnode->vSend;
                    if (!vSend.empty())
                    {
                        int nBytes = send(pnode->hSocket, &vSend[0], vSend.size(), MSG_NOSIGNAL | MSG_DONTWAIT);
                        if (nBytes > 0)
                        {
                            vSend.erase(vSend.begin(), vSend.begin() + nBytes);
                            pnode->nLastSend = GetTime();
                        }
                        else if (nBytes < 0)
                        {
                            // error
                            int nErr = WSAGetLastError();
                            if (nErr != WSAEWOULDBLOCK && nErr != WSAEMSGSIZE && nErr != WSAEINTR && nErr != WSAEINPROGRESS)
                            {
                                printf("socket send error %d\n", nErr);
                                pnode->CloseSocketDisconnect();
                            }
                        }
                        if (vSend.size() > SendBufferSize()) {
                            if (!pnode->fDisconnect)
                                printf("socket send flood control disconnect (%d bytes)\n", vSend.size());
                            pnode->CloseSocketDisconnect();
                        }
                    }
                }
            }

            //
            // Inactivity checking
            //
            if (pnode->vSend.empty())
                pnode->nLastSendEmpty = GetTime();
            if (GetTime() - pnode->nTimeConnected > 60)
            {
                if (pnode->nLastRecv == 0 || pnode->nLastSend == 0)
                {
                    printf("socket no message in first 60 seconds, %d %d\n", pnode->nLastRecv != 0, pnode->nLastSend != 0);
                    pnode->fDisconnect = true;
                }
                else if (GetTime() - pnode->nLastSend > 90*60 && GetTime() - pnode->nLastSendEmpty > 90*60)
                {
                    printf("socket not sending\n");
                    pnode->fDisconnect = true;
                }
                else if (GetTime() - pnode->nLastRecv > 90*60)
                {
                    printf("socket inactivity timeout\n");
                    pnode->fDisconnect = true;
                }
            }
        }
        CRITICAL_BLOCK(cs_vNodes)
        {
            BOOST_FOREACH(CNode* pnode, vNodesCopy)
                pnode->Release();
        }

        Sleep(10);
    }
}









#ifdef USE_UPNP
void ThreadMapPort(void* parg)
{
    IMPLEMENT_RANDOMIZE_STACK(ThreadMapPort(parg));
    try
    {
        vnThreadsRunning[5]++;
        ThreadMapPort2(parg);
        vnThreadsRunning[5]--;
    }
    catch (std::exception& e) {
        vnThreadsRunning[5]--;
        PrintException(&e, "ThreadMapPort()");
    } catch (...) {
        vnThreadsRunning[5]--;
        PrintException(NULL, "ThreadMapPort()");
    }
    printf("ThreadMapPort exiting\n");
}

void ThreadMapPort2(void* parg)
{
    printf("ThreadMapPort started\n");

    char port[6];
    sprintf(port, "%d", GetListenPort());

    const char * rootdescurl = 0;
    const char * multicastif = 0;
    const char * minissdpdpath = 0;
    int error = 0;
    struct UPNPDev * devlist = 0;
    char lanaddr[64];

    devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0, 0, &error);

    struct UPNPUrls urls;
    struct IGDdatas data;
    int r;

    r = UPNP_GetValidIGD(devlist, &urls, &data, lanaddr, sizeof(lanaddr));
    if (r == 1)
    {
        char intClient[16];
        char intPort[6];
        string strDesc = "Bitcoin " + FormatFullVersion();
        r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
	                        port, port, lanaddr, strDesc.c_str(), "TCP", 0, "0");

        if(r!=UPNPCOMMAND_SUCCESS)
            printf("AddPortMapping(%s, %s, %s) failed with code %d (%s)\n",
                port, port, lanaddr, r, strupnperror(r));
        else
            printf("UPnP Port Mapping successful.\n");
        loop {
            if (fShutdown || !fUseUPnP)
            {
                r = UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, port, "TCP", 0);
                printf("UPNP_DeletePortMapping() returned : %d\n", r);
                freeUPNPDevlist(devlist); devlist = 0;
                FreeUPNPUrls(&urls);
                return;
            }
            Sleep(2000);
        }
    } else {
        printf("No valid UPnP IGDs found\n");
        freeUPNPDevlist(devlist); devlist = 0;
        if (r != 0)
            FreeUPNPUrls(&urls);
        loop {
            if (fShutdown || !fUseUPnP)
                return;
            Sleep(2000);
        }
    }
}

void MapPort(bool fMapPort)
{
    if (fUseUPnP != fMapPort)
    {
        fUseUPnP = fMapPort;
        WriteSetting("fUseUPnP", fUseUPnP);
    }
    if (fUseUPnP && vnThreadsRunning[5] < 1)
    {
        if (!CreateThread(ThreadMapPort, NULL))
            printf("Error: ThreadMapPort(ThreadMapPort) failed\n");
    }
}
#else
void MapPort(bool /* unused fMapPort */)
{
    // Intentionally left blank.
}
#endif










static const char *strDNSSeed[] = {
    "bitseed.xf2.org",
    "bitseed.bitcoin.org.uk",
    "dnsseed.bluematt.me",
};

void DNSAddressSeed()
{
    int found = 0;

    if (!fTestNet)
    {
        printf("Loading addresses from DNS seeds (could take a while)\n");

        for (int seed_idx = 0; seed_idx < ARRAYLEN(strDNSSeed); seed_idx++) {
            vector<Endpoint> vaddr;
            if (Lookup(strDNSSeed[seed_idx], vaddr, NODE_NETWORK, -1, true))
            {
                BOOST_FOREACH (Endpoint& addr, vaddr)
                {
                    if (addr.getByte(3) != 127)
                    {
                        addr.setTime(0);
                        _endpointPool->addEndpoint(addr, 0);
                        found++;
                    }
                }
            }
        }

    }

    printf("%d addresses found from DNS seeds\n", found);
}



unsigned int pnSeed[] =
{
    0x6884ac63, 0x3ffecead, 0x2919b953, 0x0942fe50, 0x7a1d922e, 0xcdd6734a, 0x953a5bb6, 0x2c46922e,
    0xe2a5f143, 0xaa39103a, 0xa06afa5c, 0x135ffd59, 0xe8e82863, 0xf61ef029, 0xf75f042e, 0x2b363532,
    0x29b2df42, 0x16b1f64e, 0xd46e281b, 0x5280bf58, 0x60372229, 0x1be58e4f, 0xa8496f45, 0x1fb1a057,
    0x756b3844, 0x3bb79445, 0x0b375518, 0xcccb0102, 0xb682bf2e, 0x46431c02, 0x3a81073a, 0xa3771f1f,
    0x213a121f, 0x85dc2c1b, 0x56b4323b, 0xb34e8945, 0x3c40b33d, 0xfa276418, 0x1f818d29, 0xebe1e344,
    0xf6160a18, 0xf4fa384a, 0x34b09558, 0xb882b543, 0xe3ce2253, 0x6abf56d8, 0xe91b1155, 0x688ee6ad,
    0x2efc6058, 0x4792cd47, 0x0c32f757, 0x4c813a46, 0x8c93644a, 0x37507444, 0x813ad218, 0xdac06d4a,
    0xe4c63e4b, 0x21a1ea3c, 0x8d88556f, 0x30e9173a, 0x041f681b, 0xdc77ba50, 0xc0072753, 0xceddd44f,
    0x052d1743, 0xe3c77a4a, 0x13981c3a, 0x5685d918, 0x3c0e4e70, 0x3e56fb54, 0xb676ae0c, 0xac93c859,
    0x22279f43, 0x975a4542, 0xe527f071, 0xea162f2e, 0x3c65a32e, 0x5be5713b, 0x961ec418, 0xb202922e,
    0x5ef7be50, 0xce49f53e, 0x05803b47, 0x8463b055, 0x78576153, 0x3ec2ae3a, 0x4bbd7118, 0xafcee043,
    0x56a3e8ba, 0x6174de4d, 0x8d01ba4b, 0xc9af564e, 0xdbc9c547, 0xa627474d, 0xdada9244, 0xd3b3083a,
    0x523e071f, 0xd6b96f18, 0xbd527c46, 0xdf2bbb4d, 0xd37b4a4b, 0x3a6a2158, 0xc064b055, 0x18a8e055,
    0xec4dae3b, 0x0540416c, 0x475b4fbe, 0x064803b2, 0x48e9f062, 0x2898524b, 0xd315ff43, 0xf786d247,
    0xc7ea2f3e, 0xc087f043, 0xc163354b, 0x8250284d, 0xed300029, 0xbf36e05c, 0x8eb3ae4c, 0xe7aa623e,
    0x7ced0274, 0xdd362c1b, 0x362b995a, 0xca26b629, 0x3fc41618, 0xb97b364e, 0xa05b8729, 0x0f5e3c43,
    0xdf942618, 0x6aeb9b5b, 0xbf04762e, 0xfaaeb118, 0x87579958, 0x76520044, 0xc2660c5b, 0x628b201b,
    0xf193932e, 0x1c0ad045, 0xff908346, 0x8da9d4da, 0xed201c1f, 0xa47a2b1b, 0x330007d4, 0x8ba1ed47,
    0xb2f02d44, 0x7db62c1b, 0x781c454b, 0xc0300029, 0xb7062a45, 0x88b52e3a, 0x78dd6b63, 0x1cb9b718,
    0x5d358e47, 0x59912c3b, 0x79607544, 0x5197f759, 0xc023be48, 0xd1013743, 0x0f354057, 0x8e3aac3b,
    0x4114693e, 0x22316318, 0xe27dda50, 0x878eac3b, 0x4948a21f, 0x5db7f24c, 0x8ccb6157, 0x26a5de18,
    0x0a11bd43, 0x27bb1e41, 0x60a7a951, 0x3e16b35e, 0x07888b53, 0x5648a853, 0x0149fe50, 0xd070a34f,
    0x6454c96d, 0xd6e54758, 0xa96dc152, 0x65447861, 0xf6bdf95e, 0x10400202, 0x2c29d483, 0x18174732,
    0x1d840618, 0x12e61818, 0x089d3f3c, 0x917e931f, 0xd1b0c90e, 0x25bd3c42, 0xeb05775b, 0x7d550c59,
    0x6cfacb01, 0xe4224444, 0xa41dd943, 0x0f5aa643, 0x5e33731b, 0x81036d50, 0x6f46a0d1, 0x7731be43,
    0x14840e18, 0xf1e8d059, 0x661d2b1f, 0x40a3201b, 0x9407b843, 0xedf0254d, 0x7bd1a5bc, 0x073dbe51,
    0xe864a97b, 0x2efd947b, 0xb9ca0e45, 0x4e2113ad, 0xcc305731, 0xd39ca63c, 0x733df918, 0xda172b1f,
    0xaa03b34d, 0x7230fd4d, 0xf1ce6e3a, 0x2e9fab43, 0xa4010750, 0xa928bd18, 0x6809be42, 0xb19de348,
    0xff956270, 0x0d795f51, 0xd2dec247, 0x6df5774b, 0xbac11f79, 0xdfb05c75, 0x887683d8, 0xa1e83632,
    0x2c0f7671, 0x28bcb65d, 0xac2a7545, 0x3eebfc60, 0x304ad7c4, 0xa215a462, 0xc86f0f58, 0xcfb92ebe,
    0x5e23ed82, 0xf506184b, 0xec0f19b7, 0x060c59ad, 0x86ee3174, 0x85380774, 0xa199a562, 0x02b507ae,
    0x33eb2163, 0xf2112b1f, 0xb702ba50, 0x131b9618, 0x90ccd04a, 0x08f3273b, 0xecb61718, 0x64b8b44d,
    0x182bf4dc, 0xc7b68286, 0x6e318d5f, 0xfdb03654, 0xb3272e54, 0xe014ad4b, 0x274e4a31, 0x7806375c,
    0xbc34a748, 0x1b5ad94a, 0x6b54d10e, 0x73e2ae6e, 0x5529d483, 0x8455a76d, 0x99c13f47, 0x1d811741,
    0xa9782a78, 0x0b00464d, 0x7266ea50, 0x532dab46, 0x33e1413e, 0x780d0c18, 0x0fb0854e, 0x03370155,
    0x2693042e, 0xfa3d824a, 0x2bb1681b, 0x37ea2a18, 0x7fb8414b, 0x32e0713b, 0xacf38d3f, 0xa282716f,
    0xb1a09d7b, 0xa04b764b, 0x83c94d18, 0x05ee4c6d, 0x0e795f51, 0x46984352, 0xf80fc247, 0x3fccb946,
    0xd7ae244b, 0x0a8e0a4c, 0x57b141bc, 0x3647bed1, 0x1431b052, 0x803a8bbb, 0xfc69056b, 0xf5991862,
    0x14963b2e, 0xd35d5dda, 0xc6c73574, 0xc8f1405b, 0x0ca4224d, 0xecd36071, 0xa9461754, 0xe7a0ed72,
    0x559e8346, 0x1c9beec1, 0xc786ea4a, 0x9561b44d, 0x9788074d, 0x1a69934f, 0x23c5614c, 0x07c79d4b,
    0xc7ee52db, 0xc72df351, 0xcb135e44, 0xa0988346, 0xc211fc4c, 0x87dec34b, 0x1381074d, 0x04a65cb7,
    0x4409083a, 0x4a407a4c, 0x92b8d37d, 0xacf50b4d, 0xa58aa5bc, 0x448f801f, 0x9c83762e, 0x6fd5734a,
    0xfe2d454b, 0x84144c55, 0x05190e4c, 0xb2151448, 0x63867a3e, 0x16099018, 0x9c010d3c, 0x962d8f3d,
    0xd51ee453, 0x9d86801f, 0x68e87b47, 0x6bf7bb73, 0x5fc7910e, 0x10d90118, 0x3db04442, 0x729d3e4b,
    0xc397d842, 0x57bb15ad, 0x72f31f4e, 0xc9380043, 0x2bb24e18, 0xd9b8ab50, 0xb786801f, 0xf4dc4847,
    0x85f4bb51, 0x4435995b, 0x5ba07e40, 0x2c57392e, 0x3628124b, 0x9839b64b, 0x6fe8b24d, 0xaddce847,
    0x75260e45, 0x0c572a43, 0xfea21902, 0xb9f9742e, 0x5a70d443, 0x8fc5910e, 0x868d4744, 0x56245e02,
    0xd7eb5f02, 0x35c12c1b, 0x4373034b, 0x8786554c, 0xa6facf18, 0x4b11a31f, 0x3570664e, 0x5a64bc42,
    0x0b03983f, 0x8f457e4c, 0x0fd874c3, 0xb6cf31b2, 0x2bbc2d4e, 0x146ca5b2, 0x9d00b150, 0x048a4153,
    0xca4dcd43, 0xc1607cca, 0x8234cf57, 0x9c7daead, 0x3dc07658, 0xea5c6e4c, 0xf1a0084e, 0x16d2ee53,
    0x1b849418, 0xfe913a47, 0x1e988f62, 0x208b644c, 0xc55ee980, 0xbdbce747, 0xf59a384e, 0x0f56091b,
    0x7417b745, 0x0c37344e, 0x2c62ab47, 0xf8533a4d, 0x8030084d, 0x76b93c4b, 0xda6ea0ad, 0x3c54f618,
    0x63b0de1f, 0x7370d858, 0x1a70bb4c, 0xdda63b2e, 0x60b2ba50, 0x1ba7d048, 0xbe1b2c1b, 0xabea5747,
    0x29ad2e4d, 0xe8cd7642, 0x66c80e18, 0x138bf34a, 0xc6145e44, 0x2586794c, 0x07bc5478, 0x0da0b14d,
    0x8f95354e, 0x9eb11c62, 0xa1545e46, 0x2e7a2602, 0x408c9c3d, 0x59065d55, 0xf51d1a4c, 0x3bbc6a4e,
    0xc71b2a2e, 0xcdaaa545, 0x17d659d0, 0x5202e7ad, 0xf1b68445, 0x93375961, 0xbd88a043, 0x066ad655,
    0x890f6318, 0x7b7dca47, 0x99bdd662, 0x3bb4fc53, 0x1231efdc, 0xc0a99444, 0x96bbea47, 0x61ed8748,
    0x27dfa73b, 0x8d4d1754, 0x3460042e, 0x551f0c4c, 0x8d0e0718, 0x162ddc53, 0x53231718, 0x1ecd65d0,
    0x944d28bc, 0x3b79d058, 0xaff97fbc, 0x4860006c, 0xc101c90e, 0xace41743, 0xa5975d4c, 0x5cc2703e,
    0xb55a4450, 0x02d18840, 0xee2765ae, 0xd6012fd5, 0x24c94d7d, 0x8c6eec47, 0x7520ba5d, 0x9e15e460,
    0x8510b04c, 0x75ec3847, 0x1dfa6661, 0xe172b3ad, 0x5744c90e, 0x52a0a152, 0x8d6fad18, 0x67b74b6d,
    0x93a089b2, 0x0f3ac5d5, 0xe5de1855, 0x43d25747, 0x4bad804a, 0x55b408d8, 0x60a36441, 0xf553e860,
    0xdb2fa2c8, 0x03152b32, 0xdd27a7d5, 0x3116a8b8, 0x0a1d708c, 0xeee2f13c, 0x6acf436f, 0xce6eb4ca,
    0x101cd3d9, 0x1c48a6b8, 0xe57d6f44, 0x93dcf562,
};



void ThreadOpenConnections(void* parg)
{
    IMPLEMENT_RANDOMIZE_STACK(ThreadOpenConnections(parg));
    try
    {
        vnThreadsRunning[1]++;
        ThreadOpenConnections2(parg);
        vnThreadsRunning[1]--;
    }
    catch (std::exception& e) {
        vnThreadsRunning[1]--;
        PrintException(&e, "ThreadOpenConnections()");
    } catch (...) {
        vnThreadsRunning[1]--;
        PrintException(NULL, "ThreadOpenConnections()");
    }
    printf("ThreadOpenConnections exiting\n");
}

void ThreadOpenConnections2(void* parg)
{
    printf("ThreadOpenConnections started\n");

    // Connect to specific addresses
    if (mapArgs.count("-connect"))
    {
        for (int64 nLoop = 0;; nLoop++)
        {
            BOOST_FOREACH(string strAddr, mapMultiArgs["-connect"])
            {
                Endpoint addr(strAddr, fAllowDNS);
                if (addr.isValid())
                    OpenNetworkConnection(addr);
                for (int i = 0; i < 10 && i < nLoop; i++)
                {
                    Sleep(500);
                    if (fShutdown)
                        return;
                }
            }
        }
    }

    // Connect to manually added nodes first
    if (mapArgs.count("-addnode"))
    {
        BOOST_FOREACH(string strAddr, mapMultiArgs["-addnode"])
        {
            Endpoint addr(strAddr, fAllowDNS);
            if (addr.isValid())
            {
                OpenNetworkConnection(addr);
                Sleep(500);
                if (fShutdown)
                    return;
            }
        }
    }

    // Initiate network connections
    int64 nStart = GetTime();
    loop
    {
        // Limit outbound connections
        vnThreadsRunning[1]--;
        Sleep(500);
        loop
        {
            int nOutbound = 0;
            CRITICAL_BLOCK(cs_vNodes)
                BOOST_FOREACH(CNode* pnode, vNodes)
                    if (!pnode->fInbound)
                        nOutbound++;
            int nMaxOutboundConnections = MAX_OUTBOUND_CONNECTIONS;
            nMaxOutboundConnections = min(nMaxOutboundConnections, (int)GetArg("-maxconnections", 125));
            if (nOutbound < nMaxOutboundConnections)
                break;
            Sleep(2000);
            if (fShutdown)
                return;
        }
        vnThreadsRunning[1]++;
        if (fShutdown)
            return;

        CRITICAL_BLOCK(cs_mapAddresses)
        {
            // Add seed nodes if IRC isn't working
            bool fTOR = (fUseProxy && addrProxy.getPort() == htons(9050));
            if ((_endpointPool->getPoolSize() == 0) && (GetTime() - nStart > 60 || fTOR) && !fTestNet)
            {
                for (int i = 0; i < ARRAYLEN(pnSeed); i++)
                {
                    // It'll only connect to one or two seed nodes because once it connects,
                    // it'll get a pile of addresses with newer timestamps.
                    // Seed nodes are given a random 'last seen time' of between one and two
                    // weeks ago.
                    const int64 nOneWeek = 7*24*60*60;
                    Endpoint addr;
                    addr.setIP(pnSeed[i]);
                    addr.setTime(GetTime()-GetRand(nOneWeek)-nOneWeek);
                    _endpointPool->addEndpoint(addr);
                }
            }
        }


    
    // Only connect to one address per a.b.?.? range.
    // Do this here so we don't have to critsect vNodes inside mapAddresses critsect.
    set<unsigned int> setConnected;
    CRITICAL_BLOCK(cs_vNodes)
    BOOST_FOREACH(CNode* pnode, vNodes)
    setConnected.insert(pnode->addr.getIP() & 0x0000ffff);
    
    Endpoint addrConnect = _endpointPool->getCandidate(setConnected, nStart);
        if (addrConnect.isValid())
            OpenNetworkConnection(addrConnect);
    }
}

bool OpenNetworkConnection(const Endpoint& addrConnect)
{
    //
    // Initiate outbound network connection
    //
    if (fShutdown)
        return false;
    if (addrConnect.getIP() == __localhost.getIP() || !addrConnect.isIPv4() || FindNode(addrConnect.getIP()))
        return false;

    vnThreadsRunning[1]--;
    CNode* pnode = ConnectNode(addrConnect);
    vnThreadsRunning[1]++;
    if (fShutdown)
        return false;
    if (!pnode)
        return false;
    pnode->fNetworkNode = true;

    return true;
}








void ThreadMessageHandler(void* parg)
{
    IMPLEMENT_RANDOMIZE_STACK(ThreadMessageHandler(parg));
    try
    {
        vnThreadsRunning[2]++;
        ThreadMessageHandler2(parg);
        vnThreadsRunning[2]--;
    }
    catch (std::exception& e) {
        vnThreadsRunning[2]--;
        PrintException(&e, "ThreadMessageHandler()");
    } catch (...) {
        vnThreadsRunning[2]--;
        PrintException(NULL, "ThreadMessageHandler()");
    }
    printf("ThreadMessageHandler exiting\n");
}

void ThreadMessageHandler2(void* parg)
{
    printf("ThreadMessageHandler started\n");
    SetThreadPriority(THREAD_PRIORITY_BELOW_NORMAL);
    while (!fShutdown)
    {
        vector<CNode*> vNodesCopy;
        CRITICAL_BLOCK(cs_vNodes)
        {
            vNodesCopy = vNodes;
            BOOST_FOREACH(CNode* pnode, vNodesCopy)
                pnode->AddRef();
        }

        // Poll the connected nodes for messages
        CNode* pnodeTrickle = NULL;
        if (!vNodesCopy.empty())
            pnodeTrickle = vNodesCopy[GetRand(vNodesCopy.size())];
        BOOST_FOREACH(CNode* pnode, vNodesCopy)
        {
            // Receive messages
            TRY_CRITICAL_BLOCK(pnode->cs_vRecv)
                pnode->ProcessMessages();
            if (fShutdown)
                return;

            // Send messages
            TRY_CRITICAL_BLOCK(pnode->cs_vSend)
                pnode->SendMessages(pnode == pnodeTrickle);
            if (fShutdown)
                return;
        }

        CRITICAL_BLOCK(cs_vNodes)
        {
            BOOST_FOREACH(CNode* pnode, vNodesCopy)
                pnode->Release();
        }

        // Wait and allow messages to bunch up.
        // Reduce vnThreadsRunning so StopNode has permission to exit while
        // we're sleeping, but we must always check fShutdown after doing this.
        vnThreadsRunning[2]--;
        Sleep(100);
        if (fRequestShutdown)
            Shutdown(NULL);
        vnThreadsRunning[2]++;
        if (fShutdown)
            return;
    }
}






bool BindListenPort(string& strError)
{
    strError = "";
    int nOne = 1;
    __localhost.setPort(htons(GetListenPort()));

#ifdef _WIN32
    // Initialize Windows Sockets
    WSADATA wsadata;
    int ret = WSAStartup(MAKEWORD(2,2), &wsadata);
    if (ret != NO_ERROR)
    {
        strError = strprintf("Error: TCP/IP socket library failed to start (WSAStartup returned error %d)", ret);
        printf("%s\n", strError.c_str());
        return false;
    }
#endif

    // Create socket for listening for incoming connections
    hListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (hListenSocket == INVALID_SOCKET)
    {
        strError = strprintf("Error: Couldn't open socket for incoming connections (socket returned error %d)", WSAGetLastError());
        printf("%s\n", strError.c_str());
        return false;
    }

#ifdef SO_NOSIGPIPE
    // Different way of disabling SIGPIPE on BSD
    setsockopt(hListenSocket, SOL_SOCKET, SO_NOSIGPIPE, (void*)&nOne, sizeof(int));
#endif

#ifndef _WIN32
    // Allow binding if the port is still in TIME_WAIT state after
    // the program was closed and restarted.  Not an issue on windows.
    setsockopt(hListenSocket, SOL_SOCKET, SO_REUSEADDR, (void*)&nOne, sizeof(int));
#endif

#ifdef _WIN32
    // Set to nonblocking, incoming connections will also inherit this
    if (ioctlsocket(hListenSocket, FIONBIO, (u_long*)&nOne) == SOCKET_ERROR)
#else
    if (fcntl(hListenSocket, F_SETFL, O_NONBLOCK) == SOCKET_ERROR)
#endif
    {
        strError = strprintf("Error: Couldn't set properties on socket for incoming connections (error %d)", WSAGetLastError());
        printf("%s\n", strError.c_str());
        return false;
    }

    // The sockaddr_in structure specifies the address family,
    // IP address, and port for the socket that is being bound
    struct sockaddr_in sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = INADDR_ANY; // bind to all IPs on this computer
    sockaddr.sin_port = htons(GetListenPort());
    if (::bind(hListenSocket, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) == SOCKET_ERROR)
    {
        int nErr = WSAGetLastError();
        if (nErr == WSAEADDRINUSE)
            strError = strprintf(_("Unable to bind to port %d on this computer.  Bitcoin is probably already running."), ntohs(sockaddr.sin_port));
        else
            strError = strprintf("Error: Unable to bind to port %d on this computer (bind returned error %d)", ntohs(sockaddr.sin_port), nErr);
        printf("%s\n", strError.c_str());
        return false;
    }
    printf("Bound to port %d\n", ntohs(sockaddr.sin_port));

    // Listen for incoming connections
    if (listen(hListenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        strError = strprintf("Error: Listening for incoming connections failed (listen returned error %d)", WSAGetLastError());
        printf("%s\n", strError.c_str());
        return false;
    }

    return true;
}

void StartNode(void* parg)
{
#ifdef _WIN32
    // Get local host ip
    char pszHostName[1000] = "";
    if (gethostname(pszHostName, sizeof(pszHostName)) != SOCKET_ERROR)
    {
        vector<Endpoint> vaddr;
        if (Lookup(pszHostName, vaddr, nLocalServices, -1, true))
            BOOST_FOREACH (const Endpoint &addr, vaddr)
                if (addr.GetByte(3) != 127)
                {
                    __localhost = addr;
                    break;
                }
    }
#else
    // Get local host ip
    struct ifaddrs* myaddrs;
    if (getifaddrs(&myaddrs) == 0)
    {
        for (struct ifaddrs* ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr == NULL) continue;
            if ((ifa->ifa_flags & IFF_UP) == 0) continue;
            if (strcmp(ifa->ifa_name, "lo") == 0) continue;
            if (strcmp(ifa->ifa_name, "lo0") == 0) continue;
            char pszIP[100];
            if (ifa->ifa_addr->sa_family == AF_INET)
            {
                struct sockaddr_in* s4 = (struct sockaddr_in*)(ifa->ifa_addr);
                if (inet_ntop(ifa->ifa_addr->sa_family, (void*)&(s4->sin_addr), pszIP, sizeof(pszIP)) != NULL)
                    printf("ipv4 %s: %s\n", ifa->ifa_name, pszIP);

                // Take the first IP that isn't loopback 127.x.x.x
                Endpoint addr(*(unsigned int*)&s4->sin_addr, GetListenPort(), nLocalServices);
                if (addr.isValid() && addr.getByte(3) != 127)
                {
                    __localhost = addr;
                    break;
                }
            }
            else if (ifa->ifa_addr->sa_family == AF_INET6)
            {
                struct sockaddr_in6* s6 = (struct sockaddr_in6*)(ifa->ifa_addr);
                if (inet_ntop(ifa->ifa_addr->sa_family, (void*)&(s6->sin6_addr), pszIP, sizeof(pszIP)) != NULL)
                    printf("ipv6 %s: %s\n", ifa->ifa_name, pszIP);
            }
        }
        freeifaddrs(myaddrs);
    }
#endif
    printf("addrLocalHost = %s\n", __localhost.toString().c_str());

    if (fUseProxy || mapArgs.count("-connect") || fNoListen)
    {
        // Proxies can't take incoming connections
        __localhost.setIP(Endpoint("0.0.0.0").getIP());
        printf("addrLocalHost = %s\n", __localhost.toString().c_str());
    }
    else
    {
        CreateThread(ThreadGetMyExternalIP, NULL);
    }

    //
    // Start threads
    //

    // Map ports with UPnP
    if (fHaveUPnP)
        MapPort(fUseUPnP);

    // Get addresses from IRC and advertise ours
    if (!CreateThread(ThreadIRCSeed, NULL))
        printf("Error: CreateThread(ThreadIRCSeed) failed\n");

    // Send and receive from sockets, accept connections
    CreateThread(ThreadSocketHandler, NULL, true);

    // Initiate outbound connections
    if (!CreateThread(ThreadOpenConnections, NULL))
        printf("Error: CreateThread(ThreadOpenConnections) failed\n");

    // Process messages
    if (!CreateThread(ThreadMessageHandler, NULL))
        printf("Error: CreateThread(ThreadMessageHandler) failed\n");

    // Generate coins in the background
//    GenerateBitcoins(fGenerateBitcoins, pwalletMain);
}

bool StopNode()
{
    printf("StopNode()\n");
    fShutdown = true;
    nTransactionsUpdated++;
    int64 nStart = GetTime();
    while (vnThreadsRunning[0] > 0 || vnThreadsRunning[2] > 0 || vnThreadsRunning[3] > 0 || vnThreadsRunning[4] > 0
#ifdef USE_UPNP
        || vnThreadsRunning[5] > 0
#endif
    )
    {
        if (GetTime() - nStart > 20)
            break;
        Sleep(20);
    }
    if (vnThreadsRunning[0] > 0) printf("ThreadSocketHandler still running\n");
    if (vnThreadsRunning[1] > 0) printf("ThreadOpenConnections still running\n");
    if (vnThreadsRunning[2] > 0) printf("ThreadMessageHandler still running\n");
    if (vnThreadsRunning[3] > 0) printf("ThreadBitcoinMiner still running\n");
    if (vnThreadsRunning[4] > 0) printf("ThreadRPCServer still running\n");
    if (fHaveUPnP && vnThreadsRunning[5] > 0) printf("ThreadMapPort still running\n");
    while (vnThreadsRunning[2] > 0 || vnThreadsRunning[4] > 0)
        Sleep(20);
    Sleep(50);

    return true;
}
