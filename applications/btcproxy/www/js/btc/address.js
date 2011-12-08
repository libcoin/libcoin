btc.define.Address = function() {
    
    btc.Address = function (bytes) {
        if ("string" == typeof bytes) {
            bytes = btc.Address.decodeString(bytes);
        }
        this.hash = bytes;
        
        this.version = 0x00;
    };
    
    btc.Address.prototype.toString = function () {
        // Get a copy of the hash
        var hash = this.hash.slice(0);
        // Version
        hash.unshift(this.version);
        
        var checksum = Crypto.SHA256(Crypto.SHA256(hash, {asBytes: true}), {asBytes: true});
        
        var bytes = hash.concat(checksum.slice(0,4));
        
        return btc.Base58.encode(bytes);
    };
    
    btc.Address.prototype.getHashBase64 = function () {
        return Crypto.util.bytesToBase64(this.hash);
    };
    
    btc.Address.prototype.getHashHex = function () {
        return Crypto.util.bytesToHex(this.hash);
    };
    
    btc.Address.decodeString = function (string) {
        var bytes = btc.Base58.decode(string);
        
        var hash = bytes.slice(0, 21);
        
        var checksum = Crypto.SHA256(Crypto.SHA256(hash, {asBytes: true}), {asBytes: true});
        
        if (checksum[0] != bytes[21] ||
            checksum[1] != bytes[22] ||
            checksum[2] != bytes[23] ||
            checksum[3] != bytes[24]) {
            throw "Checksum validation failed!";
        }
        
        var version = hash.shift();
        
        if (version != 0) {
            throw "Version "+version+" not supported!";
        }
        
        return hash;
    };
    
};
btc.define.Address();