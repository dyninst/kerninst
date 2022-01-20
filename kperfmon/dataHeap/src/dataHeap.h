// dataHeap.h
// Ariel Tamches
//
// Data heap -- a heap of 8-byte integer counters, 16-byte timers, and 32-byte
//              virtual timers (vtimers). For each primitive we allocate a
//              vector, which length is the number of cpus.
#ifndef _DATA_HEAP_H_
#define _DATA_HEAP_H_

#include <inttypes.h> // for uint32_t, etc.
#include "vectorHeap.h"
#include "vtimer.h"

class dataHeap {
 private:
    // Heap id 0: 8-byte unsigned integers
    vectorHeap<uint64_t> uint8heap;

    // Heap id 1: timers (16 bytes)
    struct timer16 {
	uint64_t x,y;
    };
    vectorHeap<timer16> timer16heap;

    // Heap id 2: vtimers (32 bytes)
    vectorHeap<vtimer> vtimerheap;
    // we use new_vtimer in the template type, but really, any 32-byte entity
    // would do (including old_vtimer).
   
 public:
    dataHeap(unsigned num_cpus,
	     unsigned num_uint8_elems,
	     unsigned num_timer16_elems,
	     unsigned num_vtimer_elems);

    ~dataHeap();

    // Allocate a vector counter or timer
    kptr_t alloc8() { 
	return uint8heap.alloc(); 
    }
    kptr_t alloc16() { 
	return timer16heap.alloc(); 
    }
    kptr_t allocVT() { 
	return vtimerheap.alloc(); 
    }

    // Free the vector specified by its base address
    void unAlloc8(kptr_t kernelAddr) { 
	return uint8heap.free(kernelAddr); 
    }
    void unAlloc16(kptr_t kernelAddr) {
	return timer16heap.free(kernelAddr);
    }
    void unAllocVT(kptr_t kernelAddr) {
	return vtimerheap.free(kernelAddr);
    }

    // Convert the base address of a vector to the kerninstd space
    dptr_t kernelAddr2KerninstdAddrVT(kptr_t /*kernelAddr*/) const {
	assert(false);
	return 0;
    }
    
    // Return the length of vectors (should match ncpus)
    unsigned getElemsPerVector8() const {
	return uint8heap.getElemsPerVector();
    }
    unsigned getElemsPerVector16() const {
	return timer16heap.getElemsPerVector();
    }
    unsigned getElemsPerVectorVT() const {
	return vtimerheap.getElemsPerVector();
    }

    // Return the distance between successive elements of a vector counter --
    // the vector is not stored contiguously to avoid cache coherence misses
    unsigned getBytesPerStride8() const {
	return uint8heap.getBytesPerStride();
    }
    unsigned getBytesPerStride16() const {
	return timer16heap.getBytesPerStride();
    }
    unsigned getBytesPerStrideVT() const {
	return vtimerheap.getBytesPerStride();
    }
};

#endif
