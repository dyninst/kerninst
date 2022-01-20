#include "driver.h"
#include "moduleMgr.h"
#include "machineInfo.h"

#include "util/h/StringPool.h"
#include "util/h/hashFns.h"
#include "util/h/binomialDistribStats.h"
#include "util/h/out_streams.h"
#include "util/h/minmax.h"

#include <fstream.h>
#include <fcntl.h> // O_RDONLY
#include <assert.h>
#include <sys/utsname.h>
#include <algorithm> // stl's sort()
#include <iostream>
#include <iomanip>
#ifdef ppc64_unknown_linux2_4
#include "power_CondRegClearLauncher.h"
#include "launcher.h"
#include "SpringBoardHeap.h"
#include "fillin.h"
#endif

#ifdef ppc64_unknown_linux2_4
dictionary_hash<pdstring, unsigned> requestedInternalKernelSymbolIndex (stringHash);
//key: name   value: index

kptr_t requestedInternalKernelSymbols[KERNINST_NUM_REQUESTED_INTERNAL_SYMBOLS];

void initRequestedInternalKernelSymbols () {
   requestedInternalKernelSymbolIndex["get_lock_slot"] = 
	                                       KERNINST_GET_LOCK_SLOT_INDEX;
   requestedInternalKernelSymbolIndex["bolted_dir"] = 
	                                       KERNINST_BOLTED_DIR_INDEX;
   requestedInternalKernelSymbolIndex["hash_table_lock"] = 
	                                       KERNINST_HASH_TABLE_LOCK_INDEX;
   requestedInternalKernelSymbolIndex["__pmd_alloc"] = 
	                                       KERNINST_PMD_ALLOC_INDEX;
   requestedInternalKernelSymbolIndex["pte_alloc"] = 
	                                       KERNINST_PTE_ALLOC_INDEX;
   requestedInternalKernelSymbolIndex["ppc_md"] = 
	                                       KERNINST_PPC_MD_INDEX;
      
   requestedInternalKernelSymbolIndex["btmalloc_mm"] = 
	                                       KERNINST_BTMALLOC_MM_INDEX;
   requestedInternalKernelSymbolIndex["local_free_bolted_pages"] = 
	                                KERNINST_LOCAL_FREE_BOLTED_PAGES_INDEX;
   requestedInternalKernelSymbolIndex["smp_call_function"] = 
	                                KERNINST_SMP_CALL_FUNCTION_INDEX;
   requestedInternalKernelSymbolIndex["ste_allocate"] = 
	                                KERNINST_STE_ALLOCATE_INDEX;
   requestedInternalKernelSymbolIndex["htab_data"] = 
	                                KERNINST_HTAB_DATA_INDEX;
   requestedInternalKernelSymbolIndex["hpte_remove"] = 
	                                KERNINST_HPTE_REMOVE_INDEX;
   requestedInternalKernelSymbolIndex["find_linux_pte"] = 
	                                KERNINST_FIND_LINUX_PTE_INDEX;
   requestedInternalKernelSymbolIndex["pSeries_tlbie_lock"] = 
	                                KERNINST_PSERIES_TLBIE_LOCK_INDEX;
   requestedInternalKernelSymbolIndex["plpar_pte_remove"] = 
	                                KERNINST_PLPAR_PTE_REMOVE_INDEX;
   requestedInternalKernelSymbolIndex["kerninst_malloc"] = 
                                        KERNINST_MALLOC_INDEX;
   requestedInternalKernelSymbolIndex["kerninst_free"] = 
	                                KERNINST_FREE_INDEX;
   requestedInternalKernelSymbolIndex["init_requested_pointers_to_kernel"] = 
	                      KERNINST_INIT_REQUESTED_POINTERS_TO_KERNEL_INDEX;
   requestedInternalKernelSymbolIndex["plpar_hcall_norets"] = 
	                                KERNINST_PLPAR_HCALL_NORETS_INDEX;
   requestedInternalKernelSymbolIndex["pte_alloc_kernel"] = 
	                                KERNINST_PTE_ALLOC_KERNEL_INDEX;
   requestedInternalKernelSymbolIndex["paca"] = 
	                                KERNINST_PACA_INDEX;
   requestedInternalKernelSymbolIndex["cur_cpu_spec"] = 
	                                KERNINST_CUR_CPU_SPEC_INDEX;
}

