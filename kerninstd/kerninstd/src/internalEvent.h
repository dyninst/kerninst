// internalEvent.h

#ifndef _INTERNAL_EVENT_H_
#define _INTERNAL_EVENT_H_

#include "util/h/mrtime.h"

class internalEvent { // a base class
 private:
   mrtime_t whenToDo;
   bool verboseTiming;

   // private to ensure they're not called
   internalEvent(const internalEvent &);
   internalEvent &operator=(const internalEvent &);
   
 public:
   internalEvent(mrtime_t iwhenToDo, bool iverboseTiming) :
      whenToDo(iwhenToDo), verboseTiming(iverboseTiming)
   {
   }

   mrtime_t getWhenToDo() const { return whenToDo; }
   bool getVerboseTiming() const { return verboseTiming; }
   
   virtual ~internalEvent() { }

   virtual void doNow(bool verboseTiming) const = 0;
};

#endif
