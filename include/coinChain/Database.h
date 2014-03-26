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

#ifndef _DATABASE_H
#define _DATABASE_H

#include <coin/uint256.h>

#include <sqlite3.h>

#include <boost/date_time/posix_time/posix_time_types.hpp>

#include <map>
#include <string>
#include <vector>
#include <stdexcept>

namespace sqliterate {
    
    struct undefined {};
    
    namespace detail {
        
        template<typename Ctor> struct ctor_traits_helper;
        
        template<class C, class T0>
        struct ctor_traits_helper<C (*)(T0)> {
            typedef C class_type;
            typedef T0 arg0_type;
            typedef undefined arg1_type;
            typedef undefined arg2_type;
            typedef undefined arg3_type;
            typedef undefined arg4_type;
            typedef undefined arg5_type;
            typedef undefined arg6_type;
            typedef undefined arg7_type;
            typedef undefined arg8_type;
            typedef undefined arg9_type;
        };
        template<class C, class T0, class T1>
        struct ctor_traits_helper<C (*)(T0, T1)> {
            typedef C class_type;
            typedef T0 arg0_type;
            typedef T1 arg1_type;
            typedef undefined arg2_type;
            typedef undefined arg3_type;
            typedef undefined arg4_type;
            typedef undefined arg5_type;
            typedef undefined arg6_type;
            typedef undefined arg7_type;
            typedef undefined arg8_type;
            typedef undefined arg9_type;
        };
        template<class C, class T0, class T1, class T2>
        struct ctor_traits_helper<C (*)(T0, T1, T2)> {
            typedef C class_type;
            typedef T0 arg0_type;
            typedef T1 arg1_type;
            typedef T2 arg2_type;
            typedef undefined arg3_type;
            typedef undefined arg4_type;
            typedef undefined arg5_type;
            typedef undefined arg6_type;
            typedef undefined arg7_type;
            typedef undefined arg8_type;
            typedef undefined arg9_type;
        };
        template<class C, class T0, class T1, class T2, class T3>
        struct ctor_traits_helper<C (*)(T0, T1, T2, T3)> {
            typedef C class_type;
            typedef T0 arg0_type;
            typedef T1 arg1_type;
            typedef T2 arg2_type;
            typedef T3 arg3_type;
            typedef undefined arg4_type;
            typedef undefined arg5_type;
            typedef undefined arg6_type;
            typedef undefined arg7_type;
            typedef undefined arg8_type;
            typedef undefined arg9_type;
        };
        template<class C, class T0, class T1, class T2, class T3, class T4>
        struct ctor_traits_helper<C (*)(T0, T1, T2, T3, T4)> {
            typedef C class_type;
            typedef T0 arg0_type;
            typedef T1 arg1_type;
            typedef T2 arg2_type;
            typedef T3 arg3_type;
            typedef T4 arg4_type;
            typedef undefined arg5_type;
            typedef undefined arg6_type;
            typedef undefined arg7_type;
            typedef undefined arg8_type;
            typedef undefined arg9_type;
        };
        template<class C, class T0, class T1, class T2, class T3, class T4, class T5>
        struct ctor_traits_helper<C (*)(T0, T1, T2, T3, T4, T5)> {
            typedef C class_type;
            typedef T0 arg0_type;
            typedef T1 arg1_type;
            typedef T2 arg2_type;
            typedef T3 arg3_type;
            typedef T4 arg4_type;
            typedef T5 arg5_type;
            typedef undefined arg6_type;
            typedef undefined arg7_type;
            typedef undefined arg8_type;
            typedef undefined arg9_type;
        };
        template<class C, class T0, class T1, class T2, class T3, class T4, class T5, class T6>
        struct ctor_traits_helper<C (*)(T0, T1, T2, T3, T4, T5, T6)> {
            typedef C class_type;
            typedef T0 arg0_type;
            typedef T1 arg1_type;
            typedef T2 arg2_type;
            typedef T3 arg3_type;
            typedef T4 arg4_type;
            typedef T5 arg5_type;
            typedef T6 arg6_type;
            typedef undefined arg7_type;
            typedef undefined arg8_type;
            typedef undefined arg9_type;
        };
        template<class C, class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7>
        struct ctor_traits_helper<C (*)(T0, T1, T2, T3, T4, T5, T6, T7)> {
            typedef C class_type;
            typedef T0 arg0_type;
            typedef T1 arg1_type;
            typedef T2 arg2_type;
            typedef T3 arg3_type;
            typedef T4 arg4_type;
            typedef T5 arg5_type;
            typedef T6 arg6_type;
            typedef T7 arg7_type;
            typedef undefined arg8_type;
            typedef undefined arg9_type;
        };
        template<class C, class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
        struct ctor_traits_helper<C (*)(T0, T1, T2, T3, T4, T5, T6, T7, T8)> {
            typedef C class_type;
            typedef T0 arg0_type;
            typedef T1 arg1_type;
            typedef T2 arg2_type;
            typedef T3 arg3_type;
            typedef T4 arg4_type;
            typedef T5 arg5_type;
            typedef T6 arg6_type;
            typedef T7 arg7_type;
            typedef T8 arg8_type;
            typedef undefined arg9_type;
        };
        template<class C, class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9>
        struct ctor_traits_helper<C (*)(T0, T1, T2, T3, T4, T5, T6, T7, T8, T9)> {
            typedef C class_type;
            typedef T0 arg0_type;
            typedef T1 arg1_type;
            typedef T2 arg2_type;
            typedef T3 arg3_type;
            typedef T4 arg4_type;
            typedef T5 arg5_type;
            typedef T6 arg6_type;
            typedef T7 arg7_type;
            typedef T8 arg8_type;
            typedef T9 arg9_type;
        };
        
    } // end namespace detail
    
