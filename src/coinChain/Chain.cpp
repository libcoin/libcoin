#include "coinChain/Chain.h"

#include "coin/Block.h"
#include "coinChain/BlockIndex.h"
#include "coin/Transaction.h"

using namespace std;
using namespace boost;

BitcoinChain::BitcoinChain() : _genesis("0x000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f") {
    _messageStart[0] = 0xf9; _messageStart[1] = 0xbe; _messageStart[2] = 0xb4; _messageStart[3] = 0xd9;
    const char* pszTimestamp = "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";
    Transaction txNew;
    Script signature = Script() << 486604799 << CBigNum(4) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.addInput(Input(Coin(), signature)); 
    Script script = Script() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
    txNew.addOutput(Output(50 * COIN, script)); 
    _genesisBlock = Block(1, 0, 0, 1231006505, 0x1d00ffff, 2083236893);
    _genesisBlock.addTransaction(txNew);
    _genesisBlock.updateMerkleTree(); // genesisBlock
    assert(_genesisBlock.getHash() == _genesis);
}

const Block& BitcoinChain::genesisBlock() const {
    return _genesisBlock;
}

const int64 BitcoinChain::subsidy(unsigned int height) const {
    int64 s = 50 * COIN;
    
    // Subsidy is cut in half every 4 years
    s >>= (height / 210000);
    
    return s;
}

bool BitcoinChain::isStandard(const Transaction& tx) const {
    BOOST_FOREACH(const Input& txin, tx.getInputs())
        if (!txin.signature().isPushOnly())
            return error("nonstandard txin: %s", txin.signature().toString().c_str());
    BOOST_FOREACH(const Output& txout, tx.getOutputs()) {
        vector<pair<opcodetype, valtype> > solution;
        if (!Solver(txout.script(), solution))
            return error("nonstandard txout: %s", txout.script().toString().c_str());        
    }
    return true;
}

unsigned int BitcoinChain::nextWorkRequired(const CBlockIndex* pindexLast) const {
    const int64 nTargetTimespan = 14 * 24 * 60 * 60; // two weeks
    const int64 nTargetSpacing = 10 * 60;
    const int64 nInterval = nTargetTimespan / nTargetSpacing;
    
    // Genesis block
    if (pindexLast == NULL)
        return proofOfWorkLimit().GetCompact();
    
    // Only change once per interval
    if ((pindexLast->nHeight+1) % nInterval != 0)
        return pindexLast->nBits;
    
    // Go back by what we want to be 14 days worth of blocks
    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < nInterval-1; i++)
        pindexFirst = pindexFirst->pprev;
    assert(pindexFirst);
    
    // Limit adjustment step
    int64 nActualTimespan = pindexLast->GetBlockTime() - pindexFirst->GetBlockTime();
    printf("  nActualTimespan = %"PRI64d"  before bounds\n", nActualTimespan);
    if (nActualTimespan < nTargetTimespan/4)
        nActualTimespan = nTargetTimespan/4;
    if (nActualTimespan > nTargetTimespan*4)
        nActualTimespan = nTargetTimespan*4;
    
    // Retarget
    CBigNum bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;
    
    if (bnNew > proofOfWorkLimit())
        bnNew = proofOfWorkLimit();
    
    /// debug print
    printf("GetNextWorkRequired RETARGET\n");
    printf("nTargetTimespan = %"PRI64d"    nActualTimespan = %"PRI64d"\n", nTargetTimespan, nActualTimespan);
    printf("Before: %08x  %s\n", pindexLast->nBits, CBigNum().SetCompact(pindexLast->nBits).getuint256().toString().c_str());
    printf("After:  %08x  %s\n", bnNew.GetCompact(), bnNew.getuint256().toString().c_str());
    
    return bnNew.GetCompact();
}

