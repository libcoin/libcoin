#ifndef _NODERPC_H_
#define _NODERPC_H_

#include "btcNode/Node.h"
#include "btcHTTP/Method.h"

/// Base class for all Node rpc methods - they all need a handle to the node.
class NodeMethod : public Method {
public:
    NodeMethod(Node& node) : _node(node) {}
protected:
    Node& _node;
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
protected:
    double getDifficulty();
};

class GetInfo : public GetDifficulty {
public:
    GetInfo(Node& node) : GetDifficulty(node) {}
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);
};

#endif // _NODERPC_H_