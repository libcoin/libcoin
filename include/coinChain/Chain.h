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

#ifndef CHAIN_H
#define CHAIN_H

#include <coin/Block.h>
#include <coin/Currency.h>

#include <coinChain/Export.h>
#include <coinChain/BlockTree.h>

#include <boost/noncopyable.hpp>
#include <boost/array.hpp>

/// Pure virtual baseclass Chain defining a chain. Using this template you can define any bitcoin based cryptocurrency.

typedef boost::array<unsigned char, 4> MessageStart;

class COINCHAIN_EXPORT Chain : public Currency {
public:
    Chain(const std::string name, const std::string currency_code, size_t decimals) : Currency(name, currency_code, decimals) {}
    enum ChangeIdentifier {
        BIP0016 = 16,
        BIP0030 = 30
    };
    virtual const int protocol_version() const = 0;
    virtual const Block& genesisBlock() const = 0;
    virtual const uint256& genesisHash() const = 0;
    virtual const int64_t subsidy(unsigned int height, uint256 prev = uint256(0)) const = 0;
    virtual bool isStandard(const Transaction& tx) const = 0;
    virtual int nextWorkRequired(BlockIterator blk) const = 0;
    virtual const CBigNum proofOfWorkLimit() const = 0;
    virtual const int maxInterBlockTime() const { return INT_MAX; }
    virtual bool adhere_aux_pow() const { return false; }
    virtual bool adhere_names() const { return false; }
    virtual bool enforce_name_rules(int count) const { return false; }
    virtual const bool checkProofOfWork(const Block& block) const = 0;
    virtual bool checkPoints(const unsigned int height, const uint256& hash) const { return true; } // optional
    virtual unsigned int totalBlocksEstimate() const { return 0; } // optional
    
    virtual void check(const Transaction& txn) const;
    virtual void check(const Block& block) const;
    
    virtual const std::string dataDirSuffix() const { return name(); };
    
    virtual int timeStamp(ChangeIdentifier id) const {
        switch(id) {
            case BIP0016: return 1333238400; // April 1st                
            case BIP0030: return 1331769600; // 03/15/2012 12:00am GMT.
        }
        return 0;
    }
    
    virtual int maturity(int height) const {
        return 100;
    }
    
    virtual int64_t max_money() const {
        return 2100000000000000;
    }

    virtual size_t max_block_size() const {
        return 1000000;
    }
    
    virtual bool range(int64_t value) const {
        return (value >= 0 && value <= max_money());
    }
    
    virtual int64_t min_fee() const {
        return 0;
    }
    
    /// specially for namecoin
    virtual int64_t network_fee(int count) const {
        return 0;
    }
    
    virtual int expirationDepth(int count) const {
        return 0;
    }
    
    const std::set<std::string>& seeds() const {
        return _seeds;
    }
    
    // To enable upgrade from one block version to another we define a quorum and a majority for acceptance
    // of minimum this version as well as a quorum and majority for when the checks of that block version should be enforced.
    virtual size_t accept_quorum() const { return 1000;}
    virtual size_t accept_majority() const { return 950; } // 95% of the last 1000

    virtual size_t enforce_quorum() const { return 1000; }
    virtual size_t enforce_majority() const { return 750; } // 75% of the last 1000

    const PubKey& alert_key() const { return _alert_key; }
    
    //    virtual char networkId() const = 0; // 0x00, 0x6f, 0x34 (bitcoin, testnet, namecoin)
    virtual ChainAddress getAddress(PubKeyHash hash) const = 0;
    virtual ChainAddress getAddress(ScriptHash hash) const = 0;
    virtual ChainAddress getAddress(std::string str) const = 0;
    
    /// signedMessageMagic is for using the wallet keys to sign messages - you can overload "Bitcoin" in other chains or leave it as is.
    virtual std::string signedMessageMagic() const { return "LibCoin Signed Message:\n"; }
    
    virtual const MessageStart& messageStart() const = 0;
    virtual short defaultPort() const = 0;

    virtual std::string ircChannel() const { return name(); }
    virtual unsigned int ircChannels() const = 0; // number of groups to try (100 for bitcoin, 2 for litecoin)
    
    protected:
        PubKey _alert_key;
        std::set<std::string> _seeds;
};

