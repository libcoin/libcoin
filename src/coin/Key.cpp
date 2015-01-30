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

#include <coin/Key.h>

#include <openssl/pem.h>

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

Key::Key(const SecureData& serialized_key) : _ec_key(NULL){
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
    else if (serialized_key.size() == 65) { // public key
        const unsigned char* p = &serialized_key[0];
        EC_KEY_set_conv_form(_ec_key, POINT_CONVERSION_UNCOMPRESSED);
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
            throw runtime_error("Key::sign_compact : unable to construct recoverable key");
        
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
    // New versions of OpenSSL will reject non-canonical DER signatures. de/re-serialize first.
    unsigned char *norm_der = NULL;
    ECDSA_SIG *norm_sig = ECDSA_SIG_new();
    const unsigned char* sigptr = &signature[0];
    d2i_ECDSA_SIG(&norm_sig, &sigptr, signature.size());
    int derlen = i2d_ECDSA_SIG(norm_sig, &norm_der);
    ECDSA_SIG_free(norm_sig);
    if (derlen <= 0)
        return false;
    
    // -1 = error, 0 = bad sig, 1 = good
    bool ret = ECDSA_verify(0, (unsigned char*)&hash, sizeof(hash), norm_der, derlen, _ec_key) == 1;
    OPENSSL_free(norm_der);
    return ret;

    // -1 = error, 0 = bad sig, 1 = good
//    int check = ECDSA_verify(0, (unsigned char*)&hash, sizeof(hash), &signature[0], signature.size(), _ec_key);
//    return (check == 1);
}

const BIGNUM* Key::private_number() const {
    return EC_KEY_get0_private_key(_ec_key);
}

// Perform ECDSA key recovery (see SEC1 4.1.6) for curves over (mod p)-fields
// recid selects which key is recovered
// if check is nonzero, additional checks are performed
int ECDSA_SIG_recover_key_GFp(EC_KEY *eckey, ECDSA_SIG *ecsig, const unsigned char *msg, int msglen, int recid, int check)
{
    if (!eckey) return 0;
    
    int ret = 0;
    BN_CTX *ctx = NULL;
    
    BIGNUM *x = NULL;
    BIGNUM *e = NULL;
    BIGNUM *order = NULL;
    BIGNUM *sor = NULL;
    BIGNUM *eor = NULL;
    BIGNUM *field = NULL;
    EC_POINT *R = NULL;
    EC_POINT *O = NULL;
    EC_POINT *Q = NULL;
    BIGNUM *rr = NULL;
    BIGNUM *zero = NULL;
    int n = 0;
    int i = recid / 2;
    
    const EC_GROUP *group = EC_KEY_get0_group(eckey);
    if ((ctx = BN_CTX_new()) == NULL) { ret = -1; goto err; }
    BN_CTX_start(ctx);
    order = BN_CTX_get(ctx);
    if (!EC_GROUP_get_order(group, order, ctx)) { ret = -2; goto err; }
    x = BN_CTX_get(ctx);
    if (!BN_copy(x, order)) { ret=-1; goto err; }
    if (!BN_mul_word(x, i)) { ret=-1; goto err; }
    if (!BN_add(x, x, ecsig->r)) { ret=-1; goto err; }
    field = BN_CTX_get(ctx);
    if (!EC_GROUP_get_curve_GFp(group, field, NULL, NULL, ctx)) { ret=-2; goto err; }
    if (BN_cmp(x, field) >= 0) { ret=0; goto err; }
    if ((R = EC_POINT_new(group)) == NULL) { ret = -2; goto err; }
    if (!EC_POINT_set_compressed_coordinates_GFp(group, R, x, recid % 2, ctx)) { ret=0; goto err; }
    if (check)
    {
        if ((O = EC_POINT_new(group)) == NULL) { ret = -2; goto err; }
        if (!EC_POINT_mul(group, O, NULL, R, order, ctx)) { ret=-2; goto err; }
        if (!EC_POINT_is_at_infinity(group, O)) { ret = 0; goto err; }
    }
    if ((Q = EC_POINT_new(group)) == NULL) { ret = -2; goto err; }
    n = EC_GROUP_get_degree(group);
    e = BN_CTX_get(ctx);
    if (!BN_bin2bn(msg, msglen, e)) { ret=-1; goto err; }
    if (8*msglen > n) BN_rshift(e, e, 8-(n & 7));
    zero = BN_CTX_get(ctx);
    if (!BN_zero(zero)) { ret=-1; goto err; }
    if (!BN_mod_sub(e, zero, e, order, ctx)) { ret=-1; goto err; }
    rr = BN_CTX_get(ctx);
    if (!BN_mod_inverse(rr, ecsig->r, order, ctx)) { ret=-1; goto err; }
    sor = BN_CTX_get(ctx);
    if (!BN_mod_mul(sor, ecsig->s, rr, order, ctx)) { ret=-1; goto err; }
    eor = BN_CTX_get(ctx);
    if (!BN_mod_mul(eor, e, rr, order, ctx)) { ret=-1; goto err; }
    if (!EC_POINT_mul(group, Q, eor, R, sor, ctx)) { ret=-2; goto err; }
    if (!EC_KEY_set_public_key(eckey, Q)) { ret=-2; goto err; }
    
    ret = 1;
    
err:
    if (ctx) {
        BN_CTX_end(ctx);
        BN_CTX_free(ctx);
    }
    if (R != NULL) EC_POINT_free(R);
    if (O != NULL) EC_POINT_free(O);
    if (Q != NULL) EC_POINT_free(Q);
    return ret;
}


