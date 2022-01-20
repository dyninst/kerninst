// sparc_bitwise_dataflow_fn.h
// Ariel Tamches
// A class to manage a bitwise data-flow-function for sparc registers

// TODO: only put *integer* registers through the preSaveFrames[] stack.
// To do this cleanly, first make some changes to class sparc_reg_set: separate
// the int, float, and misc reg sets into separate sub-classes.

#ifndef _SPARC_BITWISE_DATAFLOW_FN_H_
#define _SPARC_BITWISE_DATAFLOW_FN_H_

#include "bitwise_dataflow_fn.h"
#include "sparc_mbdf_window.h"
#include "sparc_reg.h"
#include "sparc_reg_set.h"

// We keep a stack of pre-save (also post-restore) register windows, to handle
// processing of save and restore instructions.  The top of the stack is the
// current register window, so the stack always contains at least one element.
// If we pop an empty stack, we create an all-PASS frame on the stack and
// increment numPreSaveFramesManufactured; it's necessary to keep track of this
// variable when merging and composing.

class sparc_mbdf_window_alloc {
 public:
   static sparc_mbdf_window *alloc(unsigned nelems);
   static void free(sparc_mbdf_window *vec, unsigned nelems);

   static unsigned getMemUsage_1heap();
   static unsigned getMemUsage_2heap();
};

class sparc_bitwise_dataflow_fn_ll : public monotone_bitwise_dataflow_fn_ll {
 private:
   pdvector<sparc_mbdf_window, sparc_mbdf_window_alloc> preSaveFrames; // also, post-restore windows
      // actually, the top of the stack isn't pre-save; it's the current
      // window.  ONLY the top window has %i and %l regs defined.  (What about %g
      // and %f regs?) Remember this!!!
   unsigned numPreSaveFramesManufactured;

   sparc_mbdf_window &top();
   const sparc_mbdf_window &top() const;

   void boost(); // insert an all-PASS window at the bottom of the stack; only
      // allowed if numPreSaveFramesManufactured==0

 public:
   sparc_bitwise_dataflow_fn_ll(XDR *);
   bool send(XDR *) const;
   
   sparc_bitwise_dataflow_fn_ll(Dummy) {
      numPreSaveFramesManufactured = -1U;
   }

   unsigned getMemUsage_exceptObjItself() const {
      return sizeof(mbdf_window) * preSaveFrames.size();
      // each mbdf_window is two sparc_reg_set's.
   }

   sparc_bitwise_dataflow_fn_ll(bool iPassBit, bool iGenBit);
      // you specify an initial value, which is used for all registers in the set.
      // No register in the set is ever left undefined.

   sparc_bitwise_dataflow_fn_ll(const sparc_bitwise_dataflow_fn_ll &src);
   sparc_bitwise_dataflow_fn_ll& operator=(const sparc_bitwise_dataflow_fn_ll &src);

   bool operator==(const sparc_bitwise_dataflow_fn_ll &src) const;
   bool operator!=(const sparc_bitwise_dataflow_fn_ll &src) const;

   void setEachLevelTo(const regset_t *new_thru,
                       const regset_t *new_gen);
   void swapStartsAndStops();
   
   regset_t* apply(const regset_t &x) const;
      // calculates f(x), where f, the monotone bitwise data flow function, is 'this'.

   monotone_bitwise_dataflow_fn_ll*
   compose(const monotone_bitwise_dataflow_fn_ll &g) const;
      // calculates f o g, the composition of two monotone bitwise data flow functions.
      // f, the first function, is 'this'.  g, the second function, is the parameter.
      // We return a new function which is f o g, so if you later apply() it to some
      // bit vector x, you'll get f(g(x)).  To obtain this, we first evaluate g, then
      // evaluate f; that's the result we return.  See Allen, ed. section 4.1.2.

   void compose_overwrite_param(monotone_bitwise_dataflow_fn_ll &g) const;
      // calculate f o g, where f is 'this' and g is the parameter.
      // Writes the result onto g instead of returning it (that's the difference
      // between this routine and the one above it).

   void mergeWith(const monotone_bitwise_dataflow_fn_ll &src, bool mergeWithBitOr);
      // calculates 'this' merge 'src' and writes its result onto 'this'
      // You supply the merge operator, which depends on the dataflow problem you are
      // solving; presumably it is bitwise-and or bitwise-or.
      // Note that there's no need for an alternate version that writes its result
      // onto 'src', because merging is commutative.

