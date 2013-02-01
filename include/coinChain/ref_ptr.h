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

#ifndef REF_PTR_H
#define REF_PTR_H

#include <iostream>

/** Smart pointer for handling referenced counted objects.*/
template<class T>
class ref_ptr
{
public:
    typedef T element_type;
    
    ref_ptr() :_ptr(0L) {}
    ref_ptr(T* t):_ptr(t)              { if (_ptr) _ptr->ref(); }
    ref_ptr(const ref_ptr& rp):_ptr(rp._ptr)  { if (_ptr) _ptr->ref(); }
    ~ref_ptr()                           { if (_ptr) _ptr->unref(); _ptr=0; }
    
    inline ref_ptr& operator = (const ref_ptr& rp) {
        if (_ptr==rp._ptr) return *this;
        T* tmp_ptr = _ptr;
        _ptr = rp._ptr;
        if (_ptr) _ptr->ref();
            // unref second to prevent any deletion of any object which might
            // be referenced by the other object. i.e rp is child of the
            // original _ptr.
            if (tmp_ptr) tmp_ptr->unref();
                return *this;
    }
    
    inline ref_ptr& operator = (T* ptr) {
        if (_ptr==ptr) return *this;
        T* tmp_ptr = _ptr;
        _ptr = ptr;
        if (_ptr) _ptr->ref();
            // unref second to prevent any deletion of any object which might
            // be referenced by the other object. i.e rp is child of the
            // original _ptr.
            if (tmp_ptr) tmp_ptr->unref();
                return *this;
    }
    
    // comparison operators for ref_ptr.
    inline bool operator == (const ref_ptr& rp) const { return (_ptr==rp._ptr); }
    inline bool operator != (const ref_ptr& rp) const { return (_ptr!=rp._ptr); }
    inline bool operator < (const ref_ptr& rp) const { return (_ptr<rp._ptr); }
    inline bool operator > (const ref_ptr& rp) const { return (_ptr>rp._ptr); }
    
    // comparion operator for const T*.
    inline bool operator == (const T* ptr) const { return (_ptr==ptr); }
    inline bool operator != (const T* ptr) const { return (_ptr!=ptr); }
    inline bool operator < (const T* ptr) const { return (_ptr<ptr); }
    inline bool operator > (const T* ptr) const { return (_ptr>ptr); }
    
    
    inline T& operator*()  { return *_ptr; }
    
    inline const T& operator*() const { return *_ptr; }
    
    inline T* operator->() { return _ptr; }
    
    inline const T* operator->() const   { return _ptr; }
    
    inline bool operator!() const	{ return _ptr==0L; }
    
    inline bool valid() const	{ return _ptr!=0L; }
    
    inline T* get() { return _ptr; }
    
    inline const T* get() const { return _ptr; }
    
    /** take control over the object pointed to by ref_ptr, unreference but do not delete even if ref count goes to 0,
     * return the pointer to the object.
     * Note, do not use this unless you are 100% sure your code handles the deletion of the object correctly, and
     * only use when absolutely required.*/
    inline T* take() { return release();}
    
    inline T* release() { T* tmp=_ptr; if (_ptr) _ptr->unref_nodelete(); _ptr=0; return tmp;}
    
private:
    T* _ptr;
};

template<typename T>
std::istream& operator>> (std::istream& is, ref_ptr<T>& t) {
	if(is.peek() == EOF)
		return is;
    //		t = NULL;
	if(!t.valid()) t = new T;
        return is >> *(t.get());
}

template<typename T>
std::ostream& operator<< (std::ostream& os, ref_ptr<T>& t) {
	if(t.valid())
		os << *(t.get());
        return os;
        }
        
#endif // REF_PTR_H
