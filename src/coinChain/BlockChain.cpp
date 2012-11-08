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

//
// BlockChain
//

BlockChain::BlockChain(const Chain& chain, const string dataDir) : Database((dataDir == "" ? CDB::dataDir(chain.dataDirSuffix()) : dataDir) + "/blockchain.sqlite3"), _chain(chain)
    {

    _acceptBlockTimer = 0;
    _connectInputsTimer = 0;
    _verifySignatureTimer = 0;
    _setBestChainTimer = 0;
    _addToBlockIndexTimer = 0;
    _bestReceivedTime = 0;
    
    // setup the database tables
    // The blocks points backwards, so they could create a tree. Which tree to choose ? The best of them... So each time a new block is inserted, it is checked against the main chain. If the main chain needs to be updated it will be.
    
    execute("PRAGMA journal_mode=WAL");
    execute("PRAGMA locking_mode=EXCLUSIVE");
    execute("PRAGMA synchronous=OFF");
    execute("PRAGMA page_size=16384"); // this is 512MiB of cache with 4kiB page_size
    execute("PRAGMA cache_size=131072"); // this is 512MiB of cache with 4kiB page_size
    execute("PRAGMA temp_store=MEMORY"); // use memory for temp tables
    
    execute("CREATE TABLE IF NOT EXISTS Blocks ("
                "blk INTEGER PRIMARY KEY AUTOINCREMENT,"
                "hash BINARY,"
                "version INTEGER,"
                "prev INTEGER REFERENCES Blocks(blk),"
                "mrkl BINARY,"
                "time INTEGER,"
                "bits INTEGER,"
                "nonce INTEGER"
            ")");
    
    execute("CREATE INDEX IF NOT EXISTS BlockHash ON Blocks (hash)");
    
    execute("CREATE TABLE IF NOT EXISTS Transactions ("
                "tx INTEGER PRIMARY KEY AUTOINCREMENT,"
                "hash BINARY,"
                "version INTEGER,"
                "locktime INTEGER"
            ")");

    execute("CREATE INDEX IF NOT EXISTS TransactionHash ON Transactions (hash)");
    
    // This table maps the transactions to blocks - it is needed as each transaction can belong to several blocks, further, it can be unconfirmed
    // The unconfirmed transactions have the following properties: blk=0, idx=timestamp
    execute("CREATE TABLE IF NOT EXISTS Confirmations ("
                "tx INTEGER REFERENCES Transactions(tx),"
                "blk INTEGER REFERENCES Blocks(blk),"
                "idx INTEGER,"
                "PRIMARY KEY (tx, blk)"
            ")");

    execute("CREATE INDEX IF NOT EXISTS BlockConfirmations ON Confirmations (blk)");
    /*
    execute("CREATE TABLE IF NOT EXISTS Scripts ("
                "script INTEGER PRIMARY KEY AUTOINCREMENT,"
                "data BINARY"
            ")");

    execute("CREATE INDEX IF NOT EXISTS ScriptIndex ON Scripts (data)");
     */

    execute("CREATE TABLE IF NOT EXISTS Outputs ("
                "coin INTEGER PRIMARY KEY AUTOINCREMENT,"
                "tx INTEGER REFERENCES Transactions(tx),"
                "idx INTEGER,"
                "val INTEGER,"
                "script BINARY"
            ")");

    execute("CREATE INDEX IF NOT EXISTS CoinIndex ON Outputs (tx, idx)");
    
    execute("CREATE TABLE IF NOT EXISTS Inputs ("
                "spent INTEGER PRIMARY KEY AUTOINCREMENT,"
                "tx INTEGER REFERENCES Transactions(tx),"
                "idx INTEGER,"
                "coin INTEGER REFERENCES Outputs(coin),"
                "signature BINARY,"
                "sequence INTEGER"
            ")");

    // makes it fast to lookup if a coin is spent, and where...
    execute("CREATE INDEX IF NOT EXISTS SpentIndex ON Inputs (coin)");
    // lookup inputs by tx
    execute("CREATE INDEX IF NOT EXISTS TxIndex ON Inputs (tx)");

    execute("CREATE TABLE IF NOT EXISTS Spendables ("
                "hash BINARY,"
                "idx INTEGER,"
                "val INTEGER,"
                "script BINARY,"
                "height INTEGER" // height gives the height in the block chain and
            ")");

    // initialize all the database actions

    _findBlock = prepare("SELECT blk FROM Blocks WHERE hash = ?");
    _insertBlock = prepare("INSERT INTO Blocks (hash, version, prev, mrkl, time, bits, nonce) VALUES (?, ?, ?, ?, ?, ?, ?)");
    
    _findBlockTransactions = prepare("SELECT tx FROM Confirmations WHERE blk = ? ORDER BY idx ASC");
    
    _insertConfirmation = prepare("INSERT INTO Confirmations (tx, blk, idx) VALUES (?, ?, ?)");
    _deleteConfirmation = prepare("DELETE FROM Confirmations WHERE blk = ? AND tx = ?");
    
    _findTransaction = prepare("SELECT tx FROM Transactions WHERE hash = ?");
    _insertTransaction = prepare("INSERT INTO Transactions (hash, version, locktime) VALUES (?, ?, ?)");
    _getTransaction = prepare("SELECT version, locktime FROM Transactions WHERE tx = ?");
    
    _getConfirmationBlocks = prepare("SELECT blk FROM Confirmations WHERE tx = ?");

    //    _alreadySpent = prepare("SELECT COUNT(*) FROM Inputs, Confirmations, MainChain WHERE (Confirmations.blk = MainChain.blk OR Confirmations.blk = 0) AND Confirmations.tx = Inputs.tx AND Inputs.tx <> ?1 AND coin IN (SELECT coin FROM Inputs WHERE tx = ?1)");
    _getInputConfirmationBlocks = prepare("SELECT Confirmations.blk FROM Inputs, Confirmations WHERE Confirmations.tx = Inputs.tx AND Inputs.coin IN (SELECT coin FROM Inputs WHERE tx=?)");
    
    _confirmationIdx = prepare("SELECT idx FROM Confirmations WHERE blk = ? AND tx = ?");
    
    _nonfirmationTime = prepare("SELECT idx FROM Confirmations WHERE tx = ? AND blk = 0");
    
    _getBlock = prepare("SELECT b.version, p.hash, b.mrkl, b.time, b.bits, b.nonce FROM Blocks b, (SELECT COALESCE((SELECT hash FROM Blocks WHERE blk = (SELECT prev FROM Blocks WHERE blk = ?1)), X'0000000000000000000000000000000000000000000000000000000000000000') AS hash) p WHERE blk = ?1");

    _findOutput = prepare("SELECT coin FROM Outputs WHERE tx = ? AND idx = ?");
    _getOutput = prepare("SELECT Outputs.val, Outputs.script FROM Outputs WHERE Outputs.coin = ?");
    _getOutputs = prepare("SELECT Outputs.val, Outputs.script FROM Outputs WHERE Outputs.tx = ? ORDER BY Outputs.idx ASC");
    _insertOutput = prepare("INSERT INTO Outputs (tx, idx, val, script) VALUES (?, ?, ?, ?)");

    // Check if a transactions spent pointers are used by other transactions already confirmed in this chain
    //    _alreadySpent = prepare("SELECT COUNT(*) FROM Inputs, Confirmations, MainChain WHERE (Confirmations.blk = MainChain.blk OR Confirmations.blk = 0) AND Confirmations.tx = Inputs.tx AND Inputs.tx <> ?1 AND coin IN (SELECT coin FROM Inputs WHERE tx = ?1)");
    
    _insertInput = prepare("INSERT INTO Inputs (tx, idx, coin, signature, sequence) VALUES (?, ?, ?, ?, ?)");
    _getInputs = prepare("SELECT Transactions.hash, Outputs.idx, Inputs.signature, Inputs.sequence FROM Transactions, Outputs, Inputs WHERE Inputs.tx = ? AND Outputs.coin = Inputs.coin AND Transactions.tx = Outputs.tx ORDER BY Inputs.idx ASC");
    
    //   _findScript = prepare("SELECT script FROM Scripts WHERE data = ?");
    //    _insertScript = prepare("INSERT INTO Scripts (data) VALUES (?)");

    // Check in which blocks an output has been spent
    _spentInBlocks = prepare("SELECT Confirmations.blk FROM Inputs, Confirmations WHERE Inputs.coin = ? AND Inputs.tx = Confirmations.tx");
    
    // Being spent takes a prevout / webcoin (hash,idx) and checks if a blk=0 confirmation (nonfirmation) exists:
    _beingSpent = prepare("SELECT COUNT(*) FROM Confirmations, Inputs WHERE Confirmations.blk = 0 AND Inputs.tx = Confirmations.tx AND Inputs.coin = (SELECT coin FROM Outputs WHERE tx = (SELECT tx FROM Transactions WHERE hash = ?) AND idx = ?)");

    // Being spent takes a prevout / webcoin and checks if a mainchain confirmation/nonfirmation exists:
    //    _isSpent = prepare("SELECT COUNT(*) FROM Confirmations, Inputs, MainChain WHERE (Confirmations.blk = MainChain.blk OR Confirmations.blk = 0) AND Inputs.tx = Confirmations.tx AND Inputs.coin = ?");

    /// Pruning:
    _prunedTransactions = prepare("SELECT tx FROM Outputs WHERE coin IN (SELECT coin FROM Inputs WHERE tx = ?) GROUP BY tx");
    
    _pruneOutputs = prepare("DELETE FROM Outputs WHERE coin IN (SELECT coin FROM Inputs WHERE tx = ?)");
    _pruneInputs = prepare("DELETE FROM Inputs WHERE tx = ?");
    
    _countOutputs = prepare("SELECT COUNT(*) FROM Outputs WHERE tx = ?");
    
    _pruneTransaction = prepare("DELETE FROM Transactions WHERE tx = ?");
    _pruneConfirmation = prepare("DELETE FROM Confirmations WHERE tx = ?");

    
    // find dump all block references:
    StatementVec<BlockRef, 5, uint256, int64, int64, unsigned int, unsigned int> dumpBlockRefs = prepare("SELECT hash, blk, prev, time, bits FROM Blocks ORDER BY blk");
    
    // populate the tree
    _tree.assign(dumpBlockRefs());
    
    if (_tree.height() < 0) { // there are no blocks, insert the genesis block
        Block block = _chain.genesisBlock();
        try {
            begin();
            int64 key = _insertBlock(block.getHash(), block.getVersion(), 0, block.getMerkleRoot(), block.getTime(), block.getBits(), block.getNonce());
            Transaction txn = block.getTransaction(0);
            int64 tx = _insertTransaction(txn.getHash(), txn.version(), txn.lockTime());
            _insertConfirmation(tx, key, 0);
            // now insert the input
            _insertInput(tx, 0, 0, txn.getInput(0).signature(), txn.getInput(0).sequence());
            //            int64 scr = _insertScript(txn.getOutput(0).script());
            _insertOutput(tx, 0, 50*COIN, txn.getOutput(0).script());
            commit();
        }
        catch (std::exception& e) {
            rollback();
            throw Error(string("BlockChain - creating genesisblock failed: ") + e.what());
        }
        catch (...) {
            rollback();
            throw Error("BlockChain - creating genesisblock failed");
        }
        _tree.assign(dumpBlockRefs()); // load the genesis block into the tree
    }
    updateBestLocator();
    log_info("BlockChain initialized - main best height: %d", _tree.height());
}

