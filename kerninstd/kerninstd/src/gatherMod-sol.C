#include "driver.h"
#include "moduleMgr.h"
#include "section.h" // getSecAndSecHdr()
#include "SpringBoardHeap.h"

#include "util/h/StringPool.h"
#include "util/h/hashFns.h"
#include "util/h/binomialDistribStats.h"
#include "util/h/minmax.h"
#include "util/h/out_streams.h"

#include <assert.h>
#include <algorithm> // stl's sort()
#include <iostream>
#include <iomanip>
#include <sys/sysmacros.h> // roundup()

extern bool doublestring_cmp(const pair<pdstring,pdstring> &first,
                             const pair<pdstring,pdstring> &second);

extern dictionary_hash<kptr_t, pdvector< pair< pdstring, StaticString > > > untypedSymbols;

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
   for (unsigned modlcv=0; modlcv < numModules; modlcv++) {
      // module name:
      unsigned char modnamelen = 0; // initialize to avoid purify UMR
      memcpy(&modnamelen, user_buffer + offset, sizeof(modnamelen));
      offset += sizeof(modnamelen);
      const pdstring modNameAsString(user_buffer + offset);
      offset += modnamelen;
      offset = roundup(offset, sizeof(kptr_t));
      
      if (verbose_symbolTable)
         cout << "name of module #" << modlcv << " is: \"" 
              << modNameAsString << "\"\n";

      // module description:
      unsigned moddescriplen = 0;
      memcpy(&moddescriplen, user_buffer + offset, sizeof(moddescriplen));
      assert(moddescriplen < 1000);
      offset += sizeof(moddescriplen);
      char *mod_description = new char[moddescriplen+1]; // +1 for \0
      assert(mod_description);
      memcpy(mod_description, user_buffer + offset, moddescriplen);
      offset += moddescriplen;
      mod_description[moddescriplen] = '\0';

      const pdstring modDescriptionAsString(mod_description);
      delete [] mod_description;

      if (verbose_symbolTable)
         cout << "description of module is: \"" << modDescriptionAsString << "\"" << endl;

      // start of text seg:
      kptr_t text_start;
      offset = roundup(offset, sizeof(kptr_t));
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
      offset = roundup(offset, sizeof(kptr_t));
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
      
      // length of the string table:
      uint32_t strtablen = 0; // initialize to avoid purify UMR
      memcpy(&strtablen, user_buffer + offset, sizeof(strtablen));
      offset += sizeof(strtablen);
     
      if (verbose_symbolTable)
         cout << "size of string table=" << strtablen << endl;

      // Now actually get the string table
      const unsigned string_table_offset = offset;

      offset += strtablen;
#ifdef sparc_sun_solaris2_7
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
				  strtablen, (kptr_t)0);
      const StringPool &theStringPool = theModuleMgrAsConst.find(modNameAsString)->getStringPool();

      pdvector<basic_sym_info> thisModuleTypedSymbols, thisModuleAllSymbols;
	 // all symbols (most definitely including STT_FUNC *and* STT_NOTYPE,
	 // and even data symbols) with addresses.  Useful for detecting 
         // maximum function end addresses.
      
      const kptr_t crudeModuleEnd = roundup(max(text_start + text_len,
						data_start + data_len), sizeof(kptr_t));
      crudeModuleEnds.set(modNameAsString, crudeModuleEnd);

      // get the symbols, and add them
      if (verbose_symbolTable)
	 cout << "There are " << numsyms << " symbols in the module" << endl;

      for (unsigned symlcv=0; symlcv < numsyms; symlcv++) {

         const KELF::ET_Sym &sym = *(const KELF::ET_Sym*)(user_buffer+offset);
	 offset += sizeof(KELF::ET_Sym);
	 offset = roundup(offset, sizeof(kptr_t));

	 const StaticString symname = theStringPool.getStaticString(sym.st_name);
	 const KELF::ET_Addr symaddr = sym.st_value;
	 const KELF::ET_Size symsize = sym.st_size;

	 thisModuleAllSymbols += basic_sym_info(symaddr, symsize);
         
	 if (KELF::ET_ST_TYPE(sym.st_info) == KELF::ET_STT_NOTYPE) {
	    untypedSymbols[symaddr] += make_pair(modNameAsString, symname);
	    // works whether or not untypedSymbols.defines(symaddr) was true
	    // before the call

	    assert(untypedSymbols.defines(symaddr));
	 }
	 else {
	    thisModuleTypedSymbols += basic_sym_info(symaddr, symsize);
	 }

	 if (KELF::ET_ST_TYPE(sym.st_info) == KELF::ET_STT_FUNC &&
	     0 == strcmp(symname.c_str(), "_kerninst_springboard_space_")) {
	    const unsigned nbytes = sym.st_size;
            
	    dout << "Found a symbol called \"_kerninst_springboard_space_\"!"
	         << "with size " << (void*)nbytes
                 << ". Adding it to the springboard heap" << endl;

	    extern SpringBoardHeap *global_springboardHeap;
	    assert(global_springboardHeap);
	    global_springboardHeap->addChunk(symaddr, nbytes);

	    continue;
	 }

	 if (verbose_symbolTable) {
	    cout << "@ " << addr2hex((kptr_t)symaddr) << " is symbol: " << symname
		 << "; type=";
	    switch (KELF::ET_ST_TYPE(sym.st_info)) {
	    case KELF::ET_STT_FUNC:
	       cout << "STT_FUNC" << endl;
	       break;
	    case KELF::ET_STT_NOTYPE:
	       cout << "STT_NOTYPE (unspecified)" << endl;
	       break;
	    default:
	       cout << KELF::ET_ST_TYPE(sym.st_info) << endl;
	       break;
	    }
	 }

	 if (KELF::ET_ST_TYPE(sym.st_info) != KELF::ET_STT_FUNC)
	    continue;

         theModuleMgr.addFnToModuleButDontParseYet(modNameAsString,
						   symaddr,
						   symname,
						   false // don't remove aliases now
						   );

	 if (binary_search(functionNamesThatReturnForTheCaller.begin(),
			   functionNamesThatReturnForTheCaller.end(),
			   make_pair(modNameAsString, pdstring(symname.c_str())),
			      // call to symname.c_str() isn't expensive; the object
			      // type is StaticString, which has a dirt cheap
			      // c_str() call...
			   doublestring_cmp)) {
	    if (verbose_returningFuncs)
	       cout << "NOTE: found returning-on-behalf-of-caller routine "
		    << symname.c_str() << " in the kernel [as expected]" << endl;

	    functionsThatReturnForTheCaller += symaddr;
	 }

      } // symbols of this module

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

      // sort and remove aliases for this module (2d pass)
      if (verbose_removeAliases)
	 cout << "Removing aliases:" << endl;
      theModuleMgr.removeAliasesForModule(modNameAsString);
      if (verbose_removeAliases)
	 cout << "There are " << theModuleMgrAsConst.find(modNameAsString)->numFns()
	      << " fns after removing aliases" << endl;
      
      // 3d pass for module: parse fns (interproc jumps deferred until 4th next pass)
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
}
