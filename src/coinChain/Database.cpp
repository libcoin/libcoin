/* -*-c++-*- clusterizer - Copyright (C) 2012 Michael Gronager
 *
 * libcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * libcoin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libcoin.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <coinChain/Database.h>
#include <coin/Logger.h>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/xpressive/xpressive_dynamic.hpp>

#include <sstream>

#ifndef __POSTGRESQL__
typedef enum
{
    PGRES_EMPTY_QUERY = 0,		/* empty query string was executed */
    PGRES_COMMAND_OK,			/* a query command that doesn't return
                                 * anything was executed properly by the
                                 * backend */
    PGRES_TUPLES_OK,			/* a query command that returns tuples was
                                 * executed properly by the backend, PGresult
                                 * contains the result tuples */
    PGRES_COPY_OUT,				/* Copy Out data transfer in progress */
    PGRES_COPY_IN,				/* Copy In data transfer in progress */
    PGRES_BAD_RESPONSE,			/* an unexpected response was recv'd from the
                                 * backend */
    PGRES_NONFATAL_ERROR,		/* notice or warning message */
    PGRES_FATAL_ERROR,			/* query failed */
    PGRES_COPY_BOTH,			/* Copy In/Out data transfer in progress */
    PGRES_SINGLE_TUPLE			/* single tuple from larger resultset */
} ExecStatusType;

typedef enum
{
    CONNECTION_OK,
    CONNECTION_BAD,
    /* Non-blocking mode only below here */
    
    /*
     * The existence of these should never be relied upon - they should only
     * be used for user feedback or similar purposes.
     */
    CONNECTION_STARTED,			/* Waiting for connection to be made.  */
    CONNECTION_MADE,			/* Connection OK; waiting to send.     */
    CONNECTION_AWAITING_RESPONSE,		/* Waiting for a response from the
                                         * postmaster.        */
    CONNECTION_AUTH_OK,			/* Received authentication; waiting for
                                 * backend startup. */
    CONNECTION_SETENV,			/* Negotiating environment. */
    CONNECTION_SSL_STARTUP,		/* Negotiating SSL. */
    CONNECTION_NEEDED			/* Internal state: connect() needed */
} ConnStatusType;

PGconn *PQconnectdb(const char *conninfo) { return NULL; }
void PQfinish(PGconn *conn) { }

PGresult *PQprepare(PGconn *conn, const char *stmtName, const char *query, int nParams, const Oid *paramTypes) { return NULL; }
PGresult *PQdescribePrepared(PGconn *conn, const char *stmt) { return NULL; }
PGresult *PQexecPrepared(PGconn *conn, const char *stmtName, int nParams, const char *const * paramValues, const int *paramLengths, const int *paramFormats, int resultFormat) {
    return NULL;
}

int	PQntuples(const PGresult *res) { return 0; }
int	PQnfields(const PGresult *res) { return 0; }
Oid	PQftype(const PGresult *res, int field_num) { return 0; }

void PQclear(PGresult *res) { }

char *PQerrorMessage(const PGconn *conn) { return NULL; }

ConnStatusType PQstatus(const PGconn *conn) { return CONNECTION_BAD; }

ExecStatusType PQresultStatus(const PGresult *res) { return PGRES_EMPTY_QUERY; }
char *PQresStatus(ExecStatusType status) { return NULL; }
char *PQresultErrorMessage(PGresult *res) { return NULL; }
char *PQgetvalue(const PGresult *res, int tup_num, int field_num) { return NULL; }
int	PQgetlength(const PGresult *res, int tup_num, int field_num) { return 0; }
int	PQgetisnull(const PGresult *res, int tup_num, int field_num) { return 0; }
int	PQnparams(const PGresult *res) { return 0; }
Oid	PQparamtype(const PGresult *res, int param_num) { return 0; }

#endif


using namespace std;
using namespace boost;

namespace postgresqlite {
    