class COINCHAIN_EXPORT BitcoinChain : public Chain {
public:
    BitcoinChain();
    virtual const int protocol_version() const { return 70002; } // 0.7.0.2
    virtual const Block& genesisBlock() const ;
    virtual const uint256& genesisHash() const { return _genesis; }
    virtual const int64_t subsidy(unsigned int height, uint256 prev = uint256(0)) const ;
    virtual bool isStandard(const Transaction& tx) const ;
    virtual const CBigNum proofOfWorkLimit() const { return CBigNum(~uint256(0) >> 32); }
    virtual int nextWorkRequired(BlockIterator blk) const;
    virtual const bool checkProofOfWork(const Block& block) const;
    virtual bool checkPoints(const unsigned int height, const uint256& hash) const ;
    virtual unsigned int totalBlocksEstimate() const;

    virtual int64_t min_fee() const {
        return 10000;
    }

    const PubKey& alert_key() const { return _alert_key; }
    
    //    virtual char networkId() const { return 0x00; } // 0x00, 0x6f, 0x34 (bitcoin, testnet, namecoin)
    virtual ChainAddress getAddress(PubKeyHash hash) const { return ChainAddress(0x00, hash); }
    virtual ChainAddress getAddress(ScriptHash hash) const { return ChainAddress(0x05, hash); }
    virtual ChainAddress getAddress(std::string str) const {
        ChainAddress addr(str);
        if(addr.version() == 0x00)
            addr.setType(ChainAddress::PUBKEYHASH);
        else if (addr.version() == 0x05)
            addr.setType(ChainAddress::SCRIPTHASH);
        return addr;
    }
    
    virtual std::string signedMessageMagic() const { return "Bitcoin Signed Message:\n"; }

    virtual const MessageStart& messageStart() const { return _messageStart; };
    virtual short defaultPort() const { return 8333; }
    
    virtual unsigned int ircChannels() const { return 100; } // number of groups to try (100 for bitcoin, 2 for litecoin)

private:
    Block _genesisBlock;
    uint256 _genesis;
    MessageStart _messageStart;
    typedef std::map<int, uint256> Checkpoints;
    Checkpoints _checkpoints;
};

extern const BitcoinChain bitcoin;

class COINCHAIN_EXPORT TestNet3Chain : public Chain
{
public:
    TestNet3Chain();
    virtual const int protocol_version() const { return 70002; } // 0.7.0.2
    virtual const Block& genesisBlock() const ;
    virtual const uint256& genesisHash() const { return _genesis; }
    virtual const int64_t subsidy(unsigned int height, uint256 prev = uint256(0)) const ;
    virtual bool isStandard(const Transaction& tx) const ;
    virtual const CBigNum proofOfWorkLimit() const { return CBigNum(~uint256(0) >> 32); }
    virtual const int maxInterBlockTime() const { return 2*10*60; }
    virtual const bool checkProofOfWork(const Block& block) const;
    virtual int nextWorkRequired(BlockIterator blk) const;
    //    virtual const bool checkProofOfWork(uint256 hash, unsigned int nBits) const;
    
    virtual int timeStamp(ChangeIdentifier id) const {
        switch(id) {
            case BIP0016: return 1329264000; // Feb 15
            case BIP0030: return 1329696000; // 02/20/2012 12:00am GMT
        }
        return 0;
    }
    
    virtual size_t accept_quorum() const { return 100;}
    virtual size_t accept_majority() const { return 75; } // 75% of the last 100
    
    virtual size_t enforce_quorum() const { return 100; }
    virtual size_t enforce_majority() const { return 51; } // 51% of the last 100
    

    //    virtual char networkId() const { return 0x6f; } // 0x00, 0x6f, 0x34 (bitcoin, testnet, namecoin)
    virtual ChainAddress getAddress(PubKeyHash hash) const { return ChainAddress(0x6f, hash); }
    virtual ChainAddress getAddress(ScriptHash hash) const { return ChainAddress(0xc4, hash); }
    virtual ChainAddress getAddress(std::string str) const {
        ChainAddress addr(str);
        if(addr.version() == 0x6f)
            addr.setType(ChainAddress::PUBKEYHASH);
        else if (addr.version() == 0xc4)
            addr.setType(ChainAddress::SCRIPTHASH);
        return addr;
    }

    virtual std::string signedMessageMagic() const { return "Bitcoin Signed Message:\n"; }

    virtual const MessageStart& messageStart() const { return _messageStart; };
    virtual short defaultPort() const { return 18333; }
    
    virtual std::string ircChannel() const { return "bitcoinTEST"; }
    virtual unsigned int ircChannels() const { return 1; } // number of groups to try (100 for bitcoin, 2 for litecoin)

private:
    Block _genesisBlock;
    uint256 _genesis;
    MessageStart _messageStart;
};

