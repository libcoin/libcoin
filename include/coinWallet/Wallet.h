// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_WALLET_H
#define BITCOIN_WALLET_H

#include <coin/BigNum.h>
#include <coin/Key.h>
#include <coin/Script.h>

#include <coin/Block.h>
#include <coinChain/Node.h>

#include <coinWallet/Export.h>
#include <coinWallet/BerkeleyDB.h>
#include <coinWallet/CryptoKeyStore.h>
#include <coinWallet/WalletTx.h>

#include <boost/bind.hpp>

class CWalletDB;

class CReserveKey;
class CKeyPool;

class COINWALLET_EXPORT TransactionListener : public TransactionFilter::Listener {
public:
    TransactionListener(Wallet& wallet) : _wallet(wallet) {}
    virtual void operator()(const Transaction& tx);
private:
    Wallet& _wallet;
};

class COINWALLET_EXPORT BlockListener : public BlockFilter::Listener {
public:
    BlockListener(Wallet& wallet) : _wallet(wallet) {}
    virtual void operator()(const Block& blk);
private:
    Wallet& _wallet;
};

class COINWALLET_EXPORT Wallet : public CCryptoKeyStore
{
private:
    bool SelectCoinsMinConf(int64_t nTargetValue, int nConfMine, int nConfTheirs, std::set<std::pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64_t& nValueRet) const;
    bool SelectCoins(int64_t nTargetValue, std::set<std::pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64_t& nValueRet) const;

    CWalletDB *pwalletdbEncryption;

public:
    mutable CCriticalSection cs_wallet;
    int64_t nTransactionFee;

    bool fFileBacked;
    std::string strWalletFile;
    std::string _dataDir;
    
    std::set<int64_t> setKeyPool;

    typedef std::map<unsigned int, CMasterKey> MasterKeyMap;
    MasterKeyMap mapMasterKeys;
    unsigned int nMasterKeyMaxID;

    class TransactionEmitter : private boost::noncopyable {
    public:
        TransactionEmitter(Node& node) : _node(node) {}
        void operator()(const Transaction& tx) {
            _node.post(tx);
        }
    private:
        Node& _node; 
    };

    Wallet(Node& node, std::string walletFile = "", std::string dataDir = "") : CCryptoKeyStore(), nTransactionFee(0), _blockChain(node.blockChain()), _emit(node), _resend_timer(node.get_io_service()) {
        if(walletFile == "NOTFILEBACKED")
            fFileBacked = false;
        else {
            _dataDir = (dataDir == "") ? default_data_dir(_blockChain.chain().dataDirSuffix()) : dataDir;
            strWalletFile = (walletFile == "") ? strWalletFile = "wallet.dat" : walletFile;
            fFileBacked = true;
        }
        nMasterKeyMaxID = 0;
        pwalletdbEncryption = NULL;
        
        // install callbacks to get notified about new tx'es and blocks
        node.subscribe(TransactionFilter::listener_ptr(new TransactionListener(*this)));
        node.subscribe(BlockFilter::listener_ptr(new BlockListener(*this)));
        
        bool firstRun = false;
        LoadWallet(firstRun);
        if(firstRun) log_info("Created a new wallet!");
        
        // Do this infrequently and randomly to avoid giving away
        // that these are our transactions.
        _resend_timer.expires_from_now(boost::posix_time::seconds(GetRand(30 * 60)));
//        _resend_timer.async_wait(boost::bind(&Wallet::resend, this));
    }

    const Chain& chain() const { return _blockChain.chain(); }
    
    /// acceptTransaction is a thread safe way to post transaction to the chain network. It first checks the transaction, if it can be send and then it emits it through the TransactionEmitter that connects to the Node to run a acceptTransaction in the proper thread.
    bool acceptTransaction(const Transaction& tx) {
        if(_blockChain.checkTransaction(tx)) {
            _emit(tx);
            return true;
        }
        else
            return false;
    }
    
    void transactionAccepted(const Transaction& tx) {
        AddToWalletIfInvolvingMe(tx, NULL, true);
    }
    void blockAccepted(const Block& b) {
        TransactionList txes = b.getTransactions();
        for(TransactionList::const_iterator tx = txes.begin(); tx != txes.end(); ++tx)
            AddToWalletIfInvolvingMe(*tx, &b, true);
    }
    /*
    void commit(const Transaction tx) {
        _node.post(this, Commit, tx); // call this on Node thread...
    }
    */
    
    bool isFinal(const Transaction& tx) const { return _blockChain.isFinal(tx); }
    bool isInMainChain(const uint256& hash) const { return _blockChain.isInMainChain(hash); }
    
    std::string getDataDir() const { return _dataDir; }
    
    bool WriteToDisk(const CWalletTx& wtx);
    
