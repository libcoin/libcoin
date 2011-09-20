//
//  main.cpp
//  bitcoin
//
//  Created by Michael Grønager on 17/09/11.
//  Copyright 2011 Ceptacle. All rights reserved.
//

#include <iostream>
#include <openssl/pem.h>

#include "db.h"
#include "main.h"
#include "keystore.h"
#include "wallet.h"
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/interprocess/sync/file_lock.hpp>

using namespace std;
using namespace boost;

CWallet* pwalletMain;

//////////////////////////////////////////////////////////////////////////////
//
// Shutdown
//

void ExitTimeout(void* parg)
{
#ifdef __WXMSW__
    Sleep(5000);
    ExitProcess(0);
#endif
}

void Shutdown(void* parg)
{
    static CCriticalSection cs_Shutdown;
    static bool fTaken;
    bool fFirstThread;
    CRITICAL_BLOCK(cs_Shutdown)
    {
        fFirstThread = !fTaken;
        fTaken = true;
    }
    static bool fExit;
    if (fFirstThread)
    {
        fShutdown = true;
        nTransactionsUpdated++;
        DBFlush(false);
        StopNode();
        DBFlush(true);
        boost::filesystem::remove(GetPidFile());
        UnregisterWallet(pwalletMain);
        delete pwalletMain;
        CreateThread(ExitTimeout, NULL);
        Sleep(50);
        printf("Bitcoin exiting\n\n");
        fExit = true;
        exit(0);
    }
    else
    {
        while (!fExit)
            Sleep(500);
        Sleep(100);
        ExitThread(0);
    }
}

void HandleSIGTERM(int)
{
    fRequestShutdown = true;
}

class CWalletImporter : public CWalletDB
{
public:
    CWalletImporter(std::string strFilename, const char* pszMode="r") : CWalletDB(strFilename, pszMode)
    {
    }
    
    int importWallet(CKeyStore& store)
    {
        int nFileVersion = 0;
        
        // Get cursor
        Dbc* pcursor = GetCursor();
        if (!pcursor)
            return DB_CORRUPT;
        
        loop
        {
            // Read next record
            CDataStream ssKey;
            CDataStream ssValue;
            int ret = ReadAtCursor(pcursor, ssKey, ssValue);
            if (ret == DB_NOTFOUND)
                break;
            else if (ret != 0)
                return DB_CORRUPT;
            
            // Unserialize
            // Taking advantage of the fact that pair serialization
            // is just the two items serialized one after the other
            string strType;
            ssKey >> strType;
            
            // read only the keys
            if (strType == "name")
                continue;
            else if (strType == "tx")
                continue;
            else if (strType == "acentry")
                continue;
            else if (strType == "key" || strType == "wkey")
            {
                vector<unsigned char> vchPubKey;
                ssKey >> vchPubKey;
                CKey key;
                if (strType == "key")
                {
                    CPrivKey pkey;
                    ssValue >> pkey;
                    key.SetPrivKey(pkey);
                }
                else
                {
                    CWalletKey wkey;
                    ssValue >> wkey;
                    key.SetPrivKey(wkey.vchPrivKey);
                }
                if (!store.AddKey(key))
                    return DB_CORRUPT;
            }
            else if (strType == "mkey")
                continue;
/*
            {
                unsigned int nID;
                ssKey >> nID;
                CMasterKey kMasterKey;
                ssValue >> kMasterKey;
                if(pwallet->mapMasterKeys.count(nID) != 0)
                    return DB_CORRUPT;
                pwallet->mapMasterKeys[nID] = kMasterKey;
                if (pwallet->nMasterKeyMaxID < nID)
                    pwallet->nMasterKeyMaxID = nID;
            }
 */
            else if (strType == "ckey")
                continue;
/*
            {
                vector<unsigned char> vchPubKey;
                ssKey >> vchPubKey;
                vector<unsigned char> vchPrivKey;
                ssValue >> vchPrivKey;
                if (!pwallet->LoadCryptedKey(vchPubKey, vchPrivKey))
                    return DB_CORRUPT;
            }
 */
            else if (strType == "defaultkey")
                continue;
            else if (strType == "pool")
                continue;
            else if (strType == "version")
                continue;
            else if (strType == "setting")
                continue;
            else if (strType == "minversion")
                continue;
        }
        
        pcursor->close();
        
        return DB_LOAD_OK;
    }
};

typedef pair<uint256, unsigned int> Coin;

class CAsset1 : public CKeyStore
{
public:
    //    typedef COutPoint Coin;
    typedef set<Coin> Coins;
    typedef map<uint256, CTransaction> TxCache;
    typedef set<uint256> TxSet;
    typedef map<uint160, CKey> KeyMap;
private:
    KeyMap _keymap;
    TxCache _tx_cache;
    Coins _coins;
    
public:
    CAsset1() {}
    void addAddress(uint160 hash160) { _keymap[hash160]; }
    void addKey(CKey key) { _keymap[Hash160(key.GetPubKey())] = key; }
    
    set<uint160> getAddresses()
    {
        set<uint160> addresses;
        for(KeyMap::iterator addr = _keymap.begin(); addr != _keymap.end(); ++addr)
            addresses.insert(addr->first);
        return addresses;
    }
    
    virtual bool AddKey(const CKey& key)
    {
        _keymap[Hash160(key.GetPubKey())] = key;
        return true;
    }
    
    virtual bool HaveKey(const CBitcoinAddress &address) const
    {
        uint160 hash160 = address.GetHash160();
        return (_keymap.count(hash160) > 0);
    }
    
    virtual bool GetKey(const CBitcoinAddress &address, CKey& keyOut) const
    {
        uint160 hash160 = address.GetHash160();
        KeyMap::const_iterator pair = _keymap.find(hash160);
        if(pair != _keymap.end())
        {
            keyOut = pair->second;
            return true;
        }
        return false;        
    }
    
};

class CPEMKey : public CKey
{
public:
    void writePrivKey(string fn)
    {
        if(fSet)
        {
            FILE* fp = fopen(fn.c_str(), "w");
            
            EVP_PKEY* evpkey = EVP_PKEY_new();
            EVP_PKEY_set1_EC_KEY(evpkey, pkey);
            
            if (!PEM_write_PrivateKey(fp, evpkey, EVP_aes_256_cbc(), NULL, 0, 0, (void*)"passphrase"))
            {
                /* Error */
            }        
            EVP_PKEY_free(evpkey);
            fclose(fp);
        }
    }
};

int main (int argc, const char * argv[])
{
    // first exercise is to steal the wallet
    CWalletImporter importer("wallet.dat");
    
    CAsset1 asset;
    importer.importWallet(asset);
    // then get the list of btc-addrs of the wallet
    set<uint160> addresses = asset.getAddresses();
    // now make a file pr key and save the private key in each
    for(set<uint160>::iterator addr = addresses.begin(); addr != addresses.end(); ++ addr)
    {
        string btcaddr = CBitcoinAddress(*addr).ToString();

        
        CPEMKey key;
        asset.GetKey(CBitcoinAddress(*addr), key);
        key.writePrivKey(btcaddr + ".pem");
    }
    
    

    return 0;
}