    enum {
        BOOLOID = 16,
        BYTEAOID = 17,
        CHAROID = 18,
        NAMEOID = 19,
        INT8OID = 20,
        INT2OID = 21,
        INT4OID = 23,
        TEXTOID = 25,
        FLOAT4OID = 700,
        FLOAT8OID = 701
    };
    
    void StatementBase::bind(int64_t arg, int col) const {
        if (isPSQL()) {
            if (--col >= _->values.size())
                throw Database::Error("StatementBase::bind(" + lexical_cast<string>(arg) + ", " + lexical_cast<string>(col) + ") : too many params - max is : " + lexical_cast<string>(_->values.size()-1));
            
            if (_->params[col] != INT8OID)
                throw Database::Error("StatementBase::bind(" + lexical_cast<string>(arg) + ", " + lexical_cast<string>(col) + ") : binding an int64_t to an oid # : " + lexical_cast<string>(_->params[col]));
            
            const char * cval = (const char *)(&arg);
            int imax = sizeof(arg)-1;
            for (size_t i = 0; i < sizeof(arg); ++i)
                _->values[col].push_back(cval[imax-i]);
        }
        else {
            int ret = sqlite3_bind_int64(_->stmt, col, arg);
            if (ret != SQLITE_OK)
                throw Database::Error("StatementBase::bind(int64_t) : error " + lexical_cast<string>(ret) + " binding a value");
        }
    }
    void StatementBase::bind(double arg, int col) const {
        if (isPSQL()) {
            if (--col >= _->values.size())
                throw Database::Error("StatementBase::bind(" + lexical_cast<string>(arg) + ", " + lexical_cast<string>(col) + ") : too many params - max is : " + lexical_cast<string>(_->values.size()-1));
            
            if (_->params[col] != FLOAT8OID)
                throw Database::Error("StatementBase::bind(" + lexical_cast<string>(arg) + ", " + lexical_cast<string>(col) + ") : binding a double to an oid # : " + lexical_cast<string>(_->params[col]));
            
            const char * cval = (const char *)(&arg);
            int imax = sizeof(arg)-1;
            for (size_t i = 0; i < sizeof(arg); ++i)
                _->values[col].push_back(cval[imax-i]);
        }
        else {
            int ret = sqlite3_bind_double(_->stmt, col, arg);
            if (ret != SQLITE_OK)
                throw Database::Error("StatementBase::bind(double) : error " + lexical_cast<string>(ret) + " binding a value");
        }
    }
    void StatementBase::bind(const std::string& arg, int col) const {
        if (isPSQL()) {
            if (--col >= _->values.size())
                throw Database::Error("StatementBase::bind(" + arg + ", " + lexical_cast<string>(col) + ") : too many params - max is : " + lexical_cast<string>(_->values.size()-1));
            
            if (_->params[col] != TEXTOID)
                throw Database::Error("StatementBase::bind(" + lexical_cast<string>(arg) + ", " + lexical_cast<string>(col) + ") : binding a string to an oid # : " + lexical_cast<string>(_->params[col]));
            
            for (size_t i = 0; i < arg.size(); ++i)
                _->values[col].push_back(arg[i]);
        }
        else {
            int ret = sqlite3_bind_text(_->stmt, col, arg.c_str(), -1, SQLITE_TRANSIENT);
            if (ret != SQLITE_OK)
                throw Database::Error("StatementBase::bind(text) : error " + lexical_cast<string>(ret) + " binding a value");
        }
    }
    void StatementBase::bind(const blob& arg, int col) const {
        if (isPSQL()) {
            if (--col >= _->values.size())
                throw Database::Error("StatementBase::bind( BLOB, " + lexical_cast<string>(col) + ") : too many params - max is : " + lexical_cast<string>(_->values.size()-1));
            
            if (_->params[col] != BYTEAOID)
                throw Database::Error("StatementBase::bind( BLOB, " + lexical_cast<string>(col) + ") : binding a blob to an oid # : " + lexical_cast<string>(_->params[col]));
            
            for (size_t i = 0; i < arg.size(); ++i)
                _->values[col].push_back(arg[i]);
        }
        else {
            int ret = sqlite3_bind_blob(_->stmt, col, (const char*)&arg.front(), arg.size(), SQLITE_TRANSIENT);
            if (ret != SQLITE_OK)
                throw Database::Error("StatementBase::bind(blob) : error " + lexical_cast<string>(ret) + " binding a value");
        }
    }
    void StatementBase::bind(const uint256& arg, int col) const {
        blob b((const char*) arg.begin(), (const char*) arg.end());
        reverse(b.begin(), b.end());
        bind(b, col);
    }
    void StatementBase::bind(const uint160& arg, int col) const {
        blob b((const char*) arg.begin(), (const char*) arg.end());
        reverse(b.begin(), b.end());
        bind(b, col);
    }
    
