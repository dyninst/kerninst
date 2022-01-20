// VectorHeapBase.h
// Pure virtual base class; provides fast allocation and deallocation of
// strided vectors. Note that the only virtual fns are the dtor and
// create_subheap() -- the allocation and deallocation routines don't
// need to be virtual.
//
// Suppose we have a stride size of 64 bytes, we need to allocate
// five vectors (W, V, X, Y and Z) of length 6 and each element of a vector 
// takes 16 bytes. The data would be laid out in the following way. 
// Notice that each vector does not occupy a contiguous address range, 
// instead, its successive elements are 64 bytes apart.
// ->----< stride size (64 bytes == 4 elements)
//   wvxy             ^
//   wvxy             |
//   wvxy             | Vector length
//   wvxy             |
//   wvxy             |
//   wvxy             v
// Z gets placed in the second block:  
//   z---     
//   z---
//   z---
//   z---
//   z---
//   z---
//
// What a derived class needs to provide:
// 1) a create_subheap() method
// 2) A destructor, which will fry all of the subheaps recursively.

#ifndef _VECTOR_HEAP_BASE_H_
#define _VECTOR_HEAP_BASE_H_

#include <sys/sysmacros.h> // roundup()
#include <assert.h>

template <class Tptr> // Tptr: uint32_t or uint64_t, not a pointer !
class VectorHeapBase {
 protected:
    static const unsigned int minBytesPerStride = 64;
    unsigned sizeOfT;        // size of an element of a vector
    unsigned elemsPerVector; // vector length
    unsigned elemsPerStride; // see diagram
    unsigned bytesPerStride; // The stride may not be rounded to the element
                             // boundary -- it has to be a power of two.

    // Each block has elemsPerVector * elemsPerStride elements. See diagram.
    unsigned numBlocks;

    Tptr rawData; // derived class is responsible for creating this
    unsigned rawDataBytes;

    unsigned numFree;
    Tptr* freeList;
    // Stack of Tptr.  Allocation pops; deallocation pushes.  
    // Each item is a ptr to a first element of a vector. Since the vector
    // is strided, the second element is at ptr + bytesPerStride

    VectorHeapBase<Tptr> *nextHeap; // actual type is the derived type
    // a linked list of heaps...we cannot keep just one heap (and resize
    // it when needed) because items would dangle in freed memory.  When we
    // need to create a new heap, we give it twice at many entries as this
    // one, and put it _before_ this one in the linked list.  Since each new
    // heap has 2x entries of the previous one, the #heaps grows
    // logarithmically The first element in the linked list is always NULL;
    // this makes it possible to squeeze in a new linked list _before_ the
    // current one.

    Tptr allocate();
    // Allocates an item within this heap.  Called by alloc(), below.  On
    // failure, returns NULL (derived class can/should then get more space
    // for this heap).  Else returns a ptr to some bits; they will have been
    // intialized to whatever the derived class' ctor decided (perhaps
    // uninitialized raw bits).

    bool belongs(const Tptr ptr) const;
    // Returns true iff the ptr belongs to this heap; does not search next

    void deallocate(Tptr ptr);

    unsigned numBytesThisHeap() const {
	return rawDataBytes;
    }

    // The derived class is responsible for creating subheaps.  It allocates
    // and initializes to taste.
    virtual VectorHeapBase *create_subheap(unsigned iNumVectors,
					   unsigned iElemsPerVector) = 0;

    // explicitly disallowed
    VectorHeapBase(const VectorHeapBase &); 
    // explicitly disallowed
    VectorHeapBase& operator=(const VectorHeapBase &);
    // create a sub-heap
    VectorHeapBase(unsigned iNumVectors, unsigned iElemsPerVector, 
		   unsigned size_of_T, Tptr iRawData);

    void getMemUsage_internal(unsigned &forRawData, 
			      unsigned &forMetaData) const;
    unsigned pickStrideSize(unsigned size_of_T);

 public:

    VectorHeapBase(unsigned iElemsPerVector, unsigned size_of_T);

    virtual ~VectorHeapBase();

    // NOTE: alloc() doesn't call the ctor for T, and free doesn't call
    // the dtor for T.  Not calling the dtor is necessary -- otherwise, it'd
    // get called for a second time when the entire heap is deleted.  The
    // solution is to change the heap classes to allocate just raw memory
    // (malloc). Not a virtual fn on purpose (only create_subheap() need be)
    Tptr alloc();
    
    // NOTE: not virtual fn on purpose
    void free(Tptr item);

    void getMemUsage(unsigned &forRawData, unsigned &forMetaData) const;
    // returns memory usage, including raw data, free list, etc.
    // won't include any fragmentation, however.

    unsigned getElemSize() const {
	return sizeOfT;
    }
    unsigned getElemsPerVector() const {
	return elemsPerVector;
    }
    unsigned getBytesPerStride() const {
	return bytesPerStride;
    }
};

template <class Tptr>
Tptr VectorHeapBase<Tptr>::allocate() {
    // allocate w/in a subheap
    if (numFree == 0) {
	// Hmmm; can't do it in this heap; try next heap (if any),
	// even though, because it'll be a smaller heap,
	// there's less of a chance of success with him.

	if (nextHeap == 0) {
	    // forget it; no next heap; failure to allocate.
	    return 0;
	}
	else {
	    return nextHeap->allocate();
	}
    }

    Tptr result = freeList[--numFree];
    assert(belongs(result));
    return result;
}

template <class Tptr>
void VectorHeapBase<Tptr>::
getMemUsage(unsigned &forRawData, unsigned &forMetaData) const {
    // The first heap is just a header.
    nextHeap->getMemUsage_internal(forRawData, forMetaData);
}

