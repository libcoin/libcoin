/* -*-c++-*- libcoin - Copyright (C) 2014 Daniel Kraft
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


#include <coinName/NameGetRPC.h>

#include <coinChain/NodeRPC.h>

#include <coinHTTP/Server.h>
#include <coinHTTP/RPC.h>

#include <coinName/Names.h>

json_spirit::Value
NameShow::operator() (const json_spirit::Array& params, bool fHelp)
{
  if (fHelp || params.size () != 1)
    throw RPC::error (RPC::invalid_params,
                      "name_show <name>\n"
                      "Show information about <name>.");

  /* FIXME: Correctly handle integer parameters (convert to string).  */
  const std::string name = params[0].get_str ();

  /* Query for the name in the block chain database.  */
  NameDbRow row = node.blockChain ().getNameRow (name);
  NameStatus nm(row, node.blockChain ());

  /* Error if the name was not found.  */
  if (!row.found)
    throw RPC::error (RPC::name_not_found,
                      "The requested name doesn't exist in the database.");

  const std::string txid = nm.getTransactionId ();

  json_spirit::Object res;
  res.push_back (json_spirit::Pair ("name", nm.getName ()));
  res.push_back (json_spirit::Pair ("value", nm.getValue ()));
  res.push_back (json_spirit::Pair ("expires_in", nm.getExpireCounter ()));
  if (nm.isExpired ())
    res.push_back (json_spirit::Pair ("expired", 1));
  if (txid != "")
    res.push_back (json_spirit::Pair ("txid", txid));

  return res;
}
