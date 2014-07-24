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

/* ************************************************************************** */
/* name_show implementation.  */

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
  const NameDbRow row = node.blockChain ().getNameRow (name);
  const NameStatus nm(row, node.blockChain ());

  /* Error if the name was not found.  */
  if (!row.found)
    throw RPC::error (RPC::name_not_found,
                      "The requested name doesn't exist in the database.");

  return nm.toJson ();
}

/* ************************************************************************** */
/* name_history implementation.  */

json_spirit::Value
NameHistory::operator() (const json_spirit::Array& params, bool fHelp)
{
  if (fHelp || params.size () != 1)
    throw RPC::error (RPC::invalid_params,
                      "name_history <name>\n"
                      "Show current and known past information about <name>.");

  /* FIXME: Correctly handle integer parameters (convert to string).  */
  const std::string name = params[0].get_str ();

  /* Query for the name in the block chain database.  */
  const std::vector<NameDbRow> rows = node.blockChain ().getNameHistory (name);
  if (rows.empty ())
    throw RPC::error (RPC::name_not_found,
                      "The requested name doesn't exist in the database.");

  /* Build up an array from all the rows.  */
  json_spirit::Array res;
  for (std::vector<NameDbRow>::const_iterator i = rows.begin ();
       i != rows.end (); ++i)
    {
      assert (i->found);
      const NameStatus nm(*i, node.blockChain ());
      res.push_back (nm.toJson ());
    }

  return res;
}
