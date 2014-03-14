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
#include <coin/NameOperation.h>
#include <coin/Logger.h>

#include <coinName/Names.h>

#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <numeric>

using namespace std;
using namespace boost;
using namespace sqliterate;

static const int NAMECOIN_TX_VERSION = 0x7100;

//
// BlockChain
//

typedef vector<unsigned char> Data;

BlockChain::BlockChain(const Chain& chain, const string dataDir) :
    sqliterate::Database(dataDir == "" ? ":memory:" : dataDir + "/blockchain.sqlite3", getMemorySize()/4),
    _chain(chain),
    _verifier(0),
    _lazy_purging(false),
    _purge_depth(0), // means no purging - i.e.
    _verification_depth(_chain.totalBlocksEstimate())
{
    _acceptBlockTimer = 0;
    _connectInputsTimer = 0;
    _verifySignatureTimer = 0;
    _setBestChainTimer = 0;
    _addToBlockIndexTimer = 0;
    _bestReceivedTime = 0;
    
    // setup the database tables
    // The blocks points backwards, so they could create a tree. Which tree to choose ? The best of them... So each time a new block is inserted, it is checked against the main chain. If the main chain needs to be updated it will be.
    
    size_t total_memory = getMemorySize();
    log_info("Total memory size: %d", total_memory);
    // use only 25% of the memory at max:
    const size_t page_size = 4096;
    size_t cache_size = total_memory/4/page_size;
    
    // we need to declare this static as the pointer to the c_str will live also outside this function
    static string pragma_page_size = "PRAGMA page_size = " + lexical_cast<string>(page_size);
    static string pragma_cache_size = "PRAGMA cache_size = " + lexical_cast<string>(cache_size);
    
    query("PRAGMA journal_mode=WAL");
    query("PRAGMA locking_mode=NORMAL");
    query("PRAGMA synchronous=OFF");
    query(pragma_page_size.c_str()); // this is 512MiB of cache with 4kiB page_size
    query(pragma_cache_size.c_str()); // this is 512MiB of cache with 4kiB page_size
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
    
    query("CREATE TABLE IF NOT EXISTS Confirmations ("
              "cnf INTEGER PRIMARY KEY AUTOINCREMENT," // coinbase transactions have cnf = -count
              "version INTEGER,"
              "locktime INTEGER,"
              "count INTEGER," // this points to the block where the Transaction was confirmed
              "idx INTEGER"
          ")");

    query("CREATE INDEX IF NOT EXISTS ConfCount ON Confirmations(count)");
    
    query("CREATE TABLE IF NOT EXISTS Unspents ("
              "coin INTEGER PRIMARY KEY AUTOINCREMENT,"
              "hash BINARY,"
              "idx INTEGER,"
              "value INTEGER,"
              "script BINARY,"
              "count INTEGER," // count is the block count where the spendable is confirmed (this can also be found in the confirmation, but we need it here for fast access) -- coinbases have negative height - the group of spendables are those unspent with a count bigger than -(MAX(count)-100)
              "ocnf INTEGER REFERENCES Confirmations(cnf)" // if this is < 0 the coin is part of a coinbase tx
          ")");
    
    query("CREATE INDEX IF NOT EXISTS UnspentsOut ON Unspents (ocnf)");

    query("CREATE INDEX IF NOT EXISTS UnspentCount ON Unspents(count)");
    
    query("CREATE TABLE IF NOT EXISTS Spendings ("
              "ocnf INTEGER REFERENCES Confirmations(cnf)," // ocnf is the confirmation that introduced the coin
              "coin INTEGER PRIMARY KEY," // this follows the same counting as the Unspents, except for coinbases, which has coin = -count
              "hash BINARY,"
              "idx INTEGER,"
              "value INTEGER,"
              "script BINARY,"
              "signature BINARY,"
              "sequence INTEGER,"
              "iidx INTEGER," // this is the input index - to ensure proper ordering of the inputs
              "icnf INTEGER REFERENCES Confirmations(cnf)" // icnf is the confirmation that spent the coin
          ")");

    //    query("CREATE INDEX IF NOT EXISTS SpendingIndex ON Spendings (hash, idx)");

    query("CREATE INDEX IF NOT EXISTS SpendingsIn ON Spendings (icnf)");
    query("CREATE INDEX IF NOT EXISTS SpendingsOut ON Spendings (ocnf)");
/*
    // MerkleTrie store - stores branch nodes of the merkle trie - so a lookup in the trie is done by:
    // * lookup branchnode by key
    // * if cached - lookup key
    // * if not cached - load branch, lookup key
    // * if branch altered, mark node dirty
    // * save
    // how to determine the optimal size of the branches ???
    // 
 
 // MerkleTrie store - stores branches of the MerkleTrie in chunks of a certain size:
 
    query("CREATE TABLE IF NOT EXISTS Branches ("
              "lower INTEGER PRIMARY KEY,"
              "upper INTEGER,"
              "hash BINARY,"
              "elements INTEGER,"
              "branch BINARY"
          ")");
    
    query("CREATE INDEX IF NOT EXISTS BranchesUpper ON Branches(upper)");
    
    // Now a query for a branch can be done like: "SELECT branch FROM Branches WHERE lower <= ? AND ? <= upper"
    //  
*/    
    // This table stores is the Auxiliary Proof-of-Works when using merged mining.
    query("CREATE TABLE IF NOT EXISTS AuxProofOfWorks ("
          "count INTEGER REFERENCES Blocks(count)," // linking this AuxPOW to the block chain - blocks with version&BLOCK_VERSION_AUXPOW are expected to have a row in this table
          "hash BINARY," // The hash of the auxiliary chain block
          "auxpow BINARY" // The auxiliary block proof of work - i.e. coinbase, merkle branch and header
          // Note the parent hash is not part of this table - we have the full header (parent) and it is not even checked in namecoin
    ")");

    // fast lookup based on count
    query("CREATE INDEX IF NOT EXISTS AuxProofOfWorksIndex ON AuxProofOfWorks (count)");
    query("CREATE INDEX IF NOT EXISTS AuxPoWHashIndex ON AuxProofOfWorks (hash)");
    
    // finally we have, for namecoin a name value store
    query("CREATE TABLE IF NOT EXISTS Names ("
              "coin PRIMARY KEY REFERENCES Unspents(coin),"
              "count INTEGER REFERENCES Blocks(count),"
              "name BINARY,"
              "value BINARY"
          ")");

    // enable fast lookup by name and height
    query("CREATE INDEX IF NOT EXISTS NamesIndex ON Names (name)");
    query("CREATE INDEX IF NOT EXISTS NamesCountIndex ON Names (count)");
    
    // populate the tree
    vector<BlockRef> blockchain = queryColRow<BlockRef(int, uint256, uint256, unsigned int, unsigned int)>("SELECT version, hash, prev, time, bits FROM Blocks ORDER BY count");
    _tree.assign(blockchain);
    _invalid_tree.assign(blockchain); // also assign the current tree to the invalid tree
        
    if (_tree.count() == 0) { // there are no blocks, insert the genesis block
        Block block = _chain.genesisBlock();
        blockchain.push_back(BlockRef(block.getVersion(), block.getHash(), block.getPrevBlock(), block.getBlockTime(), block.getBits()));
        _tree.assign(blockchain);
        _invalid_tree.assign(blockchain);
        
        _branches[block.getHash()] = block;
        try {
            query("BEGIN --GENESIS");
            Txns txns;
            Hashes hashes;
            BlockIterator blk = _tree.find(block.getHash());
            attach(blk, txns, hashes);
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
        _branches.clear();
    }
    updateBestLocator();
    log_info("BlockChain initialized - main best height: %d", _tree.height());
    
    // determine purge_depth from database:
    _purge_depth = query<int64_t>("SELECT CASE WHEN COUNT(*)=0 THEN 0 ELSE MIN(count) END FROM Confirmations");
    
    // determine validation index type from database:
    bool coin_index = query<int64_t>("SELECT COUNT(*) FROM SQLITE_MASTER WHERE name='UnspentIndex'");
    if (coin_index)
        _validation_depth = 0;
    else {
        _validation_depth = _chain.totalBlocksEstimate();

        // load the elements - i.e. the spendables
        Unspents spendables = queryColRow<Unspent(int64_t, uint256, unsigned int, int64_t, Script, int64_t, int64_t)>("SELECT coin, hash, idx, value, script, count, ocnf FROM Unspents WHERE count >= -?", _tree.count()-_chain.maturity(_tree.height())+1);
        
        for (Unspents::const_iterator u = spendables.begin(); u != spendables.end(); ++u)
            _spendables.insert(*u);
        
        Unspents immatures = queryColRow<Unspent(int64_t, uint256, unsigned int, int64_t, Script, int64_t, int64_t)>("SELECT coin, hash, idx, value, script, count, ocnf FROM Unspents WHERE count < -?", _tree.count()-_chain.maturity(_tree.height())+1);

        for (Unspents::const_iterator u = immatures.begin(); u != immatures.end(); ++u)
            _immature_coinbases.insert(*u);
    }
    
    
    // experimental - delete 10 blocks from the chain:
//    for (int i = 0; i < 10; i++) {
//        rollbackBlock(getBestHeight()+1);
//        _tree.pop_back();
//    }

}

int BlockChain::purge_depth() const {
    return _purge_depth;
}

void BlockChain::purge_depth(int purge_depth) {
    if (purge_depth < _purge_depth)
        log_warn("Requested a purge_depth (Persistance setting) deeper than currently, please re-download the blockchain to enforce this!");
    _purge_depth = purge_depth;
    query("DELETE FROM Spendings WHERE icnf IN (SELECT cnf FROM Confirmations WHERE count <= ?)", _purge_depth);
    query("DELETE FROM Confirmations WHERE count <= ?", _purge_depth);
}

void BlockChain::validation_depth(int v) {
    if (v == _validation_depth) return;

    _validation_depth = v;
    
    if (_validation_depth == 0) {
        query("CREATE UNIQUE INDEX IF NOT EXISTS UnspentIndex ON Unspents (hash, idx)");
    }
    else {
        query("DROP INDEX IF EXISTS UnspentIndex");
        if (_tree.count() < _validation_depth)
            _spendables.authenticated(false);
        else {
            _spendables.authenticated(true);
            log_info("MerkleTrie Hashing on with root hash: %s", _spendables.root()->hash().toString());
        }
    }
}

bool BlockChain::script_to_unspents() const {
    return query<int64_t>("SELECT COUNT(*) FROM SQLITE_MASTER WHERE name='ScriptIndex'");
}

void BlockChain::script_to_unspents(bool enable) {
    if (enable) {
        query("CREATE INDEX IF NOT EXISTS ScriptIndex ON Unspents (script)");
        query("CREATE INDEX IF NOT EXISTS SScriptIndex ON Spendings (script)");
    }
    else {
        query("DROP INDEX IF EXISTS ScriptIndex");
        query("DROP INDEX IF EXISTS SScriptIndex");
    }
}

std::pair<Claims::Spents, int64_t> BlockChain::try_claim(const Transaction& txn, bool verify) const {
    uint256 hash = txn.getHash();
    int64_t fee = 0;
    int64_t min_fee = 0;
    
    if (_claims.have(hash)) // transaction already exist
        throw Error("Transaction already exists : " + txn.getHash().toString());
    
    if (txn.isCoinBase())
        throw Error("Cannot claim coinbase transactions!");
    
    Claims::Spents spents;
    
    try {
        
        // BIP0016 check - if the time is newer than the BIP0016 date enforce strictPayToScriptHash
        bool strictPayToScriptHash = (UnixTime::s() > _chain.timeStamp(Chain::BIP0016));
        
        // redeem the inputs
        const Inputs& inputs = txn.getInputs();
        int64_t value_in = 0;
        NameOperation name_op(_chain.adhere_names()?_chain.enforce_name_rules(UnixTime::s()):false);
        for (size_t in_idx = 0; in_idx < inputs.size(); ++in_idx) {
            Unspent coin;
            const Input& input = inputs[in_idx];
            //  Already marked as spent? - either in an earlier claim or in this claim
            if (_claims.spent(input.prevout()) || spents.count(input.prevout()))
                throw Error("Coin already spent!");
            
            //  Among the outputs of a former, active, claim? - lookup the out hash in _unconfirmed / _claims
            Output output = _claims.prev(input.prevout());
            if (!output.isNull()) {
                coin = Unspent(0, input.prevout().hash, input.prevout().index, output.value(), output.script(), 0, 0);
            }
            else {
                //  3. are among the confirmed outputs in the Database / do a database lookup
                if (_validation_depth == 0) {
                    coin = queryRow<Unspent(int64_t, uint256, unsigned int, int64_t, Script, int64_t, int64_t)>("SELECT coin, hash, idx, value, script, count, ocnf FROM Unspents WHERE hash = ? AND idx = ?", input.prevout().hash, input.prevout().index);
                    if (!coin)
                        throw Reject("Spent coin not found !");
                    
                    if (coin.count < 0 && _tree.count() + coin.count < _chain.maturity(_tree.height()))
                        throw Error(cformat("Tried to spend immature (%d confirmations) coinbase: %s", _tree.count() + coin.count, hash.toString()).text());
                }
                else {
                    Spendables::Iterator is = _spendables.find(input.prevout());
                    if (!!is)
                        coin = *is;
                    else
                        throw Reject("Spent coin not found or immature coinbase");
                }
            }
            spents.insert(input.prevout());
            // all OK - spend the coin

            // lookup name in DB coin -> name lookup, as you cannot trust a coin.name
            if (_chain.adhere_names() && name_op.input(coin.output, coin.count))
                name_op.name(getCoinName(coin.coin));

            // Check for negative or overflow input values
            if (!_chain.range(coin.output.value()))
                throw Error("Input values out of range");
            
            value_in += coin.output.value();
            
            if (verify)
                if(!txn.verify(in_idx, coin.output.script(), 0, strictPayToScriptHash))
                    //                if(!VerifySignature(coin.output, txn, in_idx, strictPayToScriptHash, 0))
                    throw Error("Verify Signature failed with verifying: " + txn.getHash().toString());
        }
        
        // verify outputs
        fee = value_in - txn.getValueOut();
        if (fee < 0)
            throw Error("fee < 0");
        if (fee < min_fee)
            throw Error("fee < min_fee");
        
        if (_chain.adhere_names()) {
            for (Outputs::const_iterator out = txn.getOutputs().begin(); out != txn.getOutputs().end(); ++out)
                if (_chain.adhere_names() && name_op.output(*out)) {
                    if (txn.version() != NAMECOIN_TX_VERSION)
                        throw Error("Namecoin scripts only allowed in Namecoin transactions");
                    // finally check for conflicts with the name database
                    int count = getNameAge(name_op.name());
                    if (name_op.reserve()) {
                        if (count && count > _tree.count() - _chain.expirationDepth(_tree.count()))
                            throw Reject("Trying to reserve existing and not expired name: " + name_op.name());
                    }
                    else {
                        if (count == 0 || count <= _tree.count() - _chain.expirationDepth(_tree.count()))
                            throw Error("Trying to update expired or non existing name: " + name_op.name());
                    }
                }
            name_op.check_fees(_chain.network_fee(_tree.count()));
        }
    }
    catch (Reject& r) {
        throw Reject(string("claim(Transaction): ") + r.what());
    }
    catch (std::exception& e) {
        throw Error(string("claim(Transaction): ") + e.what());
    }

    return make_pair<Claims::Spents, int64_t>(spents, fee);
}

// claim, claims a transaction expecting it to go into a block in the near future
void BlockChain::claim(const Transaction& txn, bool verify) {
    pair<Claims::Spents, int64_t> res = try_claim(txn, verify);
    
    // we insert the unconfirmed transaction into a list/map according to the key: fee/size and delta-spendings    
    _claims.insert(txn, res.first, res.second);
    
    for (Listeners::const_iterator l = _listeners.begin(); l != _listeners.end(); ++l) {
        Listener& listener = (*l);
        listener(txn, UnixTime::s());
    }
}

int BlockChain::setMerkleBranch(MerkleTx& mtxn) const
{
    Transaction txn;
    Block block;
    int64_t height;
    int64_t time;
    
    getTransaction(mtxn.getHash(), txn, height, time);
    getBlock(height+1, block);
    // Update the tx's hashBlock
    mtxn._blockHash = block.getHash();
    
    // Locate the transaction
    TransactionList txes = block.getTransactions();
    mtxn._index = 0;
    for (TransactionList::const_iterator tx = txes.begin(); tx != txes.end(); ++tx, ++mtxn._index) {
        if (*tx == *(Transaction*)this)
            break;
    }
    
    if ((unsigned int)mtxn._index == txes.size()) {
        mtxn._merkleBranch.clear();
        mtxn._index = -1;
        log_debug("ERROR: SetMerkleBranch() : couldn't find tx in block\n");
        return 0;
    }
    
    // Fill in merkle branch
    mtxn._merkleBranch = block.getMerkleBranch(mtxn._index);
    
    return height < 0 ? 0 : height;
}


int64_t BlockChain::balance(const Script& script, int height) const {
    int count = height + 1;
    
    // we calculate two balances
    int64_t unspent = 0;
    if (count == 0)
        unspent = query<int64_t>("SELECT SUM(value) FROM Unspents WHERE script = ?", script);
    else
        unspent = query<int64_t>("SELECT SUM(value) FROM Unspents WHERE script = ? AND ABS(count) <= ?", script, count);
    
    if (count == 0)
        return unspent;
    
    int purged = query<int>("SELECT MIN(count) FROM Confirmations");

    if (count < purged)
        throw runtime_error("Cannot calculate balances from purged past!");
    
    int64_t created = query<int64_t>("SELECT SUM(value) FROM Spendings AS s, Confirmations AS o WHERE s.ocnf=o.cnf AND s.script = ? AND o.count > ?", script, count);
    
    int64_t spent = query<int64_t>("SELECT SUM(value) FROM Spendings AS s, Confirmations AS i WHERE s.icnf=i.cnf AND s.script = ? AND i.count > ?", script, count);

    return unspent-created+spent;
}

Unspent BlockChain::redeem(const Input& input, int iidx, Confirmation iconf) {
    Unspent coin;
    
    if (_validation_depth == 0) {
        coin = queryRow<Unspent(int64_t, uint256, unsigned int, int64_t, Script, int64_t, int64_t)>("SELECT coin, hash, idx, value, script, count, ocnf FROM Unspents WHERE hash = ? AND idx = ?", input.prevout().hash, input.prevout().index);

        if (!coin)
            throw Reject("Spent coin not found !");

        if (coin.count < 0 && iconf.count + coin.count < _chain.maturity(-coin.count))
            throw Error("Tried to spend immature coinbase");
    }
    else {
        _redeemStats.start();
        Spendables::Iterator is;
        is = _spendables.find(input.prevout());
        
        if (!!is)
            coin = *is;
        else
            throw Error("Spent coin: " + input.prevout().toString() + " not found or immature coinbase");
        
        _spendables.remove(is);
        _redeemStats.stop();
    }
    
    // all OK - spend the coin
    
    // Check for negative or overflow input values
    if (!_chain.range(coin.output.value()))
        throw Error("Input values out of range");

    if (iconf.count >= _purge_depth)
        query("INSERT INTO Spendings (coin, ocnf, hash, idx, value, script, signature, sequence, iidx, icnf) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", coin.coin, coin.cnf, input.prevout().hash, input.prevout().index, coin.output.value(), coin.output.script(), input.signature(), input.sequence(), iidx, iconf.cnf);
    query("DELETE FROM Unspents WHERE coin = ?", coin.coin);
    
    return coin;
}

int64_t BlockChain::issue(const Output& output, uint256 hash, unsigned int out_idx, Confirmation conf, bool unique) {
    int64_t count = conf.is_coinbase() ? -conf.count : conf.count;
    if (_validation_depth == 0) {
        if (unique)
            return query("INSERT INTO Unspents (hash, idx, value, script, count, ocnf) VALUES (?, ?, ?, ?, ?, ?)", hash, out_idx, output.value(), output.script(), count, conf.cnf); // will throw if trying to insert a dublicate value as the index is unique
        else
            return query("INSERT OR REPLACE INTO Unspents (hash, idx, value, script, count, ocnf) VALUES (?, ?, ?, ?, ?, ?)", hash, out_idx, output.value(), output.script(), count, conf.cnf);
    }
    else {
        int64_t coin = query("INSERT INTO Unspents (hash, idx, value, script, count, ocnf) VALUES (?, ?, ?, ?, ?, ?)", hash, out_idx, output.value(), output.script(), count, conf.cnf);
        
        _issueStats.start();
        Unspent unspent(coin, hash, out_idx, output.value(), output.script(), count, conf.cnf);

        // we need to test uniqueness explicitly of coinbases among the other immature coinbases
        if (conf.is_coinbase()) {
            if (unique) {
                Spendables::Iterator cb = _immature_coinbases.find(Coin(hash, out_idx));
                if (!!cb)
                    throw Error("Attempting to insert dublicate coinbase");
                cb = _spendables.find(Coin(hash, out_idx));
                if (!!cb)
                    throw Error("Attempting to insert dublicate coinbase");
            }
            _immature_coinbases.insert(unspent);
        }
        else
            _spendables.insert(unspent);
        _issueStats.stop();
        return coin;
    }
}

void BlockChain::maturate(int64_t count) {
    if (_validation_depth == 0)
        return;
    
    Unspents coinbase_unspents = queryColRow<Unspent(int64_t, uint256, unsigned int, int64_t, Script, int64_t, int64_t)>("SELECT coin, hash, idx, value, script, count, ocnf FROM Unspents WHERE count = ?", -count);

    for (Unspents::const_iterator cb = coinbase_unspents.begin(); cb != coinbase_unspents.end(); ++cb) {
        _spendables.insert(*cb);
        _immature_coinbases.remove(cb->key);
    }
}

void BlockChain::insertBlockHeader(int64_t count, const Block& block) {
    query("INSERT INTO Blocks (count, hash, version, prev, mrkl, time, bits, nonce) VALUES (?, ?, ?, ?, ?, ?, ?, ?)", count, block.getHash(), block.getVersion(), block.getPrevBlock(), block.getMerkleRoot(), block.getBlockTime(), block.getBits(), block.getNonce());

    // also insert the AuxPow extra header into the db:
    if (_chain.adhere_aux_pow() && block.getVersion()&BLOCK_VERSION_AUXPOW) {
        // serialize the AuxPow
        ostringstream os;
        os << block.getAuxPoW();
        string s = os.str();
        Data auxpow(s.begin(), s.end());
        uint256 hash = block.getAuxPoW().getHash();
        query("INSERT INTO AuxProofOfWorks (count, hash, auxpow) VALUES (?, ?, ?)", count, hash, auxpow);
    }
}

void BlockChain::updateName(const NameOperation& name_op, int64_t coin, int count) {
    // check the depth of the current (if any) name
    string name = name_op.name();
    Evaluator::Value raw_name(name.begin(), name.end());
    string value = name_op.value();
    Evaluator::Value raw_value(value.begin(), value.end());

    int latest_count = query<int>("SELECT count FROM Names WHERE name = ? ORDER BY count DESC LIMIT 1", raw_name);
    if (latest_count == 0 && raw_name.empty())
        latest_count = query<int>("SELECT count FROM Names WHERE name is null ORDER BY count DESC LIMIT 1");
    bool expired = (latest_count == 0) ? true : (latest_count < count - _chain.expirationDepth(count));
    if (name_op.reserve()) {
        if (count - name_op.input_count() < MIN_FIRSTUPDATE_DEPTH)
            throw Error("Tried to reserve a name less than 12 blocks after name_new: " + name_op.name());
        if (expired)
            query("INSERT INTO Names (name, value, coin, count) VALUES (?, ?, ?, ?)", raw_name, raw_value, coin, count);
        else
            throw Error("Tried to reserve non expired name: " + name);
    }
    else {
        if (!expired)
            query("INSERT INTO Names (name, value, coin, count) VALUES (?, ?, ?, ?)", raw_name, raw_value, coin, count);
        else
            throw Error("Tried to update an expired name: " + name);
    }
}

int BlockChain::getNameAge(const std::string& name) const {
    Evaluator::Value raw_name(name.begin(), name.end());
    return query<int64_t>("SELECT count FROM Names WHERE name = ? ORDER BY count DESC LIMIT 1", raw_name);
}

NameDbRow BlockChain::getNameRow(const std::string& name) const {
    Evaluator::Value raw_name(name.begin(), name.end());
    return queryRow<NameDbRow(int64_t, int, Evaluator::Value, Evaluator::Value)>("SELECT * FROM Names WHERE name = ? ORDER BY count DESC LIMIT 1", raw_name);
}

string BlockChain::getCoinName(int64_t coin) const {
    Evaluator::Value raw_name = query<Evaluator::Value>("SELECT name FROM Names WHERE coin=?", coin);
    return string(raw_name.begin(), raw_name.end());
}

void BlockChain::postTransaction(const Transaction txn, int64_t& fees, int64_t min_fee, BlockIterator blk, int64_t idx, bool verify) {
    Confirmation conf(txn, 0, blk.count());
    
    uint256 hash = txn.getHash();

    try {

        // BIP0016 check - if the block is newer than the BIP0016 date enforce strictPayToScriptHash
        bool strictPayToScriptHash = (blk->time > _chain.timeStamp(Chain::BIP0016));
        
        if (blk.count() >= _purge_depth)
            conf.cnf = query("INSERT INTO Confirmations (locktime, version, count, idx) VALUES (?, ?, ?, ?)", txn.lockTime(), txn.version(), blk.count(), idx);
        else
            conf.cnf = LOCKTIME_THRESHOLD; // we are downloading the chain - no need to create a confirmation
        
        // redeem the inputs
        const Inputs& inputs = txn.getInputs();
        int64_t value_in = 0;
        NameOperation name_op(_chain.adhere_names()?_chain.enforce_name_rules(blk.count()):false);
        for (size_t in_idx = 0; in_idx < inputs.size(); ++in_idx) {
            const Input& input = inputs[in_idx];
            Unspent coin = redeem(input, in_idx, conf); // this will throw in case of doublespend attempts
            const Output& output = coin.output;
            int count = coin.count;
            value_in += output.value();

            // use the name from the UTXO and not from the input - workaround from for bug
            if (_chain.adhere_names() && name_op.input(output, count))
                name_op.name(getCoinName(coin.coin));
            
            _verifySignatureTimer -= UnixTime::us();
            
            if (verify) // this is invocation only - the actual verification takes place in other threads
                _verifier.verify(output, txn, in_idx, strictPayToScriptHash, 0);
            
            _verifySignatureTimer += UnixTime::us();
        }
        
        // verify outputs
        int64_t fee = value_in - txn.getValueOut();
        if (fee < 0)
            throw Error("fee < 0");
        if (fee < min_fee)
            throw Error("fee < min_fee");
        fees += fee;
        if (!_chain.range(fees))
            throw Error("fees out of range");
        
        // issue the outputs
        const Outputs& outputs = txn.getOutputs();
        for (size_t out_idx = 0; out_idx < outputs.size(); ++out_idx) {
            int64_t coin = issue(outputs[out_idx], hash, out_idx, conf); // will throw in case of dublicate (hash,idx)
            if (_chain.adhere_names() && name_op.output(outputs[out_idx])) {
                if (txn.version() != NAMECOIN_TX_VERSION)
                    throw BlockChain::Error("Namecoin scripts only allowed in Namecoin transactions");
                updateName(name_op, coin, blk.count());
            }
        }
        if (_chain.adhere_names())
            name_op.check_fees(_chain.network_fee(blk.count()));
    }
    catch (NameOperation::Error& e) {
        throw Error("Error in transaction: " + hash.toString() + "\n" + txn.toString() + "\n\t" + e.what());
    }
    catch (Error& e) {
        throw Error("Error in transaction: " + hash.toString() + "\n" + txn.toString() + "\n\t" + e.what());
    }
    catch (std::exception& e) {
        log_error("Error in transaction: %s", hash.toString());
        throw;
    }
}

void BlockChain::postSubsidy(const Transaction txn, BlockIterator blk, int64_t fees) {
    
    if (!txn.isCoinBase())
        throw Error("postSubsidy only valid for coinbase transactions.");
    
    Confirmation conf(txn, 0, blk.count());
    
    uint256 hash = txn.getHash();
    
    if (blk.count() >= _purge_depth)
        conf.cnf = query("INSERT INTO Confirmations (cnf, locktime, version, count, idx) VALUES (?, ?, ?, ?, ?)", -blk.count(), txn.lockTime(), txn.version(), blk.count(), 0);
    else
        conf.cnf = -blk.count();

    // create the transaction and check that it is not spending already spent coins
    
    // insert coinbase into spendings
    const Input& input = txn.getInput(0);
    uint256 prev = blk.count() == 1 ? uint256(0) : (blk-1)->hash;
    int64_t value_in = _chain.subsidy(blk.height(), prev) + fees;
    if (value_in < txn.getValueOut())
        throw Error("value in < value out");
    if (blk.count() >= _purge_depth)
        query("INSERT INTO Spendings (ocnf, coin, hash, idx, value, script, signature, sequence, iidx, icnf) VALUES (?, ?, ?, ?, ?, ?, ?, ?, 0, ?)", 0, -blk.count(), uint256(0), -1, value_in, Script(), input.signature(), input.sequence(), -blk.count());
    
    // issue the outputs
    
    // BIP0030 check - transactions must be unique after a certain timestamp
    bool unique = blk->time > _chain.timeStamp(Chain::BIP0030);
    
    const Outputs& outputs = txn.getOutputs();
    for (size_t out_idx = 0; out_idx < outputs.size(); ++out_idx)
        issue(outputs[out_idx], hash, out_idx, conf, unique);

    if (_validation_depth > 0 && blk.count() > _chain.maturity(blk.height()))
        maturate(blk.count()-_chain.maturity(blk.height())+1);
}

void BlockChain::rollbackConfirmation(int64_t cnf) {
    // first get a list of spendings in which this coin was used as input and delete these iteratively...
    //    vector<int64_t> cnfs = queryCol<int64_t>("SELECT ocnf FROM Spendings WHERE icnf = ?");
    //for (size_t i = 0; i < cnfs.size(); ++i)
    //  rollbackConfirmation(cnfs[i], true);
    // delete the coins

    int64_t count = query<int64_t, int64_t>("SELECT count FROM Confirmations WHERE cnf = ?", cnf);
    
    if (_validation_depth > 0) {
        // iterate over spendings and undo them by converting spendings to unspents and remove correspoding unspent
        if (cnf > 0) {
            Unspents unspents = queryColRow<Unspent(int64_t, uint256, unsigned int, int64_t, Script, int64_t, int64_t)>("SELECT coin, hash, idx, value, script, ?, ocnf FROM Spendings WHERE icnf = ?", count, cnf); // we lose the block info count here !
            
            for (Unspents::const_iterator u = unspents.begin(); u != unspents.end(); ++u)
                _spendables.insert(*u);
            
            vector<Coin> coins = queryColRow<Coin(uint256, unsigned int)>("SELECT hash, idx FROM Unspents WHERE ocnf = ?", cnf);
            for (vector<Coin>::const_iterator c = coins.begin(); c != coins.end(); ++c)
                _spendables.remove(*c);
        }
    }
    
    if (cnf < 0) { // if coinbase don't create a matching unspent - but delete the Spending
        count = -count;
        query("DELETE FROM Spendings WHERE coin=?", count);
    }
    else {
        // now set count to when the unspent was originally introduced
        count = query<int64_t, int64_t>("SELECT count FROM Confirmations WHERE cnf = (SELECT ocnf FROM Spendings WHERE icnf=?)" , cnf);
        query("INSERT INTO Unspents (coin, hash, idx, value, script, count, ocnf) SELECT coin, hash, idx, value, script, ?, ocnf FROM Spendings WHERE icnf = ?", count, cnf);
        query("DELETE FROM Spendings WHERE icnf = ?", cnf);
    }
    
    if (_chain.adhere_names())
        query("DELETE FROM Names WHERE coin IN (SELECT coin FROM Unspents WHERE ocnf = ?)", cnf);
    
    query("DELETE FROM Unspents WHERE ocnf = ?", cnf);
    
    query("DELETE FROM Confirmations WHERE cnf = ?", cnf);
}

void BlockChain::rollbackBlock(int count) {
    typedef vector<int64_t> Cnfs;
    Cnfs cnfs = queryCol<int64_t>("SELECT cnf FROM Confirmations WHERE count = ? ORDER BY idx", count);
    // remove transactions in reverse order
    for (Cnfs::const_reverse_iterator tx = cnfs.rbegin(); tx != cnfs.rend(); ++tx) {
        rollbackConfirmation(*tx);
    }
    query("DELETE FROM Blocks WHERE count = ?", count);
    query("DELETE FROM AuxProofOfWorks WHERE count = ?", count);
}

void BlockChain::getBlockHeader(int count, Block& block) const {
    block = queryRow<Block(int, uint256, uint256, int, int, int)>("SELECT version, prev, mrkl, time, bits, nonce FROM Blocks WHERE count = ?", count);
}

void BlockChain::getBlock(int count, Block& block) const {
    block.setNull();

    if (count < getDeepestDepth())
        return;
    
    getBlockHeader(count, block);
    
    // now get the transactions
    vector<Confirmation> confs = queryColRow<Confirmation(int, unsigned int, int64_t, int64_t)>("SELECT cnf, version, locktime, count FROM Confirmations WHERE count = ? ORDER BY idx", count);
    
    for (size_t idx = 0; idx < confs.size(); idx++) {
        Inputs inputs = queryColRow<Input(uint256, unsigned int, Script, unsigned int)>("SELECT hash, idx, signature, sequence FROM Spendings WHERE icnf = ? ORDER BY iidx", confs[idx].cnf); // note that "ABS" as cnf for coinbases is negative!
        
        Outputs outputs = queryColRow<Output(int64_t, Script)>("SELECT value, script FROM (SELECT value, script, idx FROM Unspents WHERE ocnf = ?1 UNION SELECT value, script, idx FROM Spendings WHERE ocnf = ?1) ORDER BY idx", confs[idx].cnf);

        Transaction txn = confs[idx];
        txn.setInputs(inputs);
        txn.setOutputs(outputs);
        block.addTransaction(txn);
    }
    block.updateMerkleTree();
    try {
        _chain.check(block);
    }
    catch (std::exception& e) {
        block.setNull();
        log_warn("getBlock failed for count= %i with: %s", count, e.what());
    }
    
    // and get the AuxPoW if it is a merged mined block
    if (_chain.adhere_aux_pow() && block.getVersion()&BLOCK_VERSION_AUXPOW) {
        Data data = query<Data>("SELECT auxpow FROM AuxProofOfWorks WHERE count = ?", count);
        AuxPow auxpow;
        istringstream is(string(data.begin(), data.end()));
        is >> auxpow;
//        CDataStream(data) >> auxpow;
        block.setAuxPoW(auxpow);
    }
    //    cout << block.getHash().toString() << endl;
}

void BlockChain::attach(BlockIterator &blk, Txns& unconfirmed, Hashes& confirmed) {
    Block block = _branches[blk->hash];
    int height = blk.height(); // height for non trunk blocks is negative
    
    if (!_chain.checkPoints(height, blk->hash))
        throw Error("Rejected by checkpoint lockin at " + lexical_cast<string>(height));
    
    for(size_t idx = 0; idx < block.getNumTransactions(); ++idx)
        if(!isFinal(block.getTransaction(idx), height, blk->time))
            throw Error("Contains a non-final transaction");
    
    _verifier.reset();
    
    insertBlockHeader(blk.count(), block);
    
    // commit transactions
    int64_t fees = 0;
    int64_t min_fee = 0;
    bool verify = _verification_depth && (height > _verification_depth);
    for(size_t idx = 1; idx < block.getNumTransactions(); ++idx) {
        Transaction txn = block.getTransaction(idx);
        uint256 hash = txn.getHash();
        if (unconfirmed.count(hash) || _claims.have(hash)) // if the transaction is already among the unconfirmed, we have verified it earlier
            verify = false;
        postTransaction(txn, fees, min_fee, blk, idx, verify);
        unconfirmed.erase(hash);
        confirmed.insert(hash);
    }
    // post subsidy - means adding the new coinbase to spendings and the matured coinbase (100 blocks old) to spendables
    Transaction txn = block.getTransaction(0);
    postSubsidy(txn, blk, fees);
    
    if (!_verifier.yield_success())
        throw Error("Verify Signature failed with: " + _verifier.reason());    
}

void BlockChain::detach(BlockIterator &blk, Txns& unconfirmed) {
    Block block;
    getBlock(blk.count(), block);
    rollbackBlock(blk.count()); // this will also remove spendable coins and immature_coinbases
    Transactions& txns = block.getTransactions();
    for (Transactions::const_iterator tx = txns.begin() + 1; tx != txns.end(); ++tx) // skip the coinbase!
        unconfirmed[tx->getHash()] = *tx;
    _branches[blk->hash] = block; // store it in the branches map
}

// check if this is a share - it is not checked if the transactions in the share can be validated/verified etc,
// but at least it need to fit into the share chain
bool BlockChain::checkShare(const Block& share) const {
    uint256 hash = share.getHash();
    
    if (share.getVersion() < 3)
        return false;
    
    if (hash > share.getShareTarget())
        return false;
    
    // now check that it points to a block in the share chain
    ShareIterator prev = _share_tree.find(share.getPrevShare());
    
    if (prev == _share_tree.end() ) // this is not needed, however, if it does not point to a block in the share chain, it need to be a new share genesis, and if so it need to build up work over time - so we choose the best work from the last 24hr (144 blocks) to be our "best" chain.
        return false;

    // work required: if height%144 == 0 calculate - else choose last, if no last choose default
/*    int height = share.getHeight();
    int target = 0x1d00ffff;
    if (height%144) {
        target = prev->bits;
    }
    else {
        // iterate backwards to find the last 144 block
        ShareIterator blk = prev;
        //        while (
    }
    
    // check the next work required
    if (block.getHeight()%144) {
        if (block.getShareBits() != prev->bits)
            return false;
    }
    else { // time to adjust the work
        // count number of shares since last %144
        //
        
        
    }
    */
    
    try {
        if (share.getShareBits() != 0x1d00ffff)
            throw Error("Incorrect proof of work target");

        ShareTree::Dividend dividend;
        
        // check that the dividend in this share matches the one in the chain
        int64_t reward = 0;
        const Transaction& cb_txn = share.getTransaction(0);
        for (unsigned int i = 0; i < cb_txn.getNumOutputs(); ++i)
            reward += cb_txn.getOutput(i).value();
        int64_t fraction = reward/360;
        int64_t modulus = reward%360;
        
        unsigned int total = 0;
        for (unsigned int i = 1; i < cb_txn.getNumOutputs(); ++i) {
            const Script& script = cb_txn.getOutput(i).script();
            int64_t value = cb_txn.getOutput(i).value();
            if (value%fraction)
                throw Error("Wrong fractions in coinbase transaction");
            dividend[script] = value/fraction;
            
            total += value/fraction;
        }
        // now handle the reward to the share creater
        const Script& script = cb_txn.getOutput(0).script();
        int64_t value = cb_txn.getOutput(0).value();
        
        value -= modulus;
        if (value%fraction)
            throw Error("Wrong fractions in coinbase transaction 0");
        
        dividend[script] = value/fraction - 1;
        
        // now check that this matches with the numbers in the recent_fractions
        if (dividend != _share_tree.dividend(hash))
            throw Error("Fractions does not match the recent share fractions");
    }
    catch (Error& e) {
        log_info("checkShare: %s", e.what());
    }
    catch (...) {
    }
    return false;
}

// append(block)
// A block can be either a normal block or a "share", a share is used to bootstrap the block v3 network, providing a merkletrie root validation for all spendables.
// A share is a block candidate, and bound by the same rules, however, with the following exceptions:
// * must be a version 3 block
// * The hash does not fulfill the hash<target(bits), but only the relaxed hash<target(share_bits)
// * The best share is the one that forms the longest chain ~144 blocks back (360 shares) (hence the last share hash is part of the coinbase)
// * The coinbase transaction outputs follows a rule that the miner of the share is the first txout and the following 359 transactions are the miners of the last 360 shares (however, payouts to the same scripts is collapsed to save block space)
// When appending blocks normal procedures are followed, except checking if the block is also a share - if so, it is added to the share tree
// When appending shares (that are not valid blocks) the commit is canceled at its final state and the share is only added to the shares tree
// What to do if a share references a orphaned block - will it become invalidated ? - I think yes - so on reconstructs we need to find the best share as well

// as share means that the block will not be committed (well, unless it is already a valid block)
void BlockChain::append(const Block &block) {

    uint256 hash = block.getHash();

    bool valid_share = checkShare(block);
    bool invalid_block = !_chain.checkProofOfWork(block); //(hash > block.getTarget());
    
    if (invalid_block && !valid_share)
        throw Error("Block hash does not meet target and Block is not a Share either");
    
    Txns unconfirmed;
    Hashes confirmed;
    
    // check if we already have the block:
    BlockIterator blk = _tree.find(hash);
    if (blk != _tree.end())
        throw Error("Block already accepted");

    if (_invalid_tree.find(hash) != _invalid_tree.end())
        throw Error("Block already accepted as invalid");
    
    // this is enough to accept it as an invalid block - we simply do an insert:
    if (_invalid_tree.find(block.getPrevBlock()) != _invalid_tree.end())
        _invalid_tree.insert(BlockRef(block.getVersion(), hash, block.getPrevBlock(), block.getBlockTime(), block.getBits()));
    
    BlockIterator prev = _tree.find(block.getPrevBlock());
    
    // do the version check: If a super-majority of blocks within the last 1000 blocks are of version N type - reject blocks of versions below this
    if (block.getVersion() < getMinAcceptedBlockVersion())
        throw Error("Rejected version = " + lexical_cast<string>(block.getVersion())+ " block: version too old.");
    
    if (prev == _tree.end())
        throw Error("Cannot accept orphan block: " + hash.toString());

    if (block.getBits() != _chain.nextWorkRequired(prev)) {
        // check if we can allow a block to slip through due to max time:
        if (block.getTime() - (int)prev->time < _chain.maxInterBlockTime())
            throw Error("Incorrect proof of work: " + lexical_cast<string>(block.getBits()) + " versus " + lexical_cast<string>(_chain.nextWorkRequired(prev)));
        else if ( block.getBits() != _chain.proofOfWorkLimit().GetCompact())
            throw Error("Incorrect proof of work (limit): " + lexical_cast<string>(block.getBits()) + " versus " + lexical_cast<string>(_chain.proofOfWorkLimit().GetCompact()));
    }
        
    if (block.getBlockTime() <= getMedianTimePast(prev))
        throw Error("Block's timestamp is too early");

    int prev_height = abs(prev.height()); // we need to store this as the prev iterator will be invalid after insert.
    
    BlockTree::Changes changes = _tree.insert(BlockRef(block.getVersion(), hash, prev->hash, block.getBlockTime(), block.getBits()));
    // keep a snapshot of the spendables trie if we need to rollback, however, don't use it during download.
    Spendables snapshot = _spendables;

    if ((unsigned int)prev_height < _chain.totalBlocksEstimate() && changes.inserted.size() == 0)
        throw Error("Branching disallowed before last checkpoint at: " + lexical_cast<string>(_chain.totalBlocksEstimate()));
    
    _branches[hash] = block;

    if (changes.inserted.size() == 0)
        return;
    
    // for statistics - record the size of the claims now
    size_t claims_before = _claims.count();
    
    try {
        query("BEGIN --BLOCK");

        // now we need to check if the insertion of the new block will change the work
        // note that a change set is like a patch - it contains the blockrefs to remove and the blockrefs to add

        if (changes.deleted.size()) {
            // reorganization:
            log_info("Reorganizing at height %d: deleting %d blocks, inserting %d blocks", prev_height + 1, changes.deleted.size(), changes.inserted.size());
        }
        
        // loop over deleted blocks and detach them
        for (BlockTree::Hashes::const_iterator h = changes.deleted.begin(); h != changes.deleted.end(); ++h) {
            BlockIterator blk = _tree.find(*h);
            detach(blk, unconfirmed);
        }
        
        // loop over inserted blocks and attach them
        for (int i = changes.inserted.size() - 1; i >= 0; --i) {
            blk = _tree.find(changes.inserted[i]);
            attach(blk, unconfirmed, confirmed);
        }
        
        // purge spendings in old blocks - we can just as well serve other nodes with blocks if we have them (requires lazy purging)
        if (_purge_depth && !_lazy_purging && blk.count() >= _purge_depth) { // no need to purge during download as we don't store spendings anyway
            query("DELETE FROM Spendings WHERE icnf IN (SELECT cnf FROM Confirmations WHERE count <= ?)", _purge_depth);
            query("DELETE FROM Confirmations WHERE count <= ?", _purge_depth);
        }
        
        // Check that the block is conforming to its block version constraints
        int min_enforced_version = getMinEnforcedBlockVersion();
        switch (min_enforced_version > 0 ? min_enforced_version : 0) {
            default:
            case 3:
                if (block.getVersion() >= 3 && block.getSpendablesRoot() != _spendables.root()->hash())
                    throw Error("Version 3(or more) block with wrong or missing Spendable Root hash in coinbase rejected!");
            case 2:
                if (block.getVersion() >= 2 && block.getHeight() != prev_height + 1)
                    throw Error("Version 2(or more) block with wrong or missing height in coinbase rejected!");
            case 1: // nothing to enforce
            case 0: // nothing to enforce
                break;
        }
        
        // last check is if this block is also a share or only a share
        /*
        if (valid_share) {
            if(_share_tree.insert(ShareRef(block.getVersion(), hash, block.getPrevBlock(), block.getPrevShare(), block.getSpendablesRoot(), block.getTime(), block.getShareBits(), block.getRewardee())))
                _share_spendables = _spendables;
        }
        else { // check that the latest block commitment (reorganizaion) did not invalidate any shares
               //            while (!_tree.in_trunk(_share_tree.best()->getPrevHash())
               //                   _share_tree.pop_back();
        }
*/
        if (invalid_block) {
            query("ROLLBACK --BLOCK");
            _tree.pop_back();
            for (BlockTree::Hashes::const_iterator h = changes.deleted.begin(); h != changes.deleted.end(); ++h)
                _branches.erase(*h);
            
            _spendables = snapshot; // this will restore the Merkle Trie to its former state

            return;
        }
        
        // if we reach here, everything went as it should and we can commit.
        query("COMMIT --BLOCK");    
        
        // we have a commit - also commit the transactions to the merkle trie, which happens automatically when spendables goes out of scope
        
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

        _spendables = snapshot; // this will restore the Merkle Trie to its former state
        
        throw Error(string("append(Block): ") + e.what());
    }

    // the block(s) has been committed, now notify the listeners
    for (BlockTree::Hashes::const_iterator h = changes.inserted.begin(); h != changes.inserted.end(); ++h) {
        BlockIterator blk = _tree.find(hash);
        Block& block = _branches[hash];
        int height = blk.height();
    
        // iterate over transactions
        for (Transactions::const_iterator tx = block.getTransactions().begin(); tx != block.getTransactions().end(); ++tx) {
            for (Listeners::const_iterator l = _listeners.begin(); l != _listeners.end(); ++l) {
                Listener& listener = (*l);
                listener(*tx, height);
            }
        }
    }
    
    // switch on validation if we have more blocks than the validation depth
    if (_validation_depth > 0)
        _spendables.authenticated(_tree.count() >= _validation_depth);
    
    // Erase claims that have now been confirmed in a block
    for (Hashes::const_iterator h = confirmed.begin(); h != confirmed.end(); ++h)
        _claims.erase(*h);

    // delete all transactions more than 24 hrs old
    _claims.purge(UnixTime::s() - 24*60*60);
    
    // Claim transactions that didn't make it into a block, however, don't vaste time verifying, as we have done so already
    for (Txns::iterator tx = unconfirmed.begin(); tx != unconfirmed.end(); ++tx) {
        try {
            claim(tx->second, false);
        }
        catch (std::exception& e) {
            log_warn("append(Block): Transaction: %s was replaced - possible double spend or tx malleation!", tx->first.toString());
        }
    }

    size_t claims_after = _claims.count();

    log_info("BLOCK: %s", blk->hash.toString());
    log_info("\theight: %d @ %s, txns: %d", prev_height + 1, posix_time::to_simple_string(posix_time::from_time_t(block.getTime())), block.getNumTransactions());
    log_info("\tclaims: %d --> %d (resolved: %d)", claims_before, claims_after, claims_before-claims_after);
    if ((prev_height + 1)%1000 == 0) {
        log_info(statistics());
        log_info(_spendables.statistics());
        log_info("Redeem: %s", _redeemStats.str());
        log_info("Issue: %s", _issueStats.str());
        log_info("Signature verification time: %f.3s", 0.000001*_verifySignatureTimer);
        if (_spendables.root())
            log_info("This MerkleTrie Hash: %s", _spendables.root()->hash().toString());
    }

}


void BlockChain::outputPerformanceTimings() const {
    log_info("Performance timings: accept %d, addTo %.2f%%, setBest %.2f%%, connect %.2f%%, verify %.2f%%", _acceptBlockTimer/1000000, 100.*_addToBlockIndexTimer/_acceptBlockTimer, 100.*_setBestChainTimer/_acceptBlockTimer, 100.*_connectInputsTimer/_acceptBlockTimer, 100.*_verifySignatureTimer/_acceptBlockTimer );
}

// update best locator 
void BlockChain::updateBestLocator() {
    vector<int> heights;
    heights.push_back(_tree.height());
    int step = 1;
    // push back 10 heights, then double the steps until genesis is reached
    for (int i = 0;; i++) {
        if (heights.back() - step <= 0) break;
        heights.push_back(heights.back()-step);
        if (heights.size() > 10)
            step *= 2;
    }

    _bestLocator.have.clear();
    for (unsigned int i = 0; i < heights.size(); ++i) {
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

void BlockChain::getTransaction(const int64_t cnf, Transaction &txn) const {
    Confirmation conf = queryRow<Confirmation(int, unsigned int, int64_t, int64_t)>("SELECT cnf, version, locktime, count FROM Confirmations WHERE cnf = ?", cnf);

    Inputs inputs = queryColRow<Input(uint256, unsigned int, Script, unsigned int)>("SELECT hash, idx, signature, sequence FROM Spendings WHERE icnf = ? ORDER BY iidx", cnf);
    Outputs outputs = queryColRow<Output(int64_t, Script)>("SELECT value, script FROM (SELECT value, script, idx FROM Unspents WHERE ocnf = ?1 UNION SELECT value, script, idx FROM Spendings WHERE ocnf = ?1 ORDER BY idx ASC);", cnf);
    txn = conf;
    txn.setInputs(inputs);
    txn.setOutputs(outputs);
}

void BlockChain::getTransaction(const int64_t cnf, Transaction &txn, int64_t& height, int64_t& time) const {
    Confirmation conf = queryRow<Confirmation(int, unsigned int, int64_t, int64_t)>("SELECT cnf, version, locktime, count FROM Confirmations WHERE cnf = ?", cnf);
 
    if (conf.count > LOCKTIME_THRESHOLD) {
        height = -1;
        time = conf.count;
    }
    else {
        height = conf.count - 1;
        BlockIterator blk = iterator(height);
        time = blk->time;
    }
    
    Inputs inputs = queryColRow<Input(uint256, unsigned int, Script, unsigned int)>("SELECT hash, idx, signature, sequence FROM Spendings WHERE icnf = ? ORDER BY iidx", cnf);
    Outputs outputs = queryColRow<Output(int64_t, Script)>("SELECT value, script FROM (SELECT value, script, idx FROM Unspents WHERE ocnf = ?1 UNION SELECT value, script, idx FROM Spendings WHERE ocnf = ?1 ORDER BY idx ASC);", cnf);
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

int BlockChain::getDeepestDepth() const {
    return query<int64_t>("SELECT MIN(count) From Confirmations");
}

int BlockChain::getHeight(const uint256 hash) const
{
    // lock the pool and chain for reading
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);

    BlockIterator blk = _tree.find(hash);
    
    if (blk != _tree.end())
        return abs(blk.height());

    return -1;
}


bool BlockChain::haveTx(uint256 hash, bool must_be_confirmed) const
{
    return (_claims.have(hash));
    return true;
    // There is no index on hash, only on the hash + index - we shall assume that the hash + 0 is at least in the spendings, and hence we need not query for more.
    // Further, if we prune the database (remove spendings) we cannot answer this question (at least not if it is 
    int64_t cnf = query<int64_t>("SELECT ocnf FROM Unspents WHERE hash = ?", hash);
    if (!cnf)
        cnf = query<int64_t>("SELECT ocnf FROM Spendings WHERE hash = ?", hash);

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

bool BlockChain::isFinal(const Transaction& tx, int nBlockHeight, int64_t nBlockTime) const
{
    // Time based nLockTime implemented in 0.1.6
    if (tx.lockTime() == 0)
        return true;
    if (nBlockHeight == 0)
        nBlockHeight = _tree.height();
    if (nBlockTime == 0)
        nBlockTime = GetAdjustedTime();
    if ((int64_t)tx.lockTime() < (tx.lockTime() < LOCKTIME_THRESHOLD ? (int64_t)nBlockHeight : nBlockTime))
        return true;
    BOOST_FOREACH(const Input& txin, tx.getInputs())
        if (!txin.isFinal())
            return false;
    return true;
}

bool BlockChain::haveBlock(uint256 hash) const {
    return _tree.find(hash) != _tree.end() || _invalid_tree.find(hash) != _invalid_tree.end();
}

void BlockChain::getTransaction(const uint256& hash, Transaction& txn) const {
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);

    int64_t cnf = query<int64_t>("SELECT ocnf FROM Unspents WHERE hash = ? LIMIT 1", txn.getHash());
    
    getTransaction(cnf, txn);
}

void BlockChain::getTransaction(const uint256& hash, Transaction& txn, int64_t& height, int64_t& time) const
{
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);

    int64_t cnf = query<int64_t>("SELECT ocnf FROM Unspents WHERE hash = ? LIMIT 1", hash);

    getTransaction(cnf, txn, height, time);
}

bool BlockChain::isSpent(Coin coin, int confirmations) const {
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);

    if (confirmations > 1) {
        int count = query<int64_t>("SELECT count FROM Spendings AS s INNER JOIN Confirmations AS c WHERE s.icnf = c.cnf AND s.hash=? AND s.idx=?", coin.hash, coin.index);
        if (count && (getBestHeight() - abs(count) + 1 < confirmations - 1))
            return false;
    }
    
    if (_validation_depth == 0) // Database
        return query<int64_t>("SELECT coin FROM Unspents WHERE hash = ? AND idx = ?", coin.hash, coin.index) == 0;
    else
        return !_spendables.find(coin);
}

int BlockChain::getHeight(Coin coin) const {
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);
    
    if (_validation_depth == 0) // Database
        return query<int64_t>("SELECT count FROM Unspents WHERE hash = ? AND idx = ?", coin.hash, coin.index) - 1;
    else {
        Spendables::Iterator c = _spendables.find(coin);
        if (!!c)
            return c->count - 1;
        else
            return -1;
    }
}

