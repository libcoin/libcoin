// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_UTIL_H
#define BITCOIN_UTIL_H

#define NOMINMAX

#define __STDC_LIMIT_MACROS
#ifdef _MSC_VER

typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;

#else
#include <stdint.h>
#endif

#include <coin/uint256.h>
#include <coin/Logger.h>

#ifndef _WIN32
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif
#include <map>
#include <vector>
#include <string>


#include <boost/thread.hpp>
#include <boost/interprocess/sync/interprocess_recursive_mutex.hpp>
#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/chrono.hpp>

#include <openssl/sha.h>
#include <openssl/ripemd.h>



#if defined(_MSC_VER) && _MSC_VER < 1300
#define for  if (false) ; else for
#endif
#ifndef _MSC_VER
#define __forceinline  inline
#endif

#ifdef _WIN32
// This is used to attempt to keep keying material out of swap
// Note that VirtualLock does not provide this as a guarantee on Windows,
// but, in practice, memory that has been VirtualLock'd almost never gets written to
// the pagefile except in rare circumstances where memory is extremely low.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#define mlock(p, n) VirtualLock((p), (n));
#define munlock(p, n) VirtualUnlock((p), (n));
#else
#include <sys/mman.h>
#include <limits.h>
/* This comes from limits.h if it's not defined there set a sane default */
#ifndef PAGESIZE
#include <unistd.h>
#define PAGESIZE sysconf(_SC_PAGESIZE)
#endif
#define mlock(a,b) \
mlock(((void *)(((size_t)(a)) & (~((PAGESIZE)-1)))),\
(((((size_t)(a)) + (b) - 1) | ((PAGESIZE) - 1)) + 1) - (((size_t)(a)) & (~((PAGESIZE) - 1))))
#define munlock(a,b) \
munlock(((void *)(((size_t)(a)) & (~((PAGESIZE)-1)))),\
(((((size_t)(a)) + (b) - 1) | ((PAGESIZE) - 1)) + 1) - (((size_t)(a)) & (~((PAGESIZE) - 1))))
#endif

#define loop                for (;;)
#define BEGIN(a)            ((char*)&(a))
#define END(a)              ((char*)&((&(a))[1]))
#define UBEGIN(a)           ((unsigned char*)&(a))
#define UEND(a)             ((unsigned char*)&((&(a))[1]))
#define ARRAYLEN(array)     (sizeof(array)/sizeof((array)[0]))
//#define printf              OutputDebugStringF
//#define perror              OutputDebugStringF

#ifdef snprintf
#undef snprintf
#endif
#define snprintf my_snprintf

#ifndef PRI64d
#if defined(_MSC_VER) || defined(__BORLANDC__) || defined(__MSVCRT__)
#define PRI64d  "I64d"
#define PRI64u  "I64u"
#define PRI64x  "I64x"
#else
#define PRI64d  "lld"
#define PRI64u  "llu"
#define PRI64x  "llx"
#endif
#endif

// This is needed because the foreach macro can't get over the comma in pair<t1, t2>
#define PAIRTYPE(t1, t2)    pair<t1, t2>

#ifdef __MACH__
#define MSG_NOSIGNAL        0
#endif
#ifndef S_IRUSR
#define S_IRUSR             0400
#define S_IWUSR             0200
#endif

#ifdef _WIN32
#define MSG_NOSIGNAL        0
#define MSG_DONTWAIT        0
#ifndef UINT64_MAX
#define UINT64_MAX          _UI64_MAX
#define INT64_MAX           _I64_MAX
#define INT64_MIN           _I64_MIN
#endif
#ifndef S_IRUSR
#define S_IRUSR             0400
#define S_IWUSR             0200
#endif
#define unlink              _unlink
#else
#define WSAGetLastError()   errno
#define _vsnprintf(a,b,c,d) vsnprintf(a,b,c,d)
#define strlwr(psz)         to_lower(psz)
#define _strlwr(psz)        to_lower(psz)
#define MAX_PATH            1024
#define Beep(n1,n2)         (0)
#endif

