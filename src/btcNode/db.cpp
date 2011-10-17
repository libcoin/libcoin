// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

//#include "headers.h"
#include "btcNode/db.h"
#include "btcNode/net.h"

#include "btc/util.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

using namespace std;
using namespace boost;


//
// CDB
//

CCriticalSection cs_db;
static bool fDbEnvInit = false;
DbEnv dbenv(0);
map<string, int> mapFileUseCount;
static map<string, Db*> mapDb;

class CDBInit
{
public:
    CDBInit()
    {
    }
    ~CDBInit()
    {
        if (fDbEnvInit)
        {
            dbenv.close(0);
            fDbEnvInit = false;
        }
    }
}
instance_of_cdbinit;


CDB::CDB(const char* pszFile, const char* pszMode) : pdb(NULL)
{
    int ret;
    if (pszFile == NULL)
        return;

    fReadOnly = (!strchr(pszMode, '+') && !strchr(pszMode, 'w'));
    bool fCreate = strchr(pszMode, 'c');
    unsigned int nFlags = DB_THREAD;
    if (fCreate)
        nFlags |= DB_CREATE;

    CRITICAL_BLOCK(cs_db)
    {
        if (!fDbEnvInit)
        {
            if (fShutdown)
                return;
            string strDataDir = GetDataDir();
            string strLogDir = strDataDir + "/database";
            filesystem::create_directory(strLogDir.c_str());
            string strErrorFile = strDataDir + "/db.log";
            printf("dbenv.open strLogDir=%s strErrorFile=%s\n", strLogDir.c_str(), strErrorFile.c_str());

            dbenv.set_lg_dir(strLogDir.c_str());
            dbenv.set_lg_max(10000000);
            dbenv.set_lk_max_locks(100000);
            dbenv.set_lk_max_objects(100000);
//            dbenv.set_cachesize(4, 0, 1); // DB cache of 1GB
            dbenv.set_errfile(fopen(strErrorFile.c_str(), "a")); /// debug
            dbenv.log_set_config(DB_LOG_AUTO_REMOVE, 1);
            dbenv.set_flags(DB_AUTO_COMMIT, 1);
            ret = dbenv.open(strDataDir.c_str(),
                             DB_CREATE     |
                             DB_INIT_LOCK  |
                             DB_INIT_LOG   |
                             DB_INIT_MPOOL |
                             DB_INIT_TXN   |
                             DB_THREAD     |
                             DB_RECOVER,
                             S_IRUSR | S_IWUSR);
            if (ret > 0)
                throw runtime_error(strprintf("CDB() : error %d opening database environment", ret));
            fDbEnvInit = true;
        }

        strFile = pszFile;
        ++mapFileUseCount[strFile];
        pdb = mapDb[strFile];
        if (pdb == NULL)
        {
            pdb = new Db(&dbenv, 0);

            ret = pdb->open(NULL,      // Txn pointer
                            pszFile,   // Filename
                            "main",    // Logical db name
                            DB_BTREE,  // Database type
                            nFlags,    // Flags
                            0);

            if (ret > 0)
            {
                delete pdb;
                pdb = NULL;
                CRITICAL_BLOCK(cs_db)
                    --mapFileUseCount[strFile];
                strFile = "";
                throw runtime_error(strprintf("CDB() : can't open database file %s, error %d", pszFile, ret));
            }

            if (fCreate && !Exists(string("version")))
            {
                bool fTmp = fReadOnly;
                fReadOnly = false;
                WriteVersion(VERSION);
                fReadOnly = fTmp;
            }

            mapDb[strFile] = pdb;
        }
    }
}

