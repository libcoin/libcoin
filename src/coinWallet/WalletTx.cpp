// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include <coin/MerkleTx.h>

#include <coinWallet/WalletTx.h>
#include <coinWallet/Wallet.h>

#include <coinWallet/Crypter.h>

#include <coin/Logger.h>

#include <openssl/rand.h>

using namespace std;

int64_t CWalletTx::GetDebit() const
{
    if (getInputs().empty())
        return 0;
    if (fDebitCached)
        return nDebitCached;
    nDebitCached = pwallet->GetDebit(*this);
    fDebitCached = true;
    return nDebitCached;
}

int64_t CWalletTx::GetCredit(bool fUseCache) const
{
    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (isCoinBase() && pwallet->GetBlocksToMaturity(*this) > 0)
        return 0;
    
    // GetBalance can assume transactions in mapWallet won't change
    if (fUseCache && fCreditCached)
        return nCreditCached;
    nCreditCached = pwallet->GetCredit(*this);
    fCreditCached = true;
    return nCreditCached;
}

int64_t CWalletTx::GetAvailableCredit(bool fUseCache) const
{
    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (isCoinBase() && pwallet->GetBlocksToMaturity(*this) > 0)
        return 0;
    
    if (fUseCache && fAvailableCreditCached)
        return nAvailableCreditCached;
    
    int64_t nCredit = 0;
    for (int i = 0; i < _outputs.size(); i++)
        {
        if (!IsSpent(i))
            {
            const Output &txout = _outputs[i];
            nCredit += pwallet->GetCredit(txout);
            if (!MoneyRange(nCredit))
                throw std::runtime_error("CWalletTx::GetAvailableCredit() : value out of range");
            }
        }
    
    nAvailableCreditCached = nCredit;
    fAvailableCreditCached = true;
    return nCredit;
}


int64_t CWalletTx::GetChange() const
{
    if (fChangeCached)
        return nChangeCached;
    nChangeCached = pwallet->GetChange(*this);
    fChangeCached = true;
    return nChangeCached;
}


int64_t CWalletTx::GetTxTime() const
{
    return nTimeReceived;
}

int CWalletTx::GetRequestCount() const
{
    // Returns -1 if it wasn't being tracked
    int nRequests = -1;
    CRITICAL_BLOCK(pwallet->cs_wallet)
    {
        if (isCoinBase())
        {
            // Generated block
            if (_blockHash != 0)
            {
                map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(_blockHash);
                if (mi != pwallet->mapRequestCount.end())
                    nRequests = (*mi).second;
            }
        }
        else
        {
            // Did anyone request this transaction?
            map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(getHash());
            if (mi != pwallet->mapRequestCount.end())
            {
                nRequests = (*mi).second;

                // How about the block it's in?
                if (nRequests == 0 && _blockHash != 0)
                {
                    map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(_blockHash);
                    if (mi != pwallet->mapRequestCount.end())
                        nRequests = (*mi).second;
                    else
                        nRequests = 1; // If it's in someone else's block it must have got out
                }
            }
        }
    }
    return nRequests;
}

void CWalletTx::GetAmounts(int64_t& nGeneratedImmature, int64_t& nGeneratedMature, list<pair<ChainAddress, int64_t> >& listReceived,
                           list<pair<ChainAddress, int64_t> >& listSent, int64_t& nFee, string& strSentAccount) const
{
    nGeneratedImmature = nGeneratedMature = nFee = 0;
    listReceived.clear();
    listSent.clear();
    strSentAccount = strFromAccount;

    if (isCoinBase())
    {
        if (pwallet->GetBlocksToMaturity(*this) > 0)
            nGeneratedImmature = pwallet->GetCredit(*this);
        else
            nGeneratedMature = GetCredit();
        return;
    }

    // Compute fee:
    int64_t nDebit = GetDebit();
    if (nDebit > 0) // debit>0 means we signed/sent this transaction
    {
        int64_t nValueOut = getValueOut();
        nFee = nDebit - nValueOut;
    }

    // Sent/received.  Standard client will never generate a send-to-multiple-recipients,
    // but non-standard clients might (so return a list of address/amount pairs)
    BOOST_FOREACH(const Output& txout, _outputs)
    {
        PubKeyHash pubKeyHash;
        ScriptHash scriptHash; // Didn't include support for P2SH yet!!!
        PubKey vchPubKey;
        if (!ExtractAddress(txout.script(), pubKeyHash, scriptHash))
        {
            log_warn("CWalletTx::GetAmounts: Unknown transaction type found, txid %s\n",
                   this->getHash().toString().c_str());
            pubKeyHash = 0;
        }

        // Don't report 'change' txouts
        if (nDebit > 0 && pwallet->IsChange(txout))
            continue;

        if (nDebit > 0)
            listSent.push_back(make_pair(pwallet->chain().getAddress(pubKeyHash), txout.value()));

        if (pwallet->IsMine(txout))
            listReceived.push_back(make_pair(pwallet->chain().getAddress(pubKeyHash), txout.value()));
    }

}