//extern std::map<std::string, std::string> mapArgs;
//extern std::map<std::string, std::vector<std::string> > mapMultiArgs;
extern bool fDebug;
extern bool fPrintToConsole;
extern bool fPrintToDebugger;
extern std::string logfile;
extern char pszSetDataDir[MAX_PATH];
extern bool fDaemon;
extern std::string strMiscWarning;

extern bool fLogTimestamps;

void RandAddSeed();
void RandAddSeedPerfmon();
int OutputDebugStringF(const char* pszFormat, ...);
int my_snprintf(char* buffer, size_t limit, const char* format, ...);
std::string strprintf(const char* format, ...);
bool error(const char* format, ...);
void LogException(std::exception* pex, const char* pszThread);
void PrintException(std::exception* pex, const char* pszThread);
void PrintExceptionContinue(std::exception* pex, const char* pszThread);
//void ParseString(const std::string& str, char c, std::vector<std::string>& v);
std::string FormatMoney(int64_t n, bool fPlus=false);
//bool ParseMoney(const std::string& str, int64_t& nRet);
//bool ParseMoney(const char* pszIn, int64_t& nRet);
std::vector<unsigned char> ParseHex(const char* psz);
std::vector<unsigned char> ParseHex(const std::string& str);
const char* wxGetTranslation(const char* psz);
int GetFilesize(FILE* file);
std::string GetPidFile();

#ifdef _WIN32
std::string MyGetSpecialFolderPath(int nFolder, bool fCreate);
#endif
//std::string GetDefaultDataDir();
//std::string GetDataDir();
void ShrinkDebugFile();
int GetRandInt(int nMax);
uint64_t GetRand(uint64_t nMax);

int64_t GetAdjustedTime();
void AddTimeData(unsigned int ip, int64_t nTime);
std::string FormatVersion(int nVersion);


std::string default_data_dir(std::string suffix);


