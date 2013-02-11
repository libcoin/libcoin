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

#ifndef _MERKLETRIE_H
#define _MERKLETRIE_H

#include <coinChain/Referenced.h>
#include <coinChain/ref_ptr.h>

#include <boost/lexical_cast.hpp>
#include <boost/pool/singleton_pool.hpp>

#include <iostream>
#include <vector>
#include <set>
#include <string>
#include <memory>
#include <stdexcept>

/// Singleton pool to speed up allocation of small objects
template <class T>
class MemoryPool : public boost::singleton_pool<T, sizeof(T)> {};

class MerkleTrieBase {
public:
    MerkleTrieBase() { reset_timers(); }
    
    void reset_timers() const {
        _hashing_timer = 0;
        _find_timer = 0;
        _insert_timer = 0;
        _remove_timer = 0;
    }
    
    int64_t time_microseconds() const;
    std::string statistics() const;
    
protected:
    mutable int64_t _hashing_timer;
    mutable int64_t _find_timer;
    mutable int64_t _insert_timer;
    mutable int64_t _remove_timer;
};

template <class Elem, class Key = typename Elem::Key, class Hash = typename Elem::Hash>
class MerkleTrie : public MerkleTrieBase {
public:
    class Visitor;
    typedef typename Elem::KeyWidth KeyWidth;
private:
    struct Node : public Referenced {
        Node() {}
        Node(const Node&) {}
        virtual Node* clone() = 0;
        virtual void accept(Visitor& v);
        virtual int compare(Key k) const = 0;
        virtual int depth() const = 0;
        virtual Key key() const = 0;
        virtual void rehash() = 0;
        virtual Hash hash() const = 0;
    };
    struct EndNode : Node {
        EndNode() : Node() {}
        EndNode(const EndNode& node) : Node(node) {}
        Node* clone() { return this; }
        void accept(Visitor& v) {}
        int compare(Key k) const { return 100000; }
        int depth() const { return 0; }
        Key key() const { return Key(); }
        void rehash() {}
        Hash hash() const { return Hash(0); }
    };
    struct BranchNode : Node {
        BranchNode(BranchNode& node) : Node(node), left(node.left.get()), right(node.right.get()), _key(node._key), _depth(node._depth), _hash(node._hash) {}
        Node* clone() { return new BranchNode(*this); }
        /// use custom new and delete to ensure we have
        //void* operator new(size_t size) { return Pool<BranchNode>::malloc(); }
        //void operator delete(void* buffer) { Pool<BranchNode>::free(buffer); }
        virtual void accept(Visitor& v);
        BranchNode(Node* l, Node* r, KeyWidth d) : left(l), right(r), _key(r->key()), _depth(d) { /* if (l && r) rehash(); */}
        int compare(Key k) const { return Elem::compare(_key, k); }
        int depth() const { return _depth; }
        Key key() const { return _key; }
        Hash hash() const { return _hash; }
        void rehash() { if (left.valid() && right.valid()) _hash = Elem::hash(left->hash(), right->hash()); }
        ref_ptr<Node> left;
        ref_ptr<Node> right;
        Key _key; // we store the entire key, though we only compare with a small part of it
        KeyWidth _depth;
        Hash _hash;
    };
    struct LeafNode : Node {
        LeafNode(const LeafNode& node) : Node(node) {}
        Node* clone() { return this; } // no need to clone
        //void* operator new(size_t size) { return Pool<LeafNode>::malloc(); }
        //void operator delete(void* buffer) { Pool<LeafNode>::free(buffer); }
        virtual void accept(Visitor& v);
        LeafNode(Elem e) : elem(e) {}
        int compare(Key k) const { return Elem::compare(elem.key, k); }
        int depth() const { return 0; }
        Key key() const { return elem.key; }
        Hash hash() const { return elem.hash(); }
        void rehash() {}
        Elem elem;
    };
public:
    /// A MerkleTrie::Branch is a branch of the merkle trie suitable for serialization
    /// It contains a list of hashes from a specific object (incl the object) down to the hash root
    
