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

#ifndef BLOCKTREE_H
#define BLOCKTREE_H

#include <coin/BigNum.h>

#include <coinChain/Export.h>

/// SparseTree and its Iterator is used to store and traverse a tree structure.
/// It is optimised for a sparse tree - i.e. a tree with a well defined and long trunk and only
/// few short branches. The trunk is held in a vector, and the branches are held in a map.
/// It is assumed that the tree stores elements, Elem, that can be uniquely determined by their
/// hash - the hash can be e.g. a number or a hash.
/// Each element can be associated with a height - the height is returned as a signed height - a positive height indicates
/// that is belongs to the trunk. A negative height that it is associated with a branch.
/// The height can be found using height(hash) the trunk height is returned using height().
/// The tree can be traversed using an iterator. Note that stepping forward and backwards is only associative on the main branch.
/// In fact forward stepping is only supported on the main branch.
/// The comparison operators < and > can be used to determine if the elements are in the same branch:
/// a < b returns true if a is below b and in the same branch - in other words, if you can step down from b to a e.g. using --
/// As SparseTree is a sparse tree it supports trimming. Trimming the tree is done using trim(height).
/// This removes all branches ending below this height. A set of hashes of removed elements is hereby returned.
/// The Elements must satisfy the have public members "hash" and "prev", prev is pointing to the previous element in the tree

template <class Elem, class Hash = typename Elem::Hash>
class SparseTree {
public:
    class Error: public std::runtime_error {
    public:
        Error(const std::string& s) : std::runtime_error(s.c_str()) {}
    };

    typedef std::vector<Hash> Hashes;

    class Iterator {
    public:
        Iterator() : _elem(NULL), _tree(NULL) {}
        Iterator(const Iterator& i) : _elem(i._elem), _tree(i._tree) {}
        Iterator(const Elem* elem, const SparseTree* tree) : _elem(elem), _tree(tree) {}

        Iterator& operator++() { _tree->advance(*this, 1); return *this; }
        Iterator operator++(int) { return ++(*this); }
        Iterator& operator+=(int n) { _tree->advance(*this, n); return *this; }
        Iterator operator+(int n) const {
            return Iterator(*this) += n;
        }

        Iterator& operator--() { _tree->advance(*this, -1); return *this; }
        Iterator operator--(int) { return --(*this); }
        Iterator& operator-=(int n) { _tree->advance(*this, -n); return *this; }
        Iterator operator-(int n) const {
            return Iterator(*this) -= n;
        }
        
        bool operator==(const Iterator& rhs) const {
            return _elem == rhs._elem && _tree == rhs._tree;
        }
        
        bool operator!=(const Iterator& rhs) const {
            return !operator==(rhs);
        }
        
        const Elem* operator->() { return _elem; }
        const Elem& operator*() { return *_elem; }
        
        int height() const { return _tree->height(_elem->hash); };
        int count() const {
            if (is_valid())
                return _tree->count(_elem->hash);
            else
                return 0;
        }
        
        bool operator!() const { return !is_valid(); }
        
        bool operator<(Iterator rhs) {
            if (!is_valid() || !rhs.is_valid()) return false;

            int lheight = height();
            int rheight = rhs.height();
            if (abs(lheight) > abs(rheight)) return false;
            if (lheight < 0 && rheight > 0) return false;
            if (lheight > 0 && rheight > 0) return true; // we already checked that the absolute value of r is larger than l
            // only case left is: r<0 - crawl backwards till we hit l or become positive:
            for(;;) {
                if (--rhs == _tree->end()) return false;
                if (rhs == *this) return true;
                rheight = rhs.height();
                if (rheight >=0 ) {
                    if (lheight < 0)
                        return false;
                    else
                        return (lheight < rheight);
                }
            }
        }
        
        Iterator ancestor(Hashes hashes) const {
            for (typename Hashes::const_iterator hash = hashes.begin(); hash != hashes.end(); ++hash) {
                Iterator itr = _tree->find(*hash);
                if (itr < *this) return itr;
            }
            return _tree->end();
        }
        
    private:
        bool is_valid() const { return _elem != NULL && _tree != NULL; }

    private:
        const Elem* _elem;
        const SparseTree* _tree;
    };
    
    typedef std::vector<Elem> Trunk;
    virtual void assign(const Trunk& trunk) {
        if (trunk.size() == 0) return;
        
        _trunk = trunk;

        _heights[_trunk[0].hash] = 0;
        
        for (size_t h = 1; h < _trunk.size(); ++h) {
            // validate:
            if (_trunk[h].prev != _trunk[h-1].hash)
                throw Error("Tree::assign - failed to validate trunk!");
            
            // populate the hash index
            _heights[_trunk[h].hash] = h;
        }
        
        _branches.clear();
    }

