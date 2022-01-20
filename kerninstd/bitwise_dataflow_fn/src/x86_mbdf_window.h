// x86_mbdf_window.h
// mbdf stands for monotone bitwise dataflow function

#ifndef _X86_MBDF_WINDOW_H
#define _X86_MBDF_WINDOW_H

#include "mbdf_window.h"
#include "x86_reg.h"
#include "x86_reg_set.h"

class x86_mbdf_window : public mbdf_window {
 public:
   x86_mbdf_window(XDR *xdr) : mbdf_window(xdr) {}

   x86_mbdf_window() : mbdf_window() {}
      // a necessary evil, since there are calls to grow() in, e.g.,
      // monotone_bitwise_dataflow_fn::boost().  Also for igen-generated code.
     
   x86_mbdf_window(regset_t *ithru, regset_t *igen)
      : mbdf_window(ithru, igen) {}

   x86_mbdf_window(bool ithru, bool iGenForAll)
      : mbdf_window(ithru, iGenForAll) {}

   x86_mbdf_window(const mbdf_window &src) : mbdf_window(src) {}

   ~x86_mbdf_window() {}

   x86_mbdf_window& operator=(const x86_mbdf_window &src) {
      if (&src != this) {
         *thru = *(src.thru);
         *gen = *(src.gen);
      }
      return *this;
   }

   mbdf_window* compose(const mbdf_window &g) const {
      // this <- this o g  (i.e., evaluate g first, then this)
      x86_mbdf_window *result = new x86_mbdf_window(*this);
      result->composeWith(g);
      return result;
   }
   void swapStartsAndStops() {
      // thru: no change
      // gen: where thru is 1, keep unchanged (at 0).  Where thru is 0, swap gen
      *(x86_reg_set *)gen = 
         (~(*(x86_reg_set *)gen) & ~(*(x86_reg_set *)thru));
   }

   mbdf_window* composeWith(const mbdf_window &g) {
      // this <- this o g (i.e., evaluate g first, then this); return this
      
      // We must write to gen first, because if we wrote to thru first,
      // the calculation of gen would be screwed up, because it'd incorrectly
      // use the updated version of thru->
    
      const x86_mbdf_window *gref = (const x86_mbdf_window *) &g;
      //this assignment is necessary in order to make for easier
      //access to g's protected members (gen and thru) below.

      *gen |= *(x86_reg_set *)thru & *(x86_reg_set *)(gref->gen);
      
      *thru &= *(gref->thru);
      return this;
   }
   
   void mergeWith(const mbdf_window &g, bool mergeWithBitOr) {
      // 'this' is a monotone bitwise dataflow function; so is 'src'. We 
      // perform an in-place merge of these two fns (this = this merge g).
      // We use bit-and as the merge operator, unless the param mergeWithBitOr
      // is true, in which case we use bit-or.
       
      // According to Allen, ed. section 4.6.3, merging two monotone bitwise 
      // fns is simple:  thru = the merge of f.thru and g.thru, and
      // gen = the merge of f.gen and g.gen. But I tried a few examples, and 
      // this seems quite wrong for the thru case.
       
      // I derived the following equations by hand instead:
      // If the merge is bitwise-or:
      //   result.thru = (f.thru v g.thru) ^ ~f.gen ^ ~g.gen  (5 operations)
      //   result.gen = f.gen v g.gen                        (1 operation)
      // 
      // If the merge is bitwise-and:
      //   result.thru = (g.thru^f.thru) v (f.thru^g.gen) v (f.gen^g.thru)
      //               = (f.thru^(g.gen v g.thru)) v (f.gen^g.thru) (4 ops)
      //   result.gen = f.gen ^ g.gen                        (1 operation)

      const x86_mbdf_window *gref = (const x86_mbdf_window *) &g;
         //this assignment is necessary in order to make for easier
         //access to g's protected members (gen and thru) below.
       
      if (mergeWithBitOr) {
         *thru = (*(x86_reg_set *)thru | *(x86_reg_set *)(gref->thru)) & 
            ~(*(x86_reg_set *)gen) & ~(*(x86_reg_set *)(gref->gen));
         *gen |= *(gref->gen);
      }
      else {
         *thru = (*(x86_reg_set *)thru & (*(x86_reg_set *)(gref->gen) | 
                                             *(x86_reg_set *)(gref->thru))) 
            | *(x86_reg_set *)gen & *(x86_reg_set *)(gref->thru);
         *gen &= *(gref->gen);
      }
   }

   regset_t *getStarts() const {
      regset_t *result = regset_t::getRegSet(~(*(x86_reg_set *)thru) & 
                                             *(x86_reg_set *)gen);
      return result;
   }
   regset_t *getStops() const {
      regset_t *result = regset_t::getRegSet(~(*(x86_reg_set *)thru) & 
                                             ~(*(x86_reg_set *)gen));
      return result;
   }
};

#endif