    class Branch {
    public:
        enum Type {
            Root,
            Leaf,
            Left,
            Right
        };
        
        typedef std::pair<Type, Hash> TypeOfHash;
        
        void push(Type type, Hash hash) {
            _hashes.push_back(TypeOfHash(type, hash));
        }
        
        bool validate(Elem elem) const {
            return validate(elem.hash());
        }

        bool validate(Hash hash) const {
            typename std::vector<TypeOfHash>::const_reverse_iterator th = _hashes.rbegin();

            // check that the hash matches the last hash - i.e. the hash of the element
            if (hash != th->second)
                return false;

            while (++th != _hashes.rend()) {
                if (th->first == Left)
                    hash = Elem::hash(th->second, hash);
                else if (th->first == Right)
                    hash = Elem::hash(hash, th->second);
                else
                    break;
            }
            
            return (hash == root());
        }
                            
        const Hash& root() const {
            return _hashes.begin()->second;
        }
        
        static bool consecutive(const Branch& lower, const Branch& upper) {
            // walk down hash path together
            size_t i;
            for (i = 0; i < std::min(lower._hashes.size(), upper._hashes.size()); ++i) {
                if (lower._hashes[i] != upper._hashes[i])
                    break;
            }
            
            if (i == std::min(lower._hashes.size(), upper._hashes.size()))
                return false;
            
            // i is now the branching point - from here lower need to go left and then right all the way to the leaf, upper right and then left
            
            if (lower._hashes[i].first != Left)
                return false;
            for (int j = i+1; j < lower._hashes.size(); ++j) {
                if (lower._hashes[j].first == Leaf)
                    break;
                if (lower._hashes[j].first != Right)
                    return false;
            }
            
            if (upper._hashes[i].first != Right)
                return false;
            for (int j = i+1; j < lower._hashes.size(); ++j) {
                if (upper._hashes[j].first == Leaf)
                    break;
                if (upper._hashes[j].first != Left)
                    return false;
            }
            
            return true;
        }

    private:
        std::vector<TypeOfHash> _hashes;
    };
    
    class Iterator {
    public:
        typedef std::vector<Node*> Path;
        Iterator(const Path& path) : _path(path) { }
        Iterator(const Iterator& it) : _path(it._path) { }
        Iterator() {} // == end()
        
        const Elem* operator->() {
            LeafNode* leaf = dynamic_cast<LeafNode*>(_path.back());
            if (!leaf)
                throw std::runtime_error("Can only dereference leaf nodes!");
            else
                return &(leaf->elem);
        }
        const Elem& operator*() {
            LeafNode* leaf = dynamic_cast<LeafNode*>(_path.back());
            if (!leaf)
                throw std::runtime_error("Can only dereference leaf nodes!");
                else
                    return leaf->elem;
        }
        // If an iterator is ==end() you can sometimes take the plus value of it, meaning the nearest element greater than
        Iterator operator+() const {
            // if the operator is already a leaf - return it self
            if (is_leaf())
                return Iterator(*this);
            else if (is_branch()) {
                Path path = _path;
                BranchNode* b = dynamic_cast<BranchNode*>(path.back());
                path.push_back(b->left.get());
                b = dynamic_cast<BranchNode*>(path.back());
                while (b) {
                    path.push_back(b->right.get());
                    b = dynamic_cast<BranchNode*>(path.back());
                }
                return Iterator(path);
            }
            else
                return Iterator(); // end()
        }
        
