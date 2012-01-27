#ifndef INVENTORY_H
#define INVENTORY_H

#include <string>

#include "coin/uint256.h"
#include "coin/serialize.h"

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
