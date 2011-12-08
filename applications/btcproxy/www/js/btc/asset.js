
// Coin
btc.define.Coin = function() {
	var Coin = btc.Coin = function(hash, n) {
		if (arguments.length == 1) {
			var str = hash;
			var bytes = Crypto.util.hexToBytes(str);
			this.hash = bytes.slice(0, 32);
			var idx = bytes.slice(32, 36);
			this.n = idx[0] + idx[1] << 8 + idx[2] << 16 + idx[3] << 24;
        }
        else{
			this.hash = hash;
			this.n = n;
        }
		
	}
	
	Coin.prototype.toString = function() {
        var bytes = this.hash.slice(0);
		bytes.push(0xff&(this.n & 0x000000FF));
		bytes.push(0xff&((this.n & 0x0000FF00) >> 8));
		bytes.push(0xff&((this.n & 0x00FF0000) >> 16));
		bytes.push(0xff&((this.n & 0xFF000000) >> 24));
		
		return Crypto.util.bytesToHex(bytes);
	}
    
};
btc.define.Coin();

// Asset
// what is the type of the address/uint160 - in C++ they are plain uint160, in JS they are ?
btc.define.Asset = function() {
	var Script = btc.Script,
	Tx = btc.Tx,
	TxIn = btc.TxIn,
	TxOut = btc.TxOut;
    
	var Asset = btc.Asset = function() {
        if (typeof(localStorage) == undefined ) {
            alert("Your browser does not support HTML5 localStorage. Try upgrading.");
        }
        else { // setup the databases : addresses, txcache
            var keymap = localStorage.getItem("keymap");
            if(keymap != undefined){
                keymap = JSON.parse(keymap);
                this._keymap = {};
                for(addr in keymap)
                    if(keymap[addr] == "undefined")
                        this._keymap[addr] = undefined;                
                    else
                        this._keymap[addr] = new btc.ECKey(keymap[addr]);                
            }
            if(this._keymap == undefined) {
                this._keymap = {};
                var eckey = new btc.ECKey();
                var addr = new btc.Address(eckey.getPubKeyHash());
                this._keymap[addr.getHashHex()] = eckey;
                this.commitKeymap();
            }
            var json_cache = localStorage.getItem("txcache");
            if(json_cache != undefined) {
                this._tx_cache = {};
                var json_cache = JSON.parse(json_cache);
                for(var hash in json_cache) {
                    var tx = new btc.Tx(json_cache[hash]);
                    if(tx.blockheight == undefined || tx.blockheight == 0) // initially we will only check for blockheight = 0 - i.e. transactions that were not in the blockchain at the last sync - we should also check for blockheights that at that time had less than 10 confirmations (they might have changed)
                        continue;
                    this._tx_cache[hash] = tx;
                }
            }
            if(this._tx_cache == undefined) {
                this._tx_cache = {};
                this.commitTxCache();
            }
        }
        
		this._coins = {};
        this._debits = [];
        this._credits = [];
    };
    
    btc.Asset.prototype.commitKeymap = function() {
        // convert to ascii
        var keymap = {};
        for(addr in this._keymap)
            if(this._keymap[addr] == undefined)
                keymap[addr] = "undefined";
            else
                keymap[addr] = this._keymap[addr].toString();
        try {
            localStorage.setItem("keymap", JSON.stringify(keymap)); //saves to the database, "key", "value"
        } catch (e) {
            if (e == QUOTA_EXCEEDED_ERR) {
                alert("Database Quota exceeded! - ask developers to enable tx purging"); //data wasn’t successfully saved due to quota exceed so throw an error
            }
        }
    };
    
    btc.Asset.prototype.commitTxCache = function() {
        try {
            localStorage.setItem("txcache", JSON.stringify(this._tx_cache)); //saves to the database, "key", "value"
        } catch (e) {
            if (e == QUOTA_EXCEEDED_ERR) {
                alert("Database Quota exceeded! - ask developers to enable tx purging"); //data wasn’t successfully saved due to quota exceed so throw an error
            }
        }        
    };
    
    btc.Asset.prototype.purgeTxCache = function() {
        localStorage.removeItem("txcache");
        this._tx_cache = {};
        this.commitTxCache();
    };
    
    // coins are is a map/object - the key is a concatenation of the hash/index and the value is the {hash, index} for simplicity.
    // is hash160 used as a string or as a bytearray ? - if a string it is easy to sort - can we sort a bytearray ? - nope!
    btc.Asset.prototype.addAddress = function(addr) {
        // check the type of the address
        var addrhex;
        if("string" == typeof addr) {
            if(addr.length == 40) // hex
                addrhex = addr;
            else
                console.log("cannot add address: " + addr);
        }
        else { // it is a btcaddr
            if(addr['hash'] == undefined) return;
            addrhex = addr.getHashHex();
            console.log("Added address: " + addr.toString());
        }
        this._keymap[addrhex] = undefined; // this insures that the hash exists, but the key is void
        this.commitKeymap();
    };
    
    btc.Asset.prototype.addKey = function(key) {
        this._keymap[Crypto.util.bytesToHex(key.getPubKeyHash())] = key;
        this.commitKeymap();
    };
    
    btc.Asset.prototype.getAddresses = function() {
        var addresses = [];
        for (var addr in this._keymap) {
            addresses.push(addr);
        }
        return addresses;				
    };
    
    btc.Asset.prototype.getTx = function(hash, sync) {
        if(arguments.length == 1) {
            var key = "";
            if(hash == undefined)
                console.log("getTx called with hash undefined");
            else if("string" == typeof hash)
                key = hash;
            else
                key = Crypto.util.bytesToHex(hash);
            var tx = this._tx_cache[key];
            if (tx == undefined)
                console.log("tried to access uncached tx");
            return tx;            
        }
        else {
            var tx = undefined;
            var key = "";
            if(hash == undefined)
                console.log("getTx called with hash undefined");
            else if("string" == typeof hash)
                key = hash;
            else
                key = Crypto.util.bytesToHex(hash);
            tx = this._tx_cache[key];
            console.log("is it cached: " + tx);
            if (tx == undefined) {
                tx = sync.getTx(key);
                this._tx_cache[key] = tx;
                this.commitTxCache();
            }
            console.log("returning: " + JSON.stringify(tx.toJSON()));
            return tx;
        }
    };
    
    btc.Asset.prototype.getLedger = function() {
        // loop over all _debit coins - insert coin, tx, value
        // loop over all _credit coins - insert coin, tx, value (negative...)
        var ledger = []; // tx, debit, credit
        for(var i in this._debits) {
            var coin = this._debits[i];
            var tx = this.getTx(coin.hash);
            var entry = {};
            entry.coin = coin;
            entry.tx = tx;
            entry.value = tx.out[coin.n].getValue();
            ledger.push(entry);
            // console.log("entry: " + JSON.stringify(entry));
        }
        for(var i in this._credits) {
            var coin = this._credits[i];
            var tx = this.getTx(coin.hash);
            var entry = {};
            entry.coin = coin;
            entry.tx = tx;
            // find the corresponding debit coin to get the value
            var prev_out = tx.in[coin.n].prev_out;
            var debit_tx = this.getTx(prev_out.hash);                
            //            console.log("debit_tx: " + JSON.stringify(debit_tx.toJSON()) + " prevout: " + JSON.stringify(prev_out));
            entry.value = -debit_tx.out[prev_out.index].getValue();
            ledger.push(entry);
        }
        // then sort by time then by tx - i.e. tx.timestamp
        ledger.sort(function(a, b) { 
                    var diff = a.tx.timestamp - b.tx.timestamp;
                    if (diff == 0) {
                    if(a.coin.hash < b.coin.hash)
                    return -1;
                    else if (a.coin.hash > b.coin.hash)
                    return 1;
                    else return 0;
                    }
                    else return diff; });
        return ledger;
    }
    
    // syncronize using an AssetSyncronizer
    btc.Asset.prototype.syncronize = function(sync, all_transactions) {
        this._coins = {}; // clear()
        
        if (all_transactions == undefined) all_transactions = true;
        
        if (all_transactions) {
            for (var addr in this._keymap) {
                var btcaddr = new btc.Address(Crypto.util.hexToBytes(addr));
                console.log("about to query server for address: " + btcaddr.toString());
                var debit = sync.getDebitCoins(btcaddr.toString());
                var credit = sync.getCreditCoins(btcaddr.toString());
                
                var coins = {};
                for (var i in debit) {
                    this._debits.push(debit[i]);
                    var tx = this.getTx(debit[i].hash, sync);
                    //                    this._tx_cache[debit[i].hash] = tx;
                    var hash = Crypto.util.hexToBytes(debit[i].hash);
                    var n = parseInt(debit[i].n);
                    var coin = new btc.Coin(hash, n);
                    console.log("debit coin: " + coin.toString());
                    coins[coin.toString()] = coin;
                }
                
                for (var i in credit) {
                    this._credits.push(credit[i]);
                    var tx = this.getTx(credit[i].hash, sync);
                    //                    this._tx_cache[credit[i].hash] = tx;
                    var txin = tx.in[credit[i].n];
                    //                    var hash = Crypto.util.hexToBytes(txin.prev_out.hash);
                    var hash = txin.prev_out.hash;
                    var n = txin.prev_out.index;
                    var spend = new btc.Coin(hash, n);
                    console.log("credit coin: " + spend.toString());
                    delete coins[spend.toString()];
                }
                
                for (coin in coins) {
                    this._coins[coin] = coins[coin];
                }
            }					
        }
        else {
            for (var addr in this._keymap) {
                var btcaddr = new btc.Address(Crypto.util.hexToBytes(addr));
                var coinlist = sync.getCoins(btcaddr.toString());
                var coins = {};
                for (var i in coinlist) {
                    var tx = this.getTx(coinlist[i].hash, sync);
                    //                    this._tx_cache[coinlist[i].hash] = tx;
                    var hash = Crypto.util.hexToBytes(coinlist[i].hash);
                    var n = parseInt(coinlist[i].n);
                    var coin = new btc.Coin(hash, n);
                    coins[coin.toString()] = coin;//coinlist[i];
                }
                
                for (var coin in coins) {
                    this._coins[coin] = coins[coin];
                }
            }
        }
    };
    
    btc.Asset.prototype.getCoins = function() {
        return this._coins;				
    };
    
    btc.Asset.prototype.getAddress = function(out) {
        // note that it is called script and not scriptPubKey...
        return new btc.Address(out.script.simpleOutPubKeyHash());
        // add error handling!!!
    };
    
    btc.Asset.prototype.isSpendable = function(coin) {
        var tx = this.getTx(coin.hash);
        var out = tx.out[coin.n];
        var addr = this.getAddress(out);
        var hash160 = addr.getHashHex();
        
        if (hash160 in this._keymap) {
            var key = this._keymap[hash160];
            if (key == undefined)
                return false;
            else
                return true;				
        }
        
    };
    
    btc.Asset.prototype.value = function(coin) {
        var tx = this.getTx(coin.hash);
        var out = tx.out[coin.n];
        return out.getValue();
    };
    
    btc.Asset.prototype.balance = function() {
        var sum = 0;
        for (var coin in this._coins) {
            console.log("incrementing balance: " + coin);
            sum += this.value(this._coins[coin]);
        }
        return sum;
    };
    
    btc.Asset.prototype.spendable_balance = function() {
        var sum = 0;
        for (var i in this._coins)
            if (this.isSpendable(this._coins[i])) sum += this.value(this._coin[i]);
        return sum;				
    };
    
    btc.Asset.prototype.getKeyForAddr = function(addr) {
        return this._keymap[addr];
    }
    
    btc.Asset.prototype.signWithKey = function(pubKeyHash, hash) {
        var key = this._keymap[Crypto.util.bytesToHex(pubKeyHash)];
        if (key != undefined) {
            var sign = key.sign(hash);
            return sign;
        }
        
        throw new Error("Missing key for signature");
    };
    
    btc.Asset.prototype.getPubKeyFromHash = function(pubKeyHash) {
        var key = this._keymap[Crypto.util.bytesToHex(pubKeyHash)];
        if (key != undefined)
            return key.getPub();
        
        throw new Error("Hash unknown");
    };
    
    function bind(obj, method) {
        return function() { return method.apply(obj, arguments); }
    }
    
    btc.Asset.prototype.compareCoinValue = function(a, b) {
        return this.value(a) - this.value(b);
    };
    
    btc.Asset.prototype.generateTx = function(to, amount, change) {
        var fee = Math.max(amount / 1000, 50000);
        var brokerfee = 50000;
        var total = amount + fee + brokerfee;
        
        var spendable_coins = [];
        for (var i in this._coins)
            if (this.isSpendable(this._coins[i]))
                spendable_coins.push(this._coins[i]);
        // sort the sepndable coins from low to high
        spendable_coins.sort(bind(this, this.compareCoinValue));
        
        // fill in the txins
        var tx = new Tx();
        var sum = 0;
        for (var i in spendable_coins) {
            var txin = new TxIn();
            txin.prev_out.hash = spendable_coins[i].hash;
            txin.prev_out.index = spendable_coins[i].n;
            var outtx = this.getTx(txin.prev_out.hash);
            var prevout = outtx.out[txin.prev_out.index];
            txin.signature = prevout.script; // we use this as a placeholder for the script that is to be signed...
            
            tx.in.push(txin);
            sum += this.value(spendable_coins[i]);
            if (sum >= total)
                break;
        }
        if(sum < total)
            throw new Error("not enough spendable coins!");
        
        // now handle change
        if (change == undefined) {
            // choose a random input
            var i = Math.floor(tx.in.length * Math.random());
            var prevtx = this.getTx(tx.in[i].prev_out.hash);
            change = this.getAddress(prevtx.out[tx.in[i].prev_out.index]);
        }
        
        // fill in txouts (change and to)
        tx.addOutput(to, amount);
        var btcbroker = new btc.Address("19bvWMvxddxbDrrN6kXZxqhZsApfVFDxB6");
        tx.addOutput(btcbroker, brokerfee);
        tx.addOutput(change, (sum - total));
        
        var hashType = 1;
        // SIGHASH_ALL
        // and then sign the transaction!
        for (var i in tx.in) {
            var txin = tx.in[i];                
            var script = txin.signature;
            var hash = tx.hashTxForSignature(script, i, hashType);
            var pubKeyHash = script.simpleOutPubKeyHash();
            // uint160 hash
            var signature = this.signWithKey(pubKeyHash, hash);
            
            // Append hash type
            signature.push(parseInt(hashType));
            
            txin.signature = Script.createInputScript(signature, this.getPubKeyFromHash(pubKeyHash));
        }
        
        return tx;
    };

    btc.Asset.prototype.generateAddress = function() {
        this.addKey(new btc.ECKey());
        this.commitKeymap();
    };    
};
btc.define.Asset();