        // If an iterator is ==end() you can sometimes take the minus value of it, meaning the nearest element smaller than
        Iterator operator-() const {
            // if the operator is already a leaf - return it self
            if (is_leaf())
                return Iterator(*this);
            else if (is_branch()) {
                Path path = _path;
                BranchNode* b = dynamic_cast<BranchNode*>(path.back());
                path.push_back(b->right.get());
                b = dynamic_cast<BranchNode*>(path.back());
                while (b) {
                    path.push_back(b->left.get());
                    b = dynamic_cast<BranchNode*>(path.back());
                }
                return Iterator(path);
            }
            else
                return Iterator(); // end()
        }
        
        //        Iterator& operator++() { ; return *this; }
        //        Iterator operator++(int) { return ++(*this); }

        
        bool operator==(const Iterator& rhs) const {
            return _path == rhs._path;
        }
        
        bool operator!=(const Iterator& rhs) const {
            return _path != rhs._path;
        }
        
        bool operator!() const {
            return !is_leaf();
        }
        
        Path path() { return _path; }
        
        Branch branch() const {
            Branch b;
            
            b.push(Branch::Root, _path[1]->hash());

            for (size_t i = 2; i < _path.size(); ++i) {
                const BranchNode* bn = dynamic_cast<const BranchNode*>(_path[i-1]);
                if (bn->right.get() == _path[i])
                    b.push(Branch::Left, bn->left->hash());
                else
                    b.push(Branch::Right, bn->right->hash());
            }

            b.push(Branch::Leaf, _path.back()->hash());

            return b;
        }

    private:
        bool is_leaf() const {
            return _path.size() && dynamic_cast<const LeafNode*>(_path.back());
        }
        bool is_branch() const {
            return _path.size() && dynamic_cast<const BranchNode*>(_path.back());
        }
    private:
        Path _path;
    };
    
    class Visitor {
    public:
        virtual void apply(Node& node) { node.accept(*this); }
        virtual void apply(BranchNode& node) = 0;
        virtual void apply(LeafNode& node) = 0;
        
        void push(Node* node) { _path.push_back(node); }
        void pop() { _path.pop_back(); }
        typename Iterator::Path& path() { return _path; }
        Node* parent() { return _path.back(); }
    private:
        typename Iterator::Path _path;
    };
    class FindVisitor : public Visitor {
    public:
        enum Mode {
            Find, // default mode - will return the exact value or end()
            Lower, // will return the value or the nearest one below it
            Upper // will return the value or the nearest one above it
        };
        FindVisitor(Key key, Mode mode = Find) : _key(key), _mode(mode) {}
        virtual void apply(LeafNode& node) {
            if (node.compare(_key) == 0)
                _found = Iterator(Visitor::path());
        }
        virtual void apply(BranchNode& node) {
            if (abs(node.left->compare(_key)) <= node.left->depth())
                node.left->accept(*this);
            else if (abs(node.right->compare(_key)) <= node.right->depth())
                node.right->accept(*this);
            else // not found
                _found = Iterator(Visitor::path());
        }
        Iterator result() const { return _found; }
        
