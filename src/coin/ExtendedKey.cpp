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


#include <coin/ExtendedKey.h>
#include <coin/Script.h>

#include <vector>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/ec.h>

using namespace std;

// Generate a private key from just the secret parameter
static int EC_KEY_regenerate_key(EC_KEY *eckey, const BIGNUM *priv_key)
{
    int ok = 0;
    BN_CTX *ctx = NULL;
    EC_POINT *pub_key = NULL;
    
    if (!eckey) return 0;
    
    const EC_GROUP *group = EC_KEY_get0_group(eckey);
    
    if ((ctx = BN_CTX_new()) == NULL)
        goto err;
    
    pub_key = EC_POINT_new(group);
    
    if (pub_key == NULL)
        goto err;
    
    if (!EC_POINT_mul(group, pub_key, priv_key, NULL, NULL, ctx))
        goto err;
    
    EC_KEY_set_private_key(eckey,priv_key);
    EC_KEY_set_public_key(eckey,pub_key);
    
    ok = 1;
    
err:
    
    if (pub_key)
        EC_POINT_free(pub_key);
    if (ctx != NULL)
        BN_CTX_free(ctx);
    
    return(ok);
}

int __pass_cb(char *buf, int size, int rwflag, void *u) {
    Key::PassphraseFunctor* pf = (Key::PassphraseFunctor*)u;
    bool verify = (rwflag == 1);
    
    SecureString ss = (*pf)(verify);
    
    int len;
    len = ss.size();
    if (len <= 0) return 0;
    // if too long, truncate
    if (len > size) len = size;
    memcpy(buf, ss.c_str(), len);
    return len;
}

Point::Point(int curve) : _ec_point(NULL), _ec_group(EC_GROUP_new_by_curve_name(curve)) {
    _ec_point = EC_POINT_new(_ec_group);
}

Point::Point(const Point& point) : _ec_group(EC_GROUP_new_by_curve_name(NID_secp256k1)) {
    EC_GROUP_copy(_ec_group, point._ec_group);
    _ec_point = EC_POINT_new(_ec_group);
    EC_POINT_copy(_ec_point, point._ec_point);
}

Point::Point(const EC_POINT* p, int curve) : _ec_group(EC_GROUP_new_by_curve_name(curve)) {
    _ec_point = EC_POINT_new(_ec_group);
    EC_POINT_copy(_ec_point, p);
}

Point::Point(Infinity inf, int curve) : _ec_group(EC_GROUP_new_by_curve_name(curve)) {
    _ec_point = EC_POINT_new(_ec_group);
    EC_POINT_set_to_infinity(_ec_group, _ec_point);
}

Point::Point(const CBigNum& x, const CBigNum& y, int curve) : _ec_group(EC_GROUP_new_by_curve_name(curve)) {
    _ec_point = EC_POINT_new(_ec_group);
    if(!EC_POINT_set_affine_coordinates_GFp(_ec_group, _ec_point, &x, &y, NULL))
        throw runtime_error("Trying to set ec point, but it might not be on curve!");
}

Point::~Point() {
    EC_POINT_clear_free(_ec_point);
    EC_GROUP_clear_free(_ec_group);
}

Point& Point::operator=(const Point& point) {
    _ec_group = EC_GROUP_new_by_curve_name(NID_secp256k1);
    EC_GROUP_copy(_ec_group, point._ec_group);
    _ec_point = EC_POINT_new(_ec_group);
    EC_POINT_copy(_ec_point, point._ec_point);
    
    return *this;
}

CBigNum Point::X() const {
    CBigNum x;
    CBigNum y;
    EC_POINT_get_affine_coordinates_GFp(_ec_group, _ec_point, &x, &y, NULL);
    return x;
}

CBigNum Point::Y() const {
    CBigNum x;
    CBigNum y;
    EC_POINT_get_affine_coordinates_GFp(_ec_group, _ec_point, &x, &y, NULL);
    return y;
}

Point& Point::operator+=(const Point& point) {
    EC_POINT* r = EC_POINT_new(_ec_group);
    EC_POINT_add(_ec_group, r, _ec_point, point._ec_point, NULL);
    EC_POINT_clear_free(_ec_point);
    _ec_point = r;
    return *this;
}

EC_GROUP* Point::ec_group() const {
    return _ec_group;
}

EC_POINT* Point::ec_point() const {
    return _ec_point;
}

