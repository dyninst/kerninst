// RTSymtab.h

#ifndef _RT_SYMTAB_H_
#define _RT_SYMTAB_H_

#include "common/h/String.h"
#include <pair.h>
#include <inttypes.h> // uint32_t
#include "util/h/kdrvtypes.h"

template<class ELFTraits> class RTSymtab {
 private:
	int theFd;

	typedef typename ELFTraits::ET_Addr ET_Addr;
	typedef typename ELFTraits::ET_Sym ET_Sym;
	typedef typename ELFTraits::ET_Elf ET_Elf;
	typedef typename ELFTraits::ET_Ehdr ET_Ehdr;
	typedef typename ELFTraits::ET_Shdr ET_Shdr;
	typedef typename ELFTraits::ET_Scn ET_Scn;
	typedef typename ELFTraits::ET_Data ET_Data;
	
	bool getSecHdrStrTab(ET_Elf *theElf, 
			     ET_Ehdr *theElfHeader,
			     ET_Scn* &secHdrStrTab,     // filled in
			     ET_Shdr* &secHdrStrTabHdr, // filled in
			     ET_Data* &secHdrStrTabData // filled in
		);
	pair<bool, ET_Addr> lookup1NameInElfSymbols(ET_Sym *symbols, 
						    unsigned num_symbols,
						    const char *symNamesPool,
						    const pdstring &symName);
	pair<bool, ET_Addr> lookup1NameInElfExecutable(ET_Elf *theElf, 
						       ET_Ehdr *theElfHeader,
						       const pdstring &symName);
	pair<bool, ET_Addr> lookup1NameInElf(ET_Elf *theElf, 
					     const pdstring &symName);
 public:
	pair<bool, ET_Addr> lookup1NameInElfFd(int fd, const pdstring &symName);
};

pair<bool, dptr_t> lookup1NameInSelf(const pdstring &symName);
   // result.first: true iff successful
   // result.second: the value (undefined if .first is false)

pair<bool, kptr_t> lookup1NameInKernel(const pdstring &symName);
   // result.first: true iff successful
   // result.second: the value (undefined if .first is false)

#endif
