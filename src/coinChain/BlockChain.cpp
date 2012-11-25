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

#include <coinChain/BlockChain.h>

#include <coin/Block.h>
#include <coinChain/MessageHeader.h>

#include <coinChain/Peer.h>

#include <coin/Script.h>
#include <coin/Logger.h>

#include <boost/lexical_cast.hpp>

using namespace std;
using namespace boost;
using namespace sqliterate;

// Confirmation is the Transaction class incl its DB index

struct Confirmation : public Transaction {
    Confirmation() : cnf(0), count(0) {}
    Confirmation(int64 cnf, int version, unsigned int locktime, int64 count) : Transaction(version, locktime), cnf(cnf), count(count) {}
    
    int64 cnf;
    int64 count;
    
    bool is_confirmed() { return (0 < count) && (count < LOCKTIME_THRESHOLD); }
    bool is_coinbase() {return (cnf < 0); }
};

struct Unspent : public Coin, public Output {
    Unspent() : coin(0), cnf(0) {}
    Unspent(int64 coin, uint256 hash, unsigned int idx, int64 value, Script script, int64 cnf) : Coin(hash, idx), Output(value, script), coin(coin),cnf(cnf) { }

    int64 coin;
    int64 cnf;

    bool is_valid() const { return (!Coin::isNull()) && (!Output::isNull()) && (cnf != 0); }
    
    bool operator!() { return !is_valid(); }
    
    static int64 hidx(uint256 hash, int index = 0) {
        return hash.getint64() + index;
    }
    
    int64 hidx() const {
        return hidx(hash, index);
    }
};

//
// BlockChain
//

