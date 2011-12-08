btc.define.Script = function() {
	var Opcode = btc.Opcode;
    
	// Make opcodes available as pseudo-constants
	for (var i in Opcode.map) {
		eval("var " + i + " = " + Opcode.map[i] + ";");
		
	}
    
	var Script = btc.Script = function(data) {
        this.chunks = [];
        
		if (!data) {
			this.buffer = [];
		} else if ("string" == typeof data) {
            // the data is a string - this could be one of two - either a base64 string or a space separated list of opcodes and hex numbers
            var tokens = data.split(" ");
            if(tokens.length == 1) // this is wrong - a pubkey sign scipt will also return 1...
                this.buffer = Crypto.util.base64ToBytes(data);
            else // loop over tokens
                {
                this.buffer = [];
                for(var i in tokens)
                    {
                    var token = tokens[i];
                    if(Opcode.map[token] != undefined)
                        this.writeOp(Opcode.map[token])
                        else
                            this.writeBytes(Crypto.util.hexToBytes(token));
                    }
                }
			
		} else if (btc.Util.isArray(data)) {
			this.buffer = data;
			
		} else if (data instanceof Script) {
			this.buffer = data.buffer;
			
		} else {
			throw new Error("Invalid script");
			
		}
        if (this.chunks.length == 0)
            this.parse();
	};
    
    Script.prototype.toJSON = function() {
        var json = [];
        // loop over chunks...
        for(var i in this.chunks)
            {
            if('number' == typeof this.chunks[i]) // opcode
                json.push(Opcode.reverseMap[this.chunks[i]]);
            else
                json.push(Crypto.util.bytesToHex(this.chunks[i]));
            }
        return json.join(" ");;
    };
    
    
	Script.prototype.parse = function() {
		var self = this;
        
		this.chunks = [];
        
		// Cursor
		var i = 0;
        
		// Read n bytes and store result as a chunk
		function readChunk(n) {
			self.chunks.push(self.buffer.slice(i, i + n));
			i += n;
			
		};
        
		while (i < this.buffer.length) {
			var opcode = this.buffer[i++];
			if (opcode >= 0xF0) {
				// Two byte opcode
				opcode = (opcode << 8) | this.buffer[i++];
				
			}
            
			var len;
			if (opcode > 0 && opcode < OP_PUSHDATA1) {
				// Read some bytes of data, opcode value is the length of data
				readChunk(opcode);
				
			} else if (opcode == OP_PUSHDATA1) {
				len = this.buffer[i++];
				readChunk(len);
				
			} else if (opcode == OP_PUSHDATA2) {
				len = (this.buffer[i++] << 8) | this.buffer[i++];
				readChunk(len);
				
			} else if (opcode == OP_PUSHDATA4) {
				len = (this.buffer[i++] << 24) | 
				(this.buffer[i++] << 16) | 
				(this.buffer[i++] << 8) | 
				this.buffer[i++];
				readChunk(len);
				
			} else {
				this.chunks.push(opcode);
				
			}
			
		}
		
	};
    
	Script.prototype.getOutType = function() {
        if (this.chunks.length == 5 && 
            this.chunks[0] == OP_DUP && 
            this.chunks[1] == OP_HASH160 && 
            this.chunks[3] == OP_EQUALVERIFY && 
            this.chunks[4] == OP_CHECKSIG) {
            
            // Transfer to Bitcoin address
            return 'Address';
            
        } else if (this.chunks.length == 2 && 
                   this.chunks[1] == OP_CHECKSIG) {
            
            // Transfer to IP address
            return 'Pubkey';
            
        } else {
            return 'Strange';
            
        }
        
	};
    
	Script.prototype.simpleOutPubKeyHash = function() {
        switch (this.getOutType()) {
            case 'Address':
                return this.chunks[2];
            case 'Pubkey':
                return btc.Util.sha256ripe160(this.chunks[0]);
            default:
                throw new Error("Encountered non-standard scriptPubKey");
                
        }
        
	};
    
	Script.prototype.getInType = function() {
        if (this.chunks.length == 1) {
            // Direct IP to IP transactions only have the public key in their scriptSig.
            return 'Pubkey';
            
        } else if (this.chunks.length == 2 && 
                   btc.Util.isArray(this.chunks[0]) && 
                   btc.Util.isArray(this.chunks[1])) {
            return 'Address';
            
        } else {
            console.log(this.chunks);
            throw new Error("Encountered non-standard scriptSig");
            
        }
        
	};
    
	Script.prototype.simpleInPubKey = function() {
        switch (this.getInType()) {
            case 'Address':
                return this.chunks[1];
            case 'Pubkey':
                return this.chunks[0];
            default:
                throw new Error("Encountered non-standard scriptSig");
                
        }
        
	};
    
	Script.prototype.simpleInPubKeyHash = function() {
        return btc.Util.sha256ripe160(this.simpleInPubKey());
        
	};
    
	Script.prototype.writeOp = function(opcode) {
        this.buffer.push(opcode);
        this.chunks.push(opcode);
        
	};
    
	Script.prototype.writeBytes = function(data) {
        if (data.length < OP_PUSHDATA1) {
            this.buffer.push(data.length);
            
        } else if (data.length <= 0xff) {
            this.buffer.push(OP_PUSHDATA1);
            this.buffer.push(data.length);
            
        } else if (data.length <= 0xffff) {
            this.buffer.push(OP_PUSHDATA2);
            this.buffer.push(data.length & 0xff);
            this.buffer.push((data.length >>> 8) & 0xff);
            
        } else {
            this.buffer.push(OP_PUSHDATA4);
            this.buffer.push(data.length & 0xff);
            this.buffer.push((data.length >>> 8) & 0xff);
            this.buffer.push((data.length >>> 16) & 0xff);
            this.buffer.push((data.length >>> 24) & 0xff);
            
        }
        this.buffer = this.buffer.concat(data);
        this.chunks.push(data);
        
	};
    
	Script.createOutputScript = function(address) {
        var script = new Script();
        script.writeOp(OP_DUP);
        script.writeOp(OP_HASH160);
        script.writeBytes(address.hash);
        script.writeOp(OP_EQUALVERIFY);
        script.writeOp(OP_CHECKSIG);
        return script;
        
	};
    
	Script.createInputScript = function(signature, pubKey) {
        var script = new Script();
        script.writeBytes(signature);
        script.writeBytes(pubKey);
        return script;
        
	};
    
	Script.prototype.clone = function() {
        return new Script(this.buffer);
        
	};
	
};
btc.define.Script();