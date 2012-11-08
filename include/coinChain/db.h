// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_DB_H
#define BITCOIN_DB_H

#include <coin/Key.h>
#include <coin/Transaction.h>
#include <coin/Asset.h>

#include <coinChain/Export.h>


#include <coin/uint256.h>

#include <map>
#include <string>
#include <vector>

#include <db_cxx.h>

#include <sqlite3.h>

#include <boost/ptr_container/ptr_vector.hpp>

typedef std::vector<unsigned char> blob;

class Database;

struct undefined {};

class StatementBase {
    friend class Database;
public:
    class Error : public std::runtime_error {
    public:
        Error(const std::string& s) : std::runtime_error(s.c_str()) {}
    };

    StatementBase() {
        _ = new Representation;
        _->ref();
    }
    
    StatementBase(const StatementBase& s) {
        s._->ref();
        _ = s._;
    }

    ~StatementBase() {
        if (_->unref() == 0) {
            sqlite3_finalize(_->stmt);
            delete _;
        }
    }
    
    StatementBase& operator=(const StatementBase& s) {
        if(this != &s) {
            s._->ref();
            if(_->unref() == 0) {
                sqlite3_finalize(_->stmt);
                delete _;
            }
            _ = s._;
        }
        return *this;
    }
    
    int64 stat_time() const { return _->stat_time; }
    int64 stat_count() const { return _->stat_count; }
protected:
    void bind(int64 arg, int col) const;
    void bind(size_t arg, int col) const { bind((int64)arg, col); }
    void bind(int arg, int col) const { bind((int64)arg, col); }
    void bind(unsigned int arg, int col) const { bind((int64)arg, col); }
    void bind(double arg, int col) const;
    void bind(const std::string& arg, int col) const;
    void bind(const blob& arg, int col) const;
    void bind(const uint256& arg, int col) const;
    
    void get(int64& arg, int col) const;
    void get(int& arg, int col) const { int64 a; get((a), col); arg = a;}
    void get(unsigned int& arg, int col) const { int64 a; get((a), col); arg = a;}
    void get(unsigned short& arg, int col) const { int64 a; get((a), col); arg = a;}

    void get(double& arg, int col) const;
    void get(std::string& arg, int col) const;
    void get(blob& arg, int col) const;
    void get(uint256& arg, int col) const;
    
    void get(undefined, int) const {}
    
    const std::string& sql() const { return _->sql; }
protected:
    class Representation {
    public:
        sqlite3_stmt *stmt;
        std::string sql;
        mutable int64 stat_time;
        mutable int64 stat_count;

        Representation() : stmt(NULL), stat_time(0), stat_count(0), _refs(0) {}
        
        int ref() { return ++_refs; }
        int unref() { return --_refs; }
    private:
        int _refs;
    };
    Representation* _;
};

#define FUNCTION_CALL_DEFINITIONS(RETURN_TYPE) \
RETURN_TYPE operator()() { \
return eval(); \
} \
template <typename T1> \
RETURN_TYPE operator()(T1 v1) { \
bind(v1, 1); \
return operator()(); \
} \
template <typename T1, typename T2> RETURN_TYPE \
operator()(T1 v1, T2 v2) { \
bind(v2, 2); \
return operator()(v1); \
} \
template <typename T1, typename T2, typename T3> RETURN_TYPE \
operator()(T1 v1, T2 v2, T3 v3) { \
bind(v3, 3); \
return operator()(v1, v2); \
} \
template <typename T1, typename T2, typename T3, typename T4> RETURN_TYPE \
operator()(T1 v1, T2 v2, T3 v3, T4 v4) { \
bind(v4, 4); \
return operator()(v1, v2, v3); \
} \
template <typename T1, typename T2, typename T3, typename T4, typename T5> RETURN_TYPE \
operator()(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5) { \
bind(v5, 5); \
return operator()(v1, v2, v3, v4); \
} \
template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6> RETURN_TYPE \
operator()(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6) { \
bind(v6, 6); \
return operator()(v1, v2, v3, v4, v5); \
} \
template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7> RETURN_TYPE \
operator()(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6, T7 v7) { \
bind(v7, 7); \
return operator()(v1, v2, v3, v4, v5, v6); \
} \
template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8> RETURN_TYPE \
operator()(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6, T7 v7, T8 v8) { \
bind(v8, 8); \
return operator()(v1, v2, v3, v4, v5, v6, v7); \
} \

