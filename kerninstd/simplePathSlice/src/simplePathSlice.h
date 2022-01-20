// simplePathSlice.h
// class for keeping track of the contents of the slice along a simple path.

// DISCUSSION: The role of control depenencies in a slice.
// When we cross a bb boundary while calculating a slice, should the control
// dependency that "connects" the two bb's be included in the slice?  Yes,
// control dependencies (like the condition of a conditional branch) should be
// in the slice because ???
// The implementation is surprisingly easy: when we cross over to a new bb,
// we check the condition that connects the two.  There are several possibilities:
// 1) A stumbles onto B
//    action to take: none
// 2) A unconditional jumps, branches, or calls B
//    action to take: none
// 3) A conditionally branches to B
//    action to take: find out which condition is tested (%icc, %xcc, %fcc0-3)
//    and add that condition bit to the slicing criteria (regsToSlice) before
//    processing any of bb A.  As a result, the comparison insn which wrote to
//    that condition bit will naturally get included in the slice.  Note that
//    the conditional branch insn (which _uses_ the condition bit) won't be included
//    in the slice, but the cmp (which _wrote_ to the cond bit) will be included.
//    This is as it should be.

#ifndef _SIMPLE_PATH_SLICE_H_
#define _SIMPLE_PATH_SLICE_H_

#include <iostream.h>
#include "common/h/Vector.h"
#include <inttypes.h> // uint32_t
class simplePath;
#include "fnCode.h"

class function_t; // avoid recursive #include
class basicblock_t; // avoid recursive #include

class simplePathSlice {
 public:
   typedef fnCode fnCode_t;
   
 private:
   const function_t &fn;
   struct sliceInsn {
      unsigned bb_id;
      unsigned ndxWithinBB;
   };
   pdvector<sliceInsn> theSlice;  // first entry is the first in a _backwards_ slice.

   // For iteration:   
   mutable unsigned iterationNdx;

   simplePathSlice& operator=(const simplePathSlice&);

 public:
   simplePathSlice(const function_t &ifn,
                   const simplePath &thePath,
                   regset_t &regsToSlice,
		   bool includeControlDependencies,
                   unsigned numInsnBytesToSkipAtEnd);
      // calculate a slice (regsToSlice are the criteria) from a given path of bb's.

   simplePathSlice(const simplePathSlice &parentSlice, unsigned numInsnsAtEndToSkip,
                   bool includeControlDependency,
                   regset_t &regsToSlice);
      // calculate a slice within another slice.  This can be very convenient.
      // For example, consider a slice S on register R1.  Let's say that we ananlyze
      // the slice and determine that registers R2 and R3 contribute to R1, so we want
      // to take a backwards slice of R2 and/or R3, starting just before where they
      // contribute to R1.  It is pretty straightforward to see why the backwards slices
      // of those registers must be a subset of the slice of R1...that's why it makes
      // sense to call this a sub-slice.

   simplePathSlice(const simplePathSlice &); // yes this is needed

  ~simplePathSlice() {}

   void addInsnToSlice(unsigned bb_id, unsigned ndxWithinBB);

   void beginBackwardsIteration() const;
   bool isEndOfIteration() const;
   instr_t *getCurrIterationInsn() const;
   kptr_t getCurrIterationInsnAddr() const;
   pair<unsigned, unsigned> getCurrIterationBlockIdAndOffset() const;
      // .second: insn ndx within bb (shouldn't we change it to a byte offset w/in bb?)
   void advanceIteration() const;

   //   friend ostream &operator<<(ostream &, const simplePathSlice &); // for debug purposes
   void printx(ostream &) const;
};

#endif