    std::map<uint256, CWalletTx> mapWallet;
    std::vector<uint256> vWalletUpdated;

    std::map<uint256, int> mapRequestCount;

    std::map<ChainAddress, std::string> mapAddressBook;

    std::vector<unsigned char> vchDefaultKey;

    // keystore implementation
    bool addKey(const CKey& key);
    bool LoadKey(const CKey& key) { return CCryptoKeyStore::addKey(key); }
    bool addCryptedKey(const std::vector<unsigned char> &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret);
    bool LoadCryptedKey(const std::vector<unsigned char> &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret) { return CCryptoKeyStore::addCryptedKey(vchPubKey, vchCryptedSecret); }

    bool Unlock(const SecureString& strWalletPassphrase);
    bool ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase);
    bool EncryptWallet(const SecureString& strWalletPassphrase);

    bool AddToWallet(const CWalletTx& wtxIn);
    bool AddToWalletIfInvolvingMe(const Transaction& tx, const Block* pblock, bool fUpdate = false);
    bool EraseFromWallet(uint256 hash);
    void WalletUpdateSpent(const Transaction& prevout);
    //    int ScanForWalletTransactions(const CBlockIndex* pindexStart = NULL, bool fUpdate = false);
    void ReacceptWalletTransactions();
    void resend();
    int64_t GetBalance(bool confirmed = true) const;
    bool CreateTransaction(const std::vector<std::pair<Script, int64_t> >& vecSend, CWalletTx& wtxNew, CReserveKey& reservekey, int64_t& nFeeRet);
    bool CreateTransaction(Script scriptPubKey, int64_t nValue, CWalletTx& wtxNew, CReserveKey& reservekey, int64_t& nFeeRet);
    bool CommitTransaction(CWalletTx& wtxNew, CReserveKey& reservekey);
    bool BroadcastTransaction(CWalletTx& wtxNew);
    std::string SendMoney(Script scriptPubKey, int64_t nValue, CWalletTx& wtxNew, bool fAskFee=false);
    std::string SendMoneyToBitcoinAddress(const ChainAddress& address, int64_t nValue, CWalletTx& wtxNew, bool fAskFee=false);

    bool TopUpKeyPool();
    void ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypool);
    void KeepKey(int64_t nIndex);
    void ReturnKey(int64_t nIndex);
    bool GetKeyFromPool(std::vector<unsigned char> &key, bool fAllowReuse=true);
    int64_t GetOldestKeyPoolTime();

    bool IsMine(const Input& txin) const;
    int64_t GetDebit(const Input& txin) const;
    bool IsMine(const Output& txout) const
    {
        return ::IsMine(*this, txout.script());
    }
    int64_t GetCredit(const Output& txout) const
    {
        if (!MoneyRange(txout.value()))
            throw std::runtime_error("Wallet::GetCredit() : value out of range");
        return (IsMine(txout) ? txout.value() : 0);
    }
    bool IsChange(const Output& txout) const
    {
        PubKeyHash pubKeyHash;
        ScriptHash scriptHash;
        if (ExtractAddress(txout.script(), pubKeyHash, scriptHash)) {
            ChainAddress address;
            if (pubKeyHash != 0)
                address = chain().getAddress(pubKeyHash);
            else if (scriptHash != 0)
                address = chain().getAddress(scriptHash);
            else
                return false;
                
            CRITICAL_BLOCK(cs_wallet)
                if (!mapAddressBook.count(address))
                    return true;
    }
        return false;
    }
    int64_t GetChange(const Output& txout) const
    {
        if (!MoneyRange(txout.value()))
            throw std::runtime_error("Wallet::GetChange() : value out of range");
        return (IsChange(txout) ? txout.value() : 0);
    }
    bool IsMine(const Transaction& tx) const
    {
        BOOST_FOREACH(const Output& txout, tx.getOutputs())
            if (IsMine(txout))
                return true;
        return false;
    }
    bool IsFromMe(const Transaction& tx) const
    {
        return (GetDebit(tx) > 0);
    }
    
    bool IsConfirmed(const CWalletTx& tx) const;
    
    int64_t GetDebit(const Transaction& tx) const
    {
        int64_t nDebit = 0;
        BOOST_FOREACH(const Input& txin, tx.getInputs())
        {
            nDebit += GetDebit(txin);
            if (!MoneyRange(nDebit))
                throw std::runtime_error("Wallet::GetDebit() : value out of range");
        }
        return nDebit;
    }
    int64_t GetCredit(const Transaction& tx) const
    {
        int64_t nCredit = 0;
        BOOST_FOREACH(const Output& txout, tx.getOutputs())
        {
            nCredit += GetCredit(txout);
            if (!MoneyRange(nCredit))
                throw std::runtime_error("Wallet::GetCredit() : value out of range");
        }
        return nCredit;
    }
    int64_t GetChange(const Transaction& tx) const
    {
        int64_t nChange = 0;
        BOOST_FOREACH(const Output& txout, tx.getOutputs())
        {
            nChange += GetChange(txout);
            if (!MoneyRange(nChange))
                throw std::runtime_error("Wallet::GetChange() : value out of range");
        }
        return nChange;
    }
    //    void SetBestChain(const CBlockLocator& loc);

    int GetBlocksToMaturity(const Transaction& tx) const {
        return _blockChain.getBlocksToMaturity(tx);
    }
    
    int getDepthInMainChain(const uint256 hash) const { return _blockChain.getDepthInMainChain(hash); }
    int getHeight(const uint256 hash) const { return _blockChain.getHeight(hash); }
    int getBestHeight() const { return _blockChain.getBestHeight(); }

    void getTransaction(const uint256 hash, Transaction& tx) const { _blockChain.getTransaction(hash, tx); }
    
    int LoadWallet(bool& fFirstRunRet);