   monotone_bitwise_dataflow_fn_ll*
   merge(const monotone_bitwise_dataflow_fn_ll &src,
	 bool mergeWithBitOr) const {
      // return (this merge src)
      // You supply the merge operator, which depends on the dataflow problem you are
      // solving; presumably it is bitwise-and or bitwise-or.

      sparc_bitwise_dataflow_fn_ll *result = new sparc_bitwise_dataflow_fn_ll(*this);
      result->mergeWith(src, mergeWithBitOr);
      return result;
   }

   // pass: pass along a proposition
   void pass(reg_t &);
   void passSet(const regset_t &);

   // start: validate a proposition
   void start(reg_t &);
   void startSet(const regset_t &);

   // start: invalidate a proposition
   void stop(reg_t &);
   void stopSet(const regset_t &);

   void processSaveInBackwardsDFProblem();
      // The current working window (stack top) gets popped.  Before it gets thrown
      // away, old top xfers its %g's to the new top's %g's, and its %i's to the new
      // top's %o's.  (What about floats???  Misc regs???)
      // What if a previous stack window doesn't exist?  We create a new frame with
      // all-PASS in %i's and %l's (yes, this is safe; no need for all-START
      // assumptions).  Additionally, we increment a vrble
      // numPreSaveFramesManufactured (when it's nonzero, the stack itself contains
      // just the top) to make a note of the fact that the stack top is really what's
      // obtained after a save is processed.
   
   void processRestoreInBackwardsDFProblem();
      // push a new window onto the window stack, which thus becomes the new top().
      // characteristics of this new window: g's get g's of old top, i's get
      // o's of old top, l's and o's get STOP (yea!).  What about float???  Misc regs??
      // But if numPreSaveFramesManufactured was nonzero, then we decrement that
      // count, don't push or pop anything on the stack, and then adjust the stack top
      // (which should be the only elem on the stack) in the familiar way:
      // no change to %g's; %i's get %o's.

   void getFnForRegSavedLevel(reg_t &, unsigned howFarBack,
                              bool &iThruBit, bool &iGenBit) const;
      // howFarBack: 0 --> current window (just like the prev fn)
      //             1 --> 1 level back (1 pop)
      //             etc.

   std::pair<regset_t *, regset_t *> getStartsAndStopsForLevel(unsigned level) const {
      return preSaveFrames[level].getStartsAndStops();
   }
   const regset_t *getThruForLevel(unsigned level) const {
      return preSaveFrames[level].getThru();
   }
   const regset_t *getGenForLevel(unsigned level) const {
      return preSaveFrames[level].getGen();
   }

   unsigned getNumSavedWindows() const {
      assert(preSaveFrames.size() > 0);
      return preSaveFrames.size()-1;
   }
   unsigned getNumLevels() const {
      return preSaveFrames.size();
   }
   unsigned getNumPreSaveFramesManufactured() const {
      return numPreSaveFramesManufactured;
   }

   pdstring sprintf(const pdstring &starts_banner,
                    const pdstring &stops_banner,
                    bool starts_banner_first) const;
};

class sparc_bitwise_dataflow_fn : public monotone_bitwise_dataflow_fn {
 public:
   static sparc_bitwise_dataflow_fn startAllRegs;
   static sparc_bitwise_dataflow_fn stopAllRegs;
   static sparc_bitwise_dataflow_fn passAllRegs;

 private:
   refCounterK<sparc_bitwise_dataflow_fn_ll> data;

   // slow ctors:
   sparc_bitwise_dataflow_fn(const sparc_bitwise_dataflow_fn_ll &src) : 
      data(src) {}
   sparc_bitwise_dataflow_fn(bool iThruBit, bool iGenBit) :
      data(sparc_bitwise_dataflow_fn_ll(iThruBit, iGenBit)) {}

 public:
   sparc_bitwise_dataflow_fn(XDR *xdr);
   bool send(XDR *xdr) const;
   
   class Dummy {};
   static sparc_bitwise_dataflow_fn dummy_fn;
   sparc_bitwise_dataflow_fn(bool, bool, bool, bool);
      
