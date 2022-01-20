// relocatableCode.h
// Ariel Tamches
// Relocatable code: presently a poor-man's representation of a typical .o file
// (which is by nature relocatable).
// Features:
// 1) there may be calls to external functions, whose address is not known until
//    run time.
// 2) there may be what I call "imm32"s: a 2 instruction sethi/bset of the address
//    of some external function, whose address is not known until run time.
//  2a) a special imm32 symbol (tempBufferEmitter::this_chunk_addr) is always
//      implicitly available for an imm32.
// That's it.  Certainly a poor-man's version of an elf .o file, which has over a
// dozen other relocatable features.  I envision that such features will be added
// to this class as needed, and as the final coup de grace, there's be a feature where
// an ELF .o file can be given as a constructor argument to this class...meaning
// you can bring in anything that you can make a .o file (asm, C, etc.)

// Resolving is done via a user-specified oracle (the template argument to
// the method obtainFullyResolvedCode(), below).  This provides flexibility:
// one oracle might be used for downloading code into kerninstd, while a quite
// different one might be used for downloading code into the kernel itself.

// The general idea is that you start with a tempBufferEmitter, use it to emit
// some code, and when done, you create a relocatableCode object (passing the
// tempBufferEmitter to its ctor) and then call obtainFullyResolvedCode() when
// you're ready to assign the code so a fixed address.

#ifndef _RELOCATABLE_CODE_H_
#define _RELOCATABLE_CODE_H_

#include "common/h/Vector.h"
#include "util/h/Dictionary.h"
#include "util/h/kdrvtypes.h"
#include "instr.h"
#include "tempBufferEmitter.h"

class XDR;

class relocatableCode_t {
 public:
   typedef tempBufferEmitter::insnOffset insnOffset;

 protected:
   static kptr_t absolute_addr(const insnOffset &theInsnOffset,
			       const pdvector<kptr_t> &chunkStartAddrs) {
      const kptr_t chunkStartAddr = chunkStartAddrs[theInsnOffset.chunkNdx];
      return chunkStartAddr + theInsnOffset.byteOffset;
   }
   
   static kaddrdiff_t distance_calc(const insnOffset &to,
				    const insnOffset &from,
				    const pdvector<kptr_t> &chunkStartAddrs) {
      return absolute_addr(to, chunkStartAddrs) -
	     absolute_addr(from, chunkStartAddrs);
   }
   static instr_t *offset2Insn(const insnOffset &off,
                               pdvector<insnVec_t*> &chunks) {
      // used in resolve method.  Note that we pass in "chunks" *on purpose*,
      // instead of using "theChunks", because we want to be able to choose between
      // "theChunks" and "fullyResolvedCodeCache" (and indeed in practice it's
      // usually the latter).
      
      return chunks[off.chunkNdx]->get_by_byteoffset(off.byteOffset);
   }

   pdvector<insnVec_t*> theChunks;

   dictionary_hash<pdstring, insnOffset> labels;
   dictionary_hash<pdstring, kptr_t> knownSymAddrs;

   // Stuff that needs to be resolved:
   pdvector< std::pair<insnOffset, pdstring> > unresolvedBranchesToLabels;
   pdvector< std::pair<insnOffset, pdstring> > unresolvedInterprocBranchesToName;
   pdvector< std::pair<insnOffset, kptr_t> > unresolvedInterprocBranchesToAddr;
   pdvector< std::pair<insnOffset, pdstring> > unresolvedCallsByName;
      // second: name of callee (symAddr or even label OK)
   pdvector< std::pair<insnOffset, kptr_t> > unresolvedCallsByAddr;
      // second: callee addr
   pdvector< std::pair<insnOffset, pdstring> > unresolvedSymAddrs;
      // second: name of the immediate
      //         (could be tempBufferEmitter::this_code_patch_addr)

   pdvector< std::pair<insnOffset, pdstring> > unresolvedLabelAddrsAsData;
   pdvector< std::pair<insnOffset, triple<pdstring, pdstring, unsigned> > > unresolvedLabelAddrMinusLabelAddrThenDivides;

   pdvector< std::pair<insnOffset, pdstring> > unresolvedLabelAddrs;
      // .second: label name

   
   mutable pdvector<insnVec_t*> fullyResolvedCodeCache;