Point operator*(const CBigNum& m, const Point& Q) {
    if (!EC_POINT_is_on_curve(Q.ec_group(), Q.ec_point(), NULL))
        throw std::runtime_error("Q not on curve");
    
    Point R(Q);
    
    if (!EC_POINT_mul(R.ec_group(), R.ec_point(), NULL, Q.ec_point(), &m, NULL))
        throw std::runtime_error("Multiplication error");
    
    return R;
}

bool operator==(const Point& P, const Point& Q) {
    return (EC_POINT_cmp(P.ec_group(), P.ec_point(), Q.ec_point(), NULL) == 0);
}


Key::Key() : _ec_key(NULL) {
    _ec_key = EC_KEY_new_by_curve_name(NID_secp256k1);
    EC_KEY_set_conv_form(_ec_key, POINT_CONVERSION_COMPRESSED);
}

Key::Key(const Key& key) : _ec_key(NULL) {
    _ec_key = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (key.isPrivate())
        reset(key.number(), key.public_point());
    else
        reset(key.public_point());
}

Key::Key(const SecureData& serialized_key) {
    EC_KEY_free(_ec_key);
    _ec_key = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (serialized_key.size() == 32) { // private key
        CBigNum prv;
        BN_bin2bn(&serialized_key[0], serialized_key.size(), &prv);
        reset(prv);
    }
    else if (serialized_key.size() == 33) { // public key
        const unsigned char* p = &serialized_key[0];
        if (!o2i_ECPublicKey(&_ec_key, &p, serialized_key.size()))
            throw runtime_error("Could not set public key");
    }
    else
        throw runtime_error("Key initialized with wrong length data");
}

Key::Key(const CBigNum& private_number) : _ec_key(NULL) {
    _ec_key = EC_KEY_new_by_curve_name(NID_secp256k1);
    EC_KEY_regenerate_key(_ec_key, &private_number);
    EC_KEY_set_conv_form(_ec_key, POINT_CONVERSION_COMPRESSED);
}

Key::Key(const Point& public_point) : _ec_key(NULL) {
    _ec_key = EC_KEY_new();
    EC_KEY_set_group(_ec_key, public_point.ec_group());
    EC_KEY_set_public_key(_ec_key, public_point.ec_point());
    EC_KEY_set_conv_form(_ec_key, POINT_CONVERSION_COMPRESSED);
}

Key::Key(const CBigNum& private_number, const Point& public_point) : _ec_key(NULL) {
    _ec_key = EC_KEY_new_by_curve_name(NID_secp256k1);
    EC_KEY_set_private_key(_ec_key, &private_number);
    EC_KEY_set_group(_ec_key, public_point.ec_group());
    EC_KEY_set_public_key(_ec_key, public_point.ec_point());
    EC_KEY_set_conv_form(_ec_key, POINT_CONVERSION_COMPRESSED);
}

SecureString Key::GetPassFunctor::operator()(bool verify) {
    SecureString passphrase = getpass(_prompt.c_str());
    return passphrase;
}

Key::Key(const std::string& filename) : _ec_key(NULL) {
    EVP_PKEY* pkey = EVP_PKEY_new();
    FILE* f = fopen(filename.c_str(), "r");
    if (!f)
        throw runtime_error(filename + " not found, or access not permitted!");
    GetPassFunctor pf;
    PEM_read_PrivateKey(f, &pkey, &__pass_cb, (void*) &pf);
    if (pkey)
        _ec_key = EVP_PKEY_get1_EC_KEY(pkey);
    else {
        PEM_read_PUBKEY(f, &pkey, &__pass_cb, (void*) &pf);
        _ec_key = EVP_PKEY_get1_EC_KEY(pkey);
    }
    if(f) fclose(f);

    if (!_ec_key)
        throw runtime_error(filename + " did not cointain a valid PEM formatted key");
}

Key::Key(const std::string& filename, PassphraseFunctor& pf) : _ec_key(NULL) {
    EVP_PKEY* pkey = EVP_PKEY_new();
    FILE* f = fopen(filename.c_str(), "r");
    if (!f)
        throw runtime_error(filename + " not found, or access not permitted!");
    PEM_read_PrivateKey(f, &pkey, &__pass_cb, (void*) &pf);
    if (pkey)
        _ec_key = EVP_PKEY_get1_EC_KEY(pkey);
    else {
        PEM_read_PUBKEY(f, &pkey, &__pass_cb, (void*) &pf);
        _ec_key = EVP_PKEY_get1_EC_KEY(pkey);
    }
    if(f) fclose(f);
    
    if (!_ec_key)
        throw runtime_error(filename + " did not cointain a valid PEM formatted key");
}

