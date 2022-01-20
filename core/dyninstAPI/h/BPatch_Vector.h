/*
 * Copyright (c) 1996-2004 Barton P. Miller
 * 
 * We provide the Paradyn Parallel Performance Tools (below
 * described as "Paradyn") on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 * 
 * This license is for research uses.  For such uses, there is no
 * charge. We define "research use" to mean you may freely use it
 * inside your organization for whatever purposes you see fit. But you
 * may not re-distribute Paradyn or parts of Paradyn, in any form
 * source or binary (including derivatives), electronic or otherwise,
 * to any other organization or entity without our permission.
 * 
 * (for other uses, please contact us at paradyn@cs.wisc.edu)
 * 
 * All warranties, including without limitation, any warranty of
 * merchantability or fitness for a particular purpose, are hereby
 * excluded.
 * 
 * By your use of Paradyn, you understand and agree that we (or any
 * other person or entity with proprietary rights in Paradyn) are
 * under no obligation to provide either maintenance services,
 * update services, notices of latent defects, or correction of
 * defects for Paradyn.
 * 
 * Even if advised of the possibility of such damages, under no
 * circumstances shall we (or any other person or entity with
 * proprietary rights in the software licensed hereunder) be liable
 * to you or any third party for direct, indirect, or consequential
 * damages of any character regardless of type of action, including,
 * without limitation, loss of profits, loss of use, loss of good
 * will, or computer failure or malfunction.  You agree to indemnify
 * us (and any other person or entity with proprietary rights in the
 * software licensed hereunder) for any and all liability it may
 * incur to third parties resulting from your use of Paradyn.
 */

// $Id: BPatch_Vector.h,v 1.17 2004/07/28 07:24:45 jaw Exp $

#ifndef _BPatch_Vector_h_
#define _BPatch_Vector_h_

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "BPatch_dll.h"

#ifdef USE_STL_VECTOR
//#include <stl.h>
#include <vector>
#include <algorithm>
//#if ( __GNUC__ == 3 ) && ( __GNUC_MINOR__ == 1 )
#define BPatch_Vector std::vector
//#else
//#define BPatch_Vector	vector
//#endif

#else
#ifdef external_templates
#pragma interface
#endif /* external_templates */

/*
 * This causes problems when including this file in code that uses STL,
 * so I'm taking it out for now.  - brb
#ifndef BPATCH_LIBRARY
#define vector	BPatch_Vector
#endif
 */

template<class T>
class BPATCH_DLL_EXPORT BPatch_Vector {
    int		reserved;	// Number of objects for which space is reserved
    int		len;		// The current number of objects in the vector
    T*		data;		// Points to the array of objects

    void	reserve(int n);
    inline void	copy_from(const BPatch_Vector &);
public:
    BPatch_Vector(int n = 0);
    BPatch_Vector(const BPatch_Vector<T> &);
    ~BPatch_Vector();

    BPatch_Vector<T>& operator=(const BPatch_Vector<T> &);
    BPatch_Vector<T>& operator+=(const BPatch_Vector<T> &);

    unsigned int size() const { return len; }
    void	push_back(const T& x);
    void	push_front(const T& x);
    void        clear();
    void        resize(int sz);

    T&		operator[](int n) const;
};

// VG(06/15/02): VC.NET doesn't like definitions for dll imports
#ifndef BPATCH_DLL_IMPORT

// Reserve space for at least n entries in the vector
template<class T>
void BPatch_Vector<T>::reserve(int n)
{
    if (n > reserved) { // If we haven't already reserved enough space
	// Generally we double the size each time we reserve memory, so
	// that we're not doing it for every insertion.
	if (reserved*2 > n) n = reserved*2;

	// Create a new array with enough space
	T* new_data = new T[n];

	// Copy the entries from the old array to the new one
	for (int i = 0; i < len; i++)
	    new_data[i] = data[i];

	// Get rid of the old array and set up to use the new one
	if( data != NULL ) delete [] data;
	data = new_data;
	reserved = n;
    }
    assert(data != NULL || n == 0);
}

// Copy the contents of another vector into this one.
template<class T>
inline void BPatch_Vector<T>::copy_from(const BPatch_Vector &src)
{
    reserved = src.reserved;
    len      = src.len;
	if( reserved == 0 )
		data = NULL;
	else {
		data     = new T[reserved];

		for (int i = 0; i < src.len; i++)
			data[i] = src.data[i];
	}
}

// Contructor.  Takes one optional parameter, the number of entries for which
// to reserve space.
template<class T>
BPatch_Vector<T>::BPatch_Vector(int n) : reserved(0), len(0), data(NULL)
{
    assert(n >= 0);
    if (n > 0) reserve(n);
}

// Copy constructor.
template<class T>
BPatch_Vector<T>::BPatch_Vector(const BPatch_Vector<T> &src)
{
    copy_from(src);
}

// Destructor.  Frees allocated memory.
template<class T>
BPatch_Vector<T>::~BPatch_Vector()
{
	if( data != NULL ) delete [] data;
}

// Assignment operator.  Delete the contents of this vector and copy the
// contents of the other vector into it.
template<class T>
BPatch_Vector<T>& BPatch_Vector<T>::operator=(const BPatch_Vector<T> &src)
{
	if( data != NULL ) delete [] data;

    copy_from(src);

    return *this;
}

// Append operator.  Keep data and append the
// contents of the other vector into it.
template<class T>
BPatch_Vector<T>& BPatch_Vector<T>::operator+=(const BPatch_Vector<T> &src)
{
    reserve(len + src.size());
    for (unsigned int i = 0; i < src.size(); ++i) {
      data[i+len] = src[i];
    }
    len = len + src.size();
    return *this;
}

// Add an element to the end of the vector.
template<class T>
void BPatch_Vector<T>::push_back(const T& x)
{
    reserve(len+1);
    data[len] = x;
    len++;
}

// Add an element to the end of the vector.
template<class T>
void BPatch_Vector<T>::push_front(const T& x)
{
    int i;

    reserve(len+1);
    for (i=len; i > 0; i--) data[i] = data[i-1];
    data[0] = x;
    len++;
}

// Resize vector (pare elements from the end)
// This is skeletal and doesn't do much but adjust indices
// If vector needs to grow, it will do so automatically on insert
template<class T>
void BPatch_Vector<T>::resize(int sz)
{
    if (sz >= len) return;
    if (sz < 0) return;
    len = sz;
}

// Clear vector 
template<class T>
void BPatch_Vector<T>::clear()
{
  if( data != NULL ) delete [] data;
  len = 0;
  reserved = 0;
  data = NULL;
}

// Reference the nth element of the vector.
template<class T>
T& BPatch_Vector<T>::operator[](int n) const
{
    assert(data != NULL && n >= 0 && n < len);
    return data[n];
}

#endif /* BPATCH_DLL_IMPORT */
#endif /* USE_STL_VECTOR */
#endif /* _BPatch_Vector_h_ */