BlockChain::BlockChain(const Chain& chain, BlockChain::Modes mode, const string dataDir) :
    Database(dataDir == "" ? ":memory:" : dataDir + "/" + (mode == Full ? "blockchain" :
                                                           (mode == Purged ? "purgedchain" :
                                                            (mode == CheckPoint ? "cpchain" : "spvchain"))) + ".sqlite3"),
    _chain(chain),
    _verifier(0),
    _mode(mode),
    _purge_depth(144)
{
    _acceptBlockTimer = 0;
    _connectInputsTimer = 0;
    _verifySignatureTimer = 0;
    _setBestChainTimer = 0;
    _addToBlockIndexTimer = 0;
    _bestReceivedTime = 0;
    
    // setup the database tables
    // The blocks points backwards, so they could create a tree. Which tree to choose ? The best of them... So each time a new block is inserted, it is checked against the main chain. If the main chain needs to be updated it will be.
    
    query("PRAGMA journal_mode=WAL");
    query("PRAGMA locking_mode=EXCLUSIVE");
    query("PRAGMA synchronous=OFF");
    query("PRAGMA page_size=16384"); // this is 512MiB of cache with 4kiB page_size
    query("PRAGMA cache_size=131072"); // this is 512MiB of cache with 4kiB page_size
    query("PRAGMA temp_store=MEMORY"); // use memory for temp tables
    
    query("CREATE TABLE IF NOT EXISTS Blocks ("
              "count INTEGER PRIMARY KEY," // block count is height+1 - i.e. the genesis has count = 1
              "hash BINARY,"
              "version INTEGER,"
              "prev BINARY,"
              "mrkl BINARY,"
              "time INTEGER,"
              "bits INTEGER,"
              "nonce INTEGER"
          ")");
    
//    query("CREATE INDEX IF NOT EXISTS BlockHash ON Blocks (hash)");

    query("CREATE TABLE IF NOT EXISTS Confirmations ("
              "cnf INTEGER PRIMARY KEY AUTOINCREMENT," // coinbase transactions have cnf = -count
              "version INTEGER,"
              "locktime INTEGER,"
              "count INTEGER," //for block transactions this points to a block (i.e. number less than 500.000.000) if > 500.000.000 a time is assumed
              "idx INTEGER"
          ")");

    query("CREATE TABLE IF NOT EXISTS Unspents ("
              "coin INTEGER PRIMARY KEY AUTOINCREMENT,"
              "hidx INTEGER,"
              "hash BINARY,"
              "idx INTEGER,"
              "value INTEGER,"
              "script BINARY,"
              "ocnf INTEGER REFERENCES Confirmations(cnf)" // if this is < 0 the coin is part of a coinbase tx
          ")");
    
//    query("CREATE INDEX IF NOT EXISTS UnspentIndex ON Unspents (hash, idx)");
    query("CREATE INDEX IF NOT EXISTS UnspentIndex ON Unspents (hidx)");
    
    query("CREATE TABLE IF NOT EXISTS Spendings ("
              "ocnf INTEGER REFERENCES Confirmations(cnf)," // ocnf is the confirmation that introduced the coin
              "coin INTEGER PRIMARY KEY," // this follows the same counting as the Unspents, except for coinbases, which has coin = -count
              "hidx INTEGER,"
              "hash BINARY,"
              "idx INTEGER,"
              "value INTEGER,"
              "script BINARY,"
              "signature BINARY,"
              "sequence INTEGER,"
              "icnf INTEGER REFERENCES Confirmations(cnf)" // icnf is the confirmation that spent the coin
          ")");

//    query("CREATE INDEX IF NOT EXISTS SpendingIndex ON Spendings (hash, idx)");
    query("CREATE INDEX IF NOT EXISTS SpendingIndex ON Spendings (hidx)");
    query("CREATE INDEX IF NOT EXISTS SpendingsIn ON Spendings (icnf)");
    query("CREATE INDEX IF NOT EXISTS SpendingsOut ON Spendings (ocnf)");
    
    // populate the tree
    vector<BlockRef> blockchain = queryColRow<BlockRef(uint256, uint256, unsigned int, unsigned int)>("SELECT hash, prev, time, bits FROM Blocks ORDER BY count");
    _tree.assign(blockchain);
        
    if (_tree.count() == 0) { // there are no blocks, insert the genesis block
        Block block = _chain.genesisBlock();
        try {
            query("BEGIN --GENESIS");
            insertBlockHeader(1, block);
            Transaction txn = block.getTransaction(0);
            query("INSERT INTO Confirmations (cnf, locktime, version, count, idx) VALUES (?, ?, ?, ?, ?)", -1, txn.lockTime(), txn.version(), 1, 0); // conf = count for coinbase confirmations - other confirmations have
            // now insert the spending - a coinbase has a special spending using no coin
            query("INSERT INTO Spendings (ocnf, coin, hidx, hash, idx, value, script, signature, sequence, icnf) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", 0, -1, uint256(0), Unspent::hidx(uint256(0)), 0, 50*COIN, Script(), txn.getInput(0).signature(), txn.getInput(0).sequence(), 1);
            query("INSERT INTO Unspents (hidx, hash, idx, value, script, ocnf) VALUES (?, ?, ?, ?, ?, ?)", Unspent::hidx(txn.getHash()), txn.getHash(), 0, 50*COIN, txn.getOutput(0).script(), -1);
            query("COMMIT --GENESIS");
        }
        catch (std::exception& e) {
            query("ROLLBACK --GENESIS");
            throw Error(string("BlockChain - creating genesisblock failed: ") + e.what());
        }
        catch (...) {
            query("ROLLBACK --GENESIS");
            throw Error("BlockChain - creating genesisblock failed");
        }
        _tree.assign(queryColRow<BlockRef(uint256, uint256, unsigned int, unsigned int)>("SELECT hash, prev, time, bits FROM Blocks ORDER BY count"));
    }
    updateBestLocator();
    log_info("BlockChain initialized - main best height: %d", _tree.height());
}

bool BlockChain::script_to_unspents() const {
    //    return query<int64>("");
}

void BlockChain::script_to_unspents(bool enable) {
    if (enable)
        query("CREATE INDEX IF NOT EXISTS ScriptIndex ON Unspents (script)");
    else
        query("DROP INDEX IF EXISTS ScriptIndex");
}


// accept transaction insert a transaction into the block chain - note this is only the for inserting unconfirmed txns
void BlockChain::acceptTransaction(const Transaction& txn, bool verify) {
    try {
        query("BEGIN --TRANSACTION");

        _verifier.reset();
        
        int64 fees = 0;
        acceptBlockTransaction(txn, fees, txn.getMinFee(1000, true, true), BlockIterator(), 0, verify);
        
        if (!_verifier.yield_success())
            throw Error("Verify Signature failed with: " + _verifier.reason());

        query("COMMIT --TRANSACTION");
    }
    catch (std::exception& e) {
        query("ROLLBACK --TRANSACTION");
        throw Error(string("acceptTransaction: ") + e.what());
    }
}

// accept transactions going into the leaf of the branch at idx. If the transaction is already confirmed in another branch, we only need to check that it can indeed be confirmed here too.

// accept branch transaction: if the transaction is already present - only check that it can be confirmed in this branch

