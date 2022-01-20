#ifndef _POWER_BITWISE_DATAFLOW_FN_H_
#define _POWER_BITWISE_DATAFLOW_FN_H_

// See section 4.1.2, "bitwise functions on bit vector" in 1992 pre-print of
// Optimization in Compilers, ed. Fran Allen (Ari used it in cs701) -- any
// monotone bitwise fn can be represented by two bit vectors THRU and GEN;
// applying this function can be done easily: f(x) = (x ^ THRU(f)) v GEN(f);
// additionally, composing two such functions is easy, with fog represented
// by THRU=THRU1 ^ THRU2 and GEN=(THRU1 ^ GEN2) v GEN1.
// But merging (does this mean union?) is more complex (though we handle it).
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

#include "bitwise_dataflow_fn.h"
#include "power_mbdf_window.h"
#include "powerTraits.h"
#include "power_reg_set.h"

struct XDR;

//No register windows on power--things are so much simpler.  We pretend there
//is one single window throughout, thus making use of the mbdf_window class
// and its methods.  The code here is based on x86_bitwise_dataflow_fn, since
// x86 likewise does not have reg windows.

class power_bitwise_dataflow_fn : public monotone_bitwise_dataflow_fn {
 public:
  
   static const power_bitwise_dataflow_fn startAllRegs;
   static const power_bitwise_dataflow_fn stopAllRegs;
   static const power_bitwise_dataflow_fn passAllRegs;
   static const power_bitwise_dataflow_fn dummy_fn;

 private:
 
   power_mbdf_window data;
   bool Dummy;
   power_bitwise_dataflow_fn(const power_mbdf_window &srcdata)
	   : monotone_bitwise_dataflow_fn(), data(srcdata), Dummy(false) {}

   power_bitwise_dataflow_fn(bool iThruBit, bool iGenBit) :
      monotone_bitwise_dataflow_fn( ),
      data(power_mbdf_window(iThruBit, iGenBit)), Dummy(false)
     {}

 public:

   power_bitwise_dataflow_fn(XDR *xdr) : monotone_bitwise_dataflow_fn (),data(xdr), Dummy(false) {}
   bool send(XDR *xdr) const { return data.send(xdr); }

   /* Do we need this one for power? */
   power_bitwise_dataflow_fn(bool, bool, bool, bool) : monotone_bitwise_dataflow_fn(), Dummy(true) { }

   power_bitwise_dataflow_fn() : monotone_bitwise_dataflow_fn(), Dummy(false) {}
   
   power_bitwise_dataflow_fn(const power_bitwise_dataflow_fn &src) :
      monotone_bitwise_dataflow_fn(),
      data(src.data), Dummy(src.Dummy) {}

   power_bitwise_dataflow_fn(const monotone_bitwise_dataflow_fn &src) :
      monotone_bitwise_dataflow_fn(),
      data(((power_bitwise_dataflow_fn &)src).data),
      Dummy(((power_bitwise_dataflow_fn &)src).Dummy)  {}
   
   power_bitwise_dataflow_fn(functions iFn) :
      monotone_bitwise_dataflow_fn(),
      data(iFn == fnpass ? passAllRegs.data :
           iFn == fnstart ? startAllRegs.data : stopAllRegs.data),
	   Dummy(false) {
      assert(iFn == fnpass || iFn == fnstart || iFn == fnstop);
   }

   // fast assignment operator:
   power_bitwise_dataflow_fn &operator=(const power_bitwise_dataflow_fn &src) {
      if (&src == this) return *this;
      data = src.data;
      return *this;
   }

   // no faster, no slower: const methods
   bool operator==(const power_bitwise_dataflow_fn &cmp) const {
      if (&cmp == this) return true;
      return data == ((power_bitwise_dataflow_fn &)cmp).data;
   }

   
   bool operator==(const monotone_bitwise_dataflow_fn &cmp) const {
      if (&cmp == this) return true;
      return data == ((power_bitwise_dataflow_fn &)cmp).data;
   }
   
   bool operator!=(const monotone_bitwise_dataflow_fn &cmp) const {
      return !operator==(cmp);
   }
   

   bool isDummy() const {
      return Dummy; 
   }
   
unsigned getMemUsage_exceptObjItself() const {
   return sizeof(data);
}


   regset_t *apply(const regset_t &x) const {
      return data.apply(x);
   }

   monotone_bitwise_dataflow_fn*
      compose(const monotone_bitwise_dataflow_fn &g) const {
      const power_bitwise_dataflow_fn &g2 = (const power_bitwise_dataflow_fn &)g;
      power_mbdf_window *resultWindow = 
         (power_mbdf_window *)data.compose(g2.data);
      power_bitwise_dataflow_fn *result =  
         new power_bitwise_dataflow_fn(*resultWindow);
      delete resultWindow;
      return result;
   }
   
