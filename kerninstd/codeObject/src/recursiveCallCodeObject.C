// recursiveCallCodeObject.C

#include "recursiveCallCodeObject.h"
#include "xdr_send_recv_common.h"
#include "util/h/xdr_send_recv.h"

recursiveCallCodeObject::
recursiveCallCodeObject(bbid_t ientry_bb_id,
                        bool idelaySlotInSameBlock,
                        instr_t *idsInsn,
                        regset_t *iavailRegsAfterCodeObject) :
   entry_bb_id(ientry_bb_id),
   delaySlotInSameBlock(idelaySlotInSameBlock),
   dsInsn(idsInsn),
   availRegsAfterCodeObject(iavailRegsAfterCodeObject)
{
}

recursiveCallCodeObject::
recursiveCallCodeObject(const recursiveCallCodeObject &src) :
   codeObject(src),
   entry_bb_id(src.entry_bb_id),
   delaySlotInSameBlock(src.delaySlotInSameBlock)
{
   dsInsn = instr_t::getInstr(*(src.dsInsn));
   availRegsAfterCodeObject = regset_t::getRegSet((regset_t &)*(src.availRegsAfterCodeObject));
}

template <class T> 
static void destruct1(T &t) { t.~T(); }

static instr_t* read_sparc_instr(XDR *xdr) {
   instr_t *i = (instr_t *) new sparc_instr(sparc_instr::nop);
   destruct1(*i);
   if (!P_xdr_recv(xdr, i))
      throw xdr_recv_fail();
   return i;
}
static regset_t* read_sparc_reg_set(XDR *xdr) {
   trick_regset *tr = (trick_regset*) malloc(sizeof(trick_regset));
      // allocated and uninitialized, as igen expects
   if (!P_xdr_recv(xdr, *tr))
      throw xdr_recv_fail();
   regset_t* result = const_cast<regset_t*>(tr->get());
   free(tr);
   return result;
}

recursiveCallCodeObject::recursiveCallCodeObject(XDR *xdr) :
   entry_bb_id(read1_bb_id(xdr)),
   delaySlotInSameBlock(read1_bool(xdr)),
   dsInsn(read_sparc_instr(xdr)),
   availRegsAfterCodeObject(read_sparc_reg_set(xdr))
{}

bool recursiveCallCodeObject::send(bool, XDR *xdr) const {
   return (P_xdr_send(xdr, entry_bb_id) &&
           P_xdr_send(xdr, delaySlotInSameBlock) &&
           P_xdr_send(xdr, *dsInsn) &&
           availRegsAfterCodeObject->send(xdr));
}

void recursiveCallCodeObject::
emitWhenRelocatedTo(bool /*tryToKeepOriginalCode*/,
                    const function_t &parentFn,
                    bbid_t owning_bb_id,
                    tempBufferEmitter &em,
                    const outliningLocations &whereBlocksNowReside,
                    bool thisIsLastCodeObjectInBlock,
                    bbid_t fallThruBlockId) const {
   // We know the id of the entry bb; we can assume that there's a label and
   // a symAddr.

   em.emitCall(outliningLocations::getBlockLabel(entry_bb_id));
      // Here's one of the (rare) instances of emitting a call to a label, instead of to
      // a symAddr.
      // we assume that it's within range.

   if (delaySlotInSameBlock) {   
      instr_t *dsInsn_copy = instr_t::getInstr(*dsInsn);
      em.emit(dsInsn_copy);
   }
   
   if (thisIsLastCodeObjectInBlock) {
      if (fallThruBlockId == (bbid_t)-1)
         // interproc fall-thru
         emitGettingToInterProcFallee(em, whereBlocksNowReside,
                                      false, // don't leave delay slot open
                                      *availRegsAfterCodeObject,
                                      parentFn, owning_bb_id);
      else
         emitGettingToIntraProcFallee(em,
                                      whereBlocksNowReside,
                                      availRegsAfterCodeObject,
                                      owning_bb_id,
                                      fallThruBlockId);
   }

   // Possible optimization: 
   // If this is the last code object in the block, and if fallThruBlockId isn't
   // going to be emitted next, then perhaps we should fudge %o7 so that the return
   // will jump directly to the new location of fallThruBlockId.  There are
   // caveats: at least one extra instruction, and the problem of fooling
   // the USparc-II's return address stack (RAS).  See interProcCallCodeObject for
   // similar musings.  In any event, this optimization is NOT YET IMPLEMENTED.
}