#define FUNCTION_CONST_CALL_DEFINITIONS(RETURN_TYPE) \
RETURN_TYPE operator()() const { \
return eval(); \
} \
template <typename T1> \
RETURN_TYPE operator()(T1 v1) const { \
bind(v1, 1); \
return operator()(); \
} \
template <typename T1, typename T2> RETURN_TYPE \
operator()(T1 v1, T2 v2) const { \
bind(v2, 2); \
return operator()(v1); \
} \
template <typename T1, typename T2, typename T3> RETURN_TYPE \
operator()(T1 v1, T2 v2, T3 v3) const { \
bind(v3, 3); \
return operator()(v1, v2); \
} \
template <typename T1, typename T2, typename T3, typename T4> RETURN_TYPE \
operator()(T1 v1, T2 v2, T3 v3, T4 v4) const { \
bind(v4, 4); \
return operator()(v1, v2, v3); \
} \
template <typename T1, typename T2, typename T3, typename T4, typename T5> RETURN_TYPE \
operator()(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5) const { \
bind(v5, 5); \
return operator()(v1, v2, v3, v4); \
} \
template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6> RETURN_TYPE \
operator()(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6) const { \
bind(v6, 6); \
return operator()(v1, v2, v3, v4, v5); \
} \
template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7> RETURN_TYPE \
operator()(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6, T7 v7) const { \
bind(v7, 7); \
return operator()(v1, v2, v3, v4, v5, v6); \
} \
template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8> RETURN_TYPE \
operator()(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6, T7 v7, T8 v8) const { \
bind(v8, 8); \
return operator()(v1, v2, v3, v4, v5, v6, v7); \
}


template <class R, int N, class P0, class P1 = undefined, class P2 = undefined, class P3 = undefined, class P4 = undefined, class P5 = undefined, class P6 = undefined, class P7 = undefined>
class Construct {
public:
    static R construct(P0 p0, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7) {
        // will be kicked when N > 8
        undefined ONLY_UP_TO_8_RETURN_VALUES_SUPPORTED;
        P7 this_will_fail_at_compile_time = ONLY_UP_TO_8_RETURN_VALUES_SUPPORTED;
    }
};
template <class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7>
class Construct<R, 8, P0, P1, P2, P3, P4, P5, P6, P7> {
public:
    static R construct(P0 p0, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7) {
        return R(p0, p1, p2, p3, p4, p5, p6, p7);
    }
};
template <class R, class P0, class P1, class P2, class P3, class P4, class P5, class P6>
class Construct<R, 7, P0, P1, P2, P3, P4, P5, P6> {
public:
    static R construct(P0 p0, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, undefined) {
        return R(p0, p1, p2, p3, p4, p5, p6);
    }
};
template <class R, class P0, class P1, class P2, class P3, class P4, class P5>
class Construct<R, 6, P0, P1, P2, P3, P4, P5> {
public:
    static R construct(P0 p0, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, undefined, undefined) {
        return R(p0, p1, p2, p3, p4, p5);
    }
};
template <class R, class P0, class P1, class P2, class P3, class P4>
class Construct<R, 5, P0, P1, P2, P3, P4> {
public:
    static R construct(P0 p0, P1 p1, P2 p2, P3 p3, P4 p4, undefined, undefined, undefined) {
        return R(p0, p1, p2, p3, p4);
    }
};
template <class R, class P0, class P1, class P2, class P3>
class Construct<R, 4, P0, P1, P2, P3> {
public:
    static R construct(P0 p0, P1 p1, P2 p2, P3 p3, undefined, undefined, undefined, undefined) {
        return R(p0, p1, p2, p3);
    }
};
template <class R, class P0, class P1, class P2>
class Construct<R, 3, P0, P1, P2> {
public:
    static R construct(P0 p0, P1 p1, P2 p2, undefined, undefined, undefined, undefined, undefined) {
        return R(p0, p1, p2);
    }
};
template <class R, class P0, class P1>
class Construct<R, 2, P0, P1> {
public:
    static R construct(P0 p0, P1 p1, undefined, undefined, undefined, undefined, undefined, undefined) {
        return R(p0, p1);
    }
};
template <class R, class P0>
class Construct<R, 1, P0> {
public:
    static R construct(P0 p0, undefined, undefined, undefined, undefined, undefined, undefined, undefined) {
        return R(p0);
    }
};

