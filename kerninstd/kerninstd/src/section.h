// section.h

#ifndef _SECTION_H_
#define _SECTION_H_

#include <assert.h>
#include "common/h/Vector.h"
#include "common/h/String.h"
#include "elftraits.h"

bool getSecAndSecHdr(KELF::ET_Elf *theElf, 
		     const KELF::ET_Data &strTabSectionData,
                     const char *secNameToLookFor,
                     KELF::ET_Scn* &theSec, // filled in
                     KELF::ET_Shdr* &theSecHdr, // filled in
		     unsigned &ndxInSecHdrTab
                     );
// Give us a section name, and we'll give you the section (KELF::ET_Scn*),
// section header (KELF::ET_Shdr*), and index within the section header table
// (the latter needed to correlate symbols st_shndx field)

bool getSecHdrStrTab(KELF::ET_Elf *theElf, KELF::ET_Ehdr *theElfHeader,
                     KELF::ET_Scn* &secHdrStrTab, // filled in
                     KELF::ET_Shdr* &secHdrStrTabHdr, // filled in
                     KELF::ET_Data* &secHdrStrTabData // filled in
                     );
// Gets the string table (itself a section) which happens to hold the
// names of all other sections.  This particular string table is called
// the "section header string table".  book chap 5, p. 103 [for
// e_shstrndx], p. 118 [for string tables in general]

pdvector<pdstring> getAllSectionNames(KELF::ET_Elf *theElf);

#endif
