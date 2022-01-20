// HomogenousHeapUni.h
// Ariel Tamches
// Class to provide fast allocation and deallocation
// of a heap off homogenous items
// Items in this heap are *uninitialized*, so alloc() returns just a bucket of
// undefined bits.  Be sure to call the ctor explicitly after calling alloc()!
// Use HomogenousHeap.h if you prefer an initialized-item alternative.
//
// Advantages and disadvantages of this class:
// - alloc() returns an undefined value; you must call ctor explicitly, presumably
//   with a dummy placement new, like stdc++ does.
// - furthermore, you must call the dtor explicitly, too.
// + since you must call the ctor, you have flexibility of calling the one you want.
//   Furthermore, you can wait until alloc()-time to call the ctor; often calling ctor
//   before then is a burden because you don't at that time have available meaningful
//   arguments for a ctor call.
//
// Basically, the difference between this class and HomogenousHeap.h is that here,
// you have to explicitly call a ctor after alloc().  In contrast, in the other
// class, you inevitably have to call operator=() after alloc().

#ifndef _HOMOGENOUS_HEAP_UNI_H_
#define _HOMOGENOUS_HEAP_UNI_H_

#ifndef NULL
#define NULL 0
#endif

#include <assert.h>
#include "HomogenousHeapBase.h"
#include "kdrvtypes.h"

template <class T, unsigned numAtATime>
class HomogenousHeapUni : public HomogenousHeapBase<dptr_t, numAtATime> {
   typedef HomogenousHeapBase<dptr_t, numAtATime> inherited;
   
 private:
   HomogenousHeapUni(const HomogenousHeapUni &); // explicitly disallowed
   HomogenousHeapUni& operator=(const HomogenousHeapUni&); // explicitly disallowed

   // We MUST provide this method; the base class depends on us for this:
   HomogenousHeapBase<dptr_t, numAtATime> *create_subheap(unsigned inBasicElems) {
      return new HomogenousHeapUni(SUBHEAP(), inBasicElems); // below
   }

   HomogenousHeapUni(typename HomogenousHeapUni::SUBHEAP, unsigned inBasicElems) :
      inherited(inherited::SUBHEAP(), sizeof(T), inBasicElems,
                (dptr_t)malloc(inBasicElems * sizeof(T))) {
   }

 public:
   HomogenousHeapUni(unsigned nelems) : inherited(sizeof(T))
   {
      nextHeap = new HomogenousHeapUni(SUBHEAP(), nelems);
      assert(nextHeap);
   }
   
   ~HomogenousHeapUni() {
      if (rawData != 0)
         ::free((void *)rawData);
      delete nextHeap; // harmless if NULL, recurses if not
      
      nextHeap = NULL;
      rawData = 0;
   }
};

#endif
