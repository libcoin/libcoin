// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
//#include "headers.h>
#include <coin/util.h>
#include <coin/Transaction.h>
#include <coin/Logger.h>

#include <boost/program_options/parsers.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_recursive_mutex.hpp>
#include <boost/foreach.hpp>
#include <boost/program_options/detail/config_file.hpp>
#include <boost/algorithm/string/join.hpp>

#include <openssl/crypto.h>
#include <openssl/rand.h>

using namespace std;
using namespace boost;

//map<string, string> mapArgs;
//map<string, vector<string> > mapMultiArgs;
bool fDebug = false;
bool fPrintToConsole = false;
bool fPrintToDebugger = false;
string logfile("");
char pszSetDataDir[MAX_PATH] = "";
bool fDaemon = false;
string strMiscWarning;

bool fLogTimestamps = false;




// Workaround for "multiple definition of `_tls_used'"
// http://svn.boost.org/trac/boost/ticket/4258
extern "C" void tss_cleanup_implemented() { }


string default_data_dir(std::string suffix) {
    // Windows: C:\Documents and Settings\username\Application Data\Libcoin
    // Mac: ~/Library/Application Support/Libcoin
    // Unix: ~/.libcoin
#if (defined _WIN32 || defined __MACH__) // convert first letter to upper case
    std::transform(suffix.begin(), suffix.begin()+1, suffix.begin(), ::toupper);
#else // prepend a "."
      //    suffix = "." + suffix;
#endif
    
#ifdef _WIN32
    // Windows
    return MyGetSpecialFolderPath(CSIDL_APPDATA, true) + "\\LibCoin\\" + suffix;
#else
    char* pszHome = getenv("HOME");
    if (pszHome == NULL || strlen(pszHome) == 0)
        pszHome = (char*)"/";
    std::string strHome = pszHome;
    if (strHome[strHome.size()-1] != '/')
        strHome += '/';
#ifdef __MACH__
    // Mac
    strHome += "Library/Application Support/LibCoin/";
    boost::filesystem::create_directory(strHome.c_str());
#else // unix
    strHome += ".libcoin/";
    boost::filesystem::create_directory(strHome.c_str());
#endif
    
    // Unix
    return strHome + suffix;
#endif
}



// Init openssl library multithreading support
static boost::interprocess::interprocess_mutex** ppmutexOpenSSL;
void locking_callback(int mode, int i, const char* file, int line)
{
    if (mode & CRYPTO_LOCK)
        ppmutexOpenSSL[i]->lock();
    else
        ppmutexOpenSSL[i]->unlock();
}

// Init
class CInit
{
public:
    CInit()
    {
        // Init openssl library multithreading support
        ppmutexOpenSSL = (boost::interprocess::interprocess_mutex**)OPENSSL_malloc(CRYPTO_num_locks() * sizeof(boost::interprocess::interprocess_mutex*));
        for (int i = 0; i < CRYPTO_num_locks(); i++)
            ppmutexOpenSSL[i] = new boost::interprocess::interprocess_mutex();
        CRYPTO_set_locking_callback(locking_callback);

#ifdef _WIN32
        // Seed random number generator with screen scrape and other hardware sources
        RAND_screen();
#endif

        // Seed random number generator with performance counter
        RandAddSeed();
    }
    ~CInit()
    {
        // Shutdown openssl library multithreading support
        CRYPTO_set_locking_callback(NULL);
        for (int i = 0; i < CRYPTO_num_locks(); i++)
            delete ppmutexOpenSSL[i];
        OPENSSL_free(ppmutexOpenSSL);
    }
}
instance_of_cinit;








void RandAddSeed()
{
    // Seed with CPU performance counter
    int64_t nCounter = GetPerformanceCounter();
    RAND_add(&nCounter, sizeof(nCounter), 1.5);
    memset(&nCounter, 0, sizeof(nCounter));
}

