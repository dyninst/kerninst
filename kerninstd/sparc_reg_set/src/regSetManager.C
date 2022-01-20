// regSetManager.C

#include "regSetManager.h"


// This routine allocates the desired number of registers of three types:
// robust 32-bit, volatile 32-bit and volatile 64-bit
// The same code works for both sparcv8 and sparcv9 ABIs
bool regSetManager::
initialize(const sparc_reg_set &iparent_set,
           unsigned num_volatile_reg64s_desired,
           unsigned num_volatile_reg32s_desired,
           unsigned num_robust_reg32s_desired,
           const sparc_reg_set &explicitRegsNeeded
              // e.g. %o0 and %o1 are needed beacuse of fn calls made to strictly
              // v8plus-abi conforming routines (w.r.t. which regs contain
              // parameters).  Or, e.g., %xcc/%icc.
              // Of course this can be an empty set.
           ) {
   // returns true iff successful (no save needed), else returns false

   // First, check explicitRegsDesired
   if (!iparent_set.exists(explicitRegsNeeded))
      return false;

   assert(!iparent_set.exists(sparc_reg::g0));
   
   // OK, the explicit regs are there...make sure to remove them from
   // "parent set" before working any further.  And also make sure that %g0 isn't
   // ever considered "available"
   theParentSet = iparent_set - explicitRegsNeeded - sparc_reg::g0;
   regset_t * robustInts = theABI.robustints_of_regset(&theParentSet); 
   const sparc_reg_set availRobustReg32s = *(sparc_reg_set*)robustInts;
   assert(!availRobustReg32s.exists(sparc_reg::g0));
   regset_t *volatileInts =  theABI.volatileints_of_regset(&theParentSet);
   const pair<regset_t*, regset_t*> tmp_pair =
      theABI.regset_classify_int32s_and_int64s(volatileInts);
   const pair<sparc_reg_set, sparc_reg_set> availVRegs(*(sparc_reg_set*)tmp_pair.first, *(sparc_reg_set*)tmp_pair.second);
      // .first: int32 volatile regs
      // .second: int64 volatile regs
   assert(availRobustReg32s.count() + availVRegs.first.count() + availVRegs.second.count() == theParentSet.countIntRegs());
   delete robustInts;
   delete volatileInts;
   delete tmp_pair.first;
   delete tmp_pair.second;
   ////////
   // Check if we have enough registers to satisfy the requirements
   ////////
   const unsigned num_robust_reg32s_available = 
       availRobustReg32s.countIntRegs();
   unsigned num_volatile_reg64s_available = 
       availVRegs.second.countIntRegs();
   unsigned num_volatile_reg32s_available = 
       availVRegs.first.countIntRegs();

   if (num_robust_reg32s_desired > num_robust_reg32s_available) {
       return false;
   }

   if (theABI.uint64FitsInSingleRegister()) {
       // sparcv9: use the excess of robustReg32s as volatileReg64s
       num_volatile_reg64s_available +=
	   (num_robust_reg32s_available - num_robust_reg32s_desired);
   }
   else {
       // sparcv8: use the excess of robustReg32s as volatileReg32s
       num_volatile_reg32s_available +=
	   (num_robust_reg32s_available - num_robust_reg32s_desired);
   }

   if (num_volatile_reg64s_desired > num_volatile_reg64s_available) {
       return false;
   }

   // use the excess of volatileReg64s as volatileReg32s
   num_volatile_reg32s_available +=
       (num_volatile_reg64s_available - num_volatile_reg64s_desired);
   if (num_volatile_reg32s_desired > num_volatile_reg32s_available) {
       return false;
   }

   vReg64sUsed.reserve_exact(num_volatile_reg64s_desired);
   vReg32sUsed.reserve_exact(num_volatile_reg32s_desired);
   robustReg32sUsed.reserve_exact(num_robust_reg32s_desired);

   // 1) Pick the robust reg32's:
   sparc_reg_set::const_iterator availRobustReg32Iter = 
       availRobustReg32s.begin();
   sparc_reg_set::const_iterator availRobustReg32Finish = 
       availRobustReg32s.end();
   while (availRobustReg32Iter != availRobustReg32Finish && 
	  num_robust_reg32s_desired) {
      robustReg32sUsed += (sparc_reg&)*availRobustReg32Iter++;
      --num_robust_reg32s_desired;
   }

   // 2) Pick some vreg's off of the remaining available robust reg32's.
   if (theABI.uint64FitsInSingleRegister()) {
       // sparcv9: robust reg32's can be used as vreg64's
       while (availRobustReg32Iter != availRobustReg32Finish &&
	      num_volatile_reg64s_desired) {
	   vReg64sUsed += (sparc_reg&)*availRobustReg32Iter++;
	   --num_volatile_reg64s_desired;
       }
   }
   // 2.5) Pick some vreg32's off of the remaining available robust reg32's.
   while (availRobustReg32Iter != availRobustReg32Finish &&
	  num_volatile_reg32s_desired) {
       vReg32sUsed += (sparc_reg&)*availRobustReg32Iter++;
       --num_volatile_reg32s_desired;
   }

   // 3) Pick the vreg64's:
   sparc_reg_set::const_iterator availVReg64Iter = availVRegs.second.begin();
   sparc_reg_set::const_iterator availVReg64Finish = availVRegs.second.end();
   while (availVReg64Iter != availVReg64Finish && num_volatile_reg64s_desired) {
      vReg64sUsed += (sparc_reg&)*availVReg64Iter++;
      --num_volatile_reg64s_desired;
   }

   // 4) Pick some vreg32's off of the remaining vreg64's
   while (availVReg64Iter != availVReg64Finish && num_volatile_reg32s_desired) {
      vReg32sUsed += (sparc_reg&)*availVReg64Iter++;
      --num_volatile_reg32s_desired;
   }

   // 5) Pick remaining needed vreg32's off of vreg32's "proper":
   sparc_reg_set::const_iterator availVReg32Iter = availVRegs.first.begin();
   sparc_reg_set::const_iterator availVReg32Finish = availVRegs.first.begin();
   while (availVReg32Iter != availVReg32Finish && num_volatile_reg32s_desired) {
      vReg32sUsed += (sparc_reg&)*availVReg32Iter++;
      --num_volatile_reg32s_desired;
   }

   // Assert that we got them all:
   assert(num_robust_reg32s_desired == 0);
   assert(num_volatile_reg64s_desired == 0);
   assert(num_volatile_reg32s_desired == 0);

   // Assertion check: make sure that the same register isn't being assigned to two
   // different tasks.
   const unsigned total_num_regs = vReg64sUsed.size() + vReg32sUsed.size() + robustReg32sUsed.size() + explicitRegsNeeded.count();
   sparc_reg_set all_the_regs = sparc_reg_set::emptyset;

   for (pdvector<sparc_reg>::const_iterator iter = vReg64sUsed.begin();
        iter != vReg64sUsed.end(); ++iter) {
      assert(*iter != sparc_reg::g0);
      assert(theABI.isReg64Safe(*iter));
      all_the_regs += *iter;
   }

   for (pdvector<sparc_reg>::const_iterator iter = vReg32sUsed.begin();
        iter != vReg32sUsed.end(); ++iter) {
      assert(*iter != sparc_reg::g0);
      all_the_regs += *iter;
   }

   for (pdvector<sparc_reg>::const_iterator iter = robustReg32sUsed.begin();
        iter != robustReg32sUsed.end(); ++iter) {
      assert(*iter != sparc_reg::g0);
      all_the_regs += *iter;
   }

   all_the_regs += explicitRegsNeeded;
   assert(all_the_regs.count() == total_num_regs);

   return true; // success
}

