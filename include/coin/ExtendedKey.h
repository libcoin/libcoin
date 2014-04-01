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

#ifndef _COIN_EXTENDEDKEY_H_
#define _COIN_EXTENDEDKEY_H_

#include <coin/Export.h>
#include <coin/BigNum.h>
#include <coin/Key.h>
#include <coin/Transaction.h>

#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/hmac.h>
#include <openssl/ripemd.h>
#include <openssl/rand.h>

#include <boost/lexical_cast.hpp>

#include <sstream>

// secp256k1:
// const unsigned int PRIVATE_KEY_SIZE = 279;
// const unsigned int PUBLIC_KEY_SIZE  = 65;
// const unsigned int SIGNATURE_SIZE   = 72;
// curve order, n: FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141

/// An ExtendedKey is a pair (k, c), or (K, c) consisting of a private key, k, or a public key K and a chain code, c
/// An extendedKey allows building a Hierarchical Deterministic Wallet as they support key derivation
/// The algorithm used to derive a new exteneded key is:
/// I = HMAC_SHA512(key = c, data = compressed(K)//i), where // denotes concatenation ,and compressed key is a 33bytes number.
/// I_L = first 256 bits of I, I_R = last 256 bits of I
/// k_i = k + I_L
/// c_i = I_R
/// Note that K_i = K + I_L*G, hence public to public derivation is also possible
/// Methods used:
/// * private to public key transform - i.e. multiplication with the generator G
/// * addition of EC_POINTS

typedef std::vector<unsigned char> Data;
typedef std::vector<unsigned char, secure_allocator<unsigned char> > SecureData;

class COIN_EXPORT Point {
public:
    enum Infinity {};
    
    Point(int curve = NID_secp256k1);
    
    Point(const Point& point);
    
    Point(const EC_POINT* p, int curve = NID_secp256k1);

    Point(Infinity inf, int curve = NID_secp256k1);
    
    Point(const CBigNum& x, const CBigNum& y, int curve = NID_secp256k1);
    
    ~Point();

    Point& operator=(const Point& point);
    
    CBigNum X() const;

    CBigNum Y() const;
    
    Point& operator+=(const Point& point);
    
    EC_GROUP* ec_group() const;
    
    EC_POINT* ec_point() const;
    
    friend std::ostream& operator<<(std::ostream& os, const Point& p) {
        return os << p.X() << p.Y();
    }

    friend std::istream& operator>>(std::istream& is, Point& p) {
        CBigNum x, y;
        is >> x >> y;
        p = Point(x, y);
        return is;
    }

private:
    EC_POINT* _ec_point;
    EC_GROUP* _ec_group;
};

inline Point operator+(const Point& lhs, const Point& rhs)
{
    Point res(lhs);
    res += rhs;
    return res;
}

Point operator*(const CBigNum& m, const Point& Q);

bool operator==(const Point& P, const Point& Q);

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

/// The ExtendedKey class - an extended key is a Key that also holds a chain_code, which enables different schemes for deterministic wallets
class COIN_EXPORT ExtendedKey : public Key {
public:
    /// Generate the Master key - if called with no argument, a seed is generated
    ExtendedKey(SecureData seed = SecureData());
    
    ExtendedKey(const ExtendedKey& key) : Key(key), _chain_code(key._chain_code) {}
    
    ExtendedKey(const Data& serialized_key, const Data& chain_code) : Key(SecureData(serialized_key.begin(), serialized_key.end())), _chain_code(SecureData(chain_code.begin(), chain_code.end())) {
    }
    ExtendedKey(const SecureData& serialized_key, const SecureData& chain_code) : Key(serialized_key), _chain_code(chain_code) { }

    ExtendedKey(const CBigNum& private_number, const SecureData& chain_code);
    
    ExtendedKey(const Point& public_point, const SecureData& chain_code);

    ExtendedKey(const CBigNum& private_number, const Point& public_point, const SecureData& chain_code) : Key(private_number, public_point), _chain_code(chain_code) {}

    // Load a private Key as a public ExtendedKey - public key is public key, private key is the chain_code
    ExtendedKey(const std::string& filename) : Key() {
        Key key(filename);
        reset(key.public_point());
        _chain_code = key.serialized_privkey();
    }
    
