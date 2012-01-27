// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "coinWallet/MerkleTx.h"
#include "coinWallet/WalletTx.h"
#include "coinWallet/wallet.h"

#include "coinWallet/Crypter.h"

#include <openssl/rand.h>

using namespace std;

int64 CWalletTx::GetDebit() const
{
    if (_inputs.empty())
        return 0;
    if (fDebitCached)
        return nDebitCached;
    nDebitCached = pwallet->GetDebit(*this);
    fDebitCached = true;
    return nDebitCached;
}

int64 CWalletTx::GetCredit(bool fUseCache) const
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

int64 CWalletTx::GetAvailableCredit(bool fUseCache) const
{
    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (isCoinBase() && pwallet->GetBlocksToMaturity(*this) > 0)
        return 0;
    
    if (fUseCache && fAvailableCreditCached)
        return nAvailableCreditCached;
    
    int64 nCredit = 0;
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


int64 CWalletTx::GetChange() const
{
    if (fChangeCached)
        return nChangeCached;
    nChangeCached = pwallet->GetChange(*this);
    fChangeCached = true;
    return nChangeCached;
}


int64 CWalletTx::GetTxTime() const
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

void CWalletTx::GetAmounts(int64& nGeneratedImmature, int64& nGeneratedMature, list<pair<ChainAddress, int64> >& listReceived,
                           list<pair<ChainAddress, int64> >& listSent, int64& nFee, string& strSentAccount) const
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
    int64 nDebit = GetDebit();
    if (nDebit > 0) // debit>0 means we signed/sent this transaction
    {
        int64 nValueOut = getValueOut();
        nFee = nDebit - nValueOut;
    }

    // Sent/received.  Standard client will never generate a send-to-multiple-recipients,
    // but non-standard clients might (so return a list of address/amount pairs)
    BOOST_FOREACH(const Output& txout, _outputs)
    {
        Address address;
        vector<unsigned char> vchPubKey;
        if (!ExtractAddress(txout.script(), NULL, address))
        {
            printf("CWalletTx::GetAmounts: Unknown transaction type found, txid %s\n",
                   this->getHash().toString().c_str());
            address = 0;
        }

        // Don't report 'change' txouts
        if (nDebit > 0 && pwallet->IsChange(txout))
            continue;

        if (nDebit > 0)
            listSent.push_back(make_pair(ChainAddress(pwallet->chain().networkId(), address), txout.value()));

        if (pwallet->IsMine(txout))
            listReceived.push_back(make_pair(ChainAddress(pwallet->chain().networkId(), address), txout.value()));
    }

}

void CWalletTx::GetAccountAmounts(const string& strAccount, int64& nGenerated, int64& nReceived, 
                                  int64& nSent, int64& nFee) const
{
    nGenerated = nReceived = nSent = nFee = 0;

    int64 allGeneratedImmature, allGeneratedMature, allFee;
    allGeneratedImmature = allGeneratedMature = allFee = 0;
    string strSentAccount;
    list<pair<ChainAddress, int64> > listReceived;
    list<pair<ChainAddress, int64> > listSent;
    GetAmounts(allGeneratedImmature, allGeneratedMature, listReceived, listSent, allFee, strSentAccount);

    if (strAccount == "")
        nGenerated = allGeneratedMature;
    if (strAccount == strSentAccount)
    {
        BOOST_FOREACH(const PAIRTYPE(ChainAddress,int64)& s, listSent)
            nSent += s.second;
        nFee = allFee;
    }
    CRITICAL_BLOCK(pwallet->cs_wallet)
    {
        BOOST_FOREACH(const PAIRTYPE(ChainAddress,int64)& r, listReceived)
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
    BOOST_FOREACH(const Input& txin, _inputs)
    vWorkQueue.push_back(txin.prevout().hash);
    
    // This critsect is OK because txdb is already open
    CRITICAL_BLOCK(pwallet->cs_wallet) {
        map<uint256, const CMerkleTx*> mapWalletPrev;
        set<uint256> setAlreadyDone;
        for (int i = 0; i < vWorkQueue.size(); i++) {
            uint256 hash = vWorkQueue[i];
            if (setAlreadyDone.count(hash))
                continue;
            setAlreadyDone.insert(hash);
            
            CMerkleTx tx;
            map<uint256, CWalletTx>::const_iterator mi = pwallet->mapWallet.find(hash);
            if (mi != pwallet->mapWallet.end()) {
                tx = (*mi).second;
                BOOST_FOREACH(const CMerkleTx& txWalletPrev, (*mi).second.vtxPrev)
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
                printf("ERROR: AddSupportingTransactions() : unsupported transaction\n");
                continue;
            }
            
            Block block;
            blockChain.getBlock(tx.getHash(), block);
            int nDepth = tx.setMerkleBranch(block, blockChain);
            vtxPrev.push_back(tx);
            
            if (nDepth < COPY_DEPTH)
                BOOST_FOREACH(const Input& txin, tx._inputs)
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