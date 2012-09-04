// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include <coinChain/db.h>
#include <coinChain/BlockChain.h>

#include <coin/util.h>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/lexical_cast.hpp>

using namespace std;
using namespace boost;

void StatementBase::bind(int64 arg, int col) {
    int ret = sqlite3_bind_int64(_stmt, col, arg);
    if (ret != SQLITE_OK)
        throw runtime_error("StatementBase::bind(int64) : error " + lexical_cast<string>(ret) + " binding a value");
}
void StatementBase::bind(double arg, int col) {
    int ret = sqlite3_bind_double(_stmt, col, arg);
    if (ret != SQLITE_OK)
        throw runtime_error("StatementBase::bind(double) : error " + lexical_cast<string>(ret) + " binding a value");
}
void StatementBase::bind(const std::string& arg, int col) {
    int ret = sqlite3_bind_text(_stmt, col, arg.c_str(), -1, SQLITE_TRANSIENT);
    if (ret != SQLITE_OK)
        throw runtime_error("StatementBase::bind(text) : error " + lexical_cast<string>(ret) + " binding a value");
}
void StatementBase::bind(const blob& arg, int col) {
    int ret = sqlite3_bind_text(_stmt, col, (const char*)&arg.front(), arg.size(), SQLITE_TRANSIENT);
    if (ret != SQLITE_OK)
        throw runtime_error("StatementBase::bind(blob) : error " + lexical_cast<string>(ret) + " binding a value");
}

void StatementBase::get(int64& arg, int col) {
    int storage_class = sqlite3_column_type(_stmt, col);
    if (storage_class != SQLITE_INTEGER)
        throw runtime_error("StatementBase::get(" + lexical_cast<string>(col) + ") : storage class error, expected SQLITE_INTEGER got: " + lexical_cast<string>(storage_class));
    arg = sqlite3_column_int64(_stmt, col);
}
void StatementBase::get(double& arg, int col) {
    int storage_class = sqlite3_column_type(_stmt, col);
    if (storage_class != SQLITE_FLOAT)
        throw runtime_error("StatementBase::get(" + lexical_cast<string>(col) + ") : storage class error, expected SQLITE_FLOAT got: " + lexical_cast<string>(storage_class));
    arg = sqlite3_column_double(_stmt, col);
}
void StatementBase::get(std::string& arg, int col) {
    int storage_class = sqlite3_column_type(_stmt, col);
    if (storage_class != SQLITE3_TEXT)
        throw runtime_error("StatementBase::get(" + lexical_cast<string>(col) + ") : storage class error, expected SQLITE_TEXT got: " + lexical_cast<string>(storage_class));
    arg = string((const char*)sqlite3_column_text(_stmt, col));
}
void StatementBase::get(blob& arg, int col) {
    int storage_class = sqlite3_column_type(_stmt, col);
    if (storage_class != SQLITE_BLOB)
        throw runtime_error("StatementBase::get(" + lexical_cast<string>(col) + ") : storage class error, expected SQLITE_BLOB got: " + lexical_cast<string>(storage_class));
    int bytes = sqlite3_column_bytes(_stmt, col);
    unsigned char* data = (unsigned char*)sqlite3_column_blob(_stmt, col);
    arg = blob(data, data + bytes);
}


Database::Database(const string filename) {
    _db = NULL;
    
    int ret = sqlite3_open(filename.c_str(), &_db);
    if (ret != SQLITE_OK)
        throw runtime_error("Database() : error " + lexical_cast<string>(ret) + " opening database environment");
}

Database::~Database() {
    for (Statements::iterator s = _statements.begin(); s != _statements.end(); ++s) sqlite3_finalize(s->_stmt);
    if (_db) sqlite3_close(_db);
    _db = NULL;
}

StatementBase Database::prepare(string stmt) {
    StatementBase s;
    int ret = sqlite3_prepare_v2(_db, stmt.c_str(), -1, &s._stmt, NULL) ;
    if (ret != SQLITE_OK)
        throw runtime_error("Database::prepare() : error " + lexical_cast<string>(ret) + " preparing statement: " + stmt);
    _statements.push_back(s);
    return s;
}

void Database::execute(string stmt) {
    StatementVoid s = prepare(stmt);
    s();
}

const int64 Database::last_id() const {
    return sqlite3_last_insert_rowid(_db);
}


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

string CDB::dataDir(string suffix) {
    // Windows: C:\Documents and Settings\username\Application Data\Bitcoin
    // Mac: ~/Library/Application Support/Bitcoin
    // Unix: ~/.bitcoin
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
            printf("dbenv.open strLogDir=%s strErrorFile=%s\n", strLogDir.c_str(), strErrorFile.c_str());

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
                WriteVersion(PROTOCOL_VERSION);
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