    template <class T> struct remove_reference        {typedef T type;};
    template <class T> struct remove_reference<T&>  {typedef T type;};
    template <class T> struct add_pointer {typedef typename remove_reference<T>::type* type;};
    
    template<typename Ctor>
    struct ctor_traits :
    public detail::ctor_traits_helper<typename add_pointer<Ctor>::type>
    {
    };
    
    template <class C, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8, class P9>
    struct Construct {
        static C construct(P0 p0, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8, P9 p9) {
            return C(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9);
        }
    };
    template <class C, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8>
    struct Construct<C, P0, P1, P2, P3, P4, P5, P6, P7, P8, undefined> {
        static C construct(P0 p0, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8, undefined p9) {
            return C(p0, p1, p2, p3, p4, p5, p6, p7, p8);
        }
    };
    template <class C, class P0, class P1, class P2, class P3, class P4, class P5, class P6, class P7>
    struct Construct<C, P0, P1, P2, P3, P4, P5, P6, P7, undefined, undefined> {
        static C construct(P0 p0, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, undefined p8, undefined p9) {
            return C(p0, p1, p2, p3, p4, p5, p6, p7);
        }
    };
    template <class C, class P0, class P1, class P2, class P3, class P4, class P5, class P6>
    struct Construct<C, P0, P1, P2, P3, P4, P5, P6, undefined, undefined, undefined> {
        static C construct(P0 p0, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, undefined p7, undefined p8, undefined p9) {
            return C(p0, p1, p2, p3, p4, p5, p6);
        }
    };
    template <class C, class P0, class P1, class P2, class P3, class P4, class P5>
    struct Construct<C, P0, P1, P2, P3, P4, P5, undefined, undefined, undefined, undefined> {
        static C construct(P0 p0, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, undefined p6, undefined p7, undefined p8, undefined p9) {
            return C(p0, p1, p2, p3, p4, p5);
        }
    };
    template <class C, class P0, class P1, class P2, class P3, class P4>
    struct Construct<C, P0, P1, P2, P3, P4, undefined, undefined, undefined, undefined, undefined> {
        static C construct(P0 p0, P1 p1, P2 p2, P3 p3, P4 p4, undefined p5, undefined p6, undefined p7, undefined p8, undefined p9) {
            return C(p0, p1, p2, p3, p4);
        }
    };
    template <class C, class P0, class P1, class P2, class P3>
    struct Construct<C, P0, P1, P2, P3, undefined, undefined, undefined, undefined, undefined, undefined> {
        static C construct(P0 p0, P1 p1, P2 p2, P3 p3, undefined p4, undefined p5, undefined p6, undefined p7, undefined p8, undefined p9) {
            return C(p0, p1, p2, p3);
        }
    };
    template <class C, class P0, class P1, class P2>
    struct Construct<C, P0, P1, P2, undefined, undefined, undefined, undefined, undefined, undefined, undefined> {
        static C construct(P0 p0, P1 p1, P2 p2, undefined p3, undefined p4, undefined p5, undefined p6, undefined p7, undefined p8, undefined p9) {
            return C(p0, p1, p2);
        }
    };
    template <class C, class P0, class P1>
    struct Construct<C, P0, P1, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined> {
        static C construct(P0 p0, P1 p1, undefined p2, undefined p3, undefined p4, undefined p5, undefined p6, undefined p7, undefined p8, undefined p9) {
            return C(p0, p1);
        }
    };
    template <class C, class P0>
    struct Construct<C, P0, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined> {
        static C construct(P0 p0, undefined p1, undefined p2, undefined p3, undefined p4, undefined p5, undefined p6, undefined p7, undefined p8, undefined p9) {
            return C(p0);
        }
    };
    