bool BitcoinChain::checkPoints(const unsigned int height, const uint256& hash) const {
    if ((height ==  11111 && hash != uint256("0x0000000069e244f73d78e8fd29ba2fd2ed618bd6fa2ee92559f542fdb26e7c1d")) ||
        (height ==  33333 && hash != uint256("0x000000002dd5588a74784eaa7ab0507a18ad16a236e7b1ce69f00d7ddfb5d0a6")) ||
        (height ==  68555 && hash != uint256("0x00000000001e1b4903550a0b96e9a9405c8a95f387162e4944e8d9fbe501cd6a")) ||
        (height ==  70567 && hash != uint256("0x00000000006a49b14bcf27462068f1264c961f11fa2e0eddd2be0791e1d4124a")) ||
        (height ==  74000 && hash != uint256("0x0000000000573993a3c9e41ce34471c079dcf5f52a0e824a81e7f953b8661a20")) ||
        (height == 105000 && hash != uint256("0x00000000000291ce28027faea320c8d2b054b2e0fe44a773f3eefb151d6bdc97")) ||
        (height == 118000 && hash != uint256("0x000000000000774a7f8a7a12dc906ddb9e17e75d684f15e00f8767f9e8f36553")) ||
        (height == 134444 && hash != uint256("0x00000000000005b12ffd4cd315cd34ffd4a594f430ac814c91184a0d42d2b0fe")) ||
        (height == 140700 && hash != uint256("0x000000000000033b512028abb90e1626d8b346fd0ed598ac0a3c371138dce2bd")))
        return false;
    else
        return true;
}

// global const definition of the bitcoin chain
const BitcoinChain bitcoin;


TestNetChain::TestNetChain() : _genesis("0x00000007199508e34a9ff81e6ec0c477a4cccff2a4767a8eee39c11db367b008") {
    _messageStart[0] = 0xfa; _messageStart[1] = 0xbf; _messageStart[2] = 0xb5; _messageStart[3] = 0xda;

    const char* pszTimestamp = "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";
    Transaction txNew;
    
    Script signature = Script() << 486604799 << CBigNum(4) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.addInput(Input(Coin(), signature)); 
    Script script = Script() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
    txNew.addOutput(Output(50 * COIN, script)); 

    _genesisBlock = Block(1, 0, 0, 1296688602, 0x1d07fff8, 384568319);
    _genesisBlock.addTransaction(txNew);
    _genesisBlock.updateMerkleTree(); // genesisBlock
    assert(_genesisBlock.getHash() == _genesis);
    
}

const Block& TestNetChain::genesisBlock() const {
    return _genesisBlock;
}

const int64 TestNetChain::subsidy(unsigned int height) const {
    int64 s = 50 * COIN;
    
    // Subsidy is cut in half every 4 years
    s >>= (height / 210000);
    
    return s;
}

bool TestNetChain::isStandard(const Transaction& tx) const {
    // on the test net everything is allowed
    return true;
}

unsigned int TestNetChain::nextWorkRequired(const CBlockIndex* pindexLast) const {
    const int64 nTargetTimespan = 14 * 24 * 60 * 60; // two weeks
    const int64 nTargetSpacing = 10 * 60;
    const int64 nInterval = nTargetTimespan / nTargetSpacing;
    
    // Genesis block
    if (pindexLast == NULL)
        return proofOfWorkLimit().GetCompact();
    
    // Only change once per interval
    if ((pindexLast->nHeight+1) % nInterval != 0)
        return pindexLast->nBits;
    
    // Go back by what we want to be 14 days worth of blocks
    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < nInterval-1; i++)
        pindexFirst = pindexFirst->pprev;
    assert(pindexFirst);
    
    // Limit adjustment step
    int64 nActualTimespan = pindexLast->GetBlockTime() - pindexFirst->GetBlockTime();
    printf("  nActualTimespan = %"PRI64d"  before bounds\n", nActualTimespan);
    if (nActualTimespan < nTargetTimespan/4)
        nActualTimespan = nTargetTimespan/4;
    if (nActualTimespan > nTargetTimespan*4)
        nActualTimespan = nTargetTimespan*4;
    
    // Retarget
    CBigNum bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;
    
    if (bnNew > proofOfWorkLimit())
        bnNew = proofOfWorkLimit();
    
    /// debug print
    printf("GetNextWorkRequired RETARGET\n");
    printf("nTargetTimespan = %"PRI64d"    nActualTimespan = %"PRI64d"\n", nTargetTimespan, nActualTimespan);
    printf("Before: %08x  %s\n", pindexLast->nBits, CBigNum().SetCompact(pindexLast->nBits).getuint256().toString().c_str());
    printf("After:  %08x  %s\n", bnNew.GetCompact(), bnNew.getuint256().toString().c_str());
    
    return bnNew.GetCompact();
}

// global const definition of the testnet chain
const TestNetChain testnet;
