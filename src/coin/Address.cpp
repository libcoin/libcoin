
#include "coin/Address.h"
#include "coin/Key.h"

using namespace std;

static const char* pszBase58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

Address toAddress(const PubKey& vch)
{
    uint256 hash1;
    SHA256(&vch[0], vch.size(), (unsigned char*)&hash1);
    uint160 hash2;
    RIPEMD160((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
    return hash2;
}


inline string EncodeBase58(const unsigned char* pbegin, const unsigned char* pend)
{
    CAutoBN_CTX pctx;
    CBigNum bn58 = 58;
    CBigNum bn0 = 0;
    
    // Convert big endian data to little endian
    // Extra zero at the end make sure bignum will interpret as a positive number
    vector<unsigned char> vchTmp(pend-pbegin+1, 0);
    reverse_copy(pbegin, pend, vchTmp.begin());
    
    // Convert little endian data to bignum
    CBigNum bn;
    bn.setvch(vchTmp);
    
    // Convert bignum to string
    string str;
    // Expected size increase from base58 conversion is approximately 137%
    // use 138% to be safe
    str.reserve((pend - pbegin) * 138 / 100 + 1);
    CBigNum dv;
    CBigNum rem;
    while (bn > bn0)
        {
        if (!BN_div(&dv, &rem, &bn, &bn58, pctx))
            throw bignum_error("EncodeBase58 : BN_div failed");
        bn = dv;
        unsigned int c = rem.getulong();
        str += pszBase58[c];
        }
    
    // Leading zeroes encoded as base58 zeros
    for (const unsigned char* p = pbegin; p < pend && *p == 0; p++)
        str += pszBase58[0];
    
    // Convert little endian string to big endian
    reverse(str.begin(), str.end());
    return str;
}

inline string EncodeBase58(const vector<unsigned char>& vch)
{
    return EncodeBase58(&vch[0], &vch[0] + vch.size());
}

inline bool DecodeBase58(const char* psz, vector<unsigned char>& vchRet)
{
    CAutoBN_CTX pctx;
    vchRet.clear();
    CBigNum bn58 = 58;
    CBigNum bn = 0;
    CBigNum bnChar;
    while (isspace(*psz))
        psz++;
    
    // Convert big endian string to bignum
    for (const char* p = psz; *p; p++)
        {
        const char* p1 = strchr(pszBase58, *p);
        if (p1 == NULL)
            {
            while (isspace(*p))
                p++;
            if (*p != '\0')
                return false;
            break;
            }
        bnChar.setulong(p1 - pszBase58);
        if (!BN_mul(&bn, &bn, &bn58, pctx))
            throw bignum_error("DecodeBase58 : BN_mul failed");
        bn += bnChar;
        }
    
    // Get bignum as little endian data
    vector<unsigned char> vchTmp = bn.getvch();
    
    // Trim off sign byte if present
    if (vchTmp.size() >= 2 && vchTmp.end()[-1] == 0 && vchTmp.end()[-2] >= 0x80)
        vchTmp.erase(vchTmp.end()-1);
    
    // Restore leading zeros
    int nLeadingZeros = 0;
    for (const char* p = psz; *p == pszBase58[0]; p++)
        nLeadingZeros++;
    vchRet.assign(nLeadingZeros + vchTmp.size(), 0);
    
    // Convert little endian data to big endian
    reverse_copy(vchTmp.begin(), vchTmp.end(), vchRet.end() - vchTmp.size());
    return true;
}

inline bool DecodeBase58(const string& str, vector<unsigned char>& vchRet)
{
    return DecodeBase58(str.c_str(), vchRet);
}

string EncodeBase58Check(const vector<unsigned char>& vchIn)
{
    // add 4-byte hash check to the end
    vector<unsigned char> vch(vchIn);
    uint256 hash = Hash(vch.begin(), vch.end());
    vch.insert(vch.end(), (unsigned char*)&hash, (unsigned char*)&hash + 4);
    return EncodeBase58(vch);
}

inline bool DecodeBase58Check(const char* psz, vector<unsigned char>& vchRet)
{
    if (!DecodeBase58(psz, vchRet))
        return false;
    if (vchRet.size() < 4)
        {
        vchRet.clear();
        return false;
        }
    uint256 hash = Hash(vchRet.begin(), vchRet.end()-4);
    if (memcmp(&hash, &vchRet.end()[-4], 4) != 0)
        {
        vchRet.clear();
        return false;
        }
    vchRet.resize(vchRet.size()-4);
    return true;
}

bool DecodeBase58Check(const string& str, vector<unsigned char>& vchRet)
{
    return DecodeBase58Check(str.c_str(), vchRet);
}

string ChainAddress::toString() const {
    vector<unsigned char> address_part(1, _id);
    address_part.insert(address_part.end(), _address.begin(), _address.end());
    return EncodeBase58Check(address_part);
}

bool ChainAddress::setString(const string& str) {
    vector<unsigned char> temp;
    DecodeBase58Check(str, temp);
    if (temp.empty()) {
        _address = 0;
        _id = 0;
        return false;
    }
    _id = temp[0];
    memcpy(&_address, &temp[1], _address.size());
    return true;
}


