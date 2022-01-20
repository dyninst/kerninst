#include <sys/types.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "util/h/hashFns.h"
#include "instrumenter.h"
#include "memAllocator.h"

memAllocator::memAllocator(instrumenter *nstrmntr) : 
    theInstrumenter(nstrmntr), regionMap(addrHash4)
{
    assert(nstrmntr != 0);
    pageBytes = getpagesize();
    heap = new memHeap[numHeaps];
    
    unsigned elementSize = 8;
    for (unsigned i=0; i<numHeaps; i++) {
	heap[i].create(elementSize, pageBytes);
	elementSize <<= 1;
    }
}

memAllocator::~memAllocator()
{
    delete[] heap;
}


// Given the allocation size determine which heap to use
unsigned memAllocator::getHeapNum(unsigned size)
{
    assert(size < pageBytes);

    unsigned upperBound = 8;
    unsigned heapNum = 0;
    while (size > upperBound) {
	upperBound <<= 1;
	heapNum++;
    }
    return heapNum;
}

// Allocate size bytes
kptr_t memAllocator::alloc(size_t size)
{
    if (2*size <= pageBytes) {
	unsigned iheap = getHeapNum(size);
	kptr_t addr = heap[iheap].alloc();
	if (addr == 0) {
	    kptr_t newPage = allocRegion(1);
	    if (newPage == 0) {
		return 0;
	    }
	    heap[iheap].addPage(newPage);
	    addr = heap[iheap].alloc();
	}
	return addr;
    }
    else {
	unsigned numPages = (size + pageBytes - 1) / pageBytes; // roundup
	kptr_t newPages = allocRegion(numPages);
	return newPages;
    }
}

// Free region that starts at addr. The region must have been alloc'ed before
void memAllocator::dealloc(kptr_t addr)
{
    kptr_t pageStart = (addr / pageBytes) * pageBytes;
    for (unsigned i=0; i<numHeaps; i++) {
	if (heap[i].hasPage(pageStart)) {
	    heap[i].free(addr);
	    return;
	}
    }
    // The address is not in the heaps -> the region must have been
    // big enough
    freeRegion(pageStart);
}


// Allocate a contiguos region of pages
kptr_t memAllocator::allocRegion(unsigned numPages)
{
    unsigned size = numPages * pageBytes;
    pair<kptr_t, dptr_t> rv = 
	theInstrumenter->allocateMappedKernelMemory(size, false);

    pair<dptr_t, unsigned> info(rv.second, size);
    regionMap.set(rv.first, info);

    return rv.first;
}

// Free the region of pages that starts at pageStart. The same
// region must have been allocRegion'ed before
void memAllocator::freeRegion(kptr_t pageStart)
{
    pair<dptr_t, unsigned> info;
    if (!regionMap.find(pageStart, info)) {
	assert(false && "Attempt to free a non-existent region");
    }
    theInstrumenter->freeMappedKernelMemory(pageStart, info.second);
    regionMap.undef(pageStart);
}

// Convert the given address from the kernel space into the
// kerninstd space. The address must belong to a memory region
// allocated with this memAllocator, but it can be in the middle of
// it. That is, the region does not have to start with the address.
dptr_t memAllocator::kernelAddrToKerninstdAddr(kptr_t addr)
{
    dictionary_hash<kptr_t, pair<dptr_t, unsigned> >::const_iterator iter = 
	regionMap.begin();
    for (; iter != regionMap.end(); iter++) {
	kptr_t start = iter.currkey();
	if (addr >= start) {
	    pair<dptr_t, unsigned> info = iter.currval();
	    if (addr < start + info.second) {
		return info.first + (addr - start);
	    }
	}
    }
    return 0;
}

// Initialize a heap. We do not go with a regular constructor since
// all the heaps are constructed at once and initialized later
void memHeap::create(unsigned iElementSize, unsigned iPageBytes)
{
    elementBytes = iElementSize;
    pageElements = iPageBytes / elementBytes;
}

// Allocate an element
kptr_t memHeap::alloc()
{
    unsigned size = freeList.size();
    if (size > 0) {
	kptr_t rv = freeList[size-1];
	freeList.pop_back();
	return rv;
    }
    else {
	return 0;
    }
}

// Free the element at addr
void memHeap::free(kptr_t addr)
{
    freeList.push_back(addr);
}

// When our alloc fails, the parent can feed us more pages
void memHeap::addPage(kptr_t pageStart)
{
    kptr_t addr = pageStart;
    for (unsigned i=0; i<pageElements; i++) {
	freeList.push_back(addr);
	addr += elementBytes;
    }
    pageList.push_back(pageStart);
}

// Check if the given page belongs to us
bool memHeap::hasPage(kptr_t pageStart)
{
    pdvector<kptr_t>::const_iterator iter = pageList.begin();
    for (; iter != pageList.end(); iter++) {
	if (*iter == pageStart) {
	    return true;
	}
    }
    return false;
}
