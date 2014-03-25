// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
#ifndef WALLETTX_H
#define WALLETTX_H

#include <coin/BigNum.h>
#include <coin/Key.h>
#include <coin/Script.h>

#include <coin/Block.h>
#include <coin/MerkleTx.h>

#include <coinChain/BlockChain.h>

#include <coinWallet/Export.h>
#include <coinWallet/serialize.h>

#include <boost/bind.hpp>

class Wallet;
/*
template<typename Stream>
inline unsigned int SerReadWrite(Stream& s, const MerkleTx& obj, int nType, int nVersion, CSerActionGetSerializeSize ser_action)
{
    return serialize_size(obj);
}

template<typename Stream>
inline unsigned int SerReadWrite(Stream& s, const MerkleTx& obj, int nType, int nVersion, CSerActionSerialize ser_action)
{
    std::string str = serialize(obj);
    s.write(&str[0], str.size());
//    ::Serialize(s, obj, nType, nVersion);
    return 0;
}

template<typename Stream>
inline unsigned int SerReadWrite(Stream& s, MerkleTx& obj, int nType, int nVersion, CSerActionUnserialize ser_action)
{
    std::istringstream is(s.str());
    is >> obj;
    s.clear();
//    ::Unserialize(s, obj, nType, nVersion);
    return 0;
}
*/
unsigned int GetSerializeSize(const MerkleTx& obj)
{
    return serialize_size(obj);
    // Tells the size of the object if serialized to this stream
//    return ::GetSerializeSize(obj, nType, nVersion);
}

CDataStream& operator<<(CDataStream& s, const MerkleTx& obj)
{
    std::string str = serialize(obj);
    s.write(&str[0], str.size());
    //    ::Serialize(s, obj, nType, nVersion);
//    return 0;
    
//    ::Serialize(*this, obj, nType, nVersion);
    return s;
}

CDataStream& operator>>(CDataStream& s, MerkleTx& obj)
{
    return s;
//    ::Unserialize(*this, obj, nType, nVersion);
//    return (*this);
}

inline unsigned int GetSerializeSize(const MerkleTx& a, long nType, int nVersion=PROTOCOL_VERSION)
{
    return serialize_size(a);
//    return a.GetSerializeSize((int)nType, nVersion);
}

template<typename Stream>
inline void Serialize(Stream& os, const MerkleTx& a, long nType, int nVersion=PROTOCOL_VERSION)
{
    os << a;
//    a.Serialize(os, (int)nType, nVersion);
}

template<typename Stream>
inline void Unserialize(Stream& is, MerkleTx& a, long nType, int nVersion=PROTOCOL_VERSION)
{
    is >> a;
//    a.Unserialize(is, (int)nType, nVersion);
}

/// A transaction with a bunch of additional info that only the owner cares
/// about.  It includes any unrecorded transactions needed to link it back
/// to the block chain.
class COINWALLET_EXPORT CWalletTx : public MerkleTx
{
public:
    Wallet* pwallet;

    std::vector<MerkleTx> vtxPrev;
    std::map<std::string, std::string> mapValue;
    std::vector<std::pair<std::string, std::string> > vOrderForm;
    unsigned int fTimeReceivedIsTxTime;
    unsigned int nTimeReceived;  // time received by this node
    char fFromMe;
    std::string strFromAccount;
    std::vector<char> vfSpent;

    // memory only
    mutable char fDebitCached;
    mutable char fCreditCached;
    mutable char fAvailableCreditCached;
    mutable char fChangeCached;
    mutable int64_t nDebitCached;
    mutable int64_t nCreditCached;
    mutable int64_t nAvailableCreditCached;
    mutable int64_t nChangeCached;

    // memory only UI hints
    mutable unsigned int nTimeDisplayed;
    mutable int nLinesDisplayed;
    mutable char fConfirmedDisplayed;

    CWalletTx()
    {
        Init(NULL);
    }

    CWalletTx(Wallet* pwalletIn)
    {
        Init(pwalletIn);
    }

    CWalletTx(Wallet* pwalletIn, const MerkleTx& txIn) : MerkleTx(txIn)
    {
        Init(pwalletIn);
    }

    CWalletTx(Wallet* pwalletIn, const Transaction& txIn) : MerkleTx(txIn)
    {
        Init(pwalletIn);
    }