void BlockChain::acceptBlockTransaction(const Transaction txn, int64& fees, int64 min_fee, BlockIterator blk, int64 idx, bool verify) {
    int64 timestamp = 0;

    bool nonfirmation;
    
    if (!blk) {
        nonfirmation = true;
        timestamp = GetTimeMillis();
        idx = timestamp;
        blk = _tree.best();
    }
    else {
        nonfirmation = false;
        timestamp = blk->time;
    }

    uint256 hash = txn.getHash();
    
    // BIP0016 check - if the block is newer than the BIP0016 date enforce strictPayToScriptHash
    bool strictPayToScriptHash = (timestamp > _chain.timeStamp(Chain::BIP0016));
    
    int64 cnf = 0;
    // lookup the hash to see if it is in use
    vector<int64> cnfs = queryCol<int64>("SELECT ocnf FROM Unspents WHERE hidx = ?", Unspent::hidx(txn.getHash()));
    
    // if the transaction is already present there are some options:
    // - it is unconfirmed - simply update it to be included in a block - also disable verification
    // - it is a retransmit - exit and not it is already committed
    // - it is one of the pre BIP0030 cases - i.e. coinbases gone wrong - create
    // - so if coinbase older than BIP0030 OR not there - create
    // - if unconfirmed getting confirmed - update
    // - else error
    if (cnfs.size()) {
        // check for false positive
        for (size_t i = 0; i < cnfs.size(); ++i) {
            Unspent coin = queryRow<Unspent(int64, uint256, unsigned int, int64, Script, int64)>("SELECT coin, hash, idx, value, script, ocnf FROM Unspents WHERE hidx = ?", Unspent::hidx(txn.getHash()));
            if (coin.hash == txn.getHash()) {
                cnf = coin.cnf;
                Confirmation conf = queryRow<Confirmation(int64, int, unsigned int, int64)>("SELECT cnf, version, locktime, count FROM Confirmations WHERE cnf = ?", cnf);
                
                /// BIP 30: this is enforced after March 15, 2012, 0:00 UTC
                /// Note that there is, before this date two violations, both are coinbases and they are in the blocks 91842, 91880
                if (timestamp < _chain.timeStamp(Chain::BIP0030) && conf.is_coinbase())
                    cnf = 0; // this will force recreation
                else if (!conf.is_confirmed() && !nonfirmation) {
                    // remove old confirmation and replace it with the new one, that we do not need to validate...
                    removeConfirmation(cnf, true); // throw if we are trying to remove a confirmation that is already in a block
                    acceptBlockTransaction(txn, fees, min_fee, blk, idx, false);
                    return;
                }
                else
                    throw Error("Transaction already committed");
                break;
            }
        }
    }

    if (!cnf) {
        if (!nonfirmation && idx == 0) { // coinbase
            if (_mode == Full || blk.height() > _chain.totalBlocksEstimate())
                cnf = query("INSERT INTO Confirmations (cnf, locktime, version, count, idx) VALUES (?, ?, ?, ?, ?)", -blk.count(), txn.lockTime(), txn.version(), blk.count(), idx);
            else
                cnf = -blk.count();
        }
        else if (_mode == Full || blk.height() > _chain.totalBlocksEstimate())
            cnf = query("INSERT INTO Confirmations (locktime, version, count, idx) VALUES (?, ?, ?, ?)", txn.lockTime(), txn.version(), blk.count(), idx);
        else
            cnf = LOCKTIME_THRESHOLD;
    }
    
    // create the transaction and check that it is not spending already spent coins
    
    // insert the inputs
    const Inputs& inputs = txn.getInputs();
    int64 value_in = 0;
    for (size_t in_idx = 0; in_idx < inputs.size(); ++in_idx) {
        const Input& input = inputs[in_idx];
        if (input.isSubsidy()) {
            // do coinbase checks
            if (nonfirmation)
                throw Error("Coinbase only valid in a block");
            if (idx != 0)
                throw Error("Coinbase at non zero index detected");
            int64 value = _chain.subsidy(blk.height());
            value_in += value;
            if (_mode == Full || blk.height() > _chain.totalBlocksEstimate())
                query("INSERT INTO Spendings (ocnf, coin, hidx, hash, idx, value, script, signature, sequence, icnf) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", 0, -blk.count(), Unspent::hidx(uint256(0)), uint256(0), 0, value, Script(), input.signature(), input.sequence(), blk.count());
            //            value += fees;
            //            query("INSERT INTO Unspents (hidx, hash, idx, value, script, ocnf) VALUES (?, ?, ?, ?, ?, ?)", Unspent::hidx(hash), hash, 0, value, txn.getOutput(0).script(), -blk.count());
        }
        else {
            Unspent coin = queryRow<Unspent(int64, uint256, unsigned int, int64, Script, int64)>("SELECT coin, hash, idx, value, script, ocnf FROM Unspents WHERE hidx = ?", Unspent::hidx(input.prevout().hash, input.prevout().index));
            
            // if coin not found, and we are committing a block, the coin could have been spent in an other unconfirmed transaction
            // look it up in spendings before we conclude that the coin is not there
            if (!coin && !nonfirmation) {
                // get a Confirmation of a possible spending
                Confirmation conf = queryRow<Confirmation(int64, int, unsigned int, int64)>("SELECT c.cnf, c.version, c.locktime, c.count FROM Confirmations c, Spendings s WHERE s.hidx = ? AND s.icnf = c.cnf", Unspent::hidx(input.prevout().hash, input.prevout().index));

                if (conf.cnf) {
                    // check if the spending was in a nonfirmation
                    if (!conf.is_confirmed()) { // note: this is an indication of an attempted double spending!
                        // remove the confirmation and get a handle to the Unspent coin
                        removeConfirmation(conf.cnf);
                        coin = queryRow<Unspent(int64, uint256, unsigned int, int64, Script, int64)>("SELECT coin, hash, idx, value, script, ocnf FROM Unspents WHERE hidx = ?", Unspent::hidx(input.prevout().hash, input.prevout().index));
                    }
                }
            }
            if (!coin)
                throw Reject("Spent coin not found !");

            // we need to check: if the coin is a coinbase or if it is nonfirmed
            // check if the unspent coin is a coinbase - if so check maturity
            int count = query<int>("SELECT count FROM Confirmations WHERE cnf = ?", coin.cnf);

            if (count > LOCKTIME_THRESHOLD) {
                if (!nonfirmation)
                    throw Error("Can only spend confirmed coins in a block");
            }
            else if (coin.coin < 0) { // coinbase transaction
                if(blk.count() - count < COINBASE_MATURITY)
                    throw Error("Tried to spend immature coinbase");
            }
            
            // all OK - spend the coin
            
            value_in += coin.value();
            // Check for negative or overflow input values
            if (!MoneyRange(coin.value()) || !MoneyRange(value_in))
                throw Error("Input values out of range");
            
            int64 t1 = GetTimeMicros();
            
            //            if (verify && !VerifySignature(coin, txn, in_idx, strictPayToScriptHash, 0))
            //    throw runtime_error("VerifySignature failed");

            if (verify) // this is invocation only - the actual verification takes place in other threads
                _verifier.verify(coin, txn, in_idx, strictPayToScriptHash, 0);
            
            _verifySignatureTimer += GetTimeMicros() - t1;
            
            if (_mode == Full || blk.height() > _chain.totalBlocksEstimate())
                query("INSERT INTO Spendings (ocnf, coin, hidx, hash, idx, value, script, signature, sequence, icnf) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", coin.coin, coin.cnf, coin.hidx(), coin.hash, coin.index, coin.value(), coin.script(), cnf);
            query("DELETE FROM Unspents WHERE coin = ?", coin.coin);
        }
    }

    // and now create the coins

    // verify outputs
    if (idx == 0) { // coinbase
        value_in += fees;
        
        if (value_in < txn.getValueOut())
            throw Error("value in < value out");
    }
    else {
        // Tally transaction fees
        int64 fee = value_in - txn.getValueOut();
        if (fee < 0)
            throw Error("fee < 0");
        if (fee < min_fee)
            throw Error("fee < min_fee");
        fees += fee;
        if (!MoneyRange(fees))
            throw Error("fees out of range");
    }
    
    // insert the outputs
    const Outputs& outputs = txn.getOutputs();
    for (size_t out_idx = 0; out_idx < outputs.size(); ++out_idx){
        const Output& output = outputs[out_idx];

        query("INSERT INTO Unspents (hidx, hash, idx, value, script, ocnf) VALUES (?, ?, ?, ?, ?, ?)", Unspent::hidx(hash, out_idx), hash, out_idx, output.value(), output.script(), cnf);
    }
}



