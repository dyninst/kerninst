// nucleusDetector.h

#ifndef _NUCLEUS_DETECTOR_H_
#define _NUCLEUS_DETECTOR_H_

#include "util/h/kdrvtypes.h"

class inNucleusDetector {
 private:
    kptr_t belowLoBound;
    kptr_t belowHiBound;
    kptr_t nucleusLoBound;
    kptr_t nucleusHiBound;
    bool initializedYet;
   
 public:
   inNucleusDetector() {
      initializedYet = false;
   }
   
   void setBounds(kptr_t iBelowLoBound, kptr_t iBelowHiBound,
		  kptr_t iNucleusLoBound, kptr_t iNucleusHiBound) {
       assert(!initializedYet);

       belowLoBound = iBelowLoBound;
       belowHiBound = iBelowHiBound;
       nucleusLoBound = iNucleusLoBound;
       nucleusHiBound = iNucleusHiBound;
       initializedYet = true;
   }

   std::pair<kptr_t, kptr_t> queryNucleusBounds() const {
      assert(initializedYet);
      return std::make_pair(nucleusLoBound, nucleusHiBound);
   }
   std::pair<kptr_t, kptr_t> queryBelowBounds() const {
      assert(initializedYet);
      return std::make_pair(belowLoBound, belowHiBound);
   }
   bool isInNucleus(kptr_t addr) const {
      assert(initializedYet);
      return (addr >= nucleusLoBound && addr < nucleusHiBound);
   }
   bool isBelowNucleus(kptr_t addr) const {
       assert(initializedYet);
       return (addr >= belowLoBound && addr < belowHiBound);
   }
};

#endif
