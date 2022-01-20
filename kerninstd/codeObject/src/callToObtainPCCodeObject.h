// callToObtainPCCodeObject.h

#ifndef _CALL_TO_OBTAIN_PC_CODE_OBJECT_H_
#define _CALL_TO_OBTAIN_PC_CODE_OBJECT_H_

#include "codeObject.h"
#include "instr.h"

class callToObtainPCCodeObject : public codeObject {
 private:
   // Strangley enough, we only need to store stuff about the delay slot instruction:
   bool delayInsnInSameBlock;
   instr_t *dsInsn;
   
   callToObtainPCCodeObject(const callToObtainPCCodeObject &src) :
      codeObject(src), delayInsnInSameBlock(src.delayInsnInSameBlock){ 
      dsInsn = instr_t::getInstr(*(src.dsInsn));
   }
   callToObtainPCCodeObject &operator=(const callToObtainPCCodeObject &);
   
 protected:
   uint32_t getCodeObjectTypeID() const {
      return callToObtainPCCodeObjectID;
   }

 public:
   callToObtainPCCodeObject(bool idelayInsnInSameBlock,
                            instr_t *idsInsn);
   callToObtainPCCodeObject(XDR *);
  ~callToObtainPCCodeObject() { delete dsInsn; }

   codeObject *dup() const { return new callToObtainPCCodeObject(*this); }

   bool send(bool, XDR *) const;

   void emitWhenRelocatedTo(bool tryToKeepOriginalCode, // true iff testing
                            const function_t &parentFn,
                            bbid_t owning_bb_id,
                            tempBufferEmitter &em,
                            const outliningLocations &whereBlocksNowReside,
                            bool thisIsLastCodeObjectInBlock,
                            bbid_t fallThruBlockId
                               // NOT nec. the same as the block that'll get
                               // emitted next
                            ) const;
};


#endif
