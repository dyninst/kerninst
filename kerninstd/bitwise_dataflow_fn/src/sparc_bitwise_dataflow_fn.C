// sparc_bitwise_dataflow_fn.C
// Ariel Tamches
// A class to manage a bitwise data-flow-function for sparc registers

// See the .h file for overview comments...

#include "sparc_bitwise_dataflow_fn.h"
#include "util/h/out_streams.h"
#include "util/h/HomogenousHeapUni.h"
#include "util/h/rope-utils.h" // num2string()
#include "util/h/xdr_send_recv.h"

// Stuff that's normally in kerninstd/xdr/xdr_send_recv_common.[hC],
// but the test program doesn't want to pick up all that cruft.  We make it static here.
static bool P_xdr_send(XDR *xdr, const sparc_mbdf_window &w) {
   return w.send(xdr);
}
static bool P_xdr_recv(XDR *xdr, sparc_mbdf_window &w) {
   new((void*)&w)sparc_mbdf_window(xdr);
   return true;
}

/* *************************************************************************** */

// static member declaration:
int sparc_bitwise_dataflow_fn_counter::count; // intialized to 0

HomogenousHeapUni<refCounterK<sparc_bitwise_dataflow_fn_ll>::ss, 1> *refCounterK<sparc_bitwise_dataflow_fn_ll>::ssHeap;

sparc_mbdf_window::Save sparc_mbdf_window::save;


/* *************************************************************************** */

sparc_bitwise_dataflow_fn
sparc_bitwise_dataflow_fn::passAllRegs(true, false);

sparc_bitwise_dataflow_fn
sparc_bitwise_dataflow_fn::startAllRegs(false, true);

sparc_bitwise_dataflow_fn
sparc_bitwise_dataflow_fn::stopAllRegs(false, false);

sparc_bitwise_dataflow_fn
sparc_bitwise_dataflow_fn::dummy_fn(false, false, false, false);

/* *************************************************************************** */

sparc_mbdf_window& sparc_bitwise_dataflow_fn_ll::top() {
   assert(preSaveFrames.size() > 0);
   const unsigned topndx = preSaveFrames.size() - 1;
   return preSaveFrames[topndx];
}

const sparc_mbdf_window& sparc_bitwise_dataflow_fn_ll::top() const {
   assert(preSaveFrames.size() > 0);
   const unsigned topndx = preSaveFrames.size() - 1;
   return preSaveFrames[topndx];
}

/* *************************************************************************** */

// These are ptrs since we can't rely on the order of construction of
// static vrbles
static HomogenousHeapUni<sparc_mbdf_window, 1> *customVecWindowSingles;
static HomogenousHeapUni<sparc_mbdf_window, 2> *customVecWindowDoubles;

unsigned sparc_mbdf_window_alloc::getMemUsage_1heap() {
   // a static method
   unsigned forRawData = 0;
   unsigned forMetaData = 0;

   if (customVecWindowSingles)
      customVecWindowSingles->getMemUsage(forRawData, forMetaData);

   return forRawData + forMetaData;
}
unsigned sparc_mbdf_window_alloc::getMemUsage_2heap() {
   // a static method
   unsigned forRawData = 0;
   unsigned forMetaData = 0;

   if (customVecWindowDoubles)
      customVecWindowDoubles->getMemUsage(forRawData, forMetaData);

   return forRawData + forMetaData;
}

sparc_mbdf_window* sparc_mbdf_window_alloc::alloc(unsigned nitems) {
   assert(nitems >= 1);
   if (nitems==1) {
      if (customVecWindowSingles == NULL) {
	 customVecWindowSingles = new HomogenousHeapUni<sparc_mbdf_window, 1>(512);
	 assert(customVecWindowSingles);
      }
      return (sparc_mbdf_window *) customVecWindowSingles->alloc();
   }
   else if (nitems==2) {
      if (customVecWindowDoubles == NULL) {
	 customVecWindowDoubles = new HomogenousHeapUni<sparc_mbdf_window, 2>(512);
	 assert(customVecWindowDoubles);
      }
      return (sparc_mbdf_window *) customVecWindowDoubles->alloc();
   }
   else
      return (sparc_mbdf_window *) malloc(sizeof(sparc_mbdf_window)*nitems);
}

