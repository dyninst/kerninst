// regSetManager.h
// This convenience class can help you make sense out of a sparc_reg_set, by
// keeping track of a subset of registers that meet various criteria
// (e.g. which are volatile and which are robust...which are 32-bit-safe and
// which are 64-bit-safe).  It's meant to be used when emitting code: let's say that
// you are presented with a sparc_reg_set of available registers, and (say) you want to
// pick 6 32-bit robust regs and 4 64-bit volatile regs.  You can give those
// specifications to this class, and it'll tell you, for example, whether the
// criteria can be met (enough regs or not), and it'll keep track of those registers
// so you don't have to (e.g. you can at any time say, "give me the 3rd volatile
// register").
//
// If you wish to ignore the ABI's usual rule for robust vs. volatile registers,
// you can just ask for volatile registers.
//
// Possible bug: This class makes no effort to prevent usage of %sp or %fp, so make
// sure that iparent_set as passed to the constructor excludes these registers!!!
// FIX THIS!!!

#ifndef _REG_SET_MANAGER_H_
#define _REG_SET_MANAGER_H_

#include <iostream.h>
#include "sparc_reg_set.h"
#include "abi.h"
//using namespace std;

class regSetManager {
 private:
   const abi &theABI;
   
   bool save_restore_used;
   sparc_reg_set theParentSet;
      // if a save/restore is used, it WILL be applied to this set
   
   pdvector<sparc_reg> vReg64sUsed; // v stands for volatile across a fn call
   pdvector<sparc_reg> vReg32sUsed; // v stands for volatile across a fn call
   pdvector<sparc_reg> robustReg32sUsed; // robust across a fn call
      // sorry, no such thing as a robust intReg64 (at least w/ v8plus abi)

   bool initialize(const sparc_reg_set &parent_set,
                   unsigned num_volatile_reg64s_desired,
                   unsigned num_volatile_reg32s_desired,
                   unsigned num_robust_reg32s_desired,
                   const sparc_reg_set &explicitRegsNeeded);

 public:
   class NeedsASave {};
   
   regSetManager(const sparc_reg_set &iparent_set,
                 bool save_if_needed, // else throw NeedsASave
                 unsigned num_volatile_reg64s_desired,
                 unsigned num_volatile_reg32s_desired,
                 unsigned num_robust_reg32s_desired,
                 const sparc_reg_set &explicitRegsDesired,
                    // e.g. %o0 and %o1 are needed beacuse of fn calls made to strictly
                    // v8plus-abi conforming routines (w.r.t. which regs contain
                    // parameters).  Of course this can be an empty set.
                 const abi &iABI
                      // used to determine volatile vs. robust regs and 32 vs. 64 regs
                 ) throw(NeedsASave);
  ~regSetManager();

   bool needsSaveRestore() const { return save_restore_used; }
   const sparc_reg_set &getAdjustedParentSet() const {
      // Returns the "parent set", as passed to the constructor, with two possible
      // exceptions:
      // 1) explicitly required regs (as passed to ctor) will be excluded; then,
      // 2) if a save/restore was used, parent_set will be modified as such.
      return theParentSet;
   }
   sparc_reg_set getAvailRegsForACallee() const;
      // returns the parent set (as defined above, i.e. with save applied if needed)
      // minus the robust regs (but not minus the volatile regs) and minus the
      // explicitly desired regs.

   sparc_reg getNthVReg64(unsigned n) const {
      if (vReg64sUsed[n] == sparc_reg::sp ||
          vReg64sUsed[n] == sparc_reg::fp) {
         cout << "WARNING: getNthVReg64(" << n << ") returning %sp or %fp!!!" << endl;
      }
      
      return vReg64sUsed[n];
   }
   sparc_reg getNthVReg32(unsigned n) const {
      if (vReg32sUsed[n] == sparc_reg::sp ||
          vReg32sUsed[n] == sparc_reg::fp) {
         cout << "WARNING: getNthVReg32(" << n << ") returning %sp or %fp!!!" << endl;
      }
      
      return vReg32sUsed[n];
   }
   sparc_reg getNthRobustReg32(unsigned n) const {
      if (robustReg32sUsed[n] == sparc_reg::sp ||
          robustReg32sUsed[n] == sparc_reg::fp) {
         cout << "WARNING: getNthRobustReg32(" << n << ") returning %sp or %fp!!!"
              << endl;
      }
      
      return robustReg32sUsed[n];
   }
};

#endif
