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

#ifndef ALERT_H
#define ALERT_H

#include <string>
#include <vector>

//#include <coin/serialize.h>
#include <coin/uint256.h>
#include <coin/util.h>
#include <coin/Key.h>

#include <coinChain/Export.h>

/// Alerts are for notifying old versions if they become too obsolete and
/// need to upgrade.  The message is displayed in the status bar.
/// Alert messages are broadcast as a vector of signed data.  Unserializing may
/// not read the entire buffer if the alert is for a newer version, but older
/// versions can still relay the original data.

class COINCHAIN_EXPORT UnsignedAlert
{
public:
    inline friend std::ostream& operator<<(std::ostream& os, const UnsignedAlert& a) {
        return (os << const_binary<int>(a._version)
                << const_binary<int64_t>(a._relay_until)
                << const_binary<int64_t>(a._expiration)
                << const_binary<int>(a._id)
                << const_binary<int>(a._cancel)
                << a._cancels
                << const_binary<int>(a._min_version)
                << const_binary<int>(a._max_version)
                << a._subversions
                << const_binary<int>(a._priority)
                << const_varstr(a._comment)
                << const_varstr(a._status_bar)
                << const_varstr(a._reserved));
    }
    
    inline friend std::istream& operator>>(std::istream& is, UnsignedAlert& a) {
        return (is >> binary<int>(a._version)
                >> binary<int64_t>(a._relay_until)
                >> binary<int64_t>(a._expiration)
                >> binary<int>(a._id)
                >> binary<int>(a._cancel)
                >> a._cancels
                >> binary<int>(a._min_version)
                >> binary<int>(a._max_version)
                >> a._subversions
                >> binary<int>(a._priority)
                >> varstr(a._comment)
                >> varstr(a._status_bar)
                >> varstr(a._reserved));
    }

    void setNull()
    {
        _version = 1;
        _relay_until = 0;
        _expiration = 0;
        _id = 0;
        _cancel = 0;
        _cancels.clear();
        _min_version = 0;
        _max_version = 0;
        _subversions.clear();
        _priority = 0;

        _comment.clear();
        _status_bar.clear();
        _reserved.clear();
    }

    const int getPriority() const { return _priority; }
    
    const int64_t until() const { return _relay_until; }
    
    std::string toString() const;

    void print() const
    {
        log_info("%s", toString().c_str());
    }
    
    const std::string& getStatusBar() const { return _status_bar; }
    
    inline friend bool operator==(const UnsignedAlert& l, const UnsignedAlert& r) {
        return (l._version == r._version &&
                l._relay_until == r._relay_until &&
                l._expiration == r._expiration &&
                l._id == r._id &&
                l._cancel == r._cancel &&
                l._cancels == r._cancels &&
                l._min_version == r._min_version &&
                l._max_version == r._max_version &&
                l._subversions == r._subversions &&
                l._priority == r._priority &&
                l._comment == r._comment &&
                l._status_bar == r._status_bar &&
                l._reserved == r._reserved);
    }
    
    inline friend bool operator!=(const UnsignedAlert& l, const UnsignedAlert& r) {
        return !(l == r);
    }
    
protected:
    int _version;
    int64_t _relay_until;      // when newer nodes stop relaying to newer nodes
    int64_t _expiration;
    int _id;
    int _cancel;
    std::set<int> _cancels;
    int _min_version;            // lowest version inclusive
    int _max_version;            // highest version inclusive
    std::set<std::string> _subversions;  // empty matches all
    int _priority;
    
    // Actions
    std::string _comment;
    std::string _status_bar;
    std::string _reserved;
};

class Alert;

extern std::map<uint256, Alert> mapAlerts;

class Peer;

class COINCHAIN_EXPORT Alert : public UnsignedAlert
{
public:
    Alert(const PubKey& pubkey) : _pub_key(pubkey) {
        setNull();
    }

    inline friend std::ostream& operator<<(std::ostream& os, const Alert& a) {
        return os << a._message << a._signature;
    }
    
    inline friend std::istream& operator>>(std::istream& is, Alert& a) {
        return is >> a._message >> a._signature;
    }

    void setNull() {
        UnsignedAlert::setNull();
        _message.clear();
        _signature.clear();
    }

    bool isNull() const {
        return (_expiration == 0);
    }

    uint256 getHash() const {
        return serialize_hash(*this);
    }

    bool isInEffect() const {
        return (GetAdjustedTime() < _expiration);
    }

    bool cancels(const Alert& alert) const {
        if (!isInEffect())
            return false; // this was a no-op before 31403
        return (alert._id <= _cancel || _cancels.count(alert._id));
    }

    bool appliesTo(int version, std::string subversion) const {
    // TODO: rework for client-version-embedded-in-strSubVer ?
        return (isInEffect() &&
                _min_version <= version && version <= _max_version &&
                (_subversions.empty() || _subversions.count(subversion)));
    }

    //    bool appliesToMe() const {
    //        return AppliesTo(PROTOCOL_VERSION, _node.getFullVersion());
    //    }

    //    bool relayTo(Peer* peer) const;

    bool checkSignature();

    bool processAlert();
    
    inline friend bool operator==(const Alert& l, const Alert& r) {
        return ((const UnsignedAlert&)l == (const UnsignedAlert&)r &&
                l._pub_key == r._pub_key &&
                l._message == r._message &&
                l._signature == r._signature);
    }

    inline friend bool operator!=(const Alert& l, const Alert& r) {
        return !(l == r);
    }
    
private:
    const PubKey& _pub_key;
    std::vector<unsigned char> _message;
    std::vector<unsigned char> _signature;
};

#endif // ALERT_H
