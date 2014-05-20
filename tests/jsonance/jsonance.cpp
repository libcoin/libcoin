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


#include <coin/ExtendedKey.h>
#include <coin/BigNum.h>
#include <coin/Shamir.h>

#include <iomanip>

#include <map>
#include <vector>
#include <deque>
#include <iostream>
#include <boost/variant.hpp>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>

#include <openssl/rand.h>

using namespace std;
using namespace boost;

/// The jsonite classes enables easy json creation, parsing and manipulation.
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

class formatbuf : public std::streambuf {
    std::streambuf*     _dest;
    bool                _start_of_line;
    std::string         _indent;
    std::ostream*       _owner;
protected:
    virtual int         overflow( int ch ) {
        if (_start_of_line && ch != '\n') {
            _dest->sputn( _indent.data(), _indent.size() );
        }
        _start_of_line = (ch == '\n');
        return _dest->sputc( ch );
    }
    
public:
    explicit formatbuf(std::streambuf* dest, int indent = 0 )
    : _dest(dest)
    , _start_of_line(true)
    , _indent(indent, ' ')
    , _owner(NULL) {}
    
    explicit formatbuf(std::ostream& dest, int indent = 0 )
    : _dest(dest.rdbuf())
    , _start_of_line(true)
    , _indent(indent, ' ')
    , _owner(&dest) {
        _owner->rdbuf(this);
    }
    
    virtual ~formatbuf() {
        if (_owner != NULL ) {
            _owner->rdbuf(_dest);
        }
    }
    
    formatbuf& operator++() {
        _indent = string(_indent.size() + 4, ' ');
        return *this;
    }

    formatbuf& operator--() {
        if (_indent.size() > 4)
            _indent = string(_indent.size() - 4, ' ');
        else
            _indent = "";
        return *this;
    }
};

ostream& increase(ostream& os) {
    formatbuf* sb = dynamic_cast<formatbuf*>(os.rdbuf());
    if (sb) {
        ++(*sb);
    }
    return os;
}

ostream& decrease(ostream& os) {
    formatbuf* sb = dynamic_cast<formatbuf*>(os.rdbuf());
    if (sb) {
        --(*sb);
    }
    return os;
}

ostream& newline(ostream& os) {
    formatbuf* sb = dynamic_cast<formatbuf*>(os.rdbuf());
    if (sb) {
        return os << "\n";
    }
    return os;
}

ostream& space(ostream& os) {
    formatbuf* sb = dynamic_cast<formatbuf*>(os.rdbuf());
    if (sb) {
        return os << " ";
    }
    return os;
}
namespace jsonite {
    class value;
}
std::string formatted(const jsonite::value& val);

namespace jsonite {
    class number {
    public:
        number(istream& is) : _integral(0), _fraction(0), _mantissa(0), _fractional(false), _exponential(false) {
            parse(is);
        }
        number(const string& s) : _integral(0), _fraction(0), _mantissa(0), _fractional(false), _exponential(false) {
            istringstream is(s);
            parse(is);
        }
        number(double d) : _integral(0), _fraction(0), _mantissa(0), _fractional(true), _exponential(false) {
            ostringstream os;
            os << setprecision(15) << d;
            istringstream is(os.str());
            parse(is);
        }
        bool is_real() const {
            return (_fractional || _exponential);
        }
        
        int64_t integral() const {
            return _integral;
        }
        
        double real() const {
            ostringstream os;
            os << _integral << "." << setfill('0') << setw(19) << _fraction << "E" << _mantissa;
            return lexical_cast<double>(os.str());
        }

        string str() const {
            ostringstream os;
            if (_fractional || _exponential) {
                os << _integral << ".";
                if (_fraction) {
                    ostringstream oss;
                    oss << setfill('0') << setw(19) << _fraction;
                    string f = oss.str();
                    while (f[f.size()-1] == '0') f.erase(f.size()-1, 1);
                    os << f;
                }
                if (_mantissa) os << "E" << _mantissa;
            }
            else
                os << _integral;
            return os.str();
        }
        
