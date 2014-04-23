/* -*-c++-*- libcoin - Copyright (C) 2012-14 Michael Gronager
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


#include <coin/ExtendedKey.h>
#include <coin/Script.h>

#include <vector>

#include <openssl/evp.h>
#include <openssl/ec.h>

using namespace std;

class COIN_EXPORT HMAC {
public:
    enum Digest {
        MD5,
        SHA1,
        SHA256,
        SHA512,
        RIPEMD160
    };
    
    HMAC(Digest digest = SHA512) : _digest(digest) {
    }
    
    template <typename DatA, typename DatB>
    SecureData operator()(const DatA& key, const DatB& message) const {
        const EVP_MD* md = NULL;
        switch (_digest) {
            case MD5 :
                md = EVP_md5();
                break;
            case SHA1 :
                md = EVP_sha1();
                break;
            case SHA256 :
                md = EVP_sha256();
                break;
            case SHA512 :
                md = EVP_sha512();
                break;
            case RIPEMD160 :
                md = EVP_ripemd160();
                break;
            default : md = NULL;
                break;
        }
        SecureData mac(EVP_MAX_MD_SIZE);
        unsigned int size;
        ::HMAC(md, &key[0], key.size(), &message[0], message.size(), &mac[0], &size);
        mac.resize(size);
        return mac;
    }
private:
    Digest _digest;
};

ExtendedKey::ExtendedKey(SecureData seed) : Key() {
    if (seed.size() == 0) {
        seed.resize(256/8);
        RAND_bytes(&seed[0], 256/8);
    }
    
    class HMAC hmac(HMAC::SHA512);
    const char* bitcoin_seed = "Bitcoin seed";
    SecureData key(bitcoin_seed, &bitcoin_seed[12]);
    SecureData I = hmac(key, seed);
    
    SecureData I_L(I.begin(), I.begin() + 256/8);
    SecureData I_R(I.begin() + 256/8, I.end());
    
    BIGNUM* bn = BN_bin2bn(&I_L[0], 32, NULL);
    CBigNum k(bn);
    CBigNum n = order();
    
    reset(k % n);
    _chain_code = I_R;
}

ExtendedKey::ExtendedKey(const CBigNum& private_number, const SecureData& chain_code) : Key(private_number), _chain_code(chain_code) {
}

ExtendedKey::ExtendedKey(const Point& public_point, const SecureData& chain_code) : Key(public_point), _chain_code(chain_code) {
}

const SecureData& ExtendedKey::chain_code() const {
    return _chain_code;
}

unsigned int ExtendedKey::hash() const {
    Data pubkey = serialized_pubkey();
    uint256 hash1;
    SHA256(&pubkey[0], pubkey.size(), (unsigned char*)&hash1);
    uint160 md;
    RIPEMD160((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&md);
    unsigned char *p = (unsigned char*)&md;
    unsigned int fp = 0;
    fp |= p[0] << 24;
    fp |= p[1] << 16;
    fp |= p[2] << 8;
    fp |= p[3];
    return fp;
}

/// fingerprint of the chain and the public key - this is a more precise identification (less collisions) than the 4byte fingerprint.
uint160 ExtendedKey::fingerprint() const {
    SecureData data(_chain_code);
    Data pub = serialized_pubkey();
    data.insert(data.end(), pub.begin(), pub.end());
    uint256 hash;
    SHA256(&data[0], data.size(), (unsigned char*)&hash);
    uint160 md;
    RIPEMD160((unsigned char*)&hash, sizeof(hash), (unsigned char*)&md);
    return md;
}

ExtendedKey ExtendedKey::operator()(const ExtendedKey::Derivation& generator, unsigned int skip) const {
    unsigned int upto = generator.path().size() - skip;
    
    // check that the generator fingerprint matches ours:
    if (generator.fingerprint() != uint160(0) && generator.fingerprint() != fingerprint())
        throw runtime_error("Request for derivative of another extended key!");
    
    // now run through the derivatives:
    ExtendedKey ek(*this);
    size_t depth = 0;
    for (Derivation::Path::const_iterator d = generator.path().begin(); d != generator.path().end(); ++d) {
        unsigned int child_number = *d;
        if (depth++ == upto)
            break;
        ek = generator(ek, child_number); // note ! the generator uses multiply instead of add!
    }
    
    return ek;
}

namespace BIP0032 {
    ExtendedKey Derivation::operator()(const ExtendedKey& parent, unsigned int i) const {
        if (i & 0x80000000)  { // BIP0032 compatability
            Delegation delegate(dynamic_cast<const Derivation&>(*this));
            return delegate(parent, i & 0x7fffffff);
        }
        // K concat i
        Data data = parent.serialized_pubkey();
        unsigned char* pi = (unsigned char*)&i;
        data.push_back(pi[3]);
        data.push_back(pi[2]);
        data.push_back(pi[1]);
        data.push_back(pi[0]);
        
        class HMAC hmac(HMAC::SHA512);
        SecureData I = hmac(parent.chain_code(), data);
        
        SecureData I_L(I.begin(), I.begin() + 256/8);
        SecureData I_R(I.begin() + 256/8, I.end());
        
        if (parent.isPrivate()) {
            CBigNum k(parent.number());
            BIGNUM* bn = BN_bin2bn(&I_L[0], 32, NULL);
            CBigNum k_i = (k + bn) % parent.order();
            return ExtendedKey(k_i, I_R);
        }
        
        // calculate I_L*G - analogue to create a key with the secret I_L and get its EC_POINT pubkey, then add it to this EC_POINT pubkey;
        BIGNUM* bn = BN_bin2bn(&I_L[0], 32, NULL);
        Key d((CBigNum(bn)));
        Point sum = parent.public_point() + d.public_point();
        return ExtendedKey(sum, I_R);
    }

    ExtendedKey Delegation::operator()(const ExtendedKey& parent, unsigned int i) const {
        i |= 0x80000000;
        // 0x0000000 concat k concat i
        SecureData data = parent.serialized_privkey();
        data.insert(data.begin(), 1, 0x00);
        unsigned char* pi = (unsigned char*)&i;
        data.push_back(pi[3]);
        data.push_back(pi[2]);
        data.push_back(pi[1]);
        data.push_back(pi[0]);
        
        class HMAC hmac(HMAC::SHA512);
        SecureData I = hmac(parent.chain_code(), data);
        
        SecureData I_L(I.begin(), I.begin() + 256/8);
        SecureData I_R(I.begin() + 256/8, I.end());
        
        if (parent.isPrivate()) {
            CBigNum k(parent.number());
            BIGNUM* bn = BN_bin2bn(&I_L[0], 32, NULL);
            CBigNum k_i = k + CBigNum(bn);
            CBigNum n = parent.order();
            return ExtendedKey(k_i % n, I_R);
        }
        throw "Cannot delegate a public key";
    }
}

Data ExtendedKey::serialize(const Derivation& generator, bool serialize_private, unsigned int version) const {
    Data data;
    if (version > 0) {
        unsigned char* p = (unsigned char*)&version;
        data.push_back(p[3]);
        data.push_back(p[2]);
        data.push_back(p[1]);
        data.push_back(p[0]);
        data.push_back(generator.path().size());
        // derive parent to get the hash:
        ExtendedKey parent = (*this)(generator, 1);
        unsigned int hash = 0;
        unsigned int child_number = 0;
        if (generator.path().size()) {
            child_number = generator.path().back();
            hash = parent.hash();
        }
        p = (unsigned char*)&hash;
        data.push_back(p[3]);
        data.push_back(p[2]);
        data.push_back(p[1]);
        data.push_back(p[0]);
        p = (unsigned char*)&child_number;
        data.push_back(p[3]);
        data.push_back(p[2]);
        data.push_back(p[1]);
        data.push_back(p[0]);
    }
    ExtendedKey ek = (*this)(generator);
    data.insert(data.end(), ek._chain_code.begin(), ek._chain_code.end());
    if (serialize_private) {
        SecureData priv = ek.serialized_privkey();
        data.push_back(0);
        data.insert(data.end(), priv.begin(), priv.end());
    }
    else {
        Data pub = ek.serialized_pubkey();
        data.insert(data.end(), pub.begin(), pub.end());
    }
    return data;
}

ExtendedKey::Derivation::Path ExtendedKey::Derivation::parse(const std::string tree) const {
    // first skip all non number stuff - it is nice to be able to keep it there to e.g. write m/0'/1 - i.e. keep an m
    Path path;
    std::string::const_iterator c = tree.begin();
    // skip anything that is not a digit or '
    while (c != tree.end() && !isdigit(*c)) ++c;
    while (c != tree.end()) {
        std::string d;
        while (c != tree.end() && isdigit(*c)) {
            d.push_back(*c);
            ++c;
        }
        if (d.size()) {
            unsigned int i = boost::lexical_cast<unsigned int>(d);
            if (c != tree.end() && *c == '\'') {
                i |= 0x80000000;
                ++c;
            }
            path.push_back(i);
        }
        while (c != tree.end() && !isdigit(*c)) ++c; // skip '/'
    }
    return path;
}

ExtendedKey::Derivation::Derivation(const std::string tree) : _fingerprint(0), _path(parse(tree)) {
}

ExtendedKey::Derivation::Derivation(std::vector<unsigned char> script_data) {
    Script script(script_data.begin(), script_data.end());
    
    Evaluator eval;
    eval(script);
    Evaluator::Stack stack = eval.stack();
    
    if (stack.size() < 2)
        throw runtime_error("Derivation - need at least one derivative to make a Key");
    
    if (stack[0].size() != 20)
        throw runtime_error("Derivation - expects a script starting with a fingerprint");
    
    _fingerprint = uint160(stack[0]);
    
    for (size_t i = 1; i < stack.size(); ++i) {
        CBigNum bn(stack[i]);
        unsigned int n = bn.getuint();
        _path.push_back(n);
    }
}

ExtendedKey::Derivation& ExtendedKey::Derivation::operator++() {
    // first check that the last path is not a delegation (generators only work through derivation)
    if (_path.empty() || 0x80000000 & _path.back() || _path.back() == 0x7fffffff )
        _path.push_back(0);
    else
        _path.back()++;
    
    return *this;
}

std::vector<unsigned char> ExtendedKey::Derivation::serialize() const {
    Script script;
    script << _fingerprint;
    for (Path::const_iterator n = _path.begin(); n != _path.end(); ++n)
        script << *n;
    
    return script;
}
