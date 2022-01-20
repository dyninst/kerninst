/*
 * Copyright (c) 1996-1999 Barton P. Miller
 * 
 * We provide the Paradyn Parallel Performance Tools (below
 * described as Paradyn") on an AS IS basis, and do not warrant its
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

// $Id: refCounterK.h,v 1.1 2002/04/23 23:50:16 mjbrim Exp $
// refCounterK.h
// Ariel Tamches

#ifndef _REF_COUNTERK_H_
#define _REF_COUNTERK_H_

#include <assert.h>
#include <new.h> // vacuous placement new

#include "util/h/HomogenousHeapUni.h"

#ifndef NULL
#define NULL 0
#endif

template <class T>
class refCounterK {
 private:
   struct ss {
      T data;
      mutable unsigned refCount;
   };
   mutable ss *sharedStuff; // mutable so dereference() can set to NULL

   static HomogenousHeapUni<ss, 1> *ssHeap;
      // WARNING: make sure you call initialize_static_stuff() and free_static_stuff()
      // to properly initialize and avoid memory leaks, respectively.
      // ptr since we can't rely on the order of ctor calls to static members.

   static void allocHeap() {
      assert(ssHeap == NULL); // since static members always initialized to 0

      unsigned heapSize = 8192 / sizeof(ss);
      if (heapSize < 512)
	 heapSize = 512;

      ssHeap = new HomogenousHeapUni<ss, 1>(heapSize);
      assert(ssHeap);
   }
   static void freeHeap() {
      assert(ssHeap);
      delete ssHeap;
      ssHeap = NULL; // helps purify find mem leaks
   }
  

   void reference() const {
      ++sharedStuff->refCount;
   }
   void dereference() const {
      if (--sharedStuff->refCount == 0) {
         // call item dtor explicitly, since we're using HomogenousHeapUni:
         T *underlyingObject = &sharedStuff->data;

         underlyingObject->~T();
         
	 ssHeap->free((dptr_t)sharedStuff);
	 sharedStuff = NULL; // not strictly needed, but helps purify find mem leaks
      }
   }

   void makeSafeToModifyInPlace() {
      assert(sharedStuff->refCount > 0);
      if (sharedStuff->refCount == 1)
	 return; // it's already safe

      // invariant: attach count is >= 2
      const T &underlyingObject = sharedStuff->data;
         // since attach count >= 2, underlyingObject won't become a ptr to nowhere
         // after operator=() does its dereference.

      operator=(underlyingObject);
   }

 public:
   static void initialize_static_stuff() {
      allocHeap();
   }
   static void free_static_stuff() {
      freeHeap();
   }

   class MANUAL_CONSTRUCT {};
   refCounterK(MANUAL_CONSTRUCT) : sharedStuff((ss*)ssHeap->alloc()) {
      // for expert users only.  Like the below ctor, but leaves the underlying
      // object allocated but COMPLETELY uninitialized.  To initialize it yourself,
      // call getManualConstructObject(), and then do one of those neato
      // (void)new((void*)getManualConstructObject())(my_desired_ctor_params).
      sharedStuff->refCount = 1;
   }
   T* getManualConstructObject() {
      // for expert users only (in conjunction with the above MANUAL_CONSTRUCT ctor).
      return &sharedStuff->data;
   }

   T* dereferenceAndReconstructManually() {
      dereference();
      
      sharedStuff = (ss *)ssHeap->alloc();
      sharedStuff->refCount = 1;
      return &sharedStuff->data; // a completely uninitialized blob of memory
   }
   
   refCounterK(const T &src) : sharedStuff((ss *)ssHeap->alloc()) {
      // examples (where y was already initialized somehow and is of type T)
      // refCounterK<T> x = y; or
      // refCounterK<T> x(y);

      sharedStuff->refCount = 1;

      // Manually call copy-ctor for object of type T:
      T *underlyingObject = &sharedStuff->data;
      (void)new((void*)underlyingObject)T(src);
   }
   refCounterK(const refCounterK &src) : sharedStuff(src.sharedStuff) {
      // This constructor is what this whole class revolves around.  It's fast.
      // examples (assumes y is pre-initialized somehow, and is of type
      // refCounterK<T>, too):
      // refCounterK<T> x = y; or
      // refCounterK<T> x(y);

      reference();
   }
  ~refCounterK() {
      dereference();
   }
   refCounterK &operator=(const refCounterK &src) {
      if (this == &src)
         return *this; // protect against x=x

      // detach from old... (quick unless refCount was 1 in which case we fry)
      dereference();

      // ...and attach to the new (efficiently)
      sharedStuff = src.sharedStuff;
      reference();

      return *this;
   }
   refCounterK &operator=(const T &src) {
      // detach from old...
      dereference();

      // ...create new, and attach to it
      sharedStuff = (ss *)ssHeap->alloc();
      sharedStuff->refCount=1;

      T *underlyingObject = &sharedStuff->data;
      (void)new((void*)underlyingObject)T(src);

      return *this;
   }
   T &getModifiableData() {
      // Since this returns a reference that you can modify in-place,
      // it is almost surely a bug if you fail to call
      // prepareToModifyInPlace() before you call this
      // (Because you could implicitly alter other objects referencing this
      // same object, which goes against desired semantics that different
      // variables refer to different objects [They only refer to the
      // same object, a la this class, when it's safe to --- i.e. when
      // it's certain that these semantically different variables happen
      // to hold the same data contents.]).
      assert(sharedStuff);
      return sharedStuff->data;
   }
   const T &getData() const {
      // the const return type is essential; if you want to change the underlying
      // object, you should call prepareToModifyInPlace() then getModifiableData(),
      // instead of this routine.
      assert(sharedStuff);
      return sharedStuff->data;
   }
   bool isSafeToModifyInPlace() {
      return sharedStuff->refCount == 1;
   }
   void prepareToModifyInPlace() {
      // A very important member fn.  Call before you're going to modify the underlying
      // object in-place.  (i.e. about to call the non-const getData() and make changes
      // to it).  We'll make it safe to do such a thing (it's unsafe if there are
      // other(s) attached to the same object as we are; so in such a case we
      // [reluctantly] make a true copy before working on the underlying object).
      // Don't confuse "modifying in-place" with "assigning"; you should still use
      // operator=() whenever you're assigning the whole underlying object of type T;
      // but when you want to change just a field or two here and there, call this 1st.

      makeSafeToModifyInPlace();
   }
};

#endif