template <class Tptr>
void VectorHeapBase<Tptr>::
getMemUsage_internal(unsigned &forRawData, unsigned &forMetaData) const {
    assert(rawData);

    forRawData += rawDataBytes; // raw data

    forMetaData += elemsPerStride * numBlocks * sizeof(Tptr*); // freeList
    forMetaData += sizeof(*this); // ourselves

    if (nextHeap != 0) {
	// and the other heap(s)...
	nextHeap->getMemUsage_internal(forRawData, forMetaData);
    }
}

// REMINDER: Tptr is an integer type, not a real pointer.  The difference is
// important to avoid ptr arithmetic being performed when we're expecting to
// do regular integer arithmetic!!!
template <class Tptr>
bool VectorHeapBase<Tptr>::belongs(const Tptr ptr) const {
    assert(rawData);

    if (ptr < rawData || ptr >= rawData + rawDataBytes) {
	return false;
    }

    // TODO: assert that ptr is properly aligned and points to 
    // the first element of a vector

    return true;
}

template <class Tptr>
void VectorHeapBase<Tptr>::deallocate(Tptr freemedata) {
    if (belongs(freemedata)) {
	freeList[numFree++] = freemedata;
    }
    else {
	assert(nextHeap);
	nextHeap->deallocate(freemedata);
    }
}

template <class Tptr>
unsigned VectorHeapBase<Tptr>::pickStrideSize(unsigned size_of_T)
{
    // Let's compute bytesPerStride. It should satisfy the following 
    // constraints: bytesPerStride >= minBytesPerStride (== 64),
    //              bytesPerStride >= sizeOfT
    //              bytesPerStride is a power of two
    unsigned bytes_stride = minBytesPerStride;
    while (bytes_stride < size_of_T) {
	bytes_stride <<= 1;
    }

    return bytes_stride;
}

// ctor for a sub-heap
// Notice that iNumVectors may not contain the exact number of 
// allocatable vectors -- if the element size does not divide
// the stride size, we will get less than that
template <class Tptr>
VectorHeapBase<Tptr>::
VectorHeapBase(unsigned iNumVectors, unsigned iElemsPerVector, 
	       unsigned size_of_T, Tptr iRawData) :
    sizeOfT(size_of_T), elemsPerVector(iElemsPerVector),
    rawData(iRawData), rawDataBytes(iNumVectors * iElemsPerVector * size_of_T)
{
    bytesPerStride = pickStrideSize(sizeOfT);
    elemsPerStride = bytesPerStride / sizeOfT;
    assert(elemsPerStride > 0);

    const unsigned bytesPerBlock = elemsPerVector * bytesPerStride;
    numBlocks = rawDataBytes / bytesPerBlock;
    assert(numBlocks > 0);

    numFree = numBlocks * elemsPerStride;
    freeList = new Tptr[numFree];
    assert(freeList);

    unsigned k = 0;
    for (unsigned i=0; i < numBlocks; i++) {
	Tptr freeChunkPtr = rawData + i * bytesPerBlock;
	for (unsigned j=0; j < elemsPerStride; j++) {
	    freeList[k++] = freeChunkPtr;
	    freeChunkPtr += sizeOfT;
	}
    }
    assert(k == numFree);

    nextHeap = 0;
}

/* ************************* public methods ****************************** */

template <class Tptr>
VectorHeapBase<Tptr>::
VectorHeapBase(unsigned iElemsPerVector, unsigned size_of_T) :
    sizeOfT(size_of_T), elemsPerVector(iElemsPerVector),
    rawData(0), rawDataBytes(0)
{
    // This ctor just creates header (dummy) sub-heap
    bytesPerStride = pickStrideSize(sizeOfT);
    elemsPerStride = bytesPerStride / sizeOfT;
    assert(elemsPerStride > 0);

    numBlocks = 0;
    freeList = 0;

    // The next sub-heap is really the first one: we'd like to call
    // create_subheap() now but can't, since one cannot call a virtual fn
    // within a ctor
    nextHeap = 0;
}

template <class Tptr>
VectorHeapBase<Tptr>::~VectorHeapBase() {
    delete [] freeList; // harmless when NULL (header)

    freeList = 0; // not strictly necessary, but helps purify find mem leaks
   
    // We don't delete[] rawData; the derived class created it, so it
    // fries it, too.
}

template <class Tptr>
Tptr VectorHeapBase<Tptr>::alloc() {
    // The first VectorHeap is just a 'header'; it allows us to
    // adjust who's the first heap.
    Tptr result = nextHeap->allocate();
    if (result == 0) {
	// Create new subheap 200% the size of the (soon to be
	/// formerly) largest sub-heap
	const unsigned oldNumBlocks = nextHeap->numBlocks;
	unsigned newNumBlocks = ((oldNumBlocks + 1) << 1);
	unsigned newNumVectors = newNumBlocks * elemsPerStride;

	// The derived class is responsible for creating sub-heaps:
	VectorHeapBase *newBigHeap = create_subheap(newNumVectors, 
						    nextHeap->elemsPerVector);
	assert(newBigHeap);

	newBigHeap->nextHeap = nextHeap;
	nextHeap = newBigHeap;

	// Now the allocation will succeed
	result = nextHeap->allocate();
    }
   
    assert(result);
    return result;
}

template <class Tptr>
void VectorHeapBase<Tptr>::free(Tptr item) {
    // in the spirit of c++ delete, free(NULL) should be harmless:
    if (item == 0)
	return;

    // The first heap is just a header
    nextHeap->deallocate(item);
}

#endif
