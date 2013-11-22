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


#include <coinChain/Node.h>
#include <coinChain/NodeRPC.h>
#include <coinChain/Configuration.h>

#include <coinHTTP/Server.h>
#include <coinHTTP/Client.h>
#include <coinHTTP/RPC.h>

#include <coin/ExtendedKey.h>
#include <coin/Logger.h>

#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/split.hpp>

#include <fstream>

using namespace std;
using namespace boost;
using namespace json_spirit;

// stream operators for Keys and extended keys

// returns base64, header, encoded
tuple<std::string, std::string, bool> pem_extract(std::istream& is) {
    std::ostringstream cs;
	std::string line;
	getline(is, line);
	while(line.find("-----BEGIN ") == std::string::npos) {
		if(is.eof()) throw runtime_error("Unexpected End of File - Not a PEM file?");
		getline(is, line);
    }
    std::string name = line.substr(11, line.size()-11-5);
	cs << line << '\n';
    bool encrypted = false;
	do {
		if(is.eof()) throw runtime_error("Unexpected End of File - PEM File corrupted?");
		getline(is, line);
        if (line.find("ENCRYPTED") != std::string::npos)
            encrypted = true;
		cs << line << '\n';
    }
    while(line.find("-----END ") == std::string::npos);
    return tuple<std::string, std::string, bool>(cs.str(), name, encrypted);
}

class Bio {
public:
    Bio() : _bio(BIO_new(BIO_s_mem())) {}
    Bio(const std::string& str) : _bio(BIO_new(BIO_s_mem())) {
        BIO_puts(_bio, str.c_str());
    }
    ~Bio() {
        BIO_free(_bio);
    }
    BIO* get() { return _bio; }
    string str() const {
        BUF_MEM *bptr;
        BIO_get_mem_ptr(_bio, &bptr);
        return string((const char*)bptr->data, bptr->length);
    }
    
private:
    BIO* _bio;
};

class KeyEnvelope {
public:
    KeyEnvelope(Key& key, std::string name = "") : _key(key), _name(boost::to_upper_copy(name)) {}
    KeyEnvelope(const KeyEnvelope& env) : _key(env._key), _name(env._name) {}
    const std::string& name() const { return _name; }
    bool named() const { return _name.size(); }
    Key& key() { return _key; }
    const Key& key() const { return _key; }
private:
    Key& _key;
    std::string _name;
};

std::istream& operator>>(std::istream& is, KeyEnvelope envelope) {
	// now we have read the first entry, which is a certificate or certificate request
    string pem, name;
    bool encrypted;
    tie(pem, name, encrypted) = pem_extract(is);
    if (envelope.named() && name.find(envelope.name()) == string::npos)
        throw runtime_error("Trying to extract a " + name + " for " + envelope.name());
    if (encrypted)
        throw runtime_error("Trying to extract an encrypted " + name + " with no password callback");
	Bio bio(pem);
    if (name.find(" PRIVATE KEY") != string::npos) {
        EC_KEY* eckey = EC_KEY_new();
        if (!PEM_ASN1_read_bio((d2i_of_void*)&d2i_ECPrivateKey, name.c_str(), bio.get(), (void**)&eckey, NULL, NULL))
            throw runtime_error("Failed reading: " + name);
        if (!eckey)
            throw runtime_error("Failed reading: " + name);
        envelope.key().reset(eckey);
        return is;
    }

    if (name.find("PUBLIC KEY") != string::npos) {
        EC_KEY* eckey = EC_KEY_new();
        PEM_ASN1_read_bio((d2i_of_void*)&d2i_EC_PUBKEY, name.c_str(), bio.get(), (void**)&eckey, NULL, NULL);
        // extract the ec_key from the evpkey
        if (!eckey)
            throw runtime_error("Failed reading: " + name);
        envelope.key().reset(eckey);
        return is;
    }

    throw runtime_error("Cannot read: " + name);
}

std::ostream& operator<<(std::ostream& os, const KeyEnvelope& envelope) {
    const EC_KEY* eckey = envelope.key().eckey();

    Bio bio;
    if (envelope.key().isPrivate()) {
        string name = (envelope.named() ? envelope.name() : string("EC")) + " PRIVATE KEY";
        int success = PEM_ASN1_write_bio((i2d_of_void *)(&i2d_ECPrivateKey), name.c_str(), bio.get(), (void*)eckey, NULL, NULL, 0, NULL, NULL);
        if (!success)
            throw runtime_error("Failed saving " + name);
    }
    else { // assuming public key
        string name = (envelope.named() ? envelope.name() : string("EC")) + " PUBLIC KEY";
        int success = PEM_ASN1_write_bio((i2d_of_void *)(&i2d_EC_PUBKEY), name.c_str(), bio.get(), (void*)eckey, NULL, NULL, 0, NULL, NULL);
        if (!success)
            throw runtime_error("Failed saving " + name);
    }
    return os << bio.str();
}

/// Usage:
///     Account account("17uY6a7zUidQrJVvpnFchD1MX1untuAufd");
///     account.sync(blockChain());
///     cout << account.balance() << endl;

