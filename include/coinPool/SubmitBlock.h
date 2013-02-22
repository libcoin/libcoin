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

#ifndef COINPOOL_SUBMITBLOCKL_H
#define COINPOOL_SUBMITBLOCK_H

#include <coinPool/Export.h>

#include <coinPool/Pool.h>

class COINPOOL_EXPORT SubmitBlock : public PoolMethod {
public:
    
    SubmitBlock(Pool& pool) : PoolMethod(pool) { }
    
    json_spirit::Value operator()(const json_spirit::Array& params, bool fHelp);
};

#endif // COINPOOL_SUBMITBLOCK_H
