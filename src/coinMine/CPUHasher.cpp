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

#include <coinMine/CPUHasher.h>

#include <coin/Block.h>

/// We choose to insert only the parts of Crypto++ that we need here - no need to compile the entire lib...

typedef unsigned int word32;

extern const word32 SHA256_K[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

template <class T> inline T rotrFixed(T x, unsigned int y)
{
    assert(y < sizeof(T)*8);
    return T((x>>y) | (x<<(sizeof(T)*8-y)));
}

template <class T> inline T rotlFixed(T x, unsigned int y)
{
    assert(y < sizeof(T)*8);
    return T((x<<y) | (x>>(sizeof(T)*8-y)));
}

// Align by increasing pointer, must have extra space at end of buffer
template <size_t nBytes, typename T>
T* alignup(T* p)
{
    union
    {
    T* ptr;
    size_t n;
    } u;
    u.ptr = p;
    u.n = (u.n + (nBytes-1)) & ~(nBytes-1);
    return u.ptr;
}

#define Ch(x,y,z) (z^(x&(y^z)))
#define Maj(x,y,z) (y^((x^y)&(y^z)))

#define S0(x) (rotrFixed(x,2)^rotrFixed(x,13)^rotrFixed(x,22))
#define S1(x) (rotrFixed(x,6)^rotrFixed(x,11)^rotrFixed(x,25))
#define s0(x) (rotrFixed(x,7)^rotrFixed(x,18)^(x>>3))
#define s1(x) (rotrFixed(x,17)^rotrFixed(x,19)^(x>>10))

void CryptoPPSHA256Transform(word32 *state, const word32 *data)
{
	word32 T[20];
	word32 W[32];
	unsigned int i = 0, j = 0;
	word32 *t = T+8;
    
	memcpy(t, state, 8*4);
	word32 e = t[4], a = t[0];
    
	do 
        {
		word32 w = data[j];
		W[j] = w;
		w += SHA256_K[j];
		w += t[7];
		w += S1(e);
		w += Ch(e, t[5], t[6]);
		e = t[3] + w;
		t[3] = t[3+8] = e;
		w += S0(t[0]);
		a = w + Maj(a, t[1], t[2]);
		t[-1] = t[7] = a;
		--t;
		++j;
		if (j%8 == 0)
			t += 8;
        } while (j<16);
    
	do
        {
		i = j&0xf;
		word32 w = s1(W[i+16-2]) + s0(W[i+16-15]) + W[i] + W[i+16-7];
		W[i+16] = W[i] = w;
		w += SHA256_K[j];
		w += t[7];
		w += S1(e);
		w += Ch(e, t[5], t[6]);
		e = t[3] + w;
		t[3] = t[3+8] = e;
		w += S0(t[0]);
		a = w + Maj(a, t[1], t[2]);
		t[-1] = t[7] = a;
        
		w = s1(W[(i+1)+16-2]) + s0(W[(i+1)+16-15]) + W[(i+1)] + W[(i+1)+16-7];
		W[(i+1)+16] = W[(i+1)] = w;
		w += SHA256_K[j+1];
		w += (t-1)[7];
		w += S1(e);
		w += Ch(e, (t-1)[5], (t-1)[6]);
		e = (t-1)[3] + w;
		(t-1)[3] = (t-1)[3+8] = e;
		w += S0((t-1)[0]);
		a = w + Maj(a, (t-1)[1], (t-1)[2]);
		(t-1)[-1] = (t-1)[7] = a;
        
		t-=2;
		j+=2;
		if (j%8 == 0)
			t += 8;
        } while (j<64);
    
    state[0] += a;
    state[1] += t[1];
    state[2] += t[2];
    state[3] += t[3];
    state[4] += e;
    state[5] += t[5];
    state[6] += t[6];
    state[7] += t[7];
}

int static FormatHashBlocks(void* pbuffer, unsigned int len)
{
    unsigned char* pdata = (unsigned char*)pbuffer;
    unsigned int blocks = 1 + ((len + 8) / 64);
    unsigned char* pend = pdata + 64 * blocks;
    memset(pdata + len, 0, 64 * blocks - len);
    pdata[len] = 0x80;
    unsigned int bits = len * 8;
    pend[-1] = (bits >> 0) & 0xff;
    pend[-2] = (bits >> 8) & 0xff;
    pend[-3] = (bits >> 16) & 0xff;
    pend[-4] = (bits >> 24) & 0xff;
    return blocks;
}

#ifdef _MSC_VER
#include <stdlib.h>
#if _MSC_VER >= 1400
// VC2005 workaround: disable declarations that conflict with winnt.h
#define _interlockedbittestandset CRYPTOPP_DISABLED_INTRINSIC_1
#define _interlockedbittestandreset CRYPTOPP_DISABLED_INTRINSIC_2
#define _interlockedbittestandset64 CRYPTOPP_DISABLED_INTRINSIC_3
#define _interlockedbittestandreset64 CRYPTOPP_DISABLED_INTRINSIC_4
#include <intrin.h>
#undef _interlockedbittestandset
#undef _interlockedbittestandreset
#undef _interlockedbittestandset64
#undef _interlockedbittestandreset64
#define CRYPTOPP_FAST_ROTATE(x) 1
#elif _MSC_VER >= 1300
#define CRYPTOPP_FAST_ROTATE(x) ((x) == 32 | (x) == 64)
#else
#define CRYPTOPP_FAST_ROTATE(x) ((x) == 32)
#endif
#elif (defined(__MWERKS__) && TARGET_CPU_PPC) || \
(defined(__GNUC__) && (defined(_ARCH_PWR2) || defined(_ARCH_PWR) || defined(_ARCH_PPC) || defined(_ARCH_PPC64) || defined(_ARCH_COM)))
#define CRYPTOPP_FAST_ROTATE(x) ((x) == 32)
#elif defined(__GNUC__) && (CRYPTOPP_BOOL_X64 || CRYPTOPP_BOOL_X86)     // depend on GCC's peephole optimization to generate rotate instructions
#define CRYPTOPP_FAST_ROTATE(x) 1
#else
#define CRYPTOPP_FAST_ROTATE(x) 0
#endif

inline word32 ByteReverse(word32 value)
{
#if defined(__GNUC__) && defined(CRYPTOPP_X86_ASM_AVAILABLE)
    __asm__ ("bswap %0" : "=r" (value) : "0" (value));
    return value;
#elif defined(CRYPTOPP_BYTESWAP_AVAILABLE)
    return bswap_32(value);
#elif defined(__MWERKS__) && TARGET_CPU_PPC
    return (word32)__lwbrx(&value,0);
#elif _MSC_VER >= 1400 || (_MSC_VER >= 1300 && !defined(_DLL))
    return _byteswap_ulong(value);
#elif CRYPTOPP_FAST_ROTATE(32)
    // 5 instructions with rotate instruction, 9 without
    return (rotrFixed(value, 8U) & 0xff00ff00) | (rotlFixed(value, 8U) & 0x00ff00ff);
#else
    // 6 instructions with rotate instruction, 8 without
    value = ((value & 0xFF00FF00) >> 8) | ((value & 0x00FF00FF) << 8);
    return rotlFixed(value, 16U);
#endif
}

static const unsigned int pSHA256InitState[8] =
{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

inline void SHA256Transform(void* pstate, void* pinput, const void* pinit)
{
    memcpy(pstate, pinit, 32);
    CryptoPPSHA256Transform((word32*)pstate, (word32*)pinput);
}

//
// ScanHash scans nonces looking for a hash with at least some zero bits.
// It operates on big endian data.  Caller does the byte reversing.
// All input buffers are 16-byte aligned.  nNonce is usually preserved
// between calls, but periodically or if nNonce is 0xffff0000 or above,
// the block is rebuilt and nNonce starts over at zero.
//
unsigned int static ScanHash_CryptoPP(char* pmidstate, char* pdata, char* phash1, char* phash, unsigned int& nHashesDone)
{
    unsigned int& nNonce = *(unsigned int*)(pdata + 12);
    nHashesDone = nNonce;
    for (;;) {
        // Crypto++ SHA-256
        // Hash pdata using pmidstate as the starting state into
        // preformatted buffer phash1, then hash phash1 into phash
        nNonce++;
        SHA256Transform(phash1, pdata, pmidstate);
        SHA256Transform(phash, phash1, pSHA256InitState);
        
        // Return the nonce if the hash has at least some zero bits,
        // caller will check if it has enough to reach the target
        if (((unsigned short*)phash)[14] == 0) {
            nHashesDone = nNonce-nHashesDone;
            return nNonce;
        }
            
        // If nothing found after trying for a while, return -1
        if ((nNonce & 0xffff) == 0) {
            nHashesDone = 0xffff+1;
            return -1;
        }
    }
}
    
void FormatHashBuffers(Block* pblock, char* pmidstate, char* pdata, char* phash1)
{
    //
    // Prebuild hash buffers
    //
    struct
    {
    struct unnamed2
        {
        int nVersion;
        uint256 hashPrevBlock;
        uint256 hashMerkleRoot;
        unsigned int nTime;
        unsigned int nBits;
        unsigned int nNonce;
        }
    block;
    unsigned char pchPadding0[64];
    uint256 hash1;
    unsigned char pchPadding1[64];
    }
    tmp;
    memset(&tmp, 0, sizeof(tmp));
    
    tmp.block.nVersion       = pblock->getVersion();
    tmp.block.hashPrevBlock  = pblock->getPrevBlock();
    tmp.block.hashMerkleRoot = pblock->getMerkleRoot();
    tmp.block.nTime          = pblock->getTime();
    tmp.block.nBits          = pblock->getBits();
    tmp.block.nNonce         = pblock->getNonce();
    
    FormatHashBlocks(&tmp.block, sizeof(tmp.block));
    FormatHashBlocks(&tmp.hash1, sizeof(tmp.hash1));
    
    // Byte swap all the input buffer
    for (int i = 0; i < sizeof(tmp)/4; i++)
        ((unsigned int*)&tmp)[i] = ByteReverse(((unsigned int*)&tmp)[i]);
    
    // Precalc the first half of the first hash, which stays constant
    SHA256Transform(pmidstate, &tmp.block, pSHA256InitState);
    
    memcpy(pdata, &tmp.block, 128);
    memcpy(phash1, &tmp.hash1, 64);
}

bool CPUHasher::operator()(Block& block, unsigned int nonces) {
    // Cycle over nonces and hash the block header trying to find a valid hash.
    
    //
    // Prebuild hash buffers
    //
    char pmidstatebuf[32+16]; char* pmidstate = alignup<16>(pmidstatebuf);
    char pdatabuf[128+16];    char* pdata     = alignup<16>(pdatabuf);
    char phash1buf[64+16];    char* phash1    = alignup<16>(phash1buf);
    
    FormatHashBuffers(&block, pmidstate, pdata, phash1);
    
    // Search
    uint256 hashTarget = CBigNum().SetCompact(block.getBits()).getuint256();
    uint256 hashbuf[2];
    uint256& hash = *alignup<16>(hashbuf);
    unsigned int counter = 0;
    loop {
        unsigned int nHashesDone = 0;
        
        // Crypto++ SHA-256
        unsigned int nNonceFound = ScanHash_CryptoPP(pmidstate, pdata + 64, phash1,
                                                     (char*)&hash, nHashesDone);        
        // Check if something found
        if (nNonceFound != -1) {
            for (int i = 0; i < sizeof(hash)/4; i++)
                ((unsigned int*)&hash)[i] = ByteReverse(((unsigned int*)&hash)[i]);
            
            if (hash <= hashTarget) {
                // Found a solution
                block.setNonce(ByteReverse(nNonceFound));
                assert(hash == block.getHash());
                
                return true;
            }
        }
        else {
            counter += nHashesDone;
            if (counter >= nonces)
                return false;
        }
    }    
}