    /// Retrun the chain code
    const SecureData& chain_code() const;

    /// return the 4 byte hash - similar to a x509 certificate hash
    unsigned int hash() const;
    
    /// fingerprint of the chain and the public key - this is a more precise identification (less collisions) than the 4byte hash.
    uint160 fingerprint() const;
  
    /// The Derivation class enables derivation of keys, and can check that it is indeed the proper key that is derived by comparing fingerprints.
    /// It enables easy serialization of extended key derivations
    /// The Derivation class is virtual, and a method of dereivation should be implemented in derived classes (see BIP 0032)
    class Derivation {
    public:
        typedef std::vector<unsigned int> Path;
        
        Derivation() {}
        
        /// Copy constructor
        Derivation(const Derivation& generator) : _fingerprint(generator._fingerprint), _path(generator._path){ }
        
        /// Construct from a fingerprint and a path
        Derivation(uint160 fp, Path d = Path()) : _fingerprint(fp), _path(d) {}
        
        /// Construct from a script
        Derivation(std::vector<unsigned char> script_data);
        
        /// Construct from a string of the form: "m/3/5/11"
        Derivation(const std::string tree);
        
        /// simple generation
        Derivation(unsigned int i) :_fingerprint(0) {
            _path.push_back(i);
        }
        
        virtual ~Derivation() {}
        
        Path parse(const std::string tree) const;
        
        Derivation& operator++();
        
        std::vector<unsigned char> serialize() const;
        
        uint160 fingerprint() const {
            return _fingerprint;
        }
        
        void fingerprint(uint160 fp) {
            _fingerprint = fp;
        }
        
        const Path& path() const {
            return _path;
        }

        /// invoke one generator step on an extended key - will generate a new extended key.
        virtual ExtendedKey operator()(const ExtendedKey& parent, unsigned int i) const {
            throw std::runtime_error("only call generator from a derived class!");
        };

    private:
        uint160 _fingerprint;
        Path _path;
    };

    ExtendedKey operator()(const Derivation& generator, unsigned int upto = 0) const;
    
    Data serialize(const Derivation& generator, bool serialize_private = false, unsigned int version = 0) const;
        
private:
    SecureData _chain_code;
};

inline bool operator==(const ExtendedKey::Derivation& lhs, const ExtendedKey::Derivation& rhs) {
    return lhs.fingerprint() == rhs.fingerprint() && lhs.path() == rhs.path();
}

inline bool operator!=(const ExtendedKey::Derivation& lhs, const ExtendedKey::Derivation& rhs) {
    return !operator==(lhs,rhs);
}



namespace BIP0032 {
    /// Derivation of a key according to BIP-0032
    class Derivation : public ExtendedKey::Derivation {
    public:
        Derivation() {}
        
        Derivation(const Derivation& generator) : ExtendedKey::Derivation(generator) { }
        
        Derivation(uint160 fp, Path d = Path()) : ExtendedKey::Derivation(fp, d) {}
        
        Derivation(std::vector<unsigned char> script_data) : ExtendedKey::Derivation(script_data) {}
        
        Derivation(const std::string tree) : ExtendedKey::Derivation(tree) {}

        Derivation(unsigned int i) : ExtendedKey::Derivation(i) {}
        
        virtual ExtendedKey operator()(const ExtendedKey& parent, unsigned int i) const;
    };

    /// Delegation of a key according to BIP-0032
    class Delegation : public ExtendedKey::Derivation {
    public:
        Delegation() {}
        
        Delegation(const Derivation& generator) : ExtendedKey::Derivation(generator) { }
        
        Delegation(uint160 fp, Path d = Path()) : ExtendedKey::Derivation(fp, d) {}
        
        Delegation(std::vector<unsigned char> script_data) : ExtendedKey::Derivation(script_data) {}
        
        Delegation(const std::string tree) : ExtendedKey::Derivation(tree) {}
        
        Delegation(unsigned int i) : ExtendedKey::Derivation(i) {}

        virtual ExtendedKey operator()(const ExtendedKey& parent, unsigned int i) const;
    };
}

#endif // _COIN_EXTENDEDKEY_H_

