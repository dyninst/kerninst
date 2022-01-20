// mbdf_window.h
// mbdf stands for monotone bitwise dataflow function

#ifndef _MBDF_WINDOW_H
#define _MBDF_WINDOW_H

#include <utility> // STL's pair
#include "common/h/String.h" 

#include "instr.h"
#include "reg_set.h"

struct XDR;

class mbdf_window {
 protected:
     regset_t *thru, *gen;
     class Dummy {}; //used for an empty constructor, so that subclasses 
                  // can have constructors that instantiate thru and gen 
                  // the way they want (e.g. see sparc's save constructor)
     static Dummy dummy;
 public:
     static mbdf_window* getWindow(XDR *xdr);
     bool send(XDR *xdr) const {
        return thru->send(xdr) && gen->send(xdr);
     }

     mbdf_window() {
        thru = regset_t::getRegSet(regset_t::empty);
	gen = regset_t::getRegSet(regset_t::empty);
     }
      // a necessary evil, since there are calls to grow() in, e.g.,
      // monotone_bitwise_dataflow_fn::boost().  Also for igen-generated code.
     
     mbdf_window(regset_t *ithru, regset_t *igen)
	  : thru(ithru), gen(igen) {}

     mbdf_window(bool ithru, bool iGenForAll)
     {
       thru = (ithru ? regset_t::getRegSet(regset_t::full) : 
	       regset_t::getRegSet(regset_t::empty));
       gen = (iGenForAll ? regset_t::getRegSet(regset_t::full) : 
	      regset_t::getRegSet(regset_t::empty));
       
      // (We used to take in monotone_bitwise_dataflow_fn::functions, but we 
      // can't reach it now that we're in a different .h file)

      // There's a tricky situation here.  This ctor is used to initialize some
      // static members, so it mustn't access any other static members (like
      // regset_t::fullset), since the order of ctor calls for static members
      // is undefined in C++.  Leads to ugly code here; sorry.  Note how we avoid
      // using static members of class regset_t...except for ones which have
      // no data, just a type (like regset_t::empty, but not sparc_reg_set::emptyset)
     }

     mbdf_window(const mbdf_window &src) {
        thru = regset_t::getRegSet((regset_t &)*(src.thru));
        gen = regset_t::getRegSet((regset_t &)*(src.gen));
     
     }
     mbdf_window(XDR *xdr) {
       thru = regset_t::getRegSet(xdr);
       gen = regset_t::getRegSet(xdr);
     }

     mbdf_window(Dummy) {}

     virtual ~mbdf_window() {
        delete thru;
        delete gen;
     }

     mbdf_window& operator=(const mbdf_window &src);

     bool operator==(const mbdf_window &src) const {
	  return (*thru == *(src.thru) && *gen == *(src.gen));
     }

     bool operator!=(const mbdf_window &src) const {
	  return (*thru != *(src.thru) || *gen != *(src.gen));
     }

     void setLevelTo(regset_t *new_thru,
                   regset_t *new_gen) {
        delete thru;
	delete gen;
        thru = new_thru;
	gen = new_gen;
     }
   
     virtual void swapStartsAndStops() = 0;
      
     regset_t *apply(const regset_t &x) const {
        regset_t *result = regset_t::getRegSet(x);
	*result &= *thru;
	*result |= *gen;
	return result;
     }

     virtual mbdf_window* composeWith(const mbdf_window &g) = 0;
       
     void compose_overwrite_param(mbdf_window &g) const {
        // g <- this o g.  Doesn't bother returning g since the caller can get it
        // trivially.
        // As usual for any compose-type routine, we evaluate g first, then this.
        // The difference between composeWith() and this method is that the former
        // overwrites 'this' in storing its result, whereas this routine overwrites
        // 'g' in storing the result.

        // While testing, some assertions:
        //const mbdf_window shouldbe_g = this->compose(g);
      
        *(g.thru) &= *thru;
	*(g.gen) &= *thru;
	*(g.gen) |= *gen;

	//assert(g == shouldbe_g);
     }
   
     virtual mbdf_window* compose(const mbdf_window &g) const = 0;


     virtual void mergeWith(const mbdf_window &g, bool mergeWithBitOr) = 0;

     void pass(reg_t &r) {
        // to apply pass in bit r: set bit r of THRU to 1 and bit r of GEN to 0
        *thru += r; // set union
	*gen  -= r; // set removal
     }

     void passSet(const regset_t &regs) {
        // to apply pass in bit r: set bit r of THRU to 1 and bit r of GEN to 0
        *thru += regs; // set union
	*gen  -= regs; // set removal
     }

     void start(reg_t &r) {
        // to apply start in bit r: set bit r of THRU to 0 and bit r of GEN to 1
        *thru -= r;
	*gen  += r;
     }

     void startSet(const regset_t &set) {
        // to apply start in bit r: set bit r of THRU to 0 and bit r of GEN to 1
        *thru -= set; // set-removal
	*gen  += set; // set-union
     }

     void stop(reg_t &r) {
        // to apply stop in bit r: set bit r of THRU to 0 and bit r of GEN to 0
        *thru -= r; // set-removal
	*gen  -= r; // set-removal
     }

     void stopSet(const regset_t &set) {
        // to apply stop in bit r: set bit r of THRU to 0 and bit r of GEN to 0
        *thru -= set;
	*gen  -= set;
     };
     
     void getFnForReg(reg_t &r, bool &thrubit, bool &genbit) const {
        thrubit = thru->exists(r);
	genbit = gen->exists(r);
     }

     void processRestore(const mbdf_window &prevWindow);
      // there is no corresponding processSave(); instead, there's a save-ctor

     const regset_t* getThru() const {
        // It's more likely you'll want to call the higher-level getStarts()
        // and getStops().  Presently this is only used by xdr routines.
       return regset_t::getRegSet((regset_t &)*(thru));
     }
     const regset_t* getGen() const {
        // It's more likely you'll want to call the higher-level getStarts()
        // and getStops().  Presently this is only used by xdr routines.
        return regset_t::getRegSet((regset_t &)*(gen));
     }
   
     virtual regset_t* getStarts() const = 0;
     virtual regset_t* getStops() const = 0;
     std::pair<regset_t *, regset_t *> getStartsAndStops() const {
        return std::make_pair(getStarts(), getStops());
     }
   
     pdstring sprintf(const pdstring &starts_banner,
                      const pdstring &stops_banner, 
                      bool print_starts_first) const;
};

#endif
