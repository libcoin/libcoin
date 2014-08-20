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

#include <string>
#include <vector>

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

  /**
   * Given an array of NameDbRow objects as returned by a database query
   * (for name_history, name_scan or name_filter), convert that to
   * a JSON array.  Optionally enforce that names are unique, in which
   * case we only use the *first* appearance of a name (assuming they
   * are sorted with "count DESC").
   * @param arr The array of data rows.
   * @param unique Whether we should enforce unique names.
   * @return The corresponding JSON object.
   */
  json_spirit::Array processNameRows (const std::vector<NameDbRow>& arr,
                                      bool unique) const;

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

/* ************************************************************************** */
/* name_history implementation.  */

/**
 * Implementation of the name_history method to query for basic info
 * about all known (current and past, not yet pruned) states of a name.
 */
class COINNAME_EXPORT NameHistory : public NameGetMethod
{

public:

  /**
   * Construct it for the given node.
   * @param n Node to use.
   */
  explicit inline
  NameHistory (Node& n)
    : NameGetMethod(n)
  {
    setName ("name_history");
  }

  /**
   * Perform the call.
   * @param params Parameters given to call.
   * @param fHelp Help requested?
   * @return JSON result.
   */
  json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);

};

/* ************************************************************************** */
/* name_scan implementation.  */

/**
 * Implementation of the name_scan method to list (a subset of) all known
 * names (but only with their latest status).
 */
class COINNAME_EXPORT NameScan : public NameGetMethod
{

public:

  /**
   * Construct it for the given node.
   * @param n Node to use.
   */
  explicit inline
  NameScan (Node& n)
    : NameGetMethod(n)
  {
    setName ("name_scan");
  }

  /**
   * Perform the call.
   * @param params Parameters given to call.
   * @param fHelp Help requested?
   * @return JSON result.
   */
  json_spirit::Value operator() (const json_spirit::Array& params, bool fHelp);

};

/* ************************************************************************** */
/* name_filter implementation.  */

/**
 * Implementation of the name_filter method to query for names matching
 * a given regexp.
 */
class COINNAME_EXPORT NameFilter : public NameGetMethod
{

public:

  /**
   * Construct it for the given node.
   * @param n Node to use.
   */
  explicit inline
  NameFilter (Node& n)
    : NameGetMethod(n)
  {
    setName ("name_filter");
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
