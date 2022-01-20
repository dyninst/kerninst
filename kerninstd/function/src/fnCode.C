// fnCode.C

#include <algorithm> // STL's sort()
#include "fnCode.h"
#include "util/h/xdr_send_recv.h" // the fancier P_xdr_send/recv variants

// ----------------------------------------------------------------------

fnCode::codeChunk::codeChunk(kaddr_t istartAddr, insnVec_t *iCode, 
                             bool iOwner) :
   startAddr(istartAddr), theCode(iCode), theCode_owner(iOwner) {
}

fnCode::codeChunk::codeChunk(const codeChunk &src) :
   startAddr(src.startAddr), theCode(src.theCode) {
   // default copy constructor does not transfer ownership of theCode
   theCode_owner = false;
}

kaddr_t read1_kaddr(XDR *xdr) {
   kaddr_t result;
   if (!P_xdr_recv(xdr, result))
      throw xdr_recv_fail();
   
   return result;
}

fnCode::codeChunk::codeChunk(XDR *xdr) : startAddr(read1_kaddr(xdr))
{
   theCode = insnVec_t::getInsnVec(xdr);
   theCode_owner = true;
}

unsigned fnCode::getMemUsage_exceptObjItself() const {
   unsigned result = 0;
   result += chunks.capacity() * sizeof(codeChunk);
   
   const_iterator iter = begin();
   const_iterator finish = end();
   for (; iter != finish; ++iter) {
      const codeChunk &c = *iter;
      result += c.getMemUsage_exceptObjItself();
   }

   return result;
}

void fnCode::addChunk(const codeChunk &c, bool resortNow) {
   assert(c.numBytes() > 0 && "fnCode: 0-sized chunks not allowed");
   chunks += c;
   if (resortNow)
      sortChunks();
}

void fnCode::addChunk(kaddr_t addr,
		      insnVec_t *theCode,
		      bool resortNow) {
   assert(theCode->numInsnBytes() > 0 && "fnCode: 0-sized chunks not allowed");
   
   iterator iter = chunks.append_with_inplace_construction();
   (void)new((void*)iter)codeChunk(addr, theCode);
   if (resortNow)
      sortChunks();
}

void fnCode::sortChunksNow() {
   sortChunks();
}

bool fnCode::codeChunk::send(XDR *xdr) const {
   return P_xdr_send(xdr, startAddr) &&
      theCode->send(xdr);
}

unsigned fnCode::codeChunk::calcNumInsnsWithinChunk(kaddr_t low,
						    kaddr_t finish) const {
   // finishAddr is 1 byte *past* the end of the last insn (STL style)
   assert(finish >= low);
   assert(enclosesRange(low, finish));

   const unsigned insnndx1 = theCode->byteOffset2InsnNdx(finish - startAddr);
   const unsigned insnndx0 = theCode->byteOffset2InsnNdx(low - startAddr);

   return insnndx1 - insnndx0;
}

// ----------------------------------------------------------------------

fnCode::Empty fnCode::empty;
// static variable definition

template <class T>
void adestruct1(T &t) {
   t.~T();
}

void fnCode::ownerCopy(const fnCode &src) {
   pdvector<codeChunk>::const_iterator iter = src.chunks.begin();
   pdvector<codeChunk>::const_iterator fin = src.chunks.end();
   for (; iter != fin; ++iter) {
      const codeChunk &c = *iter;
      iterator iter = chunks.append_with_inplace_construction();
      (void)new((void*)iter)codeChunk(c.startAddr, c.theCode, true);
      c.theCode_owner = false;
   }
}

fnCode::fnCode(XDR *xdr) {
   adestruct1(chunks);

   if (!P_xdr_recv_ctor(xdr, chunks))
      throw xdr_recv_fail();
}

bool fnCode::send(XDR *xdr) const {
   return P_xdr_send_method(xdr, chunks);
}

class chunkcmp_definitelystartchunk {
 private:
   typedef fnCode fnCode_t;
   
 public:
   chunkcmp_definitelystartchunk() {}
   
   bool operator()(const fnCode_t::codeChunk &theChunk,
                   kaddr_t addr) const {
      // return true iff "theChunk" is "<" than "addr"
      // Since we're sure that "addr" is the start of some chunk, we can just:
      return theChunk.startAddr < addr;
   }

