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
/* NameGetMethod base class.  */

json_spirit::Array
NameGetMethod::processNameRows (const std::vector<NameDbRow>& arr,
                                bool unique) const
{
    std::set<Evaluator::Value> namesSeen;
    
    json_spirit::Array res;
    for (std::vector<NameDbRow>::const_iterator i = arr.begin ();
         i != arr.end (); ++i)
    {
        assert (i->found);
        if (unique && namesSeen.count (i->name) > 0)
            continue;
        
        if (unique)
            namesSeen.insert (i->name);
        
        const NameStatus nm(*i, node.blockChain ());
        res.push_back (nm.toJson ());
    }
    
    return res;
}

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
    
    return processNameRows (rows, false);
}

/* ************************************************************************** */
/* name_scan implementation.  */

json_spirit::Value
NameScan::operator() (const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size () > 2)
        throw RPC::error (RPC::invalid_params,
                          "name_scan [<start-name>] [<max-returned>]\n"
                          "Scan all names, starting at <start-name> and returning"
                          " a maximum number of entries (default 500)"
                          );
    
    std::string start;
    if (params.size () >= 1)
    {
        /* FIXME: Correctly handle integer parameters (convert to string).  */
        start = params[0].get_str ();
    }
    else
        start = "";
    
    unsigned count = 500;
    if (params.size () >= 2)
    {  
        count = params[1].get_int ();
	/* FIXME: bitcoin-rpc does not use integer parameters. */
    }
    /* Query for the data in the block chain database.  */
    const BlockChain& chain = node.blockChain ();
    const std::vector<NameDbRow> rows = chain.getNameScan (start, count);
    
    return processNameRows (rows, true);
}

/* ************************************************************************** */
/* name_filter implementation.  */

json_spirit::Value
NameFilter::operator() (const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size () > 5)
        throw RPC::error (RPC::invalid_params,
                          "name_filter [<pattern> [<maxage>=36000 [from=0 nb=0 [\"stat\"]]]]\n"
                          "Scan for names matching <pattern>.  Only look into"
                          " the last <maxage> blocks.\n"
                          "Return at most <nb> results (0 means all) and return them from the\n"
                          "entry with number <from> onward.\n\n"
                          "If \"stat\" is given as fifth argument, print statistics instead of\n"
                          "returning the results by themselves.");
    
    std::string pattern;
    if (params.size () >= 1)
    {
        /* FIXME: Correctly handle integer parameters (convert to string).  */
        pattern = params[0].get_str ();
    }
    else
        pattern = "";
    
    unsigned int maxage = 36000;
    unsigned from = 0, nb = 0;
    if (params.size () >= 2)
        maxage = params[1].get_int ();
    if (params.size () >= 3)
        from = params[2].get_int ();
    if (params.size () >= 4)
        nb = params[3].get_int ();
    
    /* Only allow "from" to be non-zero if "nb" is set.  "from" non-zero
     without "nb" makes no real sense, especially since the order of names
     is not strictly defined.  It also makes things complicated with the
     SQL query.  */
    if (from > 0 && nb == 0)
        throw RPC::error (RPC::invalid_params,
                          "<from> can only be non-zero if <nb> is set");
    
    bool stat = false;
    if (params.size () >= 5)
    {
        if (params[4].get_str () == "stat")
            stat = true;
        else
            throw RPC::error (RPC::invalid_params,
                              "invalid fifth argument, expected \"stat\"");
    }
    
    /* Query for the data in the block chain database.  */
    const BlockChain& chain = node.blockChain ();
    const std::vector<NameDbRow> rows = chain.getNameFilter (pattern, maxage, from, nb);
    
    /* Convert to JSON array.  */
    json_spirit::Array res = processNameRows (rows, true);
    
    /* Handle stats flag if we have it.  */
    if (stat)
    {
        json_spirit::Object stats;
        stats.push_back (json_spirit::Pair ("blocks", chain.getBestHeight ()));
        stats.push_back (json_spirit::Pair ("count", int (res.size ())));
        return stats;
    }
    
    return res;
}
