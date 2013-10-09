

#include <coin/Currency.h>

#include <boost/lexical_cast.hpp>

using namespace std;
using namespace boost;

template <typename T>
T power(T m, unsigned int e) {
    T p(1);
    
    for (unsigned int i = 0; i < e; ++i)
        p *= m;
    
    return p;
}

std::string DecimalFormatter::operator()(int64_t subunit) const {
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

int64_t DecimalFormatter::operator()(std::string str) const {
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

std::string Currency::operator()(int64_t amount, bool include_currency_symbol) const {
    return DecimalFormatter(_decimals)(amount) + (include_currency_symbol ? " " + _currency_code : "");
}

int64_t Currency::operator()(const std::string amount) const {
    return DecimalFormatter(_decimals)(amount);
}