void RandAddSeedPerfmon()
{
    RandAddSeed();

    // This can take up to 2 seconds, so only do it every 10 minutes
    static int64_t nLastPerfmon;
    if (UnixTime::s() < nLastPerfmon + 10 * 60)
        return;
    nLastPerfmon = UnixTime::s();

#ifdef _WIN32
    // Don't need this on Linux, OpenSSL automatically uses /dev/urandom
    // Seed with the entire set of perfmon data
    unsigned char pdata[250000];
    memset(pdata, 0, sizeof(pdata));
    unsigned long nSize = sizeof(pdata);
    long ret = RegQueryValueExA(HKEY_PERFORMANCE_DATA, "Global", NULL, NULL, pdata, &nSize);
    RegCloseKey(HKEY_PERFORMANCE_DATA);
    if (ret == ERROR_SUCCESS)
    {
        RAND_add(pdata, nSize, nSize/100.0);
        memset(pdata, 0, nSize);
        log_debug("%s RandAddSeed() %d bytes\n", DateTimeStrFormat("%x %H:%M", UnixTime::s()).c_str(), nSize);
    }
#endif
}

uint64_t GetRand(uint64_t nMax)
{
    if (nMax == 0)
        return 0;

    // The range of the random source must be a multiple of the modulus
    // to give every possible output value an equal possibility
    uint64_t nRange = (UINT64_MAX / nMax) * nMax;
    uint64_t nRand = 0;
    do
        RAND_bytes((unsigned char*)&nRand, sizeof(nRand));
    while (nRand >= nRange);
    return (nRand % nMax);
}

int GetRandInt(int nMax)
{
    return GetRand(nMax);
}

inline int OutputDebugStringF(const char* pszFormat, ...)
{
    int ret = 0;
    if (logfile.size() == 0)
    {
        // print to console
        va_list arg_ptr;
        va_start(arg_ptr, pszFormat);
        ret = vprintf(pszFormat, arg_ptr);
        va_end(arg_ptr);
    }
    else
    {
        // print to debug.log
        static FILE* fileout = NULL;

        if (!fileout)
        {
        //            char pszFile[MAX_PATH+100];
        //            GetDataDir(pszFile);
        //            strlcat(pszFile, "/debug.log", sizeof(pszFile));
            fileout = fopen(logfile.c_str(), "a");
            if (fileout) setbuf(fileout, NULL); // unbuffered
        }
        if (fileout)
        {
            static bool fStartedNewLine = true;

            // Debug print useful for profiling
            if (fLogTimestamps && fStartedNewLine)
                fprintf(fileout, "%s ", DateTimeStrFormat("%x %H:%M:%S", UnixTime::s()).c_str());
            if (pszFormat[strlen(pszFormat) - 1] == '\n')
                fStartedNewLine = true;
            else
                fStartedNewLine = false;

            va_list arg_ptr;
            va_start(arg_ptr, pszFormat);
            ret = vfprintf(fileout, pszFormat, arg_ptr);
            va_end(arg_ptr);
        }
    }

#ifdef _WIN32
    if (fPrintToDebugger)
    {
        static CCriticalSection cs_OutputDebugStringF;

        // accumulate a line at a time
        CRITICAL_BLOCK(cs_OutputDebugStringF)
        {
            static char pszBuffer[50000];
            static char* pend;
            if (pend == NULL)
                pend = pszBuffer;
            va_list arg_ptr;
            va_start(arg_ptr, pszFormat);
            int limit = END(pszBuffer) - pend - 2;
            int ret = _vsnprintf(pend, limit, pszFormat, arg_ptr);
            va_end(arg_ptr);
            if (ret < 0 || ret >= limit)
            {
                pend = END(pszBuffer) - 2;
                *pend++ = '\n';
            }
            else
                pend += ret;
            *pend = '\0';
            char* p1 = pszBuffer;
            char* p2;
            while (p2 = strchr(p1, '\n'))
            {
                p2++;
                char c = *p2;
                *p2 = '\0';
                OutputDebugStringA(p1);
                *p2 = c;
                p1 = p2;
            }
            if (p1 != pszBuffer)
                memmove(pszBuffer, p1, pend - p1 + 1);
            pend -= (p1 - pszBuffer);
        }
    }
#endif
    return ret;
}


// Safer snprintf
//  - prints up to limit-1 characters
//  - output string is always null terminated even if limit reached
//  - return value is the number of characters actually printed
int my_snprintf(char* buffer, size_t limit, const char* format, ...)
{
    if (limit == 0)
        return 0;
    va_list arg_ptr;
    va_start(arg_ptr, format);
    int ret = _vsnprintf(buffer, limit, format, arg_ptr);
    va_end(arg_ptr);
    if (ret < 0 || (unsigned int)ret >= limit)
    {
        ret = limit - 1;
        buffer[limit-1] = 0;
    }
    return ret;
}


