// nonLoadingBackwardsJumpTableCodeObject.h
xxx;

#ifndef _NON_LOADING_BACKWARDS_JUMP_TABLE_CODE_OBJECT_H_
#define _NON_LOADING_BACKWARDS_JUMP_TABLE_CODE_OBJECT_H_

#include "codeObject.h"
#include "instr.h"

class nonLoadingBackwardsJumpTableCodeObject : public codeObject {
 private:
   instr_t jmpInsn;
   bool delaySlotInThisBlock;
   instr_t delayInsn;

   // Information on where we jump to:
   bbid_t finishDestAddrBlockId;
   unsigned finishDestAddrCodeObjNdxWithinBlock;
   unsigned finishDestAddrByteOffsetWithinCodeObj;
   
 public:
   
};



#endif
