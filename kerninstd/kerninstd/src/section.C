// section.C

#include "section.h"

bool getSecAndSecHdr(KELF::ET_Elf *theElf, 
		     const KELF::ET_Data &strTabSectionData,
                     const char *secNameToLookFor,
                     KELF::ET_Scn* &theSec, // filled in
                     KELF::ET_Shdr* &theSecHdr, // filled in
		     unsigned &ndxInSecHdrTable
                     ) {
   // Give us a section name, and we'll give you the section (KELF::ET_Scn*)
   // and section header (KELF::ET_Shdr*)

   // Linear search
   const char *sec_hdr_names = (const char *)strTabSectionData.d_buf;

   ndxInSecHdrTable = 0;
   KELF::ET_Scn *section_ptr = NULL; // NULL --> 1st call should return sec ndx 1 (skip
                                // SHN_UNDEF present at ndx 0)
   while (NULL != (section_ptr = KELF::et_nextscn(theElf, section_ptr))) {
      // Section headers: book chap 5, p. 109

      ndxInSecHdrTable++;

      KELF::ET_Shdr *section_header = KELF::et_getshdr(section_ptr);
      if (section_header == NULL)
         assert(false && "unexpected NULL sec hdr ptr");

      const char *sec_name = (const char *)&sec_hdr_names[section_header->sh_name];
      if (0==strcmp(secNameToLookFor, sec_name)) {
         theSec = section_ptr;
         theSecHdr = section_header;
         return true;
       }
    }

   return false;
}

bool getSecHdrStrTab(KELF::ET_Elf *theElf, KELF::ET_Ehdr *theElfHeader,
                     KELF::ET_Scn* &secHdrStrTab, // filled in
                     KELF::ET_Shdr* &secHdrStrTabHdr, // filled in
                     KELF::ET_Data* &secHdrStrTabData // filled in
                     ) {
   // Gets the string table (itself a section) which happens to hold the names of
   // all other sections.  This particular string table is called the
   // "section header string table".
   // book chap 5, p. 103 [for e_shstrndx], p. 118 [for string tables in general]

   secHdrStrTab = KELF::et_getscn(theElf, theElfHeader->e_shstrndx);
   if (secHdrStrTab==NULL)
      return false;

   secHdrStrTabHdr = KELF::et_getshdr(secHdrStrTab);
   if (secHdrStrTabHdr == NULL)
      return false;

   secHdrStrTabData = KELF::et_getdata(secHdrStrTab, NULL);
      // NULL --> get first piece of data
   if (secHdrStrTabData == NULL)
      return false;

   return true;

}

pdvector<pdstring> getAllSectionNames(KELF::ET_Elf *theElf) {
   pdvector<pdstring> result;

   KELF::ET_Ehdr *theElfHeader = KELF::et_getehdr(theElf);

   KELF::ET_Scn* secHdrStrTab;
   KELF::ET_Shdr* secHdrStrTabHdr;
   KELF::ET_Data* secHdrStrTabData;

   if (!getSecHdrStrTab(theElf, theElfHeader,
			secHdrStrTab, secHdrStrTabHdr,
			secHdrStrTabData))
      assert(false);

   const char *sec_hdr_names = (const char *)secHdrStrTabData->d_buf;

   KELF::ET_Scn *sec_ptr = NULL;
   while (NULL != (sec_ptr = KELF::et_nextscn(theElf, sec_ptr))) {
      KELF::ET_Shdr *sec_hdr = KELF::et_getshdr(sec_ptr);
      assert(sec_hdr);

      const char *sec_name = (const char *)&sec_hdr_names[sec_hdr->sh_name];
      result += pdstring(sec_name);
   }

   return result;
}

