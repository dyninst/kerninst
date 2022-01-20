// fillin.h

#ifndef _FILLIN_H_
#define _FILLIN_H_

#include "util/h/kdrvtypes.h"
#include "instr.h"
#include "kernelDriver.h"
#include "insnVec.h"

//  void fillin_nonkernel(pdvector<sparc_instr> &vec, unsigned long symAddr,
//  		      unsigned long nextSymAddr, // could be ULONG_MAX
//  		      void *mmapptr, unsigned long filelen,
//  		      unsigned long whereTextBeginsInFile,
//  		      unsigned long addrWhereTextBegins,
//  		      unsigned long textNumBytes);

void fillin_kernel(insnVec_t *vec,
                   kptr_t symAddr,
		   kptr_t nextSymAddr, // could be -1
                   const kernelDriver &);
		   
#endif