void BlockChain::insertBlockHeader(int64 count, const Block& block) {
    query("INSERT INTO Blocks (count, hash, version, prev, mrkl, time, bits, nonce) VALUES (?, ?, ?, ?, ?, ?, ?, ?)", count, block.getHash(), block.getVersion(), block.getPrevBlock(), block.getMerkleRoot(), block.getBlockTime(), block.getBits(), block.getNonce());
}


void BlockChain::removeConfirmation(int64 cnf, bool only_nonfirmations) {
    // first get a list of spendings in which this coin was used as input and delete these iteratively...
    vector<int64> cnfs = queryCol<int64>("SELECT ocnf FROM Spendings WHERE icnf = ?");
    for (size_t i = 0; i < cnfs.size(); ++i)
        removeConfirmation(cnfs[i], true);
    // delete the coins
    query("DELETE FROM Unspents WHERE ocnf = ?", cnf);
    query("INSERT INTO Unspents (coin, hidx, hash, idx, value, script, ocnf) SELECT coin, hidx, hash, idx, value, script, ocnf FROM Spendings WHERE icnf = ?", cnf);
    query("DELETE FROM Spendings WHERE icnf = ?", cnf);
    // check that the confirmation is not in a block
    if (only_nonfirmations) {
        int64 count = query<int64, int64>("SELECT count FROM Confirmations WHERE cnf = ?", cnf);
        if (count && count < LOCKTIME_THRESHOLD)
            throw Error("Trying to remove a transaction already in a block!");
    }
        
    query("DELETE FROM Confirmations WHERE cnf = ?", cnf);
}

