// kernelDriver.h

#ifndef _KERNEL_DRIVER_H_
#define _KERNEL_DRIVER_H_

#if defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4)
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500 // for pread & pwrite
#endif
#include <sys/types.h> // off_t
#endif

#include <iostream.h>
#include <unistd.h> // pwrite()
#include <fcntl.h> // O_RDWR
#include <assert.h>
#include <stdio.h> // perror()
#include <string.h> // strncpy()

#include "util/h/Dictionary.h"
#include "util/h/mrtime.h"
#include "kernInstIoctls.h"
#include "abi.h"

class instr_t;//fwd decl
struct presentedMachineInfo; //fwd decl

#ifdef sparc_sun_solaris2_7

#include "nucleusDetector.h"

class undoablePokeInfo1Loc {
 private:
   uint32_t origval;
   unsigned howSoon; // 0, 1, or 2

   pdvector< std::pair<uint32_t, uint32_t> > replacementStack;
      // Highest index is the top of stack...most recent modification
      // each entry: .first is the old value, .second is value we changed it to

 public:
   undoablePokeInfo1Loc(uint32_t iorigval, unsigned ihowSoon) :
      origval(iorigval), howSoon(ihowSoon) {
   }
   undoablePokeInfo1Loc(const undoablePokeInfo1Loc &src) :
      origval(src.origval), howSoon(src.howSoon),
      replacementStack(src.replacementStack) {
   }
   void operator=(const undoablePokeInfo1Loc &src) {
      origval = src.origval;
      howSoon = src.howSoon;
      replacementStack = src.replacementStack;
   }
  ~undoablePokeInfo1Loc() {}

   uint32_t getOrigValue() const {
      return origval;
   }

   void push(uint32_t previous_value, uint32_t new_value);

   void changeTopOfStack(uint32_t new_value_was_this, // we'll assert this
                         uint32_t new_value_is_now_this // we'll change it to this
                         );

   void changeHowSoon(unsigned newHowSoon) {
      howSoon = newHowSoon;
   }

   bool pop(uint32_t previous_current_value, uint32_t new_current_value);

   unsigned getHowSoon() const {
      return howSoon;
   }
};

#elif defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4)

class undoablePokeInfo1Loc {
 private:
   char *origval;
   unsigned length;
   unsigned howSoon; // 0, 1, or 2

   pdvector< std::pair<const char *, const char *> > replacementStack;
      // Highest index is the top of stack...most recent modification
      // each entry: .first is the old value, .second is the value that we changed it to

 public:
   undoablePokeInfo1Loc(const char *iorigval, unsigned ilength, 
                        unsigned ihowSoon) :
      length(ilength), howSoon(ihowSoon) {
      origval = (char*) malloc(ilength);
      assert(origval != NULL);
      memcpy(origval, iorigval, ilength);
   }

   undoablePokeInfo1Loc(const undoablePokeInfo1Loc &src) : 
      length(src.length), howSoon(src.howSoon) {
      // this is costly, hopefully not used very often
      origval = (char*) malloc(length);
      assert(origval != NULL);
      memcpy(origval, src.origval, length);
      for(unsigned i=0; i<src.replacementStack.size(); i++) {
         char *oldv = (char *) malloc(length);
         char *newv = (char *) malloc(length);
         assert((oldv != NULL) && (newv != NULL));
         memcpy(oldv, src.replacementStack[i].first, length);
         memcpy(newv, src.replacementStack[i].second, length);
         replacementStack += make_pair(oldv, newv);
      }
   }

   ~undoablePokeInfo1Loc() {
      free(origval);
      for(unsigned i=0; i<replacementStack.size(); i++) {
         pair<const char *, const char *> &info = replacementStack.back();
         free(const_cast<char*>(info.first));
         free(const_cast<char*>(info.second));
         replacementStack.pop_back();
      }
   }

   void operator=(const undoablePokeInfo1Loc &src) {
      length = src.length;
      origval = (char*) malloc(length);
      assert(origval != NULL);
      memcpy(origval, src.origval, length);
      howSoon = src.howSoon;
      for(unsigned i=0; i<src.replacementStack.size(); i++) {
         char *oldv = (char *) malloc(length);
         char *newv = (char *) malloc(length);
         assert((oldv != NULL) && (newv != NULL));
         memcpy(oldv, src.replacementStack[i].first, length);
         memcpy(newv, src.replacementStack[i].second, length);
         replacementStack += make_pair(oldv, newv);
      }
   }