extern const TestNet3Chain testnet3;

class COINCHAIN_EXPORT NamecoinChain : public Chain {
public:
    NamecoinChain();
    virtual const int protocol_version() const { return 37200; } // 0.3.72.0
    virtual const Block& genesisBlock() const ;
    virtual const uint256& genesisHash() const { return _genesis; }
    virtual const int64_t subsidy(unsigned int height, uint256 prev = uint256(0)) const ;
    virtual bool isStandard(const Transaction& tx) const ;
    virtual const CBigNum proofOfWorkLimit() const { return CBigNum(~uint256(0) >> 32); }
    virtual int nextWorkRequired(BlockIterator blk) const;
    virtual const bool checkProofOfWork(const Block& block) const;
    virtual bool adhere_aux_pow() const { return true; }
    virtual bool adhere_names() const { return true; }
    virtual bool enforce_name_rules(int count) const { // see: https://bitcointalk.org/index.php?topic=310954
        if (count > 500000000)
            return count > 1386499470; // the timestamp of block 150000 - so will always return true...
        else
            return count > 150000; // enforce from block 150000
    }

    
    virtual bool checkPoints(const unsigned int height, const uint256& hash) const ;
    virtual unsigned int totalBlocksEstimate() const;
    
    virtual int64_t min_fee() const {
        return 500000;
    }
    
    virtual int64_t network_fee(int count) const {
        int height = count - 1;
        // Speed up network fee decrease 4x starting at 24000
        if (height >= 24000)
            height += (height - 24000) * 3;
        if ((height >> 13) >= 60)
            return 0;
        int64_t start = 50 * COIN;
        int64_t res = start >> (height >> 13);
        res -= (res >> 14) * (height % 8192);
        return res;
    }
    
    virtual int expirationDepth(int count) const {
        int height = count - 1;
        if (height < 24000)
            return 12000;
        if (height < 48000)
            return height - 12000;
        return 36000;
    }

    // To enable upgrade from one block version to another we define a quorum and a majority for acceptance
    // of minimum this version as well as a quorum and majority for when the checks of that block version should be enforced.
    virtual size_t accept_quorum() const { return 1000;}
    virtual size_t accept_majority() const { return 1001; } // 95% of the last 1000
    
    virtual size_t enforce_quorum() const { return 1000; }
    virtual size_t enforce_majority() const { return 1001; } // 75% of the last 1000
    
    const PubKey& alert_key() const { return _alert_key; }
    
    //    virtual char networkId() const { return 0x00; } // 0x00, 0x6f, 0x34 (bitcoin, testnet, namecoin)
    virtual ChainAddress getAddress(PubKeyHash hash) const { return ChainAddress(0x34, hash); }
    virtual ChainAddress getAddress(ScriptHash hash) const { throw std::runtime_error("ScriptHash not supported by Namecoin!"); }
    virtual ChainAddress getAddress(std::string str) const {
        ChainAddress addr(str);
        if(addr.version() == 0x34)
            addr.setType(ChainAddress::PUBKEYHASH);
        else
            throw std::runtime_error("ScriptHash not supported by Namecoin!");
        return addr;
    }
    
    virtual std::string signedMessageMagic() const { return "Namecoin Signed Message:\n"; }
    
    virtual const MessageStart& messageStart() const { return _messageStart; };
    virtual short defaultPort() const { return 8334; }
    
    virtual unsigned int ircChannels() const { return 1; } // number of groups to try (100 for bitcoin, 2 for litecoin)
    
private:
    Block _genesisBlock;
    uint256 _genesis;
    MessageStart _messageStart;
    typedef std::map<int, uint256> Checkpoints;
    Checkpoints _checkpoints;
};

extern const NamecoinChain namecoin;

        
class COINCHAIN_EXPORT LitecoinChain : public Chain {
public:
    LitecoinChain();
    virtual const int protocol_version() const { return 70002; } // 0.7.0.2
    virtual const Block& genesisBlock() const ;
    virtual const uint256& genesisHash() const { return _genesis; }
    virtual const int64_t subsidy(unsigned int height, uint256 prev = uint256(0)) const ;
    virtual bool isStandard(const Transaction& tx) const ;
    virtual const CBigNum proofOfWorkLimit() const { return CBigNum(~uint256(0) >> 20); } // Litecoin: starting difficulty is 1 / 2^12
    virtual const bool checkProofOfWork(const Block& block) const;
    virtual int nextWorkRequired(BlockIterator blk) const;
    //    virtual const bool checkProofOfWork(uint256 hash, unsigned int nBits) const;
    virtual bool checkPoints(const unsigned int height, const uint256& hash) const ;
    virtual unsigned int totalBlocksEstimate() const;
    