void BlockChain::removeBlock(int count) {
    typedef vector<int64> Cnfs;
    Cnfs cnfs = queryCol<int64>("SELECT cnf FROM Confirmations WHERE count = ?", count);
    // remove transactions in reverse order
    for (Cnfs::const_reverse_iterator tx = cnfs.rbegin(); tx != cnfs.rend(); ++tx) {
        removeConfirmation(*tx);
    }
    query("DELETE FROM Blocks WHERE count = ?", count);
}

void BlockChain::getBlockHeader(int count, Block& block) const {
    block = queryRow<Block(int, uint256, uint256, int, int, int)>("SELECT version, prev, mrkl, time, bits, nonce FROM Blocks WHERE count = ?", count);
}

void BlockChain::getBlock(int count, Block& block) const {
    block.setNull();

    getBlockHeader(count, block);
    
    // now get the transactions
    vector<Confirmation> confs = queryColRow<Confirmation(int, unsigned int, int64, int64)>("SELECT (version, locktime, cnf, count) FROM Confirmations WHERE count = ? ORDER BY idx", count);
    
    for (size_t idx = 0; idx < confs.size(); idx++) {
        Inputs inputs = queryColRow<Input(uint256, unsigned int, Script, unsigned int)>("SELECT (hash, idx, signature, sequence) FROM Spendings WHERE cin = ? ORDER BY idx", confs[idx].cnf);
        Outputs outputs = queryColRow<Output(int64, Script)>("SELECT value, script FROM (SELECT value, script, idx FROM Unspents WHERE ocnf = ?1 UNION SELECT value, script, idx FROM Spendings WHERE icnf = ?1 ORDER BY idx ASC);", confs[idx].cnf);
        Transaction txn = confs[idx];
        txn.setInputs(inputs);
        txn.setOutputs(outputs);
        block.addTransaction(txn);
    }
}

