// HomogeneousHeapUni.C
// Ariel Tamches
// Class to provide fast allocation and deallocation
// of a heap off homogenous items

#include "util/h/HomogenousHeapUni.h"

/* ************************* public methods ************************************* */

template <class T, unsigned numAtATime>
HomogenousHeapUni<T, numAtATime>::
HomogenousHeapUni(unsigned inumBasicElems) {
   // we'd like for the base class to call create_subheap in its ctor, but
   // one cannot call a virtual fn in a ctor
   nextHeap = new HomogenousHeapUni(false, inumBasicElems);
   assert(nextHeap);
}

template <class T, unsigned numAtATime>
HomogenousHeapUni<T, numAtATime>::~HomogenousHeapUni() {
   // dtor for base class will be called automatically...deletes free list

   // rawData was allocated as new char[...], so we should delete it as such, too.
   // Of course, dtors won't be called...but that's on purpose...remember that when
   // you use this class, you already assumed responsibility for calling ctors
   // and dtors explicitly.
   char *veryRawData = (char*)rawData;
   delete [] veryRawData; // harmless if NULL (header)
   xxx;
   
   delete nextHeap;

   // The following are not strictly necessary, but helps purify find mem leaks
   nextHeap = NULL;
   veryRawData = NULL;
}