   // fast ctors:
   sparc_bitwise_dataflow_fn(const sparc_bitwise_dataflow_fn &src) :
      monotone_bitwise_dataflow_fn(), data(src.data) {}
   sparc_bitwise_dataflow_fn(functions iFn) :
      monotone_bitwise_dataflow_fn(), 
      data(iFn == fnpass ? passAllRegs.data :
           iFn == fnstart ? startAllRegs.data : stopAllRegs.data) 
   {
      assert(iFn == fnpass || iFn == fnstart || iFn == fnstop);
   }
  ~sparc_bitwise_dataflow_fn() {} // dtor for refCounter class does the work.
   

   // fast assignment operator:
   sparc_bitwise_dataflow_fn& operator=(const sparc_bitwise_dataflow_fn &src) {
      if (&src == this) return *this;
      data = src.data;
      return *this;
   }

   bool isDummy() const {
      return (*this == dummy_fn); 
   }

   unsigned getMemUsage_exceptObjItself() const {
      return sizeof(sparc_bitwise_dataflow_fn_ll) + data.getData().getMemUsage_exceptObjItself();
   }
   
   // no faster, no slower: const methods
   bool operator==(const sparc_bitwise_dataflow_fn &cmp) const {
      if (&cmp == this) return true;
      return data.getData() == cmp.data.getData();
   }
   bool operator==(const monotone_bitwise_dataflow_fn &cmp) const {
      return operator==((const sparc_bitwise_dataflow_fn&)cmp);
   }

   bool operator!=(const sparc_bitwise_dataflow_fn &cmp) const {
      return !operator==(cmp);
   }
   bool operator!=(const monotone_bitwise_dataflow_fn &cmp) const {
      return !operator==((const sparc_bitwise_dataflow_fn &)cmp);
   }

   regset_t* apply(const regset_t &x) const {
      // can throw a CannotApply() exception from the _ll class
      return data.getData().apply(x);
   }
   monotone_bitwise_dataflow_fn* compose(const monotone_bitwise_dataflow_fn &g) const {
      const sparc_bitwise_dataflow_fn &g2 = (const sparc_bitwise_dataflow_fn &)g;
      sparc_bitwise_dataflow_fn_ll *bdf_ll = (sparc_bitwise_dataflow_fn_ll *)data.getData().compose(g2.data.getData());
      sparc_bitwise_dataflow_fn *result = new sparc_bitwise_dataflow_fn(*bdf_ll);
      delete bdf_ll;
      return result;

   }
   void compose_overwrite_param(monotone_bitwise_dataflow_fn &g) const {
      // calculates f o g (f is "this"; g is the parameter), and writes the result
      // onto g.
      sparc_bitwise_dataflow_fn &g2 = (sparc_bitwise_dataflow_fn &)g;
      g2.data.prepareToModifyInPlace();
      data.getData().compose_overwrite_param(g2.data.getModifiableData());
   }
   
   void mergeWith(const monotone_bitwise_dataflow_fn &g, mergeoperators mergeop)
   {
      const sparc_bitwise_dataflow_fn &g2 = (const sparc_bitwise_dataflow_fn &)g;
      bool mergeWithBitOr = mergeop==mergeopBitwiseOr;
      data.prepareToModifyInPlace();
      data.getModifiableData().mergeWith(g2.data.getData(), mergeWithBitOr);
         // can throw an exception (disagreeing # of pre-save frames)
   }
   
   monotone_bitwise_dataflow_fn* merge(const monotone_bitwise_dataflow_fn &g,
				       mergeoperators mergeop) const {
      bool mergeWithBitOr = mergeop==mergeopBitwiseOr;
      const sparc_bitwise_dataflow_fn &g2 = (const sparc_bitwise_dataflow_fn &)g;
      sparc_bitwise_dataflow_fn_ll *bdf_ll = (sparc_bitwise_dataflow_fn_ll *)data.getData().merge(g2.data.getData(), mergeWithBitOr);
         // can throw an exception (disagreeing # of pre-save frames)
      sparc_bitwise_dataflow_fn *result = new sparc_bitwise_dataflow_fn(*bdf_ll);
      return result;

   }