void CWalletTx::GetAccountAmounts(const string& strAccount, int64_t& nGenerated, int64_t& nReceived, 
                                  int64_t& nSent, int64_t& nFee) const
{
    nGenerated = nReceived = nSent = nFee = 0;

    int64_t allGeneratedImmature, allGeneratedMature, allFee;
    allGeneratedImmature = allGeneratedMature = allFee = 0;
    string strSentAccount;
    list<pair<ChainAddress, int64_t> > listReceived;
    list<pair<ChainAddress, int64_t> > listSent;
    GetAmounts(allGeneratedImmature, allGeneratedMature, listReceived, listSent, allFee, strSentAccount);

    if (strAccount == "")
        nGenerated = allGeneratedMature;
    if (strAccount == strSentAccount)
    {
        BOOST_FOREACH(const PAIRTYPE(ChainAddress,int64_t)& s, listSent)
            nSent += s.second;
        nFee = allFee;
    }
    CRITICAL_BLOCK(pwallet->cs_wallet)
    {
        BOOST_FOREACH(const PAIRTYPE(ChainAddress,int64_t)& r, listReceived)
        {
            if (pwallet->mapAddressBook.count(r.first))
            {
                map<ChainAddress, string>::const_iterator mi = pwallet->mapAddressBook.find(r.first);
                if (mi != pwallet->mapAddressBook.end() && (*mi).second == strAccount)
                    nReceived += r.second;
            }
            else if (strAccount.empty())
            {
                nReceived += r.second;
            }
        }
    }
}

// Add supporting transactions only gets called by the wallet function Create Transaction and that on a newly created tx, hence the setMerkleBranch will immidiately exit with a 0. I have hence chosen to drop this part of this code...
void CWalletTx::AddSupportingTransactions(const BlockChain& blockChain)
{
    vtxPrev.clear();
    
    const int COPY_DEPTH = 3;
    //    if (setMerkleBranch(block, blockChain) < COPY_DEPTH)
    //{
    vector<uint256> vWorkQueue;
    BOOST_FOREACH(const Input& txin, getInputs())
    vWorkQueue.push_back(txin.prevout().hash);
    
    // This critsect is OK because txdb is already open
    CRITICAL_BLOCK(pwallet->cs_wallet) {
        map<uint256, const MerkleTx*> mapWalletPrev;
        set<uint256> setAlreadyDone;
        for (int i = 0; i < vWorkQueue.size(); i++) {
            uint256 hash = vWorkQueue[i];
            if (setAlreadyDone.count(hash))
                continue;
            setAlreadyDone.insert(hash);
            
            MerkleTx tx;
            map<uint256, CWalletTx>::const_iterator mi = pwallet->mapWallet.find(hash);
            if (mi != pwallet->mapWallet.end()) {
                tx = (*mi).second;
                BOOST_FOREACH(const MerkleTx& txWalletPrev, (*mi).second.vtxPrev)
                mapWalletPrev[txWalletPrev.getHash()] = &txWalletPrev;
            }
            else if (mapWalletPrev.count(hash)) {
                tx = *mapWalletPrev[hash];
            }
            //            else if (!fClient && txdb.ReadDiskTx(hash, tx)) {
            else if (blockChain.getTransaction(hash, tx), !tx.isNull()) {
                ;
            }
            else {
                log_warn("ERROR: AddSupportingTransactions() : unsupported transaction\n");
                continue;
            }
            
//            Block block;
//            blockChain.getBlock(tx.getHash(), block);
            int nDepth = blockChain.setMerkleBranch(tx);
            vtxPrev.push_back(tx);
            
            if (nDepth < COPY_DEPTH)
                BOOST_FOREACH(const Input& txin, tx.getInputs())
                vWorkQueue.push_back(txin.prevout().hash);
        }
    }
    //}
    
    reverse(vtxPrev.begin(), vtxPrev.end());
}

bool CWalletTx::WriteToDisk()
{
    return pwallet->WriteToDisk(*this);
}
/*
bool CWalletTx::AcceptWalletTransaction(CTxDB& txdb, bool fCheckInputs)
{
    CRITICAL_BLOCK(cs_mapTransactions)
    {
        // Add previous supporting transactions first
        BOOST_FOREACH(CMerkleTx& tx, vtxPrev)
        {
            if (!tx.IsCoinBase())
            {
                uint256 hash = tx.GetHash();
                if (!mapTransactions.count(hash) && !txdb.ContainsTx(hash))
                    tx.AcceptToMemoryPool(txdb, fCheckInputs);
            }
        }
        return AcceptToMemoryPool(txdb, fCheckInputs);
    }
    return false;
}

bool CWalletTx::AcceptWalletTransaction() 
{
    CTxDB txdb("r");
    return AcceptWalletTransaction(txdb);
}

void CWalletTx::RelayWalletTransaction(CTxDB& txdb)
{
    BOOST_FOREACH(const CMerkleTx& tx, vtxPrev)
    {
        if (!tx.IsCoinBase())
        {
            uint256 hash = tx.GetHash();
            if (!txdb.ContainsTx(hash))
                RelayMessage(Inventory(MSG_TX, hash), (Transaction)tx);
        }
    }
    if (!IsCoinBase())
    {
        uint256 hash = GetHash();
        if (!txdb.ContainsTx(hash))
        {
            printf("Relaying wtx %s\n", hash.toString().substr(0,10).c_str());
            RelayMessage(Inventory(MSG_TX, hash), (Transaction)*this);
        }
    }
}

void CWalletTx::RelayWalletTransaction()
{
   CTxDB txdb("r");
   RelayWalletTransaction(txdb);
}

*/