    typedef std::vector<unsigned char> blob;
    
    class Database;
    
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
        
        int64_t stat_time() const { return _->stat_time; }
        int64_t stat_count() const { return _->stat_count; }
        
        void bind(int64_t arg, int col) const;
        void bind(uint64_t arg, int col) const { bind((int64_t)arg, col); }
        void bind(int32_t arg, int col) const { bind((int64_t)arg, col); }
        void bind(uint32_t arg, int col) const { bind((int64_t)arg, col); }
        void bind(double arg, int col) const;
        void bind(const std::string& arg, int col) const;
        void bind(const blob& arg, int col) const;
        void bind(const uint256& arg, int col) const;
        void bind(const uint160& arg, int col) const;
        
        sqlite3_stmt *stmt() { return _->stmt; }
    protected:
        void get(int64_t& arg, int col) const;
        void get(int& arg, int col) const { int64_t a; get((a), col); arg = a;}
        void get(unsigned int& arg, int col) const { int64_t a; get((a), col); arg = a;}
        void get(unsigned short& arg, int col) const { int64_t a; get((a), col); arg = a;}
        
        void get(double& arg, int col) const;
        void get(std::string& arg, int col) const;
        void get(blob& arg, int col) const;
        void get(uint256& arg, int col) const;
        void get(uint160& arg, int col) const;

        void get(undefined, int) const {}
        
        const std::string& sql() const { return _->sql; }
        
        inline int64_t time_microseconds() const {
            return (boost::posix_time::ptime(boost::posix_time::microsec_clock::universal_time()) -
                    boost::posix_time::ptime(boost::gregorian::date(1970,1,1))).total_microseconds();
        }
        
    protected:
        class Representation {
        public:
            sqlite3_stmt *stmt;
            std::string sql;
            mutable int64_t stat_time;
            mutable int64_t stat_count;
            
            Representation() : stmt(NULL), stat_time(0), stat_count(0), _refs(0) {}
            
