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

#ifndef REFERENCED_H
#define REFERENCED_H

#include <stdexcept>

/** Base class from providing referencing counted objects.*/
class Referenced
{
public:
    class Error: public std::runtime_error {
    public:
        Error(const std::string& s) : std::runtime_error(s.c_str()) {}
    };
    
    Referenced() {
        _refCount=0;
    }
    Referenced(const Referenced&) {
        _refCount=0;
    }
    
    inline Referenced& operator = (const Referenced&) { return *this; }
    
    /** increment the reference count by one, indicating that
     this object has another pointer which is referencing it.*/
    inline void ref() const { ++_refCount; }
    
    /** decrement the reference count by one, indicating that
     a pointer to this object is referencing it.  If the
     reference count goes to zero, it is assumed that this object
     is no longer referenced and is automatically deleted.*/
    inline void unref() const;
    
    /** decrement the reference count by one, indicating that
     a pointer to this object is referencing it.  However, do
     not delete it, even if ref count goes to 0.  Warning, unref_nodelete()
     should only be called if the user knows exactly who will
     be resonsible for, one should prefer unref() over unref_nodelete()
     as the later can lead to memory leaks.*/
    inline void unref_nodelete() const { --_refCount; }
    
    /** return the number pointers currently referencing this object. */
    inline int referenceCount() const { return _refCount; }
    
    /** return true if this object is not shared */
    inline bool unique() const { return _refCount == 1; }
    
protected:
    virtual ~Referenced() {
        if (_refCount>0) {
            throw Error("Deleting referenced object!");
        }
    }
    
    mutable int _refCount;
    
};

inline void Referenced::unref() const
{
    --_refCount;
    if (_refCount==0) {
		delete this;
    }
    else if (_refCount<0) {
        throw Error("Reference count negative!");
    }
}

#endif // REFERENCED_H
