// pcIndepNoFallThruCodeObject.C
// Like pcIndepWithFallThruCodeObject.h, except that we know that the code will
// never fall thru (example: jmpl), so when it comes time to emit code, we can assume
// that there is no fall thru.

#include "pcIndepNoFallThruCodeObject.h"
#include "instr.h"

pcIndepNoFallThruCodeObject::
pcIndepNoFallThruCodeObject(insnVec_t *iInsns) :
   theCode(iInsns) {
}

pcIndepNoFallThruCodeObject::pcIndepNoFallThruCodeObject(XDR *xdr) {
   theCode = insnVec_t::getInsnVec(xdr);
}

pcIndepNoFallThruCodeObject::
pcIndepNoFallThruCodeObject(const pcIndepNoFallThruCodeObject &src) :
   codeObject(src)
{
   theCode = insnVec_t::getInsnVec((insnVec_t &)*(src.theCode));
}

bool pcIndepNoFallThruCodeObject::send(bool, XDR *xdr) const {
   return theCode->send(xdr);
}

void pcIndepNoFallThruCodeObject::
emitWhenRelocatedTo(bool /*tryToKeepOriginalCode*/,
                    const function_t &/*parentFn*/,
                    bbid_t /*owning_bb_id*/,
                    tempBufferEmitter &em,
                    const outliningLocations &/*whereBlocksNowReside*/,
                    bool thisIsLastCodeObjectInBlock,
                    bbid_t /*fallThruBlockId*/
                       // not nec. the same as the block that'll be emitted
                       // after this one.
                    ) const {
   // Loop thru each instruction in "theCode", emitting as we go.
   insnVec_t::const_iterator iter = theCode->begin();
   insnVec_t::const_iterator finish = theCode->end();
   for (; iter != finish; ++iter) {
      instr_t *curinstr = instr_t::getInstr(**iter);
      em.emit(curinstr);
   }

   // Since we're so certain (in this class at least) that the codeObject will not
   // have a fall-thru, we don't need to emitGettingToFallee().  I hope we were right.
   // Here is a cheap assertion that may help alleviate some anxiety:
   assert(thisIsLastCodeObjectInBlock);

   // If the original code was <stuff>; ret; restore
   // Then we can emit an UltraSparc "return" instruction instead, and fill the
   // delay slot with some useful work.  Unfortunately, this doesn't always work,
   // because the delay slot will be entirely executed in the post-restore register
   // window, so beware!
   // At the least, it calls for promoting returns to their own codeObjects.
}