// accept transaction insert a transaction into the block chain - note this is only the for inserting unconfirmed txns
void BlockChain::acceptTransaction(const Transaction& txn, bool verify) {
    try {
        begin();
        
        int64 fees = 0;
        acceptBlockTransaction(txn, fees, txn.getMinFee(1000, true, true), BlockIterator(), 0, verify);
        
        commit();
    }
    catch (...) {
        rollback();
        throw;
    }
}

// accept transactions going into the leaf of the branch at idx. If the transaction is already confirmed in another branch, we only need to check that it can indeed be confirmed here too.

// accept branch transaction: if the transaction is already present - only check that it can be confirmed in this branch

void BlockChain::acceptBlockTransaction(const Transaction txn, int64& fees, int64 min_fee, BlockIterator blk, int64 idx, bool verify) {
    // BIP0016 check - if the block is newer than the BIP0016 date enforce strictPayToScriptHash
    bool create = true;
    int64 timestamp = 0;
    int64 key = 0;

    if (!blk) {
        timestamp = GetTimeMillis();
        idx = timestamp;
        blk = _tree.best();
    }
    else {
        timestamp = blk->time;
        key = blk->key;
    }

    bool strictPayToScriptHash = (timestamp > _chain.timeStamp(Chain::BIP0016));
    
    // lookup the tx to see if it is already present
    int64 tx = _findTransaction(txn.getHash());
    
    vector<int64> keys;
    if (tx) {
        // check if the transaction is in our branch already:
        keys = _getConfirmationBlocks(tx);
        /// BIP 30: this is enforced after March 15, 2012, 0:00 UTC
        /// Note that there is, before this date two violations, both are coinbases and they are in the blocks 91842, 91880
        if (timestamp > _chain.timeStamp(Chain::BIP0030)) {
            if (blk.ancestor(keys) != _tree.end())
                throw Error("Transaction already committed to this branch");
        }
        verify = false;
        create = false;
    }
    else
        tx = _insertTransaction(txn.getHash(), txn.version(), txn.lockTime());
    
    _insertConfirmation(tx, key, idx);

    // create the transaction and check that it is not spending already spent coins, relative to its confirmation chain
    
    // insert the inputs
    const Inputs& inputs = txn.getInputs();
    int64 value_in = 0;
    for (size_t in_idx = 0; in_idx < inputs.size(); ++in_idx) {
        const Input& input = inputs[in_idx];
        int64 coin;
        if (input.isSubsidy()) {
            // do coinbase checks
            if (key == 0)
                throw Error("Coinbase only valid in a block");
            if (idx != 0)
                throw Error("Coinbase at non zero index detected");
            
            coin = 0; // subsidy has no input!
            value_in += _chain.subsidy(abs(blk.height()));
        }
        else {
            // find the spent coin
            int64 prevtx = _findTransaction(input.prevout().hash);
            int64 previdx = input.prevout().index;

            if (!prevtx)
                throw Reject("Transaction for spent coin not found");
            // check that it exists and is in this branch
            vector<int64> prevkeys = _getConfirmationBlocks(prevtx);
            
            BlockIterator prevblk = blk.ancestor(prevkeys); // returns the first ancestor among the iterators with key or _tree.end() otherwise
            
            bool nonfirmed = count(prevkeys.begin(), prevkeys.end(), 0);

            if (prevblk == _tree.end() && !nonfirmed)
                throw Error("Transaction for spent coin not confirmed in this branch");

            if (prevblk != _tree.end()) {
                int64 prevconf = _confirmationIdx(prevblk->key, prevtx);
                if (blk != _tree.end() && prevconf == 0) { // using coinbase
                    if (blk.height() - prevblk.height() < COINBASE_MATURITY) // coinbase
                        throw Error("Tried to spend immature coinbase");
                }
            }
            
            coin = _findOutput(prevtx, previdx);
            if (!coin)
                throw Reject("Spent coin not found !");
            
            // check that it has not been spent in this branch or spent by in another nonfirmed tx
            vector<int64> spent_in_blks = _spentInBlocks(coin);
            if (blk.ancestor(spent_in_blks) != _tree.end())
                throw Error("Coin already Spent in this branch");
            if (key == 0 && count(spent_in_blks.begin(), spent_in_blks.end(), 0))
                throw Error("Coin already Spent (but not confirmed yet)");
            
            if (!create)
                continue;
            
            // get the value
            Output output = _getOutput(coin);
            
            value_in += output.value();
            // Check for negative or overflow input values
            if (!MoneyRange(output.value()) || !MoneyRange(value_in))
                throw Error("Input values out of range");
            
            int64 t1 = GetTimeMicros();
            
            if (verify && !VerifySignature(output, txn, in_idx, strictPayToScriptHash, 0))
                throw runtime_error("VerifySignature failed");
            
            _verifySignatureTimer += GetTimeMicros() - t1;
        }
        _insertInput(tx, in_idx, coin, input.signature(), input.sequence());
    }
    
    if (create) {
        // verify outputs
        if (idx == 0) {
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
            //            int64 script = _findScript(output.script());
            //            if (script == 0) // do we need a new script ?
            //                script = _insertScript(output.script());
            
            _insertOutput(tx, out_idx, output.value(), output.script());
        }
    }
}