template <class R, int N, class P0, class P1 = undefined, class P2 = undefined, class P3 = undefined, class P4 = undefined, class P5 = undefined, class P6 = undefined, class P7 = undefined>
class StatementVec : public StatementBase {
public:
    StatementVec() : StatementBase() {}
    StatementVec(const StatementBase& stmt) : StatementBase(stmt) {}
    
    std::vector<R> eval() {
        int64 t0 = GetTimeMicros();
        int result = sqlite3_step(_->stmt);
        std::vector<R> rs;;
        while(result == SQLITE_ROW) {
            P0 p0; P1 p1; P2 p2; P3 p3; P4 p4; P5 p5; P6 p6; P7 p7;
            get(p0, 0); get(p1, 1); get(p2, 2); get(p3, 3); get(p4, 4); get(p5, 5); get(p6, 6); get(p7, 7);
            rs.push_back(Construct<R, N, P0, P1, P2, P3, P4, P5, P6, P7>::construct(p0, p1, p2, p3, p4, p5, p6, p7));
            result = sqlite3_step(_->stmt);
        }
        sqlite3_reset(_->stmt);
        _->stat_time += GetTimeMicros() - t0;
        ++_->stat_count;
        return rs;
    }
    
    const std::vector<R> eval() const {
        int64 t0 = GetTimeMicros();
        if (sqlite3_stmt_readonly(_->stmt) == 0)
            throw Error("StatementVec::eval() const: error only readonly statements can be executed through the const interface");

        int result = sqlite3_step(_->stmt);
        std::vector<R> rs;;
        while(result == SQLITE_ROW) {
            P0 p0; P1 p1; P2 p2; P3 p3; P4 p4; P5 p5; P6 p6; P7 p7;
            get(p0, 0); get(p1, 1); get(p2, 2); get(p3, 3); get(p4, 4); get(p5, 5); get(p6, 6); get(p7, 7);
            rs.push_back(Construct<R, N, P0, P1, P2, P3, P4, P5, P6, P7>::construct(p0, p1, p2, p3, p4, p5, p6, p7));
            result = sqlite3_step(_->stmt);
        }
        sqlite3_reset(_->stmt);
        _->stat_time += GetTimeMicros() - t0;
        ++_->stat_count;
        return rs;
    }
    
    FUNCTION_CALL_DEFINITIONS(std::vector<R>)
    FUNCTION_CONST_CALL_DEFINITIONS(std::vector<R>)
};

template <class R, int N, class P0, class P1 = undefined, class P2 = undefined, class P3 = undefined, class P4 = undefined, class P5 = undefined, class P6 = undefined, class P7 = undefined>
class StatementClass : public StatementBase {
public:
    StatementClass() : StatementBase() {}
    StatementClass(const StatementBase& stmt) : StatementBase(stmt) {}
    
    R eval() {
        int64 t0 = GetTimeMicros();
        R r = R();
        if (sqlite3_step(_->stmt) == SQLITE_ROW) {
            P0 p0; P1 p1; P2 p2; P3 p3; P4 p4; P5 p5; P6 p6; P7 p7;
            get(p0, 0); get(p1, 1); get(p2, 2); get(p3, 3); get(p4, 4); get(p5, 5); get(p6, 6); get(p7, 7);
            r = Construct<R, N, P0, P1, P2, P3, P4, P5, P6, P7>::construct(p0, p1, p2, p3, p4, p5, p6, p7);
        }
        sqlite3_reset(_->stmt);
        _->stat_time += GetTimeMicros() - t0;
        ++_->stat_count;
        return r;
    }
    