// accept block is more a offer block or block offered: consume(block) - output is a code that falls in 3 categories:
// accepted(added to best chain, reorganized, sidechain...)
// orphan - might be ok, but not able to check yet
// block is considered wrong
// throw: for some reason the block caused the logic to malfunction
void BlockChain::acceptBlock(const Block &block) {

    uint256 hash = block.getHash();

    typedef map<uint256, Transaction> Txns;
    Txns unconfirmed;
    
    // check if we already have the block:
    BlockIterator blk = _tree.find(hash);
    if (blk != _tree.end())
        throw Error("Block already accepted");

    BlockIterator prev = _tree.find(block.getPrevBlock());
    
    if (prev == _tree.end())
        throw Error("Cannot accept orphan block");
    
    if (block.getBits() != _chain.nextWorkRequired(prev))
        throw Error("Incorrect proof of work");
    
    if (block.getBlockTime() <= getMedianTimePast(prev))
        throw Error("Block's timestamp is too early");

    int prev_height = prev.height(); // we need to store this as the prev iterator will be invalid after insert - iterators are not 
    
    BlockTree::Changes changes = _tree.insert(BlockRef(hash, prev->hash, block.getBlockTime(), block.getBits()));
    try {
        query("BEGIN --BLOCK");

        // now we need to check if the insertion of the new block will change the work
        // note that a change set is like a patch - it contains the blockrefs to remove and the blockrefs to add

        if (prev_height < _chain.totalBlocksEstimate() && changes.inserted.size() == 0)
            throw Error("Branching disallowed before last checkpoint at: " + lexical_cast<string>(_chain.totalBlocksEstimate()));
        
        _branches[hash] = block;
        
        // loop over deleted blocks and remove them
        for (BlockTree::Hashes::const_iterator h = changes.deleted.begin(); h != changes.deleted.end(); ++h) {
            BlockIterator blk = _tree.find(*h);
            Block block;
            getBlock(blk.count(), block);
            removeBlock(blk.count());
            Transactions& txns = block.getTransactions();
            for (Transactions::const_iterator tx = txns.begin(); tx != txns.end(); ++tx)
                unconfirmed[tx->getHash()] = *tx;
            _branches[blk->hash] = block; // store it in the branches map
        }
        
        // loop over inserted blocks and insert them
        for (BlockTree::Hashes::const_iterator h = changes.inserted.begin(); h != changes.inserted.end(); ++h) {
            uint256 hsh = *h;
            Block block = _branches[*h];
            blk = _tree.find(*h);
            int height = blk.height(); // height for non trunk blocks is negative
            
            if (!_chain.checkPoints(height, blk->hash))
                throw Error("Rejected by checkpoint lockin at " + lexical_cast<string>(height));
            
            for(size_t idx = 0; idx < block.getNumTransactions(); ++idx)
                if(!isFinal(block.getTransaction(idx), height, blk->time))
                    throw Error("Contains a non-final transaction");

            _verifier.reset();
            
            insertBlockHeader(blk.count(), block);
            
            // commit transactions
            int64 fees = 0;
            int64 min_fee = 0;
            bool verify = (height > _chain.totalBlocksEstimate());
            for(size_t idx = 1; idx < block.getNumTransactions(); ++idx) {
                Transaction txn = block.getTransaction(idx);
                acceptBlockTransaction(txn, fees, min_fee, blk, idx, verify);
                unconfirmed.erase(txn.getHash());
            }
            Transaction txn = block.getTransaction(0);
            acceptBlockTransaction(txn, fees, min_fee, blk, 0);
            
            if (!_verifier.yield_success())
                throw Error("Verify Signature failed with: " + _verifier.reason());
            
            log_info("ACCEPT: New best=%s  height=%d", hash.toString().substr(0,20), height);
            if (height%1000 == 0) {
                log_info(statistics());
                log_info("Signature verification time: %f.3s", 0.000001*_verifySignatureTimer);
            }
        }
        
        // purge spendings in old blocks
        if (_mode == Purged && _purge_depth && prev_height > _chain.totalBlocksEstimate()) { // no need to purge during download as we don't store spendings anyway
            int64 old = prev_height - _purge_depth;
            if (old >= _chain.totalBlocksEstimate()) {
                query("DELETE FROM Spendings WHERE icnf IN (SELECT cnf FROM Confirmations WHERE count <= ?)", old);
                query("DELETE FROM Confirmations WHERE count <= ?", old);
            }
        }
        
        // if we reach here, everything went as it should and we can commit.
        query("COMMIT --BLOCK");
        // delete inserted blocks from the _branches
        for (BlockTree::Hashes::const_iterator h = changes.inserted.begin(); h != changes.inserted.end(); ++h)
            _branches.erase(*h);
        updateBestLocator();
    }
    catch (std::exception& e) {
        query("ROLLBACK --BLOCK");
        _tree.pop_back();
        for (BlockTree::Hashes::const_iterator h = changes.deleted.begin(); h != changes.deleted.end(); ++h)
            _branches.erase(*h);

        throw Error(string("acceptBlock: ") + e.what());
    }
        
    for (Txns::const_iterator tx = unconfirmed.begin(); tx != unconfirmed.end(); ++tx)
        acceptTransaction(tx->second, false);
}


void BlockChain::outputPerformanceTimings() const {
    log_info("Performance timings: accept %d, addTo %.2f%%, setBest %.2f%%, connect %.2f%%, verify %.2f%%", _acceptBlockTimer/1000000, 100.*_addToBlockIndexTimer/_acceptBlockTimer, 100.*_setBestChainTimer/_acceptBlockTimer, 100.*_connectInputsTimer/_acceptBlockTimer, 100.*_verifySignatureTimer/_acceptBlockTimer );
}

// update best locator 
void BlockChain::updateBestLocator() {
    vector<int64> heights;
    heights.push_back(_tree.height());
    int step = 1;
    for (int i = 0; heights.back() && i < step; i++)
        heights.push_back(heights.back()-step);
    if (heights.size() > 10)
        step *= 2;
    if (heights.back() == 0) heights.pop_back();

    _bestLocator.have.clear();
    for (int i = 0; i < heights.size(); ++i) {
        BlockIterator blk = iterator(heights[i]);
        _bestLocator.have.push_back(blk->hash);
    }
    _bestLocator.have.push_back(getGenesisHash());
}