void CDB::Close()
{
    if (!pdb)
        return;
    if (!vTxn.empty())
        vTxn.front()->abort();
    vTxn.clear();
    pdb = NULL;

    // Flush database activity from memory pool to disk log
    unsigned int nMinutes = 0;
    if (fReadOnly)
        nMinutes = 1;
    if (strFile == "addr.dat")
        nMinutes = 2;
    if (strFile == "blkindex.dat" && IsInitialBlockDownload() && nBestHeight % 500 != 0)
        nMinutes = 1;
    dbenv.txn_checkpoint(0, nMinutes, 0);

    CRITICAL_BLOCK(cs_db)
        --mapFileUseCount[strFile];
}

void CloseDb(const string& strFile)
{
    CRITICAL_BLOCK(cs_db)
    {
        if (mapDb[strFile] != NULL)
        {
            // Close the database handle
            Db* pdb = mapDb[strFile];
            pdb->close(0);
            delete pdb;
            mapDb[strFile] = NULL;
        }
    }
}

void DBFlush(bool fShutdown)
{
    // Flush log data to the actual data file
    //  on all files that are not in use
    printf("DBFlush(%s)%s\n", fShutdown ? "true" : "false", fDbEnvInit ? "" : " db not started");
    if (!fDbEnvInit)
        return;
    CRITICAL_BLOCK(cs_db)
    {
        map<string, int>::iterator mi = mapFileUseCount.begin();
        while (mi != mapFileUseCount.end())
        {
            string strFile = (*mi).first;
            int nRefCount = (*mi).second;
            printf("%s refcount=%d\n", strFile.c_str(), nRefCount);
            if (nRefCount == 0)
            {
                // Move log data to the dat file
                CloseDb(strFile);
                dbenv.txn_checkpoint(0, 0, 0);
                printf("%s flush\n", strFile.c_str());
                dbenv.lsn_reset(strFile.c_str(), 0);
                mapFileUseCount.erase(mi++);
            }
            else
                mi++;
        }
        if (fShutdown)
        {
            char** listp;
            if (mapFileUseCount.empty())
                dbenv.log_archive(&listp, DB_ARCH_REMOVE);
            dbenv.close(0);
            fDbEnvInit = false;
        }
    }
}






//
// CTxDB
//

bool CTxDB::ReadTxIndex(uint256 hash, CTxIndex& txindex)
{
    assert(!fClient);
    txindex.SetNull();
    return Read(make_pair(string("tx"), hash), txindex);
}

typedef pair<uint256, unsigned int> Coin;

bool CTxDB::ReadDrIndex(uint160 hash160, set<Coin>& debit)
{
    assert(!fClient);
    //    txindex.SetNull();
    return Read(make_pair(string("dr"), CBitcoinAddress(hash160).ToString()), debit);
}

bool CTxDB::ReadCrIndex(uint160 hash160, set<Coin>& credit)
{
    assert(!fClient);
    //    txindex.SetNull();
    return Read(make_pair(string("cr"), CBitcoinAddress(hash160).ToString()), credit);
}

bool Solver(const CScript& scriptPubKey, vector<pair<opcodetype, vector<unsigned char> > >& vSolutionRet);

