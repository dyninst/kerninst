// fnRegInfo.h
// A cute convenience class, useful when emitting code (see emitters subdirectory)
// that will make function calls.
// Keeps track of available registers, and (here's the cute part) maintains a
// dictionary of parameter name (like "key_reg") to the actual register.

#ifndef _FN_REG_INFO_H_
#define _FN_REG_INFO_H_

#include "sparc_reg_set.h"
#include "util/h/Dictionary.h"
#include "util/h/hashFns.h"

class fnRegInfo {
 private:
   sparc_reg_set availRegsForFn;
      // will exclude the arguments (below), %o7 (after all, we were called, right?),
      // and (for safety as usual) %sp/%fp/%g7
   
   dictionary_hash<pdstring, sparc_reg> paramInfo;
   sparc_reg resultReg; // sparc_reg::g0 indicates no return value

   // private to ensure it's not used:
   fnRegInfo &operator=(const fnRegInfo &src);

 public:
   class RegsAvailAtCallTo {};
   static RegsAvailAtCallTo regsAvailAtCallTo;
//     class RegsAvailWithinFn {};
//     static RegsAvailWithinFn regsAvailWithinFn;

   static pdvector< std::pair<pdstring, sparc_reg> > blankParamInfo;
   
   fnRegInfo(bool inlined, // if true, we don't restrict use of %o7
             RegsAvailAtCallTo,
             const sparc_reg_set &, // we'll subtract params, o7, fp, and sp
             const pdvector< std::pair<pdstring, sparc_reg> > &iparamInfo,
             sparc_reg iresultReg);

//     fnRegInfo(bool inlined, // if true, we don't restrict use of %o7
//               RegsAvailWithinFn,
//               const sparc_reg_set &, // we'll assert no params, o7, fp, or sp in this set
//               const pdvector< pair<pdstring, sparc_reg> > &iparamInfo,
//               sparc_reg iresultReg);
             
   fnRegInfo(const fnRegInfo &src) : availRegsForFn(src.availRegsForFn),
      paramInfo(src.paramInfo), resultReg(src.resultReg) {
   }
  ~fnRegInfo() { }

   const sparc_reg_set &getAvailRegsForFn() const {
      // will exclude the arguments, o7 [unless inlined] and the usual
      // safety ones (fp, sp, %g7).
      // Won't necessarily exclude resultReg though.
      assert(!availRegsForFn.exists(sparc_reg::fp));
      assert(!availRegsForFn.exists(sparc_reg::sp));
      assert(!availRegsForFn.exists(sparc_reg::g7));
      
      return availRegsForFn;
   }
   
   sparc_reg getParamRegByName(const pdstring &paramName) const {
      return paramInfo.get(paramName);
   }
   sparc_reg getResultReg() const {
      // %g0 --> no result reg
      return resultReg;
   }
};


#endif