    protected:
        Key _key;
        Mode _mode;
        Iterator _found;
    };
    /// InsertVisitor - traverse the trie and insert the LeafNode at the right spot or replace an existing LeafNode
    class InsertVisitor : public Visitor {
    public:
        InsertVisitor(Elem elem, bool do_hash = true) : _elem(elem), _dirty(false), _do_hash(do_hash) {}
        virtual void apply(LeafNode& node) {
        }
        virtual void apply(BranchNode& node) {
            int lcmp = node.left->compare(_elem.key);
            int rcmp = node.right->compare(_elem.key);
            if (abs(lcmp) <= node.left->depth()) {
                if (!node.left->unique())
                    node.left = dynamic_cast<BranchNode*>(node.left->clone());
                node.left->accept(*this);
            }
            else if (abs(rcmp) <= node.right->depth()) {
                if (!node.right->unique())
                    node.right = dynamic_cast<BranchNode*>(node.right->clone());
                node.right->accept(*this);
            }
            else { // cannot descent - insert branch and leaf such that left < right
                if (abs(lcmp) < abs(rcmp)) { // continue to left
                    if (lcmp > 0) // (left=new BranchNode(new Leaf, left, abs(lcmp)))
                        node.left = new BranchNode(new LeafNode(_elem), node.left.get(), abs(lcmp));
                    else if (lcmp < 0)
                        node.left = new BranchNode(node.left.get(), new LeafNode(_elem), abs(lcmp));
                    else
                        node.left = new LeafNode(_elem);
                }
                else {
                    if (rcmp > 0)
                        node.right = new BranchNode(new LeafNode(_elem), node.right.get(), abs(rcmp));
                    else if (rcmp < 0)
                        node.right = new BranchNode(node.right.get(), new LeafNode(_elem), abs(rcmp));
                    else
                        node.right = new LeafNode(_elem);
                }
                _dirty = true;
            }
            if (_do_hash && _dirty)
                node.rehash();
        }
        bool dirty() const { return _dirty; }
    private:
        Elem _elem;
        bool _dirty;
        bool _do_hash;
    };
    class RemoveVisitor : public Visitor {
    public:
        RemoveVisitor(Key key, bool do_hash = true) : _key(key), _found(false), _dirty(false), _do_hash(do_hash), _removed(false) {}
        virtual void apply(LeafNode& node) {
            if (node.compare(_key) == 0)
                _found = true;
        }
        virtual void apply(BranchNode& node) {
            if (abs(node.left->compare(_key)) <= node.left->depth()) {
                node.left->accept(*this);
                if (_shunt.valid()) {
                    node.left = _shunt.get();
                    _shunt = NULL;
                    _dirty = true;
                }
                if (_found) {
                    _found = false;
                    _shunt = node.right.get();
                    _removed = true;
                }
            }
            else if (abs(node.right->compare(_key)) <= node.right->depth()) {
                node.right->accept(*this);
                if (_shunt.valid()) {
                    node.right = _shunt.get();
                    _shunt = NULL;
                    _dirty = true;
                }
                if (_found) {
                    _found = false;
                    _shunt = node.left.get();
                    _removed = true;
                }
            }
            if (_do_hash && _dirty)
                node.rehash();
        }
        bool removed() const { return _removed; }
    protected:
        Key _key;
        bool _found;
        bool _dirty;
        bool _do_hash;
        bool _removed;
        ref_ptr<Node> _shunt;
    };
    
    class HashVisitor : public Visitor {
    public:
        void apply(LeafNode& node) {
        }
        void apply(BranchNode& node) {
            node.left->accept(*this);
            node.right->accept(*this);
            node.rehash();
        }
    };
    class GraphVisitor : public Visitor {
    public:
        void apply(LeafNode& node) {
            if (Visitor::path().size() > 1) {
                const Node* parent = Visitor::path()[Visitor::path().size()-2];
                _nodes += "\"" + boost::lexical_cast<std::string>(&node) + "\"" + " [shape=box,label=\"" + node.elem.value + "\"];\n";
                std::string label = "";//Elem::key_str(node.key(), parent->depth(), 0);
                _edges += "\"" + boost::lexical_cast<std::string>(parent) + "\"" + " -> " + "\"" + boost::lexical_cast<std::string>(&node) + "\"" + " [label=\"" + label + "\"];\n";
            }
        }
        void apply(BranchNode& node) {
            _nodes += "\"" + boost::lexical_cast<std::string>(&node) + "\"" + " [shape=oval,label=\"" + boost::lexical_cast<std::string>(node.depth()) + "\"];\n";
            if (Visitor::path().size() > 1) {
                const Node* parent = Visitor::path()[Visitor::path().size()-2];
                std::string label = Elem::key_str(node.key(), node.depth(), parent->depth());
                _edges += "\"" + boost::lexical_cast<std::string>(parent) + "\"" + " -> " + "\"" + boost::lexical_cast<std::string>(&node) + "\"" + " [label=\"" + label + "\"];\n";
            }
            node.left->accept(*this);
            node.right->accept(*this);
        }
        const std::string str() const {
            return
            "digraph G {\n" +
            _edges +
            _nodes +
            "}\n";
        }
    private:
        std::string _nodes;
        std::string _edges;
    };
    
