#ifndef _BITWISE_DATAFLOW_FN_H_
#define _BITWISE_DATAFLOW_FN_H_

// A class to manage a bitwise data-flow-function for registers

// See section 4.1.2, "bitwise functions on bit vector" in 1992 pre-print of
// Optimization in Compilers, ed. Fran Allen (we used it in cs701) -- any
// monotone bitwise fn can be represented by two bit vectors THRU and GEN;
// applying this function can be done easily: f(x) = (x ^ THRU(f)) v GEN(f);
// additionally, composing two such functions is easy, with fog represented
// by THRU=THRU1 ^ THRU2 and GEN=(THRU1 ^ GEN2) v GEN1.
// But merging is more complex (though we handle it).
// 
// (This compares favorably [speedwise] with the more traditional
// f(x) = (x - KILL(f)) U GEN(f) that you'll probably see in the Dragon book)
// Note that our THRU is simply the negation of KILL.

// To validate a proposition for a register, call the start() method.
// To invalidate a proposition for a register, call the stop() method.
// To pass a proposition for a register, call the pass() method.
// There is no negate() method, because then the function wouldn't be monotone.
// In each of the above methods, we keep thru and gen up-to-date for you; you
// only need to call start(), stop() and pass() as desired.

#include "instr.h"
#include "reg.h"
#include "reg_set.h"

#include "util/h/refCounterK.h"
#include "common/h/Vector.h"

#include <utility> // STL pair

struct XDR;

class monotone_bitwise_dataflow_fn_ll {
 public:
   class CannotApply {};
   class DisagreeingNumberOfPreSaveFramesInMerge {};
   class Dummy {};
   
 public:
   monotone_bitwise_dataflow_fn_ll();

   virtual bool send(XDR *) const = 0;

   virtual ~monotone_bitwise_dataflow_fn_ll();
   
   virtual unsigned getMemUsage_exceptObjItself() const = 0;
   virtual void setEachLevelTo(const regset_t *new_thru,
			       const regset_t *new_gen) = 0;
   virtual void swapStartsAndStops() = 0;
   
   virtual regset_t* apply(const regset_t &x) const = 0;
      // calculates f(x), where f, the monotone bitwise data flow function, is 'this'.

   virtual monotone_bitwise_dataflow_fn_ll*
   compose(const monotone_bitwise_dataflow_fn_ll &g) const = 0;
      // calculates f o g, the composition of two monotone bitwise data flow functions.
      // f, the first function, is 'this'.  g, the second function, is the parameter.
      // We return a new function which is f o g, so if you later apply() it to some
      // bit vector x, you'll get f(g(x)).  To obtain this, we first evaluate g, then
      // evaluate f; that's the result we return.  See Allen, ed. section 4.1.2.

   virtual void compose_overwrite_param(monotone_bitwise_dataflow_fn_ll &g) const = 0;
      // calculate f o g, where f is 'this' and g is the parameter.
      // Writes the result onto g instead of returning it (that's the difference
      // between this routine and the one above it).

   virtual void mergeWith(const monotone_bitwise_dataflow_fn_ll &src, bool mergeWithBitOr) = 0;
      // calculates 'this' merge 'src' and writes its result onto 'this'
      // You supply the merge operator, which depends on the dataflow problem you are
      // solving; presumably it is bitwise-and or bitwise-or.
      // Note that there's no need for an alternate version that writes its result
      // onto 'src', because merging is commutative.

   virtual monotone_bitwise_dataflow_fn_ll* 
   merge(const monotone_bitwise_dataflow_fn_ll &src,
	 bool mergeWithBitOr) const = 0;
      // return (this merge src)
      // You supply the merge operator, which depends on the dataflow problem you are
      // solving; presumably it is bitwise-and or bitwise-or.


   // pass: pass along a proposition
   virtual void pass(reg_t &) = 0;
   virtual void passSet(const regset_t &) = 0;

   // start: validate a proposition
   virtual void start(reg_t &) = 0;
   virtual void startSet(const regset_t &) = 0;

   // start: invalidate a proposition
   virtual void stop(reg_t &) = 0;
   virtual void stopSet(const regset_t &) = 0;

   virtual void processSaveInBackwardsDFProblem() {}
   virtual void processRestoreInBackwardsDFProblem() {}

   virtual unsigned getNumSavedWindows() const = 0;
   virtual unsigned getNumLevels() const = 0;
   virtual unsigned getNumPreSaveFramesManufactured() const = 0;

