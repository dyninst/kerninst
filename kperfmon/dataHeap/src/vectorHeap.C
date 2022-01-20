// vectorHeap.C

#include "vectorHeap.h"
#include "kapi.h"

extern kapi_manager kmgr;

template <class T>
vectorHeap<T>::vectorHeap(unsigned numVectors, unsigned elemsPerVector) :
    inherited(elemsPerVector, sizeof(T))
{
    // The first heap in the linked list is always a dummy one.
    // 'nextHeap' is the real first heap
    nextHeap = new vectorHeap(SUBHEAP(), numVectors, elemsPerVector);
    assert(nextHeap);
}

template <class T>
vectorHeap<T>::vectorHeap(SUBHEAP, 
			  unsigned numVectors, unsigned elemsPerVector) :
    inherited(numVectors, elemsPerVector, sizeof(T),
	      low_level_allocate(numVectors * elemsPerVector * sizeof(T)))
{
}

template <class T>
vectorHeap<T>::~vectorHeap() {
    // We need to recursively loop thru the linked list of subheaps, frying
    // each one of them.

    if (rawData != 0) { // first heap will be NULL
	kmgr.free(rawData); // kernel addr
	rawData = 0;
    }

    // harmless (and stops the recursion of destructors) if NULL
    delete nextHeap; 

    nextHeap = NULL;
}


template <class T>
kptr_t vectorHeap<T>::low_level_allocate(unsigned nbytes) {
    // grab this many bytes from the kernel.  Also, make sure that it's mmapped
    // into kerninstd.

    const kptr_t kernelAddr = kmgr.malloc(nbytes);
    return kernelAddr;
}