string strprintf(const char* format, ...)
{
    char buffer[50000];
    char* p = buffer;
    int limit = sizeof(buffer);
    int ret;
    loop
    {
        va_list arg_ptr;
        va_start(arg_ptr, format);
        ret = _vsnprintf(p, limit, format, arg_ptr);
        va_end(arg_ptr);
        if (ret >= 0 && ret < limit)
            break;
        if (p != buffer)
            delete[] p;
        limit *= 2;
        p = new char[limit];
        if (p == NULL)
            throw std::bad_alloc();
    }
    string str(p, p+ret);
    if (p != buffer)
        delete[] p;
    return str;
}


bool error(const char* format, ...)
{
    char buffer[50000];
    int limit = sizeof(buffer);
    va_list arg_ptr;
    va_start(arg_ptr, format);
    int ret = _vsnprintf(buffer, limit, format, arg_ptr);
    va_end(arg_ptr);
    if (ret < 0 || ret >= limit)
    {
        ret = limit - 1;
        buffer[limit-1] = 0;
    }
    log_error("ERROR: %s\n", buffer);
    return false;
}

/*
void ParseString(const string& str, char c, vector<string>& v)
{
    if (str.empty())
        return;
    string::size_type i1 = 0;
    string::size_type i2;
    loop
    {
        i2 = str.find(c, i1);
        if (i2 == str.npos)
        {
            v.push_back(str.substr(i1));
            return;
        }
        v.push_back(str.substr(i1, i2-i1));
        i1 = i2+1;
    }
}
*/

string FormatMoney(int64_t n, bool fPlus)
{
    // Note: not using straight sprintf here because we do NOT want
    // localized number formatting.
    int64_t n_abs = (n > 0 ? n : -n);
    int64_t quotient = n_abs/COIN;
    int64_t remainder = n_abs%COIN;
    string str = strprintf("%"PRI64d".%08"PRI64d, quotient, remainder);

    // Right-trim excess 0's before the decimal point:
    int nTrim = 0;
    for (int i = str.size()-1; (str[i] == '0' && isdigit(str[i-2])); --i)
        ++nTrim;
    if (nTrim)
        str.erase(str.size()-nTrim, nTrim);

    if (n < 0)
        str.insert((unsigned int)0, 1, '-');
    else if (fPlus && n > 0)
        str.insert((unsigned int)0, 1, '+');
    return str;
}


bool ParseMoney(const string& str, int64_t& nRet)
{
    return ParseMoney(str.c_str(), nRet);
}
/*
bool ParseMoney(const char* pszIn, int64_t& nRet)
{
    string strWhole;
    int64_t nUnits = 0;
    const char* p = pszIn;
    while (isspace(*p))
        p++;
    for (; *p; p++)
    {
        if (*p == '.')
        {
            p++;
            int64_t nMult = CENT*10;
            while (isdigit(*p) && (nMult > 0))
            {
                nUnits += nMult * (*p++ - '0');
                nMult /= 10;
            }
            break;
        }
        if (isspace(*p))
            break;
        if (!isdigit(*p))
            return false;
        strWhole.insert(strWhole.end(), *p);
    }
    for (; *p; p++)
        if (!isspace(*p))
            return false;
    if (strWhole.size() > 14)
        return false;
    if (nUnits < 0 || nUnits > COIN)
        return false;
    int64_t nWhole = atoi64(strWhole);
    int64_t nValue = nWhole*COIN + nUnits;

    nRet = nValue;
    return true;
}

*/
vector<unsigned char> ParseHex(const char* psz)
{
    static char phexdigit[256] =
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      0,1,2,3,4,5,6,7,8,9,-1,-1,-1,-1,-1,-1,
      -1,0xa,0xb,0xc,0xd,0xe,0xf,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,0xa,0xb,0xc,0xd,0xe,0xf,-1,-1,-1,-1,-1,-1,-1,-1,-1
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, };

    // convert hex dump to vector
    vector<unsigned char> vch;
    loop
    {
        while (isspace(*psz))
            psz++;
        char c = phexdigit[(unsigned char)*psz++];
        if (c == (char)-1)
            break;
        unsigned char n = (c << 4);
        c = phexdigit[(unsigned char)*psz++];
        if (c == (char)-1)
            break;
        n |= c;
        vch.push_back(n);
    }
    return vch;
}

