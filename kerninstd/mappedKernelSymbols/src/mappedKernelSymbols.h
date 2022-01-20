// mappedKernelSymbols.h
// Ariel Tamches

// Manages a set of presently-mapped kernel symbols, reducing drudgery for others.

#ifndef _MAPPED_KERNEL_SYMBOLS_H_
#define _MAPPED_KERNEL_SYMBOLS_H_

#include "util/h/Dictionary.h"

#include "common/h/String.h"

#include "kernelDriver.h"

class oneMappedKernelPage {
 private:
   kptr_t pageStartAddrInKernel;
   void *pageStartAddrInHere;

   unsigned refCount;

 public:
   oneMappedKernelPage(kptr_t ipageStartAddrInKernel, 
		       void *ipageStartAddrInHere,
		       unsigned irefCount) {
      pageStartAddrInKernel = ipageStartAddrInKernel;
      pageStartAddrInHere = ipageStartAddrInHere;
      refCount = irefCount;
   }

   void reference() { refCount++; }
   bool dereference() { return --refCount == 0; }

   kptr_t getPageStartAddrInKernel() const { return pageStartAddrInKernel; }
   void *getPageStartAddrInHere() const { return pageStartAddrInHere; }
};

class oneMappedKernelSymbol {
 private:
   oneMappedKernelPage &theUnderlyingPage;
   kptr_t addrInKernel;
   void *mappedAddrInHere;
   unsigned refCount;

 public:
   oneMappedKernelSymbol(kptr_t iaddrInKernel, void *iMappedAddrInHere,
                         oneMappedKernelPage &iUnderlyingPage);
                         
   kptr_t getAddrInKernel() const { return addrInKernel; }
   void *getMappedAddrInHere() const { return mappedAddrInHere; }

   const oneMappedKernelPage &getUnderlyingMappedPage() const {
      return theUnderlyingPage;
   }

   void *map();
      // bump reference count (transparent to outside world) and return
      // the mapped address of the object.

   pair<bool, bool> dereference();
      // result.first: true iff the symbol's ref count is now zero.
      // result.second: true iff the undrelying page's ref count is now zero.
};

class mappedKernelSymbols {
 private:
   dictionary_hash<pdstring, oneMappedKernelSymbol *> mappedSymbolsReadOnly;
      // val type has to be ptr since oneMappedKernelSymbol can't have an operator=(),
      // due to storing a reference

   dictionary_hash<kptr_t, oneMappedKernelPage> mappedKernelPages;
      // from kernel virtual address (page start) to info about the page

   kernelDriver &theKernelDriver;

   mappedKernelSymbols(const mappedKernelSymbols &src);
   mappedKernelSymbols &operator=(const mappedKernelSymbols &src);

   bool ensureKernelPageIsMapped(kptr_t kernelStartOfPage, 
				 unsigned pageNumBytes);
   
 public:
   mappedKernelSymbols(kernelDriver &theKernelDriver);

   void *map_for_readonly(const pdstring &symName, unsigned nbytes);
   void unmap_from_readonly(const pdstring &symName, unsigned nbytes);
};

#endif
