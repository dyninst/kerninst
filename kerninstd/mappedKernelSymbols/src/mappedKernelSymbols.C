// mappedKernelSymbols.C

#include "mappedKernelSymbols.h"
#include <iostream.h>
#include "util/h/hashFns.h"
#include "RTSymtab.h"

/* --------------------------- MappedKernelSymbols -------------------------- */

mappedKernelSymbols::mappedKernelSymbols(kernelDriver &ikernelDriver) :
   mappedSymbolsReadOnly(stringHash), mappedKernelPages(addrHash4),
   theKernelDriver(ikernelDriver)
{
}

bool mappedKernelSymbols::ensureKernelPageIsMapped(kptr_t kernelStartOfPage,
                                                   unsigned pageNumBytes) {
   if (mappedKernelPages.defines(kernelStartOfPage))
      return true;

   void *pageStartAddrInHere = theKernelDriver.map_for_readonly(kernelStartOfPage,
                                                                pageNumBytes,
                                                                true);
      // true --> can assert both page-aligned & page-sized

   if (pageStartAddrInHere == NULL) {
      cout << "Cannot map a page for read only" << endl;
      return false;
   }

   oneMappedKernelPage theMappedPage(kernelStartOfPage,
                                     pageStartAddrInHere,
                                     0 // initial ref count
                                     );

   mappedKernelPages.set(kernelStartOfPage, theMappedPage);
   return true;
}

void *mappedKernelSymbols::map_for_readonly(const pdstring &symName,
                                            unsigned nbytes) {
   // Algorithm:
   // 1) Is the object (by name) already being mapped?  If so then bump up the
   //    ref count on the underlying mapped page, and RETURN the mapped address
   //    of this object.
   // 2) Look up the symbol's kernel virtual address.
   // 3) Translate that kernel virtual address into a kernel page & page offset.
   //    Is that kernel page (by address) already being mapped?  If so, then
   //    bump up the refCount for that page, and return (mapped page addr) + offset.
   //    Oh, and make a note of the symbol, its parent page, and offset.
   // 4) If that kernel page is not already mapped, then map it and repeat step 3,
   //    which will now succeed.

   // Is the object already being mapped?
   oneMappedKernelSymbol *sym;
   if (mappedSymbolsReadOnly.find(symName, sym))
      return sym->map();

   // Look up the symbol's kernel virtual address
   pair<bool, kptr_t> result = lookup1NameInKernel(symName);
   if (!result.first) {
      cout << "Could not map object \"" << symName << "\" for reading because it could not be found in /dev/ksyms" << endl;
      return NULL;
   }

   // Translate virtual address into start page
   const kptr_t objectAddr = result.second;
   const kptr_t kernelStartOfPage = kernelDriver::addr2Page(objectAddr);

   // For now, the entire object must fit within that page
   const unsigned pageNumBytes = kernelDriver::getPageNumBytes();
   const kptr_t kernelEndOfPage = kernelStartOfPage + pageNumBytes - 1;
   assert(objectAddr >= kernelStartOfPage);
   if (objectAddr + nbytes - 1 > kernelEndOfPage) {
      cout << "Sorry, cannot map kernel symbol \"" << symName
           << "\" because it seems to cross a page boundary" << endl;
      return NULL;
   }
   
   const unsigned offsetFromPageStart = objectAddr - kernelStartOfPage;
   
   // Is this kernel page already being mapped?
   if (!ensureKernelPageIsMapped(kernelStartOfPage, pageNumBytes)) {
      cout << "Cannot map kernel symbol \"" << symName << "\" for readonly because mmap failed" << endl;
      return NULL;
   }
   
   oneMappedKernelPage &theMappedKernelPage = mappedKernelPages.get(kernelStartOfPage);

   // mapped page's ref count is zero; sym->map() will bump it up

   void *addr_result = (void*)(
      (dptr_t)theMappedKernelPage.getPageStartAddrInHere() + offsetFromPageStart);

   sym = new oneMappedKernelSymbol(objectAddr,
                                   addr_result,
                                   theMappedKernelPage);
   assert(sym);

   mappedSymbolsReadOnly.set(symName, sym);
   
   return sym->map();
      // bumps ref counts of both sym and its underlying page
}

void mappedKernelSymbols::unmap_from_readonly(const pdstring &symName,
                                              unsigned /* nbytes */) {
   oneMappedKernelSymbol *sym = mappedSymbolsReadOnly.get(symName);

   pair<bool, bool> result = sym->dereference();
   // result.first: is symbol's ref count now 0?
   // result.second: is underlying pages's ref count now 0?

   if (result.second) {
      // underlying page's ref count is now 0
      const oneMappedKernelPage &theMappedPage = sym->getUnderlyingMappedPage();
      
      void *pageStartAddrInHere = theMappedPage.getPageStartAddrInHere();

      theKernelDriver.unmap_from_readonly(pageStartAddrInHere,
                                          theKernelDriver.getPageNumBytes());

      mappedKernelPages.undef(theMappedPage.getPageStartAddrInKernel());

      assert(result.first);
   }

   if (result.first) {
      delete sym;
      sym = NULL; // help purify find mem leaks
      mappedSymbolsReadOnly.undef(symName);
   }
}

/* --------------------------- oneMappedKernelSymbol -------------------------- */

void *oneMappedKernelSymbol::map() {
   ++refCount;
   theUnderlyingPage.reference();
   return mappedAddrInHere;
}

pair<bool, bool> oneMappedKernelSymbol::dereference() {
   pair<bool, bool> result;
   result.first = (--refCount == 0);
   result.second = theUnderlyingPage.dereference();

   return result;
}

oneMappedKernelSymbol::oneMappedKernelSymbol(kptr_t iaddr, 
					     void *iMappedAddrInHere,
                                             oneMappedKernelPage &iUnderlyingPage) :
      theUnderlyingPage(iUnderlyingPage),
      addrInKernel(iaddr),
      mappedAddrInHere(iMappedAddrInHere) {
   refCount = 0;
}