void sparc_mbdf_window_alloc::free(sparc_mbdf_window *vec, unsigned nitems) {
   assert(nitems >= 1);
   if (nitems==1) {
      assert(customVecWindowSingles);
      customVecWindowSingles->free((dptr_t)vec);
   }
   else if (nitems==2) {
      assert(customVecWindowDoubles);
      customVecWindowDoubles->free((dptr_t)vec);
   }
   else
      ::free(vec);
}

/***************************************************************************/

template <class T>
static inline void destruct(T *ptr) {
   ptr->~T();
}

sparc_bitwise_dataflow_fn_ll::sparc_bitwise_dataflow_fn_ll(XDR *xdr) : 
  monotone_bitwise_dataflow_fn_ll()
{
   if (!P_xdr_recv(xdr, numPreSaveFramesManufactured))
      throw xdr_recv_fail();
   
   // destruct preSaveFrames before receiving it (P_xdr_recv(), as usual, expects
   // allocated-but-uninitialized memory)
   destruct(&preSaveFrames);
   if (!P_xdr_recv(xdr, preSaveFrames))
      throw xdr_recv_fail();
}

bool sparc_bitwise_dataflow_fn_ll::send(XDR *xdr) const {
   // throws xdr_send_fail() on failure

   return P_xdr_send(xdr, numPreSaveFramesManufactured) &&
          P_xdr_send(xdr, preSaveFrames);
}

sparc_bitwise_dataflow_fn_ll::sparc_bitwise_dataflow_fn_ll(bool iThruBit,
							   bool iGenBit) :
   monotone_bitwise_dataflow_fn_ll()
{
   (void)new((void*)preSaveFrames.append_with_inplace_construction())
      sparc_mbdf_window(iThruBit, iGenBit);
   assert(preSaveFrames.size() == 1);
   numPreSaveFramesManufactured = 0;
}

sparc_bitwise_dataflow_fn_ll::
sparc_bitwise_dataflow_fn_ll(const sparc_bitwise_dataflow_fn_ll &src) :
   monotone_bitwise_dataflow_fn_ll(), preSaveFrames(src.preSaveFrames)
{
   numPreSaveFramesManufactured = src.numPreSaveFramesManufactured;
}

sparc_bitwise_dataflow_fn_ll&
sparc_bitwise_dataflow_fn_ll::operator=(const sparc_bitwise_dataflow_fn_ll &src) {
   // no need to check for &src = this; vector does it for us (and it's harmless if
   // numPreSaveFramesManufactured)

   preSaveFrames = src.preSaveFrames;
   numPreSaveFramesManufactured = src.numPreSaveFramesManufactured;

   return *this;
}

bool sparc_bitwise_dataflow_fn_ll::
operator==(const sparc_bitwise_dataflow_fn_ll &src) const {
   // operator==() is hopelessly broken for class vector, so we do stuff manually

   // First, get the hacks for dummy_fn out of the way:
   if (numPreSaveFramesManufactured == UINT_MAX)
      return src.numPreSaveFramesManufactured == UINT_MAX;
   else if (src.numPreSaveFramesManufactured == UINT_MAX)
      return false;

   // Ok, now neither 'this' nor 'src' are dummy dataflow functions
   if (numPreSaveFramesManufactured != src.numPreSaveFramesManufactured)
      return false;

   if (preSaveFrames.size() != src.preSaveFrames.size())
      return false;

   assert(preSaveFrames.size() > 0);

   // First check the contents of the stack _excluding_ the current window.
   // These are treated a bit differently; in particular, we compare only
   // the %i and %l registers
   for (unsigned lcv=0; lcv < preSaveFrames.size()-1; lcv++)
      if (!preSaveFrames[lcv].equalIsAndLs(src.preSaveFrames[lcv]))
	 return false;

   // Last check the top element of the stack
   if (top() != src.top())
      return false;

   return true;
}

