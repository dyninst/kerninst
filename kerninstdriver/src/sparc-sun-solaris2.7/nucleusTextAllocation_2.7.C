// nucleusTextAllocation_2.7.C

#include "nucleusTextAllocation.h"
#include "util/h/kdrvtypes.h"
#include "sys/machkobj.h" // kobj_free_t
#include <sys/sysmacros.h> // roundup()

// These must always go last:
#include <sys/ddi.h>
#include <sys/sunddi.h>

// --------------------

static caddr_t *find_text_pool_ptr_lowlevel() {
   kptr_t text_pool_addr = kobj_getsymvalue((char*)"text_pool", 0);
   if (text_pool_addr == 0) {
      cmn_err(CE_WARN, "find_text_pool_ptr_lowlevel() failed to find it.");
   }
   else
      cmn_err(CE_NOTE, "find_text_pool_ptr_lowlevel found it at 0x%lx\n",
              text_pool_addr);

   return (caddr_t*)text_pool_addr;
}

static caddr_t *find_text_pool_ptr() {
   static caddr_t *text_pool_ptr_cachedvalue = 0;
   if (text_pool_ptr_cachedvalue == 0)
      text_pool_ptr_cachedvalue = find_text_pool_ptr_lowlevel();
   
   return text_pool_ptr_cachedvalue;
}

// --------------------

static kobj_free_t **find_textobjs_ptr_lowlevel() {
   kptr_t textobjs_addr = kobj_getsymvalue((char*)"textobjs", 0);
   if (textobjs_addr == 0) {
      cmn_err(CE_WARN, "find_textobjs_ptr_lowlevel() failed to find it.");
   }
   else
      cmn_err(CE_NOTE, "find_textobjs_ptr_lowlevel found it at 0x%lx\n",
              textobjs_addr);

   return (kobj_free_t **)textobjs_addr;
}

static kobj_free_t **find_textobjs_ptr() {
   static kobj_free_t **textobjs_ptr_cachedvalue = 0;
   if (textobjs_ptr_cachedvalue == 0)
      textobjs_ptr_cachedvalue = find_textobjs_ptr_lowlevel();
   
   return textobjs_ptr_cachedvalue;
}

// --------------------

kptr_t OLD_try_allocate_within_nucleus_text(unsigned nbytes) {
   // returns 0 on failure.
   
   // home-brewed version of solaris7's mach_mod_alloc(), which we would love to
   // call directly, but it insists on mapping the result (using segkmem_mapin())
   // for our convenience, which we don't want (though admittedly, in practice, we'll
   // do the same thing a bit later when initializing the contents)
   //
   // The trick is obtaining "text_pool" and "textobjs", which are normally
   // invisible to us, since they're in global scope but declared "static" in
   // machkobj.c (aaargh).  Fortunately, as we do with "mm_map_2.7.C", we can find
   // the symbol thanks to the glory that is kobj_lookup().
   //
   // text_pool is of type caddr_t
   // textobjs is of type kobj_free_t*; it's sort of the head of a linked list

   caddr_t *text_pool_ptr = find_text_pool_ptr();
   kobj_free_t **textobjs_ptr = find_textobjs_ptr();

   extern caddr_t modtext;
   kobj_free_t *cookie = *textobjs_ptr;
   size_t end = (size_t)modtext + MODTEXT;
   
   nbytes = roundup(nbytes, sizeof(int32_t));
   
   void *result = kobj_find_managed_mem(nbytes, cookie);
   if (result != 0) {
      // success in the first way
      return (kptr_t)result;
   }

   if (((size_t)*text_pool_ptr + nbytes) < end) {
      // successs in the second way
      result = *text_pool_ptr;
      *text_pool_ptr = *text_pool_ptr + nbytes;
      
      return (kptr_t)result;
   }
   
   return 0;
}

void OLD_free_from_nucleus_text(kptr_t addr, unsigned nbytes) {
   mach_mod_free((caddr_t)addr, nbytes, KOBJ_TEXT);
}

// ------------------------------------------------------------

extern "C" caddr_t mach_mod_alloc(size_t, int flags, reloc_dest_t*);

kptr_t try_allocate_within_nucleus_text(unsigned nbytes,
                                          unsigned desiredAlignment,
                                          kptr_t &mapped_addr) {
   caddr_t result = 0;
   reloc_dest_t mapped_result = 0;

   // mutex_enter(&kobj_lock);  TODO! (but the symbol is static)
   result = mach_mod_alloc(nbytes, KOBJ_TEXT, &mapped_result);
   // mutex_exit(&kobj_lock); TODO! (but the symbol is static)
      // size gets rounded up to 4 bytes
      // calls kobj_find_managed_mem().  If successful then
      // calls mod_remap(), filling in "mapped_result", and returns.
      // If unsuccessful, then tries another way (directly off of
      // "text_pool"), and if successful then calls mod_remap() and returns.
      // If both methods fail then returns NULL.

   if (result != 0) {
      // success.  Well, maybe; let's check the alignment.
      mapped_addr = (kptr_t)mapped_result;

      if (mapped_addr % desiredAlignment != 0) {
         //cmn_err(CE_CONT, "try_allocate_within_nucleus_text: mapped addr of 0x%lx doesn't satisfy alignment, so giving up.\n",
         //mapped_addr);

         free_from_nucleus_text((kptr_t)result, mapped_addr, nbytes);

         mapped_addr = 0;
         return 0;
      }
      else {
         // Success!
         return (kptr_t)result;
      }
   }
   else
      return 0;
}

extern "C" void mach_mod_free(caddr_t addr, size_t size, int flags);
extern "C" void mach_mod_epilogue(size_t, reloc_dest_t mapped_at);

void free_from_nucleus_text(kptr_t addr, kptr_t mapped_addr, 
			    unsigned nbytes) {
//cmn_err(CE_CONT, "free_from_nucleus_text: addr=0x%lx mapped_addr=0x%lx\n",
//           addr, mapped_addr);
   
   mach_mod_epilogue(nbytes, (reloc_dest_t)mapped_addr);
      // unmap from segkmem.  (nbytes gets rounded up to page size, of course)
      // essentially calls segkmem_mapout() then rmfree()

   mach_mod_free((caddr_t)addr, nbytes, KOBJ_TEXT);
      // essentially calls kobj_manage_mem().
      // size gets rounded up to 4 bytes
}