const BlockLocator& BlockChain::getBestLocator() const {
    return _bestLocator;
}

int BlockChain::getDistanceBack(const BlockLocator& locator) const
{
    // lock the pool and chain for reading
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);
    // Retrace how far back it was in the sender's branch
    int distance = 0;
    int step = 1;
    for (vector<uint256>::const_iterator hash = locator.have.begin(); hash != locator.have.end(); ++hash) {
        BlockIterator blk = _tree.find(*hash);
        if (blk != _tree.end())
            return distance;
        distance += step;
        if (distance > 10)
            step *= 2;
    }
    return distance;
}

void BlockChain::getBlock(BlockIterator blk, Block& block) const {
    // lock the pool and chain for reading
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);
    block.setNull();
    getBlock(blk.count(), block);
}

void BlockChain::getBlock(const uint256 hash, Block& block) const
{
    // lookup in the database:
    BlockIterator blk = _tree.find(hash);
    
    if (!!blk) {
        getBlock(blk, block);
    }
}

void BlockChain::getTransaction(const int64 cnf, Transaction &txn) const {
    Confirmation conf = queryRow<Confirmation(int, unsigned int, int64, int64)>("SELECT (version, locktime, cnf, count) FROM Confirmations WHERE cnf = ?", cnf);
    
    Inputs inputs = queryColRow<Input(uint256, unsigned int, Script, unsigned int)>("SELECT (hash, idx, signature, sequence) FROM Spendings WHERE cin = ? ORDER BY idx", cnf);
    Outputs outputs = queryColRow<Output(int64, Script)>("SELECT value, script FROM (SELECT value, script, idx FROM Unspents WHERE ocnf = ?1 UNION SELECT value, script, idx FROM Spendings WHERE icnf = ?1 ORDER BY idx ASC);", cnf);
    txn = conf;
    txn.setInputs(inputs);
    txn.setOutputs(outputs);
}

void BlockChain::getTransaction(const int64 cnf, Transaction &txn, int64& height, int64& time) const {
    Confirmation conf = queryRow<Confirmation(int, unsigned int, int64, int64)>("SELECT (version, locktime, cnf, count) FROM Confirmations WHERE cnf = ?", cnf);
 
    if (conf.count > LOCKTIME_THRESHOLD) {
        height = -1;
        time = conf.count;
    }
    else {
        height = conf.count - 1;
        BlockIterator blk = iterator(conf.count);
        time = blk->time;
    }
    
    Inputs inputs = queryColRow<Input(uint256, unsigned int, Script, unsigned int)>("SELECT (hash, idx, signature, sequence) FROM Spendings WHERE cin = ? ORDER BY idx", cnf);
    Outputs outputs = queryColRow<Output(int64, Script)>("SELECT value, script FROM (SELECT value, script, idx FROM Unspents WHERE ocnf = ?1 UNION SELECT value, script, idx FROM Spendings WHERE icnf = ?1 ORDER BY idx ASC);", cnf);
    txn = conf;
    txn.setInputs(inputs);
    txn.setOutputs(outputs);
}

BlockIterator BlockChain::iterator(const BlockLocator& locator) const {
    // lock the pool and chain for reading
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);
    // Find the first block the caller has in the main chain
    for (vector<uint256>::const_iterator hash = locator.have.begin(); hash != locator.have.end(); ++hash) {
        BlockIterator blk = _tree.find(*hash);
        if (blk != _tree.end())
            return blk;
    }
    return _tree.begin(); // == the genesisblock
}

BlockIterator BlockChain::iterator(const uint256 hash) const {
    // lock the pool and chain for reading
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);
    // Find the first block the caller has in the main chain
    return _tree.find(hash);
}

double BlockChain::getDifficulty(BlockIterator blk) const {
    // Floating point number that is a multiple of the minimum difficulty,
    // minimum difficulty = 1.0.
    
    if(blk == _tree.end()) blk = _tree.best();

    int shift = (blk->bits >> 24) & 0xff;
    
    double diff = (double)0x0000ffff / (double)(blk->bits & 0x00ffffff);
    
    while (shift < 29) {
        diff *= 256.0;
        shift++;
    }
    while (shift > 29) {
        diff /= 256.0;
        shift--;
    }
    
    return diff;
}