    private:
        void parse(istream& is) {
            string buffer;
            int c = is.get();
            if (c != EOF) {
                if (isdigit(c) || c == '-')
                    buffer.push_back(c);
                else
                    throw runtime_error(string("Reading number: expected -, 0-9, got: '") + (char)c + "'");
            }
            c = is.get();
            while (c != EOF) {
                if (isdigit(c))
                    buffer.push_back(c);
                else if (c == '.') {
                    _fractional = true;
                    break;
                }
                else if (c == 'e' || c == 'E') {
                    _exponential = true;
                    break;
                }
                else {
                    is.putback(c);
                    break;
                }
                c = is.get();
            }
            _integral = lexical_cast<int64_t>(buffer);
            buffer = "";
            
            if (_fractional) {
                c = is.get();
                while (c != EOF) {
                    if (isdigit(c))
                        buffer.push_back(c);
                    else if (c == 'e' || c == 'E') {
                        _exponential = true;
                        break;
                    }
                    else {
                        is.putback(c);
                        break;
                    }
                    c = is.get();
                }
                if (buffer.size()) {
                    while (buffer.size() < 19) buffer.push_back('0');
                    buffer.resize(19);
                }
                _fraction = buffer.size() ? lexical_cast<uint64_t>(buffer) : 0;
            }
            buffer = "";
            if (_exponential) {
                c = is.get();
                if (c != EOF) {
                    if (isdigit(c) || c == '-')
                        buffer.push_back(c);
                    else if (c == '+')
                        ;
                    else
                        throw runtime_error(string("Reading number: expected -, +, 0-9, got: '") + (char)c + "'");
                }
                c = is.get();
                while (c != EOF) {
                    if (isdigit(c))
                        buffer.push_back(c);
                    else {
                        is.putback(c);
                        break;
                    }
                    c = is.get();
                }
                _mantissa = buffer.size() ? lexical_cast<int64_t>(buffer) : 0;
            }
        }
        
        void normalize() {
            if (!is_real())
                return;
            
            double d = lexical_cast<double>(str());
            
            istringstream is(lexical_cast<string>(d));
            _integral = _fraction = _mantissa = 0;
            parse(is);
        }

    private:
        int64_t _integral;
        uint64_t _fraction;
        int64_t _mantissa;
        bool _fractional;
        bool _exponential;
    };
    
    class value {
    public:
        typedef int64_t integer;
        typedef map<std::string, value> object;
        typedef vector<value> array;
        
        class null {
        public:
            null() {}
            friend ostream& operator<<(ostream& os, const null& n) {
                return os << "null";
            }
        };
        
        enum Type {
            Null = 0,
            String,
            Integer,
            Number,
            Boolean,
            Object,
            Array
        };
        
        static string type(Type type) {
            switch (type) {
                case (Null) : return "null";
                case (String) : return "string";
                case (Integer) : return "integer";
                case (Number) : return "number";
                case (Boolean) : return "boolean";
                case (Object) : return "object";
                case (Array) : return "array";
                default:
                    throw runtime_error("Type #" + lexical_cast<string>(type) + " not supported.");
            }
        }
        
        static Type type(const string& str) {
            if (str == "null") return Null;
            if (str == "string") return String;
            if (str == "integer") return Integer;
            if (str == "number") return Number;
            if (str == "boolean") return Boolean;
            if (str == "object") return Object;
            if (str == "array") return Array;
            throw runtime_error("Type name: '" + str + "' not supported.");
        }
        
        value() : _(null()) {}
        value(const null&) : _(null()) {}
        value(array) : _(array()) {}
        value(object) : _(object()) {}
        value(const string& s) : _(s) {}
        value(const char* s) : _(string(s)) {}
        value(const bool& b) : _(b) {}
        value(const integer& i) : _(i) {}
        value(const double& d) : _(d) {}

