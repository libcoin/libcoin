/* -*-c++-*- libcoin - Copyright (C) 2012-13 Michael Gronager
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

#include <coin/NameOperation.h>
#include <coin/Transaction.h>

NameOperation::NameOperation() : _allow_overwrite(true), _requires_network_fee(false) {}

void NameOperation::input(const Output& coin, int count) {
    // we can have only one in script, however, we allow an op_name_new to be overwritten...
    Script dropped = coin.script().getDropped();
    if (!dropped.empty()) {
        Evaluator eval;
        eval(dropped);
        if (eval.was_name_script()) {
            if (_allow_overwrite) {
                if (!_name_in.empty())
                    // Note this exception is to accept tx: 774d4c446cecfc40b1c02fdc5a13be6d2007233f9d91daefab6b3c2e70042f05 in block 99381
                    // apparently it is violating the standard by having two input namescripts a name_new and a first_update - this allows disregarding a name_new...
                    log_info("We have allowed overwrite of script: %s", coin.script().toString());
                _name_in = dropped;
                if (eval.was_name_new())
                    _input_count = count;
                else
                    _allow_overwrite = false;
            }
            else
                throw Error("Only one name pr transaction");
        }
    }
}
bool NameOperation::output(const Output& coin) {
    const Script& script = coin.script();
    if (script.size() == 1 && script[0] == OP_RETURN) {
        _network_fee += coin.value();
        return false;
    }
    Script dropped = script.getDropped();
    if (_name_out.empty() && !dropped.empty()) {
        _name_out = dropped;
        
        // evaluate the name stuff
        Evaluator eval;
        bool allow_only_new = false;
        if (_name_in.empty())
            allow_only_new = true;
        else if (!eval(_name_in) || eval.stack().empty())
            throw Error("Invalid Name Script: " + _name_in.toString());
        if (!eval(_name_out) || eval.stack().empty())
            throw Error("Invalid Name Script: " + _name_in.toString() + "\n\tout: "+ _name_out.toString());
        
        if (!eval.was_name_script())
            return false;
        
        // this means we had no inputs - don't treat this as a name op - ignore
        if (eval.was_name_new() || allow_only_new)
            return false;
        
        _name = eval.name();
        _value = eval.value();
        
        if (eval.was_first_update())
            _requires_network_fee = true;
        
        return true;
    }
    else if (!_name_out.empty() && !dropped.empty())
        Error("Only one name pr transaction");
    
    return false;
}

void NameOperation::check_fees(int64_t min_fee) const {
    // name_validator.check();
    if (_requires_network_fee && _network_fee < min_fee)
        throw Error("Network fee too small");
}

bool NameOperation::reserve() const {
    return _requires_network_fee;
}

const std::string& NameOperation::name() const {
    return _name;
}

const std::string& NameOperation::value() const {
    return _value;
}

/// NameEvaluator - Evaluates namescripts

NameOperation::Evaluator::Evaluator() : ::Evaluator(), _name_script(false) {}
bool NameOperation::Evaluator::was_name_new() const {
    return _stack.size() == 1;
}

bool NameOperation::Evaluator::was_first_update() const {
    return _stack.size() == 3;
}

std::string NameOperation::Evaluator::name() const {
    return std::string(top(-1).begin(), top(-1).end());
}

std::string NameOperation::Evaluator::value() const {
    return std::string(top(-2).begin(), top(-2).end());
}

int NameOperation::input_count() const {
    return _input_count;
}

bool NameOperation::Evaluator::was_name_script() const {
    return _name_script;
}

/// Subclass Evaluator to implement eval, enbaling evaluation of context dependent operations
boost::tribool NameOperation::Evaluator::eval(opcodetype opcode) {
    int name_op = (opcode - OP_1 + 1);
    switch (name_op) {
        case OP_NAME_NEW: {
            _name_script = true;
            // stack must be exactly 1
            if (_stack.size() != 1)
                return false;
            // check the size of the stack
            if (_stack.front().size() != 20)
                return false;
            return true;
        }
        case OP_NAME_FIRSTUPDATE: {
            _name_script = true;
            // either the stack can be 4 or it can be 3
            if (_stack.size() == 3) {
                // name, rand, value -> name, value
                swap(top(-2), top(-1));
                pop(_stack);
                return boost::indeterminate;
            }
            if (_stack.size() == 4) {
                // name, rand, value, hash -> name, value : iff hash=hash(rand,name)
                std::vector<unsigned char> rand_name(top(-2));
                Value name = top(-1);
                rand_name.insert(rand_name.end(), name.begin(), name.end());
                uint160 hash =  toPubKeyHash(rand_name);
                if (uint160(top(-4)) == hash) {
                    swap(top(-2), top(-1));
                    pop(_stack);
                    return true;
                }
            }
            return false;
        }
        case OP_NAME_UPDATE: {
            _name_script = true;
            // (the stack can contain either 2 or 4 values)
            if (_stack.size() == 2)
                // name, value -> name, value
                return boost::indeterminate;
            if (_stack.size() == 4) {
                // name1, value1, name2, value2 -> name1, value1 :: iff name1==name2
                if (top(-1) == top(-3)) {
                    swap(top(-1), top(-3));
                    swap(top(-2), top(-4));
                    pop(_stack);
                    pop(_stack);
                    return true;
                }
            }
            return false;
        }
        default:
            return boost::indeterminate;
    }
}
