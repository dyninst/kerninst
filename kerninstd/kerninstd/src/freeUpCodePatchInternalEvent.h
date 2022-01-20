// freeUpCodePatchInternalEvent.h

#ifndef _FREE_UP_CODE_PATCH_INTERNAL_EVENT_H_
#define _FREE_UP_CODE_PATCH_INTERNAL_EVENT_H_

#include "util/h/mrtime.h"
#include "internalEvent.h"
#include "patchHeap.h"

class freeUpCodePatchInternalEvent : public internalEvent {
 private:
   patchHeap &thePatchHeap;

   kptr_t patchAddr;
   unsigned patchNumBytes;
   
 public:
   freeUpCodePatchInternalEvent(mrtime_t when,
                                patchHeap &ipatchHeap,
                                kptr_t ipatchAddr,
                                unsigned ipatchNumBytes,
                                bool iverboseTiming) :
      internalEvent(when, iverboseTiming),
      thePatchHeap(ipatchHeap),
      patchAddr(ipatchAddr),
      patchNumBytes(ipatchNumBytes) {
   }
   virtual ~freeUpCodePatchInternalEvent() {}

   void doNow(bool verboseTiming) const {
      const mrtime_t startTime = verboseTiming ? getmrtime() : 0;
      
      thePatchHeap.free(patchAddr, patchNumBytes);

      if (verboseTiming) {
         const mrtime_t totalTime = getmrtime() - startTime;
         
         cout << "delayed free-up-code-patch took "
              << totalTime/1000 << " usecs." << endl;
      }
   }
};

#endif