vector<unsigned char> ParseHex(const string& str)
{
    return ParseHex(str.c_str());
}

long hex2long(const char* hexString) {
    static const long hextable[] =
    {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,         // 10-19
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,         // 30-39
        -1, -1, -1, -1, -1, -1, -1, -1,  0,  1,
        2,  3,  4,  5,  6,  7,  8,  9, -1, -1,         // 50-59
        -1, -1, -1, -1, -1, 10, 11, 12, 13, 14,
        15, -1, -1, -1, -1, -1, -1, -1, -1, -1,         // 70-79
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, 10, 11, 12,         // 90-99
        13, 14, 15, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,         // 110-109
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,         // 130-139
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,         // 150-159
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,         // 170-179
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,         // 190-199
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,         // 210-219
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,         // 230-239
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1
    };
    
    long ret = 0;
    
    while (*hexString && ret >= 0) {
        ret = (ret << 4) | hextable[(uint8_t)*hexString++];
    }
    
    return ret;
}

/*
bool WildcardMatch(const char* psz, const char* mask)
{
    loop
    {
        switch (*mask)
        {
        case '\0':
            return (*psz == '\0');
        case '*':
            return WildcardMatch(psz, mask+1) || (*psz && WildcardMatch(psz+1, mask));
        case '?':
            if (*psz == '\0')
                return false;
            break;
        default:
            if (*psz != *mask)
                return false;
            break;
        }
        psz++;
        mask++;
    }
}

bool WildcardMatch(const string& str, const string& mask)
{
    return WildcardMatch(str.c_str(), mask.c_str());
}
*/
void FormatException(char* pszMessage, std::exception* pex, const char* pszThread)
{
#ifdef _WIN32
    char pszModule[MAX_PATH];
    pszModule[0] = '\0';
    GetModuleFileNameA(NULL, pszModule, sizeof(pszModule));
#else
    const char* pszModule = "bitcoin";
#endif
    if (pex)
        snprintf(pszMessage, 1000,
            "EXCEPTION: %s       \n%s       \n%s in %s       \n", typeid(*pex).name(), pex->what(), pszModule, pszThread);
    else
        snprintf(pszMessage, 1000,
            "UNKNOWN EXCEPTION       \n%s in %s       \n", pszModule, pszThread);
}

void LogException(std::exception* pex, const char* pszThread)
{
    char pszMessage[10000];
    FormatException(pszMessage, pex, pszThread);
    log_error("\n%s", pszMessage);
}

void PrintException(std::exception* pex, const char* pszThread)
{
    char pszMessage[10000];
    FormatException(pszMessage, pex, pszThread);
    log_error("\n\n************************\n%s\n", pszMessage);
    fprintf(stderr, "\n\n************************\n%s\n", pszMessage);
    strMiscWarning = pszMessage;
#ifdef GUI
    if (wxTheApp && !fDaemon)
        MyMessageBox(pszMessage, "Bitcoin", wxOK | wxICON_ERROR);
#endif
    throw;
}
/*
void ThreadOneMessageBox(string strMessage)
{
    // Skip message boxes if one is already open
    static bool fMessageBoxOpen;
    if (fMessageBoxOpen)
        return;
    fMessageBoxOpen = true;
//    ThreadSafeMessageBox(strMessage, "Bitcoin", wxOK | wxICON_EXCLAMATION);
    fMessageBoxOpen = false;
}
*/
void PrintExceptionContinue(std::exception* pex, const char* pszThread)
{
    char pszMessage[10000];
    FormatException(pszMessage, pex, pszThread);
    printf("\n\n************************\n%s\n", pszMessage);
    fprintf(stderr, "\n\n************************\n%s\n", pszMessage);
    strMiscWarning = pszMessage;
#ifdef GUI
    if (wxTheApp && !fDaemon)
        boost::thread(boost::bind(ThreadOneMessageBox, string(pszMessage)));
#endif
}