Key::Key(uint256 hash, const Data& signature) {
    if (signature.size() != 65)
        throw runtime_error("Key::Key signature size != 65 bytes");

    // the first byte of the signature is the rec:
    int rec = (signature[0] - 27) & ~4;
    const unsigned char* p64 = &signature[1];

    if (rec<0 || rec>=3)
        throw runtime_error("Key::Key signature malformed");

    if (!((signature[0] - 27) & 4))
        throw runtime_error("Key::Key verification of uncompressed keys not supported");
    
    ECDSA_SIG *sig = ECDSA_SIG_new();
    BN_bin2bn(&p64[0],  32, sig->r);
    BN_bin2bn(&p64[32], 32, sig->s);
    _ec_key = EC_KEY_new_by_curve_name(NID_secp256k1);
    EC_KEY_set_conv_form(_ec_key, POINT_CONVERSION_COMPRESSED);
    bool ret = ECDSA_SIG_recover_key_GFp(_ec_key, sig, (unsigned char*)&hash, sizeof(hash), rec, 0) == 1;
    ECDSA_SIG_free(sig);
    
    if (!ret)
        throw runtime_error("Key::Key pub key recovery failed");
}

void Key::file(const std::string& filename, bool overwrite) {
    if (!overwrite) {
        FILE* f = fopen(filename.c_str(), "r");
        if (f) {
            fclose(f);
            return; // exit silently
        }
    }
    EVP_PKEY* pkey = EVP_PKEY_new();
    EVP_PKEY_set1_EC_KEY(pkey, _ec_key);
    FILE* f = fopen(filename.c_str(), "w");
    if (!f)
        throw runtime_error("attempt to write to " + filename + " failed");
    int success = 0;
    if (isPrivate()) {
        GetPassFunctor pf;
        //success = PEM_write_PrivateKey(f, pkey, EVP_aes_256_cbc(), NULL, 0, &__pass_cb, (void*) &pf);
        
        //success = PEM_write_PrivateKey(f, pkey, NULL, NULL, 0, NULL, NULL);
        success = PEM_write_ECPrivateKey(f, _ec_key, NULL, NULL, 0, NULL, NULL);
    }
    else
        success = PEM_write_PUBKEY(f, pkey);
    if (f) fclose(f);
    if (!success)
        throw runtime_error("attempt to output PEM to " + filename + " failed");
    
}

bool Key::isPrivate() const {
    return EC_KEY_get0_private_key(_ec_key);
}

bool Key::isConsistent() const {
    if (!isPrivate()) return true;
    Key key(number());
    return (public_point() == key.public_point());
}

void Key::reset() {
    EC_KEY_generate_key(_ec_key);
    EC_KEY_set_conv_form(_ec_key, POINT_CONVERSION_COMPRESSED);
}

void Key::reset(const CBigNum& private_number) {
    EC_KEY_regenerate_key(_ec_key, &private_number);
    EC_KEY_set_conv_form(_ec_key, POINT_CONVERSION_COMPRESSED);
}

void Key::reset(const Point& public_point) {
    EC_KEY_set_group(_ec_key, public_point.ec_group());
    EC_KEY_set_conv_form(_ec_key, POINT_CONVERSION_COMPRESSED);
    EC_KEY_set_public_key(_ec_key, public_point.ec_point());
}

void Key::reset(const CBigNum& private_number, const Point& public_point) {
    _ec_key = EC_KEY_new_by_curve_name(NID_secp256k1);
    EC_KEY_set_private_key(_ec_key, &private_number);
    EC_KEY_set_group(_ec_key, public_point.ec_group());
    EC_KEY_set_public_key(_ec_key, public_point.ec_point());
    EC_KEY_set_conv_form(_ec_key, POINT_CONVERSION_COMPRESSED);
}

void Key::reset(EC_KEY* eckey) {
    if (_ec_key)
        EC_KEY_free(_ec_key);
    _ec_key = EC_KEY_dup(eckey);
}

