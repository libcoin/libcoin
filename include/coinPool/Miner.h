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

#ifndef COINPOOL_MINER_H
#define COINPOOL_MINER_H

#include <coinPool/Export.h>

#include <boost/noncopyable.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/deadline_timer.hpp>

class Pool;

/// Miner is a reference implementation of a Miner. IT IS NOT INTENDED FOR REAL USE!
/// Miner uses naive hash calculations and is hence suitable for educational and test purposes only.

class Miner : private boost::noncopyable {
public:
    Miner(Pool& pool);
    
    // run the miner
    void run();
    
    void setGenerate(bool gen);
    
    bool getGenerate() const;
    
    /// Shutdown the Node.
    void shutdown();
    
private:
    void handle_stop();
    
    void mine();
    
protected:
    Pool& _pool;
    
    boost::asio::io_service _io_service;
    boost::asio::deadline_timer _delay;

    bool _generate;
};

#endif // COINPOOL_MINER_H