   functions getFnForRegSavedLevel(reg_t &r, unsigned howFarBack) const {
      bool iThruBit, iGenBit;
      data.getData().getFnForRegSavedLevel(r, howFarBack, iThruBit, iGenBit);

      if (iThruBit)
         return fnpass;
      else if (iGenBit)
         return fnstart;
      else
         return fnstop;
   }
   unsigned getNumSavedWindows() const {
      return data.getData().getNumSavedWindows(); // preSaveFrames.size()-1
   }
   unsigned getNumLevels() const {
      return data.getData().getNumLevels(); // preSaveFrames.size()
   }
   unsigned getNumPreSaveFramesManufactured() const {
      return data.getData().getNumPreSaveFramesManufactured();
   }

   std::pair<regset_t *, regset_t *> getStartsAndStopsForLevel(unsigned level) const {
      assert(level < getNumLevels());
      return data.getData().getStartsAndStopsForLevel(level);
   }
   const regset_t *getThruForLevel(unsigned level) const {
      return data.getData().getThruForLevel(level);
   }
   const regset_t *getGenForLevel(unsigned level) const {
      return data.getData().getGenForLevel(level);
   }

   pdstring sprintf(const pdstring &starts_banner,
                    const pdstring &stops_banner,
                    bool starts_banner_first) const;

   // non-const methods, which most of the below routines are, become clumsy with
   // reference counters.
   
   // pass: pass along a proposition
   void pass(reg_t &r) {
      data.prepareToModifyInPlace();
      data.getModifiableData().pass(r);
   }
   monotone_bitwise_dataflow_fn& pass2(reg_t &r) {
      // like above pass, but returns a reference to "this", in the style of
      // cout "operator<<", most "operator++()" variants, and so on.
      // If I thought that there was no overhead in this, then I'd make this one the
      // default.
      pass(r);
      return *this;
   }
   void passSet(const regset_t &set) {
      data.prepareToModifyInPlace();
      data.getModifiableData().passSet(set);
   }
   monotone_bitwise_dataflow_fn& passSet2(const regset_t &set) {
      passSet(set);
      return *this;
   }
      
   // start: validate a proposition
   void start(reg_t &r) {
      data.prepareToModifyInPlace();
      data.getModifiableData().start(r);
   }
   monotone_bitwise_dataflow_fn& start2(reg_t &r) {
      start(r);
      return *this;
   }
   void startSet(const regset_t &set) {
      data.prepareToModifyInPlace();
      data.getModifiableData().startSet(set);
   }
   monotone_bitwise_dataflow_fn &startSet2(const regset_t &set) {
      startSet(set);
      return *this;
   }

   // stop: invalidate a proposition
   void stop(reg_t &r) {
      data.prepareToModifyInPlace();
      data.getModifiableData().stop(r);
   }
   monotone_bitwise_dataflow_fn& stop2(reg_t &r) {
      stop(r);
      return *this;
   }
   void stopSet(const regset_t &set) {
      data.prepareToModifyInPlace();
      data.getModifiableData().stopSet(set);
   }
   monotone_bitwise_dataflow_fn& stopSet2(const regset_t &set) {
      stopSet(set);
      return *this;
   }

   void processSaveInBackwardsDFProblem() {
      data.prepareToModifyInPlace();
      data.getModifiableData().processSaveInBackwardsDFProblem();
   }
   void processRestoreInBackwardsDFProblem() {
      data.prepareToModifyInPlace();
      data.getModifiableData().processRestoreInBackwardsDFProblem();
   }

   void setEachLevelTo(const regset_t *new_thru,
                       const regset_t *new_gen) {
      data.prepareToModifyInPlace();
      data.getModifiableData().setEachLevelTo(new_thru, new_gen);
   }
   
   void swapStartsAndStops() {
      data.prepareToModifyInPlace();
      data.getModifiableData().swapStartsAndStops();
   }
};

// See Stroustrup, D & E, sec 3.11.4.2:
class sparc_bitwise_dataflow_fn_counter {
 private:
   static int count;
  
 public:
   sparc_bitwise_dataflow_fn_counter() {
      if (count++ == 0) {
	 refCounterK<sparc_bitwise_dataflow_fn_ll>::initialize_static_stuff();
      }
   }
  ~sparc_bitwise_dataflow_fn_counter() {
      if (--count == 0)
	 refCounterK<sparc_bitwise_dataflow_fn_ll>::free_static_stuff();
   }
};
static sparc_bitwise_dataflow_fn_counter sparc_bdff_ctr;

#endif /* SPARC_BITWISE_DATAFLOW_FN_H_ */
