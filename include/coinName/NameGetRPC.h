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

#ifndef NAMEGETRPC_H
#define NAMEGETRPC_H

#include <coinName/Export.h>

#include <coinChain/Node.h>
#include <coinChain/NodeRPC.h>

#include <coinHTTP/Server.h>
#include <coinHTTP/RPC.h>

/* ************************************************************************** */
/* NameGetMethod base class.  */

/**
 * Base class for Namecoin value retrieval methods.
 */
class COINNAME_EXPORT NameGetMethod : public Method
{

protected:

  /** Node used to access the blockchain for name queries.  */
  Node& node;

public:

  /**
   * Construct with a handle to the node to use.
   * @param n Node to use.
   */
  explicit inline
  NameGetMethod (Node& n)
    : Method(), node(n)
  {}

};

/* ************************************************************************** */
/* name_show implementation.  */

/**
 * Implementation of the name_show method to query for basic info
 * about the current state of a name.
 */
class COINNAME_EXPORT NameShow : public NameGetMethod
{

public:

  /**
   * Construct it for the given node.
   * @param n Node to use.
   */
  explicit inline
  NameShow (Node& n)
    : NameGetMethod(n)
  {
    setName ("name_show");
  }

  /**
   * Perform the call.
   * @param params Parameters given to call.
   * @param fHelp Help requested?
   * @return JSON result.
   */
  json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);

};

#endif // NAMEGETRPC_H