            int ref() { return ++_refs; }
            int unref() { return --_refs; }
        private:
            int _refs;
        };
        Representation* _;
    };
    
    template <class R>
    class Statement : public StatementBase {
    public:
        Statement() : StatementBase() {}
        Statement(const StatementBase& stmt) : StatementBase(stmt) {}
        
        R eval() {
            int64_t t0 = time_microseconds();
            R r = R();
            if (sqlite3_step(_->stmt) == SQLITE_ROW)
                get(r, 0);
            sqlite3_reset(_->stmt);
            _->stat_time += time_microseconds() - t0;
            ++_->stat_count;
            return r;
        }
    };
    
    struct RowId {
        int64_t rowid;
        RowId(int64_t i) : rowid(i) {}
        operator int64_t() { return rowid; }
    };
    
    // specialization for RowId
    template<>
    class Statement<RowId>  : public StatementBase {
    public:
        Statement() : StatementBase() {}
        Statement(const StatementBase& stmt) : StatementBase(stmt) {}
        
        RowId eval() {
            int64_t t0 = time_microseconds();
            while(sqlite3_step(_->stmt) == SQLITE_ROW);
            int64_t last_id = sqlite3_last_insert_rowid(sqlite3_db_handle(_->stmt));
            sqlite3_reset(_->stmt);
            _->stat_time += time_microseconds() - t0;
            ++_->stat_count;
            return last_id;
        }
    };
    
    template <class R>
    class StatementCol : public StatementBase {
    public:
        StatementCol() : StatementBase() {}
        StatementCol(const StatementBase& stmt) : StatementBase(stmt) {}
        
        std::vector<R> eval() {
            int64_t t0 = time_microseconds();
            int result = sqlite3_step(_->stmt);
            std::vector<R> rs;
            while(result == SQLITE_ROW) {
                R r;
                get(r, 0);
                rs.push_back(r);
                result = sqlite3_step(_->stmt);
            }
            sqlite3_reset(_->stmt);
            _->stat_time += time_microseconds() - t0;
            ++_->stat_count;
            return rs;
        }
    };
    
    template <class Ctor>
    class StatementRow : public StatementBase {
    public:
        typedef typename ctor_traits<Ctor>::class_type C;
        typedef typename ctor_traits<Ctor>::arg0_type P0;
        typedef typename ctor_traits<Ctor>::arg1_type P1;
        typedef typename ctor_traits<Ctor>::arg2_type P2;
        typedef typename ctor_traits<Ctor>::arg3_type P3;
        typedef typename ctor_traits<Ctor>::arg4_type P4;
        typedef typename ctor_traits<Ctor>::arg5_type P5;
        typedef typename ctor_traits<Ctor>::arg6_type P6;
        typedef typename ctor_traits<Ctor>::arg7_type P7;
        typedef typename ctor_traits<Ctor>::arg8_type P8;
        typedef typename ctor_traits<Ctor>::arg9_type P9;
        
        StatementRow() : StatementBase() {}
        StatementRow(const StatementBase& stmt) : StatementBase(stmt) {}
        
        C eval() {
            int64_t t0 = time_microseconds();
            C c;
            if (sqlite3_step(_->stmt) == SQLITE_ROW) {
                P0 p0; P1 p1; P2 p2; P3 p3; P4 p4; P5 p5; P6 p6; P7 p7; P8 p8; P9 p9;
                get(p0, 0); get(p1, 1); get(p2, 2); get(p3, 3); get(p4, 4); get(p5, 5); get(p6, 6); get(p7, 7); get(p8, 8); get(p9, 9);
                c = Construct<C, P0, P1, P2, P3, P4, P5, P6, P7, P8, P9>::construct(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9);
            }
            sqlite3_reset(_->stmt);
            _->stat_time += time_microseconds() - t0;
            ++_->stat_count;
            return c;
        }
    };
    
    template <class Ctor>
    class StatementColRow : public StatementBase {
    public:
        typedef typename ctor_traits<Ctor>::class_type C;
        typedef typename ctor_traits<Ctor>::arg0_type P0;
        typedef typename ctor_traits<Ctor>::arg1_type P1;
        typedef typename ctor_traits<Ctor>::arg2_type P2;
        typedef typename ctor_traits<Ctor>::arg3_type P3;
        typedef typename ctor_traits<Ctor>::arg4_type P4;
        typedef typename ctor_traits<Ctor>::arg5_type P5;
        typedef typename ctor_traits<Ctor>::arg6_type P6;
        typedef typename ctor_traits<Ctor>::arg7_type P7;
        typedef typename ctor_traits<Ctor>::arg8_type P8;
        typedef typename ctor_traits<Ctor>::arg9_type P9;
        
        StatementColRow() : StatementBase() {}
        StatementColRow(const StatementBase& stmt) : StatementBase(stmt) {}
        
        std::vector<C> eval() {
            int64_t t0 = time_microseconds();
            std::vector<C> cs;
            while (sqlite3_step(_->stmt) == SQLITE_ROW) {
                P0 p0; P1 p1; P2 p2; P3 p3; P4 p4; P5 p5; P6 p6; P7 p7; P8 p8; P9 p9;
                get(p0, 0); get(p1, 1); get(p2, 2); get(p3, 3); get(p4, 4); get(p5, 5); get(p6, 6); get(p7, 7); get(p8, 8); get(p9, 9);
                cs.push_back(Construct<C, P0, P1, P2, P3, P4, P5, P6, P7, P8, P9>::construct(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9));
            }
            sqlite3_reset(_->stmt);
            _->stat_time += time_microseconds() - t0;
            ++_->stat_count;
            return cs;
        }
    };
    
    class Database {
    public:
        class Error : public std::runtime_error {
        public:
            Error(const std::string& s) : std::runtime_error(s.c_str()) {}
        };
        
        Database(const std::string filename = ":memory:", int64_t heap_limit = 0);
        ~Database();
        
        StatementBase statement(const char* stmt);
        StatementBase statement(const char* stmt) const;
        
        /// The query template function takes as argument one of the standard sql types: integer, double, text, or blob and return that from a sql query.
        /// Up to 10 arguments to the query is supported.
        
        template<class R, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10>
        R query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10) {
            Statement<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8); s.bind(t9, 9); s.bind(t10, 10);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9>
        R query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9) {
            Statement<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8); s.bind(t9, 9);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
        R query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8) {
            Statement<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3, class T4, class T5, class T6, class T7>
        R query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7) {
            Statement<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3, class T4, class T5, class T6>
        R query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6) {
            Statement<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3, class T4, class T5>
        R query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5) {
            Statement<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3, class T4>
        R query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4) {
            Statement<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3>
        R query(const char* stmt, T1 t1, T2 t2, T3 t3) {
            Statement<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3);
            return s.eval();
        }
        template<class R, class T1, class T2>
        R query(const char* stmt, T1 t1, T2 t2) {
            Statement<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2);
            return s.eval();
        }
        template<class R, class T1>
        R query(const char* stmt, T1 t1) {
            Statement<R> s = statement(stmt);
            s.bind(t1, 1);
            return s.eval();
        }
        template<class R>
        R query(const char* stmt) {
            Statement<R> s = statement(stmt);
            return s.eval();
        }
        
        template<class R, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10>
        R query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10) const {
            Statement<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8); s.bind(t9, 9); s.bind(t10, 10);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9>
        R query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9) const {
            Statement<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8); s.bind(t9, 9);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
        R query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8) const {
            Statement<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3, class T4, class T5, class T6, class T7>
        R query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7) const {
            Statement<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3, class T4, class T5, class T6>
        R query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6) const {
            Statement<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3, class T4, class T5>
        R query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5) const {
            Statement<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3, class T4>
        R query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4) const {
            Statement<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3>
        R query(const char* stmt, T1 t1, T2 t2, T3 t3) const {
            Statement<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3);
            return s.eval();
        }
        template<class R, class T1, class T2>
        R query(const char* stmt, T1 t1, T2 t2) const {
            Statement<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2);
            return s.eval();
        }
        template<class R, class T1>
        R query(const char* stmt, T1 t1) const {
            Statement<R> s = statement(stmt);
            s.bind(t1, 1);
            return s.eval();
        }
        template<class R>
        R query(const char* stmt) const {
            Statement<R> s = statement(stmt);
            return s.eval();
        }
        
        /// The specialization of query with RowId returns the last inserted rowid.
        /// Up to 10 arguments to the query is supported.
        
        template<class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10>
        RowId query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10) {
            Statement<RowId> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8); s.bind(t9, 9); s.bind(t10, 10);
            return s.eval();
        }
        template<class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9>
        RowId query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9) {
            Statement<RowId> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8); s.bind(t9, 9);
            return s.eval();
        }
        template<class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
        RowId query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8) {
            Statement<RowId> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8);
            return s.eval();
        }
        template<class T1, class T2, class T3, class T4, class T5, class T6, class T7>
        RowId query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7) {
            Statement<RowId> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7);
            return s.eval();
        }
        template<class T1, class T2, class T3, class T4, class T5, class T6>
        RowId query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6) {
            Statement<RowId> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6);
            return s.eval();
        }
        template<class T1, class T2, class T3, class T4, class T5>
        RowId query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5) {
            Statement<RowId> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5);
            return s.eval();
        }
        template<class T1, class T2, class T3, class T4>
        RowId query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4) {
            Statement<RowId> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4);
            return s.eval();
        }
        template<class T1, class T2, class T3>
        RowId query(const char* stmt, T1 t1, T2 t2, T3 t3) {
            Statement<RowId> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3);
            return s.eval();
        }
        template<class T1, class T2>
        RowId query(const char* stmt, T1 t1, T2 t2) {
            Statement<RowId> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2);
            return s.eval();
        }
        template<class T1>
        RowId query(const char* stmt, T1 t1) {
            Statement<RowId> s = statement(stmt);
            s.bind(t1, 1);
            return s.eval();
        }
        RowId query(const char* stmt) {
            Statement<RowId> s = statement(stmt);
            return s.eval();
        }

        template<class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10>
        RowId query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10) const {
            Statement<RowId> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8); s.bind(t9, 9); s.bind(t10, 10);
            return s.eval();
        }
        template<class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9>
        RowId query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9) const {
            Statement<RowId> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8); s.bind(t9, 9);
            return s.eval();
        }
        template<class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
        RowId query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8) const {
            Statement<RowId> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8);
            return s.eval();
        }
        template<class T1, class T2, class T3, class T4, class T5, class T6, class T7>
        RowId query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7) const {
            Statement<RowId> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7);
            return s.eval();
        }
        template<class T1, class T2, class T3, class T4, class T5, class T6>
        RowId query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6) const {
            Statement<RowId> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6);
            return s.eval();
        }
        template<class T1, class T2, class T3, class T4, class T5>
        RowId query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5) const {
            Statement<RowId> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5);
            return s.eval();
        }
        template<class T1, class T2, class T3, class T4>
        RowId query(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4) const {
            Statement<RowId> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4);
            return s.eval();
        }
        template<class T1, class T2, class T3>
        RowId query(const char* stmt, T1 t1, T2 t2, T3 t3) const {
            Statement<RowId> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3);
            return s.eval();
        }
        template<class T1, class T2>
        RowId query(const char* stmt, T1 t1, T2 t2) const {
            Statement<RowId> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2);
            return s.eval();
        }
        template<class T1>
        RowId query(const char* stmt, T1 t1) const {
            Statement<RowId> s = statement(stmt);
            s.bind(t1, 1);
            return s.eval();
        }
        RowId query(const char* stmt) const {
            Statement<RowId> s = statement(stmt);
            return s.eval();
        }

        /// The queryCol template function takes as argument one of the standard sql types: integer, double, text, or blob and return a column, a std::vector<>, of these from a sql query.
        /// Up to 10 arguments to the query is supported.
        
        template<class R, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10>
        std::vector<R> queryCol(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10) {
            StatementCol<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8); s.bind(t9, 9); s.bind(t10, 10);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9>
        std::vector<R> queryCol(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9) {
            StatementCol<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8); s.bind(t9, 9);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
        std::vector<R> queryCol(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8) {
            StatementCol<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3, class T4, class T5, class T6, class T7>
        std::vector<R> queryCol(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7) {
            StatementCol<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3, class T4, class T5, class T6>
        std::vector<R> queryCol(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6) {
            StatementCol<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3, class T4, class T5>
        std::vector<R> queryCol(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5) {
            StatementCol<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3, class T4>
        std::vector<R> queryCol(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4) {
            StatementCol<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3>
        std::vector<R> queryCol(const char* stmt, T1 t1, T2 t2, T3 t3) {
            StatementCol<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3);
            return s.eval();
        }
        template<class R, class T1, class T2>
        std::vector<R> queryCol(const char* stmt, T1 t1, T2 t2) {
            StatementCol<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2);
            return s.eval();
        }
        template<class R, class T1>
        std::vector<R> queryCol(const char* stmt, T1 t1) {
            StatementCol<R> s = statement(stmt);
            s.bind(t1, 1);
            return s.eval();
        }
        template<class R>
        std::vector<R> queryCol(const char* stmt) {
            StatementCol<R> s = statement(stmt);
            return s.eval();
        }
        
        template<class R, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10>
        std::vector<R> queryCol(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10) const {
            StatementCol<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8); s.bind(t9, 9); s.bind(t10, 10);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9>
        std::vector<R> queryCol(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9) const {
            StatementCol<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8); s.bind(t9, 9);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
        std::vector<R> queryCol(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8) const {
            StatementCol<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3, class T4, class T5, class T6, class T7>
        std::vector<R> queryCol(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7) const {
            StatementCol<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3, class T4, class T5, class T6>
        std::vector<R> queryCol(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6) const {
            StatementCol<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3, class T4, class T5>
        std::vector<R> queryCol(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5) const {
            StatementCol<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3, class T4>
        std::vector<R> queryCol(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4) const {
            StatementCol<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4);
            return s.eval();
        }
        template<class R, class T1, class T2, class T3>
        std::vector<R> queryCol(const char* stmt, T1 t1, T2 t2, T3 t3) const {
            StatementCol<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3);
            return s.eval();
        }
        template<class R, class T1, class T2>
        std::vector<R> queryCol(const char* stmt, T1 t1, T2 t2) const {
            StatementCol<R> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2);
            return s.eval();
        }
        template<class R, class T1>
        std::vector<R> queryCol(const char* stmt, T1 t1) const {
            StatementCol<R> s = statement(stmt);
            s.bind(t1, 1);
            return s.eval();
        }
        template<class R>
        std::vector<R> queryCol(const char* stmt) const {
            StatementCol<R> s = statement(stmt);
            return s.eval();
        }

        /// The queryRow template function takes as argument a constructor signature with arguments as one of the sql types.
        /// The constructor supports up to 10 arguments and can hence be used to extract an entire row.
        /// Up to 10 arguments to the query is supported.
        template<class Ctor, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10>
        typename ctor_traits<Ctor>::class_type queryRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10) {
            StatementRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8); s.bind(t9, 9); s.bind(t10, 10);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9>
        typename ctor_traits<Ctor>::class_type queryRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9) {
            StatementRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8); s.bind(t9, 9);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
        typename ctor_traits<Ctor>::class_type queryRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8) {
            StatementRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3, class T4, class T5, class T6, class T7>
        typename ctor_traits<Ctor>::class_type queryRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7) {
            StatementRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3, class T4, class T5, class T6>
        typename ctor_traits<Ctor>::class_type queryRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6) {
            StatementRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3, class T4, class T5>
        typename ctor_traits<Ctor>::class_type queryRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5) {
            StatementRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3, class T4>
        typename ctor_traits<Ctor>::class_type queryRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4) {
            StatementRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3>
        typename ctor_traits<Ctor>::class_type queryRow(const char* stmt, T1 t1, T2 t2, T3 t3) {
            StatementRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3);
            return s.eval();
        }
        template<class Ctor, class T1, class T2>
        typename ctor_traits<Ctor>::class_type queryRow(const char* stmt, T1 t1, T2 t2) {
            StatementRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2);
            return s.eval();
        }
        template<class Ctor, class T1>
        typename ctor_traits<Ctor>::class_type queryRow(const char* stmt, T1 t1) {
            StatementRow<Ctor> s = statement(stmt);
            s.bind(t1, 1);
            return s.eval();
        }
        template<class Ctor>
        typename ctor_traits<Ctor>::class_type queryRow(const char* stmt) {
            StatementRow<Ctor> s = statement(stmt);
            return s.eval();
        }
        
        template<class Ctor, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10>
        typename ctor_traits<Ctor>::class_type queryRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10) const {
            StatementRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8); s.bind(t9, 9); s.bind(t10, 10);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9>
        typename ctor_traits<Ctor>::class_type queryRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9) const {
            StatementRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8); s.bind(t9, 9);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
        typename ctor_traits<Ctor>::class_type queryRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8) const {
            StatementRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3, class T4, class T5, class T6, class T7>
        typename ctor_traits<Ctor>::class_type queryRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7) const {
            StatementRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3, class T4, class T5, class T6>
        typename ctor_traits<Ctor>::class_type queryRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6) const {
            StatementRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3, class T4, class T5>
        typename ctor_traits<Ctor>::class_type queryRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5) const {
            StatementRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3, class T4>
        typename ctor_traits<Ctor>::class_type queryRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4) const {
            StatementRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3>
        typename ctor_traits<Ctor>::class_type queryRow(const char* stmt, T1 t1, T2 t2, T3 t3) const {
            StatementRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3);
            return s.eval();
        }
        template<class Ctor, class T1, class T2>
        typename ctor_traits<Ctor>::class_type queryRow(const char* stmt, T1 t1, T2 t2) const {
            StatementRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2);
            return s.eval();
        }
        template<class Ctor, class T1>
        typename ctor_traits<Ctor>::class_type queryRow(const char* stmt, T1 t1) const {
            StatementRow<Ctor> s = statement(stmt);
            s.bind(t1, 1);
            return s.eval();
        }
        template<class Ctor>
        typename ctor_traits<Ctor>::class_type queryRow(const char* stmt) const {
            StatementRow<Ctor> s = statement(stmt);
            return s.eval();
        }

        /// The queryColRow template function takes as argument a constructor signature with arguments as one of the sql types.
        /// The constructor supports up to 10 arguments and can hence be used to extract a column, std::vector<>, of rows.
        /// Up to 10 arguments to the query is supported.
        
        template<class Ctor, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10>
        std::vector<typename ctor_traits<Ctor>::class_type> queryColRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10) {
            StatementColRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8); s.bind(t9, 9); s.bind(t10, 10);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9>
        std::vector<typename ctor_traits<Ctor>::class_type> queryColRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9) {
            StatementColRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8); s.bind(t9, 9);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
        std::vector<typename ctor_traits<Ctor>::class_type> queryColRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8) {
            StatementColRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3, class T4, class T5, class T6, class T7>
        std::vector<typename ctor_traits<Ctor>::class_type> queryColRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7) {
            StatementColRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3, class T4, class T5, class T6>
        std::vector<typename ctor_traits<Ctor>::class_type> queryColRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6) {
            StatementColRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3, class T4, class T5>
        std::vector<typename ctor_traits<Ctor>::class_type> queryColRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5) {
            StatementColRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3, class T4>
        std::vector<typename ctor_traits<Ctor>::class_type> queryColRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4) {
            StatementColRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3>
        std::vector<typename ctor_traits<Ctor>::class_type> queryColRow(const char* stmt, T1 t1, T2 t2, T3 t3) {
            StatementColRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3);
            return s.eval();
        }
        template<class Ctor, class T1, class T2>
        std::vector<typename ctor_traits<Ctor>::class_type> queryColRow(const char* stmt, T1 t1, T2 t2) {
            StatementColRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2);
            return s.eval();
        }
        template<class Ctor, class T1>
        std::vector< typename ctor_traits<Ctor>::class_type> queryColRow(const char* stmt, T1 t1) {
            StatementColRow<Ctor> s = statement(stmt);
            s.bind(t1, 1);
            return s.eval();
        }
        template<class Ctor>
        std::vector< typename ctor_traits<Ctor>::class_type> queryColRow(const char* stmt) {
            StatementColRow<Ctor> s = statement(stmt);
            return s.eval();
        }
        
        template<class Ctor, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10>
        std::vector<typename ctor_traits<Ctor>::class_type> queryColRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9, T10 t10) const {
            StatementColRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8); s.bind(t9, 9); s.bind(t10, 10);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9>
        std::vector<typename ctor_traits<Ctor>::class_type> queryColRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9) const {
            StatementColRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8); s.bind(t9, 9);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
        std::vector<typename ctor_traits<Ctor>::class_type> queryColRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8) const {
            StatementColRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7); s.bind(t8, 8);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3, class T4, class T5, class T6, class T7>
        std::vector<typename ctor_traits<Ctor>::class_type> queryColRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7) const {
            StatementColRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6); s.bind(t7, 7);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3, class T4, class T5, class T6>
        std::vector<typename ctor_traits<Ctor>::class_type> queryColRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6) const {
            StatementColRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5); s.bind(t6, 6);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3, class T4, class T5>
        std::vector<typename ctor_traits<Ctor>::class_type> queryColRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5) const {
            StatementColRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4); s.bind(t5, 5);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3, class T4>
        std::vector<typename ctor_traits<Ctor>::class_type> queryColRow(const char* stmt, T1 t1, T2 t2, T3 t3, T4 t4) const {
            StatementColRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3); s.bind(t4, 4);
            return s.eval();
        }
        template<class Ctor, class T1, class T2, class T3>
        std::vector<typename ctor_traits<Ctor>::class_type> queryColRow(const char* stmt, T1 t1, T2 t2, T3 t3) const {
            StatementColRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2); s.bind(t3, 3);
            return s.eval();
        }
        template<class Ctor, class T1, class T2>
        std::vector<typename ctor_traits<Ctor>::class_type> queryColRow(const char* stmt, T1 t1, T2 t2) const {
            StatementColRow<Ctor> s = statement(stmt);
            s.bind(t1, 1); s.bind(t2, 2);
            return s.eval();
        }
        template<class Ctor, class T1>
        std::vector< typename ctor_traits<Ctor>::class_type> queryColRow(const char* stmt, T1 t1) const {
            StatementColRow<Ctor> s = statement(stmt);
            s.bind(t1, 1);
            return s.eval();
        }
        template<class Ctor>
        std::vector< typename ctor_traits<Ctor>::class_type> queryColRow(const char* stmt) const {
            StatementColRow<Ctor> s = statement(stmt);
            return s.eval();
        }

        const std::string error_text() const { return sqlite3_errmsg(_db); }
        
        const std::string statistics() const;
    private:
        sqlite3 *_db;
        typedef std::map<const char *, StatementBase> Statements;
        mutable Statements _statements;
    };
}

#endif
