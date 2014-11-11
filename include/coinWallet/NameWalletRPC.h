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

#ifndef NAMEWALLETRPC_H
#define NAMEWALLETRPC_H

#include <coinName/Export.h>

#include <coinWallet/Wallet.h>

#include <coinHTTP/Server.h>
#include <coinHTTP/RPC.h>

#include <string>
#include <vector>

/* ************************************************************************** */
/* NameWalletMethod base class.  */

/**
 * Base class for Namecoin methods with wallet access.
 */
class COINNAME_EXPORT NameWalletMethod : public Method
{

protected:

  /** Wallet used.  */
  Wallet& wallet;

public:

  /**
   * Construct with a handle to the wallet to use.
   * @param w Wallet to use.
   */
  explicit inline
  NameWalletMethod (Wallet& w)
    : Method(), wallet(w)
  {}

};

/* ************************************************************************** */
/* name_list implementation.  */

/**
 * Implementation of the name_list method to show all
 * names which are/were in the wallet.
 */
class COINNAME_EXPORT NameList : public NameWalletMethod
{

public:

  /**
   * Construct it for the given wallet.
   * @param w Wallet to use.
   */
  explicit inline
  NameList (Wallet& w)
    : NameWalletMethod(w)
  {
    setName ("name_list");
  }

  /**
   * Perform the call.
   * @param params Parameters given to call.
   * @param fHelp Help requested?
   * @return JSON result.
   */
  json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);

};

#endif // NAMEWALLETRPC_H