   const char* getOrigValue() const {
      return origval;
   }

   unsigned getLength() const {
      return length;
   }

   void push(const char *previous_value, const char *new_value);

   void changeTopOfStack(const char *new_value_was_this, // we'll assert this
                         const char *new_value_is_now_this // we'll change it to this
                         );

   void changeHowSoon(unsigned newHowSoon) {
      howSoon = newHowSoon;
   }

   bool pop(const char *previous_current_value, const char *new_current_value);

   unsigned getHowSoon() const {
      return howSoon;
   }
};

#endif

// ----------------------------------------------------------------------

class kernelDriver {
 public:
   class OpenFailed {};
   
 private:
   int driverfd;
   int kmemfd; // pwrite() to /dev/kmem
   int memfd; // pwrite() to /dev/mem is needed when writing to nucleus (map/unmap)
      // I though to get rid of memfd since we've now successfully implemented a
      // 4-byte write of the nucleus from inside our driver.  But since it can
      // only write 4 bytes, it's still useful to have memfd for when writing, e.g.
      // 12 byte springboards that may cross a page boundary.  Could we complete
      // our in-kerninstdriver implementation to handle more than 4 byte writes,
      // which perhaps cross page bounds?  Sure.

   // .first: cpuid of a cpu
   // .second: clock frequency of a cpu
#ifdef ppc64_unknown_linux2_4
   pdvector< std::pair<uint64_t, uint64_t> > cpuinfo;
#else
   pdvector< std::pair<unsigned, unsigned> > cpuinfo;
#endif
   dictionary_hash<kptr_t, uint32_t> allocatedKernelMemBlocks;
      // key: kernel virtual address
      // value: num bytes

   dictionary_hash<kptr_t, undoablePokeInfo1Loc> theUndoablePokes;

#ifdef sparc_sun_solaris2_7
   inNucleusDetector theNucleusDetector;
#elif defined(i386_unknown_linux2_4)
   // .first: family
   // .second: model
   pdvector< std::pair<unsigned, unsigned> > cpumodelinfo;
#endif

   static uint32_t pageNumBytes;

   void *map_common(kptr_t, uint32_t len, uint32_t prot,
                    bool assertPageSizedAndPageAligned);
   void unmap_common(void *inHere, uint32_t len);

#ifdef sparc_sun_solaris2_7
   bool waitForPokeToTakeEffect(kptr_t addr, uint32_t newVal,
                                mrtime_t maxNanoSecWaitTime) const;

   void poke1PermanentNucleusWord(kptr_t address, uint32_t value) const;
   void poke1PermanentNonNucleusWord(kptr_t address, uint32_t value) const;

   void disableTickProtectionAllCPUs();
#elif defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4)
   bool waitForPokeToTakeEffect(kptr_t addr, const char *newVal,
                                mrtime_t maxNanoSecWaitTime) const;
#endif

   static uint32_t getPageNumBytes_FirstTime();
   
   static void* kerninst_mmap(void*, size_t, int, int, int, uint32_t);

   static ssize_t kerninst_pread(int fildes, void *buf, size_t nbyte, 
				 uint32_t off);
   static ssize_t kerninst_pwrite(int fildes, const void *buf, size_t nbyte, 
				  uint32_t off);

#if defined(sparc_sun_solaris2_7) || defined (ppc64_unknown_linux2_4)
   static void* kerninst_mmap(void*, size_t, int, int, int, uint64_t);
   static ssize_t kerninst_pread(int fildes, void *buf, size_t nbyte, 
				 uint64_t off);
   static ssize_t kerninst_pwrite(int fildes, const void *buf, size_t nbyte, 
				  uint64_t off);
#endif

   // Do ioctl on every processor in the system
   void ioctl_all_cpus(int fildes, int request, void *argp);

#ifdef sparc_sun_solaris2_7
   // Personally I have no problem making the following method public,
   // but it seems to only be used by this class, so what the heck, we'll
   // make it private for now:
   void poke1WordPermanent(kptr_t address, uint32_t value) const;
#endif