        operator string() {
            return boost::get<string>(_);
        }
        
        operator integer() {
            return boost::get<integer>(_);
        }
        
        const string& str() const {
            return boost::get<string>(_);
        }
        
        template <typename T>
        const T& as() const {
            return boost::get<T>(_);
        }
        
        Type type() const {
            return (Type) _.which();
        }
        
        void push_back(const value& val) {
            boost::get<array>(_).push_back(val);
        }
        
        value& operator[](const string& name) {
            if (type() != Object)
                throw runtime_error(formatted(*this) + " is not an object");
            object& obj = boost::get<object>(_);
            return obj[name];
        }
        value& operator[](const char* name) {
            return operator[](string(name));
        }
        
        const value& operator[](const string& name) const {
            if (type() != Object)
                throw runtime_error(formatted(*this) + " is not an object");
            const object& obj = boost::get<object>(_);
            object::const_iterator p = obj.find(name);
            if (p == obj.end())
                throw runtime_error(formatted(*this) + " does not have a property called '" + name + "'");
            return p->second;
        }
        const value& operator[](const char* name) const {
            return operator[](string(name));
        }
        
        object::const_iterator find(const string& name) const {
            const object& obj = boost::get<object>(_);
            return obj.find(name);
        }
        
        object::const_iterator end() const {
            const object& obj = boost::get<object>(_);
            return obj.end();
        }
        
        bool has(const string& name) const {
            const object& obj = boost::get<object>(_);
            return obj.find(name) != obj.end();
        }
        
        value& operator[](size_t idx) {
            if (type() != Array)
                throw runtime_error(formatted(*this) + " is not an array");
            array& arr = boost::get<array>(_);
            if (idx >= arr.size())
                throw runtime_error(formatted(*this) + " out of bound request of: " + lexical_cast<string>(idx));
            return arr[idx];
        }
        
        const value& operator[](size_t idx) const {
            if (type() != Array)
                throw runtime_error(formatted(*this) + " is not an array");
            const array& arr = boost::get<array>(_);
            if (idx >= arr.size())
                throw runtime_error(formatted(*this) + " out of bound request of: " + lexical_cast<string>(idx));
            return arr[idx];
        }
        
        friend bool operator==(const value& l, const value& r) {
            return l._ == r._;
        }
        