bool sparc_bitwise_dataflow_fn_ll::
operator!=(const sparc_bitwise_dataflow_fn_ll &src) const {
   return !operator==(src);
}

void sparc_bitwise_dataflow_fn_ll::setEachLevelTo(const regset_t *new_thru,
						  const regset_t *new_gen) {
   for (unsigned lev=0; lev < preSaveFrames.size(); lev++)
      preSaveFrames[lev].setLevelTo(regset_t::getRegSet(*new_thru), 
				    regset_t::getRegSet(*new_gen));

   // no changes to numPreSaveFramesManufactured or to preSaveFrames.size()
}

void sparc_bitwise_dataflow_fn_ll::swapStartsAndStops() {
   for (unsigned lev=0; lev < preSaveFrames.size(); lev++)
      preSaveFrames[lev].swapStartsAndStops();

   // no changes to numPreSaveFramesManufactured or to preSaveFrames.size()
}

regset_t* sparc_bitwise_dataflow_fn_ll::apply(const regset_t &x) const {
   // calculates f(x), where f, the monotone bitwise data flow function, is 'this'.
   // f(x) = (x ^ THRU(f)) v GEN(f)

   // Is this right in the case where there are elements back on the stack
   // and/or when numPreSaveFramesManufactured>0???  NO -- we must 'simulate'
   // step by step, a la compose()!
   if (preSaveFrames.size()==1 && numPreSaveFramesManufactured==0)
      return top().apply(x);
   else if (preSaveFrames.size() > 1 && numPreSaveFramesManufactured == 0) {
      // A part of me would like to assert fail here.
      // But instead, we'll just apply() the top() of the stack.
      //assert(false && "sorry, can't (yet) apply() a non-trivial dataflow fn");
      return top().apply(x);
   }
   else if (preSaveFrames.size() == 0) {
      //assert(false && "preSaveFrames.size() should never be 0 (in apply())");
      throw CannotApply();
   }
   else { // numPreSaveFramesManufactured > 0
      throw CannotApply();
      //assert(false); // until we figure out what to do here.
   }
}

