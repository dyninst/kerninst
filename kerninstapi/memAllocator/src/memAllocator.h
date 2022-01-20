#ifndef _MEM_ALLOCATOR_H_
#define _MEM_ALLOCATOR_H_

// This general-purpose, variable-length memory allocator allocates
// kernel memory and maps it into the user space in kerninstd.

// It contains a set of specialized allocators (memHeaps) for each
// power-of-two allocation size between 8 bytes and 4096 bytes. Larger
// requests bypass memHeaps and are satisfied with the page
// granularity
class memHeap {
    unsigned elementBytes;  // size of an element
    unsigned pageElements;  // number of elements per page
    pdvector<kptr_t> freeList; // list of elements available
    pdvector<kptr_t> pageList; // list of pages allocated underneath
 public:
    // Initialize a heap. We do not go with a regular constructor since
    // all the heaps are constructed at once and initialized later
    void create(unsigned iElementBytes, unsigned iPageBytes);

    // Allocate an element
    kptr_t alloc();

    // Free the element at addr
    void free(kptr_t addr);

    // When our alloc fails, the parent can feed us more pages
    void addPage(kptr_t pageStart);

    // Check if the given page belongs to us
    bool hasPage(kptr_t pageStart);
    // no freePage so far
};

class instrumenter;

class memAllocator {
    instrumenter *theInstrumenter;
    static const unsigned numHeaps = 10; // 8, 16, ..., 4096-byte
    memHeap *heap;
    unsigned pageBytes;
    dictionary_hash<kptr_t, pair<dptr_t, unsigned> > regionMap;
    
    // Allocate a contiguos region of pages
    kptr_t allocRegion(unsigned numPages);

    // Free the region of pages that starts at pageStart. The same
    // region must have been allocRegion'ed before
    void freeRegion(kptr_t pageStart);

    // Given the allocation size determine which heap to use
    unsigned getHeapNum(unsigned size);
 public:
    memAllocator(instrumenter *nstrmntr);
    ~memAllocator();
    
    // Allocate size bytes
    kptr_t alloc(size_t size);

    // Free region that starts at addr. The region must have been alloc'ed
    // before
    void dealloc(kptr_t addr);

    // Convert the given address from the kernel space into the
    // kerninstd space. The address must belong to a memory region
    // allocated with this memAllocator, but it can be in the middle of
    // it. That is, the region does not have to start with the address.
    dptr_t kernelAddrToKerninstdAddr(kptr_t addr);
};

#endif