template <typename T>
std::string hexify(const T& t) {
    char chars[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
    unsigned char* bytes = (unsigned char*)&t;
    std::ostringstream ss;
    for (size_t i = 0; i < sizeof(T); i++) {
        ss << chars[bytes[i]>>4];
        ss << chars[bytes[i]&0xf];
    }
    return ss.str();
}

inline std::string hexify(const std::string& t) {
    char chars[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
    std::ostringstream ss;
    for (size_t i = 0; i < t.size(); i++) {
        ss << chars[((char)t[i])>>4];
        ss << chars[((char)t[i])&0xf];
    }
    return ss.str();
}

template <typename T>
std::string hexify(const std::vector<T>& t) {
    std::ostringstream ss;
    for (size_t i = 0; i < t.size(); i++)
        ss << hexify(t[i]);
    return ss.str();
}

long hex2long(const char* hexString);

// Wrapper to automatically initialize mutex
class CCriticalSection
{
protected:
    boost::interprocess::interprocess_recursive_mutex mutex;
public:
    explicit CCriticalSection() { }
    ~CCriticalSection() { }
    void Enter(const char* pszName, const char* pszFile, int nLine);
    void Leave();
    bool TryEnter(const char* pszName, const char* pszFile, int nLine);
};

// Automatically leave critical section when leaving block, needed for exception safety
class CCriticalBlock
{
protected:
    CCriticalSection* pcs;

public:
    CCriticalBlock(CCriticalSection& csIn, const char* pszName, const char* pszFile, int nLine)
    {
        pcs = &csIn;
        pcs->Enter(pszName, pszFile, nLine);
    }
    ~CCriticalBlock()
    {
        pcs->Leave();
    }
};

// WARNING: This will catch continue and break!
// break is caught with an assertion, but there's no way to detect continue.
// I'd rather be careful than suffer the other more error prone syntax.
// The compiler will optimise away all this loop junk.
#define CRITICAL_BLOCK(cs)     \
    for (bool fcriticalblockonce=true; fcriticalblockonce; assert(("break caught by CRITICAL_BLOCK!" && !fcriticalblockonce)), fcriticalblockonce=false) \
        for (CCriticalBlock criticalblock(cs, #cs, __FILE__, __LINE__); fcriticalblockonce; fcriticalblockonce=false)

class CTryCriticalBlock
{
protected:
    CCriticalSection* pcs;

public:
    CTryCriticalBlock(CCriticalSection& csIn, const char* pszName, const char* pszFile, int nLine)
    {
        pcs = (csIn.TryEnter(pszName, pszFile, nLine) ? &csIn : NULL);
    }
    ~CTryCriticalBlock()
    {
        if (pcs)
        {
            pcs->Leave();
        }
    }
    bool Entered() { return pcs != NULL; }
};

#define TRY_CRITICAL_BLOCK(cs)     \
    for (bool fcriticalblockonce=true; fcriticalblockonce; assert(("break caught by TRY_CRITICAL_BLOCK!" && !fcriticalblockonce)), fcriticalblockonce=false) \
        for (CTryCriticalBlock criticalblock(cs, #cs, __FILE__, __LINE__); fcriticalblockonce && (fcriticalblockonce = criticalblock.Entered()); fcriticalblockonce=false)

//
// Allocator that locks its contents from being paged
// out of memory and clears its contents before deletion.
//
template<typename T>
struct secure_allocator : public std::allocator<T>
{
    // MSVC8 default copy constructor is broken
    typedef std::allocator<T> base;
    typedef typename base::size_type size_type;
    typedef typename base::difference_type  difference_type;
    typedef typename base::pointer pointer;
    typedef typename base::const_pointer const_pointer;
    typedef typename base::reference reference;
    typedef typename base::const_reference const_reference;
    typedef typename base::value_type value_type;
    secure_allocator() throw() {}
    secure_allocator(const secure_allocator& a) throw() : base(a) {}
    template <typename U>
    secure_allocator(const secure_allocator<U>& a) throw() : base(a) {}
    ~secure_allocator() throw() {}
    template<typename _Other> struct rebind
    { typedef secure_allocator<_Other> other; };
    
    T* allocate(std::size_t n, const void *hint = 0)
    {
    T *p;
    p = std::allocator<T>::allocate(n, hint);
    if (p != NULL)
        mlock(p, sizeof(T) * n);
    return p;
    }
    
    void deallocate(T* p, std::size_t n)
    {
    if (p != NULL)
        {
        memset(p, 0, sizeof(T) * n);
        munlock(p, sizeof(T) * n);
        }
    std::allocator<T>::deallocate(p, n);
    }
};


// This is exactly like std::string, but with a custom allocator.
// (secure_allocator<> is defined in serialize.h)
typedef std::basic_string<char, std::char_traits<char>, secure_allocator<char> > SecureString;

inline std::string i64tostr(int64_t n)
{
    return strprintf("%"PRI64d, n);
}

inline std::string itostr(int n)
{
    return strprintf("%d", n);
}

inline int64_t atoi64(const char* psz)
{
#ifdef _MSC_VER
    return _atoi64(psz);
#else
    return strtoll(psz, NULL, 10);
#endif
}

inline int64_t atoi64(const std::string& str)
{
#ifdef _MSC_VER
    return _atoi64(str.c_str());
#else
    return strtoll(str.c_str(), NULL, 10);
#endif
}

inline int atoi(const std::string& str)
{
    return atoi(str.c_str());
}

inline int roundint(double d)
{
    return (int)(d > 0 ? d + 0.5 : d - 0.5);
}

inline int64_t roundint64(double d)
{
    return (int64_t)(d > 0 ? d + 0.5 : d - 0.5);
}

inline int64_t abs64(int64_t n)
{
    return (n >= 0 ? n : -n);
}

template<typename T>
std::string HexStr(const T itbegin, const T itend, bool fSpaces=false)
{
    if (itbegin == itend)
        return "";
    const unsigned char* pbegin = (const unsigned char*)&itbegin[0];
    const unsigned char* pend = pbegin + (itend - itbegin) * sizeof(itbegin[0]);
    std::string str;
    str.reserve((pend-pbegin) * (fSpaces ? 3 : 2));
    for (const unsigned char* p = pbegin; p != pend; p++)
        str += strprintf((fSpaces && p != pend-1 ? "%02x " : "%02x"), *p);
    return str;
}

inline std::string HexStr(const std::vector<unsigned char>& vch, bool fSpaces=false)
{
    return HexStr(vch.begin(), vch.end(), fSpaces);
}

template<typename T>
std::string HexNumStr(const T itbegin, const T itend, bool f0x=true)
{
    if (itbegin == itend)
        return "";
    const unsigned char* pbegin = (const unsigned char*)&itbegin[0];
    const unsigned char* pend = pbegin + (itend - itbegin) * sizeof(itbegin[0]);
    std::string str = (f0x ? "0x" : "");
    str.reserve(str.size() + (pend-pbegin) * 2);
    for (const unsigned char* p = pend-1; p >= pbegin; p--)
        str += strprintf("%02x", *p);
    return str;
}

inline std::string HexNumStr(const std::vector<unsigned char>& vch, bool f0x=true)
{
    return HexNumStr(vch.begin(), vch.end(), f0x);
}

template<typename T>
void PrintHex(const T pbegin, const T pend, const char* pszFormat="%s", bool fSpaces=true)
{
    log_info(pszFormat, HexStr(pbegin, pend, fSpaces).c_str());
}

inline void PrintHex(const std::vector<unsigned char>& vch, const char* pszFormat="%s", bool fSpaces=true)
{
    log_info(pszFormat, HexStr(vch, fSpaces).c_str());
}

inline int64_t GetPerformanceCounter()
{
    int64_t nCounter = 0;
#ifdef _WIN32
    QueryPerformanceCounter((LARGE_INTEGER*)&nCounter);
#else
    timeval t;
    gettimeofday(&t, NULL);
    nCounter = t.tv_sec * 1000000 + t.tv_usec;
#endif
    return nCounter;
}

class UnixTime {
public:
    inline static int s() {
        return (boost::posix_time::ptime(boost::posix_time::second_clock::universal_time()) -
                boost::posix_time::ptime(boost::gregorian::date(1970,1,1))).total_seconds();
    }
    inline static int64_t ms() {
        return us()/1000;
    }
    inline static int64_t us() {
        return (boost::posix_time::ptime(boost::posix_time::microsec_clock::universal_time()) -
                boost::posix_time::ptime(boost::gregorian::date(1970,1,1))).total_microseconds();
    }
    inline static int64_t ns() {
        boost::chrono::high_resolution_clock::time_point now = boost::chrono::high_resolution_clock::now();
        boost::chrono::nanoseconds nano = now.time_since_epoch();
        return nano.count()+adjust();
    }
    static int64_t adjust() {
        static int64_t zero = 0;
        if (zero == 0) {
            boost::chrono::high_resolution_clock::time_point now = boost::chrono::high_resolution_clock::now();
            boost::chrono::nanoseconds nano = now.time_since_epoch();
            zero = us()*1000-nano.count();
        }
        return zero;
    }
};

class Monotonic {
public:
    inline static int s() {
        static int last = 0;
        int t = UnixTime::s();
        if (t <= last)
            t = ++last;
        else
            last = t;
        return t;
    }
    inline static int64_t ms() {
        static int64_t last = 0;
        int64_t t = UnixTime::ms();
        if (t <= last)
            t = ++last;
        else
            last = t;
        return t;
    }
    inline static int64_t us() {
        static int64_t last = 0;
        int64_t t = UnixTime::us();
        if (t <= last)
            t = ++last;
        else
            last = t;
        return t;
    }
    inline static int64_t ns() {
        static int64_t last = 0;
        int64_t t = UnixTime::ns();
        if (t <= last)
            t = ++last;
        else
            last = t;
        return t;
    }
};

inline int64_t GetTimeMillis()
{
    return (boost::posix_time::ptime(boost::posix_time::microsec_clock::universal_time()) -
            boost::posix_time::ptime(boost::gregorian::date(1970,1,1))).total_milliseconds();
}

inline std::string DateTimeStrFormat(const char* pszFormat, int64_t nTime)
{
    time_t n = nTime;
    struct tm* ptmTime = gmtime(&n);
    char pszTime[200];
    strftime(pszTime, sizeof(pszTime), pszFormat, ptmTime);
    return pszTime;
}

template<typename T>
void skipspaces(T& it)
{
    while (isspace(*it))
        ++it;
}

inline bool IsSwitchChar(char c)
{
#ifdef _WIN32
    return c == '-' || c == '/';
#else
    return c == '-';
#endif
}
/*
inline std::string GetArg(const std::string& strArg, const std::string& strDefault)
{
    if (mapArgs.count(strArg))
        return mapArgs[strArg];
    return strDefault;
}

inline int64_t GetArg(const std::string& strArg, int64_t nDefault)
{
    if (mapArgs.count(strArg))
        return atoi64(mapArgs[strArg]);
    return nDefault;
}

inline bool GetBoolArg(const std::string& strArg)
{
    if (mapArgs.count(strArg))
    {
        if (mapArgs[strArg].empty())
            return true;
        return (atoi(mapArgs[strArg]) != 0);
    }
    return false;
}
*/



enum
{
    CSIDL_DESKTOP = 0x0000, CSIDL_INTERNET = 0x0001, CSIDL_PROGRAMS = 0x0002,
    CSIDL_CONTROLS = 0x0003,
    CSIDL_PRINTERS = 0x0004, CSIDL_PERSONAL = 0x0005, CSIDL_FAVORITES = 0x0006,
    CSIDL_STARTUP = 0x0007,
    CSIDL_RECENT = 0x0008, CSIDL_SENDTO = 0x0009, CSIDL_BITBUCKET/*Recycle
                                                                  Bin*/ = 0x000A,
    CSIDL_STARTMENU = 0x000B, CSIDL_MYDOCUMENTS = 0x000C, CSIDL_MYMUSIC =
    0x000D,
    CSIDL_MYVIDEO = 0x000E, CSIDL_DESKTOPDIRECTORY = 0x0010, CSIDL_DRIVES/*My
                                                                          Computer*/ = 0x0011,
    CSIDL_NETWORK = 0x0012, CSIDL_NETHOOD = 0x0013, CSIDL_FONTS = 0x0014,
    CSIDL_TEMPLATES = 0x0015, CSIDL_COMMON_STARTMENU = 0x0016,
    CSIDL_COMMON_PROGRAMS = 0x0017,
    CSIDL_COMMON_STARTUP = 0x0018, CSIDL_COMMON_DESKTOPDIRECTORY = 0x0019,
    CSIDL_APPDATA = 0x001A,
    CSIDL_PRINTHOOD = 0x001B, CSIDL_LOCAL_APPDATA = 0x001C, CSIDL_ALTSTARTUP =
    0x001D,
    CSIDL_COMMON_ALTSTARTUP = 0x001E, CSIDL_COMMON_FAVORITES = 0x001F,
    CSIDL_INTERNET_CACHE = 0x0020,
    CSIDL_COOKIES = 0x0021, CSIDL_HISTORY = 0x0022, CSIDL_COMMON_APPDATA =
    0x0023, CSIDL_WINDOWS = 0x0024,
    CSIDL_SYSTEM = 0x0025, CSIDL_PROGRAM_FILES = 0x0026, CSIDL_MYPICTURES =
    0x0027, CSIDL_PROFILE = 0x0028,
    CSIDL_PROGRAM_FILES_COMMON = 0x002B, CSIDL_COMMON_TEMPLATES = 0x002D,
    CSIDL_COMMON_DOCUMENTS = 0x002E, CSIDL_COMMON_ADMINTOOLS = 0x002F,
    CSIDL_ADMINTOOLS = 0x0030, CSIDL_COMMON_MUSIC = 0x0035,
    CSIDL_COMMON_PICTURES = 0x0036, CSIDL_COMMON_VIDEO = 0x0037,
    CSIDL_CDBURN_AREA = 0x003B, CSIDL_PROFILES = 0x003E, CSIDL_FLAG_CREATE =
    0x8000,
}; //CSIDL





inline void heapchk()
{
#ifdef _WIN32
    /// for debugging
    //if (_heapchk() != _HEAPOK)
    //    DebugBreak();
#endif
}

// Randomize the stack to help protect against buffer overrun exploits
#define IMPLEMENT_RANDOMIZE_STACK(ThreadFn)     \
    {                                           \
        static char nLoops;                     \
        if (nLoops <= 0)                        \
            nLoops = GetRand(20) + 1;           \
        if (nLoops-- > 1)                       \
        {                                       \
            ThreadFn;                           \
            return;                             \
        }                                       \
    }

#define CATCH_PRINT_EXCEPTION(pszFn)     \
    catch (std::exception& e) {          \
        PrintException(&e, (pszFn));     \
    } catch (...) {                      \
        PrintException(NULL, (pszFn));   \
    }










template<typename T1>
inline uint256 Hash(const T1 pbegin, const T1 pend)
{
    static unsigned char pblank[1];
    uint256 hash1;
    SHA256((pbegin == pend ? pblank : (unsigned char*)&pbegin[0]), (pend - pbegin) * sizeof(pbegin[0]), (unsigned char*)&hash1);
    uint256 hash2;
    SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
    return hash2;
}

template<typename T1, typename T2>
inline uint256 Hash(const T1 p1begin, const T1 p1end,
                    const T2 p2begin, const T2 p2end)
{
    static unsigned char pblank[1];
    uint256 hash1;
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, (p1begin == p1end ? pblank : (unsigned char*)&p1begin[0]), (p1end - p1begin) * sizeof(p1begin[0]));
    SHA256_Update(&ctx, (p2begin == p2end ? pblank : (unsigned char*)&p2begin[0]), (p2end - p2begin) * sizeof(p2begin[0]));
    SHA256_Final((unsigned char*)&hash1, &ctx);
    uint256 hash2;
    SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
    return hash2;
}

template<typename T1, typename T2, typename T3>
inline uint256 Hash(const T1 p1begin, const T1 p1end,
                    const T2 p2begin, const T2 p2end,
                    const T3 p3begin, const T3 p3end)
{
    static unsigned char pblank[1];
    uint256 hash1;
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, (p1begin == p1end ? pblank : (unsigned char*)&p1begin[0]), (p1end - p1begin) * sizeof(p1begin[0]));
    SHA256_Update(&ctx, (p2begin == p2end ? pblank : (unsigned char*)&p2begin[0]), (p2end - p2begin) * sizeof(p2begin[0]));
    SHA256_Update(&ctx, (p3begin == p3end ? pblank : (unsigned char*)&p3begin[0]), (p3end - p3begin) * sizeof(p3begin[0]));
    SHA256_Final((unsigned char*)&hash1, &ctx);
    uint256 hash2;
    SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
    return hash2;
}

template <typename T>
uint256 serialize_hash(const T& t) {
    std::string s = serialize(t);
    return ::Hash(s.begin(), s.end());
}

// Median filter over a stream of values
// Returns the median of the last N numbers
template <typename T> class CMedianFilter
{
private:
    std::vector<T> vValues;
    std::vector<T> vSorted;
    int nSize;
public:
    CMedianFilter(int size, T initial_value):
    nSize(size)
    {
    vValues.reserve(size);
    vValues.push_back(initial_value);
    vSorted = vValues;
    }
    
    void input(T value)
    {
    if(vValues.size() == nSize)
        {
        vValues.erase(vValues.begin());
        }
    vValues.push_back(value);
    
    vSorted.resize(vValues.size());
    std::copy(vValues.begin(), vValues.end(), vSorted.begin());
    std::sort(vSorted.begin(), vSorted.end());
    }
    
    T median() const
    {
    int size = vSorted.size();
    assert(size>0);
    if(size & 1) // Odd number of elements
        {
        return vSorted[size/2];
        }
    else // Even number of elements
        {
        return (vSorted[size/2-1] + vSorted[size/2]) / 2;
        }
    }
    
    int size() const
    {
    return vValues.size();
    }
    
    std::vector<T> sorted () const
    {
    return vSorted;
    }
};

// litecoin stuff

#ifdef __cplusplus
extern "C" {
#endif
    void scrypt_1024_1_1_256_sp(const char *input, char *output, char *scratchpad);
    void scrypt_1024_1_1_256(const char *input, char *output);
#ifdef __cplusplus
}
#endif


size_t getMemorySize();

#endif
