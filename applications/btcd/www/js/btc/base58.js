btc.define.Base58 = function () {
    var B58 = btc.Base58 = {};
    
    btc.Base58.alphabet = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
    btc.Base58.base = BigInteger.valueOf(58);
    
    /**
     * Convert a byte array to a base58-encoded string.
     *
     * Written by Mike Hearn for BitcoinJ.
     *   Copyright (c) 2011 Google Inc.
     *
     * Ported to JavaScript by Stefan Thomas.
     */
    btc.Base58.encode = function (input) {
        var bi = BigInteger.fromByteArrayUnsigned(input);
        var chars = [];
        
        while (bi.compareTo(BigInteger.valueOf(58)) >= 0) {
            var mod = bi.mod(BigInteger.valueOf(58));
            chars.unshift(btc.Base58.alphabet[mod.intValue()]);
            bi = bi.subtract(mod).divide(BigInteger.valueOf(58));
        }
        chars.unshift(B58.alphabet[bi.intValue()]);
        
        // Convert leading zeros too.
        for (var i = 0; i < input.length; i++) {
            if (input[i] == 0x00) {
                chars.unshift(B58.alphabet[0]);
            }
            else break;
        }
        
        s = chars.join('');
        return s;
    };
    
    /**
     * Convert a base58-encoded string to a byte array.
     *
     * Written by Mike Hearn for BitcoinJ.
     *   Copyright (c) 2011 Google Inc.
     *
     * Ported to JavaScript by Stefan Thomas.
     */
    btc.Base58.decode = function (input) {
        bi = BigInteger.valueOf(0);
        var leadingZerosNum = 0;
        for (var i = input.length - 1; i >= 0; i--) {
            var alphaIndex = B58.alphabet.indexOf(input[i]);
            bi = bi.add(BigInteger.valueOf(alphaIndex)
                        .multiply(BigInteger.valueOf(58).pow(input.length - 1 -i)));
            
            // This counts leading zero bytes
            if (input[i] == "1") leadingZerosNum++;
            else leadingZerosNum = 0;
        }
        var bytes = bi.toByteArrayUnsigned();
        
        // Add leading zeros
        while (leadingZerosNum-- > 0) bytes.unshift(0);
        
        return bytes;
    };
    
};
btc.define.Base58();