static void installNoSlbBoltedPatch( kptr_t DataAccessSLBAddr) {
   //create a launcher to overwrite the call to do_slb_bolted with
   //a jump to the next instruction (nop is probably better but
   // this will suffice).  It's pretty safe to assume fixed offset
   // from start of the function, since this is hand assembly, not
   // C code.
   SpringBoardHeap dummyheap;
   extern kernelDriver *global_kernelDriver; // sorry about this ugliness
   insnVec_t *filledin_insns = insnVec_t::getInsnVec();	
   fillin_kernel(filledin_insns,
                 DataAccessSLBAddr + 20,
                 DataAccessSLBAddr + 24,
                 *global_kernelDriver);
   instr_t *originsn = filledin_insns->get_by_ndx(0);
   launcher *nopLauncher = launcher::pickAnAnnulledLauncher(
                                      DataAccessSLBAddr + 20, //from
                                      DataAccessSLBAddr + 24, //to next instr
                                      originsn,
                                      dummyheap); 
   try {
      nopLauncher->installNotOverwritingExisting(false, //not justTesting
                                                 NULL,
                                                 *global_kernelDriver,
                                                 false);
   } catch (...) {
      assert(!"Could not install SLB patch");
   }
}

static void installNoEaBitsCheckPatch( kptr_t ste_allocateAddr) {
   //create a launcher to overwrite the check for bad EA bits with 
   //a jump to the next instruction (nop is probably better but
   // this will suffice).
   SpringBoardHeap dummyheap;
   extern kernelDriver *global_kernelDriver; // sorry about this ugliness
   insnVec_t *filledin_insns = insnVec_t::getInsnVec();	
  
   //We will search for the particular instruction we want to overwrite
   //starting with the beginning of the function.
   fillin_kernel(filledin_insns,
                 ste_allocateAddr ,
                 ste_allocateAddr + 0x200, //arbitrary stopping point
                 *global_kernelDriver);

   //instruction we are looking for is rldicr. rx, rx, 60, 22 where
   //rx could be any register but is usually r0.
   kptr_t checkInstrIndex = 0;
   insnVec_t::const_iterator filled_iter = filledin_insns->begin();
   insnVec_t::const_iterator filled_finish = filledin_insns->end();
   for (;filled_iter != filled_finish; filled_iter++, checkInstrIndex++) {
      power_instr *instr = (power_instr *) *filled_iter;
      if ( ( instr->isRotateLeftDoubleWordImmediateClearRight() ) && 
           ( instr->getShFieldMd() == 60) &&
           ( instr->getMeFieldMd() == 22)  ) 
         break;
   }
   assert( (filled_iter != filled_finish) && "Could not install bad EA bits checkpatch "); 

   instr_t *originsn = filledin_insns->get_by_ndx(checkInstrIndex);

   //Now we overwrite it with an instruction that sets the Zero bit 
   //of the CR, thus disabling the bne branch that follows.
   launcher *crclrLauncher =  new power_CondRegClearLauncher(
                                ste_allocateAddr + checkInstrIndex*4, //from
                                0, //nowhere 
                                originsn);
   try {
      crclrLauncher->installNotOverwritingExisting(false, //not justTesting
                                                   NULL,
                                                   *global_kernelDriver,
                                                   false);
   } catch (...) {
      assert(!"Could not install SLB patch");
   }
}
#endif // ppc64_unknown_linux2_4

extern bool global_SystemMap_provided;
extern char global_SystemMapFile[];
extern machineInfo *global_machineInfo;

extern bool doublestring_cmp(const pair<pdstring,pdstring> &first,
                             const pair<pdstring,pdstring> &second);

