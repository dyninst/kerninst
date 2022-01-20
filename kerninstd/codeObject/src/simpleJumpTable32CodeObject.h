// simpleJumpTable32CodeObject.h

#ifndef _SIMPLE_JUMP_TABLE_CODE_OBJECT_H_
#define _SIMPLE_JUMP_TABLE_CODE_OBJECT_H_

#include "jumpTableCodeObject.h"
#include "instr.h"

class simpleJumpTable32CodeObject : public jumpTableCodeObject {
 private:
   instr_t *instr;
   bool delaySlotInThisBlock;
   instr_t *dsInsn;
   
   pdvector<bbid_t> jumpTableBlockIds;
      // we fiddle with this at runtime, as jump tables are trimmed, etc.
      // But by emit time, this must be stable.
      // Duplicates are *not* weeded out; this vector must contain duplicate bbs, or
      // else we don't be able to correctly emit the jump table!
   
   bbid_t setHiInsnBBId;
   unsigned setHiCodeObjectNdxWithinBB;
   unsigned setHiInsnByteOffsetWithinCodeObject;

   bbid_t bsetOrAddInsnBBId;
   unsigned bsetOrAddCodeObjectNdxWithinBB;
   unsigned bsetOrAddInsnByteOffsetWithinCodeObject;

   void shrinkJumpTableSizeIfAppropriate(unsigned maxNumBytes);
      // required by the base class

   unsigned exactNumBytesForPreFunctionData() const {
      const unsigned jumpTableEntryNumBytes=4;
      return jumpTableBlockIds.size() * jumpTableEntryNumBytes;
   }
   
   simpleJumpTable32CodeObject(const simpleJumpTable32CodeObject &);
      // will be defined (used by dup()) but stays private
   simpleJumpTable32CodeObject &operator=(const simpleJumpTable32CodeObject &);

 protected:
   uint32_t getCodeObjectTypeID() const {
      return simpleJumpTable32CodeObjectID;
   }

 public:
   simpleJumpTable32CodeObject(kptr_t jumpTableDataStartAddr,
                               // normally a no-no to use absolute addrs in
                               // code objects; this is just for passing to
                               // the base class jumpTableCodeObject, which'll
                               // only use it to detect & trim jump tables that
                               // (appear to) overlap
                               instr_t *instr,
                               bool delaySlotInThisBlock,
                               instr_t *dsInsn,
                               bbid_t isetHiInsnBBId,
                               unsigned isetHiInsnCodeObjectNdxWithinBB,
                               unsigned isetHiInsnByteOffsetWithinCodeObject,
                               bbid_t ibsetOrAddInsnBBId,
                               unsigned ibsetOrAddCodeObjectNdxWithinBB,
                               unsigned ibsetOrAddByteOffsetWithinCodeObject);
   simpleJumpTable32CodeObject(XDR *);
  ~simpleJumpTable32CodeObject() {
     delete instr;
     delete dsInsn;
  }

   codeObject *dup() const { return new simpleJumpTable32CodeObject(*this); }

   bool send(bool, XDR *) const;

   bool addAnEntry(bbid_t);
      // returns true iff a duplicate entry (useful info for the caller)

   void emitWhenRelocatedTo(bool tryToKeepOriginalCode,
                            const function_t &parentFn,
                            bbid_t owning_bb_id,
                            tempBufferEmitter &em,
                            const outliningLocations &whereBlocksNowReside,
                            bool thisIsLastCodeObjectInBlock,
                            bbid_t fallThruBlockId
                               // NOT nec. the same as the block that'll get emitted
                               // next.
                            ) const;
};

#endif
