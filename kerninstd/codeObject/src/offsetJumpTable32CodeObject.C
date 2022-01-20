// offsetJumpTable32CodeObject.C

#include "offsetJumpTable32CodeObject.h"
#include "util/h/xdr_send_recv.h" // the fancier P_xdr_send/recv routines
#include "util/h/out_streams.h"
#include "xdr_send_recv_common.h"

#include "sparc_tempBufferEmitter.h"

typedef codeObject::bbid_t bbid_t;

offsetJumpTable32CodeObject::
offsetJumpTable32CodeObject(kptr_t jumpTableDataStartAddr,
                            instr_t *iinstr,
                            bool idelaySlotInThisBlock,
                            instr_t *idelayInsn,
                            const pdvector<bbid_t> &ijumpTableBlockIds,
                            bbid_t isetHiInsnBBId,
                            unsigned isetHiCodeObjectNdxWithinBB,
                            unsigned isetHiInsnByteOffsetWithinCodeObject,
                            bbid_t ibsetOrAddInsnBBId,
                            unsigned ibsetOrAddCodeObjectNdxWithinBB,
                            unsigned ibsetOrAddInsnByteOffsetWithinCodeObject) :
   jumpTableCodeObject(jumpTableDataStartAddr),
   instr(iinstr),
   delaySlotInThisBlock(idelaySlotInThisBlock),
   delayInsn(idelayInsn),
   jumpTableBlockIds(ijumpTableBlockIds),
   setHiInsnBBId(isetHiInsnBBId),
   setHiCodeObjectNdxWithinBB(isetHiCodeObjectNdxWithinBB),
   setHiInsnByteOffsetWithinCodeObject(isetHiInsnByteOffsetWithinCodeObject),
   bsetOrAddInsnBBId(ibsetOrAddInsnBBId),
   bsetOrAddCodeObjectNdxWithinBB(ibsetOrAddCodeObjectNdxWithinBB),
   bsetOrAddInsnByteOffsetWithinCodeObject(ibsetOrAddInsnByteOffsetWithinCodeObject) {
}

offsetJumpTable32CodeObject::
offsetJumpTable32CodeObject(const offsetJumpTable32CodeObject &src) :
   jumpTableCodeObject(src),
   delaySlotInThisBlock(src.delaySlotInThisBlock),
   jumpTableBlockIds(src.jumpTableBlockIds),
   setHiInsnBBId(src.setHiInsnBBId),
   setHiCodeObjectNdxWithinBB(src.setHiCodeObjectNdxWithinBB),
   setHiInsnByteOffsetWithinCodeObject(src.setHiInsnByteOffsetWithinCodeObject),
   bsetOrAddInsnBBId(src.bsetOrAddInsnBBId),
   bsetOrAddCodeObjectNdxWithinBB(src.bsetOrAddCodeObjectNdxWithinBB),
   bsetOrAddInsnByteOffsetWithinCodeObject(src.bsetOrAddInsnByteOffsetWithinCodeObject) {
   instr = instr_t::getInstr(*(src.instr));
   delayInsn = instr_t::getInstr(*(src.delayInsn));
}

static bbid_t read1_bbid(XDR *xdr) {
   bbid_t result;
   if (!P_xdr_recv(xdr, result))
      throw xdr_recv_fail();
   return result;
}

static sparc_instr readIt_sparc_instr(XDR *xdr) {
   sparc_instr result(sparc_instr::nop);
   result.~sparc_instr();

   if (!P_xdr_recv(xdr, result))
      throw xdr_recv_fail();
   return result;
}

template <class T>
static void destruct1(T &t) {
   t.~T();
}

static pdvector<bbid_t> readIt_vec_bbid(XDR *xdr) {
   pdvector<bbid_t> result;
   destruct1(result);
   
   if (!P_xdr_recv(xdr, result))
      throw xdr_recv_fail();
   
   return result;
}

offsetJumpTable32CodeObject::offsetJumpTable32CodeObject(XDR *xdr) :
   jumpTableCodeObject(xdr),
   delaySlotInThisBlock(read1_bool(xdr)),
   jumpTableBlockIds(readIt_vec_bbid(xdr)),
   setHiInsnBBId(read1_bbid(xdr)),
   setHiCodeObjectNdxWithinBB(read1_unsigned(xdr)),
   setHiInsnByteOffsetWithinCodeObject(read1_unsigned(xdr)),
   bsetOrAddInsnBBId(read1_bbid(xdr)),
   bsetOrAddCodeObjectNdxWithinBB(read1_unsigned(xdr)),
   bsetOrAddInsnByteOffsetWithinCodeObject(read1_unsigned(xdr)) {
   instr = (instr_t*) new sparc_instr(readIt_sparc_instr(xdr));
   delayInsn = (instr_t*) new sparc_instr(readIt_sparc_instr(xdr));
}

bool offsetJumpTable32CodeObject::send(bool, XDR *xdr) const {
   return (jumpTableCodeObject::send(false, xdr) &&
           P_xdr_send(xdr, instr) &&
           P_xdr_send(xdr, delaySlotInThisBlock) &&
           P_xdr_send(xdr, delayInsn) &&
           P_xdr_send(xdr, jumpTableBlockIds) &&
           P_xdr_send(xdr, setHiInsnBBId) &&
           P_xdr_send(xdr, setHiCodeObjectNdxWithinBB) &&
           P_xdr_send(xdr, setHiInsnByteOffsetWithinCodeObject) &&
           P_xdr_send(xdr, bsetOrAddInsnBBId) &&
           P_xdr_send(xdr, bsetOrAddCodeObjectNdxWithinBB) &&
           P_xdr_send(xdr, bsetOrAddInsnByteOffsetWithinCodeObject));
}

