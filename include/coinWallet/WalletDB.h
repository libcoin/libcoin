// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_WALLETDB_H
#define BITCOIN_WALLETDB_H

#include <coinWallet/Export.h>
#include <coinWallet/Wallet.h>

#include <coinWallet/BerkeleyDB.h>

#include <coinWallet/WalletTx.h>

extern unsigned int nWalletDBUpdated;

class CAccount;
class CAccountingEntry;
class Wallet;

class CWalletTx;

// helpers to enable reading and writing of uint256

inline CDataStream& operator<<(CDataStream& os, const std::pair<std::string, uint256>& obj) {
    std::ostringstream oss;
    oss << const_varstr(obj.first) << obj.second;
    std::string s = oss.str();
    return os.write(&s[0], s.size());
}

inline CDataStream& operator>>(CDataStream& is, std::pair<std::string, uint256>& obj) {
    std::istringstream iss(is.str());
    std::streampos p = iss.tellg();
    iss >> varstr(obj.first) >> obj.second;
    p -= iss.tellg();
    is.ignore(p);
    return is;
}

inline CDataStream& operator>>(CDataStream& is, uint256& n) {
    std::istringstream iss(is.str());
    std::streampos p = iss.tellg();
    iss >> n;
    p -= iss.tellg();
    is.ignore(p);
    return is;
}

class COINWALLET_EXPORT CKeyPool
{
public:
    int64_t nTime;
    PubKey vchPubKey;
    
    CKeyPool()
    {
        nTime = UnixTime::s();
    }
    
    CKeyPool(const PubKey& vchPubKeyIn)
    {
        nTime = UnixTime::s();
        vchPubKey = vchPubKeyIn;
    }
    
    IMPLEMENT_SERIALIZE
    (
     if (!(nType & SER_GETHASH))
     READWRITE(nVersion);
     READWRITE(nTime);
     READWRITE(vchPubKey);
     )
};

class CMasterKey;

// Changed the WalletDB to be thread safe - keep a static mutex that is locked on write
class COINWALLET_EXPORT CWalletDB : public CDB
{
public:
    CWalletDB(const std::string dataDir, std::string strFilename, const char* pszMode="r+") : CDB(dataDir, strFilename.c_str(), pszMode)
    {
    }
private:
    CWalletDB(const CWalletDB&);
    void operator=(const CWalletDB&);
private:
    static boost::mutex _write;
public:
    bool ReadName(const std::string& strAddress, std::string& strName)
    {
        strName = "";
        return Read(std::make_pair(std::string("name"), strAddress), strName);
    }

    bool WriteName(const std::string& strAddress, const std::string& strName);

    bool EraseName(const std::string& strAddress);

    bool ReadTx(uint256 hash, CWalletTx& wtx)
    {
        return Read(std::make_pair(std::string("tx"), hash), wtx);
    }

    bool WriteTx(uint256 hash, const CWalletTx& wtx) {
        boost::mutex::scoped_lock lock(_write);
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("tx"), hash), wtx);
    }

    bool EraseTx(uint256 hash)
    {
        boost::mutex::scoped_lock lock(_write);
        nWalletDBUpdated++;
        return Erase(std::make_pair(std::string("tx"), hash));
    }

    bool ReadKey(const PubKey& vchPubKey, CPrivKey& vchPrivKey)
    {
        vchPrivKey.clear();
        return Read(std::make_pair(std::string("key"), vchPubKey), vchPrivKey);
    }

    bool WriteKey(const PubKey& vchPubKey, const CPrivKey& vchPrivKey)
    {
        boost::mutex::scoped_lock lock(_write);
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("key"), vchPubKey), vchPrivKey, false);
    }

    bool WriteCryptedKey(const PubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret, bool fEraseUnencryptedKey = true)
    {
        boost::mutex::scoped_lock lock(_write);
        nWalletDBUpdated++;
        if (!Write(std::make_pair(std::string("ckey"), vchPubKey), vchCryptedSecret, false))
            return false;
        if (fEraseUnencryptedKey)
        {
            Erase(std::make_pair(std::string("key"), vchPubKey));
            Erase(std::make_pair(std::string("wkey"), vchPubKey));
        }
        return true;
    }

    bool WriteMasterKey(unsigned int nID, const CMasterKey& kMasterKey)
    {
        boost::mutex::scoped_lock lock(_write);
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("mkey"), nID), kMasterKey, true);
    }
/*
    bool WriteBestBlock(const CBlockLocator& locator)
    {
        boost::mutex::scoped_lock lock(_write);
        nWalletDBUpdated++;
        return Write(std::string("bestblock"), locator);
    }

    bool ReadBestBlock(CBlockLocator& locator)
    {
        return Read(std::string("bestblock"), locator);
    }
*/
    bool ReadDefaultKey(PubKey& vchPubKey)
    {
        vchPubKey.clear();
        return Read(std::string("defaultkey"), vchPubKey);
    }

    bool WriteDefaultKey(const PubKey& vchPubKey)
    {
        boost::mutex::scoped_lock lock(_write);
        nWalletDBUpdated++;
        return Write(std::string("defaultkey"), vchPubKey);
    }

    bool ReadPool(int64_t nPool, CKeyPool& keypool)
    {
        return Read(std::make_pair(std::string("pool"), nPool), keypool);
    }

    bool WritePool(int64_t nPool, const CKeyPool& keypool)
    {
        boost::mutex::scoped_lock lock(_write);
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("pool"), nPool), keypool);
    }

    bool ErasePool(int64_t nPool)
    {
        boost::mutex::scoped_lock lock(_write);
        nWalletDBUpdated++;
        return Erase(std::make_pair(std::string("pool"), nPool));
    }

    template<typename T>
    bool ReadSetting(const std::string& strKey, T& value)
    {
        return Read(std::make_pair(std::string("setting"), strKey), value);
    }

    template<typename T>
    bool WriteSetting(const std::string& strKey, const T& value)
    {
        boost::mutex::scoped_lock lock(_write);
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("setting"), strKey), value);
    }

    bool ReadAccount(const std::string& strAccount, CAccount& account);
    bool WriteAccount(const std::string& strAccount, const CAccount& account);
    bool WriteAccountingEntry(const CAccountingEntry& acentry);
    int64_t GetAccountCreditDebit(const std::string& strAccount);
    void ListAccountCreditDebit(const std::string& strAccount, std::list<CAccountingEntry>& acentries);

    int LoadWallet(Wallet* pwallet);
};

//void ThreadFlushWalletDB(void* parg);
bool BackupWallet(const Wallet& wallet, const std::string& strDest);

#endif