    MerkleTrie() : MerkleTrieBase(), _root(NULL), _genitor(new BranchNode(_root, new EndNode, -1)), _authenticated(false) {
    }
    
    /// Copy constructor - shallow copy - the genitor is new, but points to the same root increasing the reference count of root by 1.
    MerkleTrie(const MerkleTrie& trie) : MerkleTrieBase(), _root(trie._root), _genitor(new BranchNode(_root, new EndNode, -1)), _authenticated(trie._authenticated) {
    }
    
    /// Assignment operator, note the signature is by value as copy construction is a cheap operation
    MerkleTrie& operator=(const MerkleTrie& rhs) {
        if (&rhs == this) return *this;
        _root = rhs._root;
        _genitor = new BranchNode(_root, new EndNode, -1);
        _authenticated = rhs._authenticated;
        return *this;
    }
    
    Iterator begin() const {
        typename Iterator::Path path;
        Node* node = _root;
        while (node) {
            path.push_back(node);
            BranchNode* branch = dynamic_cast<BranchNode*>(node);
            if (branch) {
                node = branch->left.get();
            }
            else {
                break;
            }
        }
        return Iterator(path);
    }
    
    Iterator end() const {
        return Iterator();
    }
    
    Iterator find(Key key) const {
        _find_timer -= now();
        FindVisitor fv(key);
        if (!_root)
            return end();
        _genitor->accept(fv);
        _find_timer += now();
        return fv.result();
    }

    /*
    bool insert(Elem elem) {
        bool inserted = true;
        if (!_root)
            _genitor->left = new LeafNode(elem);
        else {
            InsertVisitor iv(elem, _authenticated);
            _genitor->accept(iv);
            inserted = iv.dirty();
        }
        _root = _genitor->left.get();
        return inserted;
    }
     */
    
    bool insert(Elem elem) {
        if (!_root)
            _genitor->left = new LeafNode(elem);
        else {
            // check if key already exist
            FindVisitor fv(elem.key);
            _genitor->accept(fv);
            
            Iterator f = fv.result();

            typename Iterator::Path path = f.path();
            
            if (dynamic_cast<LeafNode*>(path.back()))
                return false; // already exists!
            
            _insert_timer -= now();
            
            // ensure path is unique
            for (size_t i = 1; i < path.size(); ++i)
                if (!path[i]->unique()) {
                    BranchNode* node = dynamic_cast<BranchNode*>(path[i-1]);
                    if (node->left.get() == path[i]) {
                        node->left = dynamic_cast<BranchNode*>(node->left->clone());
                        path[i] = node->left.get();
                    }
                    else {
                        node->right = dynamic_cast<BranchNode*>(node->right->clone());
                        path[i] = node->right.get();
                    }
                }
            
            // insert
            BranchNode* node = dynamic_cast<BranchNode*>(path.back());
            int lcmp = node->left->compare(elem.key);
            int rcmp = node->right->compare(elem.key);
            
            if (abs(lcmp) < abs(rcmp)) { // continue to left
                if (lcmp > 0) // (left=new BranchNode(new Leaf, left, abs(lcmp)))
                    node->left = new BranchNode(new LeafNode(elem), node->left.get(), abs(lcmp));
                else if (lcmp < 0)
                    node->left = new BranchNode(node->left.get(), new LeafNode(elem), abs(lcmp));
                else
                    node->left = new LeafNode(elem);
                path.push_back(node->left.get());
            }
            else {
                if (rcmp > 0)
                    node->right = new BranchNode(new LeafNode(elem), node->right.get(), abs(rcmp));
                else if (rcmp < 0)
                    node->right = new BranchNode(node->right.get(), new LeafNode(elem), abs(rcmp));
                else
                    node->right = new LeafNode(elem);
                path.push_back(node->right.get());
            }
            _insert_timer += now();
            // rehash
            _hashing_timer -= now();
            if (_authenticated)
                for (typename Iterator::Path::reverse_iterator p = path.rbegin(); p != path.rend(); ++p)
                    (*p)->rehash();
            _hashing_timer += now();
        }
        _root = _genitor->left.get();
        return true;
    }
    