bool CTxDB::UpdateTxIndex(uint256 hash, const CTxIndex& txindex)
{
    assert(!fClient);

//    cout << "update tx: " << hash.ToString() << endl;    
    
    CTransaction tx;
    tx.ReadFromDisk(txindex.pos);
    
    // gronager: hook to enable public key / hash160 lookups by a separate database
    // first find the keys and hash160s that are referenced in this transaction
    typedef pair<uint160, unsigned int> AssetPair;
    vector<AssetPair> vDebit;
    vector<AssetPair> vCredit;

    // for each tx out in the newly added tx check for a pubkey or a pubkeyhash in the script
    for(unsigned int n = 0; n < tx.vout.size(); n++)
    {
        const CTxOut& txout = tx.vout[n];
        vector<pair<opcodetype, vector<unsigned char> > > vSolution;
        if (!Solver(txout.scriptPubKey, vSolution))
            break;
        
        BOOST_FOREACH(PAIRTYPE(opcodetype, vector<unsigned char>)& item, vSolution)
        {
            vector<unsigned char> vchPubKey;
            if (item.first == OP_PUBKEY)
            {
                // encode the pubkey into a hash160
                vDebit.push_back(AssetPair(Hash160(item.second), n));                
            }
            else if (item.first == OP_PUBKEYHASH)
            {
                vDebit.push_back(AssetPair(uint160(item.second), n));                
            }
        }
    }
    if(!tx.IsCoinBase())
    {
        for(unsigned int n = 0; n < tx.vin.size(); n++)
        {
            const CTxIn& txin = tx.vin[n];
            CTransaction prevtx;
            if(!ReadDiskTx(txin.prevout, prevtx))
                continue; // OK ???
            CTxOut txout = prevtx.vout[txin.prevout.n];        
            
            vector<pair<opcodetype, vector<unsigned char> > > vSolution;
            if (!Solver(txout.scriptPubKey, vSolution))
                break;
            
            BOOST_FOREACH(PAIRTYPE(opcodetype, vector<unsigned char>)& item, vSolution)
            {
                vector<unsigned char> vchPubKey;
                if (item.first == OP_PUBKEY)
                {
                    // encode the pubkey into a hash160
                    vCredit.push_back(pair<uint160, unsigned int>(Hash160(item.second), n));                
                }
                else if (item.first == OP_PUBKEYHASH)
                {
                    vCredit.push_back(pair<uint160, unsigned int>(uint160(item.second), n));                
                }
            }
        }
    }
    
    for(vector<AssetPair>::iterator hashpair = vDebit.begin(); hashpair != vDebit.end(); ++hashpair)
    {
        set<Coin> txhashes;
        Read(make_pair(string("dr"), CBitcoinAddress(hashpair->first).ToString()), txhashes);
//        cout << "\t debit: " << CBitcoinAddress(hashpair->first).ToString() << endl;    
        txhashes.insert(Coin(hash, hashpair->second));
        Write(make_pair(string("dr"), CBitcoinAddress(hashpair->first).ToString()), txhashes); // overwrite!
    }
    
    for(vector<AssetPair>::iterator hashpair = vCredit.begin(); hashpair != vCredit.end(); ++hashpair)
    {
        set<Coin> txhashes;
        Read(make_pair(string("cr"), CBitcoinAddress(hashpair->first).ToString()), txhashes);
//        cout << "\t credit: " << CBitcoinAddress(hashpair->first).ToString() << endl;    
        txhashes.insert(Coin(hash, hashpair->second));
        Write(make_pair(string("cr"), CBitcoinAddress(hashpair->first).ToString()), txhashes); // overwrite!
    }
//    cout << "and write tx" << std::endl;
    return Write(make_pair(string("tx"), hash), txindex);
}

bool CTxDB::AddTxIndex(const CTransaction& tx, const CDiskTxPos& pos, int nHeight)
{
    assert(!fClient);
    
    // Add to tx index
    uint256 hash = tx.GetHash();
    CTxIndex txindex(pos, tx.vout.size());
    
    return UpdateTxIndex(hash, txindex);
//    return Write(make_pair(string("tx"), hash), txindex);
}