    void Init(Wallet* pwalletIn)
    {
        pwallet = pwalletIn;
        vtxPrev.clear();
        mapValue.clear();
        vOrderForm.clear();
        fTimeReceivedIsTxTime = false;
        nTimeReceived = 0;
        fFromMe = false;
        strFromAccount.clear();
        vfSpent.clear();
        fDebitCached = false;
        fCreditCached = false;
        fAvailableCreditCached = false;
        fChangeCached = false;
        nDebitCached = 0;
        nCreditCached = 0;
        nAvailableCreditCached = 0;
        nChangeCached = 0;
        nTimeDisplayed = 0;
        nLinesDisplayed = 0;
        fConfirmedDisplayed = false;
    }

    IMPLEMENT_SERIALIZE
    (
        CWalletTx* pthis = const_cast<CWalletTx*>(this);
        if (fRead)
            pthis->Init(NULL);
        char fSpent = false;

        if (!fRead)
        {
            pthis->mapValue["fromaccount"] = pthis->strFromAccount;

            std::string str;
            BOOST_FOREACH(char f, vfSpent)
            {
                str += (f ? '1' : '0');
                if (f)
                    fSpent = true;
            }
            pthis->mapValue["spent"] = str;
        }

        nSerSize += SerReadWrite(s, *(MerkleTx*)this, nType, nVersion,ser_action);
        READWRITE(vtxPrev);
        READWRITE(mapValue);
        READWRITE(vOrderForm);
        READWRITE(fTimeReceivedIsTxTime);
        READWRITE(nTimeReceived);
        READWRITE(fFromMe);
        READWRITE(fSpent);

        if (fRead)
        {
            pthis->strFromAccount = pthis->mapValue["fromaccount"];

            if (mapValue.count("spent"))
                BOOST_FOREACH(char c, pthis->mapValue["spent"])
                    pthis->vfSpent.push_back(c != '0');
            else
                pthis->vfSpent.assign(getNumOutputs(), fSpent);
        }

        pthis->mapValue.erase("fromaccount");
        pthis->mapValue.erase("version");
        pthis->mapValue.erase("spent");
    )

    // marks certain txout's as spent
    // returns true if any update took place
    bool UpdateSpent(const std::vector<char>& vfNewSpent)
    {
        bool fReturn = false;
        for (int i=0; i < vfNewSpent.size(); i++)
        {
            if (i == vfSpent.size())
                break;

            if (vfNewSpent[i] && !vfSpent[i])
            {
                vfSpent[i] = true;
                fReturn = true;
                fAvailableCreditCached = false;
            }
        }
        return fReturn;
    }

    void MarkDirty()
    {
        fCreditCached = false;
        fAvailableCreditCached = false;
        fDebitCached = false;
        fChangeCached = false;
    }

    void MarkSpent(unsigned int nOut)
    {
        if (nOut >= _outputs.size())
            throw std::runtime_error("CWalletTx::MarkSpent() : nOut out of range");
        vfSpent.resize(_outputs.size());
        if (!vfSpent[nOut])
        {
            vfSpent[nOut] = true;
            fAvailableCreditCached = false;
        }
    }

    bool IsSpent(unsigned int nOut) const
    {
        if (nOut >= _outputs.size())
            throw std::runtime_error("CWalletTx::IsSpent() : nOut out of range");
        if (nOut >= vfSpent.size())
            return false;
        return (!!vfSpent[nOut]);
    }

    int64_t GetDebit() const;
    int64_t GetCredit(bool fUseCache=true) const;
    int64_t GetAvailableCredit(bool fUseCache=true) const;
    int64_t GetChange() const;

    void GetAmounts(int64_t& nGeneratedImmature, int64_t& nGeneratedMature, std::list<std::pair<ChainAddress, int64_t> >& listReceived,
                    std::list<std::pair<ChainAddress, int64_t> >& listSent, int64_t& nFee, std::string& strSentAccount) const;

    void GetAccountAmounts(const std::string& strAccount, int64_t& nGenerated, int64_t& nReceived, 
                           int64_t& nSent, int64_t& nFee) const;

    bool IsFromMe() const
    {
        return (GetDebit() > 0);
    }

    // IsConfirmed removed - (could call the BlockChain through the wallet though - put it back ?
    
    bool WriteToDisk();

    int64_t GetTxTime() const;
    int GetRequestCount() const;

    void AddSupportingTransactions(const BlockChain& blockChain);

    //   bool AcceptWalletTransaction(CTxDB& txdb, bool fCheckInputs=true);
    //    bool AcceptWalletTransaction();

    //    void RelayWalletTransaction(CTxDB& txdb);
    //    void RelayWalletTransaction();
};

#endif