monotone_bitwise_dataflow_fn_ll*
sparc_bitwise_dataflow_fn_ll::compose(const monotone_bitwise_dataflow_fn_ll &g) const {
   // Composing is tricky.  Let's calculate f o g, where f is 'this', and g is the
   // parameter.
   // 
   // What is f?  let's say it did some work on window A, then pushed A and did
   // some work on B, then pushed B and finished off by doing some work on C.
   // Thus, C is f's top() right now.
   //
   // What is g?  let's say it did some work on window W, then pushed W and did
   // some work on X, then pushed X and finished off by doing some work on Y.
   // Thus, Y is g's top() right now.
   //
   // As always, f o g means first perform g, then perform f.  So, reading from above,
   // f o g is:
   //    work on W, push W, work on X, push X, work on Y, then
   //    work on A (aliased to Y, right?), push A, work on B, push B, work on C.
   //
   // When we think of it that way, composition is easy...start off with g, then
   // compose g's top and f's bottom (A) [perform g's top first], then push B and C
   // (the rest of f).
   //
   // Thus, the algorithm is:
   // let result <- g
   // loop thru f's stack, starting from the bottom:
   //    compose a single window: result.top() <- f[lcv] o result.top()
   //                       (i.e. first do result.top(), then do f[lcv])
   //                       But: remember that f[lcv] is a pre-save window, so it only
   //                       defines %l's and %i's (not %o's or %g's).
   //    except for lcv==top, do: result.restoreInBackwardsDFProblem()
   //                       [push a new frame onto the stack]
   //
   // But what in the event that f has numPreSaveFramesManufactured > 0?
   // It means that f has done some save(s) in a backwards df problem.
   // let result <- g  (yes, even if g has numPreSaveFramesManufactured > 0)
   // do f.numPreSaveFramesManufactured times:
   //    result.restore  (i.e., a save in a backwards dflow problem)
   //                    (result now has an even bigger numPreSaveFramesManufactured)
   // result.top() <- f.top o result.top()  (i.e. do result.top() first then f.top)

   sparc_bitwise_dataflow_fn_ll *result = new 
     sparc_bitwise_dataflow_fn_ll((sparc_bitwise_dataflow_fn_ll &)g);
      // not a fast assignment.  But reference counting this wouldn't help much, since
      // it won't be long before a change is made to 'result', necessitating a "real"
      // copy anyway.  We're just recognizing the inevitable by doing it now.

   if (this->numPreSaveFramesManufactured > 0) {
      for (unsigned lcv=0; lcv < this->numPreSaveFramesManufactured; ++lcv)
	 result->processSaveInBackwardsDFProblem();

      // result now may or may not have numPreSaveFramesManufactured > 0

      sparc_mbdf_window &resulttop = result->top();

      top().compose_overwrite_param(resulttop);
      // calculates top() o resulttop, and writes the result onto resulttop

//        resulttop = top().compose(resulttop);
//           // carefully notice the ordering!  We want to compose by doing resulttop first,
//           // then by doing this->top().  That's why we can't have the line
//           // resulttop.composeWith(this->top()) -- it would do things in the wrong
//           // order!
   }
   else {
      const unsigned num_presave_frames = this->preSaveFrames.size();
      for (unsigned lcv=0; lcv < num_presave_frames; ++lcv) {
	 sparc_mbdf_window &resulttop = result->top();

         this->preSaveFrames[lcv].compose_overwrite_param(resulttop);
         // calculates this->preSaveFrames[lcv] o resulttop and writes the result
         // onto resulttop.
         
//  	 resulttop = this->preSaveFrames[lcv].compose(resulttop);
//  	    // carefully notice the ordering!  We want to do resulttop first, then
//              // do this->top().  That's why we don't have the line
//  	    // resulttop.composeWith(this->preSaveFrames[lcv]); it would do things in
//              // the wrong order!
//  	    // WARNING: Remember, for lcv < top, only %i and %l (and perhaps %g;
//              // I'm not sure) are defined.  But perhaps it's okay to compose the
//  	    // whole window because we're about to do a restore (save in backwards
//              // flow problem) anyway.
	 
	 if (lcv < this->preSaveFrames.size()-1)
	    result->processRestoreInBackwardsDFProblem();
      }
   }

   return result;
}

void sparc_bitwise_dataflow_fn_ll::compose_overwrite_param(monotone_bitwise_dataflow_fn_ll &g) const {
   // calculates f o g, where f is 'this' and g is the parameter.
   // Writes the result to g instead of returning it.  That's the difference between
   // this routine and compose().  Perfer this routine over compose() for performance
   // when it's OK to overwrite g.  Otherwise go with the slower compose()

   sparc_bitwise_dataflow_fn_ll &g2 = (sparc_bitwise_dataflow_fn_ll &)g;

#ifdef _DO_EVEN_EXPENSIVE_ASSERTS_
   sparc_bitwise_dataflow_fn_ll *shouldbe_g = compose(g);
#endif
   
   for (unsigned lcv=0; lcv < numPreSaveFramesManufactured; ++lcv)
      g2.processSaveInBackwardsDFProblem();
      // g now may or may not have numPreSaveFramesManufactured > 0

   const unsigned num_presave_frames = this->preSaveFrames.size();
   for (unsigned lcv=0; lcv < num_presave_frames - 1; ++lcv) {
      preSaveFrames[lcv].compose_overwrite_param(g2.top());
         // calculates this->preSaveFrames[lcv] o g.top() and writes the result
         // onto g.top()

      g2.processRestoreInBackwardsDFProblem();
   }

   top().compose_overwrite_param(g2.top());
      // calculates top() o g.top(), and writes the result onto g.top()

#ifdef _DO_EVEN_EXPENSIVE_ASSERTS_
   assert(g2 == *shouldbe_g);
   delete shoulbe_g;
#endif
}

