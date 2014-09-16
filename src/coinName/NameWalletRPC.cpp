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

#include <coinName/NameWalletRPC.h>

#include <coin/NameOperation.h>

/* ************************************************************************** */
/* name_list implementation.  */

json_spirit::Value
NameList::operator() (const json_spirit::Array& params, bool fHelp)
{
  if (fHelp || params.size () > 1)
    throw RPC::error (RPC::invalid_params,
                      "name_list [<name>]\n"
                      "List my own names, optionally only <name>.");

  const bool haveUnique = (params.size () > 0);
  std::string unique;
  if (haveUnique)
    {
      /* FIXME: Correctly handle integer parameters (convert to string).  */
      unique = params[0].get_str ();
    }

  const Chain& c = wallet.chain ();
  const BlockChain& bc = wallet.blockChain ();
  assert (c.adhere_names ());

  std::map<std::string, int> namesHeight;
  std::map<std::string, json_spirit::Object> namesObj;

  BOOST_FOREACH (const PAIRTYPE(const uint256, CWalletTx)& item,
                 wallet.mapWallet)
    {
      const CWalletTx& tx = item.second;

      std::string name, value;
      bool haveUpdate = false;
      Output nameOutput;
      BOOST_FOREACH (const Output& out, tx.getOutputs ())
        {
          NameOperation::Evaluator nameEval(true);
          const Script dropped = out.script ().getDropped ();
          if (!dropped.empty () && nameEval (dropped)
              && nameEval.was_name_script () && !nameEval.was_name_new ())
            {
              haveUpdate = true;
              nameOutput = out;
              name = nameEval.name ();
              value = nameEval.value ();
              break;
            }
        }

      if (!haveUpdate || (haveUnique && name != unique))
        continue;

      const int height = wallet.getHeight (tx);
      if (height == -1)
        continue;
      assert (height >= 0);

      /* Only get last active name.  */
      std::map<std::string, int>::const_iterator heightIter;
      heightIter = namesHeight.find (name);
      if (heightIter != namesHeight.end () && heightIter->second > height)
        continue;

      json_spirit::Object obj;
      obj.push_back (json_spirit::Pair ("name", name));
      obj.push_back (json_spirit::Pair ("value", value));
      if (!wallet.IsMine (nameOutput))
        obj.push_back (json_spirit::Pair ("transferred", 1));

      const PubKeyHash hash = nameOutput.script ().getAddress (true);
      assert (hash.isNonZero ());
      const std::string addr = c.getAddress (hash).toString ();
      obj.push_back (json_spirit::Pair ("address", addr));

      const int expiresIn = height - bc.getExpirationCount ();
      obj.push_back (json_spirit::Pair ("expires_in", expiresIn));
      if (bc.isExpired (height))
        obj.push_back (json_spirit::Pair ("expired", 1));

      namesHeight[name] = height;
      namesObj[name] = obj;
    }

  json_spirit::Array res;
  BOOST_FOREACH (const PAIRTYPE(const std::string, json_spirit::Object)& item,
                 namesObj)
    res.push_back (item.second);

  return res;
}