#ifdef _WIN32
//typedef WINSHELLAPI BOOL (WINAPI *PSHGETSPECIALFOLDERPATHA)(HWND hwndOwner, LPSTR lpszPath, int nFolder, BOOL fCreate);

string MyGetSpecialFolderPath(int nFolder, bool fCreate)
{
    char pszPath[MAX_PATH+100] = "";
/*
    // SHGetSpecialFolderPath isn't always available on old Windows versions
    HMODULE hShell32 = LoadLibraryA("shell32.dll");
    if (hShell32)
    {
        PSHGETSPECIALFOLDERPATHA pSHGetSpecialFolderPath =
            (PSHGETSPECIALFOLDERPATHA)GetProcAddress(hShell32, "SHGetSpecialFolderPathA");
        if (pSHGetSpecialFolderPath)
            (*pSHGetSpecialFolderPath)(NULL, pszPath, nFolder, fCreate);
        FreeModule(hShell32);
    }
*/
    // Backup option
    if (pszPath[0] == '\0')
    {
        if (nFolder == CSIDL_STARTUP)
        {
            strcpy(pszPath, getenv("USERPROFILE"));
            strcat(pszPath, "\\Start Menu\\Programs\\Startup");
        }
        else if (nFolder == CSIDL_APPDATA)
        {
            strcpy(pszPath, getenv("APPDATA"));
        }
    }

    return pszPath;
}

#endif



//
// "Never go to sea with two chronometers; take one or three."
// Our three time sources are:
//  - System clock
//  - Median of other nodes's clocks
//  - The user (asking the user to fix the system clock if the first two disagree)
//
/*
int64_t UnixTime::s()
{
    return time(NULL);
}
*/
static int64_t nTimeOffset = 0;

int64_t GetAdjustedTime()
{
    return UnixTime::s() + nTimeOffset;
}

void AddTimeData(unsigned int ip, int64_t nTime)
{
    int64_t nOffsetSample = nTime - UnixTime::s();

    // Ignore duplicates
    static set<unsigned int> setKnown;
    if (!setKnown.insert(ip).second)
        return;

    // Add data
    static vector<int64_t> vTimeOffsets;
    if (vTimeOffsets.empty())
        vTimeOffsets.push_back(0);
    vTimeOffsets.push_back(nOffsetSample);
    log_debug("Added time data, samples %d, offset %+"PRI64d" (%+"PRI64d" minutes)\n", vTimeOffsets.size(), vTimeOffsets.back(), vTimeOffsets.back()/60);
    if (vTimeOffsets.size() >= 5 && vTimeOffsets.size() % 2 == 1)
    {
        sort(vTimeOffsets.begin(), vTimeOffsets.end());
        int64_t nMedian = vTimeOffsets[vTimeOffsets.size()/2];
        // Only let other nodes change our time by so much
        if (abs64(nMedian) < 70 * 60)
        {
            nTimeOffset = nMedian;
        }
        else
        {
            nTimeOffset = 0;

            static bool fDone;
            if (!fDone)
            {
                // If nobody has a time different than ours but within 5 minutes of ours, give a warning
                bool fMatch = false;
                BOOST_FOREACH(int64_t nOffset, vTimeOffsets)
                    if (nOffset != 0 && abs64(nOffset) < 5 * 60)
                        fMatch = true;

                if (!fMatch)
                {
                    fDone = true;
                    string strMessage = "Warning: Please check that your computer's date and time are correct.  If your clock is wrong Bitcoin will not work properly.";
                    strMiscWarning = strMessage;
                    log_warn("*** %s\n", strMessage.c_str());
//                    boost::thread(boost::bind(ThreadSafeMessageBox, strMessage+" ", string("Bitcoin"), wxOK | wxICON_EXCLAMATION, (wxWindow*)NULL, -1, -1));
                }
            }
        }
        BOOST_FOREACH(int64_t n, vTimeOffsets)
            log_debug("%+d  ", n);
        log_debug("|  nTimeOffset = %+d  (%+d minutes)\n", nTimeOffset, nTimeOffset/60);
    }
}









string FormatVersion(int nVersion)
{
    if (nVersion%100 == 0)
        return strprintf("%d.%d.%d", nVersion/1000000, (nVersion/10000)%100, (nVersion/100)%100);
    else
        return strprintf("%d.%d.%d.%d", nVersion/1000000, (nVersion/10000)%100, (nVersion/100)%100, nVersion%100);
}