static void strip_Rversion(StaticString &symname)
{
   // strip the rversion info from a symbol name by in-place truncation

   const unsigned smp_version_length = 14;
   const char smp_rversion[] = "_Rsmp_";
   const unsigned smp_rversion_len = 6;
   const unsigned version_length = 10;
   const char rversion[] = "_R";
   const unsigned rversion_len = 2;

   if(symname.size() > smp_version_length) {
      unsigned real_length = symname.size() - smp_version_length;
      char *tmp = const_cast<char*>(symname.c_str()) + real_length;
      if (0 == strncmp(tmp, &smp_rversion[0], smp_rversion_len)) {
         // possible R-version match, but let's make sure
         tmp += smp_rversion_len;
         bool is_rversion = true;
         for(int i=0; i<8; i++) {
            char digit = tmp[i];
            // if not hex character, then not rversion
            if((digit < '0' || digit > '9') && 
               (digit < 'a' || digit > 'f')) {
               is_rversion = false;
               break;
            }
         }
         if(is_rversion) {
            tmp = const_cast<char*>(symname.c_str());
            tmp[real_length] = '\0';
         }
      }
   }
   if(symname.size() > version_length) {
      unsigned real_length = symname.size() - version_length;
      char *tmp = const_cast<char*>(symname.c_str()) + real_length;
      if (0 == strncmp(tmp, &rversion[0], rversion_len)) {
         // possible R-version match, but let's make sure
         tmp += rversion_len;
         bool is_rversion = true;
         for(int i=0; i<8; i++) {
            char digit = tmp[i];
            // if not hex character, then not rversion
            if((digit < '0' || digit > '9') && 
               (digit < 'a' || digit > 'f')) {
               is_rversion = false;
               break;
            }
         }
         if(is_rversion) {
            tmp = const_cast<char*>(symname.c_str());
            tmp[real_length] = '\0';
         }
      }
   }
}

int addrCompareFunc(const void *e1, const void *e2)
{
   kptr_t addr1 = *(const kptr_t*)e1;
   kptr_t addr2 = *(const kptr_t*)e2;

   if(addr1 > addr2)
      return 1;
   if(addr1 == addr2)
      return 0;
   else 
      return -1;
}

