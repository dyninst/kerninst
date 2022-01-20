// intraProcJmpToFixedAddrCodeObject.h

#ifndef _INTRA_PROC_JMP_TO_FIXED_ADDR_CODE_OBJECT_H_
#define _INTRA_PROC_JMP_TO_FIXED_ADDR_CODE_OBJECT_H_

#include "codeObject.h"
#include "instr.h"

class intraProcJmpToFixedAddrCodeObject : public codeObject {
 private:
   bbid_t dest_bbid;
   kptr_t orig_destaddr; // probably redundant w/above, but useful for sanity checking.

   instr_t *orig_jumpinsn; // jmp [R + offset] or jmp [R], never jmp [R1 + R2]
   bool hasDelaySlot;
   instr_t *dsInsn; // must be pc-independent!

   bbid_t setHiInsnBBId;
   unsigned setHiCodeObjectNdxWithinBB;
   unsigned setHiByteOffsetWithinCodeObject;

   bbid_t bsetOrAddInsnBBId;
   unsigned bsetOrAddCodeObjectNdxWithinBB;
   unsigned bsetOrAddByteOffsetWithinCodeObject;

   intraProcJmpToFixedAddrCodeObject(const intraProcJmpToFixedAddrCodeObject &);
   intraProcJmpToFixedAddrCodeObject &operator=(const intraProcJmpToFixedAddrCodeObject &);

   uint32_t getCodeObjectTypeID() const {
      return intraProcJmpToFixedAddrCodeObjectID;
   }
   
 public:
   intraProcJmpToFixedAddrCodeObject(instr_t *originsn,
                                     bbid_t dest_bbid,
                                     kptr_t orig_destaddr,
                                     bool hasDelaySlot,
                                     instr_t *dsInsn,
                                     bbid_t setHiInsnBBId,
                                     unsigned setHiCodeObjectNdxWithinBB,
                                     unsigned setHiByteOffsetWithinCodeObject,
                                     bbid_t bSetOrAddInsnBBId,
                                     unsigned bSetOrAddCodeObjectNdxWithinBB,
                                     unsigned bSetOrAddByteOffsetWithinCodeObject);
   intraProcJmpToFixedAddrCodeObject(XDR *);
   
  ~intraProcJmpToFixedAddrCodeObject() {
     delete orig_jumpinsn;
     delete dsInsn;
  }

   codeObject *dup() const { return new intraProcJmpToFixedAddrCodeObject(*this); }

   bool send(bool, XDR *) const;
   
   void emitWhenRelocatedTo(bool tryToKeepOriginalCode,
                            const function_t &parentFn,
                            bbid_t owning_bb_id,
                            tempBufferEmitter &em,
                            const outliningLocations &whereThingsNowReside,
                            bool thisIsLastCodeObjectInBlock,
                            bbid_t fallThruBlockId
                               // NOT nec. same as block that'll be emitted next.
                               // Instead, it complements iftaken_bb_id.
                            ) const;
};

#endif
