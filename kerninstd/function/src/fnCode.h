// fnCode.h
// A class for managing the code of a function, which can sometimes be complex -- a
// series of non-contiguous chunks in the case of an outlined function for example
// NOTE: 0-sized chunks are NOT allowed; don't try to add them.

#ifndef _FN_CODE_H_
#define _FN_CODE_H_

#include <inttypes.h> // uint32_t
#include <utility> // pair
using std::pair;
#include "common/h/Vector.h"
#include "util/h/kdrvtypes.h"
#include "insnVec.h"
#include "archTraits.h" // kaddr_t

struct XDR;

class fnCode {
 public:
   class Empty {}; // for ctor
   static Empty empty;
   class BadAddr {}; // for throwing an exception

   struct codeChunk {
      kaddr_t startAddr;
      insnVec_t *theCode;
      mutable bool theCode_owner;

      typedef insnVec_t::const_iterator const_iterator;

      codeChunk(kaddr_t istartAddr, insnVec_t *iCode, bool iOwner=true);
      codeChunk(const codeChunk &src);
      codeChunk(XDR *xdr);
      ~codeChunk() { 
         if(theCode_owner) {
            delete theCode;
            theCode = NULL;
         }
      }

      unsigned getMemUsage_exceptObjItself() const {
         return theCode->getMemUsage_exceptObjItself();
      }

      bool send(XDR *xdr) const;

      unsigned numBytes() const {
         return theCode->numInsnBytes();
      }
      instr_t* get1Insn(kaddr_t addr) const {
         assert(theCode->numInsns() != 0);
         const unsigned offset = addr - startAddr;
         return theCode->get_by_byteoffset(offset);
      }
      instr_t* get1InsnByNdx(unsigned index) const {
	 assert(theCode->numInsns() != 0);
         return theCode->get_by_ndx(index);
      }
      bool enclosesAddr(kaddr_t addr) const {
         return ((addr >= startAddr) && 
		 (addr < startAddr + theCode->numInsnBytes()));
      }
      bool enclosesRange(kaddr_t low, kaddr_t hi) const {
         // "hi" is 1 byte **past** end of range (STL iterator style)
         assert(hi >= low);
         return ((low >= startAddr) && 
		 (hi <= startAddr + theCode->numInsnBytes()));
      }

      unsigned calcNumInsnsWithinChunk(kaddr_t low, kaddr_t finish) const;
      unsigned calcNumInsnsWithinBasicBlock(kaddr_t low, kaddr_t finish) const {
         assert(finish >= low);
            // in case caller mistakenly passes numBytes for last parameter
         return calcNumInsnsWithinChunk(low, finish);
      }
      
      const_iterator begin() const { return theCode->begin(); }
      const_iterator end() const { return theCode->end(); }
   };

   typedef pdvector<codeChunk>::iterator iterator;
   typedef pdvector<codeChunk>::const_iterator const_iterator;

 private:
   pdvector<codeChunk> chunks; // kept sorted at all times (even initialization(!))

   static bool chunkcmp_sorting(const codeChunk &c1, const codeChunk &c2) {
      return c1.startAddr < c2.startAddr;
   }
   void sortChunks();
   void shrink1ChunkIfAppropriate(kaddr_t, unsigned);

 public:
   fnCode(XDR *);
   fnCode(Empty) {
      // chunks left empty
   }
   fnCode(const pdvector<codeChunk> &ichunks) : chunks(ichunks) {
      sortChunks();
   }
   fnCode(const codeChunk &ichunk) : chunks(1, ichunk) {
   }
   fnCode(const fnCode &src) : chunks(src.chunks) {
   }
   
  ~fnCode() {
   }

   fnCode &operator=(const fnCode &src) {
      if (this != &src) {
         chunks = src.chunks;
      }
      return *this;
   }
   
   void ownerCopy(const fnCode &src);

   unsigned getMemUsage_exceptObjItself() const;

   void addChunk(const codeChunk &c, bool resortNow);
   void addChunk(kaddr_t addr, insnVec_t *theCode, bool resortNow);
   
   void sortChunksNow();
   
   bool send(XDR *) const;

   pdvector< pair<kptr_t, unsigned> > getCodeRanges() const;
   
   unsigned totalNumBytes_fromChunks() const;
      // Iterates thru chunks.  Include, e.g., any data embedded within the chunks.
      // If you just want code, check out a method within class function_t, not here.

   bool isEmpty() const {
      return chunks.size() == 0;
   }
   unsigned numChunks() const {
      return chunks.size();
   }
   
   unsigned calcNumInsnsWithinChunk(kaddr_t startAddr, kaddr_t finishAddr) const;
      // finishAddr is 1 byte *past* the end of the last insn (STL style)
   unsigned calcNumInsnsWithinBasicBlock(kaddr_t bbStartAddr,
                                         kaddr_t bbFinishAddr) const {
      assert(bbFinishAddr >= bbStartAddr);
         // will catch the case where the caller mistakenly passes numBytes
         // for the second parameter.

      return calcNumInsnsWithinChunk(bbStartAddr, bbFinishAddr);
   }
   
   void shrinkChunksIfAppropriate(const pdvector< pair<kptr_t,unsigned> > &nChunkSizes);
   
   const_iterator findChunk(kaddr_t addr, bool definitelyStartOfSomeChunk) const;

   instr_t *get1Insn(kaddr_t addr) const {
      // First find the right chunk
      const_iterator iter = findChunk(addr, false);
         // false --> can't assume that "addr" is the start of some chunk
         // will return end() if not found
      if (iter == chunks.end())
         throw BadAddr();

      return iter->get1Insn(addr);
   }

   bool enclosesAddr(kaddr_t addr) const {
      const_iterator iter = findChunk(addr, false);
         // false --> we're not assuming that "addr" is the start of some chunk
      return (iter != chunks.end());
   }
   bool hasChunkStartingAt(kaddr_t addr) const {
      return findChunk(addr, true) != chunks.end();
   }
   
   bool enclosesRange(kaddr_t low, kaddr_t hi) const;
      // useful for asserts.  hi is 1 byte past the end of last insn, STL style.

   // For now, only const iterators are exposed
   const_iterator begin() const {
      return chunks.begin();
   }
   const_iterator end() const {
      return chunks.end();
   }
};

#endif
