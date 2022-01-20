// simpleJumpTable32CodeObject.C

#include "simpleJumpTable32CodeObject.h"
#include <algorithm> // STL's sort()
#include "util/h/out_streams.h"
#include "util/h/xdr_send_recv.h" // xdr on common stuff
#include "xdr_send_recv_common.h" // xdr on instr_t, e.g.

#include "sparc_tempBufferEmitter.h"

simpleJumpTable32CodeObject::
simpleJumpTable32CodeObject(kptr_t ijumpTableDataStartAddr,
                            instr_t *iinstr,
                            bool idelaySlotInThisBlock,
                            instr_t *idsInsn,
                            bbid_t isetHiInsnBBId,
                            unsigned isetHiInsnCodeObjectNdxWithinBB,
                            unsigned isetHiInsnByteOffsetWithinCodeObject,
                            bbid_t ibsetOrAddInsnBBId,
                            unsigned ibsetOrAddCodeObjectNdxWithinBB,
                            unsigned ibsetOrAddByteOffsetWithinCodeObject) :
   jumpTableCodeObject(ijumpTableDataStartAddr),
   instr(iinstr), delaySlotInThisBlock(idelaySlotInThisBlock),
   dsInsn(idsInsn),
   setHiInsnBBId(isetHiInsnBBId),
   setHiCodeObjectNdxWithinBB(isetHiInsnCodeObjectNdxWithinBB),
   setHiInsnByteOffsetWithinCodeObject(isetHiInsnByteOffsetWithinCodeObject),
   bsetOrAddInsnBBId(ibsetOrAddInsnBBId),
   bsetOrAddCodeObjectNdxWithinBB(ibsetOrAddCodeObjectNdxWithinBB),
   bsetOrAddInsnByteOffsetWithinCodeObject(ibsetOrAddByteOffsetWithinCodeObject)
{
   // jumpTableBlockIds[] is left an empty function
}

simpleJumpTable32CodeObject::
simpleJumpTable32CodeObject(const simpleJumpTable32CodeObject &src) :
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
   dsInsn = instr_t::getInstr(*(src.dsInsn));
}

template <class T>
static void destruct1(T &t) {
   t.~T();
}

simpleJumpTable32CodeObject::
simpleJumpTable32CodeObject(XDR *xdr) :
   jumpTableCodeObject(xdr)
{
   instr = (instr_t*) new sparc_instr(sparc_instr::nop);
   dsInsn = (instr_t*) new sparc_instr(sparc_instr::nop);
   destruct1(*instr);
   destruct1(*dsInsn);
   destruct1(jumpTableBlockIds);
   
   if (!P_xdr_recv(xdr, instr) ||
       !P_xdr_recv(xdr, delaySlotInThisBlock) ||
       !P_xdr_recv(xdr, dsInsn) ||
       !P_xdr_recv(xdr, jumpTableBlockIds) ||
       !P_xdr_recv(xdr, setHiInsnBBId) ||
       !P_xdr_recv(xdr, setHiCodeObjectNdxWithinBB) ||
       !P_xdr_recv(xdr, setHiInsnByteOffsetWithinCodeObject) ||
       !P_xdr_recv(xdr, bsetOrAddInsnBBId) ||
       !P_xdr_recv(xdr, bsetOrAddCodeObjectNdxWithinBB) ||
       !P_xdr_recv(xdr, bsetOrAddInsnByteOffsetWithinCodeObject))
      throw xdr_recv_fail();
}