void sparc_bitwise_dataflow_fn_ll::boost() {
   // If there are no pre-save frames manufactured, pushing an all-pass entry
   // onto the bottom of the stack is harmless.  That's what this routine is for.

   assert(numPreSaveFramesManufactured == 0);

   unsigned oldsize = preSaveFrames.size(); // also newsize's maxndx
   preSaveFrames.grow(oldsize + 1);

   // shift
   for (unsigned lcv=oldsize; lcv >= 1; lcv--) {
      preSaveFrames[lcv] = preSaveFrames[lcv-1];
   }

   // Now set preSaveFrames[0] to all-PASS (normally we would want to include the
   // overlap from preSaveFrames[1], which would mean that preSaveFrames[0]'s %o
   // registers should be assigned a copy of preSaveFrames[1]'s %i registers...but we
   // don't do that because preSaveFrames[0], like all non-top frames, only defines %i
   // and %l, not %o or %g.)
   preSaveFrames[0].passSet(sparc_reg_set::fullset);
}

void
sparc_bitwise_dataflow_fn_ll::mergeWith(const monotone_bitwise_dataflow_fn_ll &g,
					bool mergeWithBitOr) {
   // this <- this merge g
   // To merge, we merge individual windows of equivalent stack levels.
   // If there are different numbers of pre-save frames, we assert fail.

   const sparc_bitwise_dataflow_fn_ll &g2 = (const sparc_bitwise_dataflow_fn_ll &)g;

   const bool existPreSaveFrames = this->numPreSaveFramesManufactured > 0 ||
                                   g2.numPreSaveFramesManufactured > 0;

   if (existPreSaveFrames)
      if (this->numPreSaveFramesManufactured == g2.numPreSaveFramesManufactured) {
	 this->top().mergeWith(g2.top(), mergeWithBitOr);
	 return;
      }
      else {
	dout << "trying to merge 'this' (" << numPreSaveFramesManufactured << ") pre-save frames manufactured" << endl;
        dout << "with 'g' (" << g2.numPreSaveFramesManufactured << ") pre-save frames manufactured" << endl;
        
        throw DisagreeingNumberOfPreSaveFramesInMerge();
      }

   assert(this->numPreSaveFramesManufactured == 0);
   assert(g2.numPreSaveFramesManufactured == 0);

   // If f or g has fewer entries on the preSaveWindow stack than the other, we do:
   // artificially put all-PASS windows on the bottom of the stack, using boost().

   while (this->preSaveFrames.size() < g2.preSaveFrames.size())
      this->boost();

   const bool changesToG = (g2.preSaveFrames.size() < this->preSaveFrames.size());
   if (!changesToG) {
      // avoids making a copy of g in the common case
      assert(this->preSaveFrames.size() == g2.preSaveFrames.size());

      unsigned num_presave_frames = this->preSaveFrames.size();
      for (unsigned lcv=0; lcv < num_presave_frames; lcv++)
         this->preSaveFrames[lcv].mergeWith(g2.preSaveFrames[lcv], mergeWithBitOr);
            // while it's true that non-top() windows only define Is and Ls, there's
            // no harm merging the undefined regs, since in the result they're also
            // undefined (a by-product of always merging equivalent levels)
   }
   else {
      sparc_bitwise_dataflow_fn_ll fn2 = g2; // we're gonna change fn2
      // WARNING: don't use 'g' after this point!

      while (fn2.preSaveFrames.size() < this->preSaveFrames.size())
         fn2.boost();

      assert(this->preSaveFrames.size() == fn2.preSaveFrames.size());

      for (unsigned lcv=0; lcv < this->preSaveFrames.size(); lcv++)
         this->preSaveFrames[lcv].mergeWith(fn2.preSaveFrames[lcv], mergeWithBitOr);
            // while it's true that non-top() windows only define Is and Ls, there's
            // no harm merging the undefined regs, since in the result they're also
            // undefined (a by-product of always merging equivalent levels)
   }
}

void sparc_bitwise_dataflow_fn_ll::pass(reg_t &r) {
   top().pass(r);
}

void sparc_bitwise_dataflow_fn_ll::passSet(const regset_t &regs) {
   top().passSet(regs);
}

void sparc_bitwise_dataflow_fn_ll::start(reg_t &r) {
   top().start(r);
}

void sparc_bitwise_dataflow_fn_ll::startSet(const regset_t &set) {
   top().startSet(set);
}