regSetManager::regSetManager(const sparc_reg_set &parent_set,
                             bool save_if_needed, // else throw NeedsASave
                             unsigned num_volatile_reg64s_desired,
                             unsigned num_volatile_reg32s_desired,
                             unsigned num_robust_reg32s_desired,
                             const sparc_reg_set &explicitRegsNeeded,
                             const abi &iABI
                                // used to determine volatile vs. robust regs and
                                // 32 vs. 64 regs
                             ) throw(regSetManager::NeedsASave) :
      theABI (iABI),
      theParentSet(sparc_reg_set::emptyset) {
   save_restore_used = false;

   // We don't ever want to hear that %g0 is available for use:
   assert(!parent_set.exists(sparc_reg::g0));
   
   if (!initialize(parent_set,
                   num_volatile_reg64s_desired,
                   num_volatile_reg32s_desired,
                   num_robust_reg32s_desired,
                   explicitRegsNeeded)) {
      // failed, so a save is needed
      if (!save_if_needed)
         throw NeedsASave();

      save_restore_used = true;

      if (!initialize(sparc_reg_set(sparc_reg_set::save, parent_set, true),
                      num_volatile_reg64s_desired,
                      num_volatile_reg32s_desired,
                      num_robust_reg32s_desired,
                      explicitRegsNeeded))
         assert(false &&
                "regSetManager: a save was insufficient to obtain desired regs");
      else {
         // succeeded on the second try
      }
   }
   else {
      // succeeded on the first try
   }
}

regSetManager::~regSetManager() {
}

sparc_reg_set regSetManager::getAvailRegsForACallee() const {
   // returns the parent set (with save applied if needed)
   // minus the robust regs (but not minus the volatile regs).  The idea being
   // that volatile regs are free to be used by the callee (after all, the
   // caller is assuming they're 'volatile'), but not the 'robust' regs
   // (which, after all, the caller is assuming will be 'robust', i.e. non-volatile
   // across a fn call).

   sparc_reg_set result = theParentSet;
   for (pdvector<sparc_reg>::const_iterator iter = robustReg32sUsed.begin();
        iter != robustReg32sUsed.end(); ++iter) {
      const sparc_reg theRobustReg32 = *iter;
      assert(result.exists(theRobustReg32));
      result -= theRobustReg32;
   }

   // no need to subtract the explicitly desired regs; they were never stored
   // in parent_set to begin with.

   // the result should be a subset of the parent set
   assert(theParentSet.exists(result));

   // We don't ever want to return anything that says %g0 is available:
   assert(!result.exists(sparc_reg::g0));
   
   return result;
}