   kernelDriver(const kernelDriver &);
   kernelDriver& operator=(const kernelDriver &);
   
 public:
   kernelDriver(const char *drivername);
  ~kernelDriver();

#ifdef sparc_sun_solaris2_7
   bool isInNucleus(kptr_t addr) const {
      return theNucleusDetector.isInNucleus(addr);
   }

   bool isBelowNucleus(kptr_t addr) const {
      return theNucleusDetector.isBelowNucleus(addr);
   }
   std::pair<kptr_t, kptr_t> queryNucleusTextBounds() const;
#elif defined(i386_unknown_linux2_4)
   bool isInNucleus(kptr_t addr) const {
      return false;
   }

   bool isBelowNucleus(kptr_t addr) const {
      return false;
   }
   
   std::pair<kptr_t, kptr_t> queryNucleusTextBounds() const {
      return std::pair<kptr_t, kptr_t>(0,0);
   }
#elif defined(ppc64_unknown_linux2_4)
   //We reuse "in nucleus" concept to reflect kernel image (vs. kernel module) 
   //space (0xC* is kernel image, 0xD* is modules)
   bool isInNucleus(kptr_t addr) const {
      return (addr < 0xD000000000000000ULL);
   }

   bool isBelowNucleus(kptr_t addr) const {
      return false;
   }
   
   std::pair<kptr_t, kptr_t> queryNucleusTextBounds() const {
      return std::pair<kptr_t, kptr_t>(0,0);
   }
#endif

#ifdef ppc64_unknown_linux2_4
   void kernelDriver::putInternalKernelSymbols(kptr_t kernelptrs[]);
   void kernelDriver::flushIcacheRange(kptr_t addr, unsigned numBytes);
#endif

#ifdef sparc_sun_solaris2_7
   void poke1WordUndoablePush(kptr_t addr,
                              uint32_t oldValue, // we assert: value in memory was this
                              uint32_t newValue, // we'll change it to this
                              unsigned howSoon);

   void poke1WordUndoableChangeTopOfStack(kptr_t addr,
                                          uint32_t oldValue, // we assert this...
                                          uint32_t newValue, // ...and change to this
                                          unsigned howSoon);

   void undoPoke1WordPop(kptr_t addr,
                         uint32_t currentValue,
                         uint32_t popBackToThisValue);

#else
   void poke1LocUndoablePush(kptr_t addr,
                             const char *oldValue, // we assert this...
                             const char *newValue, // ...and change to this
                             unsigned length,
                             unsigned howSoon);
   void poke1LocUndoableChangeTopOfStack(kptr_t addr,
                                         const char *oldValue, // assert this
                                         const char *newValue, // change to
                                         unsigned howSoon);
   void undoPoke1LocPop(kptr_t addr,
                        const char *currentValue,
                        const char *popBackToThisValue);

   void insertTrapToPatchAddrMapping(kptr_t trapAddr, kptr_t patchAddr);
   void updateTrapToPatchAddrMapping(kptr_t trapAddr, kptr_t patchAddr);
   void removeTrapToPatchAddrMapping(kptr_t trapAddr);
#endif

   uint32_t peek1Word(kptr_t address) const;
   
   uint32_t peek_block(void *dest, kptr_t startAddr, uint32_t len,
		       bool allow_partial) const;
      // returns actual number of bytes read (in particular, 0 on error)

   int poke_block(kptr_t startAddr, uint32_t len, const void *src);

   uint32_t getSymtabStuffSize() const;
   void getSymtabStuff(void *buffer, uint32_t size) const;

   std::pair<kptr_t, kptr_t> allocate_kernel_block(uint32_t nbytes,
						   uint32_t alignment,
						  // common values: 32 (i-cache
						  // block alignment) or 8192
						  // (page-aligned)
						   bool dummy,
						   unsigned allocType);
   std::pair<kptr_t, kptr_t> allocate_kernel_block(uint32_t nbytes,
						   uint32_t alignment,
						   bool tryForNucleusText);
/*
  tryForNucleusText true --> we try, but don't insist on, getting memory
  from within the nucleus text for your guaranteed-i-tlb hit pleasure.
  Return value (pay attention): .first: the kernel address that we have
  allocated.  Should be in the nucleus, if willContainCode is true.
  This is the return value you'd expect.  .second: a kernel address that 
  you can easily mmap for read/write.  This is needed when allocating from 
  nucleus text; result.first won't be mmap'able by map_for_rw(), below. If 
  not allocating from the nucleus text, then result.second will equal
  result.first.  NOTE: If result.first != result.second, take care to
  only pass result.first to free_kernel_block, below.
*/

