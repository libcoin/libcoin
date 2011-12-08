btc.define.Tx = function () {
    var Script = btc.Script;
    
    var Tx = btc.Tx = function (doc) {
        this.version = 1;
        this.lock_time = 0;
        this.in = [];
        this.out = [];
        this.timestamp = null;
        this.blockheight = null;
        
        if (doc) {
            if (doc.hash) this.hash = Crypto.util.hexToBytes(doc.hash);
            if (doc.version) this.version = doc.version;
            if (doc.ver) this.version = doc.ver;
            if (doc.lock_time) this.lock_time = doc.lock_time;
            if (doc.in && doc.in.length) {
                for (var i = 0; i < doc.in.length; i++) {
                    this.addInput(new TxIn(doc.in[i]));
                }
            }
            if (doc.out && doc.out.length) {
                for (var i = 0; i < doc.out.length; i++) {
                    this.addOutput(new TxOut(doc.out[i]));
                }
            }
            if (doc.timestamp) this.timestamp = parseInt(doc.timestamp);
            if (doc.blockheight) this.blockheight = doc.blockheight;
        }
    };
    
    Tx.objectify = function (txs) {
        var objs = [];
        for (var i = 0; i < txs.length; i++) {
            objs.push(new Tx(txs[i]));
        }
        return objs;
    };
    
    Tx.prototype.toJSON = function() {
        var json = {};
        if(this.timestamp != null) json.timestamp = this.timestamp.toString();
        json.hash = Crypto.util.bytesToHex(this.getHash().reverse());
        json.ver = this.version;
        json.vin_sz = this.in.length;
        json.vout_sz = this.out.length;
        json.lock_time = this.lock_time;
        json.size = this.getSize();
        json.in = [];
        for(var i in this.in)
            json.in.push(this.in[i].toJSON());
        json.out = [];
        for(var i in this.out)
            json.out.push(this.out[i].toJSON());
        return json;
    };
    
    Tx.prototype.addInput = function (tx, outIndex) {
        if (arguments[0] instanceof TxIn) {
            this.in.push(arguments[0]);
        }
        else {
            this.in.push(new TxIn({
                                  prev_out: {
                                  hash: Crypto.util.hexToBytes(tx.hash),
                                  index: outIndex
                                  },
                                  scriptSig: new btc.Script(),
                                  sequence: 4294967295
                                  }));
        }
    };
    
    Tx.prototype.addOutput = function (address, value) {
        if (arguments[0] instanceof TxOut) {
            this.out.push(arguments[0]);
        }
        else {
            if (value instanceof BigInteger) {
                value = value.toByteArrayUnsigned().reverse();
                while (value.length < 8) value.push(0);
            }
            else if (btc.Util.isArray(value)) {
                // Nothing to do
            }
            
            this.out.push(new TxOut({
                                    value: value,
                                    scriptPubKey: Script.createOutputScript(address)
                                    }));
        }
    };
    
    Tx.prototype.serialize = function () {
        var buffer = [];
        buffer = buffer.concat(Crypto.util.wordsToBytes([parseInt(this.version)]).reverse());
        buffer = buffer.concat(btc.Util.numToVarInt(this.in.length));
        for (var i = 0; i < this.in.length; i++) {
            var txin = this.in[i];
            buffer = buffer.concat(txin.prev_out.hash.slice(0).reverse());
            buffer = buffer.concat(Crypto.util.wordsToBytes([parseInt(txin.prev_out.index)]).reverse());
            var scriptBytes = txin.signature.buffer;
            buffer = buffer.concat(btc.Util.numToVarInt(scriptBytes.length));
            buffer = buffer.concat(scriptBytes);
            buffer = buffer.concat(Crypto.util.wordsToBytes([parseInt(txin.sequence)]).reverse());
        }
        buffer = buffer.concat(btc.Util.numToVarInt(this.out.length));
        for (var i = 0; i < this.out.length; i++) {
            var txout = this.out[i];
            buffer = buffer.concat(txout.value.slice(0).reverse());
            var scriptBytes = txout.script.buffer;
            buffer = buffer.concat(btc.Util.numToVarInt(scriptBytes.length));
            buffer = buffer.concat(scriptBytes);
        }
        buffer = buffer.concat(Crypto.util.wordsToBytes([parseInt(this.lock_time)]).reverse());
        
        return buffer;
    };
    
    var OP_CODESEPARATOR = 171;
    
    var SIGHASH_ALL = 1;
    var SIGHASH_NONE = 2;
    var SIGHASH_SINGLE = 3;
    var SIGHASH_ANYONECANPAY = 80;
    
    Tx.prototype.hashTxForSignature = function (connectedScript, inIndex, hashType) {
        var txTmp = this.clone();
        console.log("connectedScript: " + JSON.stringify(connectedScript.toJSON()));
        console.log("txTmp: " + JSON.stringify(txTmp.toJSON()));
        
        //        for(var i in txTmp.in)
        //  txTmp.in[i].prev_out.hash.reverse();
        
        // In case concatenating two scripts ends up with two codeseparators,
        // or an extra one at the end, this prevents all those possible incompatibilities.
        /*scriptCode = scriptCode.filter(function (val) {
         return val !== OP_CODESEPARATOR;
         });*/
        
        // Blank out other inputs' signatures
        for (var i = 0; i < txTmp.in.length; i++) {
            txTmp.in[i].signature = new Script();
        }
        
        txTmp.in[inIndex].signature = connectedScript;
        
        // Blank out some of the outputs
        if ((hashType & 0x1f) == SIGHASH_NONE) {
            txTmp.out = [];
            
            // Let the others update at will
            for (var i = 0; i < txTmp.in.length; i++)
                if (i != inIndex)
                    txTmp.in[i].sequence = 0;
        }
        else if ((hashType & 0x1f) == SIGHASH_SINGLE) {
            // TODO: Implement
        }
        
        // Blank out other inputs completely, not recommended for open transactions
        if (hashType & SIGHASH_ANYONECANPAY) {
            txTmp.in = [txTmp.in[inIndex]];
        }
        
        console.log(txTmp);
        var buffer = txTmp.serialize();
        
        buffer = buffer.concat(Crypto.util.wordsToBytes([parseInt(hashType)]).reverse());
        //console.log("txinhashbuffer: " + txTmp.in[0].prev_out.hash);
        console.log("buffer: " + buffer.length + " --- " + buffer);
        //        console.log("signtx: "+Crypto.util.bytesToHex(buffer));
        var hash1 = Crypto.SHA256(buffer, {asBytes: true});
        
        //        console.log("sha256_1: ", Crypto.util.bytesToHex(hash1));
        
        return Crypto.SHA256(hash1, {asBytes: true});
    };
    
    Tx.prototype.getSize = function () {
        var buffer = this.serialize();
        return buffer.length;
    }
    
    Tx.prototype.getHash = function () {
        var buffer = this.serialize();
        //console.log("serializationbuffer: " + buffer);
        return Crypto.SHA256(Crypto.SHA256(buffer, {asBytes: true}), {asBytes: true});
    };
    
    Tx.prototype.clone = function () {
        var newTx = new Tx();
        newTx.version = this.version;
        newTx.lock_time = this.lock_time;
        for (var i = 0; i < this.in.length; i++) {
            var txin = this.in[i].clone();
            newTx.addInput(txin);
        }
        for (var i = 0; i < this.out.length; i++) {
            var txout = this.out[i].clone();
            newTx.addOutput(txout);
        }
        return newTx;
    };
    
    /**
     * Analyze how this transaction affects a wallet.
     */
    Tx.prototype.analyze = function (wallet) {
        if (!(wallet instanceof btc.Wallet)) return null;
        
        var allFromMe = true,
        allToMe = true,
        firstRecvHash = null,
        firstMeRecvHash = null,
        firstSendHash = null;
        
        for (var i = this.out.length-1; i >= 0; i--) {
            var txout = this.out[i];
            var hash = txout.script.simpleOutPubKeyHash();
            if (!wallet.hasHash(hash)) {
                allToMe = false;
            } else {
                firstMeRecvHash = hash;
            }
            firstRecvHash = hash;
        }
        for (var i = this.in.length-1; i >= 0; i--) {
            var txin = this.in[i];
            firstSendHash = txin.signature.simpleInPubKeyHash();
            if (!wallet.hasHash(firstSendHash)) {
                allFromMe = false;
                break;
            }
        }
        
        var impact = this.calcImpact(wallet);
        
        var analysis = {};
        
        analysis.impact = impact;
        
        if (impact.sign > 0 && impact.value.compareTo(BigInteger.ZERO) > 0) {
            analysis.type = 'recv';
            analysis.addr = new btc.Address(firstMeRecvHash);
        } else if (allFromMe && allToMe) {
            analysis.type = 'self';
        } else if (allFromMe) {
            analysis.type = 'sent';
            analysis.addr = new btc.Address(firstRecvHash);
        } else  {
            analysis.type = "other";
        }
        
        return analysis;
    };
    
    Tx.prototype.getDescription = function (wallet) {
        var analysis = this.analyze(wallet);
        
        if (!analysis) return "";
        
        switch (analysis.type) {
            case 'recv':
                return "Received with "+analysis.addr;
                break;
                
            case 'sent':
                return "Payment to "+analysis.addr;
                break;
                
            case 'self':
                return "Payment to yourself";
                break;
                
            case 'other':
            default:
                return "";
        }
    };
    
    Tx.prototype.getTotalValue = function () {
        var totalValue = BigInteger.ZERO;
        for (var j = 0; j < this.out.length; j++) {
            var txout = this.out[j];
            totalValue = totalValue.add(btc.Util.valueToBigInt(txout.value));
        }
        return totalValue;
    };
    
    /**
     * Calculates the impact a transaction has on this wallet.
     *
     * Based on the its public keys, the wallet will calculate the
     * credit or debit of this transaction.
     *
     * It will return an object with two properties:
     *  - sign: 1 or -1 depending on sign of the calculated impact.
     *  - value: amount of calculated impact
     *
     * @returns Object Impact on wallet
     */
    Tx.prototype.calcImpact = function (wallet) {
        if (!(wallet instanceof btc.Wallet)) return BigInteger.ZERO;
        
        // Calculate credit to us from all outputs
        var valueOut = BigInteger.ZERO;
        for (var j = 0; j < this.out.length; j++) {
            var txout = this.out[j];
            var hash = Crypto.util.bytesToHex(txout.script.simpleOutPubKeyHash());
            if (wallet.hasHash(hash)) {
                valueOut = valueOut.add(btc.Util.valueToBigInt(txout.value));
            }
        }
        
        // Calculate debit to us from all ins
        var valueIn = BigInteger.ZERO;
        for (var j = 0; j < this.in.length; j++) {
            var txin = this.in[j];
            var hash = Crypto.util.bytesToHex(txin.signature.simpleInPubKeyHash());
            if (wallet.hasHash(hash)) {
                var fromTx = wallet.txIndex[txin.prev_out.hash];
                if (fromTx) {
                    valueIn = valueIn.add(btc.Util.valueToBigInt(fromTx.out[txin.prev_out.index].value));
                }
            }
        }
        if (valueOut.compareTo(valueIn) >= 0) {
            return {
            sign: 1,
            value: valueOut.subtract(valueIn)
            };
        } else {
            return {
            sign: -1,
            value: valueIn.subtract(valueOut)
            };
        }
    };
    
    var TxIn = btc.TxIn = function (data) {
        if(data) {  
            if(data.prev_out) {
                this.prev_out = {};
                this.prev_out.hash = Crypto.util.hexToBytes(data.prev_out.hash);
                this.prev_out.index = data.prev_out.n;
            }
            else
                this.prev_out = {};
            
            if (data.scriptSig instanceof Script) {
                this.signature = data.scriptSig;
            }
            else {
                this.signature = new Script(data.scriptSig);
            }
            if(data.sequence) this.sequence = data.sequence; else this.sequence = 0xFFFFFFFF; // make it final
        }
        else {
            this.prev_out = {};
            this.sequence = 0xFFFFFFFF;
        }
    };
    
    TxIn.prototype.toJSON = function() {
        var json = {};
        json.prev_out = {};
        json.prev_out.hash = Crypto.util.bytesToHex(this.prev_out.hash);
        json.prev_out.n = this.prev_out.index;
        
        json.scriptSig = this.signature.toJSON();
        return json;
    };
    
    TxIn.prototype.clone = function () {
        var newTxin = new TxIn({
                               prev_out: {
                               hash: Crypto.util.bytesToHex(this.prev_out.hash),
                               n: this.prev_out.index
                               },
                               scriptSig: this.signature.clone(),
                               sequence: this.sequence
                               });
        return newTxin;
    };
    
    var TxOut = btc.TxOut = function (data) {
        if (data.scriptPubKey instanceof Script) {
            this.script = data.scriptPubKey;
        }
        else {
            this.script = new Script(data.scriptPubKey);
        }
        
        if (btc.Util.isArray(data.value)) {
            //console.log("TxOut value is Array");
            this.value = data.value;
        }
        else if ("string" == typeof data.value) {
            var valueHex = (new BigInteger(data.value, 10)).toString(16);
            while (valueHex.length < 16) valueHex = "0" + valueHex;
            this.value = Crypto.util.hexToBytes(valueHex);
            //            this.value.reverse();
            //console.log("TxOut value is String: " + data.value + " " + valueHex + " " + this.value);
        }
        else if ("number" == typeof data.value) {
            //console.log("TxOut value is Number");
            var valueHex = (new BigInteger(data.value.toString(), 10)).toString(16);
            while (valueHex.length < 16) valueHex = "0" + valueHex;
            this.value = Crypto.util.hexToBytes(valueHex);
            //this.value.reverse();
        }
    };
    
    TxOut.prototype.toJSON = function() {
        var json = {};
        //        console.log("toJSON: " + this.value + " " + this.getValue());
        json.value = (0.00000001*this.getValue()).toFixed(8);
        json.scriptPubKey = this.script.toJSON();
        return json;
    };
    
    TxOut.prototype.clone = function () {
        var newTxout = new TxOut({
                                 scriptPubKey: this.script.clone(),
                                 value: this.value.slice(0)
                                 });
        return newTxout;
    };
    
    TxOut.prototype.getValue = function() {
        var valueHex = Crypto.util.bytesToHex(this.value);
        var value = new BigInteger(valueHex, 16);
        return parseInt(value.toRadix(10));        
    }
};
btc.define.Tx();