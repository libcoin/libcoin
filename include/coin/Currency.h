/* -*-c++-*- libcoin - Copyright (C) 2012-13 Michael Gronager
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

#ifndef CURRENCY_H
#define CURRENCY_H

#include <coin/Export.h>

#include <boost/noncopyable.hpp>

#include <string>

class COIN_EXPORT DecimalFormatter {
public:
    DecimalFormatter(unsigned int decimals = 8) : _decimals(decimals) {
    }
    
    std::string operator()(int64_t subunit) const;
    int64_t operator()(std::string str) const;
private:
    unsigned int _decimals;
};

/// Currency base class - also used for configuration purposes
class COIN_EXPORT Currency : private boost::noncopyable {
public:
    Currency(const std::string name, const std::string currency_code, size_t decimals) : _name(name), _currency_code(currency_code), _decimals(decimals) {}
    virtual const std::string name() const { return _name; }
    virtual std::string cc() const { return _currency_code; }
    virtual std::string operator()(int64_t amount, bool include_currency_symbol = true) const;
    virtual int64_t operator()(const std::string amount) const;

private:
    std::string _name;
    std::string _currency_code;
    size_t _decimals;
};

#endif // CURRENCY_H

