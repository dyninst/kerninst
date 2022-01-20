// simplePath.h
// A cycle-free path of basic block id's.
// By encapsulating this in a class, instead of using a pdvector<unsigned>,
// we can assert, upon appending to the path, that there are no cycles.

// IMPLEMENTATION NOTES:
// -- The initial implementation used pdvector<unsigned> for "thePath".  Its performance
//    for operator+=() was surprisingly good, due to vector's policy of pre-allocating
//    some extra extries when resizing.
// -- I've tried using rope<unsigned> for "thePath", but its performance seems
//    to be *worse* than a simple pdvector<unsigned> for operator+().  Go figure.
//    It's all about the high constant overhead, I think.
// -- Next I tried using basic_string<unsigned>.  It was better than the previous
//    two approaches, because the copy-ctor for "thePath", and the dtor too, were
//    made fast (due to reference counting).  And operator+=() wasn't *too* bad, though
//    it clearly could have done better had we pre-allocated some extra space the
//    way pdvector<> does.
// -- So now we have refVector<unsigned>, which we use.  It tries to achieve both
//    reference counting and pre-allocation.  On the other hand, I think that the
//    real speedup came from new constructors for pdvector<> and refVector<>, which
//    have the semantics of operator+().  I think that pre-allocation is, thanks
//    to the operator+()-style constructor, not being used, nor is it missed.

#ifndef _SIMPLE_PATH_H_
#define _SIMPLE_PATH_H_

#include "util/h/refVector.h"

class simplePath {
 private:
//   pdvector<unsigned> thePath;
   refVector<unsigned> thePath;
      // yes, we do want a reference-counted vector, even though the operator+()-style
      // ctor is normally used, because occasionally a copy-ctor is used (and not
      // frivilously) while parsing.

   typedef refVector<unsigned>::const_iterator const_iterator;

   // private to ensure it's not used:
   simplePath &operator=(const simplePath &src);
   
 public:
   simplePath() {}
   simplePath(const simplePath &src) : thePath(src.thePath) { // fast
   }

   simplePath(const simplePath &src, unsigned elem) : // the "operator+()" ctor
         thePath(src.thePath, elem) {
      // initialized as: this = src; this += elem;
      // obviates the need for operator+() and operator+=()
      
      // after the fact, assert no cycles:
      const_iterator iter = src.thePath.begin();
      const_iterator finish = src.thePath.end();
      for (; iter != finish; ++iter)
         assert(*iter != elem);
   }

  ~simplePath() {} // fast

   unsigned size() const { return thePath.size(); } // fast
   unsigned operator[](unsigned ndx) const { // fast
      return thePath[ndx];
   }
};

// See Stroustrup, D & E, sec 3.11.4.2:
class simplePath_counter {
 private:
   static int count;
  
 public:
   simplePath_counter() {
      if (count++ == 0) {
	 refCounterK<pdvector<unsigned> >::initialize_static_stuff();
      }
   }
  ~simplePath_counter() {
      if (--count == 0)
	 refCounterK<pdvector<unsigned> >::free_static_stuff();
   }
};
static simplePath_counter simplepath_ctr;

#endif
