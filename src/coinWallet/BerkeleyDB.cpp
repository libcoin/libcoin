// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include <coinWallet/BerkeleyDB.h>

#include <coinChain/BlockChain.h>

#include <coin/util.h>
#include <coin/Logger.h>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/lexical_cast.hpp>

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

/*
string CDB::dataDir(string suffix) {
    // Windows: C:\Documents and Settings\username\Application Data\Libcoin
    // Mac: ~/Library/Application Support/Libcoin
    // Unix: ~/.libcoin
#if (defined _WIN32 || defined __MACH__) // convert first letter to upper case
    transform(suffix.begin(), suffix.begin()+1, suffix.begin(), ::toupper);
#else // prepend a "."
    suffix = "." + suffix;
#endif
    
#ifdef _WIN32
    // Windows
    return MyGetSpecialFolderPath(CSIDL_APPDATA, true) + "\\" + suffix;
#else
    char* pszHome = getenv("HOME");
    if (pszHome == NULL || strlen(pszHome) == 0)
        pszHome = (char*)"/";
    string strHome = pszHome;
    if (strHome[strHome.size()-1] != '/')
        strHome += '/';
#ifdef __MACH__
    // Mac
    strHome += "Library/Application Support/";
    filesystem::create_directory(strHome.c_str());
#endif
    
    // Unix
    return strHome + suffix;
#endif
}
*/

CDB::CDB(const std::string dataDir, const char* pszFile, const char* pszMode) : pdb(NULL)
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
            string strDataDir = dataDir;
            filesystem::create_directory(strDataDir.c_str());
            string strLogDir = strDataDir + "/database";
            filesystem::create_directory(strLogDir.c_str());
            string strErrorFile = strDataDir + "/db.log";
            log_debug("dbenv.open strLogDir=%s strErrorFile=%s\n", strLogDir.c_str(), strErrorFile.c_str());

            dbenv.set_alloc(&malloc, &realloc, &free); // WIN32 fix to force the same alloc/realloc and free routines in db and outside 
            dbenv.set_lg_dir(strLogDir.c_str());
            dbenv.set_lg_max(10000000);
            dbenv.set_lk_max_locks(100000);
            dbenv.set_lk_max_objects(100000);
            dbenv.set_cachesize(1, 0, 1); // DB cache of 1GB
            dbenv.set_errfile(fopen(strErrorFile.c_str(), "a")); /// debug
            dbenv.log_set_config(DB_LOG_AUTO_REMOVE, 1);
            dbenv.set_flags(DB_AUTO_COMMIT, 1);
        //            dbenv.set_flags(DB_TXN_WRITE_NOSYNC, 1);
        //            dbenv.set_flags(DB_TXN_NOSYNC, 1);
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
                WriteVersion(LIBRARY_VERSION);
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
    //    if (strFile == "blkindex.dat" && __blockChain->isInitialBlockDownload() && __blockChain->getBestHeight() % 500 != 0)
    if (strFile == "blkindex.dat")
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
    log_info("DBFlush(%s)%s\n", fShutdown ? "true" : "false", fDbEnvInit ? "" : " db not started");
    if (!fDbEnvInit)
        return;
    CRITICAL_BLOCK(cs_db)
    {
        map<string, int>::iterator mi = mapFileUseCount.begin();
        while (mi != mapFileUseCount.end())
        {
            string strFile = (*mi).first;
            int nRefCount = (*mi).second;
            log_debug("%s refcount=%d\n", strFile.c_str(), nRefCount);
            if (nRefCount == 0)
            {
                // Move log data to the dat file
                CloseDb(strFile);
                dbenv.txn_checkpoint(0, 0, 0);
                log_debug("%s flush\n", strFile.c_str());
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



