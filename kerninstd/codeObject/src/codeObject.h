// codeObject.h
// Base class for a high-level description of a consecutive sequence of one or more
// instructions.  Each codeObject knows how to relocate itself to any address.
// The tricky part is then emitting code to jump to what used to be the fall-through
// address, if needed.  The trouble is that we may not know where this new basic
// block resides (assume it's been relocated).  We can get around this, however,
// with a two-prong strategy:
// (1) outlined blocks should get emitted first; when/if it needs to jump back to
//     a hot block, whose addr it won't know, it emits a long jump to an unresolved
//     variable.
// (2) inlined blocks are emitted last.  If it needs to jump to an outlined block, it's
//     easy, because the address is known.  If it needs to jump to an inlined block, it
//     will emit a single-insn unresolved branch instruction, assuming the provided
//     displacement will be enough.

#ifndef _CODE_OBJECT_H_
#define _CODE_OBJECT_H_

#include "tempBufferEmitter.h"
#include <typeinfo> // STL type_info
#include "outliningLocations.h"

struct XDR;
class instr_t;
class reg_t;
class regset_t;

class codeObject {
 public:
   typedef outliningLocations::bbid_t bbid_t;

 private:
   // NOTE: No codeObject should store anything such as "initialInsnAddr", because
   // that defeats the purpose of storing only the very highest-level-possible
   // meta-data-only description of the codeObject.

   // prevent copying (though see dup())
   codeObject &operator=(const codeObject &);
   
 protected:
   static bbid_t read1_bb_id(XDR *);

   codeObject(const codeObject &) {} // this will be defined, but stay protected

   void emitGettingToIntraProcFallee(tempBufferEmitter &,
                                     const outliningLocations &whereBlocksNowReside,
                                     regset_t *availRegs,
                                     bbid_t ourBlockId,
                                     bbid_t destBlockId) const;
      // A helper routine that derived codeObjects will find useful, though some,
      // such as conditional branch codeObjects, will want to emit their own clever
      // optimized version.
      // It is hoped that this routine won't have to emit any code (destBlockId
      // is being emitted immediately after outBlockId).

   void emitGettingToInterProcFallee(tempBufferEmitter &,
                                     const outliningLocations &,
                                     bool leaveDelaySlotOpen,
                                        // only pass true if you know we can achieve
                                        // this (which is if availRegs contains at
                                        // least 1 scratch int reg)
                                     const regset_t &availRegs,
                                     const function_t &parentFn,
                                     bbid_t ourBlockId) const;

   void emitSetRegToInterProcFalleeAddr(tempBufferEmitter &,
                                        const outliningLocations &,
                                        reg_t &dest_reg,
                                        regset_t &availRegs,
                                           // we might need a scratch reg to set a
                                           // 64-bit address, for example
                                        const function_t &parentFn,
                                        bbid_t ourBlockId) const;

   virtual bool send(bool, XDR*) const = 0;

   static const uint32_t pcIndepWithFallThruCodeObjectID = 100;
   static const uint32_t pcIndepNoFallThruCodeObjectID = 101;
   static const uint32_t interProcCallToAddrCodeObjectID = 102;
   static const uint32_t interProcCallToNameCodeObjectID = 103;
   static const uint32_t interProcTailCallToAddrCodeObjectID = 104;
   static const uint32_t interProcTailCallToNameCodeObjectID = 105;
   static const uint32_t callToObtainPCCodeObjectID = 106;
   static const uint32_t condBranchCodeObjectID = 107;
   static const uint32_t branchAlwaysCodeObjectID = 108;
   static const uint32_t interProcCondBranchToAddrCodeObjectID = 109;
   static const uint32_t interProcCondBranchToNameCodeObjectID = 110;
   static const uint32_t interProcBranchAlwaysToAddrCodeObjectID = 111;
   static const uint32_t interProcBranchAlwaysToNameCodeObjectID = 112;
   static const uint32_t simpleJumpTable32CodeObjectID = 113;
   static const uint32_t offsetJumpTable32CodeObjectID = 114;
   static const uint32_t recursiveCallCodeObjectID = 115;
   static const uint32_t intraProcJmpToFixedAddrCodeObjectID = 116;

   virtual uint32_t getCodeObjectTypeID() const = 0;
   
 public:
   codeObject() {}
   virtual ~codeObject() {}

   virtual codeObject *dup() const = 0;

   static codeObject *make(XDR *xdr);
      // receives canonical string representing the typename
   
   bool send(XDR *xdr) const;
      // sends an unsigned representing the typename, and then the info.
      // Needed due to polymorphism.

   virtual unsigned exactNumBytesForPreFunctionData() const {
      return 0;
   }
   
   virtual void
   emitWhenRelocatedTo(bool tryToKeepOriginalCode,
                          // true iff testing; turns off optimizations
                       const function_t &parentFn,
                       bbid_t owning_bb_id,
                       tempBufferEmitter &em,
                       const outliningLocations &whereBlocksNowReside,
                       bool thisIsLastCodeObjectInBlock,
                          // if true, then we need to worry about emitting code to
                          // reach what used to be the fall-through block (if we're no
                          // longer just before that block, then we need some kind of
                          // jump!)
                       bbid_t fallThruBlockId
                          // Listen up: this is NOT the necessarily same as the block
                          // that will be emitted immediately after this one (if it
                          // were, this param wouldn't be needed; it could be deduced
                          // from whereBlocksNowReside, above).  Instead, this is the
                          // basic block that WAS emitted immediately after this one
                          // in the original code.  (This may or may not be the same
                          // now that we have outlined a bunch of blocks.)
                       ) const = 0;
      // We no longer pass in regsFreeBefore & After the codeObject, leaving
      // it to the individual codeObject to store and/or calculate on-the-fly these
      // things.
};

#endif
