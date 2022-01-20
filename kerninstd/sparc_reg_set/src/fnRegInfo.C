// fnRegInfo.C

#include "fnRegInfo.h"

fnRegInfo::RegsAvailAtCallTo fnRegInfo::regsAvailAtCallTo;
//fnRegInfo::RegsAvailWithinFn fnRegInfo::regsAvailWithinFn;
pdvector< std::pair<pdstring, sparc_reg> > fnRegInfo::blankParamInfo;

fnRegInfo::fnRegInfo(bool inlined,
                     RegsAvailAtCallTo,
                     const sparc_reg_set &iavailRegsAtCall,
                     const pdvector< std::pair<pdstring, sparc_reg> > &iparamInfo,
                     sparc_reg iresultReg) :
      availRegsForFn(iavailRegsAtCall),
      paramInfo(stringHash),
      resultReg(iresultReg) {
   // remove params, o7, fp, and sp from availRegsForFn:
   if (!inlined)
      availRegsForFn -= sparc_reg::o7;
   availRegsForFn -= sparc_reg::fp;
   availRegsForFn -= sparc_reg::sp;
   availRegsForFn -= sparc_reg::g7;
   pdvector< std::pair<pdstring, sparc_reg> >::const_iterator iter = iparamInfo.begin();
   pdvector< std::pair<pdstring, sparc_reg> >::const_iterator finish = iparamInfo.end();
   for (; iter != finish; ++iter) {
      availRegsForFn -= iter->second;
   }
   
   iter = iparamInfo.begin();
   for (; iter != finish; ++iter) {
      const sparc_reg theReg = iter->second;
      assert(!availRegsForFn.exists(theReg));
         
      paramInfo.set(iter->first, theReg);
   }

   assert(paramInfo.size() == iparamInfo.size());
}

//  fnRegInfo::fnRegInfo(bool inlined,
//                       RegsAvailWithinFn,
//                       const sparc_reg_set &iavailRegsForFn,
//                       const pdvector< std::pair<pdstring, sparc_reg> > &iparamInfo,
//                       const sparc_reg iresultReg) :
//        availRegsForFn(iavailRegsForFn),
//        paramInfo(stringHash),
//        resultReg(iresultReg) {
//     if (!inlined)
//        assert(!availRegsForFn.exists(sparc_reg::o7));
//     assert(!availRegsForFn.exists(sparc_reg::fp)); // for safety
//     assert(!availRegsForFn.exists(sparc_reg::sp)); // for safety
//     assert(!availRegsForFn.exists(sparc_reg::g7)); // for safety
      
//     pdvector< std::pair<pdstring, sparc_reg> >::const_iterator iter = iparamInfo.begin();
//     pdvector< std::pair<pdstring, sparc_reg> >::const_iterator finish = iparamInfo.end();
//     for (; iter != finish; ++iter) {
//        const sparc_reg theReg = iter->second;
//        assert(!availRegsForFn.exists(theReg));
         
//        paramInfo.set(iter->first, theReg);
//     }

//     assert(paramInfo.size() == iparamInfo.size());
//  }

