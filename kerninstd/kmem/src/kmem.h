#ifndef _KMEM_H_
#define _KMEM_H_

// Note about kmem: it needs to be able to read -- in theory -- any kernel
// address.  In particular, it cannot be restricted to a single function
// (though it can be optimized toward the common case of reading from
// within one), because (for example) jump tables can reside outside of the 
// function's body.

#include <inttypes.h> // uint32_t
#include "util/h/kdrvtypes.h"
#include "fnCode.h"
#include "funkshun.h"

class kmem {
 public:
   typedef fnCode fnCode_t;

 protected:
   const fnCode_t &theCode;

   virtual instr_t* getinsn_general(kptr_t addr) const = 0;

   // prevent copying:
   kmem();
   kmem(const kmem &imem);
   kmem& operator=(const kmem &imem);
   
 public:
   // This one is appropriate only when "theFunction" has been 
   // completely initialized (remember that during parsing, 
   // theFunction has an uninitialized insnVec)
   kmem(const function_t &ifn);

   // This ctor is useful when the above ctor can't be used:
   kmem(const fnCode_t &iCode) : theCode(iCode) {}

   virtual ~kmem() {}

   /* These are required */

   // Reminder: in the general case, kmem's getinsn() needs to be able to read
   // any kernel address.  In the common case however, it can & will be tuned
   // toward reading quickly from within an insnVec passed to its ctor.
   instr_t* getinsn(kptr_t addr) const;
 
   // Get the doubleword located at addr. Useful for getting jmp table entries.
   virtual uint32_t getdbwd(kptr_t /*addr*/) const {
      // architectures that actually use this need to define it
      return 0;
   }

   virtual unsigned numInsnBytes(kptr_t /*addr*/) const = 0;
   virtual unsigned numInsns(kptr_t saddr, kptr_t eaddr) const = 0;
};

#endif /* _KMEM_H_ */