        void validate(const value& schema) const {
            // check for and collect the general properties: enum, type, allOf, anyOf, oneOf, not
            typedef set<Type> Types;
            Types types;
            if (schema.has("type")) {
                if (schema["type"].type() == String)
                    types.insert(type(schema["type"].as<string>()));
                else {
                    for (size_t i = 0; i < schema["type"].as<array>().size(); ++i)
                        types.insert(type(schema["type"][i].as<string>()));
                }
            }
            
            array enums;
            if (schema.has("enum"))
                enums = schema["enum"].as<array>();

            // do we have an enum ?
            bool match = true;
            for (array::const_iterator e = enums.begin(); e != enums.end(); ++e) {
                match = false;
                if ((*this) == *e) {
                    match = true;
                    break;
                }
            }
            if (!match)
                throw runtime_error("Did not match any of the enumerations listed");

            // do we have any types
            for (Types::const_iterator t = types.begin(); t != types.end(); ++t) {
                if (*t == type()) {
                    switch (*t) {
                        case Null: {
                            break;
                        }
                        case String: {
                            if (schema.has("maxLength") && as<string>().size() > schema["maxLength"].as<integer>())
                                throw runtime_error("string '" + as<string>() + "' too long");
                            if (schema.has("minLength") && as<string>().size() < schema["minLength"].as<integer>())
                                throw runtime_error("string '" + as<string>() + "' too short");
                            if (schema.has("pattern") && boost::regex_match(as<string>(), boost::regex(schema["pattern"].as<string>())))
                                throw runtime_error("string '" + as<string>() + "' didn't match pattern: " + schema["pattern"].as<string>());
                            break;
                        }
                        case Number: {
                            if (schema.has("minimum") && (as<double>() < schema["minimum"].as<double>())) {
                                throw runtime_error("number " + lexical_cast<string>(as<double>()) + " was smaller than: " + lexical_cast<string>(schema["minimum"].as<double>()));
                            }
                            if (schema.has("exclusiveMinimum") && schema["exclusiveMinimum"].as<bool>() && (as<double>() == schema["minimum"].as<double>())) {
                                throw runtime_error("number " + lexical_cast<string>(as<double>()) + " was equal: " + lexical_cast<string>(schema["minimum"].as<double>()));
                            }
                            if (schema.has("maximum") && (as<double>() > schema["maximum"].as<double>())) {
                                throw runtime_error("number " + lexical_cast<string>(as<double>()) + " was bigger than: " + lexical_cast<string>(schema["maximum"].as<double>()));
                            }
                            if (schema.has("exclusiveMaximum") && schema["exclusiveMaximum"].as<bool>() && (as<double>() == schema["maximum"].as<double>())) {
                                throw runtime_error("number " + lexical_cast<string>(as<double>()) + " was equal: " + lexical_cast<string>(schema["maximum"].as<double>()));
                            }
                        }
                        case Integer: {
                            if (schema.has("multipleOf") && (as<integer>() % schema["multipleOf"].as<integer>()))
                                throw runtime_error("number " + lexical_cast<string>(as<integer>()) + " was not a multiple of: " + lexical_cast<string>(schema["multipleOf"].as<integer>()));
                            if (schema.has("minimum") && (as<integer>() < schema["minimum"].as<integer>())) {
                                throw runtime_error("number " + lexical_cast<string>(as<integer>()) + " was smaller than: " + lexical_cast<string>(schema["minimum"].as<integer>()));
                            }
                            if (schema.has("exclusiveMinimum") && schema["exclusiveMinimum"].as<bool>() && (as<integer>() == schema["minimum"].as<integer>())) {
                                throw runtime_error("number " + lexical_cast<string>(as<integer>()) + " was equal: " + lexical_cast<string>(schema["minimum"].as<integer>()));
                            }
                            if (schema.has("maximum") && (as<integer>() > schema["maximum"].as<integer>())) {
                                throw runtime_error("number " + lexical_cast<string>(as<integer>()) + " was bigger than: " + lexical_cast<string>(schema["maximum"].as<integer>()));
                            }
                            if (schema.has("exclusiveMaximum") && schema["exclusiveMaximum"].as<bool>() && (as<integer>() == schema["maximum"].as<integer>())) {
                                throw runtime_error("number " + lexical_cast<string>(as<integer>()) + " was equal: " + lexical_cast<string>(schema["maximum"].as<integer>()));
                            }
                            break;
                        }
                        case Boolean: {
                            break;
                        }
                        case Object: {
                            if (schema.has("minProperties") && (as<object>().size() < schema["minProperties"].as<integer>()))
                                throw runtime_error("object has too few properties - min is: " + lexical_cast<string>(schema["minProperties"].as<integer>()));
                            if (schema.has("maxProperties") && (as<object>().size() > schema["maxProperties"].as<integer>()))
                                throw runtime_error("object has too many properties - max is: " + lexical_cast<string>(schema["maxProperties"].as<integer>()));
                            set<string> props;
                            if (schema.has("properties")) {
                                // validate each of the properties
                                const object& properties = schema["properties"].as<object>();
                                for (object::const_iterator p = properties.begin(); p != properties.end(); ++p) {
                                    object::const_iterator o = find(p->first);
                                    if (o != end()) {
                                        props.insert(o->first);
                                        o->second.validate(p->second);
                                    }
                                }
                            }
                            if (schema.has("required")) {
                                // check that we do in fact have the required properties
                                const array required = schema["required"].as<array>();
                                for (array::const_iterator i = required.begin(); i != required.end(); ++i)
                                    if (props.find(i->as<string>()) == props.end())
                                        throw runtime_error("object missing required property: " + i->as<string>());
                            }
                            if (schema.has("additionalProperties"))
                                throw runtime_error("validation does not cover 'addtionalProperties'");
                            if (schema.has("patternProperties"))
                                throw runtime_error("validation does not cover 'patternProperties'");
                            // (dependencies)
                            break;
                        }
                        case Array: {
                            if (schema.has("minItems") && (as<array>().size() < schema["minItems"].as<integer>()))
                                throw runtime_error("array has too few items - min is: " + lexical_cast<string>(schema["minItems"].as<integer>()));
                            if (schema.has("maxItems") && (as<array>().size() > schema["maxItems"].as<integer>()))
                                throw runtime_error("array has too many items - max is: " + lexical_cast<string>(schema["maxItems"].as<integer>()));
                            if (schema.has("items")) {
                                // items is either an array or an object - basically it is a list of allowed schemas - ordered
                                // check if this is an array or an object
                                if (schema["items"].type() == Object) {
                                    // loop over all items they all need to be valid according to schema
                                    for (size_t i = 0; i < as<array>().size(); ++i)
                                        (*this)[i].validate(schema["items"]);
                                }
                                else if (schema.type() == Array) {
                                    for (size_t i = 0; i < as<array>().size() && i < schema["items"].as<array>().size(); ++i)
                                        (*this)[i].validate(schema["items"][i]);
                                    if (schema.has("additionalItems")) {
                                        if (schema["additionalItems"].type() == Object)
                                            for (size_t i = schema["items"].as<array>().size(); i < as<array>().size(); ++i)
                                                (*this)[i].validate(schema["additionalItems"]);
                                        else
                                            if (schema["items"].as<array>().size() < as<array>().size())
                                                throw runtime_error("too many elements in array");
                                    }
                                }
                                else
                                    throw runtime_error("items should be of either Object or Array type");
                                
                            }
                            if (schema.has("uniqueItems") && schema["uniqueItems"].as<bool>()) {
                                // check that all items are unique:
                                for (size_t i = 0; i < as<array>().size(); ++i)
                                    for (size_t j = i + 1; j < as<array>().size(); ++j)
                                        if ((*this)[i] == (*this)[j])
                                            throw runtime_error("items " + lexical_cast<string>(i) + " and " + lexical_cast<string>(j) + " not unique");
                            }
                            break;
                        }
                        default:
                            throw runtime_error("No such type: #" + type(*t));
                    }
                    match = true;
                    break;
                }
                else {
                    match = false;
                    continue;
                }
            }
            if (!match)
                throw runtime_error("Did not match any of the types listed");
        }