    const R eval() const {
        if (sqlite3_stmt_readonly(_->stmt) == 0)
            throw Error("StatementClass::eval() const: error only readonly statements can be executed through the const interface");

        int64 t0 = GetTimeMicros();
        //        int result = sqlite3_step(_stmt);
        R r = R();
        if (sqlite3_step(_->stmt) == SQLITE_ROW) {
            P0 p0; P1 p1; P2 p2; P3 p3; P4 p4; P5 p5; P6 p6; P7 p7;
            get(p0, 0); get(p1, 1); get(p2, 2); get(p3, 3); get(p4, 4); get(p5, 5); get(p6, 6); get(p7, 7);
            r = Construct<R, N, P0, P1, P2, P3, P4, P5, P6, P7>::construct(p0, p1, p2, p3, p4, p5, p6, p7);
        }
        sqlite3_reset(_->stmt);
        _->stat_time += GetTimeMicros() - t0;
        ++_->stat_count;
        return r;
    }
    
    FUNCTION_CALL_DEFINITIONS(R)
    FUNCTION_CONST_CALL_DEFINITIONS(R)
};

template <class R = undefined>
class Statement : public StatementBase {
public:
    Statement() : StatementBase() {}
    Statement(const StatementBase& stmt) : StatementBase(stmt) {}
    
    R eval() {
        int64 t0 = GetTimeMicros();
        R r = R();
        if (sqlite3_step(_->stmt) == SQLITE_ROW)
            get(r, 0);
        sqlite3_reset(_->stmt);
        _->stat_time += GetTimeMicros() - t0;
        ++_->stat_count;
        return r;
    }
    
    const R eval() const {
        int64 t0 = GetTimeMicros();
        if (sqlite3_stmt_readonly(_->stmt) == 0)
            throw Error("Statement::eval() const: error only readonly statements can be executed through the const interface");
        R r = R();
        if (sqlite3_step(_->stmt) == SQLITE_ROW)
            get(r, 0);
        sqlite3_reset(_->stmt);
        _->stat_time += GetTimeMicros() - t0;
        ++_->stat_count;
        return r;
    }
    
    FUNCTION_CALL_DEFINITIONS(R)
    FUNCTION_CONST_CALL_DEFINITIONS(R)
};

template<>
class Statement<undefined> : public StatementBase {
public:
    Statement() : StatementBase() {}
    Statement(const StatementBase& stmt) : StatementBase(stmt) {}
    
    undefined eval() {
        int64 t0 = GetTimeMicros();
        while(sqlite3_step(_->stmt) == SQLITE_ROW);
        sqlite3_reset(_->stmt);
        _->stat_time += GetTimeMicros() - t0;
        ++_->stat_count;
        return undefined();
    }
    
    const undefined eval() const {
        int64 t0 = GetTimeMicros();
        if (sqlite3_stmt_readonly(_->stmt) == 0)
            throw Error("StatementVoid::eval() const: error only readonly statements can be executed through the const interface");
        while(sqlite3_step(_->stmt) == SQLITE_ROW);
        sqlite3_reset(_->stmt);
        _->stat_time += GetTimeMicros() - t0;
        ++_->stat_count;
        return undefined();
    }
    
    FUNCTION_CALL_DEFINITIONS(undefined)
    FUNCTION_CONST_CALL_DEFINITIONS(undefined)
};

typedef Statement<undefined> StatementVoid;

class StatementLastId : public StatementBase {
public:
    StatementLastId() : StatementBase() {}
    StatementLastId(const StatementBase& stmt) : StatementBase(stmt) {}
    
    int64 eval() {
        int64 t0 = GetTimeMicros();
        while(sqlite3_step(_->stmt) == SQLITE_ROW);
        int64 last_id = sqlite3_last_insert_rowid(sqlite3_db_handle(_->stmt));
        sqlite3_reset(_->stmt);
        _->stat_time += GetTimeMicros() - t0;
        ++_->stat_count;
        return last_id;
    }
    
