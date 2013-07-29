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

#include <coin/util.h>
#include <coin/ExtendedKey.h>
#include <coin/Address.h>
#include <coin/Script.h>
#include <coin/Transaction.h>

#include <boost/assign/list_of.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/copy.hpp>

using namespace std;
using namespace boost;

std::string test1_master = "000102030405060708090a0b0c0d0e0f";
std::map<string, string> test1_private = boost::assign::map_list_of
( "m", "xprv9s21ZrQH143K3QTDL4LXw2F7HEK3wJUD2nW2nRk4stbPy6cq3jPPqjiChkVvvNKmPGJxWUtg6LnF5kejMRNNU3TGtRBeJgk33yuGBxrMPHi")
( "m/0'", "xprv9uHRZZhk6KAJC1avXpDAp4MDc3sQKNxDiPvvkX8Br5ngLNv1TxvUxt4cV1rGL5hj6KCesnDYUhd7oWgT11eZG7XnxHrnYeSvkzY7d2bhkJ7")
( "m/0'/1", "xprv9wTYmMFdV23N2TdNG573QoEsfRrWKQgWeibmLntzniatZvR9BmLnvSxqu53Kw1UmYPxLgboyZQaXwTCg8MSY3H2EU4pWcQDnRnrVA1xe8fs")
( "m/0'/1/2'", "xprv9z4pot5VBttmtdRTWfWQmoH1taj2axGVzFqSb8C9xaxKymcFzXBDptWmT7FwuEzG3ryjH4ktypQSAewRiNMjANTtpgP4mLTj34bhnZX7UiM")
( "m/0'/1/2'/2", "xprvA2JDeKCSNNZky6uBCviVfJSKyQ1mDYahRjijr5idH2WwLsEd4Hsb2Tyh8RfQMuPh7f7RtyzTtdrbdqqsunu5Mm3wDvUAKRHSC34sJ7in334")
( "m/0'/1/2'/2/1000000000", "xprvA41z7zogVVwxVSgdKUHDy1SKmdb533PjDz7J6N6mV6uS3ze1ai8FHa8kmHScGpWmj4WggLyQjgPie1rFSruoUihUZREPSL39UNdE3BBDu76")
;
std::map<string, string> test1_public = boost::assign::map_list_of
( "m", "xpub661MyMwAqRbcFtXgS5sYJABqqG9YLmC4Q1Rdap9gSE8NqtwybGhePY2gZ29ESFjqJoCu1Rupje8YtGqsefD265TMg7usUDFdp6W1EGMcet8")
( "m/0'", "xpub68Gmy5EdvgibQVfPdqkBBCHxA5htiqg55crXYuXoQRKfDBFA1WEjWgP6LHhwBZeNK1VTsfTFUHCdrfp1bgwQ9xv5ski8PX9rL2dZXvgGDnw")
( "m/0'/1", "xpub6ASuArnXKPbfEwhqN6e3mwBcDTgzisQN1wXN9BJcM47sSikHjJf3UFHKkNAWbWMiGj7Wf5uMash7SyYq527Hqck2AxYysAA7xmALppuCkwQ")
( "m/0'/1/2'", "xpub6D4BDPcP2GT577Vvch3R8wDkScZWzQzMMUm3PWbmWvVJrZwQY4VUNgqFJPMM3No2dFDFGTsxxpG5uJh7n7epu4trkrX7x7DogT5Uv6fcLW5")
( "m/0'/1/2'/2", "xpub6FHa3pjLCk84BayeJxFW2SP4XRrFd1JYnxeLeU8EqN3vDfZmbqBqaGJAyiLjTAwm6ZLRQUMv1ZACTj37sR62cfN7fe5JnJ7dh8zL4fiyLHV")
( "m/0'/1/2'/2/1000000000", "xpub6H1LXWLaKsWFhvm6RVpEL9P4KfRZSW7abD2ttkWP3SSQvnyA8FSVqNTEcYFgJS2UaFcxupHiYkro49S8yGasTvXEYBVPamhGW6cFJodrTHy")
;


