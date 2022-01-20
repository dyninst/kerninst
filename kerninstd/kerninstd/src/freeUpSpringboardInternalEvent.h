// freeUpSpringboardInternalEvent.h

#ifndef _FREE_UP_SPRINGBOARD_INTERNAL_EVENT_H_
#define _FREE_UP_SPRINGBOARD_INTERNAL_EVENT_H_

#include "internalEvent.h"
#include "SpringBoardHeap.h"
#include "util/h/mrtime.h"

class freeUpSpringboardInternalEvent : public internalEvent {
 private:
   SpringBoardHeap &theSpringBoardHeap;
   kernelDriver &theKernelDriver;
   
   SpringBoardHeap::item theSpringBoardHeapItem;
   pdvector<uint32_t> origSBContents; // original springboard contents to restore to
   
 public:
   freeUpSpringboardInternalEvent(const mrtime_t when,
                                  SpringBoardHeap &itheSpringBoardHeap,
                                  kernelDriver &ikernelDriver,
                                  bool iverboseTimings,
                                  const SpringBoardHeap::item &iItem,
                                  const pdvector<uint32_t> &iOrigSbContents) :
      internalEvent(when, iverboseTimings),
      theSpringBoardHeap(itheSpringBoardHeap),
      theKernelDriver(ikernelDriver),
      theSpringBoardHeapItem(iItem),
      origSBContents(iOrigSbContents)
   {
   }
   freeUpSpringboardInternalEvent(const freeUpSpringboardInternalEvent &src);
   freeUpSpringboardInternalEvent& operator=(const freeUpSpringboardInternalEvent &);
   
  ~freeUpSpringboardInternalEvent() {}

   void doNow(bool verboseTimings) const {
      const mrtime_t startTime = verboseTimings ? getmrtime() : 0;

      const kptr_t sbItemAddr = theSpringBoardHeap.item2addr(theSpringBoardHeapItem);
      
      pdvector<uint32_t>::const_iterator insn_iter = origSBContents.begin();
      pdvector<uint32_t>::const_iterator insn_finish = origSBContents.end();
      unsigned offset = 0;
      for (; insn_iter != insn_finish; ++insn_iter) {
         const kptr_t addr = sbItemAddr + offset;
         theKernelDriver.undoPoke1WordPop(addr,
                                          theKernelDriver.peek1Word(addr),
                                             // for asserting, so this is
                                             // rather wasteful.  It would be better
                                             // to re-synthesize the springboard
                                             // contents, and use that for asserting,
                                             // with the expense.
                                          *insn_iter);
         offset += 4;
      }

      assert(offset == origSBContents.size() * sizeof(uint32_t));

      extern void flush_a_bunch(kptr_t, unsigned); // start addr, nbytes
      flush_a_bunch(sbItemAddr, offset);

      // STILL a slight bug -- we should wait a little while before returning
      // the springboard item to the springboard heap!  No, I think it's OK to
      // do these actions simultaneously.
      theSpringBoardHeap.free(theSpringBoardHeapItem);
      
      if (verboseTimings) {
         const mrtime_t totalTime = getmrtime()-startTime;
         cout << "freeUpSpringboardInternalEvent took "
              << totalTime/1000 << " usecs" << endl;
      }
   }
};

#endif
