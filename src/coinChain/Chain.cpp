/* -*-c++-*- libcoin - Copyright (C) 2012 Michael Gronager
 *
 * libcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * libcoin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libcoin.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <coinChain/Chain.h>

#include <coin/Block.h>
#include <coinChain/BlockChain.h>
#include <coin/Transaction.h>
#include <coin/KeyStore.h>
#include <coin/Logger.h>

#include <boost/assign/list_of.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>

using namespace std;
using namespace boost;

void Chain::check(const Transaction& txn) const {
    try {
        // Basic checks that don't depend on any context
        if (txn.getInputs().empty())
            throw runtime_error("vin empty");
        if (txn.getOutputs().empty())
            throw runtime_error("vout empty");
        // Size limits
//        if (::GetSerializeSize(txn, SER_NETWORK) > max_block_size())
        if (serialize_size(txn) > max_block_size())
            throw runtime_error("size limits failed");
        
        // Check for negative or overflow output values
        int64_t nValueOut = 0;
        BOOST_FOREACH(const Output& output, txn.getOutputs())
        {
            if (output.value() < 0)
                throw runtime_error("txout.nValue negative");
            if (output.value() > max_money())
                throw runtime_error("txout.nValue too high");
            nValueOut += output.value();
            if (!range(nValueOut))
                throw runtime_error("txout total out of range");
        }
        
        // Check for duplicate inputs
        set<Coin> vInOutPoints;
        BOOST_FOREACH(const Input& input, txn.getInputs())
        {
            if (vInOutPoints.count(input.prevout()))
                throw runtime_error("Transaction has duplicate inputs!");
            vInOutPoints.insert(input.prevout());
        }
        
        if (txn.isCoinBase()) {
            if (txn.getInput(0).signature().size() < 2 || txn.getInput(0).signature().size() > 100)
                throw runtime_error("coinbase script size");
        }
        else {
            BOOST_FOREACH(const Input& input, txn.getInputs())
            if (input.prevout().isNull())
                throw runtime_error("prevout is null");
        }
    }
    catch(std::exception& e) {
        throw runtime_error(string("check(Transaction): ") + e.what());
    }
}

void Chain::check(const Block& block) const {
    try {
        // These are checks that are independent of context
        // that can be verified before saving an orphan block.
        
        // Size limits
//        if (block.getTransactions().empty() || block.getTransactions().size() > max_block_size() || ::GetSerializeSize(block, SER_NETWORK) > max_block_size())
        if (block.getTransactions().empty() || block.getTransactions().size() > max_block_size() || serialize_size(block) > max_block_size())
            throw runtime_error("CheckBlock() : size limits failed");
        
        // Check timestamp
        if (block.getBlockTime() > GetAdjustedTime() + 2 * 60 * 60)
            throw runtime_error("CheckBlock() : block timestamp too far in the future");
        
        // First transaction must be coinbase, the rest must not be
        if (block.getTransactions().empty() || !block.getTransaction(0).isCoinBase())
            throw runtime_error("CheckBlock() : first tx is not coinbase");
        for (unsigned int i = 1; i < block.getTransactions().size(); i++)
            if (block.getTransaction(i).isCoinBase())
                throw runtime_error("CheckBlock() : more than one coinbase");
        
        // Check transactions
        BOOST_FOREACH(const Transaction& txn, block.getTransactions())
            check(txn);
        
        // Check that it's not full of nonstandard transactions
        if (block.GetSigOpCount() > MAX_BLOCK_SIGOPS)
            throw runtime_error("CheckBlock() : too many nonstandard transactions");
        
        // Check merkleroot
        if (block.getMerkleRoot() != block.buildMerkleTree())
            throw runtime_error("CheckBlock() : hashMerkleRoot mismatch");
    }
    catch(std::exception& e) {
        throw runtime_error(string("check(Block): ") + e.what());
    }
}

BitcoinChain::BitcoinChain() : Chain("bitcoin", "BTC", 8), _genesis("0x000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f") {
    _alert_key = ParseHex("04fc9702847840aaf195de8442ebecedf5b095cdbb9bc716bda9110971b28a49e0ead8564ff0db22209e0374782c093bb899692d524e9d6a6956e7c5ecbcd68284");
    _messageStart[0] = 0xf9; _messageStart[1] = 0xbe; _messageStart[2] = 0xb4; _messageStart[3] = 0xd9;
    const char* pszTimestamp = "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";
    Transaction txNew;
    Script signature = Script() << 0x1d00ffff << CBigNum(4) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.addInput(Input(Coin(), signature)); 
    Script script = Script() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
    txNew.addOutput(Output(50 * COIN, script)); 
    _genesisBlock = Block(1, uint256(0), uint256(0), 1231006505, 0x1d00ffff, 2083236893);
    _genesisBlock.addTransaction(txNew);
    _genesisBlock.updateMerkleTree(); // genesisBlock
    assert(_genesisBlock.getHash() == _genesis);
    
    _checkpoints = boost::assign::map_list_of
    ( 11111, uint256("0x0000000069e244f73d78e8fd29ba2fd2ed618bd6fa2ee92559f542fdb26e7c1d"))
    ( 33333, uint256("0x000000002dd5588a74784eaa7ab0507a18ad16a236e7b1ce69f00d7ddfb5d0a6"))
    ( 68555, uint256("0x00000000001e1b4903550a0b96e9a9405c8a95f387162e4944e8d9fbe501cd6a"))
    ( 70567, uint256("0x00000000006a49b14bcf27462068f1264c961f11fa2e0eddd2be0791e1d4124a"))
    ( 74000, uint256("0x0000000000573993a3c9e41ce34471c079dcf5f52a0e824a81e7f953b8661a20"))
    (105000, uint256("0x00000000000291ce28027faea320c8d2b054b2e0fe44a773f3eefb151d6bdc97"))
    (118000, uint256("0x000000000000774a7f8a7a12dc906ddb9e17e75d684f15e00f8767f9e8f36553"))
    (134444, uint256("0x00000000000005b12ffd4cd315cd34ffd4a594f430ac814c91184a0d42d2b0fe"))
    (140700, uint256("0x000000000000033b512028abb90e1626d8b346fd0ed598ac0a3c371138dce2bd"))
    (168000, uint256("0x000000000000099e61ea72015e79632f216fe6cb33d7899acb35b75c8303b763"))
    (193000, uint256("0x000000000000059f452a5f7340de6682a977387c17010ff6e6c3bd83ca8b1317"))
    (210000, uint256("0x000000000000048b95347e83192f69cf0366076336c639f9b7228e9ba171342e"))
    (216116, uint256("0x00000000000001b4f4b433e81ee46494af945cf96014816a4e2370f11b23df4e"))
    (225430, uint256("0x00000000000001c108384350f74090433e7fcf79a606b8e797f065b130575932"))
    (250000, uint256("0x000000000000003887df1f29024b06fc2200b55f8af8f35453d7be294df2d214"))
    (279000, uint256("0x0000000000000001ae8c72a0b0c301f67e3afca10e819efa9041e458e9bd7e40"))
    ;

    _seeds = boost::assign::list_of
    ("seed.bitcoin.sipa.be")
    ("dnsseed.bluematt.me")
    ("dnsseed.bitcoin.dashjr.org")
    ("bitseed.xf2.org");
}

unsigned int BitcoinChain::totalBlocksEstimate() const {
    return _checkpoints.rbegin()->first;
}


const Block& BitcoinChain::genesisBlock() const {
    return _genesisBlock;
}

const int64_t BitcoinChain::subsidy(unsigned int height, uint256 prev) const {
    int64_t s = 50 * COIN;
    
    // Subsidy is cut in half every 4 years
    s >>= (height / 210000);
    
    return s;
}

bool BitcoinChain::isStandard(const Transaction& tx) const {

    // Extremely large transactions with lots of inputs can cost the network
    // almost as much to process as they cost the sender in fees, because
    // computing signature hashes is O(ninputs*txsize). Limiting transactions
    // to MAX_STANDARD_TX_SIZE mitigates CPU exhaustion attacks.
    unsigned int sz = serialize_size(tx);//.GetSerializeSize(SER_NETWORK);
    if (sz >= MAX_STANDARD_TX_SIZE)
        return false;

    
    BOOST_FOREACH(const Input& txin, tx.getInputs()) {
        // Biggest 'standard' txin is a 3-signature 3-of-3 CHECKMULTISIG
        // pay-to-script-hash, which is 3 ~80-byte signatures, 3
        // ~65-byte public keys, plus a few script ops.
        if (txin.signature().size() > 500) {
            log_debug("nonstandard txin, size too big: %s", txin.signature().toString());
            return false;
        }
        if (!txin.signature().isPushOnly()) {
            log_debug("nonstandard txin, signature is not push only: %s", txin.signature().toString());
            return false;
        }
    }
    BOOST_FOREACH(const Output& txout, tx.getOutputs()) {
        vector<pair<opcodetype, valtype> > solution;
        if (!Solver(txout.script(), solution)) {
            log_debug("nonstandard txout - solver returned false: %s", txout.script().toString());
            return false;
        }
        if (txout.isDust(MIN_RELAY_TX_FEE)) {
            log_debug("nonstandard txout - is dust, value = %d", txout.value());
            return false;
        }
    }
    
    return true;
}

/// This function has changed as it served two purposes: sanity check for headers and real proof of work check. We only need the proofOfWorkLimit for the latter
const bool BitcoinChain::checkProofOfWork(const Block& block) const {
    uint256 hash = block.getHash();
    unsigned int nBits = block.getBits();
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);
    
    // Check range
    if (proofOfWorkLimit() != 0 && (bnTarget <= 0 || bnTarget > proofOfWorkLimit()))
        return error("CheckProofOfWork() : nBits below minimum work");
    
    // Check proof of work matches claimed amount
    if (hash > bnTarget.getuint256())
        return error("CheckProofOfWork() : hash doesn't match nBits");
    
    return true;
}

int BitcoinChain::nextWorkRequired(BlockIterator blk) const {
    const int64_t nTargetTimespan = 14 * 24 * 60 * 60; // two weeks
    const int64_t nTargetSpacing = 10 * 60;
    const int64_t nInterval = nTargetTimespan / nTargetSpacing;
    
    // Genesis block
    int h = blk.height();
    if (h == 0) // trick to test that it is asking for the genesis block
        return proofOfWorkLimit().GetCompact();
    
    // Only change once per interval
    if ((h + 1) % nInterval != 0)
        return blk->bits;
    
    // Go back by what we want to be 14 days worth of blocks
    BlockIterator former = blk - (nInterval-1);
    
    // Limit adjustment step
    int nActualTimespan = blk->time - former->time;
    log_debug("  nActualTimespan = %"PRI64d"  before bounds", nActualTimespan);
    if (nActualTimespan < nTargetTimespan/4)
        nActualTimespan = nTargetTimespan/4;
    if (nActualTimespan > nTargetTimespan*4)
        nActualTimespan = nTargetTimespan*4;
    
    // Retarget
    CBigNum bnNew;
    bnNew.SetCompact(blk->bits);
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;
    
    if (bnNew > proofOfWorkLimit())
        bnNew = proofOfWorkLimit();
    
    /// debug print
    log_info("GetNextWorkRequired RETARGET");
    log_info("\tnTargetTimespan = %"PRI64d"    nActualTimespan = %"PRI64d"", nTargetTimespan, nActualTimespan);
    log_info("\tBefore: %08x  %s", blk->bits, CBigNum().SetCompact(blk->bits).getuint256().toString().c_str());
    log_info("\tAfter:  %08x  %s", bnNew.GetCompact(), bnNew.getuint256().toString().c_str());
    
    return bnNew.GetCompact();
}

bool BitcoinChain::checkPoints(const unsigned int height, const uint256& hash) const {
    Checkpoints::const_iterator i = _checkpoints.find(height);
    if (i == _checkpoints.end())
        return true;
    
    return hash == i->second;
}

// global const definition of the bitcoin chain
const BitcoinChain bitcoin;


TestNet3Chain::TestNet3Chain() : Chain("testnet3", "TST", 8), _genesis("0x000000000933ea01ad0ee984209779baaec3ced90fa3f408719526f8d77f4943") {
    _alert_key = ParseHex("04302390343f91cc401d56d68b123028bf52e5fca1939df127f63c6467cdf9c8e2c14b61104cf817d0b780da337893ecc4aaff1309e536162dabbdb45200ca2b0a");
    _messageStart[0] = 0x0b; _messageStart[1] = 0x11; _messageStart[2] = 0x09; _messageStart[3] = 0x07;

    const char* pszTimestamp = "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";
    Transaction txNew;
    
    Script signature = Script() << 0x1d00ffff << CBigNum(4) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.addInput(Input(Coin(), signature)); 
    Script script = Script() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
    txNew.addOutput(Output(50 * COIN, script)); 

    _genesisBlock = Block(1, uint256(0), uint256(0), 1296688602, 0x1d00ffff, 414098458);
    _genesisBlock.addTransaction(txNew);
    _genesisBlock.updateMerkleTree(); // genesisBlock
    assert(_genesisBlock.getHash() == _genesis);
    
    _seeds = boost::assign::list_of
    ("testnet-seed.bitcoin.petertodd.org")
    ("testnet-seed.bluematt.me");
}

const Block& TestNet3Chain::genesisBlock() const {
    return _genesisBlock;
}

const int64_t TestNet3Chain::subsidy(unsigned int height, uint256 prev) const {
    int64_t s = 50 * COIN;
    
    // Subsidy is cut in half every 4 years
    s >>= (height / 210000);
    
    return s;
}

bool TestNet3Chain::isStandard(const Transaction& tx) const {
    // on the test net everything is allowed
    return true;
}

const bool TestNet3Chain::checkProofOfWork(const Block& block) const {
    uint256 hash = block.getHash();
    unsigned int nBits = block.getBits();
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);
    
    // Check range
    if (proofOfWorkLimit() != 0 && (bnTarget <= 0 || bnTarget > proofOfWorkLimit()))
        return error("CheckProofOfWork() : nBits below minimum work");
    
    // Check proof of work matches claimed amount
    if (hash > bnTarget.getuint256())
        return error("CheckProofOfWork() : hash doesn't match nBits");
    
    return true;
}


int TestNet3Chain::nextWorkRequired(BlockIterator blk) const {
    const int64_t nTargetTimespan = 14 * 24 * 60 * 60; // two weeks
    const int64_t nTargetSpacing = 10 * 60;
    const int64_t nInterval = nTargetTimespan / nTargetSpacing;
    
    // Genesis block
    int h = blk.height();
    if (h == 0) // trick to test that it is asking for the genesis block
        return proofOfWorkLimit().GetCompact();
    
    // Only change once per interval
    if ((h + 1) % nInterval != 0) {
        // Return the last non-special-min-difficulty-rules-block
        while (blk.height() % nInterval != 0 && blk->bits == proofOfWorkLimit().GetCompact())
            blk--;
        return blk->bits;
    }
    
    // Go back by what we want to be 14 days worth of blocks
    BlockIterator former = blk - (nInterval-1);
    
    // Limit adjustment step
    int nActualTimespan = blk->time - former->time;
    log_debug("  nActualTimespan = %"PRI64d"  before bounds", nActualTimespan);
    if (nActualTimespan < nTargetTimespan/4)
        nActualTimespan = nTargetTimespan/4;
    if (nActualTimespan > nTargetTimespan*4)
        nActualTimespan = nTargetTimespan*4;
    
    // Retarget
    CBigNum bnNew;
    bnNew.SetCompact(blk->bits);
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;
    
    if (bnNew > proofOfWorkLimit())
        bnNew = proofOfWorkLimit();
    
    /// debug print
    log_info("GetNextWorkRequired RETARGET");
    log_info("\tnTargetTimespan = %"PRI64d"    nActualTimespan = %"PRI64d"", nTargetTimespan, nActualTimespan);
    log_info("\tBefore: %08x  %s", blk->bits, CBigNum().SetCompact(blk->bits).getuint256().toString().c_str());
    log_info("\tAfter:  %08x  %s", bnNew.GetCompact(), bnNew.getuint256().toString().c_str());
    
    return bnNew.GetCompact();
}

// global const definition of the testnet chain
const TestNet3Chain testnet3;


NamecoinChain::NamecoinChain() : Chain("namecoin", "NMC", 8), _genesis("000000000062b72c5e2ceb45fbc8587e807c155b0da735e6483dfba2f0a9c770") {
    _alert_key = ParseHex("04fc9702847840aaf195de8442ebecedf5b095cdbb9bc716bda9110971b28a49e0ead8564ff0db22209e0374782c093bb899692d524e9d6a6956e7c5ecbcd68284");
    _messageStart[0] = 0xf9; _messageStart[1] = 0xbe; _messageStart[2] = 0xb4; _messageStart[3] = 0xfe;
    const char* pszTimestamp = "... choose what comes next.  Lives of your own, or a return to chains. -- V";
    Transaction txNew;
    Script signature = Script() << 0x1c007fff << CBigNum(522) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.addInput(Input(Coin(), signature));
    Script script = Script() << ParseHex("04b620369050cd899ffbbc4e8ee51e8c4534a855bb463439d63d235d4779685d8b6f4870a238cf365ac94fa13ef9a2a22cd99d0d5ee86dcabcafce36c7acf43ce5") << OP_CHECKSIG;
    txNew.addOutput(Output(50 * COIN, script));
    _genesisBlock = Block(1, uint256(0), uint256(0), 1303000001, 0x1c007fff, 0xa21ea192U);
    _genesisBlock.addTransaction(txNew);
    _genesisBlock.updateMerkleTree(); // genesisBlock
    assert(_genesisBlock.getHash() == _genesis);
    
    _checkpoints = boost::assign::map_list_of
    (  2016, uint256("0x0000000000660bad0d9fbde55ba7ee14ddf766ed5f527e3fbca523ac11460b92"))
    (  4032, uint256("0x0000000000493b5696ad482deb79da835fe2385304b841beef1938655ddbc411"))
    (  6048, uint256("0x000000000027939a2e1d8bb63f36c47da858e56d570f143e67e85068943470c9"))
    (  8064, uint256("0x000000000003a01f708da7396e54d081701ea406ed163e519589717d8b7c95a5"))
    ( 10080, uint256("0x00000000000fed3899f818b2228b4f01b9a0a7eeee907abd172852df71c64b06"))
    ( 12096, uint256("0x0000000000006c06988ff361f124314f9f4bb45b6997d90a7ee4cedf434c670f"))
    ( 14112, uint256("0x00000000000045d95e0588c47c17d593c7b5cb4fb1e56213d1b3843c1773df2b"))
    ( 16128, uint256("0x000000000001d9964f9483f9096cf9d6c6c2886ed1e5dec95ad2aeec3ce72fa9"))
    ( 18940, uint256("0x00000000000087f7fc0c8085217503ba86f796fa4984f7e5a08b6c4c12906c05"))
    ( 30240, uint256("0xe1c8c862ff342358384d4c22fa6ea5f669f3e1cdcf34111f8017371c3c0be1da"))
    ( 57000, uint256("0xaa3ec60168a0200799e362e2b572ee01f3c3852030d07d036e0aa884ec61f203"))
    (112896, uint256("0x73f880e78a04dd6a31efc8abf7ca5db4e262c4ae130d559730d6ccb8808095bf"))
    ;
    
}

unsigned int NamecoinChain::totalBlocksEstimate() const {
    return _checkpoints.rbegin()->first;
}


const Block& NamecoinChain::genesisBlock() const {
    return _genesisBlock;
}

const int64_t NamecoinChain::subsidy(unsigned int height, uint256 prev) const {
    int64_t s = 50 * COIN;
    
    // Subsidy is cut in half every 4 years
    s >>= (height / 210000);
    
    return s;
}

bool NamecoinChain::isStandard(const Transaction& tx) const {
    
    // Extremely large transactions with lots of inputs can cost the network
    // almost as much to process as they cost the sender in fees, because
    // computing signature hashes is O(ninputs*txsize). Limiting transactions
    // to MAX_STANDARD_TX_SIZE mitigates CPU exhaustion attacks.
    unsigned int sz = serialize_size(tx);//tx.GetSerializeSize(SER_NETWORK);
    if (sz >= MAX_STANDARD_TX_SIZE)
        return false;
    
    BOOST_FOREACH(const Input& txin, tx.getInputs()) {
        // Biggest 'standard' txin is a 3-signature 3-of-3 CHECKMULTISIG
        // pay-to-script-hash, which is 3 ~80-byte signatures, 3
        // ~65-byte public keys, plus a few script ops.
        if (txin.signature().size() > 500) {
            log_debug("nonstandard txin, size too big: %s", txin.signature().toString());
            return false;
        }
        if (!txin.signature().isPushOnly()) {
            log_debug("nonstandard txin, signature is not push only: %s", txin.signature().toString());
            return false;
        }
    }
    BOOST_FOREACH(const Output& txout, tx.getOutputs()) {
        vector<pair<opcodetype, valtype> > solution;
        if (!Solver(txout.script(), solution)) {
            log_debug("nonstandard txout - solver returned false: %s", txout.script().toString());
            return false;
        }
        if (txout.isDust(MIN_RELAY_TX_FEE)) {
            log_debug("nonstandard txout - is dust, value = %d", txout.value());
            return false;
        }
    }
    
    return true;
}

/// This function has changed as it served two purposes: sanity check for headers and real proof of work check. We only need the proofOfWorkLimit for the latter
/// For namecoin we allow merged mining for the PoW!
const bool NamecoinChain::checkProofOfWork(const Block& block) const {
    log_trace("Enter %s (block.version = %d)", __FUNCTION__, block.getVersion());
    // we accept aux pow all the time - the lockins will ensure we get the right chain
    // Prevent same work from being submitted twice:
    // - this block must have our chain ID
    // - parent block must not have the same chain ID (see CAuxPow::Check)
    // - index of this chain in chain merkle tree must be pre-determined (see CAuxPow::Check)
    //    if (nHeight != INT_MAX && GetChainID() != GetOurChainID())
    //    return error("CheckProofOfWork() : block does not have our chain ID");

    CBigNum target;
    target.SetCompact(block.getBits());
    if (proofOfWorkLimit() != 0 && (target <= 0 || target > proofOfWorkLimit())) {
        cout << target.GetHex() << endl;
        cout << proofOfWorkLimit().GetHex() << endl;
        return error("CheckProofOfWork() : nBits below minimum work");
    }
    
    if (block.getVersion()&BLOCK_VERSION_AUXPOW) {
        if (!block.getAuxPoW().Check(block.getHash(), block.getVersion()/BLOCK_VERSION_CHAIN_START))
            return error("CheckProofOfWork() : AUX POW is not valid");
        // Check proof of work matches claimed amount
        if (block.getAuxPoW().GetParentBlockHash() > target.getuint256())
            return error("CheckProofOfWork() : AUX proof of work failed");
    }
    else {
        // Check proof of work matches claimed amount
        if (block.getHash() > target.getuint256())
            return error("CheckProofOfWork() : proof of work failed");
    }
    log_trace("Return(true): %s", __FUNCTION__);
    return true;
}

int NamecoinChain::nextWorkRequired(BlockIterator blk) const {
    const int64_t nTargetTimespan = 14 * 24 * 60 * 60; // two weeks
    const int64_t nTargetSpacing = 10 * 60;
    const int64_t nInterval = nTargetTimespan / nTargetSpacing;
    
    // Genesis block
    int h = blk.height();
    if (h == 0) // trick to test that it is asking for the genesis block
        return 0x1c007fff;
    
    // Only change once per interval
    if ((h + 1) % nInterval != 0)
        return blk->bits;
    
    // Go back by what we want to be 14 days worth of blocks
    BlockIterator former = blk - (nInterval-1);
    
    if (h >= 19200 && (h+1 > nInterval))
        former = blk - nInterval;
    
    // Limit adjustment step
    int nActualTimespan = blk->time - former->time;
    log_debug("  nActualTimespan = %"PRI64d"  before bounds", nActualTimespan);
    if (nActualTimespan < nTargetTimespan/4)
        nActualTimespan = nTargetTimespan/4;
    if (nActualTimespan > nTargetTimespan*4)
        nActualTimespan = nTargetTimespan*4;
    
    // Retarget
    CBigNum bnNew;
    bnNew.SetCompact(blk->bits);
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;
    
    if (bnNew > proofOfWorkLimit())
        bnNew = proofOfWorkLimit();
    
    /// debug print
    log_info("GetNextWorkRequired RETARGET");
    log_info("\tnTargetTimespan = %"PRI64d"    nActualTimespan = %"PRI64d"", nTargetTimespan, nActualTimespan);
    log_info("\tBefore: %08x  %s", blk->bits, CBigNum().SetCompact(blk->bits).getuint256().toString().c_str());
    log_info("\tAfter:  %08x  %s", bnNew.GetCompact(), bnNew.getuint256().toString().c_str());
    
    return bnNew.GetCompact();
}

bool NamecoinChain::checkPoints(const unsigned int height, const uint256& hash) const {
    Checkpoints::const_iterator i = _checkpoints.find(height);
    if (i == _checkpoints.end())
        return true;
    
    return hash == i->second;
}

// global const definition of the bitcoin chain
const NamecoinChain namecoin;


LitecoinChain::LitecoinChain() : Chain("litecoin", "LTC", 8), _genesis("0x12a765e31ffd4059bada1e25190f6e98c99d9714d334efa41a195a7e7e04bfe2") {
    _alert_key = ParseHex("040184710fa689ad5023690c80f3a49c8f13f8d45b8c857fbcbc8bc4a8e4d3eb4b10f4d4604fa08dce601aaf0f470216fe1b51850b4acf21b179c45070ac7b03a9");    _messageStart[0] = 0xfb; _messageStart[1] = 0xc0; _messageStart[2] = 0xb6; _messageStart[3] = 0xdb; // Litecoin: increase each by adding 2 to bitcoin's value.

    const char* pszTimestamp = "NY Times 05/Oct/2011 Steve Jobs, Apple’s Visionary, Dies at 56";
    Transaction txNew;
    
    Script signature = Script() << 486604799 << CBigNum(4) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.addInput(Input(Coin(), signature));
    Script script = Script() << ParseHex("040184710fa689ad5023690c80f3a49c8f13f8d45b8c857fbcbc8bc4a8e4d3eb4b10f4d4604fa08dce601aaf0f470216fe1b51850b4acf21b179c45070ac7b03a9") << OP_CHECKSIG;
    txNew.addOutput(Output(50 * COIN, script));
    
    _genesisBlock = Block(1, uint256(0), uint256(0), 1317972665, 0x1e0ffff0, 2084524493);
    _genesisBlock.addTransaction(txNew);
    _genesisBlock.updateMerkleTree(); // genesisBlock
    assert(_genesisBlock.getHash() == _genesis);

    _checkpoints = boost::assign::map_list_of
    (     1, uint256("0x80ca095ed10b02e53d769eb6eaf92cd04e9e0759e5be4a8477b42911ba49c78f"))
    (     2, uint256("0x13957807cdd1d02f993909fa59510e318763f99a506c4c426e3b254af09f40d7"))
    (  1500, uint256("0x841a2965955dd288cfa707a755d05a54e45f8bd476835ec9af4402a2b59a2967"))
    (  4032, uint256("0x9ce90e427198fc0ef05e5905ce3503725b80e26afd35a987965fd7e3d9cf0846"))
    (  8064, uint256("0xeb984353fc5190f210651f150c40b8a4bab9eeeff0b729fcb3987da694430d70"))
    ( 16128, uint256("0x602edf1859b7f9a6af809f1d9b0e6cb66fdc1d4d9dcd7a4bec03e12a1ccd153d"))
    ( 23420, uint256("0xd80fdf9ca81afd0bd2b2a90ac3a9fe547da58f2530ec874e978fce0b5101b507"))
    ( 50000, uint256("0x69dc37eb029b68f075a5012dcc0419c127672adb4f3a32882b2b3e71d07a20a6"))
    ( 80000, uint256("0x4fcb7c02f676a300503f49c764a89955a8f920b46a8cbecb4867182ecdb2e90a"))
    (120000, uint256("0xbd9d26924f05f6daa7f0155f32828ec89e8e29cee9e7121b026a7a3552ac6131"))
    (161500, uint256("0xdbe89880474f4bb4f75c227c77ba1cdc024991123b28b8418dbbf7798471ff43"))
    (179620, uint256("0x2ad9c65c990ac00426d18e446e0fd7be2ffa69e9a7dcb28358a50b2b78b9f709"))
    ;
    
    _seeds = boost::assign::list_of
    ("dnsseed.litecointools.com")
    ("dnsseed.litecoinpool.org")
    ("dnsseed.ltc.xurious.com")
    ("dnsseed.koin-project.com")
    ("dnsseed.weminemnc.com");
}

unsigned int LitecoinChain::totalBlocksEstimate() const {
    return _checkpoints.rbegin()->first;
}

bool LitecoinChain::checkPoints(const unsigned int height, const uint256& hash) const {
    Checkpoints::const_iterator i = _checkpoints.find(height);
    if (i == _checkpoints.end())
        return true;
    
    return hash == i->second;
}

const Block& LitecoinChain::genesisBlock() const {
    return _genesisBlock;
}

const int64_t LitecoinChain::subsidy(unsigned int height, uint256 prev) const {
    int64_t s = 50 * COIN;
    
    // Subsidy is cut in half every 4 years
    s >>= (height / 840000);
    
    return s;
}

bool LitecoinChain::isStandard(const Transaction& tx) const {
    // on the test net everything is allowed
    return true;
}

const bool LitecoinChain::checkProofOfWork(const Block& block) const {
    uint256 hash = getPoWHash(block);
    
    unsigned int nBits = block.getBits();
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);
    
    // Check range
    if (proofOfWorkLimit() != 0 && (bnTarget <= 0 || bnTarget > proofOfWorkLimit()))
        return error("CheckProofOfWork() : nBits below minimum work");
    
    // Check proof of work matches claimed amount
    if (hash > bnTarget.getuint256())
        return error("CheckProofOfWork() : hash doesn't match nBits");
    
    return true;
}

int LitecoinChain::nextWorkRequired(BlockIterator blk) const {
    const int64_t nTargetTimespan = 3.5 * 24 * 60 * 60; // two weeks
    const int64_t nTargetSpacing = 2.5 * 60;
    const int64_t nInterval = nTargetTimespan / nTargetSpacing;
    
    // Genesis block
    int h = blk.height();
    if (h == 0) // trick to test that it is asking for the genesis block
        return _genesisBlock.getBits(); // proofOfWorkLimit().GetCompact(); Actually not for the genesisblock - here it is 0x1e0ffff0, not 0x1e0fffff
    
    // Only change once per interval
    if ((h + 1) % nInterval != 0)
        return blk->bits;
    
    // Litecoin: This fixes an issue where a 51% attack can change difficulty at will.
    // Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz
    int blockstogoback = nInterval-1;
    if ((h + 1) != nInterval)
        blockstogoback = nInterval;
    
    // Go back by what we want to be 3.5 days worth of blocks
    BlockIterator former = blk - blockstogoback;
    
    // Limit adjustment step
    int nActualTimespan = blk->time - former->time;
    log_debug("  nActualTimespan = %"PRI64d"  before bounds", nActualTimespan);
    if (nActualTimespan < nTargetTimespan/4)
        nActualTimespan = nTargetTimespan/4;
    if (nActualTimespan > nTargetTimespan*4)
        nActualTimespan = nTargetTimespan*4;
    
    // Retarget
    CBigNum bnNew;
    bnNew.SetCompact(blk->bits);
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;
    
    if (bnNew > proofOfWorkLimit())
        bnNew = proofOfWorkLimit();
    
    /// debug print
    log_info("GetNextWorkRequired RETARGET");
    log_info("\tnTargetTimespan = %"PRI64d"    nActualTimespan = %"PRI64d"", nTargetTimespan, nActualTimespan);
    log_info("\tBefore: %08x  %s", blk->bits, CBigNum().SetCompact(blk->bits).getuint256().toString().c_str());
    log_info("\tAfter:  %08x  %s", bnNew.GetCompact(), bnNew.getuint256().toString().c_str());
    
    return bnNew.GetCompact();
}


const uint256 LitecoinChain::getPoWHash(const Block& block) const {
    uint256 hash;
    scrypt_1024_1_1_256(BEGIN(block), BEGIN(hash));
    return hash;
}


const LitecoinChain litecoin;

TerracoinChain::TerracoinChain() : Chain("terracoin", "TRC", 8), _genesis("0x00000000804bbc6a621a9dbb564ce469f492e1ccf2d70f8a6b241e26a277afa2") {
    _alert_key = ParseHex("04302390343f91cc401d56d68b123028bf52e5fca1939df127f63c6467cdf9c8e2c14b61104cf817d0b780da337893ecc4aaff1309e536162dabbdb45200ca2b0a");
    _messageStart[0] = 0xfb; _messageStart[1] = 0xc0; _messageStart[2] = 0xb6; _messageStart[3] = 0xdb; // Terracoin: increase each by adding 2 to bitcoin's value.
    
    const char* pszTimestamp = "June 4th 1978 - March 6th 2009 ; Rest In Peace, Stephanie.";
    Transaction txNew;
    
    Script signature = Script() << 486604799 << CBigNum(4) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.addInput(Input(Coin(), signature));
    Script script = Script() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
    txNew.addOutput(Output(50 * COIN, script));
    
    _genesisBlock = Block(1, uint256(0), uint256(0), 1351242683, 0x1d00ffff, 2820375594);
    _genesisBlock.addTransaction(txNew);
    _genesisBlock.updateMerkleTree(); // genesisBlock
    assert(_genesisBlock.getHash() == _genesis);
    
    _checkpoints = boost::assign::map_list_of
    (    0, uint256("0x00000000804bbc6a621a9dbb564ce469f492e1ccf2d70f8a6b241e26a277afa2"))
    (    1, uint256("0x000000000fbc1c60a7610c894f98d102390e9e00cc18caced4eb4198ec0c3645"))
    (   31, uint256("0x00000000045341e3ebdfa180e4a0f1e4da23829609517a3673b4a796714a7593"))
    (  104, uint256("0x000000006afe30806352f2015829527dd91f19fbc2d28f799c3ad61c37746fdb"))
    (  355, uint256("0x0000000000c0e178ddd6a8f15724f37470428c233883c302267299b21fb5d237"))
    (  721, uint256("0x00000000027671a417f3d3eafac6d150f0b2ccba37f0b63f75bc657ec25950ed"))
    ( 2000, uint256("0x000000000283ecd683fe30d4542929a78873df7818029793f84e9c65dc337d94"))
    ( 3000, uint256("0x0000000000317b69ff2a56284442fede7ffa66f75d000f1cf34171ad671db4b6"))
    ( 4000, uint256("0x00000000001190cad5d66d028b6afcf22db58d2b5c17abf2bf2e1353be13097d"))
    ( 4255, uint256("0x000000000018b3ba5b241f3e88b4a88e580f9e7dd0fc7fc786ff22c586e53dd9"))
    ( 5631, uint256("0x00000000001243509866938e344c0010bd88b156da27ef9d707cb1f1698f2a32"))
    ( 6000, uint256("0x00000000000c1abfd7c29d07e23ef52631c6874e42e6a240a4d3678d9c716d81"))
    ( 7395, uint256("0x0000000000005d8e1281d6b28fe6b504ab81e7e3ec561e97b7a98973e449f7fb"))
    ( 8001, uint256("0x00000000001ca63707536b6dfc8ba4c5aa7fce19b6295ef73e195554cdb92d44"))
    ( 8157, uint256("0x00000000001303998da2714abf02159f1421103e77fee3b876d69c0fa7b108d9"))
    (12311, uint256("0x00000000002b5708fefdceb5db42cd2135eaf23f23a95c285e8f310262f8d639"))
    (13224, uint256("0x00000000000765c69f777ccbc44fab23edab9126f1b4ec5078450aebc3809c36"))
    (14401, uint256("0x0000000000388541b88c57883a480fd6cfa7b93f68e0c71538b08b2c4d875fa2"))
    (15238, uint256("0x000000000000ae09d46bc1a9c5ca4c7b5e51bf23d3108926daa140d64355d390"))
    (18426, uint256("0x00000000001fb1c075dbfa27f7ba83928a2d35152ccae59c742f698fb8cb8108"))
    (19114, uint256("0x000000000022224f57adecffdc8f5cffb3b932838310aefdd12aaabcc6122259"))
    (20124, uint256("0x00000000002374e0e1fcee28b520dff3f3e86cb7ff7afdea112aaf2002101900"))
    (21711, uint256("0x00000000002189b0ae139b8c449bbcb99d520b8a798b7e0f0ff8ab8539e9bcb4"))
    (22100, uint256("0x0000000000142ace7d8db003da69191896986c5564e604e3100d14c17daaf90f"))
    (22566, uint256("0x00000000000c28dc052d277d18e104e4e63c53e4018273b3af49f772af205d43"))
    (24076, uint256("0x00000000000522702c7f0ee6fa2ce3978cc0f3056ecc73d8cde1035891c5c4d5"))
    (25372, uint256("0x0000000000202428a01ad6b8a2e256162b5bba35efd1e7f45dc42dbf5861f784"))
    (25538, uint256("0x00000000002faa5586493e3821d90482348b0462d625d03d086a4a2a2301f6ef"))
    (26814, uint256("0x0000000000179b0ed2a7d4390ff2c6213f1c522790b4e084a4e2036b53e6765b"))
    (28326, uint256("0x0000000000141af96e4ab491d6534a6740491d62799b1669418d33bb007acfd7"))
    (28951, uint256("0x00000000002404c991c7f9e1d641e8938f1ab704a3f9e1d22589d817948a202f"))
    (30765, uint256("0x00000000004be710d96035855f406c1393c922d5948d82dce494368e3993c76a"))
    (32122, uint256("0x0000000000109367e18d9c9762cdb8bfb3c524628ad2ce9bcb02a6a9bfa13e39"))
    (33526, uint256("0x0000000000406137d7afb03df768f0c6d7ab1efdbc14325d018b62b7ef17fa1c"))
    (34310, uint256("0x0000000000325205f89b7685639fc997248cbaf8dbf2d53ad2e00f98948de58c"))
    (35277, uint256("0x00000000000214b28bc7b1ea230417442417d2381673bcec4ac3ef87c6d0b9f8"))
    (36341, uint256("0x0000000000143a6fd244037ebf301c6898f34c6db428916057eadddec4e35061"))
    (51013, uint256("0x00000000002afdd383affb15708fd329feaa68fdabe477bce4eceec7525dc7f2"))
    (63421, uint256("0x000000000020867e3050e7f4b4402c578215a2174723f614d31d5f21eb61a173"))
    (67755, uint256("0x00000000001151bbacf6f169312aaa0c71e0f71765ceb4e8ebffabc219993ba7"))
    (69198, uint256("0x000000000041498e3911ccbd9aee327bb9bc58dcdf4cd51956909ce5575fccf5"))
    (71945, uint256("0x00000000006d1cb2ad6700614c54a962bbe7d6baeb02c227b9dda2e0a59077da"))
    (74654, uint256("0x00000000001b274b44531f13d5a023ae0450e765e403893c64aea7c6c21eec8e"))
    (77505, uint256("0x00000000003be70f239212ba753a1fdd985fee027e276556619eeb40a771c151"))
    (95971, uint256("0x000000000009d877e435995db1565558e30a7f8ee220cad5d0a4a055d0ebb8bf"))
    (106681, uint256("0x000000000000d081d43eb74429e7344f18c4300796faaa54f5432c4976a9fc2b"))
    (106685, uint256("0x00000000000017e88ad9cf01e74941a134434bf0c39ef254498e4cbb754b604f"))
    (106693, uint256("0x00000000000271eb870d1573f3c1e1ba4c33a56f80333b89219d8affb2a8dadc"))
    (106715, uint256("0x0000000000012d109bfe2a5b464defb0214b5b25090acb9cc0fd9ca054c53d6a"))
    (106725, uint256("0x000000000007c734700d0be1f0c2358f57a009752257da8dcc2206185e9a580a"))
    (106748, uint256("0x000000000000148fc833528722f41af106c08ed2da67e777f5a6b8ec2d60d695"))
    (106753, uint256("0x0000000000003cb9976d3785c2960728bdd67d4ddfce640682b9c9c4df8582d9"))
    (106759, uint256("0x000000000002ed231df0f8c4f21f2a73eeacbbc88f411b438ad4fd4b60418c42"))
    (107368, uint256("0x000000000009062fe1b4c0654b3d152282545aa8beba3c0e4981d9dfa71b1eaa"))
    (108106, uint256("0x00000000000282bf4e2bd0571c42165a67ffede3b81f2387e301369162107020"))
    (110197, uint256("0x0000000000002d863064910c8964f5d8e2883aca9760c19368fe043263e2bfdd"))
    ;
}

unsigned int TerracoinChain::totalBlocksEstimate() const {
    return _checkpoints.rbegin()->first;
}


const Block& TerracoinChain::genesisBlock() const {
    return _genesisBlock;
}

const int64_t TerracoinChain::subsidy(unsigned int height, uint256 prev) const {
    int64_t s = 50 * COIN;
    
    // Subsidy is cut in half every 4 years
    s >>= (height / 1050000);
    
    return s;
}

bool TerracoinChain::isStandard(const Transaction& tx) const {
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

/// This function has changed as it served two purposes: sanity check for headers and real proof of work check. We only need the proofOfWorkLimit for the latter
const bool TerracoinChain::checkProofOfWork(const Block& block) const {
    uint256 hash = block.getHash();
    unsigned int nBits = block.getBits();
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);
    
    // Check range
    if (proofOfWorkLimit() != 0 && (bnTarget <= 0 || bnTarget > proofOfWorkLimit()))
        return error("CheckProofOfWork() : nBits below minimum work");
    
    // Check proof of work matches claimed amount
    if (hash > bnTarget.getuint256())
        return error("CheckProofOfWork() : hash doesn't match nBits");
    
    return true;
}

int TerracoinChain::nextWorkRequired(BlockIterator blk) const {
    const int64_t nTargetTimespan = 60 * 60; // one hour weeks
    const int64_t nTargetSpacing = 2 * 60; // new block every two minutes
    const int64_t nInterval = nTargetTimespan / nTargetSpacing;
    
    // Genesis block
    int h = blk.height();
    if (h == 0) // trick to test that it is asking for the genesis block
        return proofOfWorkLimit().GetCompact();
    
    // Only change once per interval
    if ((h + 1) % nInterval != 0)
        return blk->bits;
    
    // Go back by what we want to be 14 days worth of blocks
    BlockIterator former = blk - (nInterval-1);
    
    // Limit adjustment step
    int nActualTimespan = blk->time - former->time;
    log_debug("  nActualTimespan = %"PRI64d"  before bounds", nActualTimespan);
    if (nActualTimespan < nTargetTimespan/4)
        nActualTimespan = nTargetTimespan/4;
    if (nActualTimespan > nTargetTimespan*4)
        nActualTimespan = nTargetTimespan*4;
    
    // Retarget
    CBigNum bnNew;
    bnNew.SetCompact(blk->bits);
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;
    
    if (bnNew > proofOfWorkLimit())
        bnNew = proofOfWorkLimit();
    
    /// debug print
    log_info("GetNextWorkRequired RETARGET");
    log_info("nTargetTimespan = %"PRI64d"    nActualTimespan = %"PRI64d"", nTargetTimespan, nActualTimespan);
    log_info("Before: %08x  %s", blk->bits, CBigNum().SetCompact(blk->bits).getuint256().toString().c_str());
    log_info("After:  %08x  %s", bnNew.GetCompact(), bnNew.getuint256().toString().c_str());
    
    return bnNew.GetCompact();
}

bool TerracoinChain::checkPoints(const unsigned int height, const uint256& hash) const {
    Checkpoints::const_iterator i = _checkpoints.find(height);
    if (i == _checkpoints.end())
        return true;
    
    return hash == i->second;
}

// global const definition of the bitcoin chain
const TerracoinChain terracoin;

DogecoinChain::DogecoinChain() : Chain("dogecoin", "DGE", 8), _genesis("0x1a91e3dace36e2be3bf030a65679fe821aa1d6ef92e7c9902eb318182c355691") {
    _alert_key = ParseHex("04d4da7a5dae4db797d9b0644d57a5cd50e05a70f36091cd62e2fc41c98ded06340be5a43a35e185690cd9cde5d72da8f6d065b499b06f51dcfba14aad859f443a");    _messageStart[0] = 0xc0; _messageStart[1] = 0xc0; _messageStart[2] = 0xc0; _messageStart[3] = 0xc0; // Litecoin: increase each by adding 2 to bitcoin's value.
    
    const char* pszTimestamp = "Nintondo";
    Transaction txNew;
    
    Script signature = Script() << 486604799 << CBigNum(4) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.addInput(Input(Coin(), signature));
    Script script = Script() << ParseHex("040184710fa689ad5023690c80f3a49c8f13f8d45b8c857fbcbc8bc4a8e4d3eb4b10f4d4604fa08dce601aaf0f470216fe1b51850b4acf21b179c45070ac7b03a9") << OP_CHECKSIG;
    txNew.addOutput(Output(88 * COIN, script));
    
    _genesisBlock = Block(1, uint256(0), uint256(0), 1386325540, 0x1e0ffff0, 99943);
    _genesisBlock.addTransaction(txNew);
    _genesisBlock.updateMerkleTree(); // genesisBlock
    assert(_genesisBlock.getHash() == _genesis);
    
    _checkpoints = boost::assign::map_list_of
    (     0, uint256("0x1a91e3dace36e2be3bf030a65679fe821aa1d6ef92e7c9902eb318182c355691"))
    ( 42279, uint256("0x8444c3ef39a46222e87584ef956ad2c9ef401578bd8b51e8e4b9a86ec3134d3a"))
    ( 42400, uint256("0x557bb7c17ed9e6d4a6f9361cfddf7c1fc0bdc394af7019167442b41f507252b4"))
    ;
    
    _seeds = boost::assign::list_of
    ("seed.dogecoin.com");

}

unsigned int DogecoinChain::totalBlocksEstimate() const {
    return _checkpoints.rbegin()->first;
}

bool DogecoinChain::checkPoints(const unsigned int height, const uint256& hash) const {
    Checkpoints::const_iterator i = _checkpoints.find(height);
    if (i == _checkpoints.end())
        return true;
    
    return hash == i->second;
}

const Block& DogecoinChain::genesisBlock() const {
    return _genesisBlock;
}

int static generateMTRandom(unsigned int s, int range)
{
    boost::mt19937 gen(s);
    boost::uniform_int<> dist(1, range);
    return dist(gen);
}

const int64_t DogecoinChain::subsidy(unsigned int height, uint256 prev) const {
    if (height == 0) return 88 * COIN;
    
    int64_t s = 500000 * COIN;
    
    std::string cseed_str = prev.toString().substr(7,7);
    const char* cseed = cseed_str.c_str();
    long seed = hex2long(cseed);
    int rand = generateMTRandom(seed, 999999);
    int rand1 = 0;
    
    if(height < 100000) {
        s = (1 + rand) * COIN;
    }
    else if(height < 145000) {
        cseed_str = prev.toString().substr(7,7);
        cseed = cseed_str.c_str();
        seed = hex2long(cseed);
        rand1 = generateMTRandom(seed, 499999);
        s = (1 + rand1) * COIN;
    }
    else if(height < 600000) {
        s >>= (height / 100000);
    }
    else {
        s = 10000 * COIN;
    }

    
    return s;
}

bool DogecoinChain::isStandard(const Transaction& tx) const {
    // on the test net everything is allowed
    return true;
}
/*
unsigned int ComputeMinWork(unsigned int nBase, int64_t nTime) {
    
    CBigNum bnResult;
    bnResult.SetCompact(nBase);
    
    while (nTime > 0 && bnResult < bnProofOfWorkLimit) {
        if(nBestHeight+1<nDiffChangeTarget){
            // Maximum 400% adjustment...
            bnResult *= 4;
            // ... in best-case exactly 4-times-normal target time
            nTime -= nTargetTimespan*4;
        } else {
            // Maximum 10% adjustment...
            bnResult = (bnResult * 110) / 100;
            // ... in best-case exactly 4-times-normal target time
            nTime -= nTargetTimespanNEW*4;
        }
    }
    if (bnResult > bnProofOfWorkLimit)
        bnResult = bnProofOfWorkLimit;
    return bnResult.GetCompact();
}
*/
const bool DogecoinChain::checkProofOfWork(const Block& block) const {
    uint256 hash = getPoWHash(block);
    
    unsigned int nBits = block.getBits();
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);
    
    // Check range
    if (proofOfWorkLimit() != 0 && (bnTarget <= 0 || bnTarget > proofOfWorkLimit()))
        return error("CheckProofOfWork() : nBits below minimum work");
    
    // Check proof of work matches claimed amount
    if (hash > bnTarget.getuint256())
        return error("CheckProofOfWork() : hash doesn't match nBits");
    
    return true;
}

