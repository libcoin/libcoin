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

#include <coinName/Names.h>

/* ************************************************************************** */
/* Name base class.  */

NameStatus::NameStatus (const NameDbRow& d, const BlockChain& c)
  : data(d), coin(), chain(c)
{
  chain.getCoinById (data.coin, coin, data.count);
}

std::string
NameStatus::getTransactionId () const
{
  if (!coin)
    return "";

  return coin.key.hash.GetHex ();
}

std::string
NameStatus::getAddress () const
{
  if (!coin)
    return "";

  const Chain& c = chain.chain ();
  const PubKeyHash hash = coin.output.script ().getAddress (c.adhere_names ());
  if (!hash)
    return "";

  return c.getAddress (hash).toString ();
}
