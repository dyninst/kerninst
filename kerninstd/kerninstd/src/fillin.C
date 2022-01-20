// fillin.C

#include "fillin.h"
#include <iostream.h>
#include <limits.h>
#include "util/h/minmax.h"

//  void fillin_nonkernel(pdvector<sparc_instr> &vec,
//  		      unsigned long symAddr,
//  		      unsigned long nextSymAddr, // could be ULONG_MAX
//  		      void *mmapptr, unsigned long filelen,
//  		      unsigned long whereTextBeginsInFile, // text_sec_hdr->sh_offset
//  		      unsigned long addrWhereTextBegins, // text_sec_hdr->sh_addr
//  		      unsigned long textNumBytes
//  		      ) {
//     const unsigned long offset_withinText = symAddr - addrWhereTextBegins;
//     const unsigned long offset_withinFile = whereTextBeginsInFile + offset_withinText;

//     unsigned *insnPtr = (unsigned*)((char*)mmapptr + offset_withinFile);

//     if (nextSymAddr == ULONG_MAX) {
//        cout << "fillin_nonkernel: found nextSymAddr of ULONG_MAX with" << endl;
//        cout << "symAddr=" << addr2hex(symAddr) << " and filelen=" << (void*)filelen << endl;
//        nextSymAddr = addrWhereTextBegins + textNumBytes;
//        cout << "setting nextSymAddr to " << addr2hex(nextSymAddr)
//             << "; hope it's not too big" << endl;
//     }

//     const unsigned numbytes = nextSymAddr - symAddr;
//     assert(numbytes % 4 == 0);

//     // assert within range of mmapptr:
//     assert(offset_withinFile < filelen);
//     const unsigned nextFn_offset_withinFile = whereTextBeginsInFile + (nextSymAddr - addrWhereTextBegins);
//     assert(nextFn_offset_withinFile < filelen);

//     const unsigned numinsns = numbytes / 4;

//     vec.resize(numinsns);
//     for (unsigned lcv=0; lcv < numinsns; lcv++) {
//        const unsigned raw_insn = insnPtr[lcv];
//        vec[lcv] = sparc_instr(raw_insn);
//     }
//  }

void fillin_kernel(insnVec_t *theInsnVec,
                   kptr_t symAddr,
		   kptr_t nextSymAddr, // could be (kptr_t)-1
                   const kernelDriver &theKernelDriver) {
   // fill in vec by kvm_read'ing from "symAddr" thru "nextSymAddr-1"
   // The only concern is when nextSymAddr is ULONG_MAX (last symbol).  In that case,
   // we want to read until the end of the text segment.  The last address in the
   // text segment is (addrWhereTextBegins + textNumBytes - 1)

   if (nextSymAddr == (kptr_t)-1) {
      nextSymAddr = symAddr + 1000; // Disgustingly arbitrary!

//       cout << "end of kernel text not yet implemented!" << endl;
//       assert(false);
   }

   assert(nextSymAddr > symAddr);
   
#ifndef i386_unknown_linux2_4
   // for purposes of reading bytes, round numBytesToRead **down** to nearest 4 bytes.
   nextSymAddr &= ~0x3;
   assert(nextSymAddr % 4 == 0);
#endif

   unsigned numBytesToRead = nextSymAddr - symAddr;

   ipmin(numBytesToRead, 100000U);
      // when symbol addrs jumped from 0x10000000 to
      // 0x500000000 we were running out of virtual memory
      // in this routine!
      // (but we can't make it too low; it used to be limited to 13k, and then all
      // of the sudden some huge routines (afs_GetDCache) would not parse)!
#ifndef i386_unknown_linux2_4
   assert(numBytesToRead % 4 == 0);
#endif

   /* read kernel memory to get function code, store in buffer */
   void *databuffer = (void*) malloc(numBytesToRead);
   assert(databuffer != NULL);
   unsigned nbytesActuallyRead = 
       theKernelDriver.peek_block(databuffer, symAddr, numBytesToRead,
				  true); // Try to reread upon a fault
   assert(nbytesActuallyRead > 0);

   /* now, we initialize theInsnVec using overwriteEnMasse */
   theInsnVec->overwriteEnMasse(databuffer, nbytesActuallyRead);
   free(databuffer);
}
