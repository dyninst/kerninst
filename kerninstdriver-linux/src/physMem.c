// physMem.c

#include <linux/slab.h> // memcpy

#include "physMem.h"
#include "linux/kernel.h"

extern uint32_t kerninst_enable_bits;
extern uint32_t kerninst_debug_bits;

#ifdef ppc64_unknown_linux2_4
void kerninst_flush_icache_range (void *data) {
  struct kernInstFlushRange *range = (struct kernInstFlushRange *) data;
  flush_icache_range(range->addr, range->addr + range->numBytes );
}
extern int (*kerninst_on_each_cpu_ptr) (void (*func) (void *info), void *info, int nonatomic, int wait);
#endif

#ifdef i386_unknown_linux2_4
static void do_atomic_write(void *to, void *from, unsigned length)
{
   char buff[16];
   int dword1, dword2;
   unsigned write_length = 4; // if needed, we'll pad our write to make atomic

   memcpy(&buff[0], (char*)from, length);
   if(length > write_length) {
      if(length <= 8)
         write_length = 8;
      else
         write_length = 16;
   }
   if(length < write_length) { 
      memcpy(&buff[length], (char*)to+length, write_length-length);
   }

   switch(write_length) {
   case 4:
      // if aligned to 32-bit just do write (guaranteed atomic), 
      // else use cmpxchg
      if(((unsigned long)to)%4 == 0) {
         *(int*)to = *(int*)&buff[0];
      }
      else {
         // NOTE: we don't check to make sure the compare was successful,
         // since we assume we are the only one updating this memory 
         // location at this point. this means that the exchange portion
         // has a very slight possiblity of not happening. the solution is
         // to put the below assembly in a do-while, and set the while 
         // condition variable based on the cmpxchg in the assembly
         dword1 = *(int*)to;
         __asm__ __volatile__("lock ; cmpxchg %2, %0"
                              :"=m" (*(int*)to)
                              :"a" (dword1), "r" (*(int*)&buff[0])
                              );
      }
      break;
   case 8:
      // use cmpxchg8b (same NOTE as above for cmpxchg)
      dword1 = *(int*)to;
      dword2 = *((int*)to+1);
      __asm__ __volatile__("lock ; cmpxchg8b %0"
                           :"=m" (*(int64_t*)to)
                           :"a" (dword1), 
                            "d" (dword2),
                            "b" (*(int*)&buff[0]), 
                            "c" (*(int*)&buff[4])
                           );
      break;
   case 16:
      // need to use two cmpxchg8b insns, which isn't strictly an atomic 
      // 16-byte write, but it is the best we can do. Note that since we're
      // only ever writing a 5-byte jmp over an existing instruction, it
      // is safe to do two cmpxchg8b's, since the first one will do the
      // entire atomic write for the jmp, while the second one will
      // presumably just be writing nop's
      dword1 = *(int*)to;
      dword2 = *((int*)to+1);
      __asm__ __volatile__("lock ; cmpxchg8b %0"
                           :"=m" (*(int64_t*)to)
                           :"a" (*(int*)to), 
                            "d" (*(int*)((int*)to+1)),
                            "b" (*(int*)&buff[0]), 
                            "c" (*(int*)&buff[4])
                           );
      dword1 = *((int*)to+2);
      dword2 = *((int*)to+3);
      __asm__ __volatile__("lock ; cmpxchg8b %0"
                           :"=m" (*((int64_t*)to+1))
                           :"a" (dword1), 
                            "d" (dword2),
                            "b" (*(int*)&buff[8]), 
                            "c" (*(int*)&buff[12])
                           );
      break;
   default:
      printk(KERN_ERR "kerninst: do_atomic_write - impossible write_length\n");
   }
}
#endif // i386_unknown_linux2_4

int write1Insn(kptr_t addr, char *val, unsigned length)
{
   int i;
   if(kerninst_enable_bits & KERNINST_ENABLE_UNDOMEMORY) {
#ifdef i386_unknown_linux2_4
      if(length > 1) {
         do_atomic_write((void*)addr, (void*)val, length);
      }
      else // all single byte writes are atomic
         *(char*)addr = *val;
#elif defined(ppc64_unknown_linux2_4)
      struct kernInstFlushRange range ; 
      memcpy((char *)addr, val, length);
      range.addr = addr;
      range.numBytes = length;  
      kerninst_on_each_cpu_ptr(kerninst_flush_icache_range, (void *) &range, 0, 1);
#endif
   }
   else
      printk(KERN_INFO "kerninst: physMem.c//write1Insn() - just testing, so not writing to kernel memory @ 0x%08lx\n", (unsigned long)addr);

   if(kerninst_debug_bits & KERNINST_DEBUG_MEMORY) {
      printk(KERN_INFO "kerninst: physMem.c//write1Insn() - addr=0x%08lx, insn_bytes=0x",
             (unsigned long)addr);
      for(i=0; i<length; i++)
         printk("%02x", (unsigned char)val[i]);
      printk(", length=%d\n", length);
   }
   return 0;
}

int write1UndoableInsn(loggedWrites *theLoggedWrites, kptr_t addr,
                       char *val, unsigned length, unsigned howSoon,
                       char *oldval_fillin)
{
   return LW_performUndoableWrite(theLoggedWrites, addr, length, val, 
                                  oldval_fillin, howSoon);
}

int undoWrite1Insn(loggedWrites *theLoggedWrites, kptr_t addr,
                   unsigned length, char *oldval_to_set_to)
{
   return LW_undoWrite1Insn(theLoggedWrites, addr, length, oldval_to_set_to);
}

int changeUndoableInsnHowSoon(loggedWrites *theLoggedWrites, kptr_t addr,
                              unsigned howSoon)
{
   return LW_changeUndoableWriteHowSoon(theLoggedWrites, addr, howSoon);
}