std::string test2_master = "fffcf9f6f3f0edeae7e4e1dedbd8d5d2cfccc9c6c3c0bdbab7b4b1aeaba8a5a29f9c999693908d8a8784817e7b7875726f6c696663605d5a5754514e4b484542";
std::map<string, string> test2_private = boost::assign::map_list_of
( "m", "xprv9s21ZrQH143K31xYSDQpPDxsXRTUcvj2iNHm5NUtrGiGG5e2DtALGdso3pGz6ssrdK4PFmM8NSpSBHNqPqm55Qn3LqFtT2emdEXVYsCzC2U")
( "m/0", "xprv9vHkqa6EV4sPZHYqZznhT2NPtPCjKuDKGY38FBWLvgaDx45zo9WQRUT3dKYnjwih2yJD9mkrocEZXo1ex8G81dwSM1fwqWpWkeS3v86pgKt")
( "m/0/2147483647'", "xprv9wSp6B7kry3Vj9m1zSnLvN3xH8RdsPP1Mh7fAaR7aRLcQMKTR2vidYEeEg2mUCTAwCd6vnxVrcjfy2kRgVsFawNzmjuHc2YmYRmagcEPdU9")
( "m/0/2147483647'/1", "xprv9zFnWC6h2cLgpmSA46vutJzBcfJ8yaJGg8cX1e5StJh45BBciYTRXSd25UEPVuesF9yog62tGAQtHjXajPPdbRCHuWS6T8XA2ECKADdw4Ef")
( "m/0/2147483647'/1/2147483646'", "xprvA1RpRA33e1JQ7ifknakTFpgNXPmW2YvmhqLQYMmrj4xJXXWYpDPS3xz7iAxn8L39njGVyuoseXzU6rcxFLJ8HFsTjSyQbLYnMpCqE2VbFWc")
( "m/0/2147483647'/1/2147483646'/2", "xprvA2nrNbFZABcdryreWet9Ea4LvTJcGsqrMzxHx98MMrotbir7yrKCEXw7nadnHM8Dq38EGfSh6dqA9QWTyefMLEcBYJUuekgW4BYPJcr9E7j")
;
std::map<string, string> test2_public = boost::assign::map_list_of
( "m", "xpub661MyMwAqRbcFW31YEwpkMuc5THy2PSt5bDMsktWQcFF8syAmRUapSCGu8ED9W6oDMSgv6Zz8idoc4a6mr8BDzTJY47LJhkJ8UB7WEGuduB")
( "m/0", "xpub69H7F5d8KSRgmmdJg2KhpAK8SR3DjMwAdkxj3ZuxV27CprR9LgpeyGmXUbC6wb7ERfvrnKZjXoUmmDznezpbZb7ap6r1D3tgFxHmwMkQTPH")
( "m/0/2147483647'", "xpub6ASAVgeehLbnwdqV6UKMHVzgqAG8Gr6riv3Fxxpj8ksbH9ebxaEyBLZ85ySDhKiLDBrQSARLq1uNRts8RuJiHjaDMBU4Zn9h8LZNnBC5y4a")
( "m/0/2147483647'/1", "xpub6DF8uhdarytz3FWdA8TvFSvvAh8dP3283MY7p2V4SeE2wyWmG5mg5EwVvmdMVCQcoNJxGoWaU9DCWh89LojfZ537wTfunKau47EL2dhHKon")
( "m/0/2147483647'/1/2147483646'", "xpub6ERApfZwUNrhLCkDtcHTcxd75RbzS1ed54G1LkBUHQVHQKqhMkhgbmJbZRkrgZw4koxb5JaHWkY4ALHY2grBGRjaDMzQLcgJvLJuZZvRcEL")
( "m/0/2147483647'/1/2147483646'/2", "xpub6FnCn6nSzZAw5Tw7cgR9bi15UV96gLZhjDstkXXxvCLsUXBGXPdSnLFbdpq8p9HmGsApME5hQTZ3emM2rnY5agb9rXpVGyy3bdW6EEgAtqt")
;


std::string serialize_private(const ExtendedKey& master, std::string tree) {
    BIP0032::Derivation derive(tree);
    Data data = master.serialize(derive, true, 0x0488ADE4);
    return EncodeBase58Check(data);
}

std::string serialize_public(const ExtendedKey& master, std::string tree) {
    BIP0032::Derivation derive(tree);
    Data data = master.serialize(derive, false, 0x0488B21E);
    return EncodeBase58Check(data);
}

Data passphrase2seed(const std::string& pass_phrase) {
    std::vector<unsigned char> data(pass_phrase.data(), (pass_phrase.data()) + pass_phrase.size());
    
    uint256 j[2];
	SHA512(&(data.front()), data.size(), (unsigned char *) j);
	// the 128bit seed is the first 128 bits in the reinterprete cast of j;
    unsigned char *p = reinterpret_cast<unsigned char*>(j);
    
    Data seed(p, p + 16);
	return seed;
}


