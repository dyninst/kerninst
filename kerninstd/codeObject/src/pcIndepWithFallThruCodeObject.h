// pcIndepWithFallThruCodeObject.h
// Like pcIndepNoFallThruCodeObject, except that we have to worry about getting to
// the fall-thru block.

#ifndef _PC_INDEP_WITH_FALL_THRU_CODE_OBJECT_H_
#define _PC_INDEP_WITH_FALL_THRU_CODE_OBJECT_H_

#include "codeObject.h"
#include "insnVec.h"

class pcIndepWithFallThruCodeObject : public codeObject {
 private:
   const insnVec_t *theCode; // trivial to relocate
   regset_t *availRegsAfterCodeObject;
      // needed in case this codeObject stumbles onto another basic block

   pcIndepWithFallThruCodeObject(const pcIndepWithFallThruCodeObject &);
      // will be defined but stays private
   pcIndepWithFallThruCodeObject &operator=(const pcIndepWithFallThruCodeObject &);
   
 protected:
   uint32_t getCodeObjectTypeID() const {
      return pcIndepWithFallThruCodeObjectID;
   }
   
 public:
   pcIndepWithFallThruCodeObject(insnVec_t *iInsns,
                                 regset_t *iavailRegsAfterCodeObject);
   pcIndepWithFallThruCodeObject(XDR *);
   ~pcIndepWithFallThruCodeObject() {
      delete theCode;
      delete availRegsAfterCodeObject;
   }

   codeObject *dup() const { return new pcIndepWithFallThruCodeObject(*this); }

   bool send(bool, XDR *) const;

   void emitWhenRelocatedTo(bool tryToKeepOriginalCode,
                               // true iff testing; we turn off optimizations
                            const function_t &parentFn,
                            const bbid_t owning_bb_id,
                            tempBufferEmitter &em,
                            const outliningLocations &whereBlocksNowReside,
                            bool thisIsLastCodeObjectInBlock,
                            const bbid_t fallThruBlockId
                               // NOT nec. the same as the block that'll get emitted
                               // next.
                            ) const;
};


#endif