    void StatementBase::pgsql_exec() {
        std::vector<char*> values;
        std::vector<int> lengths;
        std::vector<int> formats;
        for (size_t i = 0; i < _->values.size(); ++i) {
            values.push_back(_->values[i].data());
            lengths.push_back(_->values[i].size());
            formats.push_back(1); // 1 for binary
        }
        
        _->row = 0; // reset the row pointer
        
        _->pgres = PQexecPrepared(_->pgsql, _->sql.c_str(), _->values.size(), values.data(), lengths.data(), formats.data(), 1);
        
        // checking for errors and throw
        ExecStatusType status = PQresultStatus(_->pgres);
        if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
            std::string error = PQresultErrorMessage(_->pgres);
            PQclear(_->pgres);
            throw Database::Error("StatementBase::execute(" + _->sql + ") : " + string(PQresStatus(status)) + ", " + error);
        }
    }
    
    void StatementBase::get(int64_t& arg, int col) const {
        if (isPSQL()) {
            if (col >= _->fields.size())
                throw Database::Error("StatementBase::get(int64_t, " + lexical_cast<string>(col) + ") : too many params - max is : " + lexical_cast<string>(_->fields.size()-1));
            
            if (_->fields[col] != INT8OID)
                throw Database::Error("StatementBase::get(int64_t, " + lexical_cast<string>(col) + ") : binding an int64_t to an oid # : " + lexical_cast<string>(_->params[col]));
            
            
            if (PQntuples(_->pgres) == 0 || PQgetisnull(_->pgres, _->row, col)) {
                arg = 0;
                return;
            }
            
            const char* p = PQgetvalue(_->pgres, _->row, col);
            int imax = PQgetlength(_->pgres, _->row, col) - 1;
            
            for (int i = 0; i <= imax; ++i)
                ((char*)&arg)[i] = p[imax-i];
        }
        else {
            int storage_class = sqlite3_column_type(_->stmt, col);
            if (storage_class == SQLITE_NULL) {
                arg = 0;
                return;
            }
            if (storage_class != SQLITE_INTEGER)
                throw Database::Error("StatementBase::get(" + lexical_cast<string>(col) + ") : storage class error, expected SQLITE_INTEGER got: " + lexical_cast<string>(storage_class));
            arg = sqlite3_column_int64(_->stmt, col);
        }
    }
    void StatementBase::get(double& arg, int col) const {
        if (isPSQL()) {
            if (col >= _->fields.size())
                throw Database::Error("StatementBase::get(double, " + lexical_cast<string>(col) + ") : too many params - max is : " + lexical_cast<string>(_->fields.size()-1));
            
            if (_->fields[col] != FLOAT8OID)
                throw Database::Error("StatementBase::get(double, " + lexical_cast<string>(col) + ") : binding a double to an oid # : " + lexical_cast<string>(_->params[col]));
            
            
            if (PQntuples(_->pgres) == 0 || PQgetisnull(_->pgres, _->row, col)) {
                arg = 0.;
                return;
            }
            
            const char* p = PQgetvalue(_->pgres, _->row, col);
            int imax = PQgetlength(_->pgres, _->row, col) - 1;
            
            for (int i = 0; i <= imax; ++i)
                ((char*)&arg)[i] = p[imax-i];
        }
        else {
            int storage_class = sqlite3_column_type(_->stmt, col);
            if (storage_class != SQLITE_FLOAT)
                throw Database::Error("StatementBase::get(" + lexical_cast<string>(col) + ") : storage class error, expected SQLITE_FLOAT got: " + lexical_cast<string>(storage_class));
            arg = sqlite3_column_double(_->stmt, col);
        }
    }
    void StatementBase::get(std::string& arg, int col) const {
        if (isPSQL()) {
            if (col >= _->fields.size())
                throw Database::Error("StatementBase::get(string, " + lexical_cast<string>(col) + ") : too many params - max is : " + lexical_cast<string>(_->fields.size()-1));
            
            if (_->fields[col] != TEXTOID)
                throw Database::Error("StatementBase::get(string, " + lexical_cast<string>(col) + ") : binding a string to an oid # : " + lexical_cast<string>(_->params[col]));
            
            
            if (PQntuples(_->pgres) == 0 || PQgetisnull(_->pgres, _->row, col)) {
                arg = "";
                return;
            }
            
            const char* p = PQgetvalue(_->pgres, _->row, col);
            
            arg = p;
        }
        else {
            int storage_class = sqlite3_column_type(_->stmt, col);
            if (storage_class == SQLITE_NULL) {
                arg.clear();
                return;
            }
            if (storage_class != SQLITE3_TEXT)
                throw Database::Error("StatementBase::get(" + lexical_cast<string>(col) + ") : storage class error, expected SQLITE_TEXT got: " + lexical_cast<string>(storage_class));
            arg = string((const char*)sqlite3_column_text(_->stmt, col));
        }
    }
    void StatementBase::get(blob& arg, int col) const {
        if (isPSQL()) {
            if (col >= _->fields.size())
                throw Database::Error("StatementBase::get(blob, " + lexical_cast<string>(col) + ") : too many params - max is : " + lexical_cast<string>(_->fields.size()-1));
            
            if (_->fields[col] != BYTEAOID)
                throw Database::Error("StatementBase::get(blob, " + lexical_cast<string>(col) + ") : binding a double to an oid # : " + lexical_cast<string>(_->params[col]));
            
            
            if (PQntuples(_->pgres) == 0 || PQgetisnull(_->pgres, _->row, col)) {
                arg.clear();
                return;
            }
            
            const char* p = PQgetvalue(_->pgres, _->row, col);
            int l = PQgetlength(_->pgres, _->row, col);
            
            arg.assign(p, p+l);
        }
        else {
            int storage_class = sqlite3_column_type(_->stmt, col);
            if (storage_class == SQLITE_NULL) {
                arg.clear();
                return;
            }
            if (storage_class != SQLITE_BLOB)
                throw Database::Error("StatementBase::get(" + lexical_cast<string>(col) + ") : storage class error, expected SQLITE_BLOB got: " + lexical_cast<string>(storage_class));
            int bytes = sqlite3_column_bytes(_->stmt, col);
            unsigned char* data = (unsigned char*)sqlite3_column_blob(_->stmt, col);
            arg = blob(data, data + bytes);
        }
    }
    void StatementBase::get(uint256& arg, int col) const {
        vector<unsigned char> a;
        get(a, col);
        reverse(a.begin(), a.end());
        arg = uint256(a);
    }
    void StatementBase::get(uint160& arg, int col) const {
        vector<unsigned char> a;
        get(a, col);
        reverse(a.begin(), a.end());
        arg = uint160(a);
    }
    
    void StatementBase::pqsql_reset() {
        PQclear(_->pgres);
        _->row = 0;
        _->values.clear();
        _->values.resize(_->params.size());
    }
    
    Database::Database(const string uri, const Dictionary dict, int64_t heap_limit) : _pgsql(NULL), _sqlite(NULL), _dict(dict) {
        // check if this is postgresql or sqlite - if the uri starts with 'postgresql://' we assume postgresql
        if (uri.find("postgresql://") != string::npos) { // postgresql
            log_info("logging into database uri %s", uri);
            _pgsql = PQconnectdb(uri.c_str());
            if (PQstatus(_pgsql) != CONNECTION_OK)
                throw Database::Error("Database() : error " + lexical_cast<string>(PQstatus(_pgsql)) + " opening database environment");
            _dict["INTEGER"] = "INT8";
            _dict["BINARY"] = "BYTEA";
            _dict["CREATE INDEX IF NOT EXISTS"] = "CREATE INDEX";
            _dict["?"] = "$";
            _dict["SERIAL PRIMARY KEY"] = "SERIAL8 PRIMARY KEY";
            _dict["PRAGMA"] = "SELECT 0 -- PRAGMA"; // this will comment out PRAGMAS for postgresql
        }
        else { // sqlite
            static bool configured = false;
            if (!configured) {
                configured = true;
                int ret = sqlite3_config(SQLITE_CONFIG_SERIALIZED);
                if (ret != SQLITE_OK)
                    throw Database::Error("Database() : error " + lexical_cast<string>(ret) + " configuring database environment");
            }
            int ret = sqlite3_open(uri.c_str(), &_sqlite);
            if (ret != SQLITE_OK)
                throw Database::Error("Database() : error " + lexical_cast<string>(ret) + " opening database environment");
            
            sqlite3_soft_heap_limit64(heap_limit);
            
            /* Note: In the odd case that registerFunctions throws an error,
             the DB handle in _db won't be destroyed correctly.  Not sure
             if this is a problem or not.  */
            //registerFunctions();
            
            _dict["SERIAL PRIMARY KEY"] = "INTEGER PRIMARY KEY";
            _dict["RETURNING"] = "-- RETURNING";
            
        }
    }
    
    Database::~Database() {
        _statements.clear(); // this will unref all statements and force them to call finalize
        if (_pgsql) PQfinish(_pgsql);
        _pgsql = NULL;
        if (_sqlite) sqlite3_close(_sqlite);
        _sqlite = NULL;
    }
    
    string Database::search_and_replace(string str) const {
        for (Dictionary::const_iterator translation = _dict.begin(); translation != _dict.end(); ++translation)
            replace_all(str, translation->first, translation->second);
        
        return str;
    }
    
    StatementBase Database::statement(const char* stmt) {
        if (isPSQL()) {
            return ((const Database*)this)->statement(stmt);
        }
        else {
            // lookup if we already have it
            if (_statements.count(stmt))
                return _statements[stmt];
            StatementBase s;
            int ret = sqlite3_prepare_v2(_sqlite, stmt, -1, &s._->stmt, NULL) ;
            if (ret != SQLITE_OK)
                throw Database::Error("Database::query() Code:" + lexical_cast<string>(ret) + ", Message: \"" + sqlite3_errmsg(_sqlite) + "\" preparing statement: " + stmt);
            s._->sql = stmt;
            _statements[stmt] = s;
            return s;
        }
    }
    
    StatementBase Database::statement(const char* stmt) const {
        if (isPSQL()) {
            // lookup if we already have it
            if (_statements.count(stmt))
                return _statements[stmt];
            StatementBase s;
            
            PGresult* res = PQprepare(_pgsql, stmt, search_and_replace(stmt).c_str(), 0, NULL);
            ExecStatusType status = PQresultStatus(res);
            if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
                std::string error = PQresultErrorMessage(res);
                PQclear(res);
                throw Database::Error("StatementBase::statement(" + string(stmt) + ") : in preparing : " + error);
            }
            PQclear(res);
            
            res = PQdescribePrepared(_pgsql, stmt);
            if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
                std::string error = PQresultErrorMessage(res);
                PQclear(res);
                throw Database::Error("StatementBase::statement(" + string(stmt) + ") : in describing : " + error);
            }
            
            int params = PQnparams(res);
            for (int i = 0; i < params; ++i)
                s._->params.push_back(PQparamtype(res, i));
            
            int fields = PQnfields(res);
            for (int i = 0; i < fields; ++i)
                s._->fields.push_back(PQftype(res, i));
            
            PQclear(res);
            
            s._->sql = stmt;
            s._->pgsql = _pgsql;
            _statements[stmt] = s;
            return s;
        }
        else {
            StatementBase s;
            // lookup if we already have it
            if (_statements.count(stmt))
                s = _statements[stmt];
            else {
                int ret = sqlite3_prepare_v2(_sqlite, stmt, -1, &s._->stmt, NULL) ;
                if (ret != SQLITE_OK)
                    throw Database::Error("Database::query() Code:" + lexical_cast<string>(ret) + ", Message: \"" + sqlite3_errmsg(_sqlite) + "\" preparing statement: " + stmt);
                s._->sql = stmt;
                _statements[stmt] = s;
            }
            if (sqlite3_stmt_readonly(s._->stmt) == 0)
                throw Error("StatementVec::eval() const: error only readonly statements can be executed through the const interface");
            return s;
        }
    }
    
    const std::string Database::error_text() const {
        if (isPSQL()) return PQerrorMessage(_pgsql);
        return sqlite3_errmsg(_sqlite);
    }

    const std::string Database::statistics() const {
        typedef map<int64_t, string> Stats;
        Stats stats;
        int64_t total_time = 0;
        int64_t total_count = 0;
        for (Statements::const_iterator is = _statements.begin(); is != _statements.end(); ++is) {
            const StatementBase& s = is->second;
            if (s.stat_count()) {
                double avg = 1./s.stat_count()*s.stat_time();
                stats[s.stat_time()] = cformat("%9.3fs / #%6d = %6.3fus : \"%s\"", 0.000001*s.stat_time(), s.stat_count(), avg, s.sql()).text();
                total_time += s.stat_time();
                total_count += s.stat_count();
            }
        }
        double avg = 1./total_count*total_time;
        stats[total_time] = cformat("%9.3fs / #%6d = %6.3fus : \"%s\"", 0.000001*total_time, total_count, avg, "TOTAL").text();
        stringstream oss;
        oss << "\nTOTAL TIME / #CALLS = AVERAGE_TIME : SQL_STATEMENT\n";
        for (Stats::const_reverse_iterator s = stats.rbegin(); s != stats.rend(); ++s) {
            oss << cformat("%4.1f% ", 100.*s->first/total_time).text() << s->second << "\n";
        }
        oss << std::endl;
        return oss.str();
    }
    
    /* Register user-defined functions in SQLite.  */
    
    static std::string sqliteBlobToString (sqlite3_value* val) {
        const unsigned len = sqlite3_value_bytes(val);
        const char* data = reinterpret_cast<const char*>(sqlite3_value_blob(val));
        return std::string(data, data + len);
    }
    
    static void fcn_regexp(sqlite3_context* context, int argc, sqlite3_value** argv) {
        if (argc != 2)
            return;
        
        const std::string subject = sqliteBlobToString(argv[0]);
        const std::string pattern = sqliteBlobToString(argv[1]);
        
        try
        {
            const xpressive::sregex regex = xpressive::sregex::compile(pattern);
            xpressive::smatch matches;
            
            if (xpressive::regex_search(subject, matches, regex))
                sqlite3_result_int(context, 1);
            else
                sqlite3_result_int(context, 0);
        } catch(...)
        {
            std::ostringstream msg;
            msg << "error parsing regex: " << pattern;
            sqlite3_result_error(context, msg.str().c_str(), msg.str().size());
        }
    }
    
    void Database::registerFunctions() {
        if (isPSQL()) return;
        int ret;
        
        /* We could declare the function to be SQLITE_DETERMINISTIC, but
         this constant seems to be undefined on some systems.  */
        ret = sqlite3_create_function(_sqlite, "regexp", 2, SQLITE_UTF8, NULL, &fcn_regexp, NULL, NULL);
        if (ret != SQLITE_OK)
            throw Database::Error("Database() : error " + lexical_cast<std::string>(ret) + " registering function 'regexp'");
    }
}
