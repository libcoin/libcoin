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

#include <coinChain/Verifier.h>

#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>

using namespace std;
using namespace boost;

Verifier::Verifier(size_t threads) : _work(_io_service), _failed(false) {
    if (threads == 0)
        threads = boost::thread::hardware_concurrency();
    if (threads == 0) // #cores query not supported
        threads = 1;
    while (threads--)
        _threads.add_thread(new boost::thread(boost::bind(&boost::asio::io_service::run, &_io_service)));
}

Verifier::~Verifier() {
    _threads.join_all();
}

void Verifier::reset() {
    boost::unique_lock< boost::shared_mutex > lock(_counter_access);
    _counter = 0;
    _reason = "";
    _failed = false;
}

void Verifier::verify(const Output& output, const Transaction& txn, unsigned int in_idx, bool strictPayToScriptHash, int hash_type) {
    queued(true);
    _io_service.post(boost::bind(&Verifier::do_verify, this, output, txn, in_idx, strictPayToScriptHash, hash_type));
    if (!_failed && !VerifySignature(output, txn, in_idx, strictPayToScriptHash, hash_type)) {
        _failed = true;
        _reason = "Transaction hash: " + txn.getHash().toString();
    }
}

void Verifier::do_verify(const Output& output, const Transaction& txn, unsigned int in_idx, bool strictPayToScriptHash, int hash_type) {
    bool ok = VerifySignature(output, txn, in_idx, strictPayToScriptHash, hash_type);
 
    queued(false);
    
    if (!queued())
        _all_done.notify_one(); // weel only one is waiting
}

void Verifier::queued(bool more) {
    boost::unique_lock< boost::shared_mutex > lock(_counter_access);
    if(more)
        ++_counter;
    else
        --_counter;
}

bool Verifier::queued() const {
    boost::shared_lock< boost::shared_mutex > lock(_counter_access);
    return _counter;
}

bool Verifier::yield_success() const {
    // wait for all threads to exit
    unique_lock<boost::mutex> lock(_finish);
    if (queued())
        _all_done.wait(lock);
    return !_failed;
}
