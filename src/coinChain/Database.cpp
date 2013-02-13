/* -*-c++-*- libcoin - Copyright (C) 2012 Michael Gronager
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

using namespace std;
using namespace boost;

namespace sqliterate {
    
    void StatementBase::bind(sqlite3_int64 arg, int col) const {
        int ret = sqlite3_bind_int64(_->stmt, col, arg);
        if (ret != SQLITE_OK)
            throw Database::Error("StatementBase::bind(sqlite3_int64) : error " + lexical_cast<string>(ret) + " binding a value");
    }
    void StatementBase::bind(double arg, int col) const {
        int ret = sqlite3_bind_double(_->stmt, col, arg);
        if (ret != SQLITE_OK)
            throw Database::Error("StatementBase::bind(double) : error " + lexical_cast<string>(ret) + " binding a value");
    }
    void StatementBase::bind(const std::string& arg, int col) const {
        int ret = sqlite3_bind_text(_->stmt, col, arg.c_str(), -1, SQLITE_TRANSIENT);
        if (ret != SQLITE_OK)
            throw Database::Error("StatementBase::bind(text) : error " + lexical_cast<string>(ret) + " binding a value");
    }
    void StatementBase::bind(const blob& arg, int col) const {
        int ret = sqlite3_bind_blob(_->stmt, col, (const char*)&arg.front(), arg.size(), SQLITE_TRANSIENT);
        if (ret != SQLITE_OK)
            throw Database::Error("StatementBase::bind(blob) : error " + lexical_cast<string>(ret) + " binding a value");
    }
    void StatementBase::bind(const uint256& arg, int col) const {
        blob b((const char*) arg.begin(), (const char*) arg.end());
        reverse(b.begin(), b.end());
        bind(b, col);
    }
    
    
    void StatementBase::get(sqlite3_int64& arg, int col) const {
        int storage_class = sqlite3_column_type(_->stmt, col);
        if (storage_class != SQLITE_INTEGER)
            throw Database::Error("StatementBase::get(" + lexical_cast<string>(col) + ") : storage class error, expected SQLITE_INTEGER got: " + lexical_cast<string>(storage_class));
        arg = sqlite3_column_int64(_->stmt, col);
    }
    void StatementBase::get(double& arg, int col) const {
        int storage_class = sqlite3_column_type(_->stmt, col);
        if (storage_class != SQLITE_FLOAT)
            throw Database::Error("StatementBase::get(" + lexical_cast<string>(col) + ") : storage class error, expected SQLITE_FLOAT got: " + lexical_cast<string>(storage_class));
        arg = sqlite3_column_double(_->stmt, col);
    }
    void StatementBase::get(std::string& arg, int col) const {
        int storage_class = sqlite3_column_type(_->stmt, col);
        if (storage_class != SQLITE3_TEXT)
            throw Database::Error("StatementBase::get(" + lexical_cast<string>(col) + ") : storage class error, expected SQLITE_TEXT got: " + lexical_cast<string>(storage_class));
        arg = string((const char*)sqlite3_column_text(_->stmt, col));
    }
    void StatementBase::get(blob& arg, int col) const {
        int storage_class = sqlite3_column_type(_->stmt, col);
        if (storage_class != SQLITE_BLOB)
            throw Database::Error("StatementBase::get(" + lexical_cast<string>(col) + ") : storage class error, expected SQLITE_BLOB got: " + lexical_cast<string>(storage_class));
        int bytes = sqlite3_column_bytes(_->stmt, col);
        unsigned char* data = (unsigned char*)sqlite3_column_blob(_->stmt, col);
        arg = blob(data, data + bytes);
    }
    void StatementBase::get(uint256& arg, int col) const {
        vector<unsigned char> a;
        get(a, col);
        reverse(a.begin(), a.end());
        arg = uint256(a);
    }
    
    Database::Database(const string filename, sqlite3_int64 heap_limit) {
        _db = NULL;

        int ret = sqlite3_open(filename.c_str(), &_db);
        if (ret != SQLITE_OK)
            throw Database::Error("Database() : error " + lexical_cast<string>(ret) + " opening database environment");
        
        sqlite3_soft_heap_limit64(heap_limit);
    }
    
    Database::~Database() {
        _statements.clear(); // this will unref all statements and force them to call finalize
        if (_db) sqlite3_close(_db);
        _db = NULL;
    }
    
    StatementBase Database::statement(const char* stmt) {
        // lookup if we already have it
        if (_statements.count(stmt))
            return _statements[stmt];
        StatementBase s;
        int ret = sqlite3_prepare_v2(_db, stmt, -1, &s._->stmt, NULL) ;
        if (ret != SQLITE_OK)
            throw Database::Error("Database::query() Code:" + lexical_cast<string>(ret) + ", Message: \"" + sqlite3_errmsg(_db) + "\" preparing statement: " + stmt);
        s._->sql = stmt;
        _statements[stmt] = s;
        return s;
    }

    StatementBase Database::statement(const char* stmt) const {
        StatementBase s;
        // lookup if we already have it
        if (_statements.count(stmt))
            s = _statements[stmt];
        else {
            int ret = sqlite3_prepare_v2(_db, stmt, -1, &s._->stmt, NULL) ;
            if (ret != SQLITE_OK)
                throw Database::Error("Database::query() Code:" + lexical_cast<string>(ret) + ", Message: \"" + sqlite3_errmsg(_db) + "\" preparing statement: " + stmt);
            s._->sql = stmt;
            _statements[stmt] = s;
        }
        if (sqlite3_stmt_readonly(s.stmt()) == 0)
            throw Error("StatementVec::eval() const: error only readonly statements can be executed through the const interface");
        return s;
    }
    
    const std::string Database::statistics() const {
        typedef map<sqlite3_int64, string> Stats;
        Stats stats;
        sqlite3_int64 total_time = 0;
        sqlite3_int64 total_count = 0;
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
            oss << cformat("%4.1f%% ", 100.*s->first/total_time).text() << s->second << "\n";
        }
        oss << std::endl;
        return oss.str();
    }
}