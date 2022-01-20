// ari.C

#include "ari.h"
#include "util/h/kdrvtypes.h"
#include <sys/cmn_err.h> // cmn_err()
#include <sys/modctl.h>
#include <sys/kmem.h> // kmem_alloc()
extern "C" int copyout(caddr_t, caddr_t, size_t);
extern "C" int copyin(const void*, void*, size_t);

#include <sys/debug.h> // ASSERT
#include "vm/hat.h"
#include "sys/memlist.h"

#include <sys/ddi.h>
#include <sys/sunddi.h>

extern uint32_t kerninst_symtab_do(int, unsigned char *);

uint32_t kerninst_symtab_size() {
   return kerninst_symtab_do(0, // false
                               NULL);
}

uint32_t kerninst_symtab_get(unsigned char *userbuffer,
			     uint32_t buffer_size) {
   uint32_t size = kerninst_symtab_size();
   
   if (size > buffer_size) {
      cmn_err(CE_CONT, "note: kerninst_symtab_get passed too small a buffer so doing nothing - really\n");
      return size;
   }

   void *kernelbuffer = kmem_alloc(buffer_size, KM_SLEEP);
   if( kernelbuffer == NULL ) cmn_err(CE_CONT, "could not allocate kernel buffer\n");
   uint32_t result = kerninst_symtab_do(1, // true
					(unsigned char*)kernelbuffer);
   copyout((char*)kernelbuffer, /* src in driver */
           (char*)userbuffer, /* dest in user */
           buffer_size);
   
   kmem_free(kernelbuffer, buffer_size);

//   cmn_err(CE_CONT, "kerninst_symtab_get successful; returning %d\n", buffer_size);
   return result;
}

// NOTE: The following is from seg_kmem.c in sun4u:
extern "C" u_int sfmmu_getprot(struct hat *, struct as *, caddr_t);

// static int
// ari_segkmem_getprot(register struct seg *seg, register caddr_t addr,
//                     register u_int len, register u_int *protv) {
//    cmn_err(CE_CONT, "welcome to ari_segkmem_getprot...about to\n");
   
//    u_int pgno1 = seg_page(seg, addr + len);

//    cmn_err(CE_CONT, "welcome to ari_segkmem_getprot...did for pgno1\n");

//    u_int pgno2 = seg_page(seg, addr);

//    cmn_err(CE_CONT, "welcome to ari_segkmem_getprot...did for pgno2\n");

//    u_int pgno = pgno1 - pgno2 + 1;

//    cmn_err(CE_CONT, "ari_segkmem_getprot: pgno=%d\n", pgno);

//    delay(drv_usectohz(1000000)); // 1 second

//    cmn_err(CE_CONT, "ari_segkmem_getprot: seg is at 0x%x\n", (unsigned)seg);
//    cmn_err(CE_CONT, "ari_segkmem_getprot: seg->s_as field is at 0x%x\n", (unsigned)(&seg->s_as));
//    cmn_err(CE_CONT, "ari_segkmem_getprot: seg->s_data field is at 0x%x\n", (unsigned)(&seg->s_data));

//    delay(drv_usectohz(1000000)); // 1 second

//    cmn_err(CE_CONT, "ari_segkmem_getprot: value of s_as is 0x%x\n", (unsigned)(seg->s_as));

//    register int i;

//    ASSERT(seg->s_as == &kas);

//    delay(drv_usectohz(1000000)); // 1 second

//    cmn_err(CE_CONT, "ari_segkmem_getprot: value of s_data field is 0x%x\n", (unsigned)(seg->s_data));

// //   if (seg->s_data == NULL)
// //      return -1;

//    for (i = 0; i < pgno; i++, addr += PAGESIZE) {
//       /*
//        * XXX Eventually this should become hat_getprot
//        */
//       protv[i] = sfmmu_getprot(kas.a_hat, seg->s_as, addr);
//    }

//    cmn_err(CE_CONT, "ari_segkmem_getprot succeeded!\n");

//    return (0);
// }

void *ari_virtual_to_physical_addr(void *vaddr) {
//   cmn_err(CE_CONT, "welcome to virtual-2-physical-addr\n");
   kptr_t physpagenum = hat_getkpfnum((caddr_t)vaddr);
   kptr_t offset = (kptr_t)vaddr % MMU_PAGESIZE;

   void *result = (void*)((physpagenum << MMU_PAGESHIFT) + offset);

//   cmn_err(CE_CONT, "stats: vaddr=%x physpagenum=%u offset=%u physaddr1=%u\n",
//           (unsigned)vaddr, physpagenum, offset, (unsigned)result);
   
   return result;
}

static void kerninst_fast_custom_memcpy(uint32_t *dest, const uint32_t *src,
                                        unsigned len) {
   uint32_t *dest_end = dest + (len/4); // ptr arith
   while (dest < dest_end)
      *dest++ = *src++;
}

void kerninst_pokeblock_nosafetychex(const unsigned char *srcAddrInUser,
                                     unsigned char *destAddrInKernel,
                                     unsigned len) {
   // we still need to copyin() the source buffer.  Then we write to destAddrInKernel.
   // In theory, the two steps can be combined into one.  But then, in theory, communism
   // works.
   while (len) {
      uint32_t stack_buffer[250];
      ASSERT(sizeof(stack_buffer) == 1000);
      
      unsigned numbytes_thistime = (len < 1000) ? len : 1000;
      ASSERT(numbytes_thistime < len);

      int retval = copyin((void*)srcAddrInUser, (void*)stack_buffer, numbytes_thistime);
      if (retval == -1) {
         cmn_err(CE_CONT, "kerninst_pokeblock_nosafetychex FAILED (copyin returned -1)\n");
         return;
      }
      
      kerninst_fast_custom_memcpy((uint32_t*)destAddrInKernel,
                                  (const uint32_t*)stack_buffer,
                                  len);

      srcAddrInUser += numbytes_thistime;
      destAddrInKernel += numbytes_thistime;
      len -= numbytes_thistime;
   }
}
