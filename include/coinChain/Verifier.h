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

#ifndef _VERIFIER_H
#define _VERIFIER_H

#include <coinChain/Export.h>

#include <coin/Transaction.h>

#include <string>

#include <boost/asio.hpp>

class COINCHAIN_EXPORT Verifier
{
public:
    Verifier(size_t threads = 0);
    ~Verifier();
    
    void reset();
    
    void verify(const Output& output, const Transaction& txn, unsigned int in_idx, bool strictPayToScriptHash, int hash_type);
    
    std::string reason() const { return _reason; }
    
    bool yield_success() const;
    
private:
    bool do_verify(const Output& output, const Transaction& txn, unsigned int in_idx, bool strictPayToScriptHash, int hash_type);
    
    void failed_with_reason(std::string reason);
    bool already_failed() const;
    
private:
    boost::asio::io_service _io_service;
    boost::asio::io_service::work _work;

    boost::thread_group _threads;

    std::vector<boost::shared_future<bool> > _pending_verifications;
    
    bool _failed;
    std::string _reason;
    
    mutable boost::shared_mutex _state_access;
};

#endif // _VERIFIER_H
