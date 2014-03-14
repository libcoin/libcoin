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

#ifndef COINNAME_NAMES_H
#define COINNAME_NAMES_H

#include <coinName/Export.h>

/**
 * Struct to hold data about one name history entry, corresponding to
 * the Names database table.
 */
struct NameDbRow
{

  bool found;

  int64_t coin;
  int count;
  Evaluator::Value name;
  Evaluator::Value value;

  inline NameDbRow ()
    : found(false)
  {}

  inline NameDbRow (int64_t co, int cnt, const Evaluator::Value& n,
                    const Evaluator::Value& v)
    : found(true), coin(co), count(cnt), name(n), value(v)
  {}

};

#endif // NAMES_H