void gatherMods(moduleMgr &theModuleMgr, const moduleMgr &theModuleMgrAsConst, 
                dictionary_hash<pdstring, kptr_t> &crudeModuleEnds,
                dictionary_hash<kptr_t,bool> &interProcJumps,
                pdvector<basic_sym_info> &allModulesAllSymbols,
                pdvector<kptr_t> &functionsThatReturnForTheCaller,
          pdvector< pair<pdstring,pdstring> > &functionNamesThatReturnForTheCaller,
                pdvector<hiddenFn> &hiddenFns,
                mrtime_t &findNextSymbolStartAddr_totalTime,
                unsigned numModules, char *user_buffer, unsigned long &offset,
                bool verbose_symbolTable, bool verbose_removeAliases, 
                bool verbose_fnParse, bool verbose_skips,
                bool verbose_returningFuncs)
{
   char SysMapFileName[100];
   SysMapFileName[0] = '\0';
   bool using_sysmap = (global_SystemMap_provided ? true : false);
   unsigned sysmap_numsyms = 0;
   unsigned new_stringpool_size = 0;

#ifdef ppc64_unknown_linux2_4
   kptr_t DataAccessSLBAddr = 0;
   kptr_t ste_allocateAddr = 0;
   initRequestedInternalKernelSymbols();
#endif

   for (unsigned modlcv=0; modlcv < numModules; modlcv++) {
      // module name:
      unsigned char modnamelen = 0; // initialize to avoid purify UMR
      memcpy(&modnamelen, user_buffer + offset, sizeof(modnamelen));
      offset += sizeof(modnamelen);
      const pdstring modNameAsString(user_buffer + offset);
      offset += modnamelen;
#if  defined(ppc64_unknown_linux2_4)
      offset = roundup(offset, sizeof(kptr_t));
#endif

      if (verbose_symbolTable)
         cout << "name of module #" << modlcv << " is: \"" 
              << modNameAsString << "\"\n";
      
      // just use name since Linux doesn't provide description at run time
      const pdstring modDescriptionAsString(modNameAsString);

      // start of text seg:
      kptr_t text_start;
#if defined(ppc64_unknown_linux2_4)
      offset = roundup(offset, sizeof(kptr_t));
#endif
      memcpy(&text_start, user_buffer + offset, sizeof(text_start));
      offset += sizeof(text_start);
      if (verbose_symbolTable)
         cout << "start of text seg: " << addr2hex(text_start) << endl;

      // length of text seg:
      uint32_t text_len;
      memcpy(&text_len, user_buffer + offset, sizeof(text_len));
      offset+= sizeof(text_len);
      if (verbose_symbolTable) {
         cout << "length of text seg: " << text_len << endl;
         cout << "so the last text address is "
              << addr2hex((text_start) + text_len - 1) << endl;
      }
      
      // start of data seg:
      kptr_t data_start;
#if  defined(ppc64_unknown_linux2_4)
      offset = roundup(offset, sizeof(kptr_t));
#endif
      memcpy(&data_start, user_buffer + offset, sizeof(data_start));
      offset += sizeof(data_start);
      if (verbose_symbolTable)
         cout << "start of data seg: " << addr2hex(data_start) << endl;

      // length of data seg:
      uint32_t data_len; // initialize to avoid purify UMR
      memcpy(&data_len, user_buffer + offset, sizeof(data_len));
      offset+= sizeof(data_len);
      if (verbose_symbolTable) {
         cout << "length of data seg: " << data_len << endl;
         cout << "so the last data address is "
              << addr2hex((data_start) + data_len - 1) << endl;
      }

      kptr_t TOCPtr = 0; //useful on power only, for now.
#ifdef ppc64_unknown_linux2_4
      offset = roundup(offset, sizeof(kptr_t));
  // start of toc seg:
      kptr_t toc_start;

      memcpy(&toc_start, user_buffer + offset, sizeof(toc_start));
      offset += sizeof(toc_start);
      if (verbose_symbolTable)
         cout << "start of toc seg: " << addr2hex(toc_start) << endl;

      // length of toc seg:
      uint32_t toc_len;
      memcpy(&toc_len, user_buffer + offset, sizeof(toc_len));
      offset+= sizeof(toc_len);
      offset = roundup(offset, sizeof(kptr_t));
      if (verbose_symbolTable) {
         cout << "length of toc seg: " << toc_len << endl;
         cout << "so the last toc address is "
              << addr2hex((toc_start) + toc_len - 1) << endl;
      }
      TOCPtr = toc_start + 0x8000;
      //r2 points to the middle of TOC so that all 64K are
      //addressable with a signed offset.
#endif
    
      // length of the string table:
      uint32_t strtablen = 0; // initialize to avoid purify UMR
      memcpy(&strtablen, user_buffer + offset, sizeof(strtablen));
      offset += sizeof(strtablen);
     
      if (verbose_symbolTable)
         cout << "size of string table=" << strtablen << endl;

      // Now actually get the string table
      const unsigned string_table_offset = offset;
      offset += strtablen;
#if defined(ppc64_unknown_linux2_4)
      offset = roundup(offset, sizeof(kptr_t));
#endif

      // cookie
      uint32_t cookie;
      memcpy(&cookie, user_buffer + offset, sizeof(cookie));
      offset += sizeof(cookie);
      assert(cookie == 0x12345678);

      // number of symbols in this module:
      unsigned long numsyms = 0; // initialize to avoid purify UMR
      memcpy(&numsyms, user_buffer + offset, sizeof(numsyms));
      offset += sizeof(numsyms);

      theModuleMgr.addEmptyModule(modNameAsString,
				  modDescriptionAsString,
				  user_buffer+string_table_offset,
				  strtablen,
                                  TOCPtr);
      const StringPool &theStringPool = theModuleMgrAsConst.find(modNameAsString)->getStringPool();

      pdvector<basic_sym_info> thisModuleTypedSymbols, thisModuleAllSymbols;
	 // all symbols with addresses.  Useful for detecting maximum 
         // function end addresses.

      dictionary_hash<pdstring, uint32_t> the_syms(stringHash);
      pdvector<kptr_t> the_addrs;
      pdvector<uint32_t> the_name_offsets;
      unsigned txt_ndx = 0;

      bool is_kernel = (modNameAsString == pdstring("kernel"));

      struct kerninst_symbol ksym;
      kptr_t symaddr = 0;
      kptr_t name_off = 0;

      if(!is_kernel || (is_kernel && !using_sysmap)) {
         // use symbol data from /dev/kerninst

         if(is_kernel) {
            assert(global_machineInfo != NULL);
            if(global_machineInfo->getOSVersion() == 26) { // Linux 2.6.x
               cerr << "ERROR: you must use the '-system_map <filename>' option to kerninstd when running with a Linux 2.6 series kernel\n. Please re-run kerninstd and specify a valid System.map file for the currently running kernel.\n";
               exit(5);
            }
         }

         for (unsigned symlcv=0; symlcv < numsyms; symlcv++) {

            memcpy((void*)&ksym, (void*)(user_buffer + offset), sizeof(ksym));
            symaddr = ksym.sym_addr;
            name_off = ksym.sym_name_offset;
            offset += sizeof(ksym);
#if  defined(ppc64_unknown_linux2_4)
            offset = roundup(offset, sizeof(kptr_t));
#endif

            StaticString sym_name = theStringPool.getStaticString(name_off);
            pdstring new_name(sym_name.c_str());

            // we can ignore symbols with no name or address == 0
            if((symaddr != 0) && (new_name != pdstring(""))) {
               bool dup_sym = false;
               if(the_syms.defines(new_name)) {
                  uint32_t prev_sym_ndx = the_syms.get(new_name);
                  if(the_addrs[prev_sym_ndx] != symaddr) {
                     // duplicate named symbol @ different address, 
                     // so must be weak. we'll append the new one's address
                     // to differentiate
                     new_name += "_";
                     new_name += addr2hex(symaddr);
                     if (the_syms.defines(new_name)) {
                        //Yes, this happens on power
                        dup_sym = true;
                     }
                  }
                  else
                     dup_sym = true;
               }

               if(!dup_sym) {
                  the_syms.set(new_name, txt_ndx++);
                  the_addrs.push_back(symaddr);
                  the_name_offsets.push_back(name_off);
               
                  if((text_start == 0) || (data_start == 0)) {
                     /* check for special insmod symbols */
                     if(!strncmp(sym_name.c_str(), "__insmod_", 9)) {
                        char *tmp = const_cast<char*>(sym_name.c_str()) + 
                           10 + (modnamelen-1);
                        if(!strncmp(tmp, "S.text_L", 8)) {
                           text_start = symaddr;
                           /* determine length from sym string */
                           tmp += 8;
                           text_len = (unsigned long) atol(tmp);
                        }
                        else if(!strncmp(tmp, "S.data_L", 8)) {
                           data_start = symaddr;
                           /* determine length from sym string */
                           tmp += 8;
                           data_len = (unsigned long) atol(tmp);
                        }
                     }
                  }
               }
            }
         }
      }
      else {
         // this is the "kernel" module and using System.map

         kptr_t lo_addr = 0xC0100000;
         kptr_t hi_addr = 0xC0400000;

         strcpy(&SysMapFileName[0], &global_SystemMapFile[0]);
         assert(SysMapFileName[0] != '\0');

         FILE *sysmap_f = fopen(SysMapFileName, "r" );
         if(sysmap_f == NULL) 
            cerr << "kernelDriver: unable to open System.map file with name " 
                 << &global_SystemMapFile[0] << endl;
         else {
            using_sysmap = true;
            while(!feof(sysmap_f)) {
               char strbuf[100];
               char *res = fgets(strbuf, 98, sysmap_f);
               char type;
               kptr_t sym_addr;
               char sym_name[80];
               if(res != NULL) {
                  int totassigned = sscanf(res, "%08lx %c %s",
                                           &sym_addr, &type, 
                                           &sym_name[0]);
                  if(totassigned == 3) {
                     if((type == 't') || (type == 'T') || (type == 'A')) {
                        if(! strncmp(&sym_name[0], "_stext", 6)) {
                           lo_addr = sym_addr;
                        }
                        else if(! strncmp(&sym_name[0], "_etext", 6)) {
                           hi_addr = sym_addr;
                        }
                        // ignore fake "__crc_<symname>" symbols
                        if(strncmp(&sym_name[0], "__crc_", 6)) {
                           pdstring new_name(&sym_name[0]);
                           if(the_syms.defines(new_name)) {
                              if(type == 't') {
                                 // duplicate name, append address
                                 new_name += "_";
                                 new_name += addr2hex(sym_addr);
                              }
                              else {
                                 // duplicate, but this is a strong symbol
                                 // so need to do some switching to make 
                                 // it the only symbol with it's name
                                 // (as necessary for the System.map 
                                 //  validity check later on)
                                 unsigned prev_ndx = the_syms.get(new_name);
                                 the_syms.undef(new_name);
                                 pdstring prev_name = new_name;
                                 prev_name += "_";
                                 prev_name += addr2hex(the_addrs[prev_ndx]);
                                 the_syms.set(prev_name, prev_ndx);
                                 new_stringpool_size += prev_name.length()+1;
                              }
                           }
                           the_syms.set(new_name, txt_ndx++);
                           the_addrs.push_back(sym_addr);
                           new_stringpool_size += new_name.length() + 1;
                        }
                     }
                  }
               }
            }
            fclose(sysmap_f);
            sysmap_numsyms = the_syms.size();
         }
         text_start = lo_addr;
         text_len = hi_addr - text_start;            
      } 

      const kptr_t crudeModuleEnd = roundup(max(text_start + text_len,
                                                data_start + data_len),
                                            sizeof(kptr_t) );
      crudeModuleEnds.set(modNameAsString, crudeModuleEnd);

      // get the symbols, and add them
      char *new_stringpool;
      unsigned new_stringpool_offset = 0;
      if(is_kernel && using_sysmap) {
         if(sysmap_numsyms > numsyms)
            numsyms = sysmap_numsyms;
         new_stringpool = new char[new_stringpool_size]; // must use new[]
      }

      if (verbose_symbolTable)
	 cout << "There are " << numsyms << " symbols in the module" << endl;


      pdvector<kptr_t> sorted_addrs(the_addrs);
      sorted_addrs.sort(addrCompareFunc);
      unsigned sorted_size = sorted_addrs.size();

      dictionary_hash<pdstring,unsigned>::const_iterator iter = the_syms.begin();
      dictionary_hash<pdstring,unsigned>::const_iterator finish = the_syms.end();
      for (; iter != finish; iter++) {
         unsigned ndx = iter.currval();
         kptr_t symaddr = the_addrs[ndx];
         unsigned symsize = 0;
         StaticString symname;

         if(!(is_kernel && using_sysmap)) {
            name_off = the_name_offsets[ndx];
            symname = theStringPool.getStaticString(name_off);
         }
         else {
            const pdstring &sym = iter.currkey();
            unsigned sym_name_len = sym.length() + 1;
            assert(new_stringpool_offset < new_stringpool_size);
            strcpy(new_stringpool+new_stringpool_offset, sym.c_str());
            symname = StaticString(new_stringpool+new_stringpool_offset);
            new_stringpool_offset += sym_name_len;
         }
         // strip off R-versioning, if found
         strip_Rversion(symname);
#ifdef ppc64_unknown_linux2_4
         if (0 == strcmp(symname.c_str(), ".text"))
            continue;
#endif

         if(symaddr != sorted_addrs[sorted_size-1]) {
            // not last symbol
            unsigned curraddr_ndx = 0;
            if(find(sorted_addrs, symaddr, curraddr_ndx)) {
               unsigned nextaddr_ndx = curraddr_ndx+1;
               kptr_t nextaddr = sorted_addrs[nextaddr_ndx];
               while((nextaddr_ndx < sorted_size) && 
                     (symaddr == nextaddr)) {
                  nextaddr = sorted_addrs[++nextaddr_ndx];
               }
               if(symaddr < nextaddr)
                  symsize = nextaddr - symaddr;
               else {
                  // last symbol - assume last in text section
                  symsize = (text_start + text_len) - symaddr + 1;
               }
            }
            else
               assert(!"internal error: could not find symbol addr in sorted vector of addresses\n");
         }
         else {
            // last symbol - assume last in text section
            symsize = (text_start + text_len) - symaddr + 1;
         }

         thisModuleAllSymbols += basic_sym_info(symaddr, symsize);

	 // we don't have typed vs untyped since no ELF info, so assume the
	 // symbol is interesting and add it to the typed symbols list
	 thisModuleTypedSymbols += basic_sym_info(symaddr, symsize);

	 if (verbose_symbolTable) {
	    cout << "@ " << addr2hex(symaddr) << " is symbol: " 
                 << symname.c_str() << endl;
	 }
#ifdef ppc64_unknown_linux2_4
         pdstring symn(symname.c_str());
         if (requestedInternalKernelSymbolIndex.defines(symn) ) {
             unsigned ind = requestedInternalKernelSymbolIndex[symn];
             requestedInternalKernelSymbols[ind] = symaddr;
             //cout<<"setting symbol "<<symn << " to be address " << addr2hex(symaddr) <<endl;
         }
         if (symn == "DataAccessSLB_common") {
            DataAccessSLBAddr = symaddr;
         }
         if (symn == ".ste_allocate") {
            ste_allocateAddr = symaddr;
         }
         //need to work with only function symbols from here on;
         //on power, symbols whose name beging with a dot (.) are
         //function entry addresses
         if ( (symname == "")|| (symname[0] != '.') || (symname == ".data") ||
              (symname == ".bss") || (symname == ".opd") ||
              (symname == ".sbss") ||
              (symname == ".kstrtab") || (symname == ".fixup")
              || (symname == ".toc") || (symname == ".rodata")  
              || (symname == ".toc1")  ) 
            continue;
        //remove the dot now, so that it's not ever displayed
        char *tmp = const_cast<char *> (symname.c_str() + 1);
        StaticString new_sym(tmp);
        symname = new_sym; 
#else
	 // need to work with only function symbols from here on; best
	 // effort guess is to check if addr is in text section
	 if(!((symsize > 0) && (symaddr >= text_start) 
	      && (symaddr+symsize <= text_start+text_len))) {
	    continue;
	 }
#endif //ppc64

         theModuleMgr.addFnToModuleButDontParseYet(modNameAsString,
						   symaddr,
						   symname,
						   false // don't remove aliases now
						   );

         if (binary_search(functionNamesThatReturnForTheCaller.begin(),
			   functionNamesThatReturnForTheCaller.end(),
			   make_pair(modNameAsString, pdstring(symname.c_str())),
                           doublestring_cmp)) {
	    if (verbose_returningFuncs)
	       cout << "NOTE: found returning-on-behalf-of-caller routine "
		    << symname.c_str() << " in the kernel [as expected]" << endl;

	    functionsThatReturnForTheCaller += symaddr;
	 }

      } // symbols of this module

      if(is_kernel && using_sysmap) {
         StringPool &mutable_theStringPool = const_cast<StringPool &>(theStringPool);
         mutable_theStringPool.~StringPool(); // free old pool
         mutable_theStringPool.set_contents_hack(new_stringpool, 
                                                 new_stringpool_size);
      }

      allModulesAllSymbols += thisModuleAllSymbols; // All!
      std::sort(thisModuleTypedSymbols.begin(), thisModuleTypedSymbols.end()); 
	// Typed-only !
      pdvector<basic_sym_info>::const_iterator mod_end =
	 std::unique(thisModuleTypedSymbols.begin(), thisModuleTypedSymbols.end());
      if (mod_end != thisModuleTypedSymbols.end()) {
	 thisModuleTypedSymbols.shrink(mod_end - 
				       thisModuleTypedSymbols.begin());
      }

      // Sort and remove duplicates from "functionsThatReturnForTheCaller[]"
      std::sort(functionsThatReturnForTheCaller.begin(),
		functionsThatReturnForTheCaller.end());
      pdvector<kptr_t>::const_iterator funcs_end = 
	 std::unique(functionsThatReturnForTheCaller.begin(),
		     functionsThatReturnForTheCaller.end());
      if (funcs_end != functionsThatReturnForTheCaller.end())
	 functionsThatReturnForTheCaller.shrink(funcs_end - functionsThatReturnForTheCaller.begin());

      // (2nd pass) Sort and remove aliases for this module
      if (verbose_removeAliases)
	 cout << "Removing aliases:" << endl;
      theModuleMgr.removeAliasesForModule(modNameAsString);
      if (verbose_removeAliases)
	 cout << "There are " << theModuleMgrAsConst.find(modNameAsString)->numFns()
	      << " fns after removing aliases" << endl;
      
      // (3rd pass) Parse fns (interproc jumps deferred until 4th pass)
      const loadedModule &mod = *theModuleMgrAsConst.find(modNameAsString);
      loadedModule::const_iterator fniter = mod.begin();
      loadedModule::const_iterator fnfinish = mod.end();
      for (; fniter != fnfinish; ++fniter) {
	 const function_t *thisFn = *fniter;

	 const mrtime_t findNextSymbolStartAddr_startTime = getmrtime();
         
	 // Try to find the next function's start addr, so we can max out
	 // the fnCode of this function.  Or better: try to find the next address
	 // of *any* symbol.  Use thisModuleTypedSymbols[].
	 pdvector<basic_sym_info>::const_iterator iter =
	    std::lower_bound(thisModuleTypedSymbols.begin(), 
			     thisModuleTypedSymbols.end(),
			     basic_sym_info(thisFn->getEntryAddr(), 0));
	 assert(iter != thisModuleTypedSymbols.end() &&
		iter->addr == thisFn->getEntryAddr());
	 unsigned fnSizeElf = iter->size; // size as reported by ELF
	 kptr_t endFnAddr = iter->addr + iter->size; // as reported by ELF

	 ++iter; // now we have the *next* symbol!  Yea!  (or, end())

	 const kptr_t nextSymbolAddr = (iter == thisModuleTypedSymbols.end() ?
					crudeModuleEnd : iter->addr);
	 assert(nextSymbolAddr > thisFn->getEntryAddr());
	 //assert(nextSymbolAddr <= crudeModuleEnd);

	 if (verbose_fnParse) {
	    cout << "@ " << addr2hex(thisFn->getEntryAddr()) << " names:";
	    cout << thisFn->getPrimaryName();
	    for (unsigned lcv=0; lcv < thisFn->getNumAliases(); lcv++)
	       cout << ' ' << thisFn->getAliasName(lcv);
	    cout << endl;
	 }

	 findNextSymbolStartAddr_totalTime +=
	    getmrtime() - findNextSymbolStartAddr_startTime;

	 if (fnSizeElf == 0 || endFnAddr > nextSymbolAddr) {
	   endFnAddr = nextSymbolAddr;
	 }
	 (void)parse1now(modNameAsString,
			 thisFn,
			 endFnAddr,
			 crudeModuleEnd,
			 interProcJumps,
			 hiddenFns,
			 verbose_fnParse, verbose_skips);
      } // iterate thru fns in this module

      if (verbose_fnParse)
	 cout << "finished parsing functions for this module" << endl;
   } // modules

#ifdef ppc64_unknown_linux2_4
   extern kernelDriver *global_kernelDriver; // sorry about this ugliness
   global_kernelDriver->putInternalKernelSymbols(requestedInternalKernelSymbols);
   assert(DataAccessSLBAddr);
   assert(ste_allocateAddr);
   installNoSlbBoltedPatch(DataAccessSLBAddr);
   installNoEaBitsCheckPatch(ste_allocateAddr);
#endif
   

}