bool CTxDB::EraseTxIndex(const CTransaction& tx)
{
    assert(!fClient);
    uint256 hash = tx.GetHash();

    // gronager: hook to enable public key / hash160 lookups by a separate database
    // first find the keys and hash160s that are referenced in this transaction
    typedef pair<uint160, unsigned int> AssetPair;
    vector<AssetPair> vDebit;
    vector<AssetPair> vCredit;
    cout << "erase tx: " << hash.ToString() << endl;    
    // for each tx out in the newly added tx check for a pubkey or a pubkeyhash in the script
    for(unsigned int n = 0; n < tx.vout.size(); n++)
    {
        const CTxOut& txout = tx.vout[n];
        vector<pair<opcodetype, vector<unsigned char> > > vSolution;
        if (!Solver(txout.scriptPubKey, vSolution))
            break;
        
        BOOST_FOREACH(PAIRTYPE(opcodetype, vector<unsigned char>)& item, vSolution)
        {
            vector<unsigned char> vchPubKey;
            if (item.first == OP_PUBKEY)
            {
                // encode the pubkey into a hash160
                vDebit.push_back(AssetPair(Hash160(item.second), n));                
            }
            else if (item.first == OP_PUBKEYHASH)
            {
                vDebit.push_back(AssetPair(uint160(item.second), n));                
            }
        }
    }
    if(!tx.IsCoinBase())
    {
        for(unsigned int n = 0; n < tx.vin.size(); n++)
        {
            const CTxIn& txin = tx.vin[n];
            CTransaction prevtx;
            if(!ReadDiskTx(txin.prevout, prevtx))
                continue; // OK ???
            CTxOut txout = prevtx.vout[txin.prevout.n];        
            
            vector<pair<opcodetype, vector<unsigned char> > > vSolution;
            if (!Solver(txout.scriptPubKey, vSolution))
                break;
            
            BOOST_FOREACH(PAIRTYPE(opcodetype, vector<unsigned char>)& item, vSolution)
            {
                vector<unsigned char> vchPubKey;
                if (item.first == OP_PUBKEY)
                {
                    // encode the pubkey into a hash160
                    vCredit.push_back(pair<uint160, unsigned int>(Hash160(item.second), n));                
                }
                else if (item.first == OP_PUBKEYHASH)
                {
                    vCredit.push_back(pair<uint160, unsigned int>(uint160(item.second), n));                
                }
            }
        }
    }
    
    for(vector<AssetPair>::iterator hashpair = vDebit.begin(); hashpair != vDebit.end(); ++hashpair)
    {
        set<Coin> txhashes;
        Read(make_pair(string("dr"), CBitcoinAddress(hashpair->first).ToString()), txhashes);
        txhashes.erase(Coin(hash, hashpair->second));
        Write(make_pair(string("dr"), CBitcoinAddress(hashpair->first).ToString()), txhashes); // overwrite!
    }
    
    for(vector<AssetPair>::iterator hashpair = vCredit.begin(); hashpair != vCredit.end(); ++hashpair)
    {
        set<Coin> txhashes;
        Read(make_pair(string("cr"), CBitcoinAddress(hashpair->first).ToString()), txhashes);
        txhashes.erase(Coin(hash, hashpair->second));
        Write(make_pair(string("cr"), CBitcoinAddress(hashpair->first).ToString()), txhashes); // overwrite!
    }
    
    return Erase(make_pair(string("tx"), hash));
}

bool CTxDB::ContainsTx(uint256 hash)
{
    assert(!fClient);
    return Exists(make_pair(string("tx"), hash));
}

bool CTxDB::ReadOwnerTxes(uint160 hash160, int nMinHeight, vector<CTransaction>& vtx)
{
    assert(!fClient);
    vtx.clear();

    // Get cursor
    Dbc* pcursor = GetCursor();
    if (!pcursor)
        return false;

    unsigned int fFlags = DB_SET_RANGE;
    loop
    {
        // Read next record
        CDataStream ssKey;
        if (fFlags == DB_SET_RANGE)
            ssKey << string("owner") << hash160 << CDiskTxPos(0, 0, 0);
        CDataStream ssValue;
        int ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
        {
            pcursor->close();
            return false;
        }

        // Unserialize
        string strType;
        uint160 hashItem;
        CDiskTxPos pos;
        ssKey >> strType >> hashItem >> pos;
        int nItemHeight;
        ssValue >> nItemHeight;

        // Read transaction
        if (strType != "owner" || hashItem != hash160)
            break;
        if (nItemHeight >= nMinHeight)
        {
            vtx.resize(vtx.size()+1);
            if (!vtx.back().ReadFromDisk(pos))
            {
                pcursor->close();
                return false;
            }
        }
    }

    pcursor->close();
    return true;
}

