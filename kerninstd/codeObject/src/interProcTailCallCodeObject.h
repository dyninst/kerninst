// interProcTailCallCodeObject.h
// call; restore    or    call; mov xxx, %o7
// True calls only (not jmpl)

#ifndef _INTERPROC_TAIL_CALL_CODE_OBJECT_BASE_H_
#define _INTERPROC_TAIL_CALL_CODE_OBJECT_BASE_H_

#include "codeObject.h"
#include "instr.h"

class interProcTailCallCodeObjectBase : public codeObject {
 private:
   bool delaySlotInThisBlock;
   instr_t *delayInsn;
      // had better be pc-independent!
      // must be either a restore or something that writes to %o7

   interProcTailCallCodeObjectBase &operator=(const interProcTailCallCodeObjectBase &);
 protected:
   interProcTailCallCodeObjectBase(const interProcTailCallCodeObjectBase &);
      // will be defined but stays protected; derived classes will call this
   
 public:
   interProcTailCallCodeObjectBase(bool idelaySlotInThisBlock,
                                   instr_t *idelayInsn);
   interProcTailCallCodeObjectBase(XDR *);

   virtual ~interProcTailCallCodeObjectBase() {
      delete delayInsn;
   }
   
   virtual bool send(bool, XDR *) const = 0;
      // we define some; derived class must define more

   // derived class does *NOT* override this one:
   void emitWhenRelocatedTo(bool tryToKeepOriginalCode,
                            const function_t &parentFn,
                            bbid_t owning_bb_id,
                            tempBufferEmitter &em,
                            const outliningLocations &whereBlocksNowReside,
                            bool thisIsLastCodeObjectInBlock,
                            bbid_t fallThruBlockId
                               // NOT nec. the same as the block that'll get
                               // emitted next
                            ) const;

   // derived classes must define this:
   virtual void emitCallInsn(bool tryToKeepOriginalCode,
                             bbid_t owning_bb_id,
                             tempBufferEmitter &em,
                             const outliningLocations &,
                             bool leaveDelaySlotOpen) const = 0;
};

// ----------------------------------------------------------------------

class interProcTailCallToAddrCodeObject : public interProcTailCallCodeObjectBase {
 private:
   typedef interProcTailCallCodeObjectBase base_class;
   kptr_t calleeAddr;

   interProcTailCallToAddrCodeObject(const interProcTailCallToAddrCodeObject &);
      // will be defined but stays private
   interProcTailCallToAddrCodeObject &operator=(const interProcTailCallToAddrCodeObject &);
   
 protected:
   uint32_t getCodeObjectTypeID() const {
      return interProcTailCallToAddrCodeObjectID;
   }
   
 public:
   interProcTailCallToAddrCodeObject(kptr_t icalleeAddr,
                                     bool idelayInsnToo,
                                     instr_t *idelayInsn);
   interProcTailCallToAddrCodeObject(XDR *);

   codeObject *dup() const { return new interProcTailCallToAddrCodeObject(*this); }

   bool send(bool, XDR *) const;
   
   void emitCallInsn(bool tryToKeepOriginalCode,
                     bbid_t owning_bb_id,
                     tempBufferEmitter &em,
                     const outliningLocations &,
                     bool leaveDelaySlotOpen) const;
};

// ----------------------------------------------------------------------

class interProcTailCallToNameCodeObject : public interProcTailCallCodeObjectBase {
 private:
   typedef interProcTailCallCodeObjectBase base_class;
   pdstring calleeSymName;

   interProcTailCallToNameCodeObject(const interProcTailCallToNameCodeObject &);
      // will be defined but stays private
   interProcTailCallToNameCodeObject &operator=(const interProcTailCallToNameCodeObject &);
   
 protected:
   uint32_t getCodeObjectTypeID() const {
      return interProcTailCallToNameCodeObjectID;
   }
   
 public:
   interProcTailCallToNameCodeObject(const pdstring &calleeSymName,
                                     bool idelayInsnToo,
                                     instr_t *idelayInsn);
   interProcTailCallToNameCodeObject(XDR *);

   codeObject *dup() const { return new interProcTailCallToNameCodeObject(*this); }

   bool send(bool, XDR *) const;
   
   void emitCallInsn(bool tryToKeepOriginalCode,
                     bbid_t owning_bb_id,
                     tempBufferEmitter &em,
                     const outliningLocations &,
                     bool leaveDelaySlotOpen) const;
};

#endif
