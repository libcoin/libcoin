#ifndef _MINERPC_H_
#define _MINERPC_H_

#include "btcMine/Miner.h"
#include "btcHTTP/Method.h"

/// Base class for all Mining rpc methods - they all need a handle to the Miner.
class MineMethod : public Method {
public:
    MineMethod(Miner& miner) : _miner(miner) {}
protected:
    Miner& _miner;
};

class SetGenerate : public MineMethod {
public:
    SetGenerate(Miner& miner) : MineMethod(miner) {}
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);
};

class GetGenerate : public MineMethod {
public:
    GetGenerate(Miner& miner) : MineMethod(miner) {}
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);
};

class GetHashesPerSec : public MineMethod {
public:
    GetHashesPerSec(Miner& miner) : MineMethod(miner) {}
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);
};

#endif // _MINERPC_H_