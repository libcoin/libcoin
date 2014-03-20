#include <coin/Serialization.h>
#include <climits>

using namespace std;

static const unsigned int MAX_VARINT_SIZE = 0x02000000;

istream& operator>>(istream& is, const varint& var) {
    var._int = 0;
    
    unsigned char s;
    is.read((char*)&s, sizeof(s));
    if (s < 253)
        var._int = s;
    else if (s == 253) {
        unsigned short data;
        is.read((char*)&data, sizeof(data));
        var._int = data;
    }
    else if (s == 254) {
        unsigned int data;
        is.read((char*)&data, sizeof(data));
        var._int = data;
    }
    else
        is.read((char*)var._int, sizeof(var._int));
    
    if (var._int > (uint64_t)MAX_VARINT_SIZE)
        throw std::ios_base::failure("read compact : size too large");
    
    return is;
}

ostream& operator<<(ostream& os, const const_varint& var) {
    if (var._int < 253) {
        unsigned char s = var._int;
        return os.write((const char*)&s, sizeof(s));
    }
    else if (var._int <= USHRT_MAX) {
        unsigned char s = 253;
        unsigned short data = var._int;
        os.write((const char*)&s, sizeof(s));
        return os.write((const char*)&data, sizeof(data));
    }
    else if (var._int <= UINT_MAX) {
        unsigned char s = 254;
        unsigned int data = var._int;
        os.write((const char*)&s, sizeof(s));
        return os.write((const char*)&data, sizeof(data));
    }
    else
    {
        unsigned char s = 255;
        uint64_t data = var._int;
        os.write((const char*)&s, sizeof(s));
        return os.write((const char*)&data, sizeof(data));
    }
}
