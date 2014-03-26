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


#include <coin/BigNum.h>

#include <iomanip>

#include <map>
#include <vector>
#include <deque>
#include <iostream>
#include <boost/variant.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>


using namespace std;
using namespace boost;

template <typename T>
T power(T m, unsigned int e) {
    T p(1);
    
    for (unsigned int i = 0; i < e; ++i)
        p *= m;
    
    return p;
}

class DecimalFormatter {
public:
    DecimalFormatter(unsigned int decimals = 8) : _decimals(decimals) {
    }
    
    std::string operator()(int64_t subunit) {
        bool negative = (subunit < 0);
        if (negative) subunit = -subunit;
        string str = lexical_cast<string>(subunit);
        size_t len = str.size();
        if (len < 1+_decimals)
            str = string(1+_decimals-len, '0') + str;
        str.insert(str.size()-_decimals, 1, '.');
        if (negative) str.insert(0, 1, '-');
        return str;
    }
    
    int64_t operator()(std::string str) {
        // amount is a string - change into an int64_t
        bool negative = (str[0] == '-');
        if (negative) str = str.substr(1);
        
        size_t decimal = str.find('.');
        int64_t subunit = power(10, _decimals)*lexical_cast<int64_t>(str.substr(0, decimal));
        if (decimal != string::npos) {
            str = str.substr(decimal + 1) + string(_decimals, '0');
            str.resize(_decimals);
            subunit += lexical_cast<int64_t>(str);
        }
        
        if (negative) subunit = -subunit;
        return subunit;
    }
private:
    unsigned int _decimals;
};
/*
namespace json {
    class null {
    public:
        null() {}
    };

    typedef bool boolean;
    typedef int64_t integer;
    typedef double number;
    typedef map<string, value> object;
    typedef vector<value> array;
    
    typedef boost::variant<null,
    boost::recursive_wrapper<boolean>,
    boost::recursive_wrapper<integer>,
    boost::recursive_wrapper<number>,
    boost::recursive_wrapper<string>,
    boost::recursive_wrapper<integer>,
    boost::recursive_wrapper<object>,
    boost::recursive_wrapper<array> > value;
}

*/

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
/// stringstream("{abc:123, aaa:[123,aaa,232]}") >> json::loose >> value;
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

class error : public runtime_error {
public:
    error(std::string what) : runtime_error(what) {}
    error(std::string what, std::exception& e) : runtime_error(what + ":\n" + indent(e.what())) {}
    
    std::string indent(std::string str) const {
        // indent first line
        str.insert(0, "\t");
        boost::replace_all(str, "\n", "\n\t");
        return str;
    }
};

string strip_path(string path) {
    size_t slash = path.rfind("/");
    return path.substr(slash + 1);
}
/*
namespace coin {

void f0() {
    throw error(strip_path(__FILE__) + " f0");
}

void f1() {
    cout << strip_path(__FILE__) << ":" << __LINE__ << " " << __FUNCTION__ << endl;
    try {
        f0();
    }
    catch (std::exception&e) {
        throw error(__FUNCTION__, e);
    }
}

void f2() {
    try {
        f1();
    }
    catch (std::exception&e) {
        throw error(__FUNCTION__, e);
    }
}

void f3() {
    try {
        f2();
    }
    catch (std::exception&e) {
        throw error(__FUNCTION__, e);
    }
}
}
 */
int main(int argc, char* argv[]) {
    string amount = "299.99998000";
    cout << DecimalFormatter(6)(amount) << endl;

    /*
    json::value res = json::object("id", lexical_cast<string>(id))("amounts", lexical_cast<string>(payment.amount()))("txs", json::array(lexical_cast<string>(payment.hash().toString())))("fees", lexical_cast<string>(payment.fee()));
    stringstream ss;
    ss << json::style(json::compact) << res;
    ss.str();
    
    json::value res;
    stringstream ss(response);
    ss >> json::schema("{type:object, properties:{ id:{type:string,required:true}, {id:amounts,required:true}, {fee:}") >> res;
    res["txs"].push_back(lexical_cast<string>(payment.hash().toString()));
    res["amounts"] = lexical_cast<int64_t>(res["amounts"]) + payment.amount();
    res["fees"] = lexical_cast<int64_t>(res["fees"]) + payment.amount();
    ss << res;
    ss.str();

*/    
    /*
    cout << boost::interprocess::ipcdetail::get_current_process_id() << endl;
    
    CBigNum pow;
    pow.SetCompact(0x1c008000);
    cout << hex << (pow-1).GetCompact() << endl;
    
    
    cout << hex << CBigNum(~uint256(0) >> 30).GetCompact() << endl;
    cout << hex << CBigNum(~uint256(0) >> 31).GetCompact() << endl;
    cout << hex << CBigNum(~uint256(0) >> 32).GetCompact() << endl;
    cout << hex << CBigNum(~uint256(0) >> 33).GetCompact() << endl;
    cout << hex << CBigNum(~uint256(0) >> 34).GetCompact() << endl;
    cout << hex << CBigNum(~uint256(0) >> 35).GetCompact() << endl;
    cout << hex << CBigNum(~uint256(0) >> 36).GetCompact() << endl;
    cout << hex << CBigNum(~uint256(0) >> 37).GetCompact() << endl;
    cout << hex << CBigNum(~uint256(0) >> 38).GetCompact() << endl;
    cout << hex << CBigNum(~uint256(0) >> 39).GetCompact() << endl;
    cout << hex << CBigNum(~uint256(0) >> 40).GetCompact() << endl;
    cout << hex << CBigNum(~uint256(0) >> 41).GetCompact() << endl;
    cout << hex << CBigNum(~uint256(0) >> 42).GetCompact() << endl;
    
    int64_t amount = 100000000;
    cout << DecimalFormatter()(amount) << endl;
    
    cout << DecimalFormatter()("0.01") << endl;
     */
    /*
    try {
        coin::f3();
    } catch (std::exception& e) {
        cout << "Error:\n" << e.what() << endl;
    }
    
    return 0;
    
    
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
