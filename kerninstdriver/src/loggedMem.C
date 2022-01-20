// loggedMem.C

#include "util/h/kdrvtypes.h"
#include "loggedMem.h"
#include <sys/cmn_err.h>

// These must always go last:
#include <sys/ddi.h>
#include <sys/sunddi.h>

// ----------------------------------------------------------------

oneWrite::oneWrite(kptr_t iaddr, uint32_t iorigval, bool imaybeInNucleus) {
   addr = iaddr;
   if (addr % 4 != 0)
      cmn_err(CE_PANIC, "oneWrite ctor: addr %lx is not aligned!\n", addr);
   
   origval = iorigval;
   maybeInNucleus = imaybeInNucleus;
}

void oneWrite::undo_nucleus(uint32_t oldval_to_set_to) const {
   if (oldval_to_set_to != origval) {
      cmn_err(CE_CONT, "undo_nucleus mismatch: addr=%lx; supplied oldval=%x; my oldval=%x\n", addr, oldval_to_set_to, origval);
   }

   undo_nucleus();
}

void oneWrite::undo_nucleus() const {
   if (0 != write1AlignedWordToNucleus(addr, origval)) {
      cmn_err(CE_CONT, "oneWrite::undo_nucleus(): write1AlignedWordToNucleus addr=%lx failed\n", addr);
      return;
   }

//     cmn_err(CE_CONT, "oneWrite undo_nucleus: wrote value %x to loc %lx\n",
//             origval, addr);
}

void oneWrite::undo_nonnucleus(uint32_t oldval_to_set_to) const {
   if (oldval_to_set_to != origval) {
      cmn_err(CE_CONT, "undo_nonnucleus mismatch: addr=%lx; supplied oldval=%x; my oldval=%x\n", addr, oldval_to_set_to, origval);
   }

   undo_nonnucleus();
}

void oneWrite::undo_nonnucleus() const {
   if (0 != write1AlignedWordToNonNucleus(addr, origval)) {
      cmn_err(CE_CONT, "oneWrite::undo_nonnucleus(): write1AlignedWordToNonNucleus addr=%lx failed\n", addr);
      return;
   }

//    cmn_err(CE_CONT, "oneWrite undo_nonnucleus: wrote value %x to loc %lx\n",
//            origval, addr);
}


// ----------------------------------------------------------------

static unsigned kptrHash(const kptr_t &iaddr) {
   kptr_t addr = iaddr >> 2; // assume divisible by 4

   unsigned result = 0;
   while (addr) { // siphon off 3 bits at a time
      result = (result << 1) + result + (addr & 0x8);
      addr >>= 3;
   }

   return result;
}

loggedWrites::loggedWrites() : pri0(kptrHash), pri1(kptrHash) {
   /*cmn_err(CE_CONT, "welcome to loggedWrites ctor!\n");*/
}

loggedWrites::~loggedWrites() {
   if (pri0.size() > 0)
      cmn_err(CE_PANIC, "loggedWrites dtor: pri0 size is %d, should be zero\n",
              pri0.size());
   
   if (pri1.size() > 0)
      cmn_err(CE_PANIC, "loggedWrites dtor: pri1 size is %d, should be zero\n",
              pri1.size());
}

int loggedWrites::performUndoableAlignedWrite(kptr_t addr, 
					      bool maybeNucleus,
                                              uint32_t val, uint32_t origval,
                                              unsigned howSoon) {
   // If there is already an entry in pri0 or pri1 for this addr, DONT'T re-log it
   // (on the assumption that the existing entry, being older, must therefore
   // be better for undoing purposes), just do the write.

   // XXX TODO -- if exists, should we promote depending on the value of howSoon?

   const bool exists = pri0.defines(addr) || pri1.defines(addr);

   if (!exists) {
      // Log the original value, so it can be undone
      switch (howSoon) {
         case 0:
            pri0.set(addr, oneWrite(addr, origval, maybeNucleus));
            break;
         case 1:
            pri1.set(addr, oneWrite(addr, origval, maybeNucleus));
            break;
         default:
            cmn_err(CE_PANIC, "warning: howSoon not 0 or 1 in loggedWrites::performUndoableAlignedWrite");
            break;
      }
   }
   
   // Now, perform the actual write
   int result = maybeNucleus ? write1AlignedWordToNucleus(addr, val) :
                               write1AlignedWordToNonNucleus(addr, val);

   if (result == 0) {
      // write happened ok
      if (exists)
         // partial success -- write happened, but log didn't (on purpose)
         return 1;
      else
         // total success
         return 0;
   }
   else
      return -1;
}