bool simpleJumpTable32CodeObject::send(bool, XDR *xdr) const {
   return (jumpTableCodeObject::send(false, xdr) &&
           P_xdr_send(xdr, instr) &&
           P_xdr_send(xdr, delaySlotInThisBlock) &&
           P_xdr_send(xdr, dsInsn) &&
           P_xdr_send(xdr, jumpTableBlockIds) &&
           P_xdr_send(xdr, setHiInsnBBId) &&
           P_xdr_send(xdr, setHiCodeObjectNdxWithinBB) &&
           P_xdr_send(xdr, setHiInsnByteOffsetWithinCodeObject) &&
           P_xdr_send(xdr, bsetOrAddInsnBBId) &&
           P_xdr_send(xdr, bsetOrAddCodeObjectNdxWithinBB) &&
           P_xdr_send(xdr, bsetOrAddInsnByteOffsetWithinCodeObject));
}

void simpleJumpTable32CodeObject::
shrinkJumpTableSizeIfAppropriate(unsigned maxNumBytes) {
   const unsigned numBytesPerJumpTableEntry = 4;
   const unsigned currSize = jumpTableBlockIds.size() * numBytesPerJumpTableEntry;
   
   if (maxNumBytes < currSize) {
      dout << "simpleJumpTable32CodeObject: shrinking from " << currSize
           << " to " << maxNumBytes << " bytes!" << endl;
      const unsigned newNumEntries = maxNumBytes / numBytesPerJumpTableEntry;
      
      jumpTableBlockIds.shrink(newNumEntries);
   }
}

bool simpleJumpTable32CodeObject::addAnEntry(bbid_t bbid) {
   // returns true if a duplicate entry (already existed).  Useful for the caller
   // to know this.

   pdvector<bbid_t> sorted_blockIds(jumpTableBlockIds);
   std::sort(sorted_blockIds.begin(), sorted_blockIds.end());
   
   const bool alreadyExisted = std::binary_search(sorted_blockIds.begin(),
                                                  sorted_blockIds.end(), bbid);

   jumpTableBlockIds += bbid;
   
   return alreadyExisted;
}

void simpleJumpTable32CodeObject::
emitWhenRelocatedTo(bool /*tryToKeepOriginalCode*/,
                    const function_t &/*parentFn*/,
                    bbid_t owning_bb_id,
                    tempBufferEmitter &em,
                    const outliningLocations &/*whereBlocksNowReside*/,
                    bool thisIsLastCodeObjectInBlock,
                    bbid_t /*fallThruBlockId*/
                       // NOT nec. the same as the block that'll get emitted
                       // next.
                    ) const {
   instr_t *instr_copy =  instr_t::getInstr(*instr);
   em.emit(instr_copy);
   if (delaySlotInThisBlock) {
      instr_t *dsInsn_copy = instr_t::getInstr(*dsInsn);
      em.emit(dsInsn_copy);
   }
   
   assert(thisIsLastCodeObjectInBlock);
   
   // Emit the contents of the new jump table.  Remember that in a
   // simpleJumpTable32, the contents are absolute addresses to jump to.
   
   const pdstring startOfJumpTableLabel =
      outliningLocations::getJumpTableStartOfDataLabel(owning_bb_id);

   const unsigned save_chunk_ndx = em.getCurrChunkNdx();
   
   em.switchToChunk(0);
      // 0 --> the pre-fn data (jump table stuff) chunk
      // NOTE: In the original code, the jump table wasn't necessarily
      // located before the fn!  Sometimes after the function, and
      // possibly even embedded within the function.
      // So that could cause a testing headache.

   em.emitLabel(startOfJumpTableLabel);

   pdvector<bbid_t>::const_iterator iter = jumpTableBlockIds.begin();
   pdvector<bbid_t>::const_iterator finish = jumpTableBlockIds.end();
   for (; iter != finish; ++iter) {
      const bbid_t succ_bb_id = *iter;

      // Note that we're still in the data-before-fn chunk, which is what we want
      // for the following:
      ((sparc_tempBufferEmitter&)em).emitLabelAddrAsData(outliningLocations::getBlockLabel(succ_bb_id));
   }
   
   // back to the regular chunk:
   em.switchToChunk(save_chunk_ndx);
   
   // Patch up the sethi and bset/add instruction that set a reg to the start of
   // the jump table data.

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

   // All done.
}