void BlockChain::reacceptTransaction(int64 tx) {
    // assume that tx exist - accept to main branch:
    // check that inputs exists in this branch and they are not already spend (in this branch)
    // get the list of inputs:
    int64 now = GetTimeMillis();
    BlockIterator blk = _tree.best();
    try {
        begin();
        vector<int64> keys = _getInputConfirmationBlocks(tx);
        if (blk.ancestor(keys) == _tree.end())
            _insertConfirmation(tx, 0, now);
        commit();
    }
    catch (std::exception& e) {
        rollback();
        throw Error(string("acceptTransaction: ") + e.what());
    }
}

void BlockChain::acceptBlock(const Block &block) {

    uint256 hash = block.getHash();

    typedef set<int64> Txes;
    Txes unconfirmed;
    
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

    try {
        begin();

        int64 key = _insertBlock(hash, block.getVersion(), prev->key, block.getMerkleRoot(), block.getBlockTime(), block.getBits(), block.getNonce());

        // now we need to check if the insertion of the new block will change the work
        // note that a change set is like a patch - it contains the blockrefs to remove and the blockrefs to add
        BlockTree::Changes changes = _tree.insert(BlockRef(hash, key, prev->key, block.getBlockTime(), block.getBits()));
        
        // get a list of unconfirmed txes:
        if (changes.deleted.size()) { // only for reconstructions!
            for (BlockTree::Keys::const_iterator key = changes.deleted.begin(); key != changes.deleted.end(); ++key) {
                vector<int64> txes = _findBlockTransactions(*key);
                unconfirmed.insert(txes.begin() + 1, txes.end());
            }
            for (BlockTree::Keys::const_iterator key = changes.inserted.begin(); key != changes.inserted.end(); ++key) {
                vector<int64> txes = _findBlockTransactions(*key);
                for (vector<int64>::const_iterator tx = txes.begin() + 1; tx != txes.end(); ++tx)
                    unconfirmed.erase(*tx);
            }
        }
        
        blk = _tree.find(key);
        int height = blk.height();
        
        if (height < 0 && -height < _chain.totalBlocksEstimate())
            throw Error("Branching disallowed before last checkpoint at: " + lexical_cast<string>(_chain.totalBlocksEstimate()));
        
        if (!_chain.checkPoints(height, hash))
            throw Error("Rejected by checkpoint lockin at " + lexical_cast<string>(height));
        
        for(size_t idx = 0; idx < block.getNumTransactions(); ++idx)
            if(!isFinal(block.getTransaction(idx), abs(height), blk->time))
                throw Error("Contains a non-final transaction");

        // commit transactions
        int64 fees = 0;
        int64 min_fee = 0;
        bool verify = (abs(height) > _chain.totalBlocksEstimate());
        for(size_t idx = 1; idx < block.getNumTransactions(); ++idx) {
            Transaction txn = block.getTransaction(idx);
            acceptBlockTransaction(txn, fees, min_fee, blk, idx, verify);
        }
        Transaction txn = block.getTransaction(0);
        acceptBlockTransaction(txn, fees, min_fee, blk, 0);
        
        // check if we got a new best chain
        if (height >= 0) {
            log_info("ACCEPT: New best=%s  height=%d", hash.toString().substr(0,20), height);
            if (height%1000 == 0) {
                log_info(statistics());
                log_info("Signature verification time: %f.3s", 0.000001*_verifySignatureTimer);
            }
        }
        // if we reach here, everything went as it should and we can commit.
        commit();
        updateBestLocator();
    }
    catch (std::exception& e) {
        rollback();
        _tree.pop_back();
        throw Error(string("acceptBlock: ") + e.what());
    }
    
    for (Txes::const_iterator tx = unconfirmed.begin(); tx != unconfirmed.end(); ++tx)
        reacceptTransaction(*tx);

    // now trim the tree
    if (_tree.height() < COINBASE_MATURITY)
        return;
    BlockTree::Pruned pruned = _tree.trim(_tree.height() - COINBASE_MATURITY);
    // delete branches
    for (BlockTree::Keys::const_iterator key = pruned.branches.begin(); key != pruned.branches.end(); ++key) {
        // delete block with key and all its content
    }
    // prune trunk
    //     To prune a block you remove all inputs and the outputs they spend
    //     Further remove all empty transactions
    return; // no pruning!
    try {
        begin();
        for (BlockTree::Keys::const_iterator key = pruned.trunk.begin(); key != pruned.trunk.end(); ++key) {
            // loop over all transactions
            typedef vector<int64> Txes;
            Txes txes = _findBlockTransactions(*key);
            for (Txes::const_iterator tx = txes.begin(); tx != txes.end(); ++tx) {
                Txes pruned_txes = _prunedTransactions(*tx);
                _pruneOutputs(*tx);
                _pruneInputs(*tx);
                
                for (Txes::const_iterator ptx = pruned_txes.begin(); ptx != pruned_txes.end(); ++ ptx) {
                    if (_countOutputs(*tx) == 0) {
                        _deleteTransaction(*tx);
                        _deleteConfirmation(*tx);
                    }
                }
            }
        }
        commit();
    }
    catch (std::exception& e) {
        rollback();
        //        _tree.untrim();
        throw Error(string("acceptBlock - trim: ") + e.what());
    }
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
    block = _getBlock(blk->key);
    
    // obtain a list of txids from the database:
    vector<int64> txs = _findBlockTransactions(blk->key); // this list is sorted according to idx in the block
    
    for (vector<int64>::iterator itx = txs.begin(); itx != txs.end(); ++itx) {
        Transaction txn;
        getTransaction(*itx, txn);
        block.addTransaction(txn);
    }
    
}

