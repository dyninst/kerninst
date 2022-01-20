// condBranchCodeObject.h
// NOTE: only for those branches that are truly conditional.  branchAlways's and
// branchNever's need not apply.

#ifndef _CODE_BRANCH_CODE_OBJECT_H_
#define _CODE_BRANCH_CODE_OBJECT_H_

#include "codeObject.h"
#include "instr.h"

class condBranchCodeObject : public codeObject {
 private:
   bbid_t iftaken_bb_id;

   instr_t *insn;
   // in lieu of storing just the controlFlowInfo, which is tough with no
   // copy ctor for that subclass.
   instr_t::controlFlowInfo *cfi;

   bool delaySlotInThisBlock;
      // usually true...false --> the delay slot is in another basic block, or
      // perhaps even in an entirely different function(!), and we'll have to emit
      // our own copy of the delay slot, for use by this basic block's branch.
      // Unfortunate to copy an instruction, but the silver lining is that the new
      // version of the code won't have this bizarre sequence.
   instr_t *dsInsn; // must be pc-independent!
   regset_t *availRegsBeforeCodeObject;
   regset_t *availRegsAfterCodeObject;

   void emitWithInRangeTakenAddress(bool tryToKeepOriginalCode,
                                    const function_t &parentFn,
                                    bbid_t owning_bb_id,
                                    tempBufferEmitter &,
                                    const outliningLocations &whereBlocksNowReside,
                                    bbid_t fallThruBlockId
                                    // NOT nec. same as block that'll be emitted next.
                                    // Instead, it complements iftaken_bb_id.
                                    ) const;

   // prevent copying:
   condBranchCodeObject(const condBranchCodeObject &);
   condBranchCodeObject &operator=(const condBranchCodeObject &);
   
 protected:
   uint32_t getCodeObjectTypeID() const {
      return condBranchCodeObjectID;
   }
   
 public:
   condBranchCodeObject(bbid_t iftaken_bb_id,
                        instr_t *insn,
                        instr_t::controlFlowInfo *cfi,
                        bool delaySlotInThisBlock,
                        instr_t *dsInsn,
                        regset_t *iavailRegsBefore,
                        regset_t *iavailRegsAfter);
   condBranchCodeObject(XDR *);

   codeObject *dup() const { return new condBranchCodeObject(*this); }
   
   ~condBranchCodeObject() {
	delete insn;
	delete dsInsn;
	delete cfi;
	delete availRegsBeforeCodeObject;
	delete availRegsAfterCodeObject;
   }
   
   bool send(bool, XDR *) const;
   
   void emitWhenRelocatedTo(bool tryToKeepOriginalCode,
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
