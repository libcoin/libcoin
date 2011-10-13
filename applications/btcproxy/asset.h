// Copyright (c) 2011 Michael Gronager
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef _BITCOIN_ASSET_H_
#define _BITCOIN_ASSET_H_

#include "btc/tx.h"

#include "btcNode/main.h"

typedef std::pair<uint256, unsigned int> Coin;
typedef std::set<Coin> Coins;

class CAssetSyncronizer
{
public:
    typedef std::set<Coin> Coins;
    virtual void getCreditCoins(uint160 btc, Coins& coins) = 0;
    virtual void getDebitCoins(uint160 btc, Coins& coins) = 0;
    virtual void getCoins(uint160 btc, Coins& coins) = 0;
    
    virtual void getTransaction(const Coin& coin, CTx&) = 0;
};

class CDBAssetSyncronizer : public CAssetSyncronizer
{
public:
    CDBAssetSyncronizer(CTxDB& txdb) : _txdb(txdb) {}
    virtual void getCreditCoins(uint160 btc, Coins& coins);
    virtual void getDebitCoins(uint160 btc, Coins& coins);
    virtual void getCoins(uint160 btc, Coins& coins);
    
    virtual void getTransaction(const Coin& coin, CTx&);
private:
    CTxDB& _txdb;
};

class CAsset;

class CAsset : public CKeyStore
{
public:
    //    typedef COutPoint Coin;
    typedef std::map<uint256, CTransaction> TxCache;
    typedef std::set<uint256> TxSet;
    typedef std::map<uint160, CKey> KeyMap;
private:
    KeyMap _keymap;
    TxCache _tx_cache;
    Coins _coins;
    
public:
    CAsset() {}
    void addAddress(uint160 hash160) { _keymap[hash160]; }
    void addKey(CKey key) { _keymap[Hash160(key.GetPubKey())] = key; }
    
    std::set<uint160> getAddresses();
    
    virtual bool AddKey(const CKey& key);
    
    virtual bool HaveKey(const CBitcoinAddress &address) const;
    
    virtual bool GetKey(const CBitcoinAddress &address, CKey& keyOut) const;
    
    // we might need to override these as well... Depending how well we want to support having only keys with pub/160 part
    //    virtual bool GetPubKey(const CBitcoinAddress &address, std::vector<unsigned char>& vchPubKeyOut) const
    //    virtual std::vector<unsigned char> GenerateNewKey();
    
    const CTransaction& getTx(uint256 hash) const;
    
    uint160 getAddress(const CTxOut& out);
    
    void remote_sync();

    void syncronize(CAssetSyncronizer& sync);
    
    const Coins& getCoins()
    {
        return _coins;
    }
    
    bool isSpendable(Coin coin);
    const int64 value(Coin coin) const;
    
    struct CompValue {
        const CAsset& _asset;
        CompValue(const CAsset& asset) : _asset(asset) {}
        bool operator() (Coin a, Coin b)
        {
            return (_asset.value(a) < _asset.value(b));
        }
    };
    
    int64 balance();    
    int64 spendable_balance();
    
    typedef std::pair<uint160, int64> Payment;
    CTransaction generateTx(std::set<Payment> payments, uint160 changeaddr = 0);
    
    CTransaction generateTx(uint160 to, int64 amount, uint160 change = 0);
};

#endif