    const int64 eval() const {
        if (sqlite3_stmt_readonly(_->stmt) == 0)
            throw Error("StatementLastId::eval() const: error only readonly statements can be executed through the const interface");
        int64 t0 = GetTimeMicros();
        while(sqlite3_step(_->stmt) == SQLITE_ROW);
        int64 last_id = sqlite3_last_insert_rowid(sqlite3_db_handle(_->stmt));
        sqlite3_reset(_->stmt);
        _->stat_time += GetTimeMicros() - t0;
        ++_->stat_count;
        return last_id;
    }
    
    FUNCTION_CALL_DEFINITIONS(int64)
    FUNCTION_CONST_CALL_DEFINITIONS(int64)
};

class Database {
public:
    class Error : public std::runtime_error {
    public:
        Error(const std::string& s) : std::runtime_error(s.c_str()) {}
    };
    
	Database(const std::string filename);
	~Database();
	
    StatementBase prepare(std::string stmt);
    StatementBase& rprepare(std::string stmt);
    void execute(std::string stmt);
    
    void begin();
    void commit();
    void rollback();
    
    const int64 last_id() const;
    
    const std::string error_text() const { return sqlite3_errmsg(_db); }
    
    const std::string statistics() const;
private:
    StatementVoid _begin;
    StatementVoid _commit;
    StatementVoid _rollback;
private:
	sqlite3 *_db;
    typedef std::vector<StatementBase> Statements;
    Statements _statements;
};

/// OLD Database code for Berkeley DB

extern CCriticalSection cs_db;
extern DbEnv dbenv;
extern std::map<std::string, int> mapFileUseCount;

extern void DBFlush(bool fShutdown);

class COINCHAIN_EXPORT CDB
{
protected:
    Db* pdb;
    std::string strFile;
    std::vector<DbTxn*> vTxn;
    bool fReadOnly;

    explicit CDB(const std::string dataDir, const char* pszFile, const char* pszMode="r+");
    ~CDB() { Close(); }
public:
    static std::string dataDir(std::string suffix);
    
    void Close();
private:
    CDB(const CDB&);
    void operator=(const CDB&);

protected:
    template<typename K, typename T>
    bool Read(const K& key, T& value) const
    {
        if (!pdb)
            return false;

        // Key
        CDataStream ssKey(SER_DISK);
        ssKey.reserve(1000);
        ssKey << key;
        Dbt datKey(&ssKey[0], ssKey.size());

        // Read
        Dbt datValue;
        datValue.set_flags(DB_DBT_MALLOC);
//        std::3000 << "about to read" << std::endl;
        int ret = pdb->get(GetTxn(), &datKey, &datValue, 0);
//        std::cout << "did read" << std::endl;
        memset(datKey.get_data(), 0, datKey.get_size());
        if (datValue.get_data() == NULL)
            return false;

        // Unserialize value
        CDataStream ssValue((char*)datValue.get_data(), (char*)datValue.get_data() + datValue.get_size(), SER_DISK);
        ssValue >> value;

        // Clear and free memory
        memset(datValue.get_data(), 0, datValue.get_size());
        free(datValue.get_data());
        return (ret == 0);
    }

    template<typename K, typename T>
    bool Write(const K& key, const T& value, bool fOverwrite=true)
    {
        if (!pdb)
            return false;
        if (fReadOnly)
            assert(!"Write called on database in read-only mode");

        // Key
        CDataStream ssKey(SER_DISK);
        ssKey.reserve(1000);
        ssKey << key;
        Dbt datKey(&ssKey[0], ssKey.size());

        // Value
        CDataStream ssValue(SER_DISK);
        ssValue.reserve(10000);
        ssValue << value;
        Dbt datValue(&ssValue[0], ssValue.size());

        // Write
//        std::cout << "about to write: " << ssKey.size() << ", " << ssValue.size() << std::endl;
        int ret = pdb->put(GetTxn(), &datKey, &datValue, (fOverwrite ? 0 : DB_NOOVERWRITE));
//        std::cout << "did write" << std::endl;

        // Clear memory in case it was a private key
        memset(datKey.get_data(), 0, datKey.get_size());
        memset(datValue.get_data(), 0, datValue.get_size());
        return (ret == 0);
    }