   // Check that instructions at the given offset correspond to the
   // expected call sequence, replace dummy addresses with the valid ones
   // in-place
   static void resolve1Call(const insnOffset &offs,
			    pdvector<insnVec_t*> &chunks,
			    kptr_t insnAddr,
			    kptr_t calleeAddr,
                            kptr_t calleeAddrAtEmitTime
                               // only used for asserting
                            );
   // Check that instructions at the given offset correspond to the
   // expected immediate address generation, replace dummy addresses with 
   // the valid ones in-place
   static void resolve1SetImmAddr(const insnOffset &offs, 
				  pdvector<insnVec_t*> &chunks,
				  kptr_t symAddr);

   void fillLabels(const pdvector< std::pair<pdstring, insnOffset> > &ilabels);
   void fillKnownSymAddrs(const pdvector< std::pair<pdstring, kptr_t> > 
			  &iknownSymAddrs);

 private:  
   // private to ensure they're not called:
   relocatableCode_t& operator=(const relocatableCode_t &src);
   
 public:
   explicit relocatableCode_t(const tempBufferEmitter &em);

   class Dummy {};
   static Dummy _dummy;
   relocatableCode_t(Dummy);
   static relocatableCode_t* getRelocatableCode(Dummy);

   relocatableCode_t(const relocatableCode_t &src); // yes, we really do need this (nuts)
   static relocatableCode_t* getRelocatableCode(const relocatableCode_t &src);
   
   virtual ~relocatableCode_t() { 
      //clean up vectors that contain pointers
      for (unsigned chunk_lcv=0; chunk_lcv < theChunks.size(); ++chunk_lcv) {
         delete theChunks[chunk_lcv];
      }
      for (unsigned cache_lcv=0; cache_lcv < fullyResolvedCodeCache.size(); ++cache_lcv) {
         delete fullyResolvedCodeCache[cache_lcv];
      }
   }

   bool isDummy() const {
      return theChunks.size() == 0;
   }
   
   unsigned numChunks() const { return theChunks.size(); }
   unsigned chunkNumBytes(unsigned chunk_ndx) const {
      return theChunks[chunk_ndx]->numInsnBytes();
   }

   const insnVec_t* getChunkOne() const { return theChunks[0]; } 

   virtual bool send(XDR *) const = 0;
   static relocatableCode_t* getRelocatableCode(XDR *);
   static void putRelocatableCode(XDR *, relocatableCode_t &);

   // NOTE: it's nice that the resolver abilities are in a template.
   // Perhaps we should ALSO template how we "emit"; that is: returning a copy
   // of the emitted code isn't always as efficient as it could be; why not
   // have the ability for this method to emit directly to a directToKernelEmitter,
   // when desired?  Perhaps an "emitterOracle?"
   template <class resolverOracle>
   const pdvector<insnVec_t*> &
   obtainFullyResolvedCode(const pdvector<kptr_t> &chunkStartAddrs, // an entry per chunk
			   const resolverOracle &theResolverOracle) const;
        // theResolverOracle must have certain methods, which this method will invoke:
        // (1) std::pair<bool, kptr_t> obtainFunctionAddr(const pdstring &fnName)
        // (2) std::pair<bool, kptr_t> obtainImmAddr(const pdstring &objectName)
        // presumably, these methods will search some kind of runtime symbol table for the
        // given symbol names, and return their address.
        // Internally: updates code cache & returns a reference to it.
};


//We'd really like this function to be virtual, but since it
//isn't, we are forced into somewhat ugly hack that accomplishes same

#include "reloc.h"

template <class resolverOracle>
const pdvector<insnVec_t*> & relocatableCode_t::
obtainFullyResolvedCode(const pdvector<kptr_t> &chunkStartAddrs, // an entry per chunk
			const resolverOracle &theResolverOracle) const {
#ifdef sparc_sun_solaris2_7
   return ((const sparc_relocatableCode *)this)->obtainFullyResolvedCode(chunkStartAddrs, theResolverOracle);
#elif defined(i386_unknown_linux2_4)
   return ((const x86_relocatableCode *)this)->obtainFullyResolvedCode(chunkStartAddrs, theResolverOracle);
#elif defined(ppc64_unknown_linux2_4)
   return ((const power_relocatableCode *)this)->obtainFullyResolvedCode(chunkStartAddrs, theResolverOracle);
#endif
}

#endif // _RELOCATABLE_CODE_H_