void BlockChain::getUnspents(const Script& script, Unspents& unspents, unsigned int before) const {
    // check if we have an index of this:
    if (!script_to_unspents())
        throw Error("Lookup of unspents requires an INDEX!");
    
    unspents = queryColRow<Unspent(int64_t, uint256, unsigned int, int64_t, Script, int64_t, int64_t)>("SELECT coin, hash, idx, value, script, count, ocnf FROM Unspents WHERE script = ?", script);
    
    if (before == 0 || before > LOCKTIME_THRESHOLD) { // include unconfirmed transactions too
        typedef vector<pair<Coin, Output> > Claimed;
        Claimed claimed = _claims.claimed(script);
        for (Claimed::const_iterator c = claimed.begin(); c != claimed.end(); ++c) {
            unsigned int timestamp = _claims.timestamp(c->first.hash);
            if (!before || timestamp <= before)
                unspents.push_back(Unspent(0, c->first.hash, c->first.index, c->second.value(), c->second.script(), timestamp, 0));
        }
    }
    else { // remove those newer than before
        for (int i = unspents.size() - 1; i >= 0; --i)
            if (unspents[i].count > before)
                unspents.erase(unspents.begin()+i);
    }
    
    // finally remove spents
    for (int i = unspents.size() - 1; i >= 0; --i)
        if (_claims.spent(unspents[i].key))
            unspents.erase(unspents.begin()+i);
    
}

