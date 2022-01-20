#error no longer supported

// HomogenousHeap.h
// Ariel Tamches
// Class to provide fast allocation and deallocation
// of a heap off homogenous items
// Items in the heap are initialized, so alloc() doesn't
// just return a bucket of undefined bits.
// Use HomogenousHeapUni if you prefer alloc() to return a bucket of
// undefined bits (you should, of course, then call the ctor explicitly).
// Advantages and disadvantages:
// This class:
// + alloc() returns a constructed value, so you don't need to call ctor explicitly
//   via a dummy placement operator new (like stdc++ does)
// - the constructed value is always set to iVal (passed to ctor of this class), which
//   is a waste of time if you're just going to call operator=() anyway.
// - Need to pass an initial value to the ctor, which can be a burden for very
//   complex types.

#ifndef _HOMOGENOUS_HEAP_H_
#define _HOMOGENOUS_HEAP_H_

#include "HomogenousHeapBase.h"

template <class T, unsigned numAtATime>
class HomogenousHeap : public HomogenousHeapBase<T, dptr_t, numAtATime> {
 private:
   T *initialValue;
      // Just a single element; allocated with new.  You might wonder why it's a ptr;
      // why not just 'T'?  The reason has to do with compiling; if just a 'T' then at
      // this time, T must be a complete type.  Occasionally, this creates compile
      // problems.
      // Also, now that we allow uninitialized elements in the ctor; having a ptr
      // avoids the need for a default ctor for type T.

   HomogenousHeap(const HomogenousHeap &); // explicitly disallowed
   HomogenousHeap& operator=(const HomogenousHeap&); // explicitly disallowed

   // We MUST provide this method; the base class depends on us for this:
   HomogenousHeapBase<T, dptr_t, numAtATime> *create_subheap(unsigned nBasicElems);

   HomogenousHeap(bool, unsigned nBasicElems, const T &iVal);

 public:

   HomogenousHeap(unsigned nelems, const T &iVal);
  ~HomogenousHeap();
};

#endif
