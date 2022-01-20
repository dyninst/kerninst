// snippet.C

#include "snippet.h"

snippet::~snippet() {
    if (isPredicated) {
	delete pred;
	pred = NULL; // help purify find mem leaks
    }

   delete prim;
   prim = NULL; // help purify find mem leaks
}

snippet &snippet::operator=(const snippet &src) {
   if (&src == this) return *this;
   if (isPredicated) {
       delete pred;
   }
   delete prim;
      
   pl = src.pl;
   prim = src.prim->dup();
   isPredicated = src.isPredicated;
   if (src.isPredicated) {
       pred = src.pred->dup();
   }
   return *this;
}
