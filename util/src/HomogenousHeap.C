// HomogeneousHeap.C
// Ariel Tamches
// Class to provide fast allocation and deallocation
// of a heap off initialized homogenous items

#include "util/h/HomogenousHeap.h"

/* ************************* private methods ************************************* */

template <class T, unsigned numAtATime>
HomogenousHeap<T, numAtATime>::HomogenousHeap(bool, unsigned nBasicElems,
					      const T &iVal) :
	HomogenousHeapBase<T,dptr_t,numAtATime>(false, 
						nBasicElems,
						(dptr_t)new T[nBasicElems](iVal)),
	initialValue(new T(iVal)) {
   // create a sub-heap
   assert(initialValue);
}

template <class T, unsigned numAtATime>
HomogenousHeapBase<T, dptr_t, numAtATime> * // return type
HomogenousHeap<T, numAtATime>::create_subheap(unsigned nBasicElems) {
   return new HomogenousHeap(false, nBasicElems, *initialValue);
}

/* ************************* public methods ************************************* */

template <class T, unsigned numAtATime>
HomogenousHeap<T, numAtATime>::HomogenousHeap(unsigned inumBasicElems, 
					      const T &iVal) :
	HomogenousHeapBase<T,dptr_t,numAtATime>(),
	initialValue(new T(iVal)) {
   // we'd like for the base class to call create_subheap in its ctor, but
   // one cannot call a virtual fn in a ctor
   nextHeap = new HomogenousHeap(false, inumBasicElems, iVal);
   assert(nextHeap);
}

template <class T, unsigned numAtATime>
HomogenousHeap<T, numAtATime>::~HomogenousHeap() {
   // dtor for base class will be called automatically
   delete initialValue; // harmless when NULL (uninitialized heap)
   delete [] (T *)rawData;   // harmless when NULL (uninitialized heap)

   delete nextHeap;

   // The following are not strictly necessary, but helps purify find mem leaks
   nextHeap = NULL;
   initialValue = NULL;
   rawData = 0;
}