int loggedWrites::undoWriteWordCommon(kptr_t addr,
                                      uint32_t oldval_to_set_to,
                                      bool inNucleus) {
   // search for the entry in pri0
   if (pri0.defines(addr)) {
      pri0.get_and_remove(addr).undo(inNucleus, oldval_to_set_to);
      return 0;
   }
   
   // search for the entry in pri1
   if (pri1.defines(addr)) {
      pri1.get_and_remove(addr).undo(inNucleus, oldval_to_set_to);
      return 0;
   }
   
   cmn_err(CE_CONT, "loggedWrites::undoWriteWordCommon(): could not find an entry with addr %lx\n", addr);
   
   return -1;
}

int loggedWrites::undoWriteWordNucleus(kptr_t addr,
                                       uint32_t oldval_to_set_to) {
   return undoWriteWordCommon(addr, oldval_to_set_to,
                              true); // true --> nucleus
}

int loggedWrites::undoWriteWordNonNucleus(kptr_t addr,
                                          uint32_t oldval_to_set_to) {
   return undoWriteWordCommon(addr, oldval_to_set_to,
                              false); // false --> nucleus
}

void loggedWrites::changeUndoWriteWordHowSoon(kptr_t addr,
                                              unsigned newHowSoon) {
   ASSERT(newHowSoon == 0 || newHowSoon == 1);
   
   if (pri0.defines(addr)) {
      if (newHowSoon == 0)
         return; // do nothing
      
      pri1.set(addr, pri0.get_and_remove(addr));
      return;
   }

   // Not found in pri0.  Retry with pri1.
   if (pri1.defines(addr)) {
      if (newHowSoon == 1)
         return; // do nothing

      pri0.set(addr, pri1.get_and_remove(addr));
      return;
   }
   
   // Not found anywhere
   cmn_err(CE_CONT, "kerninst: could not change howSoon since addr now found\n");
}

void loggedWrites::undo() {
   // Undo everything!

/*   cmn_err(CE_CONT, "welcome to loggedWrites::undo!\n"); */

   // New feature: if no pri0's then don't cmn_err() and don't delay when done
   if (pri0.size() > 0) {
      cmn_err(CE_CONT, "loggedWrites::undo(): undoing %d pri0's now\n",
              pri0.size());
      
      dicttype::const_iterator iter = pri0.begin();
      dicttype::const_iterator finish = pri0.end();
      for (; iter != finish; ++iter) {
         const oneWrite &info = iter.currval();
         info.undo();
      }

      pri0.clear();
      delay(drv_usectohz(1000000)); // million microsecs --> 1 sec
   }

   // New feature: if no pri1's then don't cmn_err()
   if (pri1.size() > 0) {
      cmn_err(CE_CONT, "loggedWrites::undo(): undoing %d pri1's now\n",
              pri1.size());

      dicttype::const_iterator iter = pri1.begin();
      dicttype::const_iterator finish = pri1.end();
      for (; iter != finish; ++iter) {
         const oneWrite &info = iter.currval();
         info.undo();
      }

      pri1.clear();
   }
}

// ------------------------------------------------------------

extern "C" void *initialize_logged_writes();
void *initialize_logged_writes() {
   loggedWrites *theLoggedWrites =
      (loggedWrites*)kmem_alloc(sizeof(loggedWrites), KM_SLEEP);

   (void)new((void*)theLoggedWrites)loggedWrites; // no params to this ctor for now.

   return theLoggedWrites;
}

extern "C" void destroy_logged_writes(void *);
void destroy_logged_writes(void *ptr) {
   loggedWrites &theLoggedWrites = *(loggedWrites*)ptr;

   theLoggedWrites.undo();
   
   theLoggedWrites.~loggedWrites();

   kmem_free(ptr, sizeof(loggedWrites));
}