uint256 BlockChain::getBlockHash(const BlockLocator& locator) const {
    // lock the pool and chain for reading
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);
    // Find the first block the caller has in the main chain
    BlockIterator blk = iterator(locator);

    return blk->hash;
}

bool BlockChain::isInMainChain(const uint256 hash) const {
    // lock the pool and chain for reading
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);

    BlockIterator blk = _tree.find(hash);
    return blk.height() >= 0;
}

int BlockChain::getHeight(const uint256 hash) const
{
    // lock the pool and chain for reading
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);

    BlockIterator blk = _tree.find(hash);
    
    if (blk != _tree.end())
        return abs(blk.height());

    // assume it is a tx

    //    return query<int64>("SELECT count FROM Confirmations WHERE ") - 1;
}


bool BlockChain::haveTx(uint256 hash, bool must_be_confirmed) const
{
    // There is no index on hash, only on the hash + index - we shall assume that the hash + 0 is at least in the spendings, and hence we need not query for more.
    // Further, if we prune the database (remove spendings) we cannot answer this question (at least not if it is 
    int64 cnf = query<int64, int64>("SELECT ocnf FROM Unspents WHERE hidx = ?", Unspent::hidx(hash));
    if (!cnf)
        cnf = query<int64, int64>("SELECT ocnf FROM Spendings WHERE hidx = ?", Unspent::hidx(hash));

    if (!must_be_confirmed)
        return cnf;
    
    if (!cnf)
        return false;
    
    int count = query<int>("SELECT count FROM Confirmations WHERE cnf = ?", cnf);
    
    if (count < LOCKTIME_THRESHOLD)
        return true;
    else
        return false;
}

bool BlockChain::isFinal(const Transaction& tx, int nBlockHeight, int64 nBlockTime) const
{
    // Time based nLockTime implemented in 0.1.6
    if (tx.lockTime() == 0)
        return true;
    if (nBlockHeight == 0)
        nBlockHeight = _tree.height();
    if (nBlockTime == 0)
        nBlockTime = GetAdjustedTime();
    if ((int64)tx.lockTime() < (tx.lockTime() < LOCKTIME_THRESHOLD ? (int64)nBlockHeight : nBlockTime))
        return true;
    BOOST_FOREACH(const Input& txin, tx.getInputs())
        if (!txin.isFinal())
            return false;
    return true;
}

bool BlockChain::haveBlock(uint256 hash) const
{
    return _tree.find(hash) != _tree.end();
}

void BlockChain::getTransaction(const uint256& hash, Transaction& txn) const {
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);

    int64 cnf = query<int64>("SELECT cnf FROM Unspents WHERE hash = ? LIMIT 1", txn.getHash());
    
    getTransaction(cnf, txn);
}

void BlockChain::getTransaction(const uint256& hash, Transaction& txn, int64& height, int64& time) const
{
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);

    int64 cnf = query<int64>("SELECT cnf FROM Unspents WHERE hash = ? LIMIT 1", txn.getHash());

    getTransaction(cnf, txn, height, time);
}

Transactions BlockChain::unconfirmedTransactions() const {
    // lock the pool and chain for reading
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);
    
    Transactions txns;
    std::vector<int64> cnfs = queryCol<int64>("SELECT cnf FROM Confirmations WHERE count > ?", LOCKTIME_THRESHOLD);
    
    for (std::vector<int64>::const_iterator cnf = cnfs.begin(); cnf != cnfs.end(); ++cnf) {
        Transaction txn;
        getTransaction(*cnf, txn);
        txns.push_back(txn);
    }
    
    return txns;
}

bool BlockChain::isSpent(Coin coin) const {
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);

    int64 c = query<int64, int64>("SELECT coin FROM Unspents WHERE hidx = ?", Unspent::hidx(coin.hash, coin.index));

    return c;
}

bool BlockChain::beingSpent(Coin coin) const {
    int64 cnf = query<int64, int64>("SELECT c.cnf FROM Confirmations c, Unspents u WHERE c.ocnf = u.cnf AND u.hidx = ? AND c.count > ?", Unspent::hidx(coin.hash, coin.index), LOCKTIME_THRESHOLD);
    return (cnf != 0);
}

int64 BlockChain::value(Coin coin) const {
    // get the transaction, then get the Output of the prevout, then get the value
    Transaction tx;
    getTransaction(coin.hash, tx);
    if (!tx.isNull() && coin.index < tx.getNumOutputs()) {
        Output out = tx.getOutput(coin.index);
        return out.value();
    }
    return 0;
}