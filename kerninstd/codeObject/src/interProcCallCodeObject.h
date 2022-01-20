// interProcCallCodeObjectBase.h

#ifndef _INTER_PROC_CALL_CODE_OBJECT_BASE_H_
#define _INTER_PROC_CALL_CODE_OBJECT_BASE_H_

#include "codeObject.h"
#include "instr.h"

class interProcCallCodeObjectBase : public codeObject {
 private:
   // the destination is specific to the derived class (an address or a symName)
   // But the following stuff is common:
   bool delayInsnToo;
   instr_t *delayInsn; // had better be pc-independent!
   regset_t *availRegsAfterCodeObject;

   interProcCallCodeObjectBase &operator=(const interProcCallCodeObjectBase &);

 protected:
   bool callIsToARoutineReturningOnCallersBehalf;
      // usually false; if true, then we assume that the call doesn't return as usual.

   interProcCallCodeObjectBase(const interProcCallCodeObjectBase &);
   
   interProcCallCodeObjectBase(bool idelayInsnToo,
                               instr_t *idelayInsn,
                               regset_t *iavailRegsAfterCodeObject,
                               bool callIsToARoutineReturningOnCallersBehalf);
   interProcCallCodeObjectBase(XDR *);
      // we receive everything except the dest identifier (name or addr)

   virtual ~interProcCallCodeObjectBase() {
      delete delayInsn;
      delete availRegsAfterCodeObject;
   }

   virtual bool send(bool, XDR *) const = 0;
      // we supply a version that sends everything except the dest identifier
   
   // Derived classes must supply this:
   virtual void emitCallInsn(bool tryToKeepOriginalCode,
                             bbid_t owning_bb_id,
                             tempBufferEmitter &em,
                             const outliningLocations &whereBlocksNowReside) const = 0;

 public:
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
      // derived class does *NOT* override this one
};

// ----------------------------------------------------------------------

class interProcCallToAddrCodeObject : public interProcCallCodeObjectBase {
 private:
   typedef interProcCallCodeObjectBase base_class;
   
   kptr_t calleeAddr;

   interProcCallToAddrCodeObject(const interProcCallToAddrCodeObject &);
      // will be defined but stays private
   interProcCallToAddrCodeObject &operator=(const interProcCallToAddrCodeObject &);

 protected:
   uint32_t getCodeObjectTypeID() const {
      return interProcCallToAddrCodeObjectID;
   }

 public:
   interProcCallToAddrCodeObject(kptr_t icalleeAddr,
                                 bool idelayInsnToo,
                                 instr_t *idelayInsn,
                                 regset_t *iavailRegsAfterCodeObject,
                                 bool callIsToARoutineReturningOnCallersBehalf);
   interProcCallToAddrCodeObject(XDR *);

   codeObject *dup() const { return new interProcCallToAddrCodeObject(*this); }

   bool send(bool, XDR *) const;
   
   // Base class requires that we supply this:
   void emitCallInsn(bool tryToKeepOriginalCode,
                     bbid_t owning_bb_id,
                     tempBufferEmitter &em,
                     const outliningLocations &whereBlocksNowReside) const;
};

// ----------------------------------------------------------------------

class interProcCallToNameCodeObject : public interProcCallCodeObjectBase {
 private:
   typedef interProcCallCodeObjectBase base_class;
   
   pdstring calleeSymName;
      // we'll emit a call to this 'symAddr32' (tempBufferEmitter terminology)

   interProcCallToNameCodeObject(const interProcCallToNameCodeObject &);
      // will be defined but stays private
   interProcCallToNameCodeObject &operator=(const interProcCallToNameCodeObject &);

 protected:
   uint32_t getCodeObjectTypeID() const {
      return interProcCallToNameCodeObjectID;
   }

 public:
   interProcCallToNameCodeObject(const pdstring &calleeSymName,
                                 bool idelayInsnToo,
                                 instr_t *idelayInsn,
                                 regset_t *iavailRegsAfterCodeObject,
                                 bool callIsToARoutineReturningOnCallersBehalf);
   interProcCallToNameCodeObject(XDR *);

   codeObject *dup() const { return new interProcCallToNameCodeObject(*this); }

   bool send(bool, XDR *) const;
   
   // Base class requires that we supply this:
   void emitCallInsn(bool tryToKeepOriginalCode,
                     bbid_t owning_bb_id,
                     tempBufferEmitter &em,
                     const outliningLocations &whereBlocksNowReside) const;
};

// ----------------------------------------------------------------------

#endif