Data Key::serialized_pubkey() const {
    Data data(33, 0); // has to be 33 bytes long!
    unsigned char* begin = &data[0];
    if (i2o_ECPublicKey(_ec_key, &begin) != 33)
        throw std::runtime_error("i2o_ECPublicKey returned unexpected size, expected 33");
    
    return data;
}

Data Key::serialized_full_pubkey() const {
    Data data(65, 0); // has to be 65 bytes long!
    unsigned char* begin = &data[0];
    EC_KEY* full_ec = EC_KEY_new();
    EC_KEY_set_conv_form(_ec_key, POINT_CONVERSION_UNCOMPRESSED);
    EC_KEY_copy(full_ec, _ec_key);
    if (i2o_ECPublicKey(_ec_key, &begin) != 65)
        throw std::runtime_error("i2o_ECPublicKey returned unexpected size, expected 65");
    
    return data;
}

SecureData Key::serialized_privkey() const {
    if (!isPrivate())
        throw std::runtime_error("cannot serialize priv key in neutered key");
    SecureData data(32, 0); // has to be 32 bytes long!
    const BIGNUM *bn = EC_KEY_get0_private_key(_ec_key);
    int bytes = BN_num_bytes(bn);
    if (bn == NULL)
        throw std::runtime_error("EC_KEY_get0_private_key failed");
    if ( BN_bn2bin(bn, &data[32 - bytes]) != bytes)
        throw std::runtime_error("BN_bn2bin failed");
    
    return data;
}

const Point Key::public_point() const {
    return Point(EC_KEY_get0_public_key(_ec_key));
}

CBigNum Key::order() const {
    CBigNum bn;
    EC_GROUP_get_order(EC_KEY_get0_group(_ec_key) , &bn, NULL);
    return bn;
}

CBigNum Key::number() const {
    CBigNum bn;
    return CBigNum(EC_KEY_get0_private_key(_ec_key));
}

Data Key::sign(uint256 hash) const {
    Data signature;
    unsigned int size = ECDSA_size(_ec_key);
    signature.resize(size); // Make sure it is big enough
    if (!ECDSA_sign(0, (unsigned char*)&hash, sizeof(hash), &signature[0], &size, _ec_key))
        throw runtime_error("Could not sign!");
    signature.resize(size); // Shrink to fit actual size
    return signature;
}

Data Key::sign_compact(uint256 hash, Data signature) const {
    Data compact;
    
    if (signature.size() == 0)
        signature = sign(hash);
    
    bool fOk = false;
    const unsigned char *pp = &signature[0];
    ECDSA_SIG *sig = d2i_ECDSA_SIG(NULL, &pp, signature.size());
    
    if (sig==NULL)
        throw runtime_error("Key::sign_compact failed: wrong signature provided.");
    compact.resize(65,0);
    int nBitsR = BN_num_bits(sig->r);
    int nBitsS = BN_num_bits(sig->s);
    if (nBitsR <= 256 && nBitsS <= 256) {
        int nRecId = -1;
        for (int i=0; i<4; i++) {
            EC_KEY* eckey = EC_KEY_new_by_curve_name(NID_secp256k1);
            EC_KEY_set_conv_form(eckey, POINT_CONVERSION_COMPRESSED);
            if (ECDSA_SIG_recover_key_GFp(eckey, sig, (unsigned char*)&hash, sizeof(hash), i, 1) == 1) {
                Key key;
                key.reset(eckey);
                if (key.serialized_pubkey() == serialized_pubkey()) {
                    nRecId = i;
                    break;
                }
            }
            EC_KEY_free(eckey);
        }
        
        if (nRecId == -1)
            throw key_error("Key::sign_compact : unable to construct recoverable key");
        
        compact[0] = nRecId+27+(true ? 4 : 0);
        BN_bn2bin(sig->r,&compact[33-(nBitsR+7)/8]);
        BN_bn2bin(sig->s,&compact[65-(nBitsS+7)/8]);
        fOk = true;
    }
    ECDSA_SIG_free(sig);
    if (!fOk)
        throw runtime_error("Could not generate compact signature!");
    
    return compact;
}

bool Key::verify(uint256 hash, const Data& signature) const {
    // -1 = error, 0 = bad sig, 1 = good
    int check = ECDSA_verify(0, (unsigned char*)&hash, sizeof(hash), &signature[0], signature.size(), _ec_key);
    return (check == 1);
}

const BIGNUM* Key::private_number() const {
    return EC_KEY_get0_private_key(_ec_key);
}

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
