/* -*-c++-*- libcoin - Copyright (C) 2012-14 Michael Gronager
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

#ifndef _COIN_POINT_H_
#define _COIN_POINT_H_

#include <coin/Export.h>
#include <coin/BigNum.h>

#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>

class COIN_EXPORT Point {
public:
    enum Infinity {};
    
    Point(int curve = NID_secp256k1);
    
    Point(const Point& point);
    
    Point(const EC_POINT* p, int curve = NID_secp256k1);
    
    Point(Infinity inf, int curve = NID_secp256k1);
    
    Point(const CBigNum& x, const CBigNum& y, int curve = NID_secp256k1);
    
    ~Point();
    
    Point& operator=(const Point& point);
    
    CBigNum X() const;
    
    CBigNum Y() const;
    
    Point& operator+=(const Point& point);
    
    EC_GROUP* ec_group() const;
    
    EC_POINT* ec_point() const;
    
    friend std::ostream& operator<<(std::ostream& os, const Point& p) {
        return os << p.X() << p.Y();
    }
    
    friend std::istream& operator>>(std::istream& is, Point& p) {
        CBigNum x, y;
        is >> x >> y;
        p = Point(x, y);
        return is;
    }
    
private:
    EC_POINT* _ec_point;
    EC_GROUP* _ec_group;
};

inline Point operator+(const Point& lhs, const Point& rhs)
{
    Point res(lhs);
    res += rhs;
    return res;
}

Point operator*(const CBigNum& m, const Point& Q);

bool operator==(const Point& P, const Point& Q);

#endif // _COIN_POINT_H_