    template<typename K>
    bool Erase(const K& key)
    {
        if (!pdb)
            return false;
        if (fReadOnly)
            assert(!"Erase called on database in read-only mode");

        // Key
        CDataStream ssKey(SER_DISK);
        ssKey.reserve(1000);
        ssKey << key;
        Dbt datKey(&ssKey[0], ssKey.size());

        // Erase
        int ret = pdb->del(GetTxn(), &datKey, 0);

        // Clear memory
        memset(datKey.get_data(), 0, datKey.get_size());
        return (ret == 0 || ret == DB_NOTFOUND);
    }

    template<typename K>
    bool Exists(const K& key) const
    {
        if (!pdb)
            return false;

        // Key
        CDataStream ssKey(SER_DISK);
        ssKey.reserve(1000);
        ssKey << key;
        Dbt datKey(&ssKey[0], ssKey.size());

        // Exists
        int ret = pdb->exists(GetTxn(), &datKey, 0);

        // Clear memory
        memset(datKey.get_data(), 0, datKey.get_size());
        return (ret == 0);
    }

    Dbc* GetCursor()
    {
        if (!pdb)
            return NULL;
        Dbc* pcursor = NULL;
        int ret = pdb->cursor(NULL, &pcursor, 0);
        if (ret != 0)
            return NULL;
        return pcursor;
    }

    int ReadAtCursor(Dbc* pcursor, CDataStream& ssKey, CDataStream& ssValue, unsigned int fFlags=DB_NEXT)
    {
        // Read at cursor
        Dbt datKey;
        if (fFlags == DB_SET || fFlags == DB_SET_RANGE || fFlags == DB_GET_BOTH || fFlags == DB_GET_BOTH_RANGE)
        {
            datKey.set_data(&ssKey[0]);
            datKey.set_size(ssKey.size());
        }
        Dbt datValue;
        if (fFlags == DB_GET_BOTH || fFlags == DB_GET_BOTH_RANGE)
        {
            datValue.set_data(&ssValue[0]);
            datValue.set_size(ssValue.size());
        }
        datKey.set_flags(DB_DBT_MALLOC);
        datValue.set_flags(DB_DBT_MALLOC);
        int ret = pcursor->get(&datKey, &datValue, fFlags);
        if (ret != 0)
            return ret;
        else if (datKey.get_data() == NULL || datValue.get_data() == NULL)
            return 99999;

        // Convert to streams
        ssKey.SetType(SER_DISK);
        ssKey.clear();
        ssKey.write((char*)datKey.get_data(), datKey.get_size());
        ssValue.SetType(SER_DISK);
        ssValue.clear();
        ssValue.write((char*)datValue.get_data(), datValue.get_size());

        // Clear and free memory
        memset(datKey.get_data(), 0, datKey.get_size());
        memset(datValue.get_data(), 0, datValue.get_size());
        free(datKey.get_data());
        free(datValue.get_data());
        return 0;
    }

    DbTxn* GetTxn() const
    {
        if (!vTxn.empty())
            return vTxn.back();
        else
            return NULL;
    }

public:
    bool TxnBegin()
    {
        if (!pdb)
            return false;
        DbTxn* ptxn = NULL;
        int ret = dbenv.txn_begin(GetTxn(), &ptxn, DB_TXN_NOSYNC);
        if (!ptxn || ret != 0)
            return false;
        vTxn.push_back(ptxn);
        return true;
    }

    bool TxnCommit()
    {
        if (!pdb)
            return false;
        if (vTxn.empty())
            return false;
        int ret = vTxn.back()->commit(0);
        vTxn.pop_back();
        return (ret == 0);
    }

    bool TxnAbort()
    {
        if (!pdb)
            return false;
        if (vTxn.empty())
            return false;
        int ret = vTxn.back()->abort();
        vTxn.pop_back();
        return (ret == 0);
    }

    bool ReadVersion(int& nVersion)
    {
        nVersion = 0;
        return Read(std::string("version"), nVersion);
    }

    bool WriteVersion(int nVersion)
    {
        return Write(std::string("version"), nVersion);
    }
};

enum DBErrors
{
    DB_LOAD_OK,
    DB_CORRUPT,
    DB_TOO_NEW,
    DB_LOAD_FAIL,
};

void CloseDb(const std::string& strFile);

#endif