   // in case you're wondering, both cummutative versions are present in order
   // to satisfy equal_range()

   bool operator()(kaddr_t addr, const fnCode_t::codeChunk &theChunk) const {
      // return true iff "addr" is "<" than "theChunk"
      return addr < theChunk.startAddr;
   }
};

unsigned fnCode::
calcNumInsnsWithinChunk(kaddr_t startAddr, kaddr_t finishAddr) const {
   // finishAddr is 1 byte *past* the end of the last insn (STL style)
   if (startAddr == finishAddr)
      return 0;
   
   const_iterator chunkiter = findChunk(startAddr, false);
   assert(chunkiter == findChunk(finishAddr-1, false));

   return chunkiter->calcNumInsnsWithinChunk(startAddr, finishAddr);
}

void fnCode::sortChunks() {
   std::sort(chunks.begin(), chunks.end(), chunkcmp_sorting);
}

void fnCode::
shrink1ChunkIfAppropriate(kaddr_t chunkStartAddr, unsigned newnumbytes) {
   chunkcmp_definitelystartchunk foo;
      // definitelystartchunk --> we're sure that "chunkStartAddr" is the start of
      // some chunk (makes comparison a bit quicker).
      // We use equal_range so we'll be able to skip past zero-sized chunks
      // conveniently.
   std::pair<pdvector<codeChunk>::iterator, 
             pdvector<codeChunk>::iterator> res =
      std::equal_range(chunks.begin(), chunks.end(), chunkStartAddr, foo);

   pdvector<codeChunk>::iterator iter = res.first;
   pdvector<codeChunk>::iterator finish = res.second;
   for (; iter != finish; ++iter) {
      codeChunk &c = *iter; // not const; we may change the contents
      
      if (c.numBytes() == 0) // skip past a zero-sized chunk (e.g. empty pre-fn data)
         continue;
      else {
         assert(c.startAddr == chunkStartAddr);
         assert(c.numBytes() >= newnumbytes); // a very useful assert
         c.theCode->shrinkToBytes(newnumbytes, true);
            // true --> yes, aggressively free up memory if we're shrinking
         return;
      }
   }
   assert(false && "chunk not found?");
}

void fnCode::
shrinkChunksIfAppropriate(const pdvector< std::pair<kptr_t,unsigned> > &nChunkSizes) {
   pdvector< std::pair<kptr_t,unsigned> >::const_iterator iter = nChunkSizes.begin();
   pdvector< std::pair<kptr_t,unsigned> >::const_iterator finish = nChunkSizes.end();
   for (; iter != finish; ++iter) {
      shrink1ChunkIfAppropriate(iter->first, iter->second);
      // params: chunk start addr, chunk numbytes
   }
}

pdvector< std::pair<kptr_t, unsigned> > 
fnCode::getCodeRanges() const {
   pdvector< std::pair<kptr_t, unsigned> > result;
   result.reserve_exact(chunks.size());
      
   const_iterator citer = begin();
   const_iterator cfinish = end();
   for (; citer != cfinish; ++citer) {
      const kptr_t startAddr = citer->startAddr;
      const unsigned numbytes = citer->theCode->numInsnBytes();
         
      result += std::make_pair(startAddr, numbytes);
   }

   return result;
}

unsigned fnCode::totalNumBytes_fromChunks() const {
   // Iterates thru chunks.  Include, e.g., any data embedded within the chunks.
   // Compare to totalNumInsnBytes_fromBlocks()
   unsigned result = 0;
   const_iterator iter = begin();
   const_iterator finish = end();
   for (; iter != finish; ++iter) {
      const codeChunk &c = *iter;
      result += c.numBytes();
   }
   return result;
}


class chunkcmp_notnecessarilystartchunk {
   // notnecessarilystartchunk --> we're not assuming that "addr" is the start of
   // some chunk, so our code is a tad more complex than chunkcmp_definitelystartchunk()
 private:
   typedef fnCode fnCode_t;
   
 public:
   bool operator()(const fnCode_t::codeChunk &theChunk, kaddr_t addr) const {
      // return true if "theChunk" is "<" "addr".

      // Curiously, the code for this routine is more complex than the commutative
      // version that follows us.

      // CAUTION: we often have zero-sized dummy chunks (startAddr=0 and nbytes=0).
      // Beware of subtracting 1; you'll end up with unsigned underflow, and probably
      // an undesired result.  Hence the "<=" below instead of "... -1 < addr"
   
      return theChunk.startAddr + theChunk.numBytes() <= addr;
   }