   void compose_overwrite_param(monotone_bitwise_dataflow_fn &g) const {
      // calculates f o g (f is "this"; g is the parameter), and writes the result
      // onto g.
      power_bitwise_dataflow_fn &g2 = (power_bitwise_dataflow_fn &)g;
      data.compose_overwrite_param(g2.data);
   }

   void mergeWith(const monotone_bitwise_dataflow_fn &g, mergeoperators mergeop) {
      const power_bitwise_dataflow_fn &g2 = (const power_bitwise_dataflow_fn &)g;
      bool mergeWithBitOr = mergeop==mergeopBitwiseOr;
      
      data.mergeWith(g2.data, mergeWithBitOr);
    
   }

   monotone_bitwise_dataflow_fn* merge(const monotone_bitwise_dataflow_fn &g,
                                      mergeoperators mergeop) const {  
      power_bitwise_dataflow_fn *result = (power_bitwise_dataflow_fn *)getDataflowFn(*this);
      result->mergeWith(g, mergeop);
         
      return result;
   }
	
   pdstring sprintf(const pdstring &starts_banner,
                  const pdstring &stops_banner,
                  bool starts_banner_first) const {
      return data.sprintf(starts_banner, stops_banner,
			  starts_banner_first);
   }

   // pass: pass along a proposition
   void pass(reg_t &r) {
      data.pass(r);
   }
  monotone_bitwise_dataflow_fn &pass2(reg_t &r) {
     // like above pass, but returns a reference to "this", in the style of
     // cout "operator<<", most "operator++()" variants, and so on.
     // If I thought that there was no overhead in this, then I'd make this one the
     // default.
     pass(r);
     return *this;
   }
   

   void passSet(const regset_t &set) {
      data.passSet(set);
   }

   monotone_bitwise_dataflow_fn &passSet2(const regset_t &set) {
      passSet(set);
      return *this;
   }

   // start: validate a proposition
   void start(reg_t &r) {
      data.start(r);
   }

   monotone_bitwise_dataflow_fn &start2(reg_t &r) {
      start(r);
      return *this;
   }

   void startSet(const regset_t &set) {
      data.startSet(set);
   }

   monotone_bitwise_dataflow_fn &startSet2(const regset_t &set) {
      startSet(set);
      return *this;
   }

   // start: invalidate a proposition
   void stop(reg_t &r) {
      data.stop(r);
   }
   monotone_bitwise_dataflow_fn &stop2(reg_t &r) {
      stop(r);
      return *this;
   }

   void stopSet(const regset_t &set) {
      data.stopSet(set);
   }

   monotone_bitwise_dataflow_fn &stopSet2(const regset_t &set) {
      stopSet(set);
      return *this;
   }

   void swapStartsAndStops() {
      data.swapStartsAndStops();
   }

   const regset_t *getGenForLevel(unsigned level) const {
      assert (level == 0);
      return data.getGen();
   }

   const regset_t *getThruForLevel(unsigned level) const { 
      assert (level == 0);
      return data.getThru();
   }

   unsigned getNumSavedWindows() const { return 0; };
   unsigned getNumLevels() const {return 1; //we always have 1 level (no windows)  
   }
   unsigned getNumPreSaveFramesManufactured() const {return 0;} 

   
   //The rest are not applicable to POWER, but need to be declared, since
   //they are virtual

   std::pair<regset_t *, regset_t *> getStartsAndStopsForLevel
      (unsigned level) const { 
      assert (level == 0);
      return data.getStartsAndStops();
   }
  
   void getFnForRegSavedLevel(reg_t &r, unsigned howFarBack,
                              bool &iThruBit, bool &iGenBit) const {
      assert (howFarBack == 0);
      data.getFnForReg(r, iThruBit, iGenBit);
   }

   functions getFnForRegSavedLevel(reg_t &r, unsigned howFarBack) const {
      assert(howFarBack == 0);
      bool iThruBit, iGenBit;
      data.getFnForReg(r, iThruBit, iGenBit);
      
      if (iThruBit)
         return fnpass;
      else if (iGenBit)
         return fnstart;
      else
         return fnstop;
   }
   void setEachLevelTo(const regset_t *new_thru,
                       const regset_t *new_gen) {
      data.setLevelTo(regset_t::getRegSet(*new_thru), 
                      regset_t::getRegSet(*new_gen));
   }
};

#endif /* _POWER_BITWISE_DATAFLOW_FN_H_ */
