// sparc_misc.C

/***** All of this Appears to be unnecessary on POWER (for now). -Igor */
#if 0
#include "sparc_misc.h"
#include "kernelDriver.h"
#include "sparc_instr.h"
#include "simplePathSlice.h"
#include "util/h/minmax.h"
#include "sparcTraits.h"
vector<hiddenFn> hiddenFnsToParse;



void readFromKernelInto(vector<uint32_t> &result,
			kptr_t startAddr, kptr_t end) {
   // end is actually 1 byte past the end addr, in the spirit of c++ iterators.
   extern kernelDriver *global_kernelDriver; // sorry about this ugliness
   assert(global_kernelDriver);
   
   const unsigned numBytes = end - startAddr;
   result.resize(numBytes / sizeof(uint32_t));
   
   unsigned numRead =  global_kernelDriver->peek_block(&result[0], startAddr, 
						       numBytes, true);
   result.resize(numRead / sizeof(uint32_t));
}

static bool update_fallthrough_bounds(sparc_instr::IntCondCodes cond, unsigned val,
                                      unsigned &min_val, unsigned &max_val) {
   // There should probably be another version of this routine with 'int'
   // arguments instead of 'unsigned' arguments.

   // Given the following information:
   //    reg (cond) val
   // update min_val and max_val as appropriate

   switch (cond) {
      case sparc_instr::iccAlways:
         // doesn't fall through, so return false 
         return false;
      case sparc_instr::iccNever:
         // falls through, but nothing to update
         break;
      case sparc_instr::iccNotEq:
         // branch away if not equal --> fall through if equal!
         min_val = max_val = val;
         break;
      case sparc_instr::iccEq:
         // branch away if equal --> fall through if not equal
         if (max_val == val) --max_val;
         if (min_val == val) ++min_val;
         break;
      case sparc_instr::iccGr:
      case sparc_instr::iccGrUnsigned:
         // branch away if greater than --> fall through if <=
         if (max_val == 0) max_val = UINT_MAX; // prepare for first update
         ipmin(max_val, val);
         break;
      case sparc_instr::iccLessOrEq:
      case sparc_instr::iccLessOrEqUnsigned:
         // branch away if <= --> fall through if greater than
         // WARNING: shouldn't the unsigned cmp have a slightly different behavior?
         if (min_val == UINT_MAX) min_val = 0; // prepare for first update
         ipmax(min_val, val+1);
         break;
      case sparc_instr::iccGrOrEq:
      case sparc_instr::iccGrOrEqUnsigned:
         // branch away if >= --> fall through if less than
         // WARNING: shouldn't the unsigned cmp have a slightly different behavior?
         if (max_val == 0) max_val = UINT_MAX; // prepare for first update
         ipmin(max_val, val-1);
         break;
      case sparc_instr::iccLess:
      case sparc_instr::iccLessUnsigned:
         // branch away if < --> fall through if >=
         // WARNING: shouldn't the unsigned cmp have a slightly different behavior?
         if (min_val == UINT_MAX) min_val = 0; // prepare for first update
         ipmax(min_val, val);
         break;
      case sparc_instr::iccPositive:
         // branch away if positive; fall through --> negative 
         // This can't happen with unsigned values, so return false
         return false;
      case sparc_instr::iccNegative:
         // branch away if negative, fall through implies positive.
         // Nothing to update
         break;
      case sparc_instr::iccOverflowClear:
         // how to handle this?
         break;
      case sparc_instr::iccOverflowSet:
         // how to handle this?
         break;
      default:
         assert(false);
   }

   return true;
}

static bool analyzeSliceForRegBoundsVia1CmpAndBranch(simplePathSlice_t &theSlice,
                                                     sparc_reg R,
                                                     unsigned &min_val,
                                                     unsigned &max_val) {
   theSlice.beginBackwardsIteration();
   if (theSlice.isEndOfIteration())
      return false;
   sparc_instr *lastInsn = theSlice.getCurrIterationInsn();

   theSlice.advanceIteration();
   if (theSlice.isEndOfIteration())
      return false;
   sparc_instr *secToLastInsn = theSlice.getCurrIterationInsn();
   
   sparc_instr::IntCondCodes cond;
   if (!lastInsn->isBicc(cond) &&
       !lastInsn->isBPcc(cond, false) && // icc
       !lastInsn->isBPcc(cond, true)) // xcc
      return false;

   int val = INT_MAX;
   if (!secToLastInsn->isCmp(R, val))
      return false;

   update_fallthrough_bounds(cond, val, min_val, max_val);
   return true;
}

bool analyzeSliceForRegisterBoundsViaBranches(simplePathSlice_t &parentSlice,
                                              sparc_reg R,
                                              unsigned &min_val,
                                              unsigned &max_val) {
   // WARNING: not fully general, due to delay slot instructions. (but slicing might
   // correctly filter them out)

   // backwards slice on "pc"
   sparc_reg_set regsToSlicePC(sparc_reg::reg_pc);
   simplePathSlice_t pcSlice1(parentSlice, 0, true, regsToSlicePC);

   if (!analyzeSliceForRegBoundsVia1CmpAndBranch(pcSlice1, R, min_val, max_val))
      return false;

   regsToSlicePC = sparc_reg::reg_pc;
   simplePathSlice_t pcSlice2(pcSlice1, 2, true, regsToSlicePC);

   if (!analyzeSliceForRegBoundsVia1CmpAndBranch(pcSlice2, R, min_val, max_val))
      return false;
   
   return true;   
}

#endif
