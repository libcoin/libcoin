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

#include <coinPool/Miner.h>

#include <coinPool/Pool.h>

using namespace std;
using namespace boost;

Miner::Miner(Pool& pool) : _pool(pool), _io_service(), _delay(_io_service), _generate(true) {}

// run the miner
void Miner::run() {
    // get a block candidate
    _io_service.run();
}

void Miner::setGenerate(bool gen) {
    if (gen^_generate) {
        _generate = gen;
        if (_generate)
            _io_service.post(boost::bind(&Miner::mine, this));
    }
}

bool Miner::getGenerate() const {
    return _generate;
}

/// Shutdown the Node.
void Miner::shutdown() { _io_service.dispatch(boost::bind(&Miner::handle_stop, this)); }

void Miner::handle_stop() {
    _io_service.stop();
}

// return true on success
void Miner::mine() {
    boost::this_thread::sleep(boost::posix_time::milliseconds(2000));
    int tries = 65536;
    
    // reorganize roughly every 11'th block
    bool invalid = !(rand()%3);
    Pool::Work work = _pool.getWork(invalid);
    Block& block = work.first;
//    Block block = _pool.getBlockTemplate();
    
    uint256 target = _pool.target();
    
    // mine - i.e. adjust the nonce and calculate the hash - note this is a totally crappy mining implementation! Don't used except for testing
    while (block.getHash() > target && tries--)
        block.setNonce(block.getNonce() + 1);
    
    if (block.getHash() <= target) {
        block.stripTransactions();
        _pool.submitBlock(block);
    }
    
    if (_generate)
        _io_service.post(boost::bind(&Miner::mine, this));
}

