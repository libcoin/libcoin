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


#include <map>
#include <vector>
#include <deque>
#include <iostream>
#include <boost/variant.hpp>

using namespace std;

/// The jsonance classes enables easy json creation, parsing and manipulation.
/// There are two basic classes: value and pair, from which all json expressions
/// can be build.
/// A value can be build like: value ival(123), sval("abc"), bval(true), nval(), fval(1.23); // (int)number, string, bool, null, (float)number
/// An object can be build like: value obj = (pair("id", 123), pair("name", "abc")); // { id : 123, name : "abc" }
/// An array can be build like: value arr = (value("abc", value(123)); // [ "abc", 123 ]
/// Also you can do: a[2] = 5; // [null, null, 5]
/// And: a["property"] = "red"; // { property : "red" }
/// And parse or output json as: value val; istream >> val; or ostream << val; or ostream << format("  ", "\n") << val;
/// Finally, you can do a schema validated parsing by: istream >> schema(json_val) >> val;
/*
namespace jsonance {
    class null {
    public:
        null() {}
    };
    
    class value {
    public:
        typedef int64_t integer;
        typedef map<std::string, value> object;
        typedef vector<value> array;
        
        value() : _(null()) {}
        value(const string& s) : _(s) {}
        value(const integer& i) : _(i) {}
        
        value& operator[](const string& name) {
            object& obj = boost::get<object>(_);
            return obj[name];
        }
        
        const value& operator[](const string& name) const {
            const object& obj = boost::get<object>(_);
            object::const_iterator p = obj.find(name);
            return p->second;
        }
        
        value& operator[](size_t idx) {
            array& arr = boost::get<array>(_);
            return arr[idx];
        }
        
        const value& operator[](size_t idx) const {
            const array& arr = boost::get<array>(_);
            return arr[idx];
        }
        
    private:
        boost::variant<null,
        boost::recursive_wrapper<string>,
        boost::recursive_wrapper<integer>,
        boost::recursive_wrapper<object>,
        boost::recursive_wrapper<array> > _;
    };
    
    class array_wrapper : public boost::static_visitor<value> {
    public:
    
        value operator()(string v) const {
            value arr;
            arr[0] = v;
            return v;
        }
    
        value operator()(value::integer v) const {
            value arr;
            arr[0] = v;
            return v;
        }
        
        value operator()(value::object v) const {
            value arr;
            //            arr[0] = v;
            //            return v;
        }
        
        value operator()(value::array v) const {
            //            return v;
        }
        
    };
    
    value operator,(value l, value r) {
        value arr = boost::apply_visitor(array_wrapper(), l);
        // three options : value a = (value("abc"), value(123)) -> ["abc", 123], but what about: value b = (a, a) ? is that ["abc", 123, "abc", 123] or [["abc",
        // solution is to introduce a element(value): value a = (value("abc"), value(123)), and (a, a) is one array, but (elem(a), elem(a)) is nested
        // so this operator will always concatenate into one array
        
    }
}

string test_vector =
    "[ \
        { \
            \"precision\": \"zip\", \
            \"Latitude\":  37.7668, \
            \"Longitude\": -122.3959, \
            \"Address\":   \"\", \
            \"City\":      \"SAN FRANCISCO\", \
            \"State\":     \"CA\", \
            \"Zip\":       \"94107\", \
            \"Country\":   \"US\" \
        }, \
        { \
            \"precision\": \"zip\", \
            \"Latitude\":  37.371991, \
            \"Longitude\": -122.026020, \
            \"Address\":   \"\", \
            \"City\":      \"SUNNYVALE\", \
            \"State\":     \"CA\", \
            \"Zip\":       \"94085\", \
            \"Country\":   \"US\" \
        } \
    ]";


/*
 
namespace json {
    class value {
    private:
        value parse(const string& json, const string& schema) const {
            // lets initially ignore the schema
            string test_vector =
            "[ \
                { \
                    \"precision\": \"zip\", \
                    \"Latitude\":  37.7668, \
                    \"Longitude\": -122.3959, \
                    \"Address\":   \"\", \
                    \"City\":      \"SAN FRANCISCO\", \
                    \"State\":     \"CA\", \
                    \"Zip\":       \"94107\", \
                    \"Country\":   \"US\" \
                }, \
                { \
                    \"precision\": \"zip\", \
                    \"Latitude\":  37.371991, \
                    \"Longitude\": -122.026020, \
                    \"Address\":   \"\", \
                    \"City\":      \"SUNNYVALE\", \
                    \"State\":     \"CA\", \
                    \"Zip\":       \"94085\", \
                    \"Country\":   \"US\" \
                } \
            ]";
            
            value root;
            deque<value> stack;
            //            ready_value
            
        }
    public:
        typedef int64_t integer;
        typedef map<std::string, value> object;
        typedef vector<value> array;
        
        struct type {
            enum id {
                string,
                integer,
                real,
                object,
                array,
                boolean,
                null
            };
        };
        
        value() : _type(type::null) {}
        
        value(const string json, const string schema) {
            //            (*this) = parse(json, schema);
        }
        value(const string& str) : _type(type::string), _string(str) {}
        value(const integer i) : _type(type::integer), _integer(i) {}
        
        value& operator[](const string& idx) {
            if (_type != type::object)
                throw;
            return _object[idx];
        }
        value& operator[](const char* idx) {
            return operator[](string(idx));
        }
        value& operator[](integer i) {
            if (_type != type::array)
                throw;
            return _array[i];
        }
        const value& operator[](integer i) const {
            if (_type != type::array)
                throw;
            return _array[i];
        }
        operator integer () {
            if (_type != type::integer)
                throw;
            return _integer;
        }
        operator string () {
            if (_type != type::string)
                throw;
            return _string;
        }
    private:
        type::id _type;
        string _string;
        integer _integer;
        double _double;
        object _object;
        array _array;
    };
    
    value operator,(const value& val1, const value& val2) {
        value ret;
        return ret;
    }
    
    struct pair {
    public:
        pair(const string& name, const value& val) : name(name), val(val) {}
        
        template <typename V>
        pair(const string& name, const V& val) : name(name), val(value(val)) {}
        
        string name;
        value val;
    };
    
    value operator,(const pair& p1, const pair& p2) {
        value ret;
        return ret;
    }
    value operator,(const pair& p, const value& v) {
        value ret;
        return ret;
    }
}
*/

int main(int argc, char* argv[]) {
/*
    try {
        try {
            json::value obj = (json::pair("id", 123), json::pair("name", "abc"));
            
            json::value arr = (json::value("abc"), obj);
            
            string name = arr[1]["name"];
            
            // this will throw if the schema is violated
            json::value val("{id : 123, name : 'abc'}", "{ properties : { id : { type : 'number' }, name : { type : 'string' } } }");

            int64_t id = val["id"];
            //            string name = val["name"];
            
            cout << val["id"] << endl;
            cout << val["name"] << endl;
            
            
        } catch (...) { throw; }
    }
    catch (std::exception &e) {
        cout << "Exception: " << e.what() << endl;
        cout << "FAILED: JSON class" << endl;
        return 1;
    }
*/
    
    return 0;
}