    bool remove(Key key) {
        Iterator f = find(key);
        if (!f)
            return false;
        
        remove(f);
        return true;
    }

    void remove(Iterator f) {
        _remove_timer -= now();
        if (!f)
            throw std::runtime_error("Trying to remove nonexisting element!");
        
        typename Iterator::Path path = f.path();
        // ensure path is unique
        for (size_t i = 1; i < path.size() - 1; ++i)
            if (!path[i]->unique()) {
                BranchNode* node = dynamic_cast<BranchNode*>(path[i-1]);
                if (node->left.get() == path[i]) {
                    node->left = dynamic_cast<BranchNode*>(node->left->clone());
                    path[i] = node->left.get();
                }
                else {
                    node->right = dynamic_cast<BranchNode*>(node->right->clone());
                    path[i] = node->right.get();
                }
            }
        
        // remove - we do this by removing back and back-1 and "shunting" the back sibling with the back-1 sibling in back-2
        if (path.size() < 3) { // there is only one element in the trie
            dynamic_cast<BranchNode*>(path[0])->left = NULL;
            path.resize(path.size()-1);
        }
        else {
            BranchNode* parent = dynamic_cast<BranchNode*>(path[path.size()-3]);
            BranchNode* shunt = dynamic_cast<BranchNode*>(path[path.size()-2]);
            Node* child;
            if (shunt->left.get() == path.back())
                child = shunt->right.get();
            else
                child = shunt->left.get();
            
            if (parent->left.get() == shunt)
                parent->left = child;
            else
                parent->right = child;
            
            // remove the child and the shunt from the path
            path.resize(path.size()-2);
        }

        _remove_timer += now();
        
        // rehash
        _hashing_timer -= now();
        if (_authenticated)
            for (typename Iterator::Path::reverse_iterator p = path.rbegin(); p != path.rend(); ++p)
                (*p)->rehash();
        _hashing_timer += now();

        _root = _genitor->left.get();
    }
    
    bool is_authenticated() const {
        return _authenticated;
    }
    
    void authenticated(bool a) {
        if (_authenticated == a)
            return;
        
        _authenticated = a;
        if (_authenticated)
            rehash();
    }
    
    void rehash() {
        if (!_root) return;
        _hashing_timer -= now();
        HashVisitor hv;
        _root->accept(hv);
        _hashing_timer += now();
    }
    
    Node* root() const { return _root; }
    
    std::string graph() {
        GraphVisitor grapher;
        _root->accept(grapher);
        return  grapher.str();
    }

    inline int64_t now() const { return time_microseconds(); }
    
private:
    mutable Node* _root;
    mutable ref_ptr<BranchNode> _genitor;
    bool _authenticated;
};

template <class Elem, class Key, class Hash>
void MerkleTrie<Elem, Key, Hash>::Node::accept(MerkleTrie::Visitor &v) {
    v.push(this);
    v.apply(*this);
    v.pop();
}

template <class Elem, class Key, class Hash>
void MerkleTrie<Elem, Key, Hash>::BranchNode::accept(MerkleTrie::Visitor &v) {
    v.push(this);
    v.apply(*this);
    v.pop();
}

template <class Elem, class Key, class Hash>
void MerkleTrie<Elem, Key, Hash>::LeafNode::accept(MerkleTrie::Visitor &v) {
    v.push(this);
    v.apply(*this);
    v.pop();
}

#endif // _MERKLETRIE_H
