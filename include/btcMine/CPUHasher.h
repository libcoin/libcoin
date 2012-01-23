
#ifndef _CPUHASHER_H_
#define _CPUHASHER_H_

#include "btcMine/Miner.h"

class CPUHasher : public Miner::Hasher {
public:
    virtual bool operator()(Block& block, unsigned int nonces);
    
    /// Description - optional
    virtual const std::string description() const { 
        return
        "The CPU Hasher is the hasher also part of the original Satoshi client.\n"
        "It slow compared to more recent GPU miners and is hence supplied mainly\n"
        "for informational and educational purposes. E.g. if you want to setup\n"
        "your own coin chain in a class room.\n";
    }
    
    /// Return true if the hashing algorithm is supported on this platform, false otherwise - mandatory.
    virtual const bool supported() const { return true; }; // No special requirements, hence always supported
};

#endif // _CPUHASHER_H_