bool CTxDB::ReadDiskTx(uint256 hash, CTransaction& tx, CTxIndex& txindex)
{
    assert(!fClient);
    tx.SetNull();
    if (!ReadTxIndex(hash, txindex))
        return false;
    return (tx.ReadFromDisk(txindex.pos));
}

bool CTxDB::ReadDiskTx(uint256 hash, CTransaction& tx)
{
    CTxIndex txindex;
    return ReadDiskTx(hash, tx, txindex);
}

bool CTxDB::ReadDiskTx(COutPoint outpoint, CTransaction& tx, CTxIndex& txindex)
{
    return ReadDiskTx(outpoint.hash, tx, txindex);
}

bool CTxDB::ReadDiskTx(COutPoint outpoint, CTransaction& tx)
{
    CTxIndex txindex;
    return ReadDiskTx(outpoint.hash, tx, txindex);
}

bool CTxDB::WriteBlockIndex(const CDiskBlockIndex& blockindex)
{
    return Write(make_pair(string("blockindex"), blockindex.GetBlockHash()), blockindex);
}

bool CTxDB::EraseBlockIndex(uint256 hash)
{
    return Erase(make_pair(string("blockindex"), hash));
}

bool CTxDB::ReadHashBestChain(uint256& hashBestChain)
{
    return Read(string("hashBestChain"), hashBestChain);
}

bool CTxDB::WriteHashBestChain(uint256 hashBestChain)
{
    return Write(string("hashBestChain"), hashBestChain);
}

bool CTxDB::ReadBestInvalidWork(CBigNum& bnBestInvalidWork)
{
    return Read(string("bnBestInvalidWork"), bnBestInvalidWork);
}

bool CTxDB::WriteBestInvalidWork(CBigNum bnBestInvalidWork)
{
    return Write(string("bnBestInvalidWork"), bnBestInvalidWork);
}

CBlockIndex static * InsertBlockIndex(uint256 hash)
{
    if (hash == 0)
        return NULL;

    // Return existing
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hash);
    if (mi != mapBlockIndex.end())
        return (*mi).second;

    // Create new
    CBlockIndex* pindexNew = new CBlockIndex();
    if (!pindexNew)
        throw runtime_error("LoadBlockIndex() : new CBlockIndex failed");
    mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);

    return pindexNew;
}