        class stream_visitor : public boost::static_visitor<> {
        public:
            stream_visitor(ostream& os) : _os(os) {}
            
            void operator()(const array& operand) const {
                if (operand.size()) {
                    _os << "[" << operand[0];
                    for (size_t i = 1; i < operand.size(); ++i )
                        _os << "," << space << operand[i];
                    _os << "]";
                }
                else
                    _os << "[]";

            }
            
            void operator()(const object& operand) const {
                if (operand.size()) {
                    object::const_iterator i = operand.begin();
                    _os << "{" << increase << newline;
                    _os << "\"" << i->first << "\"" << space << ":" << space << i->second;
                    for (++i; i != operand.end(); ++i)
                        _os << "," << newline << "\"" << i->first << "\"" << space << ":" << space << i->second;
                    _os << decrease << newline << "}";
                }
                else
                    _os << "{}";
            }
            
            void operator()(const string& operand) const {
                _os << "\"" << operand << "\"";
            }
            
            void operator()(const bool& operand) const {
                _os << (operand ? "true" : "false");
            }
            
            void operator()(const double& operand) const {
                number num(operand);
                _os << num.str();
            }
            template <typename T>
            void operator()(const T& operand) const {
                _os << operand;
            }
        private:
            ostream& _os;
        };
        