void BlockChain::getBlock(const uint256 hash, Block& block) const
{
    // lookup in the database:
    BlockIterator blk = _tree.find(hash);

    if (!!blk) {
        getBlock(blk, block);
    }
    else { // now try if the hash was for a transaction
        int64 tx = _findTransaction(hash);
        if (tx) {
            // get block from transaction and look up again...
        }
    }
}

void BlockChain::getTransaction(const int64 tx, Transaction &txn) const {
    // find the transaction:
    txn = _getTransaction(tx);
    
    // fill it with outputs and inputs
    Outputs outputs = _getOutputs(tx);
    for (Outputs::const_iterator out = outputs.begin(); out != outputs.end(); ++out)
        txn.addOutput(*out);

    Inputs inputs = _getInputs(tx);
    for (Inputs::const_iterator in = inputs.begin(); in != inputs.end(); ++in)
        txn.addInput(*in);
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
    
    int64 tx = _findTransaction(hash);
    vector<int64> keys = _getConfirmationBlocks(tx);
    
    blk = _tree.best().ancestor(keys);
    
    if (blk != _tree.end())
        return blk.height();
    
    return -1;
}


bool BlockChain::haveTx(uint256 hash, bool must_be_confirmed) const
{
    int64 tx = _findTransaction(hash);
    vector<int64> keys = _getConfirmationBlocks(tx);
    
    if (!must_be_confirmed && count(keys.begin(), keys.end(), 0) > 0) // nonfirmed
        return true;
    
    BlockIterator blk = _tree.best().ancestor(keys);
    
    if (blk == _tree.end())
        return false;
    
    return true;
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

    txn.setNull();
    int64 tx = _findTransaction(hash);
    if (tx)
        getTransaction(tx, txn);
}

