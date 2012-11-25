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

#include <coinChain/BlockTree.h>

#include <coin/Block.h>

using namespace std;
using namespace boost;

void BlockTree::assign(const Trunk& trunk) {
    SparseTree<BlockRef>::assign(trunk);
    
    if (trunk.size() == 0) return;

    // set the genesis work
    _acc_work.push_back(_trunk[0].work());
    for (size_t h = 1; h < _trunk.size(); ++h) {
        // update the work:
        _acc_work.push_back(_acc_work[h-1] + _trunk[h].work());
    }
}

CBigNum BlockTree::accumulatedWork(BlockIterator blk) const {
    int h = blk.height();
    if (h < 0) { // trace back
        CBigNum sum = blk->work();
        while ((--blk).height() < 0) {
            sum += blk->work();
        }
        return sum + _acc_work[blk.height()];
    }
    else
        return _acc_work[h];
}

BlockTree::Changes BlockTree::insert(const BlockRef ref) {
    Changes changes;

    BlockIterator blk = find(ref.prev);
    
    if (blk == end())
        throw Error("BlockTree::insert: BlockRef not connected to blocktree!");
    
    // check for best work
    CBigNum work = accumulatedWork(blk) + ref.work();
    
    if (work > _acc_work.back()) {

        // step backwards to the trunk and add the new best blocks along the way
        while (blk.height() < 0) {
            changes.inserted.push_back(blk->hash);
            --blk;
        }
        
        BlockIterator root = blk;
        
        for (++blk ;blk != end(); ++blk) {
            changes.deleted.push_back(blk->hash);
            _branches[blk->hash] = *blk;
            _heights[blk->hash] *=-1; // flip height
        }
        
        // delete old branch from trunk
        _trunk.resize(root.height() + 1);
        
        // insert new branch into trunk and update the heights
        for (Hashes::const_reverse_iterator hash = changes.inserted.rbegin(); hash != changes.inserted.rend(); ++hash) {
            _trunk.push_back(_branches[*hash]);
            _heights[*hash] *= 1;
            _acc_work.push_back(_acc_work.back() + _branches[*hash].work());
            _branches.erase(*hash);
        }
        _heights[ref.hash] = _trunk.size();
        _trunk.push_back(ref);
        _acc_work.push_back(work);
        // insert the hash of the new BlockRef:
        changes.inserted.insert(changes.inserted.begin(), ref.hash);
    }
    else {
        _branches[ref.hash] = ref;
        _heights[ref.hash] = -abs(blk.height()) - 1; // negative height as it is not part of the trunk
    }
    
    return changes;
}

void BlockTree::pop_back() {
    // remove the best blockref:
    BlockIterator blk = best();
    _trunk.pop_back();
    _acc_work.pop_back();
    
    
    // now find the best chain - assumption - it is still in the trunk
    bool reorganize = false;
    BlockIterator best_blk = best();
    CBigNum best_work = accumulatedWork(best_blk);
    _heights.erase(blk->hash);
    for (Branches::const_iterator i = _branches.begin(); i != _branches.end(); ++i) {
        BlockIterator candidate = find(i->first);
        CBigNum candidate_work = accumulatedWork(candidate);
        if (candidate_work > best_work) {
            best_blk = candidate;
            reorganize = true;
        }
    }
    
    if (reorganize) {
        blk = best_blk;
        
        Hashes inserted;
        while (blk.height() < 0) {
            inserted.push_back(blk->hash);
            --blk;
        }
        
        BlockIterator root = blk;
        
        for (++blk ;blk != end(); ++blk) {
            _branches[blk->hash] = *blk;
            _heights[blk->hash] *=-1; // flip height
        }
        
        // delete old branch from trunk
        _trunk.resize(root.height() + 1);
        _acc_work.resize(root.height() + 1);
        
        // insert new branch into trunk and update the heights
        for (Hashes::const_reverse_iterator hash = inserted.rbegin(); hash != inserted.rend(); ++hash) {
            _trunk.push_back(_branches[*hash]);
            _acc_work.push_back(_acc_work.back() + _branches[*hash].work());
            _branches.erase(*hash);
            _heights[*hash] *= 1;
        }
    }
}

bool operator==(const BlockLocator& a, const BlockLocator& b) {
    return a.have == b.have;
}