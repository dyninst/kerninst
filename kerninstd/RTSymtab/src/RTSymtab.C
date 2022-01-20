// RTSymtab.C

#include "RTSymtab.h"
#include <stdio.h> // perror()
#include <string.h> // strcmp()
#include "util/h/Dictionary.h"
#include "util/h/hashFns.h"
#include <unistd.h> // close()
#include <fcntl.h> // O_RDONLY
#include "map_info.h"
#include "elftraits.h"

template<class ELFTraits> 
bool 
RTSymtab<ELFTraits>::getSecHdrStrTab(ET_Elf *theElf, 
                                     ET_Ehdr *theElfHeader,
                                     ET_Scn* &secHdrStrTab, // filled in
                                     ET_Shdr* &secHdrStrTabHdr,// filled in
                                     ET_Data* &secHdrStrTabData// filled in
	) {
   // Gets the string table (itself a section) which happens to hold the names of
   // all other sections.  This particular string table is called the
   // "section header string table".
   // book chap 5, p. 103 [for e_shstrndx], p. 118 [for string tables in general]

   secHdrStrTab = ELFTraits::et_getscn(theElf, theElfHeader->e_shstrndx);
   if (secHdrStrTab==NULL)
      return false;

   secHdrStrTabHdr = ELFTraits::et_getshdr(secHdrStrTab);
   
   if (secHdrStrTabHdr == NULL)
      return false;

   secHdrStrTabData = ELFTraits::et_getdata(secHdrStrTab, NULL);
   
   // NULL --> get first piece of data
   if (secHdrStrTabData == NULL)
      return false;

   return true;
}

template<class ELFTraits> pair<bool, typename ELFTraits::ET_Addr> 
RTSymtab<ELFTraits>::
lookup1NameInElfSymbols(ET_Sym *symbols, 
			unsigned num_symbols,
			const char *symNamesPool,
			const pdstring &symNameToLookFor) {
   const char *lookingFor = symNameToLookFor.c_str();
   
   ET_Sym *finish = symbols + num_symbols;
   
   while (symbols != finish) {
      switch (ELFTraits::ET_ST_TYPE(symbols->st_info)) {
         case ELFTraits::ET_STT_FUNC:
         case ELFTraits::ET_STT_OBJECT: {
            const char *symname = symNamesPool + symbols->st_name;
         
            const ET_Addr symvalue = symbols->st_value;
         
//           // in relocatable files, a section offset for the symbol
//           // in executable and shared object files, a virtual address
//           Elf32_Half symSectionNdx = symbols->st_shndx;
         
            // what section this symbol belongs to (section header table ndx)

            if (0==strcmp(lookingFor, symname)) {
               //cout << "Found desired symbol \"" << symNameToLookFor << "\"" << endl;
               return make_pair(true, symvalue);
            }
//           else
//              cout << symname << endl;
            break;
         }
         default:
            break;
      }

      symbols++;
   }

   cout << "Could not find symbol \"" << symNameToLookFor << "\"" << endl;
   return make_pair(false, 0);
}