void BlockChain::getTransaction(const uint256& hash, Transaction& txn, int64& height, int64& time) const
{
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);

    txn.setNull();
    int64 tx = _findTransaction(hash);
    if (!tx) return;
    
    // check if the transaction is confirmed in the main chain
    
    vector<int64> keys = _getConfirmationBlocks(tx);
    
    BlockIterator blk = _tree.best().ancestor(keys); // returns the first ancestor among the iterators with key or _tree.end() otherwise
    
    if (blk != _tree.end()) {
        height = blk.height();
        Block block = _getBlock(blk->key);
        time = blk->time;
    }
    else {
        // if not does it have a nonfirmation, and what is the time (idx)
        int64 idx = _nonfirmationTime(tx);
        if (idx) {
            time = idx;
            height = -1;
        }
        else
            return;
    }
    
    getTransaction(tx, txn);
}

bool BlockChain::isSpent(Coin coin) const {
    boost::shared_lock< boost::shared_mutex > lock(_chain_and_pool_access);

    int64 out = _findOutput(coin.hash, coin.index);

    if (!out)
        return true; // technically we don't know... but we cannot claim it is not spent
    
    // check if it is used in some inputs
    vector<int64> keys = _spentInBlocks(out);
    
    // check that it exists and is in this branch
    BlockIterator blk = _tree.best().ancestor(keys); // returns the first ancestor among the iterators with key or _tree.end() otherwise

    if (blk != _tree.end())
        return true; // spent in best chain
    
    bool nonfirmed = count(keys.begin(), keys.end(), 0);
    
    return nonfirmed; // being spent
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