class BlockChainAccessor : protected sqliterate::Database {
    private: // noncopyable
        BlockChainAccessor(const BlockChainAccessor&);
        void operator=(const BlockChainAccessor&);
        
    public:
    BlockChainAccessor(const Chain& chain = bitcoin, const std::string dataDir = "") : sqliterate::Database(dataDir + "/blockchain.sqlite3") {
        
    }
    
    int getBestHeight() const {
        return query<int>("SELECT MAX(count) FROM Blocks")-1;
    }
    
    Block getBlockHeader(int height = -1) const {
        if (height == -1)
            height = getBestHeight();
        return queryRow<Block(int, uint256, uint256, int, int, int)>("SELECT version, prev, mrkl, time, bits, nonce FROM Blocks WHERE count = ?", height+1);
    }
    
    void getUnspents(const Script& script, Unspents& unspents, unsigned int before = 0) const {
        unspents = queryColRow<Unspent(int64_t, uint256, unsigned int, int64_t, Script, int64_t, int64_t)>("SELECT coin, hash, idx, value, script, count, ocnf FROM Unspents WHERE script = ?", script);
        
        if (0 < before && before < LOCKTIME_THRESHOLD) { // remove those newer than before
            for (int i = unspents.size() - 1; i >= 0; --i)
                if (unspents[i].count > before)
                    unspents.erase(unspents.begin()+i);
        }
    }


};

class Account {
public:
    virtual void sync(const BlockChainAccessor& blockChain, bool ignore_age = false) = 0;

    /// return a payment script
    virtual Script invoice() = 0;
    
    /// add a transaction to the account, if the account is affected the registered callback will be called
    virtual void debit(const Transaction& txn, int count = 0) = 0;
    
    // total balance
    virtual int64_t balance() const = 0;

    // make a payout
    virtual Transaction credit(const Script& script, int64_t amount) = 0;

    /// if signing fails or for some other reason the transfer is cancelled revoke the transaction
    virtual void revoke(const Transaction& txn) = 0;
};

class SingleAccount : public Account {
public:
    SingleAccount(const ChainAddress& addr) : _addr(addr) {}
    ~SingleAccount() {}
    
    virtual void sync(const BlockChainAccessor& blockChain, bool ignore_age = false) {
        Block block = blockChain.getBlockHeader();
        if (!ignore_age && block.getTime() < UnixTime::s() - 60*60) {
            throw runtime_error("block chain out of date!");
        }
        
        _unspents.clear();
        blockChain.getUnspents(_addr.getStandardScript(), _unspents);
    }
    
    /// return a payment script
    virtual Script invoice() {
        return _addr.getStandardScript();
    }
    
    /// add a transaction to the account, if the account is affected the registered callback will be called
    virtual void debit(const Transaction& txn, int count = 0) {}
    
    // total balance
    virtual int64_t balance() const {
        int64_t sum = 0;
        for (Unspents::const_iterator u = _unspents.begin(); u != _unspents.end(); ++u)
            sum += u->output.value();
        return sum;
    }
    
    // make a payout
    virtual Transaction credit(const Script& script, int64_t amount) {}
    
    /// if signing fails or for some other reason the transfer is cancelled revoke the transaction
    virtual void revoke(const Transaction& txn) {}

private:
    ChainAddress _addr;
    Unspents _unspents;
};

/// The Deterministic Account
class DeterministicAccount : public Account, private sqliterate::Database {
public:
    DeterministicAccount(std::string filename);
private:
    
};

/// The Multi Account takes several addresses / pem files and create a P2SH script
class MultiAccount : public Account {
public:
    typedef std::vector<ChainAddress> ChainAddresses;
    // default is one less that the number of addresses listed
    MultiAccount(const ChainAddresses& addrs, int min = 0) : _addrs(addrs), _min(min?min:addrs.size()-1) {
        if (addrs.size() < 2)
            throw runtime_error("MultiAccount requires at least 2 addresses");
    }
    
    ChainAddress pay2script() const {
        // loop over the pubkeys
    }
private:
    ChainAddresses _addrs;
    int _min;
};

/// Cryptoshi is a commandline client that enables you to use a running libcoind daemon as a gateway to the bitcoin network.
/// You can e.g. use it to determine balance for addresses through the blockchain as stored in blockchain.sqlite3

/// cryptoshi balance 17uY6a7zUidQrJVvpnFchD1MX1untuAufd
/// cryptoshi tfiransaction 17uY6a7zUidQrJVvpnFchD1MX1untuAufd 0.10 17uY6a7zUidQrJVvpnFchD1MX1untuAufd
/// cryptoshi sign []
/// cryptoshi post ...

