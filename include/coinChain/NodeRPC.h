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

#ifndef _NODERPC_H_
#define _NODERPC_H_

#include "coinChain/Node.h"
#include "coinHTTP/Method.h"

/// Base class for all Node rpc methods - they all need a handle to the node.
class NodeMethod : public Method {
public:
    NodeMethod(Node& node) : _node(node) {}
protected:
    Node& _node;
};


class GetBlockHash : public NodeMethod {
public:
    GetBlockHash(Node& node) : NodeMethod(node) {}
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);    
};

class GetBlock : public NodeMethod {
public:
    GetBlock(Node& node) : NodeMethod(node) {}
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);    
};

json_spirit::Object tx2json(Transaction &tx, int64 timestamp = 0, int64 blockheight = 0);

class GetTransaction : public NodeMethod {
public:
    GetTransaction(Node& node) : NodeMethod(node) {}
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);    
};

class GetBlockCount : public NodeMethod {
public:
    GetBlockCount(Node& node) : NodeMethod(node) {}
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);
};

class GetConnectionCount : public NodeMethod {
public:
    GetConnectionCount(Node& node) : NodeMethod(node) {}
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);
};

class GetDifficulty : public NodeMethod {
public:
    GetDifficulty(Node& node) : NodeMethod(node) {}
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);
};

class GetInfo : public NodeMethod {
public:
    GetInfo(Node& node) : NodeMethod(node) {}
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);
};

#endif // _NODERPC_H_