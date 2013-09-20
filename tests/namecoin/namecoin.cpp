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


#include <coin/BigNum.h>
#include <coin/Script.h>
#include <coin/NameOperation.h>

#include <iomanip>

#include <map>
#include <vector>
#include <deque>
#include <iostream>
#include <boost/variant.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>


using namespace std;
using namespace boost;




int main(int argc, char* argv[]) {
    try {

        // create the 4 script set transactions
        const string name = "d/domain";
        const string value1 = "{\"ns\":\"ns1.domain.com\"}";
        const string value2 = "{\"ns\":\"ns2.domain.com\"}";
        const string value3 = "{\"ns\":\"ns3.domain.com\"}";
        
        uint64_t rand = GetRand((uint64_t)-1);
        vector<unsigned char> seed = CBigNum(rand).getvch();
        string rand_str = Hash(seed.begin(), seed.end()).toString();
        cout << rand_str << endl;
        rand_str.resize(16);
        vector<unsigned char> rand_val = ParseHex(rand_str);
        vector<unsigned char> rand_name(rand_val);
        rand_name.insert(rand_name.end(), name.begin(), name.end());
        uint160 hash =  toPubKeyHash(rand_name);
        
        uint160 pkh = uint160("0x1234567890ABCDEF1234567890ABCDEF12345678");
        Script pay_to_pub_key_hash = (Script() << OP_DUP << OP_HASH160 << pkh << OP_EQUALVERIFY << OP_CHECKSIG);
        
        Script script_new = (Script() << ((int)NameOperation::OP_NAME_NEW) << hash << OP_2DROP);
        script_new += pay_to_pub_key_hash;
        
        Script script_first = (Script() << (int)NameOperation::OP_NAME_FIRSTUPDATE << Evaluator::Value(name.begin(), name.end()) << rand_val << Evaluator::Value(value1.begin(), value1.end()) << OP_2DROP << OP_2DROP);
        script_first += pay_to_pub_key_hash;
        
        Script script_update = (Script() << (int)NameOperation::OP_NAME_UPDATE << Evaluator::Value(name.begin(), name.end()) << Evaluator::Value(value2.begin(), value2.end()) << OP_2DROP << OP_DROP);
        script_update += pay_to_pub_key_hash;
        
        Script script_final = (Script() << (int)NameOperation::OP_NAME_UPDATE << Evaluator::Value(name.begin(), name.end()) << Evaluator::Value(value3.begin(), value3.end()) << OP_2DROP << OP_DROP);
        script_final += pay_to_pub_key_hash;
        
        cout << "script_new: " << script_new.toString() << endl;
        cout << "script_first: " << script_first.toString() << endl;
        cout << "script_update: " << script_update.toString() << endl;
        
        // loop over the 4 options: (null, new), (new, first), (first, update), (update, final)
        
        typedef vector<pair<Script, Script> > ScriptPairs;
        ScriptPairs scripts;
        scripts.push_back(make_pair(Script(), script_new));
        scripts.push_back(make_pair(script_new, script_first));
        scripts.push_back(make_pair(script_first, script_update));
        scripts.push_back(make_pair(script_update, script_final));
        
        for (ScriptPairs::const_iterator sp = scripts.begin(); sp != scripts.end(); ++sp) {
            const Script name_in = sp->first.getDropped();
            const Script name_out = sp->second.getDropped();
            
            cout << "name_in part (reversed): " << name_in.toString() << endl;
            cout << "name_out part (reversed): " << name_out.toString() << endl;
            
            NameOperation::Evaluator eval;
            bool allow_only_new = false;
            if (name_in.empty())
                allow_only_new = true;
            else if (!eval(name_in) || eval.stack().empty())
                throw runtime_error("Invalid Name Script");
            if (!eval(name_out) || eval.stack().empty())
                throw runtime_error("Invalid Name Script");
            
            if (eval.was_name_new()) {
                uint160 hash(eval.stack().back());
                cout << "name_new with hash: " << hash.toString() << endl;
            }
            else if (allow_only_new)
                throw runtime_error("Only NAME_NEW need no inputs");
            else {
                cout << "update: \"" << eval.name() << "\" = \"" << eval.value() << "\"" << endl;
            }
        }
    }
    catch (std::exception &e) {
        cout << "Exception: " << e.what() << endl;
        cout << "FAILED: NameEvaluator class" << endl;
        return 1;
    }
    
    return 0;
}