template<class ELFTraits> 
pair<bool, typename ELFTraits::ET_Addr> 
RTSymtab<ELFTraits>::lookup1NameInElfExecutable(ET_Elf *theElf, 
                                                ET_Ehdr *theElfHeader,
                                                const pdstring &symName) {
#ifdef sparc_sun_solaris2_7
   if (theElfHeader->e_machine != EM_SPARC &&
       theElfHeader->e_machine != EM_SPARC32PLUS) {
      // We expect to see EM_SPARC, EM_SPARC32PLUS (ultra)
      cout << "process_elf_executable: not EM_SPARC or EM_SPARC32PLUS" << endl;
      return make_pair(false, 0);
   }
#elif defined(i386_unknown_linux2_4)
   if (theElfHeader->e_machine != EM_386) {
      // We expect to see EM_386
      cout << "process_elf_executable: not EM_386" << endl;
      return make_pair(false, 0);
   }
#elif defined(ppc64_unknown_linux2_4)
   if (theElfHeader->e_machine != EM_PPC64) {
      // We expect to see EM_PPC64
      cout << "process_elf_executable: not EM_PPC64" << endl;
      return make_pair(false, 0);
   }
#endif

   // First, find that special elf section known as the
   // "section header string table"
   ET_Scn *sec_hdr_strtab_descriptor;
   ET_Shdr *sec_hdr_strtab_header;
   ET_Data *sec_hdr_strtab_data;

   if (!getSecHdrStrTab(theElf, theElfHeader,
			sec_hdr_strtab_descriptor,
			sec_hdr_strtab_header,
			sec_hdr_strtab_data)) {
      cout << "could not read section header string table" << endl;
      return make_pair(false, 0);
   }

   map_info<ELFTraits> theMapInfo;

   const char *sec_hdr_names = (const char *)sec_hdr_strtab_data->d_buf;
   ET_Scn *sec_ptr = NULL;
   unsigned ndxInSecHdrTable = 0;
   
   while (NULL != (sec_ptr = ELFTraits::et_nextscn(theElf, sec_ptr))) {
      ET_Shdr *sec_hdr = ELFTraits::et_getshdr(sec_ptr);
      assert(sec_hdr);

      const char *sec_name = (const char *)&sec_hdr_names[sec_hdr->sh_name];
      ndxInSecHdrTable++;

      sectionInfo<ELFTraits> theSectionInfo(sec_name, ndxInSecHdrTable, 
					    sec_ptr, sec_hdr);

      theMapInfo.add_section(sec_name, theSectionInfo);
   }
   
   // Try to find a .symtab section and .strtab section
   if (!theMapInfo.sectionNameExists(".symtab")) {
      cout << "No .symtab section" << endl;
      return make_pair(false, 0);
   }
   
   if (!theMapInfo.sectionNameExists(".strtab")) {
      cout << "No .strtab section" << endl;
      return make_pair(false, 0);
   }

   sectionInfo<ELFTraits> &symtabSectionInfo = 
	   theMapInfo.getSectionInfoByName(".symtab");
   sectionInfo<ELFTraits> &strtabSectionInfo = 
	   theMapInfo.getSectionInfoByName(".strtab");

   ET_Data *symtab_data = symtabSectionInfo.getSectionData();
   ET_Data *strtab_data = strtabSectionInfo.getSectionData();
   ET_Sym *symbols = (ET_Sym *)symtab_data->d_buf;
   
   const unsigned num_symbols = 
	   symtab_data->d_size / sizeof(ET_Sym);

   return lookup1NameInElfSymbols(symbols, num_symbols,
				  (const char *)strtab_data->d_buf,
				  symName);
}

template<class ELFTraits> pair<bool, typename ELFTraits::ET_Addr> 
RTSymtab<ELFTraits>::
lookup1NameInElf(ET_Elf *theElf, const pdstring &symName) {
   ET_Ehdr *theElfHeader = ELFTraits::et_getehdr(theElf);
   if (theElfHeader == NULL) {
      cout << "et_getehdr failed" << endl;
      return make_pair(false, 0);
   }
   
   if (theElfHeader->e_ident[EI_CLASS] != ELFTraits::ET_CLASS32) {
      cout << "e_ident[EI_CLASS] not ET_CLASS32 as was expected" << endl;
      return make_pair(false, 0);
   }
   
   if (theElfHeader->e_ident[EI_DATA] != ELFTraits::ET_DATA2MSB) {
      cout << "e_ident[EI_DATA] not ET_DATA2MSB as was expected" << endl;
      return make_pair(false, 0);
   }

   switch (theElfHeader->e_type) {
      case ET_EXEC:
         // an executable file
         return lookup1NameInElfExecutable(theElf, theElfHeader, 
					   symName);
      case ET_REL:
         // a relocatable file
         cout << "lookup1NameInElf: don't know how to look up in a relocatable file"
              << endl;
         return make_pair(false, 0);
      case ET_DYN:
         // a shared object file
         cout << "lookup1NameInElf: don't know how to look up in a shared obj file"
              << endl;
         return make_pair(false, 0);
      default:
         cout << "lookup1NameInElf: unknown elf header e_type" << endl;
         return make_pair(false, 0);
   }
}