int main(int argc, char* argv[])
{
    ExtendedKey alice;
    alice.reset();

    ExtendedKey bob;
    bob.reset();
    
    alice.file("/Users/gronager/alice.pem");
    
    ExtendedKey alicebob(alice.number(), alice.number()*bob.public_point(), alice.chain_code());
    alicebob.file("/Users/gronager/alicebob.pem");
    
    Key alice1("/Users/gronager/alice.pem");
    
    Key alicebob1("/Users/gronager/alicebob.pem");

    cout << hexify(alice.serialized_pubkey()) << endl;
    cout << hexify(alice1.serialized_pubkey()) << endl;
    cout << hexify(alicebob.serialized_pubkey()) << endl;
    cout << hexify(alicebob1.serialized_pubkey()) << endl;
    
    return 0;
    
    try {
        cout << "TESTING: ExtendedKey class" << endl << endl;
        
        try {
            cout << "Testing Test1 with seed: " << test1_master << endl;

            Data seed1 = ParseHex(test1_master);
            ExtendedKey test1(SecureData(seed1.begin(), seed1.end()));
            
            vector<string> keys;
            boost::copy(test1_private | boost::adaptors::map_keys, std::back_inserter(keys)); // Retrieve all keys
            for (vector<string>::const_iterator key = keys.begin(); key != keys.end(); ++key) {
                string pub = serialize_public(test1, *key);
                string prv = serialize_private(test1, *key);
                cout << "\t" << *key;
                if (prv == test1_private[*key])
                    cout << " passed private key test";
                else
                    throw std::runtime_error("Expected: " + test1_private[*key] + " got:" + prv);
                if (pub == test1_public[*key])
                    cout << " - passed public key test\n";
                else
                    throw std::runtime_error("Expected: " + test1_public[*key] + " got:" + pub);
            }
        } catch (...) { throw; }

        cout << endl;
        
        try {
            cout << "Testing Test2 with seed: " << test2_master << endl;

            Data seed2 = ParseHex(test2_master);
            ExtendedKey test2(SecureData(seed2.begin(), seed2.end()));
            
            vector<string> keys;
            boost::copy(test2_private | boost::adaptors::map_keys, std::back_inserter(keys)); // Retrieve all keys
            for (vector<string>::const_iterator key = keys.begin(); key != keys.end(); ++key) {
                string pub = serialize_public(test2, *key);
                string prv = serialize_private(test2, *key);
                cout << "\t" << *key;
                if (prv == test2_private[*key])
                    cout << " passed private key test";
                else
                    throw std::runtime_error("Expected: " + test2_private[*key] + " got:" + prv);
                if (pub == test2_public[*key])
                    cout << " - passed public key test\n";
                else
                    throw std::runtime_error("Expected: " + test2_public[*key] + " got:" + pub);
            }
        } catch (...) { throw; }

        cout << endl;

        // now try with a neutered key vs a private key
        try {
            cout << "Testing Test2 with seed: " << "Top Secret!" << endl;

            string master_secret("Top Secret!");
            Data seed3(master_secret.begin(), master_secret.end());
            ExtendedKey master(SecureData(seed3.begin(), seed3.end()));
            ExtendedKey neutered(master.public_point(), master.chain_code());

            vector<string> keys;
            keys.push_back("m/3/5");
            keys.push_back("m/3/5/7");
            keys.push_back("m/3/5/11");

            for (vector<string>::const_iterator key = keys.begin(); key != keys.end(); ++key) {
                string ppub = serialize_public(master, *key);
                string npub = serialize_public(neutered, *key);
                cout << "\t" << *key;
                if (ppub == npub)
                    cout << " public keys matches" << endl;
                else
                    throw std::runtime_error("Expected: " + ppub + " got:" + npub);

                // insert test for sign / verify etc...
            }
        } catch (...) { throw; }
        
        // non test part - shwoing printing of different alt coin addresses and 
        try {
            cout << "Showing printing of different type of alt coin addresses and keys" << endl;
            
            string master_secret("masterpassphrase");
            Data seed3(master_secret.begin(), master_secret.end());
            ExtendedKey master(SecureData(seed3.begin(), seed3.end()));
            
            ChainAddress addr(0, toPubKeyHash(master.serialized_full_pubkey()));
            cout << "Bitcoin address: " << addr.toString() << endl;

            Data secret = passphrase2seed(master_secret);
            secret.insert(secret.begin(), 1, 33);
            
            cout << "Ripple style secret: " << EncodeBase58Check(secret, "rpshnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCg65jkm8oFqi1tuvAxyz") << endl;

            // private key printing:
            SecureData master_prv = master.serialized_privkey();
            Data prv(master_prv.begin(), master_prv.end());
            
            prv.insert(prv.begin(), 1, 34);
            cout << "Ripple style prvkey: " << EncodeBase58Check(prv, "rpshnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCg65jkm8oFqi1tuvAxyz") << endl;
            
        } catch (...) { throw; }
        
        try {
            // create a key
            CKey key;
            key.MakeNewKey();
            
            PubKeyHash pkh = toPubKeyHash(key.GetPubKey());

            Script script = Script() << OP_DUP << OP_HASH160 << pkh << OP_EQUALVERIFY << OP_CHECKSIG;
            Output from(100000000, script); // One btc
        
            // fake from outpoint
            Coin prevout(uint256("0x123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0"), 2);
            
            Transaction txn;
            txn.addInput(Input(prevout));
            
            txn.addOutput(Output(95000000, Script() << OP_DUP << OP_HASH160 << pkh << OP_EQUALVERIFY << OP_CHECKSIG));

            // now sign the transaction:
            uint256 hash = txn.getSignatureHash(script, 0, SIGHASH_ALL);
            
            vector<unsigned char> sig;
            if (!key.Sign(hash, sig))
                throw runtime_error("couldn't sign!");
            sig.push_back((unsigned char)SIGHASH_ALL);

            Script signature = Script() << sig << key.GetPubKey();
            
            Input input(prevout, signature);
            
            txn.replaceInput(0, input);

            
            if (txn.verify(0, from.script()))
                cout << "verify signature ok!" << endl;
            else
                cout << "verify signature fail!" << endl;
            // create an output that we can spend
        } catch (...) { throw; }
        
        try {
            // create a key
            string master_secret("The secret seed!");
            Data seed3(master_secret.begin(), master_secret.end());
            ExtendedKey master(SecureData(seed3.begin(), seed3.end()));

            // derive a key using multiply
            ExtendedKey derived = master;
            ExtendedKey::Derivation::Path path;
            path.push_back(1);
            path.push_back(2);
            path.push_back(3);
            BIP0032::Derivation derive(derived.fingerprint(), path);
            // Derivation derive = Derivation(fingerprint)[1][2][3];
            // derived = derived<Derivation>(1)
            // derived = derived(Derivation(1))
            derived = derived(derive);
            //derived = derived.derive(1);
            //derived = derived.derive(2);
            //derived = derived.derive(3);
            
            // now do it with a neutered key:
            ExtendedKey neutered(master.public_point(), master.chain_code());
            neutered = neutered(BIP0032::Derivation(1));
            neutered = neutered(BIP0032::Derivation(2));
            neutered = neutered(BIP0032::Derivation(3));
            //            neutered = neutered(derive);
            //neutered = neutered.derive(1);
            //neutered = neutered.derive(2);
            //neutered = neutered.derive(3);
            
            // get public key:
            PubKeyHash pkh = toPubKeyHash(neutered.serialized_pubkey());
            
            Script script = Script() << OP_DUP << OP_HASH160 << pkh << OP_EQUALVERIFY << OP_CHECKSIG;
            Output from(100000000, script); // One btc
            
            // fake from outpoint
            Coin prevout(uint256("0x123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0"), 2);
            
            Transaction txn;
            txn.addInput(Input(prevout));
            
            txn.addOutput(Output(95000000, Script() << OP_DUP << OP_HASH160 << pkh << OP_EQUALVERIFY << OP_CHECKSIG));
            
            // now sign the transaction:
            uint256 hash = txn.getSignatureHash(script, 0, SIGHASH_ALL);
            
            vector<unsigned char> sig = derived.sign(hash);

            sig.push_back((unsigned char)SIGHASH_ALL);
            
            Script signature = Script() << sig << derived.serialized_pubkey();
            
            Input input(prevout, signature);
            
            txn.replaceInput(0, input);
            
            if (txn.verify(0, from.script()))
                cout << "verify signature ok!" << endl;
            else
                cout << "verify signature fail!" << endl;
            // create an output that we can spend
        } catch (...) { throw; }
        cout << "\nPASSED: ExtendedKey class" << endl;

        return 0;
    }
    catch (std::exception &e) {
        cout << "Exception: " << e.what() << endl;
        cout << "FAILED: ExtendedKey class" << endl;
        return 1;
    }
}