void sparc_bitwise_dataflow_fn_ll::stop(reg_t &r) {
   top().stop(r);
}

void sparc_bitwise_dataflow_fn_ll::stopSet(const regset_t &set) {
   top().stopSet(set);
}

void sparc_bitwise_dataflow_fn_ll::processSaveInBackwardsDFProblem() {
   // The current working window (stack top) gets popped.  Before it gets thrown
   // away, old top xfers its %g's to the new top's %g's, and its %i's to the new
   // top's %o's.
   // What if a previous stack window doesn't exist?  We create a new frame with
   // all-PASS assumptions (yes, this is safe; no need for all-START assumptions).
   // Additionally, we increment a vrble numPreSaveFramesManufactured (when it's
   // nonzero, the stack itself contains just the top) to make a note of the fact
   // that the stack top is really what's obtained after a save is processed.

   assert(preSaveFrames.size() >= 1);
   if (preSaveFrames.size() == 1) {
      // Fudge a window onto the bottom of the stack...the top o' stack is unchanged,
      // but we increment numPreSaveFramesManufactured to indicate that we're working
      // with a pre-save frame.

      numPreSaveFramesManufactured++;

      const sparc_mbdf_window oldtop = top(); // we'll use this for an assert, too

      sparc_mbdf_window w(true, false); // true, false --> passfn (thru=true, gen=false)
      preSaveFrames.resize(2);
      preSaveFrames[1] = oldtop; // move top to its new position
      preSaveFrames[0] = w; // sneak this into position at the bottom of the stack

      assert(top() == oldtop);
   }

   assert(preSaveFrames.size() >= 2);
      // the current top() plus at least one previous frame.

   const sparc_mbdf_window oldtop = top();
   preSaveFrames.resize(preSaveFrames.size()-1);

   if (numPreSaveFramesManufactured > 0)
      assert(preSaveFrames.size() == 1);
   
   sparc_mbdf_window &newWorkingWindow = top();
   // set %g's of newWorkingWindow to the %g's of oldtop;
   // set %o's of newWorkingWindow to the %i's of oldtop;
   // the %l's and %o's of oldtop are thrown in the bit bucket forever.

   newWorkingWindow.processRestore(oldtop); // yes, processRestore
}

void sparc_bitwise_dataflow_fn_ll::processRestoreInBackwardsDFProblem() {
   // push a new window onto the window stack, which thus becomes the new top().
   // characteristics of this new window: g's get g's of old top, i's get
   // o's of old top, l's and o's get STOP (yea!)  [actually, this isn't general
   // enough...we should set it to whatever is optimistic...for some data flow
   // problems, that's STOP and for others that's START...the ctor should probably
   // be passed that information!!  Of course, in either event, it's still "yea!!"]
   // But if numPreSaveFramesManufactured was nonzero, then we decrement that
   // count, don't push or pop anything on the stack, and then adjust the stack top
   // (which should be the only elem on the stack) in the familiar way:
   // no change to %g's; %i's get %o's.

   // We have been working on the post-restore / pre-save frame.
   if (numPreSaveFramesManufactured > 0) {
      assert(preSaveFrames.size() == 1); // just the mandatory frame
      
      // The current stack top is actually a pre-save/post-restore frame.
      // And since we're doing a restore in a backwards dataflow problem, we want
      // to move to a pre-save/post-restore frame...so things work out great!
      // We can just decrement numPreSaveFramesManufactured and process a reverse
      // restore (no change to %g's; %i's get %o's)

      numPreSaveFramesManufactured--;
      const sparc_mbdf_window &oldTop = top();
      sparc_mbdf_window newWindow(sparc_mbdf_window::save, oldTop);
      top() = newWindow;
   }
   else {
      sparc_mbdf_window newTopWindow(sparc_mbdf_window::save, top()); // yes, save-ctor
         // special 'save-ctor' of class window.  Creates new frame, copies the
         // appropriate registers to it (and sets others to STOP...yea)
      preSaveFrames += newTopWindow;
   }
}

