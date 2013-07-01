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
    
    ~Point();

    Point& operator=(const Point& point);
    
    CBigNum X() const;
    
    Point& operator+=(const Point& point);
    
    EC_GROUP* ec_group() const;
    
    EC_POINT* ec_point() const;
    
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

class COIN_EXPORT Key {
public:
    /// Generate a void key
    Key();
 
    Key(const SecureData& private_number) {
        EC_KEY_free(_ec_key);
        _ec_key = EC_KEY_new_by_curve_name(NID_secp256k1);
        if (private_number.size() != 32)
            throw key_error("Key::Key : private number must be 32 bytes");
        CBigNum prv;
        BN_bin2bn(&private_number[0], 32, &prv);
        reset(prv);
    }

    Key(const CBigNum& private_number);
    
    Key(const Point& public_point);
    
    /// Check if the key is private or only contains the public key data
    bool isPrivate() const;

    /// Reset to random
    void reset();
    
    /// Reset to new private key
    void reset(const CBigNum& private_number);

    /// Reset to new public key
    void reset(const Point& public_point);
    
    Data serialized_pubkey() const;
    
    Data serialized_full_pubkey() const;

    SecureData serialized_privkey() const;

    /// Get the public point - usefull for ECC math
    const Point public_point() const;

    /// Get the field order - this is the number modulo all operations are taken
    CBigNum order() const;
    
    /// return the private number
    CBigNum number() const;
protected:
    const BIGNUM* private_number() const;
    
private:
    EC_KEY* _ec_key; // Note this can contain both a public and a private key    
};

/// The ExtendedKey class
class COIN_EXPORT ExtendedKey : public Key {
public:
    /// Generate the Master key - if called with no argument, a seed is generated
    ExtendedKey(SecureData seed = SecureData());
    
    ExtendedKey(const Data& private_number, const Data& chain_code) : Key(SecureData(private_number.begin(), private_number.end())), _chain_code(SecureData(chain_code.begin(), chain_code.end())) {
    }
    ExtendedKey(const SecureData& private_number, const SecureData& chain_code) : Key(private_number), _chain_code(chain_code) { }

    ExtendedKey(const CBigNum& private_number, const SecureData& chain_code);
    
    ExtendedKey(const Point& public_point, const SecureData& chain_code);

    /// Retrun the chain code
    const SecureData& chain_code() const;

    /// return the 4 byte hash - similar to a x509 certificate hash
    unsigned int hash() const;
    
    /// fingerprint of the chain and the public key - this is a more precise identification (less collisions) than the 4byte hash.
    uint160 fingerprint() const;
    
    /// derive a new extended key
    ExtendedKey derive(unsigned int i, bool multiply = false) const;

    /// delegate to get a new isolated private key hieracy
    ExtendedKey delegate(unsigned int i) const;

    typedef std::vector<unsigned int> Derivatives;
    Derivatives parse(const std::string tree) const;
    
    ExtendedKey path(const std::string tree, unsigned char& depth, unsigned int& hash, unsigned int& child_number) const;
    
    ExtendedKey path(const std::string tree) const;

    /// get the key (private or public) represented by this extended key
    CKey key() const;
    
    Data serialize(bool serialize_private = false, unsigned int version = 0, unsigned char depth = 0, unsigned int hash = 0, unsigned int child_number = 0) const;
    
    /// The Generator class enables serialization to and from Scripts. It is an easy way to handle Extended Keys.
    class Generator {
    public:
        Generator() {}
        
        Generator(std::vector<unsigned char> script_data);
        
        Generator& operator++();
        
        std::vector<unsigned char> serialize() const;
        
        uint160 fingerprint() const {
            return _fingerprint;
        }

        void fingerprint(uint160 fp) {
            _fingerprint = fp;
        }

        
        const Derivatives& derivatives() const {
            return _derivatives;
        }
        
        //        typedef std::vector<unsigned int> Path;
    private:
        uint160 _fingerprint;
        Derivatives _derivatives;
    };
    
    /// Interface to CKey - create from an extended key and a generator a public/private key
    CKey operator()(const Generator& generator) const;
    
private:
    SecureData _chain_code;
};

/// Serialize a generator to a Script
inline Script& operator<<(Script& script, const ExtendedKey::Generator& generator) {
    return script << generator.serialize();
}
    

/// Enhance the script Evaluator with capabilities to handle OP codes for resolving and resolving and signing using an extended key denoted by its generator
class ExtendedKeyEvaluator : public TransactionEvaluator {
public:
    ExtendedKeyEvaluator(const ExtendedKey& exkey) : TransactionEvaluator(), _exkey(exkey) {}
    
protected:
    virtual boost::tribool eval(opcodetype opcode);
    
private:
    const ExtendedKey& _exkey;
};

#endif // _COIN_EXTENDEDKEY_H_

