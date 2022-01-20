// sparc_mbdf_window.h
// mbdf stands for monotone bitwise dataflow function

#ifndef _SPARC_MBDF_WINDOW_H
#define _SPARC_MBDF_WINDOW_H

#include "mbdf_window.h"
#include "sparc_reg.h"
#include "sparc_reg_set.h"

class sparc_mbdf_window : public mbdf_window {
 public:
     sparc_mbdf_window(XDR *xdr) : mbdf_window(xdr) {
     }

     sparc_mbdf_window() : mbdf_window() {
     }
      // a necessary evil, since there are calls to grow() in, e.g.,
      // monotone_bitwise_dataflow_fn::boost().  Also for igen-generated code.
     
     sparc_mbdf_window(regset_t *ithru, regset_t *igen)
	  : mbdf_window(ithru, igen) {}

     sparc_mbdf_window(bool ithru, bool iGenForAll)
          : mbdf_window(ithru, iGenForAll) {}

     sparc_mbdf_window(const mbdf_window &src) : mbdf_window(src) {}

     ~sparc_mbdf_window() {
     }

     class Save {};
     static Save save;
     sparc_mbdf_window(Save, const sparc_mbdf_window &src);

     sparc_mbdf_window& operator=(const sparc_mbdf_window &src);

     bool operator==(const sparc_mbdf_window &src) const {
	  return (*thru == *(src.thru) && *gen == *(src.gen));
     }

     bool operator!=(const sparc_mbdf_window &src) const {
	  return (*thru != *(src.thru) || *gen != *(src.gen));
     }

     bool equalIsAndLs(const sparc_mbdf_window &src) const {
        return (((sparc_reg_set *)thru)->equalIsAndLs((sparc_reg_set &)*(src.thru)) &&
		((sparc_reg_set *)gen)->equalIsAndLs((sparc_reg_set &)*(src.gen)));
     }

     mbdf_window* composeWith(const mbdf_window &g) {
        // this <- this o g (i.e., evaluate g first, then this); return this

        sparc_mbdf_window &g2 = (sparc_mbdf_window&)g;
        
	// We must write to gen first, because if we wrote to thru first,
        // the calculation of gen would be screwed up, because it'd incorrectly
        // use the updated version of thru
        *(sparc_reg_set*)gen |= *(sparc_reg_set*)thru & *(sparc_reg_set*)(g2.gen);
	*(sparc_reg_set*)thru &= *(sparc_reg_set*)(g2.thru);
	return this;
     }

     mbdf_window* compose(const mbdf_window &g) const {
        // this <- this o g  (i.e., evaluate g first, then this)
        sparc_mbdf_window *result = new sparc_mbdf_window(*this);
	result->composeWith(g);
	return result;
     }

     void mergeWith(const mbdf_window &g, bool mergeWithBitOr) {
        sparc_mbdf_window &g2 = (sparc_mbdf_window&)g;
        if(mergeWithBitOr) {
           *thru = (*(sparc_reg_set*)thru | *(sparc_reg_set*)(g2.thru)) &
	           ~(*(sparc_reg_set*)gen) & ~(*(sparc_reg_set*)(g2.gen));
           *gen |= *(g2.gen);
        }
        else {
           *thru = (*(sparc_reg_set*)thru & 
                    (*(sparc_reg_set*)(g2.gen) | *(sparc_reg_set*)(g2.thru))) |
	           (*(sparc_reg_set*)gen & *(sparc_reg_set*)(g2.thru));
           *gen &= *(g2.gen);
        }
     }

     void processRestore(const sparc_mbdf_window &prevWindow);
      // there is no corresponding processSave(); instead, there's a save-ctor

    regset_t *getStarts() const {
       regset_t *result = 
          regset_t::getRegSet(~(*(sparc_reg_set *)thru) & *(sparc_reg_set *)gen);
       return result;
    }
    regset_t *getStops() const {
       regset_t *result = 
          regset_t::getRegSet(~(*(sparc_reg_set *)thru) & 
                              ~(*(sparc_reg_set *)gen));
       return result;
    }

    void swapStartsAndStops() {
       // thru: no change
       // gen: where thru is 1, keep unchanged (at 0).  Where thru is 0, swap gen
       *(sparc_reg_set *)gen = (~(*(sparc_reg_set *)gen) & 
				~(*(sparc_reg_set *)thru));
    }
};

#endif
