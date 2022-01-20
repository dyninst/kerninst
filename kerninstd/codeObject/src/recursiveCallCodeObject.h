// recursiveCallCodeObject.h

#ifndef _RECURSIVE_CALL_CODE_OBJECT_H_
#define _RECURSIVE_CALL_CODE_OBJECT_H_

#include "codeObject.h"
#include "instr.h"

class recursiveCallCodeObject : public codeObject {
 private:
   bbid_t entry_bb_id;

   bool delaySlotInSameBlock;
   instr_t *dsInsn;

   regset_t *availRegsAfterCodeObject;
   
   recursiveCallCodeObject(const recursiveCallCodeObject &);
      // will be defined but stays private
   recursiveCallCodeObject &operator=(const recursiveCallCodeObject &);
   
 protected:
   uint32_t getCodeObjectTypeID() const {
      return recursiveCallCodeObjectID;
   }
   
 public:
   recursiveCallCodeObject(bbid_t entry_bb_id,
                           bool idelaySlotInSameBlock,
                           instr_t *idsInsn,
                           regset_t *iavailRegsAfterCodeObject);
   recursiveCallCodeObject(XDR *);
  ~recursiveCallCodeObject() {
     delete dsInsn;
     delete availRegsAfterCodeObject;
  }

   codeObject *dup() const { return new recursiveCallCodeObject(*this); }
   
   bool send(bool, XDR *) const;

   void emitWhenRelocatedTo(bool tryToKeepOriginalCode,
                            const function_t &parentFn,
                            bbid_t owning_bb_id,
                            tempBufferEmitter &em,
                            const outliningLocations &whereBlocksNowReside,
                            bool thisIsLastCodeObjectInBlock,
                            bbid_t fallThruBlockId
                            ) const;
};


#endif
