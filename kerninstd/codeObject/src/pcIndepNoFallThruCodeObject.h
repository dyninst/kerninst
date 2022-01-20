// pcIndepNoFallThruCodeObject.h
// Like pcIndepWithFallThruCodeObject.h, except that we know that the code will
// never fall thru (example: jmpl), so when it comes time to emit code, we can assume
// that there is no fall thru.

#ifndef _PC_INDEP_NO_FALL_THRU_CODE_OBJECT_H_
#define _PC_INDEP_NO_FALL_THRU_CODE_OBJECT_H_

#include "codeObject.h"
#include "insnVec.h"

class pcIndepNoFallThruCodeObject : public codeObject {
 private:
   const insnVec_t *theCode; // trivial to relocate

   pcIndepNoFallThruCodeObject(const pcIndepNoFallThruCodeObject &);
      // will be defined but stays private
   pcIndepNoFallThruCodeObject &operator=(const pcIndepNoFallThruCodeObject &);
   
 protected:
   uint32_t getCodeObjectTypeID() const {
      return pcIndepNoFallThruCodeObjectID;
   }
   
 public:
   pcIndepNoFallThruCodeObject(insnVec_t *iInsns);
   pcIndepNoFallThruCodeObject(XDR *);

   ~pcIndepNoFallThruCodeObject() {
     delete theCode;
   }

   codeObject *dup() const { return new pcIndepNoFallThruCodeObject(*this); }

   bool send(bool, XDR *) const;

   void emitWhenRelocatedTo(bool tryToKeepOriginalCode,
                               // true iff testing
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
