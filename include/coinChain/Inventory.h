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

#ifndef INVENTORY_H
#define INVENTORY_H

#include <string>

#include <coin/uint256.h>
#include <coin/serialize.h>

enum
{
    MSG_TX = 1,
    MSG_BLOCK,
};

class Inventory
{
public:
    Inventory();
    Inventory(int type, const uint256& hash);
    Inventory(const std::string& type_name, const uint256& hash);
    
    IMPLEMENT_SERIALIZE
    (
     READWRITE(_type);
     READWRITE(_hash);
     )
    
    friend bool operator<(const Inventory& a, const Inventory& b);
    
    bool isKnownType() const;
    const char* getCommand() const;
    std::string toString() const;
    void print() const;
    
    const int getType() const { return _type; }
    const uint256 getHash() const { return _hash; }
    
private:
    int _type;
    uint256 _hash;
};

#endif // INVENTORY_H
