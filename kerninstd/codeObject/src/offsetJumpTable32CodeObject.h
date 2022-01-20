// offsetJumpTable32CodeObject.h

#ifndef _OFFSETJUMPTABLE32_CODE_OBJECT_H_
#define _OFFSETJUMPTABLE32_CODE_OBJECT_H_

#include "jumpTableCodeObject.h"
#include "instr.h"

class offsetJumpTable32CodeObject : public jumpTableCodeObject {
 private:
   instr_t *instr;
   bool delaySlotInThisBlock;
   instr_t *delayInsn;
   
   pdvector<bbid_t> jumpTableBlockIds;
      // we fiddle with this at runtime, as jump tables are trimmed, etc.
      // But by emit time, this must be stable.

   // TODO: The following assumes that an address is specified with sethi/bset;
   // that's not valid for 64-bit addresses!!!

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

   offsetJumpTable32CodeObject(const offsetJumpTable32CodeObject &);
      // will be defined (used by dup()) but stays private
   offsetJumpTable32CodeObject &operator=(const offsetJumpTable32CodeObject &);
   
 protected:
   uint32_t getCodeObjectTypeID() const {
      return offsetJumpTable32CodeObjectID;
   }
   
 public:
   offsetJumpTable32CodeObject(kptr_t jumpTableDataStartAddr,
                               // normally a no-no to use absolute addrs in
                               // code objects; this is just for passing to
                               // the base class jumpTableCodeObject, which'll
                               // only use it to detect & trim jump tables that
                               // (appear to) overlap
                               instr_t *instr,
                               bool delaySlotInThisBlock,
                               instr_t *delaySlotInsn,
                               const pdvector<bbid_t> &jumpTableBlockIds,
                               bbid_t isetHiInsnBBId,
                               unsigned isetHiInsnCodeObjectNdxWithinBB,
                               unsigned isetHiInsnByteOffsetWithinCodeObject,
                               bbid_t ibsetOrAddInsnBBId,
                               unsigned ibsetOrAddCodeObjectNdxWithinBB,
                               unsigned ibsetOrAddByteOffsetWithinCodeObject);

   offsetJumpTable32CodeObject(XDR *);
   
  ~offsetJumpTable32CodeObject() { 
     delete instr;
     delete delayInsn;
  }

   codeObject *dup() const { return new offsetJumpTable32CodeObject(*this); }

   bool send(bool, XDR *) const;

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

