// x86_kmem.C

//#include <unistd.h>
//#include <sys/mman.h>
//#include <inttypes.h>
//#include <sys/stat.h>
//#include <fcntl.h>
//#include <elf.h>

//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h> /* memset */
#include <assert.h>

//#include "x86Traits.h"
#include "x86_kmem.h"
#include "kernelDriver.h"

extern kernelDriver *global_kernelDriver; // sorry for this global use.

instr_t* x86_kmem::getinsn_general(kptr_t addr) const {
   char *insn = (char*) malloc(20*sizeof(char)); // allow for insn up to 20B
   uint32_t nbytes = global_kernelDriver->peek_block((void*)insn, addr, 20, false);
   if(nbytes) {      
      instr_t *result = new x86_instr(insn);
      free(insn);
      return result;
   }
   else return (instr_t*)NULL;
}

uint32_t x86_kmem::getdbwd(kptr_t addr) const {
   return global_kernelDriver->peek1Word(addr);
}

//  x86_kmemopenkern x86_kmemopenKern;

//  x86_kmem::x86_kmem(x86_kmemopenkern) {
//       memset(cachedcode, 0, sizeof(cachedcode));
//       fd = open("/dev/kmem", O_RDONLY);
//       if (0 > fd) {
//  	  throw openfailedexception();
//       }
//       cachedaddr = 0;
//       cachedlen = 0;
//  }

//  void x86_kmem::fetch(kptr_t addr, uint32_t len) {
//       assert(fd >= 0);
//       assert(len < cachesize);
//       if (addr != lseek(fd, addr, SEEK_SET))
//  	  throw fetchfailedexception(addr);
//       if (len != read(fd, cachedcode, len))
//  	  throw fetchfailedexception(addr);
//       cachedaddr = addr;
//       cachedlen = 0;
//  }

//  x86_instr x86_kmem::getinsn(kptr_t addr) {
//       unsigned char buf[32];
//       assert(fd >= 0);
//       if (cachedlen > 0
//  	 && addr >= cachedaddr
//  	 && addr < cachedaddr + cachedlen) {
//  	  /* We have this instruction cached */
//  	  /* FIXME: Unless addr is at the boundary of the cache */
//  	  unsigned char *coreaddr;
//  	  coreaddr = (unsigned char *)(&cachedcode[addr - cachedaddr]);
//  	  return x86_instr(coreaddr);
//       } else {
//  	  /* FIXME: this is bad if we read from the address space
//  	     boundary.  */
//  	  if (addr != lseek(fd, addr, SEEK_SET))
//  	       throw fetchfailedexception(addr);
//  	  if (sizeof(buf) != read(fd, buf, sizeof(buf)))
//  	       throw fetchfailedexception(addr);
//  	  return x86_instr(buf);
//       }
//  }

//  unsigned long
//  x86_kmem::getdbwd(kptr_t addr) {
//       unsigned long dbwd;
//       assert(fd >= 0);
//       if (cachedlen > 0
//  	 && addr >= cachedaddr
//  	 && addr + sizeof(dbwd) < cachedaddr + cachedlen) {
//  	  memcpy(&dbwd, &cachedcode[addr - cachedaddr], sizeof(dbwd));
//       } else {
//  	  if (addr != lseek(fd, addr, SEEK_SET))
//  	       throw fetchfailedexception(addr);
//  	  if (sizeof(dbwd) != read(fd, &dbwd, sizeof(dbwd)))
//  	       throw fetchfailedexception(addr);
//       }
//       return dbwd;
//  }

unsigned x86_kmem::numInsns(kptr_t saddr, kptr_t eaddr) const {
   unsigned num_insns = 0;
   uint32_t nbytes = eaddr - saddr;
   char *insns = (char*) malloc(nbytes*sizeof(char));
   nbytes = global_kernelDriver->peek_block((void*)insns, saddr, 
                                            nbytes, true);
   if(nbytes)
      num_insns = x86_instr::numInsnsInRange((kptr_t)insns,
					     (kptr_t)insns+nbytes);
      
   free(insns);
   return num_insns;
//       if (cachedlen > 0
//  	 && saddr >= cachedaddr
//  	 && saddr < cachedaddr + cachedlen
//  	 && eaddr >= cachedaddr
//  	 && eaddr <= cachedaddr + cachedlen) {
//  	  return numInsnsInRange((uint32_t) &cachedcode[saddr - cachedaddr],
//  				 (uint32_t) &cachedcode[eaddr - cachedaddr]);
//       } else {
//  	  unsigned nbytes = eaddr - saddr;
//  	  unsigned char buf[nbytes];
//  	  if (saddr != lseek(fd, saddr, SEEK_SET))
//  	       throw fetchfailedexception(saddr);
//  	  if (nbytes != read(fd, buf, nbytes))
//  	       throw fetchfailedexception(saddr);
//  	  return numInsnsInRange((uint32_t) &buf[0],
//  				 (uint32_t) &buf[nbytes]);
//       }
}

//  void x86_kmem::closecode() {
//     if (fd >= 0) {
//        close(fd);
//        fd = -1;
//     }
//  }
