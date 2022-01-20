// jumpTableCodeObject.h

#ifndef _JUMP_TABLE_CODE_OBJECT_H_
#define _JUMP_TABLE_CODE_OBJECT_H_

#include "codeObject.h"

class jumpTableCodeObject : public codeObject {
 private:
   kptr_t jumpTableDataStartAddr;
      // private; not even derived classes can access it.  We store an absolute addr
      // like this (normally a no-no for codeObjects) for one reason only: to be able
      // handle calls to maybeAdjustJumpTableSize().

   virtual void shrinkJumpTableSizeIfAppropriate(unsigned maxNumBytes) = 0;
   
 protected:
   jumpTableCodeObject(kptr_t ijumpTableDataStartAddr) :
      jumpTableDataStartAddr(ijumpTableDataStartAddr) {}
   jumpTableCodeObject(XDR *);
   
   jumpTableCodeObject(const jumpTableCodeObject &src) :
      codeObject(src),
      jumpTableDataStartAddr(src.jumpTableDataStartAddr) {
   }

 public:
   virtual ~jumpTableCodeObject() {}

   virtual bool send(bool, XDR *) const;

   virtual unsigned exactNumBytesForPreFunctionData() const = 0;

   void maybeAdjustJumpTableSize(kptr_t otherJumpTableStartAddr) {
      if (otherJumpTableStartAddr == jumpTableDataStartAddr)
         assert(false); // unexpected
      else if (otherJumpTableStartAddr > jumpTableDataStartAddr)
         shrinkJumpTableSizeIfAppropriate(otherJumpTableStartAddr - jumpTableDataStartAddr);
   }
   
};

#endif
