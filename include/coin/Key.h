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

#ifndef _COIN_KEY_H_
#define _COIN_KEY_H_

#include <coin/Export.h>
#include <coin/BigNum.h>
#include <coin/Point.h>
#include <coin/Script.h>

#include <openssl/hmac.h>
#include <openssl/ripemd.h>
#include <openssl/rand.h>

typedef std::vector<unsigned char> Data;
typedef std::vector<unsigned char, secure_allocator<unsigned char> > SecureData;

int extern ECDSA_SIG_recover_key_GFp(EC_KEY *eckey, ECDSA_SIG *ecsig, const unsigned char *msg, int msglen, int recid, int check);

/// A Key can be in 3 states: pure public key, pure private key (with public key derived from the private key) and separate private and public keys. The latter is hidden as a protected constructor for the Key class, but exposed for the ExtendedKey

class COIN_EXPORT Key {
public:
    class PassphraseFunctor {
    public:
        virtual SecureString operator()(bool verify = false) = 0;
    };
    
    class GetPassFunctor : public PassphraseFunctor {
    public:
        GetPassFunctor(std::string prompt = "Enter passphrase: ") : _prompt(prompt) {}
        
        virtual SecureString operator()(bool verify = false);
    private:
        std::string _prompt;
    };
    
public:
    /// Generate a void key
    Key();
    
    Key(const Key& key);
    
    Key(const SecureData& serialized_key);
    
    Key(const CBigNum& private_number);
    
    Key(const Point& public_point);
    
    Key(const CBigNum& private_number, const Point& public_point);
    
    Key(const std::string& filename);
    Key(const std::string& filename, PassphraseFunctor& pf);
    
    /// extract the public key from a signature and the hash - will throw if signature is invalid.
    Key(uint256 hash, const Data& signature);
    
    void file(const std::string& filename, bool overwrite = false);
    
    /// Check if the key is private or only contains the public key data
    bool isPrivate() const;
    
    /// Keys can contain a public point P which is not consistent with the private key p : P != p*G
    /// This is a feature used for saving public ExtendedKeys, as well as for split key pairs
    bool isConsistent() const;
    
    /// Reset to random
    void reset();
    
    /// Reset to new private key
    void reset(const CBigNum& private_number);
    
    /// Reset to new public key
    void reset(const Point& public_point);
    
    void reset(const CBigNum& private_number, const Point& public_point);
    
    void reset(EC_KEY* eckey);
    
    Data serialized_pubkey() const;
    
    Data serialized_full_pubkey() const;
    
    PubKeyHash serialized_pubkeyhash(bool compact = true) const {
        return compact ? toPubKeyHash(serialized_pubkey()) : toPubKeyHash(serialized_full_pubkey());
    }
    
    SecureData serialized_privkey() const;
    
    /// Get the public point - usefull for ECC math
    const Point public_point() const;
    
    /// Get the field order - this is the number modulo all operations are taken
    CBigNum order() const;
    
    /// return the private number
    CBigNum number() const;
    
    /// Sign a hash using ECDSA
    Data sign(uint256 hash) const;
    
    /// Compact signature including both the signature and the pubkey - if the signature is given, sign is not called
    Data sign_compact(uint256 hash, Data signature = Data()) const;
    
    /// Verify an ECDSA signature on a hash
    bool verify(uint256 hash, const Data& signature) const;
    
    /// included for debugging purposes
    const EC_KEY* eckey() const { return _ec_key; }
    
protected:
    const BIGNUM* private_number() const;
    
private:
    EC_KEY* _ec_key; // Note this can contain both a public and a private key
};

#endif //#ifndef _COIN_KEY_H_