template<class ELFTraits> 
pair<bool, typename ELFTraits::ET_Addr> 
RTSymtab<ELFTraits>::lookup1NameInElfFd(int fd, const pdstring &symName) {
   (void)ELFTraits::et_version(ELFTraits::ET_CURRENT);
   (void)ELFTraits::et_errno();

   ET_Elf *theElf = ELFTraits::et_begin(fd, ELFTraits::ET_C_READ, NULL);
   if (ELFTraits::et_kind(theElf) != ELFTraits::ET_K_ELF) {
      cout << "elf_kind didn't return expected ELF_K_ELF" << endl;
      (void)ELFTraits::et_end(theElf);
      return make_pair(false, 0);
   }

   pair<bool, ET_Addr> result = lookup1NameInElf(theElf, symName);
   
   (void)ELFTraits::et_end(theElf);

   return result;
}


pair<bool, dptr_t> lookup1NameInSelf(const pdstring &symName) {
   static dictionary_hash<pdstring, dptr_t> cache(stringHash);

   // First check the cache:
   dptr_t cached_result;
   if (cache.find(symName, cached_result))
      return make_pair(true, cached_result);

#ifdef sparc_sun_solaris2_7   
   int fd = open("/proc/self/object/a.out", O_RDONLY);
   if (fd == -1) {
      perror("Could not open /proc/self/object/a.out");
      return make_pair(false, 0);
   }
#elif defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4)
   int fd = open("/proc/self/exe", O_RDONLY);
   if (fd == -1) {
      perror("Could not open /proc/self/exe");
      return make_pair(false, 0);
   }
#endif
   RTSymtab< elftraits<dptr_t> > searcher;

   const pair<bool, dptr_t> result = searcher.lookup1NameInElfFd(fd, symName);

   (void)close(fd);

   // Add to cache if successful:
   if (result.first)
      cache.set(symName, result.second);

   return result;
}

pair<bool, kptr_t> lookup1NameInKernel(const pdstring &symName) {
   static dictionary_hash<pdstring, kptr_t> cache(stringHash);

   // First check the cache:
   kptr_t cached_result;
   if (cache.find(symName, cached_result))
      return make_pair(true, cached_result);
   
#ifdef sparc_sun_solaris2_7
   int fd = open("/dev/ksyms", O_RDONLY);
   if (fd == -1) {
      perror("Could not open /dev/ksyms");
      return make_pair(false, 0);
   }

   RTSymtab< elftraits<kptr_t> > searcher;

   const pair<bool, kptr_t> result = searcher.lookup1NameInElfFd(fd, symName);

   (void)close(fd);

#elif defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4)
   pair<bool, kptr_t> result(false,0);

   FILE *ksymsinfo_f = fopen("/proc/ksyms", "r");
   if (ksymsinfo_f == NULL) {
      perror("Could not open /proc/ksyms");
      return make_pair(false, 0);
   }
   
   while(!feof(ksymsinfo_f)) {
      char strbuf[150];
      char *res = fgets(strbuf, 148, ksymsinfo_f);
      kptr_t symAddr = 0;
      char symbol[100];
      if(res != NULL) {
	 int assigned = sscanf(res, "%08lx %s", &symAddr, symbol);
	 if(assigned == 2) {
	    if(symName == pdstring(symbol)) {
	       result = pair<bool, kptr_t>(true, symAddr);
	       break;
	    }
	 }
      }
   } 

   fclose(ksymsinfo_f);
#endif

   // Add to cache if successful:
   if (result.first)
      cache.set(symName, result.second);

   return result;
}

