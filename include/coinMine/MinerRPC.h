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

#ifndef _MINERPC_H_
#define _MINERPC_H_

#include "coinMine/Miner.h"
#include "coinHTTP/Method.h"

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