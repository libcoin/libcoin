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

#include <coinPool/GetWork.h>

#include <coinHTTP/RPC.h>

using namespace std;
using namespace boost;
using namespace json_spirit;

Value GetWork::operator()(const Array& params, bool fHelp) {
    if (fHelp || params.size() > 1)
        throw RPC::error(RPC::invalid_params, "getwork [data]\n"
                         "If [data] is not specified, returns formatted hash data to work on:\n"
                         "  \"midstate\" : precomputed hash state after hashing the first half of the data (DEPRECATED)\n" // deprecated
                         "  \"data\" : block data\n"
                         "  \"hash1\" : formatted hash buffer for second hash (DEPRECATED)\n" // deprecated
                         "  \"target\" : little endian hash target\n"
                         "If [data] is specified, tries to solve the block and returns true if it was successful.");
    
    if (params.size() == 0) {
        // Update block
        Block block;
        uint256 target;
        boost::tie(block, target) = _pool.getWork();
        
        // Pre-build hash buffers
        char pmidstate[32];
        char pdata[128];
        char phash1[64];
        formatHashBuffers(block, pmidstate, pdata, phash1);
        
        Object result;
        result.push_back(Pair("midstate", HexStr(BEGIN(pmidstate), END(pmidstate)))); // deprecated
        result.push_back(Pair("data",     HexStr(BEGIN(pdata), END(pdata))));
        result.push_back(Pair("hash1",    HexStr(BEGIN(phash1), END(phash1)))); // deprecated
        result.push_back(Pair("target",   HexStr(BEGIN(target), END(target))));
        return result;
    }
    else {
        // Parse parameters
        vector<unsigned char> data = ParseHex(params[0].get_str());
        if (data.size() != 128)
            throw RPC::error(RPC::invalid_params, "Invalid parameter");
        
        Block* header = (Block*)&data[0];
        
        // Byte reverse
        for (int i = 0; i < 128/4; i++)
            ((unsigned int*)header)[i] = byteReverse(((unsigned int*)header)[i]);
        
        string result = _pool.submitBlock(*header);
        
        if (result.size())
            return false;
        else
            return true;
    }
}
