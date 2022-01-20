// readySet.h

#ifndef _READY_SET_H_
#define _READY_SET_H_

#include "common/h/Vector.h"

class customReadySetAllocator {
 public:
   static bool *alloc(unsigned nelems);
   static void free(bool *vec, unsigned nelems);
};

class readySet {
 private:
   pdvector<bool, customReadySetAllocator> theset;
   unsigned numCurrInSet;

 public:
   readySet(unsigned setsize, bool initialValues);
  ~readySet();

   void operator+=(unsigned ndx); // add to set
   void operator-=(unsigned ndx); // remove from set
   unsigned get_and_remove_any();

   bool contains(unsigned ndx) const;

   unsigned size() const {
      return numCurrInSet;
   }

   bool empty() const {return numCurrInSet==0;}

   bool verify() const;
};

#endif
