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

#include <coin/Address.h>
#include <coin/Key.h>
#include <coin/Script.h>

using namespace std;

static const char* pszBase58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

PubKeyHash toPubKeyHash(const PubKey& vch)
{
    uint256 hash1;
    SHA256(&vch[0], vch.size(), (unsigned char*)&hash1);
    uint160 hash2;
    RIPEMD160((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
    return hash2;
}

PubKeyHash toPubKeyHash(const std::string& str) {
    PubKeyHash tmp(str);
    unsigned char* p = tmp.begin();
    for(unsigned int i = 0; i < tmp.size()/2; i++) {
        unsigned char swap = p[i];
        p[i] = p[tmp.size()-i-1];
        p[tmp.size()-i-1] = swap;
    }
    return tmp;
}

ScriptHash toScriptHash(const Script& vch)
{
    uint256 hash1;
    SHA256(&vch[0], vch.size(), (unsigned char*)&hash1);
    ScriptHash hash2;
    RIPEMD160((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
    return hash2;
}


inline string EncodeBase58(const unsigned char* pbegin, const unsigned char* pend, const char* alphabet = pszBase58)
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
        str += alphabet[c];
        }
    
    // Leading zeroes encoded as base58 zeros
    for (const unsigned char* p = pbegin; p < pend && *p == 0; p++)
        str += alphabet[0];
    
    // Convert little endian string to big endian
    reverse(str.begin(), str.end());
    return str;
}

inline string EncodeBase58(const vector<unsigned char>& vch, const char* alphabet = pszBase58)
{
    return EncodeBase58(&vch[0], &vch[0] + vch.size(), alphabet);
}

inline bool DecodeBase58(const char* psz, vector<unsigned char>& vchRet, const char* alphabet = pszBase58)
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
        const char* p1 = strchr(alphabet, *p);
        if (p1 == NULL)
            {
            while (isspace(*p))
                p++;
            if (*p != '\0')
                return false;
            break;
            }
        bnChar.setulong(p1 - alphabet);
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
    for (const char* p = psz; *p == alphabet[0]; p++)
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

string EncodeBase58Check(const vector<unsigned char>& vchIn, const char* alphabet)
{
    if (!alphabet) alphabet = pszBase58;
    // add 4-byte hash check to the end
    vector<unsigned char> vch(vchIn);
    uint256 hash = Hash(vch.begin(), vch.end());
    vch.insert(vch.end(), (unsigned char*)&hash, (unsigned char*)&hash + 4);
    return EncodeBase58(vch, alphabet);
}

inline bool DecodeBase58Check(const char* psz, vector<unsigned char>& vchRet, const char* alphabet)
{
    if (!alphabet) alphabet = pszBase58;
    if (!DecodeBase58(psz, vchRet, alphabet))
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

bool DecodeBase58Check(const string& str, vector<unsigned char>& vchRet, const char* alphabet)
{
    if (!alphabet) alphabet = pszBase58;
    return DecodeBase58Check(str.c_str(), vchRet, alphabet);
}

/*
string ChainAddress::toString() const {
    vector<unsigned char> address_part(1, _id);
    address_part.insert(address_part.end(), _address.begin(), _address.end());
    return EncodeBase58Check(address_part);
}

bool ChainAddress::setString(const string& str) {
    vector<unsigned char> temp;
    DecodeBase58Check(str, temp);
    if (temp.empty()) {
        _address = PubKeyHash(0);
        _id = 0;
        return false;
    }
    _id = temp[0];
    memcpy(&_address, &temp[1], _address.size());
    return true;
}
*/

