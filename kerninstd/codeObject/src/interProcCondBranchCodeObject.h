// interProcCondBranchCodeObject.h
// For truly conditional branches only; look elsewhere for branch-always or branch-never

#ifndef _INTER_PROC_COND_BRANCH_CODE_OBJECT_H_
#define _INTER_PROC_COND_BRANCH_CODE_OBJECT_H_

#include "codeObject.h"
#include "instr.h"

class interProcCondBranchCodeObjectBase : public codeObject {
 protected:
   instr_t *origBranchInsn;

   // For a truly conditional branch, a delay slot instruction always exists.
   // There's only one reason for the "delayInsnToo" flag: if false, then it means
   // that a basic block was split on its delay slot.
   bool delayInsnToo;
   instr_t *dsInsn; // must be pc-independent!

   regset_t *availIntRegsBeforeCodeObject;
   regset_t *availIntRegsAfterBranchInsnBeforeDelayInsn;
   regset_t *availIntRegsAfterCodeObject;

   // prevent copying:
   interProcCondBranchCodeObjectBase(const interProcCondBranchCodeObjectBase &);
   interProcCondBranchCodeObjectBase &operator=(const interProcCondBranchCodeObjectBase &);
   
   interProcCondBranchCodeObjectBase(instr_t *insn,
                                     bool delayInsnToo,
                                        // only false if bb was split on the delay slot
                                     instr_t *dsInsn,
                                     regset_t *iavailRegsBefore,
                                     regset_t *iavailRegsAfterBranchInsn,
                                     regset_t *iavailRegsAfter);
   interProcCondBranchCodeObjectBase(XDR *);

   virtual bool send(bool, XDR *) const = 0;

   // derived classes do *NOT* override this one:
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

   virtual bool willBranchDefinitelyStillFit(const outliningLocations &) const = 0;
   virtual void emitBranchInsnAssumeWillFit(tempBufferEmitter &,
                                            bool tryToKeepOriginalCode) const = 0;
   virtual void emitAssignIfTakenAddrToReg(tempBufferEmitter &,
                                           sparc_reg) const = 0;
   virtual void emit1InsnCallToIfTakenAddr(tempBufferEmitter &) const = 0;

 public:
   virtual ~interProcCondBranchCodeObjectBase() {
       delete origBranchInsn;
       delete dsInsn;
       delete availIntRegsBeforeCodeObject;
       delete availIntRegsAfterBranchInsnBeforeDelayInsn;
       delete availIntRegsAfterCodeObject;
   }
};

// ----------------------------------------------------------------------

class interProcCondBranchToAddrCodeObject : public interProcCondBranchCodeObjectBase {
 private:
   typedef interProcCondBranchCodeObjectBase base_class;
   kptr_t destAddr;

   bool willBranchDefinitelyStillFit(const outliningLocations &) const;
   void emitBranchInsnAssumeWillFit(tempBufferEmitter &,
                                    bool tryToKeepOriginalCode) const;
   void emitAssignIfTakenAddrToReg(tempBufferEmitter &, sparc_reg) const;
   void emit1InsnCallToIfTakenAddr(tempBufferEmitter &) const;
   
   // prevent copying:
   interProcCondBranchToAddrCodeObject(const interProcCondBranchToAddrCodeObject &src) :
      base_class(src),
      destAddr(src.destAddr) {
   }
   interProcCondBranchToAddrCodeObject &operator=(const interProcCondBranchToAddrCodeObject &);
   
 protected:
   uint32_t getCodeObjectTypeID() const {
      return interProcCondBranchToAddrCodeObjectID;
   }
   
 public:
   interProcCondBranchToAddrCodeObject(kptr_t idestAddr,
                                       instr_t *insn,
                                       bool delayInsnToo,
                                          // always true for a truly conditional branch!
                                       instr_t *dsInsn,
                                       regset_t *iavailRegsBefore,
                                       regset_t *iavailRegsAfterBranchInsn,
                                       regset_t *iavailRegsAfter) :
      base_class(insn, delayInsnToo, dsInsn,
                 iavailRegsBefore, iavailRegsAfterBranchInsn, iavailRegsAfter),
      destAddr(idestAddr) {
   }
   interProcCondBranchToAddrCodeObject(XDR *xdr);

   codeObject *dup() const { return new interProcCondBranchToAddrCodeObject(*this); }
   
   bool send(bool, XDR *xdr) const;
};

// ----------------------------------------------------------------------

class interProcCondBranchToNameCodeObject : public interProcCondBranchCodeObjectBase {
 private:
   typedef interProcCondBranchCodeObjectBase base_class;
   pdstring destName; // a "symAddr", in tempBufferEmitter/relocatableCode terms
   kptr_t destOriginalAddress;
      // this field makes me nervous; quite a kludge.  It means the address of
      // "destName", *before* destName was outlined (we're kinda assuming that destName
      // is, like us, being relocated/outlined).  Ugh; assumptions, assumptions.

   bool willBranchDefinitelyStillFit(const outliningLocations &) const;
   void emitBranchInsnAssumeWillFit(tempBufferEmitter &,
                                    bool tryToKeepOriginalCode) const;
   void emitAssignIfTakenAddrToReg(tempBufferEmitter &, sparc_reg) const;
   void emit1InsnCallToIfTakenAddr(tempBufferEmitter &) const;
   
   // prevent copying:
   interProcCondBranchToNameCodeObject(const interProcCondBranchToNameCodeObject &src) :
      base_class(src),
      destName(src.destName), destOriginalAddress(src.destOriginalAddress) {
   }
   interProcCondBranchToNameCodeObject &operator=(const interProcCondBranchToNameCodeObject &);
   
 protected:
   uint32_t getCodeObjectTypeID() const {
      return interProcCondBranchToNameCodeObjectID;
   }
   
 public:
   interProcCondBranchToNameCodeObject(const pdstring &idestName,
                                       kptr_t idestOriginalAddress,
                                       instr_t *insn,
                                       bool delayInsnToo,
                                          // always true for a truly conditional branch!
                                       instr_t *dsInsn,
                                       regset_t *iavailRegsBefore,
                                       regset_t *iavailRegsAfterBranchInsn,
                                       regset_t *iavailRegsAfter) :
      base_class(insn, delayInsnToo, dsInsn,
                 iavailRegsBefore, iavailRegsAfterBranchInsn, iavailRegsAfter),
      destName(idestName), destOriginalAddress(idestOriginalAddress) {
   }
   interProcCondBranchToNameCodeObject(XDR *xdr);
   
   codeObject *dup() const { return new interProcCondBranchToNameCodeObject(*this); }
   
   bool send(bool, XDR *xdr) const;
};

#endif
