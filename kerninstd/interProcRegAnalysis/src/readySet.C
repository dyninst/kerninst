// readySet.C

#include "util/h/HomogenousHeapUni.h"
#include "readySet.h"

static HomogenousHeapUni<bool,1> vecBoolSingles(512);
static HomogenousHeapUni<bool,2> vecBoolDoubles(512);
static HomogenousHeapUni<bool,4> vecBoolQuads(512);
static HomogenousHeapUni<bool,8> vecBoolEights(512);
static HomogenousHeapUni<bool,16> vecBoolSixteens(512);

bool* customReadySetAllocator::alloc(unsigned nitems) {
   dptr_t rv;

   if (nitems <= 1)
      rv = vecBoolSingles.alloc();
   else if (nitems <= 2)
      rv = vecBoolDoubles.alloc();
   else if (nitems <= 4)
      rv = vecBoolQuads.alloc();
   else if (nitems <= 8)
      rv = vecBoolEights.alloc();
   else if (nitems <= 16)
      rv = vecBoolSixteens.alloc();
   else
      rv = (dptr_t)malloc(sizeof(bool)*nitems);
   return (bool *)rv;
}

void customReadySetAllocator::free(bool *vec, unsigned nitems) {
   if (nitems <= 1)
      vecBoolSingles.free((dptr_t)vec);
   else if (nitems <= 2)
      vecBoolDoubles.free((dptr_t)vec);
   else if (nitems <= 4)
      vecBoolQuads.free((dptr_t)vec);
   else if (nitems <= 8)
      vecBoolEights.free((dptr_t)vec);
   else if (nitems <= 16)
      vecBoolSixteens.free((dptr_t)vec);
   else
      ::free(vec);
}

// ----------------------------------------------------------------------

readySet::readySet(unsigned setsize, bool iValues) : theset(setsize, iValues) {
   if (!iValues)
      numCurrInSet = 0;
   else
      numCurrInSet = setsize;
}

readySet::~readySet() {}

void readySet::operator+=(unsigned ndx) {
   // add to set
   if (!theset[ndx]) {
      theset[ndx] = true;
      numCurrInSet++;
   }
}

void readySet::operator-=(unsigned ndx) {
   // remove from set
   if (theset[ndx]) {
      theset[ndx] = false;
      numCurrInSet--;
   }
}

unsigned readySet::get_and_remove_any() {
   pdvector<bool, customReadySetAllocator>::const_iterator iter = theset.begin();
   pdvector<bool, customReadySetAllocator>::const_iterator finish = theset.end();
   for (; iter != finish; ++iter) {
      const unsigned ndx = iter - theset.begin();
      
      if (contains(ndx)) {
         this->operator-=(ndx);
         return ndx;
      }
   }

   assert(false);
#ifdef NDEBUG
   abort(); // placate compiler when compiling NDEBUG
#endif
}

bool readySet::contains(unsigned ndx) const {
   return theset[ndx];
}

bool readySet::verify() const {
   unsigned verifyNumCurrInSet = 0;
   for (unsigned lcv=0; lcv < theset.size(); lcv++)
      if (theset[lcv])
	 verifyNumCurrInSet++;

   return verifyNumCurrInSet == numCurrInSet;
}