void sparc_bitwise_dataflow_fn_ll::getFnForRegSavedLevel(reg_t &r,
							 unsigned howFarBack,
							 bool &thru_bit,
							 bool &gen_bit) const {
   const unsigned topndx = preSaveFrames.size() - 1;
  
   assert(howFarBack <= topndx);
   const unsigned ndx = topndx - howFarBack;

   preSaveFrames[ndx].getFnForReg(r, thru_bit, gen_bit); // writes to thru_bit, gen_bit
}

pdstring sparc_bitwise_dataflow_fn_ll::sprintf(const pdstring &starts_banner,
					     const pdstring &stops_banner,
					     bool starts_banner_first) const {
   pdstring result;

   if (numPreSaveFramesManufactured > 0) {
      result += pdstring("[") + num2string(numPreSaveFramesManufactured)
               + pdstring(" pre-save frames manufactured]\n");
      assert(preSaveFrames.size() == 1);
   }
   
   assert(preSaveFrames.size() > 0);
   unsigned lcv=preSaveFrames.size()-1;
   unsigned check_count = 0; // just for debugging
   do {
//        const bool is_top = (lcv == preSaveFrames.size() - 1);
//        if (is_top)
//           result += "top";
//        else
//           result += pdstring(lcv).c_str();
//        result += ": \n";

      const unsigned howFarBack = preSaveFrames.size() - 1 - lcv;

      sparc_reg_set starts = *(preSaveFrames[lcv].getStarts());
      sparc_reg_set stops  = *(preSaveFrames[lcv].getStops());

      if (howFarBack > 0) {
         // Only the top frame defines %g, %o, %fp, and misc regs
         starts -= sparc_reg_set::allGs;
         starts -= sparc_reg_set::allOs;
         starts -= sparc_reg_set::allFPs;
         starts -= sparc_reg_set::allMiscRegs;

         stops -= sparc_reg_set::allGs;
         stops -= sparc_reg_set::allOs;
         stops -= sparc_reg_set::allFPs;
         stops -= sparc_reg_set::allMiscRegs;
      }

      result += starts_banner_first ? starts_banner : stops_banner;
      result += ":";
      result += (starts_banner_first ? starts : stops).disassx();
      result += "\n";

      result += starts_banner_first ? stops_banner : starts_banner;
      result += ":";
      result += (starts_banner_first ? stops : starts).disassx();
      result += "\n";

      check_count++;
   } while (lcv-- > 0);
   assert(check_count == preSaveFrames.size());

   return result;
}

/* -------------------------------------------------------------------------------- */

sparc_bitwise_dataflow_fn::
sparc_bitwise_dataflow_fn(bool, bool, bool, bool) :
   data(sparc_bitwise_dataflow_fn_ll(sparc_bitwise_dataflow_fn_ll::Dummy())) {
}

sparc_bitwise_dataflow_fn::sparc_bitwise_dataflow_fn(XDR *xdr) :
   data(refCounterK<sparc_bitwise_dataflow_fn_ll>::MANUAL_CONSTRUCT()) {

   // The above ctor for "data" simply *allocated*, but did not construct, a
   // reference counted sparc_bitwise_dataflow_fn_ll structure.  We must now
   // manually initialize said structure.

   sparc_bitwise_dataflow_fn_ll *f = data.getManualConstructObject();
   new((void*)f)sparc_bitwise_dataflow_fn_ll(xdr);

   // Note that by constructing in-place, we have avoided a spurious copy-ctor and
   // dtor call for sparc_bitwise_dataflow_fn_ll.  Don't believe me?  Try replacing
   // the above with the following code, which will run slower:
   // sparc_bitwise_dataflow_fn::
   //    sparc_bitwise_dataflow_fn(XDR *xdr) :
   // data(sparc_bitwise_dataflow_fn_ll(xdr)) {
   // }
}

bool sparc_bitwise_dataflow_fn::send(XDR *xdr) const {
   return data.getData().send(xdr);
}

pdstring sparc_bitwise_dataflow_fn::
sprintf(const pdstring &starts_banner,
        const pdstring &stops_banner,
        bool starts_banner_first) const {
   return data.getData().sprintf(starts_banner, stops_banner,
                                 starts_banner_first);
}
