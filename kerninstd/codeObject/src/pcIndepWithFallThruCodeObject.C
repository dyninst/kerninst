// pcIndepWithFallThruCodeObject.C

#include "pcIndepWithFallThruCodeObject.h"
#include "instr.h"

pcIndepWithFallThruCodeObject::
pcIndepWithFallThruCodeObject(insnVec_t *iInsns,
                              regset_t *iavailRegsAfterCodeObject) :
   theCode(iInsns), availRegsAfterCodeObject(iavailRegsAfterCodeObject) {
}

pcIndepWithFallThruCodeObject::pcIndepWithFallThruCodeObject(XDR *xdr) {
   theCode = insnVec_t::getInsnVec(xdr);
   availRegsAfterCodeObject = regset_t::getRegSet(xdr);
}

pcIndepWithFallThruCodeObject::
pcIndepWithFallThruCodeObject(const pcIndepWithFallThruCodeObject &src) :
   codeObject(src)
{
   theCode = insnVec_t::getInsnVec((insnVec_t &) *(src.theCode));
   availRegsAfterCodeObject = regset_t::getRegSet((regset_t &)*(src.availRegsAfterCodeObject));
}

bool pcIndepWithFallThruCodeObject::send(bool, XDR *xdr) const {
   return theCode->send(xdr) && availRegsAfterCodeObject->send(xdr);
}

void pcIndepWithFallThruCodeObject::
emitWhenRelocatedTo(bool /*tryToKeepOriginalCode*/,
                       // true iff testing; we turn off optimizations
                    const function_t & /*parentFn*/,
                    bbid_t owning_bb_id,
                    tempBufferEmitter &em,
                    const outliningLocations &whereBlocksNowReside,
                    bool thisIsLastCodeObjectInBlock,
                    bbid_t fallThruBlockId
                       // not nec. the same as the block that'll be emitted
                       // after this one.
                    ) const {
   bool branchIsNeededAtEnd;
   pdstring labelStr;

   if (!thisIsLastCodeObjectInBlock) {
      branchIsNeededAtEnd = false;
   }
   else {
      const bbid_t blockEmittedNext =
         whereBlocksNowReside.whoIfAnyoneWillBeEmittedAfter(owning_bb_id);
      if (blockEmittedNext == fallThruBlockId)
         branchIsNeededAtEnd = false;
      else {
         branchIsNeededAtEnd = true;
         
         labelStr = whereBlocksNowReside.getBlockLabel(fallThruBlockId);
      }
   }
   
   // Loop thru each instruction in "theCode", emitting as we go.
   bool emittedTheBranch = false;
   unsigned numInsnsEmittedAfterTheBranch = 0;
   insnVec_t::const_iterator iter = theCode->begin();
   insnVec_t::const_iterator finish = theCode->end();
   assert(iter != finish && "need at least 1 insn in a pcIndepWithFallThruCodeObject");

   for (; iter != finish; ++iter) {
      if (iter == finish-1 && branchIsNeededAtEnd) {
	 instr_t *i = (instr_t *) new sparc_instr(sparc_instr::bpcc,
						  sparc_instr::iccAlways,
						  false, // not annulled
						  true, // predict taken
						  true, // xcc (matters?)
						  0); // displacement for now
         em.emitCondBranchToLabel(i, labelStr);
         emittedTheBranch = true;
      }
      instr_t *curinstr = instr_t::getInstr(**iter);
      em.emit(curinstr);

      if (emittedTheBranch)
         ++numInsnsEmittedAfterTheBranch;
   }

   if (branchIsNeededAtEnd) {
      assert(emittedTheBranch);
      assert(numInsnsEmittedAfterTheBranch == 1);
   }
}
