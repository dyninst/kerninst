// interProcBranchAlwaysCodeObject.h

#ifndef _INTER_PROC_BRANCH_ALWAYS_CODE_OBJECT_H_
#define _INTER_PROC_BRANCH_ALWAYS_CODE_OBJECT_H_

#include "codeObject.h"
#include "instr.h"

class interProcBranchAlwaysCodeObjectBase : public codeObject {
 protected:
   instr_t *originsn;
      // matters when "tryToKeepOriginalCode" is true.  Could be bicc, bpcc, or floating
      // point variants thereof.
   bool hasDelaySlot;
   instr_t *dsInsn; // must be pc-independent!

   regset_t *availIntRegsBeforeCodeObject;
      // Needed when emitting an equivalent jump, when the delay insn is going to
      // be put in the open delay slot
   regset_t *availIntRegsAfterCodeObject;
      // needed when emitting an equivalent jump after having emitted
      // the delay insn already.

   // prevent copying (though see dup())
   interProcBranchAlwaysCodeObjectBase(const interProcBranchAlwaysCodeObjectBase &src);
      // this will be defined but stay private
   interProcBranchAlwaysCodeObjectBase &operator=(const interProcBranchAlwaysCodeObjectBase &);

   virtual bool willBranchDefinitelyStillFit(const outliningLocations &) const = 0;
   virtual void emitBranchInsnAssumeWillFit(tempBufferEmitter &em,
                                            bool tryToKeepOriginalCode) const = 0;
   virtual void emitLongJumpEquivUsing1RegLeaveDelayOpen(tempBufferEmitter &,
                                                         sparc_reg r) const = 0;
   virtual void emitLongJumpEquivUsing0RegsIgnoreDelay(tempBufferEmitter &) const = 0;

 public:
   interProcBranchAlwaysCodeObjectBase(instr_t *insn,
                                       bool delayInsnToo,
                                       instr_t *dsInsn,
                                       regset_t *iavailRegsBeforeCodeObject,
                                       regset_t *iavailRegsAfterCodeObject);
   interProcBranchAlwaysCodeObjectBase(XDR *);

   virtual ~interProcBranchAlwaysCodeObjectBase() {
      delete originsn;
      delete dsInsn;
      delete availIntRegsBeforeCodeObject;
      delete availIntRegsAfterCodeObject;
   }
   
   virtual bool send(bool, XDR *) const = 0;
      // we supply a version; derived class must supply some extra

   // derived class does NOT override the following method:
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

// ----------------------------------------------------------------------

class interProcBranchAlwaysToAddrCodeObject : public interProcBranchAlwaysCodeObjectBase {
 private:
   typedef interProcBranchAlwaysCodeObjectBase base_class;
   kptr_t destAddr;

   bool willBranchDefinitelyStillFit(const outliningLocations &) const;
   void emitBranchInsnAssumeWillFit(tempBufferEmitter &em,
                                            bool tryToKeepOriginalCode) const;
   void emitLongJumpEquivUsing1RegLeaveDelayOpen(tempBufferEmitter &,
                                                 sparc_reg r) const;
   void emitLongJumpEquivUsing0RegsIgnoreDelay(tempBufferEmitter &) const;

   // prevent copying (though see dup())
   interProcBranchAlwaysToAddrCodeObject(const interProcBranchAlwaysToAddrCodeObject &src);
      // this will be defined but stay private
   interProcBranchAlwaysToAddrCodeObject &operator=(const interProcBranchAlwaysToAddrCodeObject &);
   
 protected:
   uint32_t getCodeObjectTypeID() const {
      return interProcBranchAlwaysToAddrCodeObjectID;
   }
   
 public:
   interProcBranchAlwaysToAddrCodeObject(kptr_t destAddr,
                                         instr_t *insn,
                                         bool delayInsnToo,
                                         instr_t *dsInsn,
                                         regset_t *iavailRegsBeforeCodeObject,
                                         regset_t *iavailRegsAfterCodeObject);
   interProcBranchAlwaysToAddrCodeObject(XDR *);

   codeObject *dup() const { return new interProcBranchAlwaysToAddrCodeObject(*this); }

   bool send(bool, XDR *) const;
};

// ----------------------------------------------------------------------

class interProcBranchAlwaysToNameCodeObject : public interProcBranchAlwaysCodeObjectBase {
 private:
   typedef interProcBranchAlwaysCodeObjectBase base_class;
   pdstring destName; // of the form "modName/fnName"
   kptr_t destOriginalAddress;
      // this field makes me nervous; quite a kludge.  It means the address of
      // "destName", *before* destName was outlined (we're kinda assuming that destName
      // is, like us, being relocated/outlined).  Ugh; assumptions, assumptions.

   bool willBranchDefinitelyStillFit(const outliningLocations &) const;
   void emitBranchInsnAssumeWillFit(tempBufferEmitter &,
                                            bool tryToKeepOriginalCode) const;
   void emitLongJumpEquivUsing1RegLeaveDelayOpen(tempBufferEmitter &,
                                                 sparc_reg r) const;
   void emitLongJumpEquivUsing0RegsIgnoreDelay(tempBufferEmitter &) const;

   // prevent copying (though see dup())
   interProcBranchAlwaysToNameCodeObject(const interProcBranchAlwaysToNameCodeObject &src);
      // this will be defined but stay private
   interProcBranchAlwaysToNameCodeObject &operator=(const interProcBranchAlwaysToNameCodeObject &);
   
 protected:
   uint32_t getCodeObjectTypeID() const {
      return interProcBranchAlwaysToNameCodeObjectID;
   }
   
 public:
   interProcBranchAlwaysToNameCodeObject(bool, // temp dummy param
                                         const pdstring &destName, // form "modNm/fnName"
                                         kptr_t destOriginalAddress,
                                         instr_t *insn,
                                         bool delayInsnToo,
                                         instr_t *dsInsn,
                                         regset_t *iavailRegsBeforeCodeObject,
                                         regset_t *iavailRegsAfterCodeObject);
   interProcBranchAlwaysToNameCodeObject(XDR *);

   codeObject *dup() const { return new interProcBranchAlwaysToNameCodeObject(*this); }

   bool send(bool, XDR *) const;
};

#endif