        friend ostream& operator<<(ostream& os, const jsonite::value& val) {
            boost::apply_visitor( stream_visitor(os), val._ );
            return os;
        }
        
    private:
        boost::variant<null,
        boost::recursive_wrapper<string>,
        boost::recursive_wrapper<integer>,
        boost::recursive_wrapper<double>,
        boost::recursive_wrapper<bool>,
        boost::recursive_wrapper<object>,
        boost::recursive_wrapper<array> > _;
    };
    
    string read_string_until_end_quote(istream& is) {
        string s;
        int c = is.get();
        while (c != EOF) {
            if (c == '\\') {
                c = is.get();
                switch (c) {
                    case '"' : break;
                    case '\\' : c = '\\'; break;
                    case '/' : break;
                    case 'b' : c = '\b'; break;
                    case 'f' : c = '\f'; break;
                    case 'n' : c = '\n'; break;
                    case 'r' : c = '\r'; break;
                    case 't' : c = '\t'; break;
                    case EOF:
                        throw runtime_error("Expected control character - got EOF");
                    default:
                        throw runtime_error(string("Expected control character - got '\\") + (char)c + "'");
                }
            }
            else if (c == '"')
                break;
            s.push_back(c);
            c = is.get();
        }
        return s;
    }
    
    istream& operator>>(istream& is, jsonite::value& val) {
        // make a parser that parses the content: obj start with {, arr start with [, string start with ", number start with .0123456789, null, true, false
        // check if first sign is [, {, ", n, t, f, or number
        // if [ call >> val, push_back, until ]
        // if { call >> "string", :, val, insert, until }
        // else read string, number, bool, null
        char c;
        is >> c;
        switch (c) {
            case '[' : {
                val = jsonite::value::array();
                is >> c;
                if (c == ']')
                    break;
                else
                    is.putback(c);
                do {
                    jsonite::value v;
                    is >> v;
                    val.push_back(v);
                    is >> c;
                } while (c == ',');
                if (c != ']')
                    throw runtime_error(string("Expected ',' or ']' got: '") + c + "'");
                break;
            }
            case '{' : {
                val = jsonite::value::object();
                do {
                    is >> c;
                    if (c == '}')
                        break;
                    if (c != '"')
                        throw runtime_error(string("Expected '\"' got: '") + c + "'");
                    string key = read_string_until_end_quote(is);
                    is >> c;
                    if (c != ':')
                        throw runtime_error(string("Expected ':' got: '") + c + "'");
                    jsonite::value v;
                    is >> v;
                    val[key] = v;
                    is >> c;
                } while (c == ',');
                if (c != '}')
                    throw runtime_error(string("Expected ',' or '}' got: '") + c + "'");
                break;
            }
            case '"' : {
                val = read_string_until_end_quote(is);
                break;
            }
            case 'n' : {
                char ull[4];
                is.get(ull, 4);
                if (strcmp(ull, "ull") != 0)
                    throw runtime_error(string("Expected 'null' got: 'n") + ull + "'");
                val = value::null();
                break;
            }
            case 't' : {
                char rue[4];
                is.get(rue, 4);
                if (strcmp(rue, "rue") != 0)
                    throw runtime_error(string("Expected 'true' got: 't") + rue + "'");
                val = true;
                break;
            }
            case 'f' : {
                char alse[5];
                is.get(alse, 5);
                if (strcmp(alse, "alse") != 0)
                    throw runtime_error(string("Expected 'false' got: 'f") + alse + "'");
                val = false;
                break;
            }
            default: {
                is.putback(c);
                number num(is);
                if (num.is_real())
                    val = num.real();
                else
                    val = num.integral();
                break;
            }
        }
        return is;
    }