int BlockChain::getMinAcceptedBlockVersion() const {
    size_t quorum = _chain.accept_quorum();
    size_t majority = _chain.accept_majority();
    
    if (quorum < majority) return 1; // don't enforce
    
    map<int, size_t> bins; // map will initialize size_t using the default initializer size_t() which is 0.
    // iterate backwards in the chain until
    BlockTree::Iterator bi = _tree.best();
    size_t blocks = 0;
    do {
        if (++bins[bi->version] > majority) return bi->version;
        if (++blocks > quorum) {
            map<int, size_t>::const_reverse_iterator i = bins.rbegin();
            size_t count = i->second;
            while (++i != bins.rend()) {
                count += i->second;
                if (count > majority)
                    return i->first;
            }
        }
    } while(--bi != _tree.end());
    
    return 1;
}

int BlockChain::getMinEnforcedBlockVersion() const {
    size_t quorum = _chain.enforce_quorum();
    size_t majority = _chain.enforce_majority();

    if (quorum < majority) return 1; // don't enforce
    
    map<int, size_t> bins; // map will initialize size_t using the default initializer size_t() which is 0.
                          // iterate backwards in the chain until
    BlockTree::Iterator bi = _tree.best();
    size_t blocks = 0;
    do {
        if (++bins[bi->version] > majority) return bi->version;
        if (++blocks > quorum) {
            map<int, size_t>::const_reverse_iterator i = bins.rbegin();
            size_t count = i->second;
            while (++i != bins.rend()) {
                count += i->second;
                if (count > majority)
                    return i->first;
            }
        }
    } while(--bi != _tree.end());
    
    return 1;
}