   // in case you're wondering, we have both cummutative versions in order to
   // satisfy equal_range()

   bool operator()(kaddr_t addr, const fnCode_t::codeChunk &theChunk) const {
      // return true iff "addr" is "<" "theChunk".
      return addr < theChunk.startAddr;
   }
};


fnCode::const_iterator
fnCode::findChunk(kaddr_t addr, bool definitelyStartOfSomeChunk) const {
   // returns chunks.end() if not found

   // Making the common case (1 chunk) fast:
   if (chunks.size() == 1) {
      const_iterator iter = begin();
      const codeChunk &c = *iter;

      kaddr_t chunkStartAddr = c.startAddr;
      const unsigned chunkNumBytes = c.numBytes();

      // Note how the "<" below nicely forces a chunk of size 0 to *not* match.
      // This is what we want.
      if (addr >= chunkStartAddr && addr < chunkStartAddr + chunkNumBytes) {
         // match!
         if (definitelyStartOfSomeChunk)
            assert(addr == chunkStartAddr);

         return iter;
      }
      else
         return end();
   }
   else if (definitelyStartOfSomeChunk) {
      // This version is a *tad* faster
      chunkcmp_definitelystartchunk foo;
         
      std::pair<const_iterator,const_iterator> p = 
         std::equal_range(chunks.begin(), chunks.end(), addr, foo);

      const_iterator iter = p.first;
      const_iterator finish = p.second;
      for (; iter != finish; ++iter) {
         if (iter->numBytes() == 0) // ignore empty chunks (e.g. empty pre-fn data)
            continue;
         else {
            assert(iter->startAddr == addr);
            return iter;
         }
      }
      return end();
   }
   else {
      chunkcmp_notnecessarilystartchunk foo;
         
      std::pair<const_iterator,const_iterator> p = 
         std::equal_range(chunks.begin(), chunks.end(), addr, foo);

      const_iterator iter = p.first;
      const_iterator finish = p.second;
      for (; iter != finish; ++iter) {
         if (iter->numBytes() == 0) // ignore empty chunks (e.g. empty pre-fn data)
            continue;
         else {
            assert(addr >= iter->startAddr &&
                   addr < iter->startAddr + iter->numBytes());
            return iter;
         }
      }
      
      return end();
   }
}
   
bool fnCode::enclosesRange(kaddr_t low, kaddr_t hi) const {
   // Reminder: "hi" is 1 byte **past** the end (STL iterator style)
   assert(hi >= low);

   pdvector<codeChunk>::const_iterator hiiter = findChunk(hi-1, false);
      // false --> we're not assuming that "hi" is the start of some chunk
      // -1 because "hi" is 1 byte **past** the end
   if (hiiter == chunks.end())
      return false;
      
   // A weird special case when low==hi (i.e. when we're searching for a range of zero
   // bytes)...weird, but one that we must handle properly.
   if (low == hi) {
      // as long as hiiter had a match (which it did if we got this far), then we can
      // return true, ignoring "low".
      return true;
   }
   
   // Find the chunk encompassing low.
   pdvector<codeChunk>::const_iterator lowiter = findChunk(low, false);
      // false --> we're not assuming that "low" is the start of some chunk
   if (lowiter == chunks.end())
      return false;
   
   // Note that chunks are sorted.  Now simply loop from lowiter to hiiter,
   // making sure there are no gaps.
   if (lowiter == hiiter)
      // no gaps since just one chunk
      return true;
   
   while (lowiter != hiiter-1) {
      const kaddr_t curr_iter_finish =
         lowiter->startAddr + lowiter->theCode->numInsnBytes();

      const_iterator nextiter = lowiter + 1;
      
      // assert sorted as expected:
      assert(lowiter->startAddr < nextiter->startAddr);
      assert(curr_iter_finish <= nextiter->startAddr);
      
      if (curr_iter_finish != nextiter->startAddr)
         // found a gap
         return false;

      lowiter = nextiter;
   }

   // no gaps
   return true;
}