bool CTxDB::LoadBlockIndex()
{
    // Get database cursor
    Dbc* pcursor = GetCursor();
    if (!pcursor)
        return false;

    // Load mapBlockIndex
    unsigned int fFlags = DB_SET_RANGE;
    loop
    {
        // Read next record
        CDataStream ssKey;
        if (fFlags == DB_SET_RANGE)
            ssKey << make_pair(string("blockindex"), uint256(0));
        CDataStream ssValue;
        int ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
            return false;

        // Unserialize
        string strType;
        ssKey >> strType;
        if (strType == "blockindex")
        {
            CDiskBlockIndex diskindex;
            ssValue >> diskindex;

            // Construct block index object
            CBlockIndex* pindexNew = InsertBlockIndex(diskindex.GetBlockHash());
            pindexNew->pprev          = InsertBlockIndex(diskindex.hashPrev);
            pindexNew->pnext          = InsertBlockIndex(diskindex.hashNext);
            pindexNew->nFile          = diskindex.nFile;
            pindexNew->nBlockPos      = diskindex.nBlockPos;
            pindexNew->nHeight        = diskindex.nHeight;
            pindexNew->nVersion       = diskindex.nVersion;
            pindexNew->hashMerkleRoot = diskindex.hashMerkleRoot;
            pindexNew->nTime          = diskindex.nTime;
            pindexNew->nBits          = diskindex.nBits;
            pindexNew->nNonce         = diskindex.nNonce;

            // Watch for genesis block
            if (pindexGenesisBlock == NULL && diskindex.GetBlockHash() == hashGenesisBlock)
                pindexGenesisBlock = pindexNew;

            if (!pindexNew->CheckIndex())
                return error("LoadBlockIndex() : CheckIndex failed at %d", pindexNew->nHeight);
        }
        else
        {
            break;
        }
    }
    pcursor->close();

    // Calculate bnChainWork
    vector<pair<int, CBlockIndex*> > vSortedByHeight;
    vSortedByHeight.reserve(mapBlockIndex.size());
    BOOST_FOREACH(const PAIRTYPE(uint256, CBlockIndex*)& item, mapBlockIndex)
    {
        CBlockIndex* pindex = item.second;
        vSortedByHeight.push_back(make_pair(pindex->nHeight, pindex));
    }
    sort(vSortedByHeight.begin(), vSortedByHeight.end());
    BOOST_FOREACH(const PAIRTYPE(int, CBlockIndex*)& item, vSortedByHeight)
    {
        CBlockIndex* pindex = item.second;
        pindex->bnChainWork = (pindex->pprev ? pindex->pprev->bnChainWork : 0) + pindex->GetBlockWork();
    }

    // Load hashBestChain pointer to end of best chain
    if (!ReadHashBestChain(hashBestChain))
    {
        if (pindexGenesisBlock == NULL)
            return true;
        return error("CTxDB::LoadBlockIndex() : hashBestChain not loaded");
    }
    if (!mapBlockIndex.count(hashBestChain))
        return error("CTxDB::LoadBlockIndex() : hashBestChain not found in the block index");
    pindexBest = mapBlockIndex[hashBestChain];
    nBestHeight = pindexBest->nHeight;
    bnBestChainWork = pindexBest->bnChainWork;
    printf("LoadBlockIndex(): hashBestChain=%s  height=%d\n", hashBestChain.ToString().substr(0,20).c_str(), nBestHeight);

    // Load bnBestInvalidWork, OK if it doesn't exist
    ReadBestInvalidWork(bnBestInvalidWork);

    // Verify blocks in the best chain
    CBlockIndex* pindexFork = NULL;
    for (CBlockIndex* pindex = pindexBest; pindex && pindex->pprev; pindex = pindex->pprev)
    {
        if (pindex->nHeight < nBestHeight-2500 && !mapArgs.count("-checkblocks"))
            break;
        CBlock block;
        if (!block.ReadFromDisk(pindex))
            return error("LoadBlockIndex() : block.ReadFromDisk failed");
        if (!block.CheckBlock())
        {
            printf("LoadBlockIndex() : *** found bad block at %d, hash=%s\n", pindex->nHeight, pindex->GetBlockHash().ToString().c_str());
            pindexFork = pindex->pprev;
        }
    }
    if (pindexFork)
    {
        // Reorg back to the fork
        printf("LoadBlockIndex() : *** moving best chain pointer back to block %d\n", pindexFork->nHeight);
        CBlock block;
        if (!block.ReadFromDisk(pindexFork))
            return error("LoadBlockIndex() : block.ReadFromDisk failed");
        CTxDB txdb;
        block.SetBestChain(txdb, pindexFork);
    }

    return true;
}




//
// CDBAssetSyncronizer
//


void CDBAssetSyncronizer::getCreditCoins(uint160 btc, Coins& coins)
{
    _txdb.ReadCrIndex(btc, coins);
    CRITICAL_BLOCK(cs_mapTransactions)
    if(mapCredits.count(btc))
        coins.insert(mapCredits[btc].begin(), mapCredits[btc].end());
}

void CDBAssetSyncronizer::getDebitCoins(uint160 btc, Coins& coins)
{
    _txdb.ReadDrIndex(btc, coins);
    
    CRITICAL_BLOCK(cs_mapTransactions)
    if(mapDebits.count(btc))
        coins.insert(mapDebits[btc].begin(), mapDebits[btc].end());
}

