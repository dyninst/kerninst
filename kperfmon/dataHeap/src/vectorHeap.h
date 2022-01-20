// vectorHeap.h
// This class manages a heap of vectors. Each vector corresponds to a
// counter or timer. In turn, each vector element contains the counter
// value for a particular CPU

#ifndef _VECTOR_HEAP_H_
#define _VECTOR_HEAP_H_

#include "util/h/kdrvtypes.h"
#include "VectorHeapBase.h"

template <class T>
class vectorHeap : public VectorHeapBase<kptr_t> {
 private:
    typedef VectorHeapBase<kptr_t> inherited;
    class SUBHEAP {};

    static kptr_t low_level_allocate(unsigned nbytes);
   
    inherited *create_subheap(unsigned numVectors, unsigned elemsPerVector) {
	return new vectorHeap(SUBHEAP(), numVectors, elemsPerVector);
    }
    void fry_subheaps(vectorHeap *);
   
    vectorHeap(SUBHEAP, unsigned numVectors, unsigned elemsPerVector);

 public:
    vectorHeap(unsigned numVectors, unsigned elemsPerVector);
    ~vectorHeap(); // deallocates everything
};

#endif