    value parse(string json) {
        size_t pos = 0;
        while ((pos = json.find("'", pos)) != std::string::npos) {
            if (pos > 0 && json[pos-1] == '\\')
                json.replace(pos - 1, 2, "'");
            else {
                json.replace(pos, 1, "\"");
                pos++;
            }
        }
        istringstream is(json);
        value val;
        is >> val;
        return val;
    }
    
    /*
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
     */
    /*
    struct pair {
        pair(const string& key, const value& val) : key(key), val(val) {}
        string key;
        value val;
    };

    pair operator>>(const char* key, const value& val) {
        return pair(key, val);
    }
    
    pair operator>>(string key, const int64_t& val) {
        return pair(key, value(val));
    }
    value operator,(const pair& a, const pair& b) {
        value val = value::object();
        val[a.key] = a.val;
        val[b.key] = b.val;
        return val;
    }
    value operator,(const value& a, const pair& b) {
        value val = a;
        val[b.key] = b.val;
        return val;
    }
    value operator,(const value& a, const value& b) {
        value val = value::array();
        val.push_back(a);
        val.push_back(b);
        return val;
    }
    value operator,(const value& a, const char* b) {
        value val = a;
        val.push_back(b);
        return val;
        
    }
     */
}

string test_vector =
    "[ \
        { \
            \"precision\": \"zip\", \
            \"Latitude\":  377.668e-1, \
            \"Longitude\": -1.223959e+2, \
            \"Address\":   \"\", \
            \"City\":      \"SAN FRANCISCO\", \
            \"State\":     \"CA\", \
            \"Zip\":       94107, \
            \"OBJ\": {}, \
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
            \"Country\":   \"US\", \
            \"Array\": [] \
        } \
    ]";

//"{ name : 'location', properties : { precision : { type : 'string', description : 'level of precision', required : true } } }"

using namespace jsonite;

std::string formatted(const value& val) {
    ostringstream os;
    formatbuf fb(os);
    os << val;
    return os.str();
}

int main(int argc, char* argv[]) {
    
    try {
        try {
//            value o = ((string("a") >> 2), ("b" >> value((value::integer)4)), ("c" >> (value::array(), "abc","def","ghi")));
            
            value o = parse("{ 'a' : 3, 'a\\'' : 4, 'c' : [ 'abc', 'def', 'ghi', 123.0, 'ghij']}");
            cout << o << endl;

            cout << o["c"][3] << endl;

            value schema = parse("{ 'type' : 'object', 'properties' : { 'a' : { 'type' : 'integer', 'description' : 'test_a' }, 'a\\'' : { 'type' : 'integer', 'description' : 'test_aa' },  'c' : { 'type' : 'array', 'description' : 'test_c', 'items' : {'type' : ['string','number'] }, 'uniqueItems' : true } }, 'required' : ['a', 'a\\'', 'c']  }");

            cout << "Schema: " << formatted(schema) << endl;
            
            o.validate(schema);
            //o.confine(schema);
            
            
            value a((value::integer)123);
            value b("abc");
            value arr = value::array();
            
            arr.push_back(a);
            arr.push_back(b);
            arr.push_back(value(45.6));
            arr.push_back(value::null());
            arr.push_back(value::object());
            arr.push_back(value::array());
            arr.push_back(true);
            
            cout << arr << endl;
            
            arr[1] = a;
            arr[2] = b;
            
            cout << arr << endl;
            
            value obj = value::object();
            
            obj["a"] = 12.3;
            obj["b"] = "123";
            obj["c"] = arr;
            
            cout << obj << endl;
            
            istringstream is(test_vector);
            
            value val;
            
            is >> val;
            
            cout << formatted(val) << endl;
            
        } catch (...) { throw; }
    }
    catch (std::exception &e) {
        cout << "Exception: " << e.what() << endl;
        cout << "FAILED: JSON class" << endl;
        return 1;
    }
    
    return 0;
}