    int height() const {
        return _heights.size() - 1;
    }
    int count() const {
        return _heights.size();
    }
    int height(const Hash& hash) const {
        typename Heights::const_iterator h = _heights.find(hash);
        if (h == _heights.end())
            throw Error("SparseTree::height - hash not known");
        
        return h->second;
    }
    int count(const Hash& hash) const {
        int h = height(hash);
        return abs(h) + 1;
    }
    
    bool have(const Hash& hash) const { return _heights.count(hash); }
    
    Iterator find(const Hash hash) const {
        typename Heights::const_iterator h = _heights.find(hash);
        if (h == _heights.end())
            return end();

        if (h->second < 0) { // the iterator points to the stash
            typename Branches::const_iterator i = _branches.find(hash);
            return Iterator(&(i->second), this);
        }
        else
            return Iterator(&(_trunk[h->second]), this);
    }

    const Iterator begin() const {
        if (_trunk.size() == 0)
            return end();
        return Iterator(&(_trunk[0]), this);
    }
    
    const Iterator end() const {
        return Iterator();
    }


    const Elem& operator[](Hash hash) const {
        return at(hash);
    }

    const Elem& at(Hash hash) const {
        int h = height(hash);
        if (h < 0)
            return _branches.find(hash)->second;
        else
            return _trunk[h];
    }

    const int distance(const Iterator& a, const Iterator& b) const {
        
    }
    
    void advance(Iterator& i, int n) const {
        if (i == end()) return;
        if (n > 0) { // only forward movement inside the trunk makes sense
            int h = i.height();
            if (h < 0)
                i = end();
            else {
                h += n;
                if (h >= _trunk.size())
                    i = end();
                else
                    i = Iterator(&_trunk[h], this);
            }
        }
        else if (n < 0) {
            while (n < 0 && i != end() && i.height() < 0) {
                i = Iterator(&(_branches.find(i->hash)->second), this);
                ++n;
            }
            int h = i.height() + n;
            
            if (h >= 0 && i != end())
                i = Iterator(&_trunk[h], this);
            else
                i = end();
        }
    }
    
protected:
    typedef std::map<Hash, Elem> Branches;
    Branches _branches;
    
    typedef std::map<Hash, int> Heights;
    Heights _heights;
    
    Trunk _trunk;
};

struct BlockRef {
    typedef uint256 Hash;
    Hash hash;
    Hash prev;
    unsigned int time;
    unsigned int bits;
    
    CBigNum work() const {
        CBigNum target;
        target.SetCompact(bits);
        if (target <= 0)
            return 0;
        return (CBigNum(1)<<256) / (target+1);
    }
    
    BlockRef() : hash(0), prev(0), time(0), bits(0) {}
    BlockRef(Hash hash, Hash prev, unsigned int time, unsigned int bits) : hash(hash), prev(prev), time(time), bits(bits) {}
};

class COINCHAIN_EXPORT BlockTree : public SparseTree<BlockRef> {
public:
    BlockTree() : SparseTree<BlockRef>(), _trimmed(0) {}
    virtual void assign(const Trunk& trunk);
    
    struct Changes {
        Hashes deleted;
        Hashes inserted;
    };

    struct Pruned {
        Hashes branches;
        Hashes trunk;
    };
    
    CBigNum accumulatedWork(BlockTree::Iterator blk) const;
    
    Changes insert(const BlockRef ref);

    void pop_back();

    Iterator find(const BlockRef::Hash hash) const {
        return SparseTree<BlockRef>::find(hash);
    }
    
    Iterator best() const {
        return Iterator(&_trunk.back(), this);
    }
    
    /// Trim the trunk, removing all branches ending below the trim_height
    Pruned trim(size_t trim_height) {
        // ignore branches for now...
        if (trim_height > height())
            trim_height = height();
        Pruned pruned;
        for (size_t h = _trimmed; h < trim_height; ++h)
            pruned.trunk.push_back(_trunk[h].hash);
        _trimmed = trim_height;
        return pruned;
    }
    

private:
    typedef std::vector<CBigNum> AccumulatedWork;
    AccumulatedWork _acc_work;
    
    size_t _trimmed;
};

typedef BlockTree::Iterator BlockIterator;

class COINCHAIN_EXPORT BlockLocator {
public:
    std::vector<uint256> have;
public:
    
    BlockLocator() {}
    
    IMPLEMENT_SERIALIZE
    (
     if (!(nType & SER_GETHASH))
     READWRITE(nVersion);
     READWRITE(have);
     )
    
    void SetNull() {
        have.clear();
    }
    
    bool IsNull() {
        return have.empty();
    }
    
    friend bool operator==(const BlockLocator& a, const BlockLocator& b);
};
                        
#endif // BLOCKTREE_H