   virtual pdstring sprintf(const pdstring &starts_banner,
                            const pdstring &stops_banner,
                            bool starts_banner_first) const = 0;
};

class monotone_bitwise_dataflow_fn {
 public:
   enum functions {fnpass, fnstart, fnstop};
   enum mergeoperators {mergeopBitwiseAnd, mergeopBitwiseOr};

   monotone_bitwise_dataflow_fn() {}

   virtual bool send(XDR *) const = 0;

   virtual ~monotone_bitwise_dataflow_fn() {} // nothing to destruct

   class StartAllRegs {};
   static StartAllRegs startall;
   class StopAllRegs {};
   static StopAllRegs stopall;
   class PassAllRegs {};
   static PassAllRegs passall;
   
   static monotone_bitwise_dataflow_fn* getDataflowFn(StartAllRegs);
   static monotone_bitwise_dataflow_fn* getDataflowFn(StopAllRegs);
   static monotone_bitwise_dataflow_fn* getDataflowFn(PassAllRegs);

   static monotone_bitwise_dataflow_fn* getDataflowFn(XDR *);
   static monotone_bitwise_dataflow_fn* 
      getDataflowFn (const monotone_bitwise_dataflow_fn &src);
   //essentially a copy constructor

   virtual bool isDummy() const = 0;

   virtual unsigned getMemUsage_exceptObjItself() const = 0;

   virtual regset_t* apply(const regset_t &x) const = 0;

   virtual monotone_bitwise_dataflow_fn* 
   compose(const monotone_bitwise_dataflow_fn &g) const = 0;

   virtual void compose_overwrite_param(monotone_bitwise_dataflow_fn &g) const = 0;
   
   virtual void mergeWith(const monotone_bitwise_dataflow_fn &g, 
			  mergeoperators mergeop) = 0;
   
   virtual monotone_bitwise_dataflow_fn* 
   merge(const monotone_bitwise_dataflow_fn &g,
	 mergeoperators mergeop) const = 0;
   
   monotone_bitwise_dataflow_fn& operator=(const monotone_bitwise_dataflow_fn &src);
   virtual bool operator==(const monotone_bitwise_dataflow_fn &cmp) const = 0;
   virtual bool operator!=(const monotone_bitwise_dataflow_fn &cmp) const = 0;

   virtual unsigned getNumSavedWindows() const = 0;
   virtual unsigned getNumLevels() const = 0;
   virtual unsigned getNumPreSaveFramesManufactured() const = 0;
   virtual functions getFnForRegSavedLevel(reg_t &r, unsigned howFarBack) const = 0;

   virtual std::pair<regset_t *, regset_t *> 
   getStartsAndStopsForLevel(unsigned level) const = 0;

   virtual pdstring sprintf(const pdstring &starts_banner,
                            const pdstring &stops_banner,
                            bool starts_banner_first) const = 0;

   // non-const methods, as most of the below routines are, become clumsy with
   // reference counters.
   
   // pass: pass along a proposition
   virtual void pass(reg_t &r) = 0;

   virtual monotone_bitwise_dataflow_fn& pass2(reg_t &r) = 0;
      // like above pass, but returns a reference to "this", in the style of
      // cout "operator<<", most "operator++()" variants, and so on.
      // If I thought that there was no overhead in this, then I'd make this one the
      // default.

   virtual void passSet(const regset_t &set) = 0;

   virtual monotone_bitwise_dataflow_fn& passSet2(const regset_t &set) = 0;

   // start: validate a proposition
   virtual void start(reg_t &r) = 0;

   virtual monotone_bitwise_dataflow_fn &start2(reg_t &r) = 0;

   virtual void startSet(const regset_t &set) = 0;

   virtual monotone_bitwise_dataflow_fn &startSet2(const regset_t &set) = 0;

   // start: invalidate a proposition
   virtual void stop(reg_t &r) = 0;

   virtual monotone_bitwise_dataflow_fn &stop2(reg_t &r) = 0;

   virtual void stopSet(const regset_t &set) = 0;

   virtual monotone_bitwise_dataflow_fn &stopSet2(const regset_t &set) = 0;

   virtual void swapStartsAndStops() = 0;

   virtual void processSaveInBackwardsDFProblem() {}
   virtual void processRestoreInBackwardsDFProblem() {}

   virtual void setEachLevelTo(const regset_t *new_thru,
			       const regset_t *new_gen) = 0;
};

typedef monotone_bitwise_dataflow_fn bitwise_dataflow_fn_t;

#endif /* BITWISE_DATAFLOW_FN_H */