int main(int argc, char* argv[])
{
    SSL_library_init();
    OpenSSL_add_all_algorithms();

    boost::program_options::options_description extra("Extra options");
    Configuration conf(argc, argv, extra);
    
    try {
        // If we have params on the cmdline we run as a command line client contacting a server
        if (conf.method() != "") {
            if (conf.method() == "help") {
                cout << "Usage: " << argv[0] << " [options] [rpc-command param1 param2 ...]\n";
                cout << conf << "\n";
                cout << "If no rpc-command is specified, " << argv[0] << " start up as a daemon, otherwise it offers commandline access to a running instance\n";
                return 1;
            }
            
            if (conf.method() == "version") {
                cout << argv[0] << " version is: " << FormatVersion(PROTOCOL_VERSION) << "\n";
                return 1;
            }
            
            // load the block chain;
            const BlockChainAccessor blockChain(conf.chain(), conf.data_dir());

            if (conf.method() == "balance") {
                // next arg is an address / pem file / wallet.dat / cash.btc file
                filesystem::path file(conf.param(0));
                if (file.extension().string() == ".pem" && exists(file)) {
                    // create a private pem file (password not supported, yet)
                    Key key;
                    ifstream ifs(file.string().c_str());
                    ifs >> KeyEnvelope(key);
                    ChainAddress addr = conf.chain().getAddress(toPubKeyHash(key.serialized_pubkey()));
                    SingleAccount account(addr);
                    account.sync(blockChain, true);
                    cout << account.balance() << endl;
                }
                else {
                    ChainAddress addr = conf.chain().getAddress(conf.param(0));
                    SingleAccount account(addr);
                    account.sync(blockChain, true);
                    cout << account.balance() << endl;
                }
            }
            
            // -----BEGIN BITCOIN EXTENDED KEY----- // a number, which is the deterministic generator and a bitcoin public key, formatted as a private key
            // -----BEGIN BITCOIN PRIVATE KEY----- // a normal ec private key
            // -----BEGIN BITCOIN PUBLIC KEY----- // a normal ec public key
            // -----BEGIN BITCOIN SPLIT KEY----- // public from one key, private from the other
            // You can concat several keys together to form other entities - e.g. take 3 keys, a private, and 2 public and you can generate an invoice from it.
            
            
            // So for normal use - we have the following commands:
            //    cryptoshi pubkey dum.pem
            
            // take our shared key - it has an extended key and two shared keys - to operate, you generate a shared key:
            // generate a private key, export public key, send public key to peer, peer generate a private key and do a convert to shared etc...
            // SQLite3 databases:
            ///  normal, deterministic, multi, etc...
            ///  databases hold different things like special keys etc...
            ///  
            
            
            // new creates the specified : .pem or cash.xbt
            // new mywallet.pem // will create a private key in a file
            // new cash.xbt will create a single key wallet
            // new cash.xbt btcaddr btcaddr / btcaddr cash.xbt btcaddr / key.pem btcaddr btcaddr will create a multi key wallet
            // account types:
            // brain account: cryptoshi invoice "This is my secrey password"
            // cash.xbt is a multi key wallet
            if (conf.method() == "new") {
                // parse to find the number of params - is it 1 or 3
                // parse to find the type - is it .pem or .xbt/.nmt/...
                // example: new cash.btc 1Yv1313werwer 1Yasdw
                // how do we encompas a pailler signing system ? by two signatures: a .cash and a .pem/public ?
                // we treasury needs: signatory public, treusurer private, generator
                // signatory needs: signatory private, treasurer public - how to generate them ? - two / three step process - generate private,
                // signatory will have to wait..., but it is most likely a 2 key solution.
                if (conf.params().size() == 1) {
                    // does it end in .pem ?
                    filesystem::path file(conf.param(0));
                    if (file.extension().string() == ".pem") {
                        if (exists(file))
                            throw runtime_error("Key: " + file.string() + " already exist.");
                        // create a private pem file (password not supported, yet)
                        Key key;
                        key.reset();
                        
                        ofstream ofs(file.string().c_str());
                        ofs << KeyEnvelope(key, conf.chain().name());
                    }
                    else {
                        throw runtime_error("Only .pem keys supported");
                    }
                }
                else if (conf.params().size() == 3) {
                }
                else {
                    throw runtime_error("Only 1 or 3 key wallets supported");
                }
                
            }

            if (conf.method() == "invoice") {
                if (conf.params().size() == 1) {
                    // does it end in .pem ?
                    filesystem::path file(conf.param(0));
                    if (file.extension().string() == ".pem" && exists(file)) {
                        Key key(file.string());
                        ChainAddress invoice = conf.chain().getAddress(toPubKeyHash(key.serialized_pubkey()));
                        cout << invoice.toString() << endl;
                    }
                    else {
                        throw runtime_error("Only .pem keys supported");
                    }
                }
                else if (conf.params().size() == 3) {
                    // read all 3 files and generate a p2sh script
                    Script script = (Script() << 2);
                    for (int i = 0; i < 3; ++i) {
                        Key key;
                        ifstream ifs(conf.param(i).c_str());
                        ifs >> KeyEnvelope(key);
                        script << key.serialized_pubkey();
                    }
                    script << 3 << OP_CHECKMULTISIG;
                    // now turn the script into a hash:
                    ChainAddress addr = conf.chain().getAddress(toScriptHash(script));
                    cout << addr.toString() << endl;
                }
                else {
                    throw runtime_error("Only 1 or 3 key wallets supported");
                }
                
            }
            
            if (conf.method() == "credit") {
                // cryptoshi credit from amount to - return transaction in hex
            }
        }
        return 0;
        
    }
    catch(std::exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
}