// getBlockTemplate returns a block template - i.e. a block that has not yet been mined. An optional list of scripts with rewards given as :
// absolute of reward, absolute of fee, fraction of reward, fraction of fee, denominator
Block BlockChain::getBlockTemplate(BlockIterator blk, Payees payees, Fractions fractions, Fractions fee_fractions) const {
    // sanity check of the input parameters
    if (payees.size() == 0) throw Error("Trying the generate a Block Template with no payees");
    if (fractions.size() > 0 && fractions.size() != payees.size()) throw Error("Fractions should be either 0 or match the number of payees");
    if (fee_fractions.size() > 0 && fee_fractions.size() != payees.size()) throw Error("Fee fractions should be either 0 or match the number of payees and fractions");
    
    const int version = 3; // version 3 stores the block height as well as the merkle trie root hash in the coinbase
    const int timestamp = UnixTime::s();
    const unsigned int bits = _chain.nextWorkRequired(blk);
    const int nonce = 0;
    Block block(version, blk->hash, uint256(0), timestamp, bits, nonce);
    
    // now get the optimal set of transactions
    typedef vector<Transaction> Txns;
    int64_t fee = 0;
    Txns txns = _claims.transactions(fee);

    Spendables spendables = _spendables;
    for (Txns::const_iterator tx = txns.begin(); tx != txns.end(); ++tx) {
        uint256 hash = tx->getHash();
        for (size_t idx = 0; idx < tx->getNumOutputs(); ++idx) {
            const Output& output = tx->getOutput(idx);
            spendables.insert(Unspent(0, hash, idx, output.value(), output.script(), 0, 0));
        }
        
        for (Inputs::const_iterator i = tx->getInputs().begin(); i != tx->getInputs().end(); ++i)
            spendables.remove(i->prevout());
    }
    
    // insert the matured coinbase from block #-100
    int count = abs(blk.count());
    if (count > 0) {
        Unspents coinbase_unspents = queryColRow<Unspent(int64_t, uint256, unsigned int, int64_t, Script, int64_t, int64_t)>("SELECT coin, hash, idx, value, script, count, ocnf FROM Unspents WHERE count = ?", -(count-_chain.maturity(blk.height())));
        
        for (Unspents::const_iterator cb = coinbase_unspents.begin(); cb != coinbase_unspents.end(); ++cb)
            spendables.insert(*cb);
    }
    
    uint256 spendables_hash(0);
    if (false)
        spendables_hash = spendables.root()->hash();
    // insert the coinbase:
    // * calculate the merkle trie root hash
    // * if we are creating a block for distributed mining use that chain to determine the coinbase output

    Script coinbase;
    coinbase << count << spendables_hash; // a bit ugly: we are inserting the height of the block which is the count of the prev block
    Transaction coinbase_txn;
    coinbase_txn.addInput(Input(Coin(), coinbase));

    // and then the outputs!
    // we have the list of payees and their shares
    // first sum up the denominators
    int64_t denominator = accumulate(fractions.begin(), fractions.end(), 0);
    int64_t fee_denominator = accumulate(fee_fractions.begin(), fee_fractions.end(), 0);
    if (denominator == 0) denominator = payees.size();
    if (fee_denominator == 0) fee_denominator = denominator;

    int64_t subsidy = _chain.subsidy(count, blk->hash); // a bit ugly: the height of the block which is the count of the prev block
    for (size_t i = 0; i < payees.size(); ++i) {
        int64_t nominator = fractions.size() ? fractions[i] : 1;
        int64_t fee_nominator = fee_fractions.size() ? fee_fractions[i] : nominator;
        int64_t value = nominator*subsidy/denominator + fee_nominator*fee/fee_denominator;
        if (i == 0) value += subsidy%denominator + fee%fee_denominator;
        coinbase_txn.addOutput(Output(value, payees[i]));
    }

    block.addTransaction(coinbase_txn);
    
    for (Transactions::const_iterator tx = txns.begin(); tx != txns.end(); ++tx)
        block.addTransaction(*tx);
    
    block.updateMerkleTree();
    
    return block;
}

