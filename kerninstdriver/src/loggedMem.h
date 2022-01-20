// loggedMem.h

#ifndef _LOGGED_MEM_H_
#define _LOGGED_MEM_H_

#include "util/h/kdrvtypes.h"
#include "physMem.h"
#include "common/h/Vector.h"
#include <sys/kmem.h>
#include "util/h/Dictionary.h"

//  template <class T>
//  class myallocator {
//   public:
//     static T *alloc(unsigned nelems) {
//        return (T*)kmem_alloc(nelems * sizeof(T), KM_SLEEP);
//     }
//     static void free(T *vec, unsigned nelems) {
//        kmem_free(vec, nelems * sizeof(T));
//     }
//  };

class oneWrite {
 private:
   kptr_t addr; // kernel virtual memory address
   uint32_t origval;
   bool maybeInNucleus;

   void undo_nucleus(uint32_t oldval_to_set_to) const;
   void undo_nonnucleus(uint32_t oldval_to_set_to) const;

   void undo_nucleus() const;
   void undo_nonnucleus() const;
   
 public:
   oneWrite(kptr_t iaddr, uint32_t origval, bool imaybeInNucleus);

   oneWrite(const oneWrite &src) :
      addr(src.addr), origval(src.origval), maybeInNucleus(src.maybeInNucleus) {
   }

   oneWrite& operator=(const oneWrite &src) {
      if (&src != this) {
         addr = src.addr;
         origval = src.origval;
         maybeInNucleus = src.maybeInNucleus;
      }
      return *this;
   }
   
  ~oneWrite() {
   }
   
   void undo(bool inNucleus, uint32_t oldval_to_set_to) const {
      /* When called explicitly by a user program, not when it crashes */
      ASSERT(inNucleus == maybeInNucleus);
      
      if (maybeInNucleus)
         undo_nucleus(oldval_to_set_to);
      else
         undo_nonnucleus(oldval_to_set_to);
   }
   void undo() const {
      /* When called due to a user program crash (so no params, such as expected
         currval, can be sent to this routine by the user program) */
      if (maybeInNucleus)
         undo_nucleus();
      else
         undo_nonnucleus();
   }
   
   kptr_t getAddr() const { return addr; }
};

class loggedWrites {
 private:
   //typedef pdvector<oneWrite, myallocator<oneWrite> > vectype;
   typedef dictionary_hash<kptr_t,oneWrite> dicttype;
   
   dicttype pri0; // undo these first
   dicttype pri1; // undo these second

   int undoWriteWordCommon(kptr_t addr, uint32_t oldval_to_set_to,
                           bool inNucleus);

   // private so they're not called
   loggedWrites(const loggedWrites &);
   loggedWrites &operator=(const loggedWrites &);

 public:
   loggedWrites();
  ~loggedWrites();

   // Return value:
   // 0 --> write happened OK, and we're set up to undo this write when desired.
   // 1 --> write happened OK, but don't try to undo it, because someone beat you
   //       to it (and their undo should take precedence because their origval was
   //       older, and therefore better to use, than yours).
   // -1 --> write failed
   int performUndoableAlignedWrite(kptr_t addr, bool maybeNucleus,
                                   uint32_t val, uint32_t origval,
                                   unsigned howSoon);
   
   int undoWriteWordNucleus(kptr_t addr, uint32_t oldval_to_set_to);
   int undoWriteWordNonNucleus(kptr_t addr, uint32_t oldval_to_set_to);

   void changeUndoWriteWordHowSoon(kptr_t addr, unsigned newHowSoon);

   void undo();
};

#endif