//    bool BackupWallet(const std::string& strDest);

    bool SetAddressBookName(const ChainAddress& address, const std::string& strName);

    bool DelAddressBookName(const ChainAddress& address);

    void UpdatedTransaction(const uint256 &hashTx)
    {
        CRITICAL_BLOCK(cs_wallet)
            vWalletUpdated.push_back(hashTx);
    }

    void PrintWallet(const Block& block);

    void Inventory(const uint256 &hash)
    {
        CRITICAL_BLOCK(cs_wallet)
        {
            std::map<uint256, int>::iterator mi = mapRequestCount.find(hash);
            if (mi != mapRequestCount.end())
                (*mi).second++;
        }
    }

    int GetKeyPoolSize()
    {
        return setKeyPool.size();
    }

    bool GetTransaction(const uint256 &hashTx, CWalletTx& wtx);

    bool SetDefaultKey(const std::vector<unsigned char> &vchPubKey);
    
    //    void Sync();
private:
    const BlockChain& _blockChain;
    TransactionEmitter _emit;
    boost::asio::deadline_timer _resend_timer;
};

class COINWALLET_EXPORT CReserveKey
{
protected:
    Wallet* pwallet;
    int64_t nIndex;
    std::vector<unsigned char> vchPubKey;
public:
    CReserveKey(Wallet* pwalletIn)
    {
        nIndex = -1;
        pwallet = pwalletIn;
    }

    ~CReserveKey()
    {
        ReturnKey();
    }

    void ReturnKey();
    std::vector<unsigned char> GetReservedKey();
    void KeepKey();
};


//
// Private key that includes an expiration date in case it never gets used.
//
class COINWALLET_EXPORT CWalletKey
{
public:
    CPrivKey vchPrivKey;
    int64_t nTimeCreated;
    int64_t nTimeExpires;
    std::string strComment;
    //// todo: add something to note what created it (user, getnewaddress, change)
    ////   maybe should have a map<string, string> property map

    CWalletKey(int64_t nExpires=0)
    {
        nTimeCreated = (nExpires ? UnixTime::s() : 0);
        nTimeExpires = nExpires;
    }

    IMPLEMENT_SERIALIZE
    (
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vchPrivKey);
        READWRITE(nTimeCreated);
        READWRITE(nTimeExpires);
        READWRITE(strComment);
    )
};






//
// Account information.
// Stored in wallet with key "acc"+string account name
//
class COINWALLET_EXPORT CAccount
{
public:
    std::vector<unsigned char> vchPubKey;

    CAccount()
    {
        SetNull();
    }

    void SetNull()
    {
        vchPubKey.clear();
    }

    IMPLEMENT_SERIALIZE
    (
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vchPubKey);
    )
};



//
// Internal transfers.
// Database key is acentry<account><counter>
//
class COINWALLET_EXPORT CAccountingEntry
{
public:
    std::string strAccount;
    int64_t nCreditDebit;
    int64_t nTime;
    std::string strOtherAccount;
    std::string strComment;

    CAccountingEntry()
    {
        SetNull();
    }

    void SetNull()
    {
        nCreditDebit = 0;
        nTime = 0;
        strAccount.clear();
        strOtherAccount.clear();
        strComment.clear();
    }

    IMPLEMENT_SERIALIZE
    (
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        // Note: strAccount is serialized as part of the key, not here.
        READWRITE(nCreditDebit);
        READWRITE(nTime);
        READWRITE(strOtherAccount);
        READWRITE(strComment);
    )
};

bool GetWalletFile(Wallet* pwallet, std::string &strWalletFileOut);

#endif