// Format the subversion field according to BIP 14 spec (https://en.bitcoin.it/wiki/BIP_0014)
std::string FormatSubVersion(const std::string& name, int nClientVersion, const std::vector<std::string>& comments)
{
    std::ostringstream ss;
    ss << "/";
    ss << name << ":" << FormatVersion(nClientVersion);
    if (!comments.empty())
        ss << "(" << boost::algorithm::join(comments, "; ") << ")";
    ss << "/";
    return ss.str();
}


#ifdef DEBUG_LOCKORDER
//
// Early deadlock detection.
// Problem being solved:
//    Thread 1 locks  A, then B, then C
//    Thread 2 locks  D, then C, then A
//     --> may result in deadlock between the two threads, depending on when they run.
// Solution implemented here:
// Keep track of pairs of locks: (A before B), (A before C), etc.
// Complain if any thread trys to lock in a different order.
//

struct CLockLocation
{
    CLockLocation(const char* pszName, const char* pszFile, int nLine)
    {
        mutexName = pszName;
        sourceFile = pszFile;
        sourceLine = nLine;
    }

    std::string toString() const
    {
        return mutexName+"  "+sourceFile+":"+itostr(sourceLine);
    }

private:
    std::string mutexName;
    std::string sourceFile;
    int sourceLine;
};

typedef std::vector< std::pair<CCriticalSection*, CLockLocation> > LockStack;

static boost::interprocess::interprocess_mutex dd_mutex;
static std::map<std::pair<CCriticalSection*, CCriticalSection*>, LockStack> lockorders;
static boost::thread_specific_ptr<LockStack> lockstack;


static void potential_deadlock_detected(const std::pair<CCriticalSection*, CCriticalSection*>& mismatch, const LockStack& s1, const LockStack& s2)
{
    log_error("POTENTIAL DEADLOCK DETECTED\n");
    log_error("Previous lock order was:\n");
    BOOST_FOREACH(const PAIRTYPE(CCriticalSection*, CLockLocation)& i, s2)
    {
        if (i.first == mismatch.first) log_error(" (1)");
        if (i.first == mismatch.second) log_error(" (2)");
        log_error(" %s\n", i.second.toString().c_str());
    }
    log_error("Current lock order is:\n");
    BOOST_FOREACH(const PAIRTYPE(CCriticalSection*, CLockLocation)& i, s1)
    {
        if (i.first == mismatch.first) log_error(" (1)");
        if (i.first == mismatch.second) log_error(" (2)");
        log_error(" %s\n", i.second.toString().c_str());
    }
}

static void push_lock(CCriticalSection* c, const CLockLocation& locklocation)
{
    bool fOrderOK = true;
    if (lockstack.get() == NULL)
        lockstack.reset(new LockStack);

    if (fDebug) log_debug("Locking: %s\n", locklocation.toString().c_str());
    dd_mutex.lock();

    (*lockstack).push_back(std::make_pair(c, locklocation));

    BOOST_FOREACH(const PAIRTYPE(CCriticalSection*, CLockLocation)& i, (*lockstack))
    {
        if (i.first == c) break;

        std::pair<CCriticalSection*, CCriticalSection*> p1 = std::make_pair(i.first, c);
        if (lockorders.count(p1))
            continue;
        lockorders[p1] = (*lockstack);

        std::pair<CCriticalSection*, CCriticalSection*> p2 = std::make_pair(c, i.first);
        if (lockorders.count(p2))
        {
            potential_deadlock_detected(p1, lockorders[p2], lockorders[p1]);
            break;
        }
    }
    dd_mutex.unlock();
}

static void pop_lock()
{
    if (fDebug) 
    {
        const CLockLocation& locklocation = (*lockstack).rbegin()->second;
        log_debug("Unlocked: %s\n", locklocation.toString().c_str());
    }
    dd_mutex.lock();
    (*lockstack).pop_back();
    dd_mutex.unlock();
}

