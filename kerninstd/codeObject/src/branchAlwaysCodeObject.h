// branchAlwaysCodeObject.h

#ifndef _BRANCH_ALWAYS_CODE_OBJECT_H_
#define _BRANCH_ALWAYS_CODE_OBJECT_H_

#include "codeObject.h"
#include "instr.h"

class branchAlwaysCodeObject : public codeObject {
 private:
   bbid_t iftaken_bb_id;
   instr_t *originsn;
      // matters when "tryToKeepOriginalCode" is true.  Could be bicc, bpcc, or floating
      // point variants thereof.
   bool hasDelaySlot, delaySlotInThisBlock;
   instr_t *dsInsn; // must be pc-independent!

   regset_t *availRegsBeforeCodeObject;
      // Needed when emitting an equivalent jump, when the delay insn is going to
      // be put in the open delay slot
   regset_t *availRegsAfterCodeObject;
      // needed when emitting an equivalent jump after having emitted
      // the delay insn already.

   // prevent copying (though see dup())
   branchAlwaysCodeObject(const branchAlwaysCodeObject &src);
      // this will be defined but stay private
   branchAlwaysCodeObject &operator=(const branchAlwaysCodeObject &);
   
 protected:
   uint32_t getCodeObjectTypeID() const {
      return branchAlwaysCodeObjectID;
   }
   
 public:
   branchAlwaysCodeObject(bbid_t iftaken_bb_id,
                          instr_t *insn,
                          bool delayInsnToo, bool delaySlotInThisBlock,
                          instr_t *dsInsn,
                          regset_t *iavailRegsBeforeCodeObject,
                          regset_t *iavailRegsAfterCodeObject);
   branchAlwaysCodeObject(XDR *);

   codeObject *dup() const { return new branchAlwaysCodeObject(*this); }
   
   ~branchAlwaysCodeObject () { 
      delete dsInsn;
      delete availRegsBeforeCodeObject;
      delete availRegsAfterCodeObject;
   }

   bool send(bool, XDR *) const;

   void emitWhenRelocatedTo(bool tryToKeepOriginalCode,
                               // true iff testing; we turn off optimizations
                            const function_t &parentFn,
                            bbid_t owning_bb_id,
                            tempBufferEmitter &em,
                            const outliningLocations &whereBlocksNowReside,
                            bool thisIsLastCodeObjectInBlock,
                            bbid_t fallThruBlockId
                               // NOT nec. same as block that'll be emitted next.
                               // Instead, it complements iftaken_bb_id.
                            ) const;
};

#endif