    virtual int timeStamp(ChangeIdentifier id) const {
        switch(id) {
            case BIP0016: return 1349049600; // April 1st
            case BIP0030: return 1349049600; // 03/15/2012 12:00am GMT.
        }
        return 0;
    }
    
    virtual int64_t min_fee() const {
        return 500000;//2000000; will ensure that txns above 1k get the old minimum fee of 0.02BTC
    }
    
    // To enable upgrade from one block version to another we define a quorum and a majority for acceptance
    // of minimum this version as well as a quorum and majority for when the checks of that block version should be enforced.
    virtual size_t accept_quorum() const { return 1000;}
    virtual size_t accept_majority() const { return 1001; } // 95% of the last 1000
    
    virtual size_t enforce_quorum() const { return 1000; }
    virtual size_t enforce_majority() const { return 1001; } // 75% of the last 1000
    
    //    virtual char networkId() const { return 0x00; } // 0x00, 0x6f, 0x34 (bitcoin, testnet, namecoin)
    virtual ChainAddress getAddress(PubKeyHash hash) const { return ChainAddress(0x30, hash); }
    virtual ChainAddress getAddress(ScriptHash hash) const { return ChainAddress(0x05, hash); }
    virtual ChainAddress getAddress(std::string str) const {
        ChainAddress addr(str);
        if(addr.version() == 0x30)
            addr.setType(ChainAddress::PUBKEYHASH);
        else if (addr.version() == 0x05)
            addr.setType(ChainAddress::SCRIPTHASH);
        return addr;
    }
    
    virtual std::string signedMessageMagic() const { return "Litecoin Signed Message:\n"; }

    virtual const MessageStart& messageStart() const { return _messageStart; };
    virtual short defaultPort() const { return 9333; }
    
    virtual unsigned int ircChannels() const { return 1; } // number of groups to try (100 for bitcoin, 2 for litecoin)

private:
    const uint256 getPoWHash(const Block& block) const;
private:
    Block _genesisBlock;
    uint256 _genesis;
    MessageStart _messageStart;
    typedef std::map<int, uint256> Checkpoints;
    Checkpoints _checkpoints;
};

extern const LitecoinChain litecoin;

class COINCHAIN_EXPORT TerracoinChain : public Chain
{
public:
    TerracoinChain();
    virtual const int protocol_version() const { return 70002; } // 0.7.0.2
    virtual const Block& genesisBlock() const ;
    virtual const uint256& genesisHash() const { return _genesis; }
    virtual const int64_t subsidy(unsigned int height, uint256 prev = uint256(0)) const ;
    virtual bool isStandard(const Transaction& tx) const ;
    virtual const CBigNum proofOfWorkLimit() const { return CBigNum(~uint256(0) >> 32); } 
    virtual const bool checkProofOfWork(const Block& block) const;
    virtual int nextWorkRequired(BlockIterator blk) const;
    //    virtual const bool checkProofOfWork(uint256 hash, unsigned int nBits) const;
    virtual bool checkPoints(const unsigned int height, const uint256& hash) const ;
    virtual unsigned int totalBlocksEstimate() const;
    
    virtual int timeStamp(ChangeIdentifier id) const {
        switch(id) {
            case BIP0016: return 1349049600; // April 1st
            case BIP0030: return 1349049600; // 03/15/2012 12:00am GMT.
        }
        return 0;
    }
    
    virtual int64_t max_money() const {
        return 8400000000000000;
    }

    //    virtual char networkId() const { return 0x00; } // 0x00, 0x6f, 0x34 (bitcoin, testnet, namecoin)
    virtual ChainAddress getAddress(PubKeyHash hash) const { return ChainAddress(0x30, hash); }
    virtual ChainAddress getAddress(ScriptHash hash) const { return ChainAddress(0x05, hash); }
    virtual ChainAddress getAddress(std::string str) const {
        ChainAddress addr(str);
        if(addr.version() == 0x30)
            addr.setType(ChainAddress::PUBKEYHASH);
        else if (addr.version() == 0x05)
            addr.setType(ChainAddress::SCRIPTHASH);
        return addr;
    }
    