void CCriticalSection::Enter(const char* pszName, const char* pszFile, int nLine)
{
    push_lock(this, CLockLocation(pszName, pszFile, nLine));
    mutex.lock();
}
void CCriticalSection::Leave()
{
    mutex.unlock();
    pop_lock();
}
bool CCriticalSection::TryEnter(const char* pszName, const char* pszFile, int nLine)
{
    push_lock(this, CLockLocation(pszName, pszFile, nLine));
    bool result = mutex.try_lock();
    if (!result) pop_lock();
    return result;
}

#else

void CCriticalSection::Enter(const char*, const char*, int)
{
    mutex.lock();
}

void CCriticalSection::Leave()
{
    mutex.unlock();
}

bool CCriticalSection::TryEnter(const char*, const char*, int)
{
    bool result = mutex.try_lock();
    return result;
}

#endif /* DEBUG_LOCKORDER */

#if defined(_WIN32)
#include <Windows.h>

#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#if defined(BSD)
#include <sys/sysctl.h>
#endif

#else
#error "Unable to define getMemorySize( ) for an unknown OS."
#endif



/**
 * Returns the size of physical memory (RAM) in bytes.
 */
size_t getMemorySize() {
#if defined(_WIN32) && (defined(__CYGWIN__) || defined(__CYGWIN32__))
	/* Cygwin under Windows. ------------------------------------ */
	/* New 64-bit MEMORYSTATUSEX isn't available.  Use old 32.bit */
	MEMORYSTATUS status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatus( &status );
	return (size_t)status.dwTotalPhys;
    
#elif defined(_WIN32)
	/* Windows. ------------------------------------------------- */
	/* Use new 64-bit MEMORYSTATUSEX, not old 32-bit MEMORYSTATUS */
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx( &status );
	return (size_t)status.ullTotalPhys;
    
#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
	/* UNIX variants. ------------------------------------------- */
	/* Prefer sysctl() over sysconf() except sysctl() HW_REALMEM and HW_PHYSMEM */
    
#if defined(CTL_HW) && (defined(HW_MEMSIZE) || defined(HW_PHYSMEM64))
	int mib[2];
	mib[0] = CTL_HW;
#if defined(HW_MEMSIZE)
	mib[1] = HW_MEMSIZE;            /* OSX. --------------------- */
#elif defined(HW_PHYSMEM64)
	mib[1] = HW_PHYSMEM64;          /* NetBSD, OpenBSD. --------- */
#endif
	int64_t size = 0;               /* 64-bit */
	size_t len = sizeof( size );
	if ( sysctl( mib, 2, &size, &len, NULL, 0 ) == 0 )
		return (size_t)size;
	return 0L;			/* Failed? */
    
#elif defined(_SC_AIX_REALMEM)
	/* AIX. ----------------------------------------------------- */
	return (size_t)sysconf( _SC_AIX_REALMEM ) * (size_t)1024L;
    
#elif defined(_SC_PHYS_PAGES) && defined(_SC_PAGESIZE)
	/* FreeBSD, Linux, OpenBSD, and Solaris. -------------------- */
	return (size_t)sysconf( _SC_PHYS_PAGES ) *
    (size_t)sysconf( _SC_PAGESIZE );
    
#elif defined(_SC_PHYS_PAGES) && defined(_SC_PAGE_SIZE)
	/* Legacy. -------------------------------------------------- */
	return (size_t)sysconf( _SC_PHYS_PAGES ) *
    (size_t)sysconf( _SC_PAGE_SIZE );
    
#elif defined(CTL_HW) && (defined(HW_PHYSMEM) || defined(HW_REALMEM))
	/* DragonFly BSD, FreeBSD, NetBSD, OpenBSD, and OSX. -------- */
	int mib[2];
	mib[0] = CTL_HW;
#if defined(HW_REALMEM)
	mib[1] = HW_REALMEM;		/* FreeBSD. ----------------- */
#elif defined(HW_PYSMEM)
	mib[1] = HW_PHYSMEM;		/* Others. ------------------ */
#endif
	unsigned int size = 0;		/* 32-bit */
	size_t len = sizeof( size );
	if ( sysctl( mib, 2, &size, &len, NULL, 0 ) == 0 )
		return (size_t)size;
	return 0L;			/* Failed? */
#endif /* sysctl and sysconf variants */
    
#else
	return 0L;			/* Unknown OS. */
#endif
}