void CDBAssetSyncronizer::getTransaction(const Coin& coin, CTx& tx)
{
    CTransaction transaction;
    if(!_txdb.ReadDiskTx(coin.first, transaction))
    {
        CRITICAL_BLOCK(cs_mapTransactions)
        {
            if(mapTransactions.count(coin.first))
                transaction = mapTransactions[coin.first];
            //                   else
            //                       throw JSONRPCError(-5, "Invalid transaction id");        
        }
    }
           
    tx = transaction;
}

void CDBAssetSyncronizer::getCoins(uint160 btc, Coins& coins)
{
    // read all relevant tx'es
    Coins debit;
    getDebitCoins(btc, debit);
    Coins credit;
    getCreditCoins(btc, credit);
    
    for(Coins::iterator coin = debit.begin(); coin != debit.end(); ++coin)
    {
        CTransaction tx;
        getTransaction(*coin, tx);
        coins.insert(*coin);
    }
    for(Coins::iterator coin = credit.begin(); coin != credit.end(); ++coin)
    {
        CTransaction tx;
        getTransaction(*coin, tx);
        
        CTxIn in = tx.vin[coin->second];
        Coin spend(in.prevout.hash, in.prevout.n);
        coins.erase(spend);
    }
}




//
// CAddrDB
//

bool CAddrDB::WriteAddress(const CAddress& addr)
{
    return Write(make_pair(string("addr"), addr.GetKey()), addr);
}

bool CAddrDB::EraseAddress(const CAddress& addr)
{
    return Erase(make_pair(string("addr"), addr.GetKey()));
}

bool CAddrDB::LoadAddresses()
{
    CRITICAL_BLOCK(cs_mapAddresses)
    {
        // Load user provided addresses
        CAutoFile filein = fopen((GetDataDir() + "/addr.txt").c_str(), "rt");
        if (filein)
        {
            try
            {
                char psz[1000];
                while (fgets(psz, sizeof(psz), filein))
                {
                    CAddress addr(psz, false, NODE_NETWORK);
                    addr.nTime = 0; // so it won't relay unless successfully connected
                    if (addr.IsValid())
                        AddAddress(addr);
                }
            }
            catch (...) { }
        }

        // Get cursor
        Dbc* pcursor = GetCursor();
        if (!pcursor)
            return false;

        loop
        {
            // Read next record
            CDataStream ssKey;
            CDataStream ssValue;
            int ret = ReadAtCursor(pcursor, ssKey, ssValue);
            if (ret == DB_NOTFOUND)
                break;
            else if (ret != 0)
                return false;

            // Unserialize
            string strType;
            ssKey >> strType;
            if (strType == "addr")
            {
                CAddress addr;
                ssValue >> addr;
                mapAddresses.insert(make_pair(addr.GetKey(), addr));
            }
        }
        pcursor->close();

        printf("Loaded %d addresses\n", mapAddresses.size());
    }

    return true;
}

bool LoadAddresses()
{
    return CAddrDB("cr+").LoadAddresses();
}

//
// CBrokerDB
//

bool CBrokerDB::WriteTx(const CTx& tx)
{
    return Write(make_pair(string("hash"), tx.GetHash()), tx);
}

bool CBrokerDB::EraseTx(const CTx& tx)
{
    return Erase(make_pair(string("hash"), tx.GetHash()));
}

bool CBrokerDB::LoadTxes(map<uint256, CTx>& txes)
{
    // Get cursor
    Dbc* pcursor = GetCursor();
    if (!pcursor)
        return false;
    
    loop
    {
        // Read next record
        CDataStream ssKey;
        CDataStream ssValue;
        int ret = ReadAtCursor(pcursor, ssKey, ssValue);
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
            return false;
        
        // Unserialize
        string strType;
        ssKey >> strType;
        if (strType == "hash")
        {
            CTx tx;
            ssValue >> tx;
            txes.insert(make_pair(tx.GetHash(), tx));
        }
    }
    pcursor->close();
        
    
    return true;
}