   void free_kernel_block(kptr_t kernelAddr, kptr_t mappedKernelAddr,
                          uint32_t nbytes);

#ifdef sparc_sun_solaris2_7
   // The following functions allocate/deallocate kernel memory from
   // the 32-bit kernel heap. We use this region as springboard
   // space to instrument the non-nucleus parts of the kernel
   kptr_t allocate_kmem32(uint32_t nbytes);
   void free_kmem32(kptr_t kernelAddr, uint32_t nbytes);
#endif

/*
  1) Note that len *must* be a multiple of the page size
  (sysconf(_SC_PAGESIZE)), or else mmap() gets very cranky.  We could
  round it up for you, but then mmap() gets cranky when and if it finds
  that the end of what it's mapping isn't valid memory, etc.  So the
  only real solution is for you to make sure that what you allocate is
  really a multiple of page size before trying to map it.

  2) Furthermore, mmap() gets cranky if we call mmap() with something
  other than a page-aligned address.  If assertPageAlignedAlready is
  passed in as true, then we'll uphold that crankiness with an assert.
  If false, we'll (if needed) map an extra page, etc. in order to
  guarantee that the entire range you desire is covered by the mmap.
  Presumably you'll never know the difference; we still return a pointer
  to where you'd expect.
*/
   void *map_for_rw(kptr_t addrInKernel, uint32_t len,
                    bool assertPageSizedAndPageAlignedAlready);
   void unmap_from_rw(void *inKerninstd, uint32_t len);

   void *map_for_readonly(kptr_t addrInKernel, uint32_t len,
                          bool assertPageSizedAndPageAlignedAlready);
   void unmap_from_readonly(void *inhere, uint32_t len);

   // convenience functions; helps in the above mmap routines:
   static uint32_t getPageNumBytes();
   static uint32_t roundUpToMultipleOfPageSize(uint32_t);
   static kptr_t addr2Page(kptr_t);

   uint64_t getTickRaw() const;
   uint64_t getStickRaw() const;

#ifdef sparc_sun_solaris2_7
   // Routines to access & alter the PCR (control reg) and PIC (values
   // reg) performance counter registers.  User code cannot always peek &
   // poke directly to these regs, so these routines are needed.

   uint64_t getPcrRawCurrentCPU() const;
   void setPcrRawCurrentCPU(uint64_t); // const?

   uint64_t getPicRaw() const;
   void setPicRaw(uint64_t); // const?
#elif defined(i386_unknown_linux2_4)
   // Routines to read & write processor specific MSRs related to
   // performance counters
   pdvector< std::pair<unsigned, unsigned> > getP4PerfCtrState();
   void setP4PerfCtrState(const pdvector< std::pair<unsigned, unsigned> > &);
#elif defined(ppc64_unknown_linux2_4)
   void getPowerPerfCtrState(pdvector<uint64_t> &);
   void setPowerPerfCtrState(const pdvector<uint64_t> &the_state);
   void cSwitchTest();
#endif

   kptr_t getAllVTimersAddr();
   void addTimerToAllVTimers(kptr_t);
   void removeTimerFromAllVTimers(kptr_t);
   void clearClientState();

   void callOnce();

#if (defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4))
   void toggleDebugBits(uint32_t bitmask);
   void toggleEnableBits(uint32_t bitmask);
#endif
   
   // Obtain miscellaneous machine information and fill *pmi
   void getPresentedMachineInfo(const abi& kernelABI, 
				const abi& kerninstdABI,
				presentedMachineInfo *pmi) const;

   // Return callees recorded for a given indirect call site.
   // For each callee we return its address followed by the number of times it
   // was called
   pdvector<kptr_t> getCalleesForSite(kptr_t siteAddr) const;
   
   // Read the value of a hardware counter across all cpus
   pdvector<uint64_t> readHwcAllCPUs(unsigned type) const;
};

#endif