    virtual const MessageStart& messageStart() const { return _messageStart; };
    virtual short defaultPort() const { return 9333; }
    
    virtual unsigned int ircChannels() const { return 1; } // number of groups to try (100 for bitcoin, 2 for litecoin)
    
private:
    const uint256 getPoWHash(const Block& block) const;
private:
    Block _genesisBlock;
    uint256 _genesis;
    MessageStart _messageStart;
    typedef std::map<int, uint256> Checkpoints;
    Checkpoints _checkpoints;
};

extern const TerracoinChain terracoin;

class COINCHAIN_EXPORT DogecoinChain : public Chain {
public:
    DogecoinChain();
    virtual const int protocol_version() const { return 70002; } // 0.7.0.2
    virtual const Block& genesisBlock() const ;
    virtual const uint256& genesisHash() const { return _genesis; }
    virtual const int64_t subsidy(unsigned int height, uint256 prev = uint256(0)) const ;
    virtual bool isStandard(const Transaction& tx) const ;
    virtual const CBigNum proofOfWorkLimit() const { return CBigNum(~uint256(0) >> 20); } // Litecoin: starting difficulty is 1 / 2^12
    virtual const bool checkProofOfWork(const Block& block) const;
    virtual int nextWorkRequired(BlockIterator blk) const;
    //    virtual const bool checkProofOfWork(uint256 hash, unsigned int nBits) const;
    virtual bool checkPoints(const unsigned int height, const uint256& hash) const ;
    virtual unsigned int totalBlocksEstimate() const;
    
    virtual int timeStamp(ChangeIdentifier id) const {
        switch(id) {
            case BIP0016: return 1349049600; // April 1st
            case BIP0030: return 1349049600; // 03/15/2012 12:00am GMT.
        }
        return 0;
    }
    
    virtual int maturity(int height) const {
        const int COINBASE_MATURITY = 30;
        /** Coinbase maturity after block 145000 **/
        const int COINBASE_MATURITY_NEW = 60*4;
        /** Block at which COINBASE_MATURITY_NEW comes into effect **/
        const int COINBASE_MATURITY_SWITCH = 145000;

        if (height >= COINBASE_MATURITY_SWITCH)
            return COINBASE_MATURITY_NEW;
        else
            return COINBASE_MATURITY;
    }
    
    virtual int64_t max_money() const {
        return 1000000000000000000; // actually wrong - max money is 100bill coins, but that is above maxuint64
    }

    virtual int64_t min_fee() const {
        return 100000000;//2000000; will ensure that txns above 1k get the old minimum fee of 0.02BTC
    }
    
    // To enable upgrade from one block version to another we define a quorum and a majority for acceptance
    // of minimum this version as well as a quorum and majority for when the checks of that block version should be enforced.
    virtual size_t accept_quorum() const { return 1000;}
    virtual size_t accept_majority() const { return 1001; } // 95% of the last 1000
    
    virtual size_t enforce_quorum() const { return 1000; }
    virtual size_t enforce_majority() const { return 1001; } // 75% of the last 1000
    
    //    virtual char networkId() const { return 0x00; } // 0x00, 0x6f, 0x34 (bitcoin, testnet, namecoin)
    virtual ChainAddress getAddress(PubKeyHash hash) const { return ChainAddress(0x1e, hash); }
    virtual ChainAddress getAddress(ScriptHash hash) const { return ChainAddress(0x16, hash); }
    virtual ChainAddress getAddress(std::string str) const {
        ChainAddress addr(str);
        if(addr.version() == 0x1e)
            addr.setType(ChainAddress::PUBKEYHASH);
        else if (addr.version() == 0x16)
            addr.setType(ChainAddress::SCRIPTHASH);
        return addr;
    }
    
    virtual std::string signedMessageMagic() const { return "Dogecoin Signed Message:\n"; }

    virtual const MessageStart& messageStart() const { return _messageStart; };
    virtual short defaultPort() const { return 22556; }
    
    virtual unsigned int ircChannels() const { return 1; } // number of groups to try (100 for bitcoin, 2 for litecoin)
    
private:
    const uint256 getPoWHash(const Block& block) const;
private:
    Block _genesisBlock;
    uint256 _genesis;
    MessageStart _messageStart;
    typedef std::map<int, uint256> Checkpoints;
    Checkpoints _checkpoints;
};

extern const DogecoinChain dogecoin;

extern const Currency ripplecredits;

#endif // CHAIN_H
