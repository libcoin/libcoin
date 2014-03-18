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

#ifndef NAMEOPERATION_H
#define NAMEOPERATION_H

#include <coin/Export.h>
#include <coin/Script.h>

/// The NameOperation class hooks naturally into Transaction validation.
/// Each Transaction can at max contain one name operation:
///     First scan for inputs by calling input(coin)
///     Then scan for outputs by calling output(coin)
/// The output will return true if a valid name operation has been found,
/// and throw if the rules are violated
/// Finally call check fees to ensure that the right amount of fee has been sacrificed to the network
/// 

static const int MIN_FIRSTUPDATE_DEPTH = 12;

class NameOperation {
public:
    enum {
        // definitions for namecoin
        OP_NAME_INVALID = 0x00,
        OP_NAME_NEW = 0x01,
        OP_NAME_FIRSTUPDATE = 0x02,
        OP_NAME_UPDATE = 0x03,
        OP_NAME_NOP = 0x04
    };
    
    class Error: public std::runtime_error {
    public:
        Error(const std::string& s) : std::runtime_error(s.c_str()) {}
    };

    
    NameOperation(bool ignore_rules);
    
    // returns true if input is a name update
    bool input(const Output& coin, int count);
    
    int input_count() const;
    
    bool output(const Output& coin);
    
    void check_fees(int64_t min_fee) const;
    
    /// the operation tries to reserve a new (or expired) name: OP_NAME_FIRST_UPDATE
    bool reserve() const;
    
    const std::string& name() const;
    
    void name(std::string name) { _name = name; }

    const std::string& value() const;
    
public:
    /// NameEvaluator - Evaluates namescripts
    class Evaluator : public ::Evaluator {
    public:
        Evaluator(bool ignore_rules);
        
        bool was_name_new() const;
        
        bool was_first_update() const;
        
        std::string name() const;
        
        std::string value() const;
        
        bool was_name_script() const;
        
    protected:
        virtual boost::tribool eval(opcodetype opcode);    private:
        bool _name_script;
        bool _ignore_rules;
    };

private:
    bool _allow_overwrite;
    Script _name_in;
    int _input_count;
    Script _name_out;
    int64_t _network_fee;
    bool _requires_network_fee;
    bool _ignore_rules;
    std::string _name;
    std::string _value;
};

#endif // NAMEOPERATION_H