int DogecoinChain::nextWorkRequired(BlockIterator blk) const {
    const int64_t nTargetTimespan = 4 * 60 * 60; // Dogecoin every 4 hours
    const int64_t nTargetTimespanNEW = 60; // Dogecoin every minute
    const int64_t nTargetSpacing = 60; // Dogecoin: Every minute
    const int64_t nInterval = nTargetTimespan / nTargetSpacing;
    const int64_t nDiffChangeTarget = 145000;
    
    unsigned int nProofOfWorkLimit = _genesisBlock.getBits();
    CBigNum bnProofOfWorkLimit;
    bnProofOfWorkLimit.SetCompact(nProofOfWorkLimit);
    int nHeight = blk.count();
    bool fNewDifficultyProtocol = (nHeight >= nDiffChangeTarget);
    
    
    
    int64_t retargetTimespan = nTargetTimespan;
//    int64_t retargetSpacing = nTargetSpacing;
    int64_t retargetInterval = nInterval;
    
    if (fNewDifficultyProtocol) {
        retargetInterval = nTargetTimespanNEW / nTargetSpacing;
        retargetTimespan = nTargetTimespanNEW;
    }
    
    // Genesis block
    if (blk.height() == 0)
        return nProofOfWorkLimit;
    
    // Only change once per interval
    if (nHeight % retargetInterval != 0)
    {
        return blk->bits;
    }
    
    // Dogecoin: This fixes an issue where a 51% attack can change difficulty at will.
    // Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz
    int blockstogoback = retargetInterval-1;
    if (nHeight != retargetInterval)
        blockstogoback = retargetInterval;
    
    // Go back by what we want to be 14 days worth of blocks
    BlockIterator former = blk - blockstogoback;
    
    // Limit adjustment step
    int64_t nActualTimespan = blk->time;
    nActualTimespan -= former->time;
    log_debug("  nActualTimespan = %d  before bounds\n", nActualTimespan);
    
    CBigNum bnNew;
    bnNew.SetCompact(blk->bits);
    
    if(fNewDifficultyProtocol) //DigiShield implementation - thanks to RealSolid & WDC for this code
    {
        // amplitude filter - thanks to daft27 for this code
        nActualTimespan = retargetTimespan + (nActualTimespan - retargetTimespan)/8;
        log_debug("DIGISHIELD RETARGET\n");
        if (nActualTimespan < (retargetTimespan - (retargetTimespan/4)) ) nActualTimespan = (retargetTimespan - (retargetTimespan/4));
        if (nActualTimespan > (retargetTimespan + (retargetTimespan/2)) ) nActualTimespan = (retargetTimespan + (retargetTimespan/2));
    }
    else if (nHeight > 10000) {
        if (nActualTimespan < nTargetTimespan/4)
            nActualTimespan = nTargetTimespan/4;
        if (nActualTimespan > nTargetTimespan*4)
            nActualTimespan = nTargetTimespan*4;
    }
    else if(nHeight > 5000)
    {
        if (nActualTimespan < nTargetTimespan/8)
            nActualTimespan = nTargetTimespan/8;
        if (nActualTimespan > nTargetTimespan*4)
            nActualTimespan = nTargetTimespan*4;
    }
    else
    {
        if (nActualTimespan < nTargetTimespan/16)
            nActualTimespan = nTargetTimespan/16;
        if (nActualTimespan > nTargetTimespan*4)
            nActualTimespan = nTargetTimespan*4;
    }
    
    // Retarget
    
    bnNew *= nActualTimespan;
    bnNew /= retargetTimespan;
    
    if (bnNew > bnProofOfWorkLimit)
        bnNew = bnProofOfWorkLimit;
    
    /// debug print
    log_debug("GetNextWorkRequired RETARGET\n");
    log_debug("nTargetTimespan = %d    nActualTimespan = %d\n", retargetTimespan, nActualTimespan);
    log_debug("Before: %08x  %s\n", blk->bits, CBigNum().SetCompact(blk->bits).getuint256().toString().c_str());
    log_debug("After:  %08x  %s\n", bnNew.GetCompact(), bnNew.getuint256().toString().c_str());
    
    return bnNew.GetCompact();
}

const uint256 DogecoinChain::getPoWHash(const Block& block) const {
    uint256 hash;
    scrypt_1024_1_1_256(BEGIN(block), BEGIN(hash));
    return hash;
}


const DogecoinChain dogecoin;

/* DOGECOIN TESTNET
 pchMessageStart[0] = 0xfc;
 pchMessageStart[1] = 0xc1;
 pchMessageStart[2] = 0xb7;
 pchMessageStart[3] = 0xdc;
 hashGenesisBlock = uint256("0xf5ae71e26c74beacc88382716aced69cddf3dffff24f384e1808905e0188f68f");
 */



const Currency ripplecredits("ripple", "XRP", 6);