void offsetJumpTable32CodeObject::
shrinkJumpTableSizeIfAppropriate(unsigned maxNumBytes) {
   const unsigned numBytesPerJumpTableEntry = 4;
   const unsigned currSize = jumpTableBlockIds.size() * numBytesPerJumpTableEntry;
   
   if (maxNumBytes < currSize) {
      dout << "offsetJumpTable32CodeObject: shrinking from " << currSize
           << " to " << maxNumBytes << " bytes!" << endl;
      const unsigned newNumEntries = maxNumBytes / numBytesPerJumpTableEntry;
      
      jumpTableBlockIds.shrink(newNumEntries);
   }
}

// --------------------

void offsetJumpTable32CodeObject::
emitWhenRelocatedTo(bool /*tryToKeepOriginalCode*/,
                    const function_t & /*parentFn*/,
                    bbid_t owning_bb_id,
                    tempBufferEmitter &em,
                    const outliningLocations &/*whereBlocksNowReside*/,
                    bool thisIsLastCodeObjectInBlock,
                    bbid_t /*fallThruBlockId*/
                       // NOT nec. the same as the block that'll get emitted
                       // next.
                    ) const {
   instr_t *instr_copy = instr_t::getInstr(*instr);
   em.emit(instr_copy);
   if (delaySlotInThisBlock) {
      instr_t *delayInsn_copy = instr_t::getInstr(*delayInsn);
      em.emit(delayInsn_copy);
   }
   
   assert(thisIsLastCodeObjectInBlock);

   // Next we emit the contents of the new jump table.
   // Remember that the contents will be offsets from the address of the jump
   // instruction

   const pdstring startOfJumpTableLabel =
      outliningLocations::getJumpTableStartOfDataLabel(owning_bb_id);

   const unsigned save_chunk_ndx = em.getCurrChunkNdx();
   em.switchToChunk(0); // 0 --> the pre-fn data (jump table stuff) chunk
   em.emitLabel(startOfJumpTableLabel);

   pdvector<bbid_t>::const_iterator iter = jumpTableBlockIds.begin();
   pdvector<bbid_t>::const_iterator finish = jumpTableBlockIds.end();
   for (; iter != finish; ++iter) {
      const bbid_t succ_bb_id = *iter;

      // Note that we're still in the data-before-fn chunk, which is what we want
      // for the following:
      ((sparc_tempBufferEmitter&)em).emitLabelAddrMinusLabelAddrDividedBy
         (outliningLocations::getBlockLabel(succ_bb_id),
             // yes, succ_bb_id, not owning_bb_id
          startOfJumpTableLabel,
             // reminder: offset32 jump table entries
             // are byte offsets from the start of the
             // jump table, not from the jump insn.
          1 // don't need to divide by anything
            // (the shift amount was to get a load
            // offset, not a jump offset)
          );
   }

   // back to the regular chunk:
   em.switchToChunk(save_chunk_ndx);

   // Now it's time to patch up the sethi/bset or sethi/add instructions that
   // set some register to the start address of the jump table, which isn't
   // yet known (since absolute addrs are never known in this class), though we have
   // set a label to it.
   // Those instructions may, or may not, have already been emitted.
   // 
   // The use of instruction offsets within a basic block is a little fragile,
   // considering that the whole point of code objects to avoid such dependencies
   // (perhaps in the new code, the sethi/bset instructions won't quite be at the
   // same instruction offsets due to some optimization, and since some control flow
   // instructions [callCodeObject] *very often* emit code with a changed number of
   // instructions).  Fortunately, they will be at the same instruction offsets within
   // a particular *code object* (just not the same offset within a particular bb).
   // In other words, the triple [bbid, code object ndx within that bb,
   // insn ndx within that code object] doesn't change.  We have that information
   // already stored, and we convert it to a label name + offset for backpatching.
   // (label name of the code object, not of the basic block).

   // (TODO: THIS CODE IS NOT GENERIC ENOUGH TO HANDLE 64-BIT ADDRESSES).

   const pdstring codeObjContainingSetHiLabelName =
      outliningLocations::getCodeObjectLabel(setHiInsnBBId,
                                             setHiCodeObjectNdxWithinBB);
   
   const pdstring codeObjContainingBSetOrAddLabelName =
      outliningLocations::getCodeObjectLabel(bsetOrAddInsnBBId,
                                             bsetOrAddCodeObjectNdxWithinBB);
   
   sparc_tempBufferEmitter::setHiBSetOrAddLocation
      setHiBSetOrAddLoc(codeObjContainingSetHiLabelName,
                        setHiInsnByteOffsetWithinCodeObject,
                        codeObjContainingBSetOrAddLabelName,
                        bsetOrAddInsnByteOffsetWithinCodeObject);

   ((sparc_tempBufferEmitter&)em).lateBackpatchOfSetHiBSetOrAddToLabelAddrPlusOffset(setHiBSetOrAddLoc,
                                                         startOfJumpTableLabel,
                                                         0);
      // 0 --> the byte offset from "startOfJumpTableLabel"
   
   // That's about all we need to do.
   assert(thisIsLastCodeObjectInBlock);
}
