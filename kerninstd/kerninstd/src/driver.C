// driver.C

// The best elf reference I've seen is
// "SunOS 5.3 Linker and Libraries Manual"
// References to chapters & page nums are from that book --ari

#include <stdio.h>  // perror
#include <assert.h>
#include <fstream.h>
#include <math.h> // sqrt()
#include <fcntl.h> // O_RDONLY

#include "bb.h"
#include "moduleMgr.h"
#include "util/h/minmax.h"
#include "skips.h"
#include "fillin.h"
#include "util/h/StringPool.h"
#include "util/h/hashFns.h"
#include "kernInstIoctls.h"
#include "driver.h"
#include "SpringBoardHeap.h"

#ifdef sparc_sun_solaris2_7
#include <kvm.h>
#include "section.h" // getSecAndSecHdr()
#endif

#include <algorithm> // stl's sort()
#include <sys/sysmacros.h> // roundup()
#include <iostream>
#include <iomanip>
#include "util/h/binomialDistribStats.h"
#include "util/h/out_streams.h"
#include "fnBlockAndEdgeCounts.h"

pdvector<kptr_t> functionsThatReturnForTheCaller;

pdvector< pair<pdstring,pdstring> > functionNamesThatReturnForTheCaller;

pdvector<oneFnWithKnownNumLevelsByName> functionsWithKnownNumLevelsByName;
pdvector<oneFnWithKnownNumLevelsByAddr> functionsWithKnownNumLevelsByAddr;

static void fillin_functionsWithKnownNumLevelsByAddr() {
   pdvector<oneFnWithKnownNumLevelsByName>::const_iterator iter =
      functionsWithKnownNumLevelsByName.begin();
   pdvector<oneFnWithKnownNumLevelsByName>::const_iterator finish =
      functionsWithKnownNumLevelsByName.end();
   for (; iter != finish; ++iter) {
      const pdstring &modName = iter->modName;
      const pdstring &fnName = iter->fnName;
      const int numLevels = iter->numLevels;

      extern moduleMgr *global_moduleMgr;
      const moduleMgr &theModuleMgr = *global_moduleMgr;
      const loadedModule *mod = theModuleMgr.find(modName);
      if (mod == NULL) {
         cout << "fillin_functionsWithKnownNumLevelsByAddr(): unknown module name \""
              << modName << "\"; ignoring entry" << endl;
         continue;
      }
      
      const char *str = fnName.c_str();
      StaticString fnNameAsStaticString(str);
      
      const pdvector<kptr_t> addrs = mod->find_fn_by_name(fnNameAsStaticString);
      if (addrs.size() == 0) {
         cout << "fillin_functionsWithKnownNumLevelsByAddr(): found module \""
              << modName << "\" but no fn named \"" << str
              << " within it; ignoring entry" << endl;
         continue;
      }
      if (addrs.size() > 1) {
         cout << "fillin_functionsWithKnownNumLevelsByAddr(): found module \""
              << modName << "\" but the name \"" << str
              << " is ambiguous within it; ignoring entry" << endl;
         continue;
      }
      const kptr_t addr = addrs[0];
      
      dout << "fillin_functionsWithKnownNumLevelsByAddr adding entry at "
           << addr2hex(addr) << endl;
      assert(theModuleMgr.findModAndFn(addr, false).second->getEntryAddr() == addr);
      
      oneFnWithKnownNumLevelsByAddr s;
      s.fnEntryAddr = addr;
      assert(numLevels != 0);
      s.numLevels = numLevels;

      functionsWithKnownNumLevelsByAddr += s;
   }

   std::sort(functionsWithKnownNumLevelsByAddr.begin(),
             functionsWithKnownNumLevelsByAddr.end(),
             functionsWithKnownNumLevelsByAddrCmp());

   dout << "fillin_functionsWithKnownNumLevelsByAddr() bye: parsed "
        << functionsWithKnownNumLevelsByAddr.size() << " entries" << endl;
}

typedef dictionary_hash<kptr_t, pdvector< pair< pdstring, StaticString > > > untypedSymbolsType;
typedef function_t::bbid_t bbid_t;

untypedSymbolsType untypedSymbols(addrHash4);
   // key: function address
   // value[xxx].first: name of module (redundant given the key...just for asserting)
   // value[xxx].second: name of the symbol (must be static strings)

static mrtime_t begin_parsing_time;
static mrtime_t fillInFromKernel_totalTime = 0;
static mrtime_t time_parseCFGForFn_total = 0;
static mrtime_t findNextSymbolStartAddr_totalTime = 0;
static mrtime_t addChunks_totalTime = 0;

bool parse1now(const pdstring &modName,
               const function_t *thisFn,
               kptr_t nextSymbolAddr, // could be (kptr_t)-1
               kptr_t maximumNextFnStartAddr, // data_start + data_len
               dictionary_hash<kptr_t,bool> &interProcJumps,
               pdvector<hiddenFn> &hiddenFns,
               bool verbose_fnParse,
               bool verbose_skips) {
   // returns true iff parsed.

   extern kernelDriver *global_kernelDriver; // sorry about this ugliness
   extern moduleMgr *global_moduleMgr;

   if (nextSymbolAddr == (kptr_t)-1)
      nextSymbolAddr = maximumNextFnStartAddr;

   assert(nextSymbolAddr > thisFn->getEntryAddr());

   // Should we not parse this routine?
   assert(thisFn);
   if (bypass_parsing(modName, thisFn->getPrimaryName(),
                      thisFn->getAllAliasNames(),
                      thisFn->getNumAliases())) {
      static unsigned numFoundButNotTryingToParse = 0;
      ++numFoundButNotTryingToParse;
      
      if (verbose_fnParse || verbose_skips) {
         cout << "Found, but not parsing, function with primary name: "
              << thisFn->getPrimaryName() << endl
              << "(marked for bypassing in skips.txt)" << endl;
         cout << "This is skip #" << numFoundButNotTryingToParse << endl;
      }

      global_moduleMgr->doNotParseCFG_forFn(modName, *thisFn);
      
      return false;
   }

   insnVec_t *filledin_insns = insnVec_t::getInsnVec();

   const mrtime_t time_fillinKernelStart = getmrtime();
   
   fillin_kernel(filledin_insns,
                 thisFn->getEntryAddr(),
                 nextSymbolAddr,
                 *global_kernelDriver);

   fillInFromKernel_totalTime += getmrtime() - time_fillinKernelStart;

   // Gather basic blocks of function.

   const mrtime_t time_parseCFGForFn_start = getmrtime();

   try {
      assert(thisFn);
      
      // Create fnCode_t for this function (single chunk)
      typedef fnCode fnCode_t;

      const mrtime_t addChunks_startTime = getmrtime();
      
      fnCode_t theFnCode(fnCode_t::empty);
      theFnCode.addChunk(thisFn->getEntryAddr(), filledin_insns, true);
         // true --> resort now

      addChunks_totalTime += getmrtime() - addChunks_startTime;

      global_moduleMgr->parseCFG_forFn(modName,
                                       *thisFn,
                                       theFnCode,
                                       -1U, // no preFnDataChunk
                                       interProcJumps,
                                       hiddenFns,
                                       false // no strong asserts when adding to call graph (too expensive)
                                       );
         // updates call graph for us, while it's at it.
   }
   catch (function_t::parsefailed) {
      if (verbose_fnParse)
         cout << "EXCEPTION CAUGHT -- function_t::parsefailed " 
              << thisFn->getPrimaryName() << "!" << endl;
      // the sparc_fn will be left in a coherent "shell" state (no bb's, but
      // the correct startAddr & name(s))
      
      // No need to call doNotParseCFG_forFn() here; it will have already been done.

      return false;
   }

   time_parseCFGForFn_total += getmrtime() - time_parseCFGForFn_start;
         
   const fnCode &theFnCode = thisFn->getOrigCode();
   assert(theFnCode.numChunks() == 1);
   const fnCode::const_iterator first_chunk_iter = theFnCode.begin();
   const kptr_t first_chunk_startAddr = first_chunk_iter->startAddr;
   const kptr_t first_chunk_endAddrPlus1 = first_chunk_startAddr + first_chunk_iter->numBytes();

#ifndef NDEBUG
   // Assert that no basic block falls outside the range of "theFnCode"
   function_t::const_iterator bbiter = thisFn->begin();
   function_t::const_iterator bbfinish = thisFn->end();
   for (; bbiter != bbfinish; ++bbiter) {
      const basicblock_t *bb = *bbiter;
      assert(thisFn->containsAddr(bb->getStartAddr()));
      assert(thisFn->containsAddr(bb->getEndAddr()));
   }
#endif

   if (verbose_fnParse) {
      cout << " fn ends at " << addr2hex((first_chunk_endAddrPlus1-1)) << endl;
   }

   if (first_chunk_endAddrPlus1 != nextSymbolAddr &&
       nextSymbolAddr != maximumNextFnStartAddr) {
      if (verbose_fnParse) {
         cout << "note: next symbol is " << addr2hex(nextSymbolAddr)
              << " which leaves a gap between fns of "
              << nextSymbolAddr - first_chunk_endAddrPlus1
              << " bytes." << endl;
      }
   }

   assert(thisFn->getOrigCode().numChunks() > 0);
      // even unparsed fns have some code (when "fried", an unparsed fn has its
      // basic blocks & live reg info fried, but *not* its code chunks)

   return true;
}

// Check to see if a given address is in the middle of another function
// If so, fry this function. Return true if overlaps
bool fry_overlapping(kptr_t addr, moduleMgr &theModuleMgr,
		     const pdvector<basic_sym_info> &allModulesAllSymbols)
{
    pdvector<basic_sym_info>::const_iterator backiter =
	std::lower_bound(allModulesAllSymbols.begin(), 
		         allModulesAllSymbols.end(), 
		         basic_sym_info(addr, 0));
    // Scan backwards to see if addr overlaps with anything
    while (backiter != allModulesAllSymbols.begin()) {
	backiter--;
	// Now we can dereference it even if lower_bound was equal to .end
	assert(backiter->addr < addr);

	pair<pdstring, function_t*> modAndFn = 
	    theModuleMgr.findModAndFnOrNULL(backiter->addr, true);
	function_t *fn = modAndFn.second;
	if (fn != 0 && fn->containsAddr(addr)) {
	    // addr is in the middle of another region of parsed code,
	    // which may or may not correspond to a "real" function
	    if (!fn->isUnparsed()) {
		dout << "Found interprocedural branch to " << addr2hex(addr)
		     << " which is in the middle of function " 
		     << fn->getPrimaryName() 
		     << ", not parsing it and frying the function\n";
		fn->fry(true);
	    }
	    return true;
	}
	if (backiter->addr + backiter->size > addr) {
	    // addr is in the middle of a "real" (non-zero-size) function, 
	    // which may not have been parsed
	    dout << "Found interprocedural branch to " << addr2hex(addr)
		 << " which is in the middle of function at " 
		 << addr2hex(backiter->addr) << ", not parsing it\n";
	    return true;
	}
	if (backiter->size != 0) {
	    // We stop going back at the first "real" function
	    return false;
	}
    }
    return false; // reached the beginning of allModulesAllSymbols
}

void gather(bool /*verbose_memUsage*/,
            bool verbose_skips,
            bool verbose_returningFuncs,
            bool verbose_symbolTable,
            bool verbose_symbolTableTiming, // subset of above
            bool verbose_removeAliases,
            bool verbose_fnParse,
            bool /*verbose_fnParseTiming*/,
            bool /*verbose_summaryFnParse*/,
            bool verbose_hiddenFns,
            kernelDriver &theKernelDriver,
            moduleMgr &theModuleMgr,
            dictionary_hash<kptr_t, bool> &interProcJumps,
            pdvector<hiddenFn> &hiddenFns) {
   ifstream skips_file("skips.txt");
   if (!skips_file) {
      cout << "note: could not open skips.txt file; continuing" << endl;
   }
   else
      parse_skips(skips_file, verbose_skips);

   ifstream fns_returning_for_caller_file("returning_funcs.txt");
   if (!fns_returning_for_caller_file) {
      cout << "note: could not open returning_funcs.txt file; continuing" << endl;
   }
   else
      parse_functions_that_return_for_caller(fns_returning_for_caller_file,
                                             verbose_returningFuncs);

   ifstream fns_with_known_numlevels_file("function_numlevels.txt");
   if (!fns_with_known_numlevels_file) {
      cout << "note: could not open function_numlevels.txt file; continuing" << endl;
   }
   else
      parse_functions_with_known_numlevels(fns_with_known_numlevels_file,
                                           true // verbose
                                           );
   
   mrtime_t time1 = getmrtime();
   
   unsigned long the_size = theKernelDriver.getSymtabStuffSize();

   assert(the_size);
   char *user_buffer = new char[the_size];
   assert(user_buffer);

   theKernelDriver.getSymtabStuff(user_buffer, the_size);

   if (verbose_symbolTableTiming) {
      mrtime_t time2 = getmrtime();
      mrtime_t time_difference = time2 - time1;

      cout << "Time to get kernel symbol table: " 
           << time_difference / 1000 << " usecs" << endl;
   }
   
   unsigned long offset=0;
   unsigned numModules = 0; // initialize to avoid purify UMR
   numModules = *(unsigned char *)(user_buffer + offset);
   offset += sizeof(unsigned char);

   if (verbose_symbolTable)
      cout << "There are " << numModules << " modules" << endl;

   begin_parsing_time = getmrtime();

   dictionary_hash<pdstring, kptr_t> crudeModuleEnds(stringHash);
   
   const moduleMgr &theModuleMgrAsConst = theModuleMgr;

   pdvector<basic_sym_info> allModulesAllSymbols;

   gatherMods(theModuleMgr, theModuleMgrAsConst, crudeModuleEnds,
              interProcJumps, allModulesAllSymbols, 
              functionsThatReturnForTheCaller, functionNamesThatReturnForTheCaller, 
              hiddenFns, findNextSymbolStartAddr_totalTime,
              numModules, user_buffer, offset,
              verbose_symbolTable, verbose_removeAliases, verbose_fnParse, 
              verbose_skips, verbose_returningFuncs);
   
   if(offset != the_size) {
      cerr << "ERROR: buffer offset does not equal size after gathering symbol information from the kernel modules - offset = " 
           << offset << ", size = " << the_size << endl;
      exit(6);
   }
   delete [] user_buffer;

   if (verbose_hiddenFns) {
      cout << "There are " << hiddenFns.size() << " hidden fns" << endl;
      cout << "NOTE: Hidden fns are not yet implemented, but here are the names FYI" << endl;
      for (pdvector<hiddenFn>::const_iterator iter = hiddenFns.begin();
           iter != hiddenFns.end(); ++iter) {
         cout << addr2hex(iter->addr) << " " << iter->name << endl;
      }
   }

   // Before checking interProcJump entries within untypedSymbols[], let us trim
   // untypedSymbols[].  There will be items in untypedSymbols[] that are aliases
   // of existing, successfully parsed, *typed* functions.  For such cases, we should
   // note the alias and remove from untypedSymbols, since the "untyped" symbol
   // isn't so untyped after all.
   // This is necessary to avoid a later (quite strong) assertion statement that
   // everything parsed as a result of finding interProcJumps[] within untypedSymbols[]
   // hasn't yet been parsed.
   untypedSymbolsType::const_iterator ut_syms_iter = untypedSymbols.begin();
   untypedSymbolsType::const_iterator ut_syms_finish = untypedSymbols.end();
   pdvector<kptr_t> untypedSymbolsEntriesToFry;
   for (; ut_syms_iter != ut_syms_finish; ++ut_syms_iter) {
      const kptr_t ut_sym_addr = ut_syms_iter.currkey();

      pair<pdstring, const function_t*> modAndFn =
         theModuleMgrAsConst.findModAndFnOrNULL(ut_sym_addr, true);
      if (modAndFn.second == NULL)
         continue;
      
      const function_t *fn = modAndFn.second;

      const pdvector< pair<pdstring, StaticString> > the_mod_and_names =
         ut_syms_iter.currval();
         // vector of [modname, symname] pairs.  We're only interested in the latter.
      pdvector<StaticString> the_names;
      for (pdvector< pair<pdstring, StaticString> >::const_iterator iter = the_mod_and_names.begin(); iter != the_mod_and_names.end(); ++iter)
         the_names += iter->second;

      theModuleMgr.fnHasSeveralAlternateNames(fn->getEntryAddr(), the_names);

      // can't safely delete from untypedSymbolsEntriesToFry while iterating thru it so:
      untypedSymbolsEntriesToFry += ut_sym_addr;
   }
   for (pdvector<kptr_t>::const_iterator ut_syms_fryiter = untypedSymbolsEntriesToFry.begin(); ut_syms_fryiter != untypedSymbolsEntriesToFry.end(); ++ut_syms_fryiter)
      untypedSymbols.undef(*ut_syms_fryiter);
   
   
    std::sort(allModulesAllSymbols.begin(), allModulesAllSymbols.end());
    pdvector<basic_sym_info>::const_iterator all_mod_end =
	std::unique(allModulesAllSymbols.begin(), allModulesAllSymbols.end());

    if (all_mod_end != allModulesAllSymbols.end())
	allModulesAllSymbols.shrink(all_mod_end - allModulesAllSymbols.begin());

   // Now perhaps parse some symbols with no ELF type (hence we heretofore ignored
   // them) but that must be parsed since they are the target of interprocedural
   // branches/jumps.  We loop through interProcJumps[], looking each one up in
   // untypedSymbols[].
   kptr_t last_dest_addr = 0;
   bool firstTimeThroughInterProcJumps = true;
   bool didAnythingThisTimeThroughInterProcJumps = true;
   while (didAnythingThisTimeThroughInterProcJumps) {
      didAnythingThisTimeThroughInterProcJumps = false;
      dictionary_hash<kptr_t,bool> newBatchOfInterProcJumps(addrHash4);
      pdvector<hiddenFn> newBatchOfHiddenFns;
      
      for (dictionary_hash<kptr_t,bool>::const_iterator iter = interProcJumps.begin();
           iter != interProcJumps.end(); ++iter) {
         const kptr_t addr = iter.currkey();
         if (addr == last_dest_addr)
            // ignore duplicates
            continue;

         last_dest_addr = addr;

#ifndef i386_unknown_linux2_4      
         if (!untypedSymbols.defines(addr))
            continue;
         // Invariant: "addr" has been made to an untyped symbol
#endif

         pair<pdstring, const function_t*> modAndFn = theModuleMgrAsConst.findModAndFnOrNULL(addr, true);
         const function_t *fn = modAndFn.second;

         if (fn != NULL)
            // Of course, if this function already exists, then go no 
            // further in re-parsing it.
            continue;

#ifndef i386_unknown_linux2_4      
         if (firstTimeThroughInterProcJumps) {
            // The first time through interProcJumps, we're assured that the
            // fn is unparsed, since it wasn't in the typed symbols
            assert(fn == NULL && "STT_NOTYPE yet was already parsed?");
         }
#endif

         // Add it to the module (w/o parsing), sort the fns of the module 
         // (so searching is now possible), search for this fn (getting an 
         // iterator, so we can also get the successor function) and its 
         // successor, get raw instructions, and parse. Also, remove the 
         // entry from untypedSymbols[]!

#ifndef i386_unknown_linux2_4
         const pdvector< pair<pdstring, StaticString> > &mod_and_names =
            untypedSymbols.get_and_remove(addr);

	 // Check to see if addr overlaps with something else
	 // We can not parse this case correctly
	 if (fry_overlapping(addr, theModuleMgr, allModulesAllSymbols)) {
	     continue;
	 }

         pdvector<StaticString> names;
         pdstring modName;

         pdvector< pair<pdstring, StaticString> >::const_iterator mn_iter;
         for (mn_iter = mod_and_names.begin(); 
              mn_iter != mod_and_names.end(); ++mn_iter) {
            if (verbose_fnParse)
               cout << "..." << mn_iter->first << "/" 
                    << mn_iter->second.c_str() << endl;
            names += mn_iter->second;
            modName = mn_iter->first;
         }

         fn = theModuleMgr.addFnToModuleButDontParseYet(modName,
                                                        addr,
                                                        names,
                                                        false // don't remove aliases
                                                        );

         // Grab the next highest symbol
         pdvector<basic_sym_info>::const_iterator symiter =
	     std::lower_bound(allModulesAllSymbols.begin(), 
			      allModulesAllSymbols.end(), 
			      basic_sym_info(addr, 0));
         assert(symiter != allModulesAllSymbols.end() && 
		symiter->addr == addr);
         ++symiter;

#else
         // Grab the next highest symbol
         pdvector<basic_sym_info>::const_iterator symiter =
                         std::lower_bound(allModulesAllSymbols.begin(), 
                                          allModulesAllSymbols.end(), 
                                          basic_sym_info(addr, 0));

         if(symiter == allModulesAllSymbols.end())
            symiter--;

         if(symiter->addr == addr)
            // Addr was in the original list of reported symbols, but was
            // not considered for parsing, so we'll do the same
            continue;

         pdstring modName(modAndFn.first);
         if(modAndFn.first == "") {
            modAndFn = theModuleMgrAsConst.findModAndFnOrNULL(addr, false);
            if(modAndFn.first != "") {
               modName = modAndFn.first;
               if(modAndFn.second != NULL) {
                  // Of course, if this function already exists, then go no 
                  // further in re-parsing it.
                  continue;
               }
            }
            else {
               modAndFn = theModuleMgrAsConst.findModAndFnOrNULL(symiter->addr, false);
               if(modAndFn.first == "") {
                  // Ok, last try is symbol before next
                  symiter--;
                  modAndFn = theModuleMgrAsConst.findModAndFnOrNULL(symiter->addr, false);
                  if(modAndFn.first == "") {
                     if(verbose_fnParse)
                        cout << "Couldn't find module and close fn for addr " 
                             << addr2hex(addr) << ", closest fn addr was " 
                             << addr2hex(symiter->addr) << endl;
                     continue;
                  }
               }
               modName = modAndFn.first;
            }
         }
         pdstring fnName("fn_");
         fnName += addr2hex(addr);

         if(verbose_fnParse)
            cerr << "Adding function " << fnName << " to module "
                 << modName << endl;

         fn = theModuleMgr.addFnToModuleButDontParseYet(modName,
                                                        addr,
                                                        fnName,
                                                        false // don't remove aliases
                                                        );

#endif

         didAnythingThisTimeThroughInterProcJumps = true;

         const kptr_t crudeModuleEnd = crudeModuleEnds.get(modName);
         kptr_t nextSymAddr = (symiter == allModulesAllSymbols.end() ? 
			       crudeModuleEnd : symiter->addr);
	 if ((nextSymAddr > crudeModuleEnd) || (nextSymAddr < addr)) {
            nextSymAddr = crudeModuleEnd;
	 }
#if defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4)
         // Since on x86/Linux we aren't guaranteed that the destinations
         // of interprocedural jumps are in the list of reported symbols,
         // we have to also consider the possibility that the next "fn"
         // is another interprocedural jump target that may or may not have
         // already been dealt with
         dictionary_hash<kptr_t,bool>::const_iterator iter2 = interProcJumps.begin();
         for (; iter2 != interProcJumps.end(); ++iter2) {
            const kptr_t addr2 = iter2.currkey();
            if((addr2 > addr) && (addr2 < nextSymAddr)) {
               nextSymAddr = addr2;
            }
         }
#endif

         if (verbose_fnParse)
            cout << "Parsing function @ " << addr2hex(addr)
                 << " that is reached via an interprocedural jump/branch\n";
         
         parse1now(modName,
                   fn,
                   nextSymAddr,
                   crudeModuleEnd,
                   newBatchOfInterProcJumps, // adds to this vector
                   newBatchOfHiddenFns,
                   verbose_fnParse, verbose_skips);
      }
      firstTimeThroughInterProcJumps = false;

      if (newBatchOfInterProcJumps.size() > 0) {
         //interProcJumps += newBatchOfInterProcJumps;
         for (dictionary_hash<kptr_t,bool>::const_iterator newbatchiter = newBatchOfInterProcJumps.begin(); newbatchiter != newBatchOfInterProcJumps.end(); ++newbatchiter) {
            const kptr_t newaddr = newbatchiter.currkey();
            interProcJumps[newaddr] = true; // OK if already exists
         }

         //sort(interProcJumps.begin(), interProcJumps.end());

         newBatchOfInterProcJumps.clear();
      }
   }
}

static void check_ambiguities() {
   extern moduleMgr *global_moduleMgr;
   const moduleMgr &theModuleMgr = *global_moduleMgr;
   
   moduleMgr::const_iterator moditer = theModuleMgr.begin();
   moduleMgr::const_iterator modfinish = theModuleMgr.end();
   for (; moditer != modfinish; ++moditer) {
      const loadedModule *mod = moditer.currval();

      // Create a dictionary of all the fn names within this module.
      // Remove duplicate names.  And don't forget the alias names
      dictionary_hash<pdstring, bool> allNamesThisModule(stringHash);
         // key: fn name
         // value: a dummy

      loadedModule::const_iterator fniter = mod->begin();
      loadedModule::const_iterator fnfinish = mod->end();
      for (; fniter != fnfinish; ++fniter) {
         const function_t *fn = *fniter;
         
         const pdstring primary_name = pdstring(fn->getPrimaryName().c_str());
         
         allNamesThisModule[primary_name] = true;
            // operator[]: don't choke if duplicate

         const unsigned num_aliases = fn->getNumAliases();
         for (unsigned alias_lcv=0; alias_lcv < num_aliases; ++alias_lcv) {
            const StaticString &alias_name_static = fn->getAliasName(alias_lcv);
            const pdstring alias_name = pdstring(alias_name_static.c_str());
            allNamesThisModule[alias_name] = true;
         }
      }

      dictionary_hash<pdstring, bool>::const_iterator name_iter =
         allNamesThisModule.begin();
      dictionary_hash<pdstring, bool>::const_iterator name_finish =
         allNamesThisModule.end();
      for (; name_iter != name_finish; ++name_iter) {
         const pdstring &theFnName = name_iter.currkey();

         pdvector<kptr_t> theFnAddrs =
            mod->find_fn_by_name(StaticString(theFnName.c_str()));

         if (theFnAddrs.size() > 1) {
            dout << "Ambiguity: \"" << mod->getName() << '/' << theFnName
                 << "\" maps to several addresses:" << endl << "   ";
            
            pdvector<kptr_t>::const_iterator addr_iter = theFnAddrs.begin();
            pdvector<kptr_t>::const_iterator addr_finish = theFnAddrs.end();
            for (; addr_iter != addr_finish; ++addr_iter) {
               const kptr_t theAddr = *addr_iter;
               dout << addr2hex(theAddr) << ' ';
            }
            dout << endl;
         }
      }
   }
}

struct parsingTotals {
   unsigned num_code_bytes;
   unsigned num_fns;
   unsigned num_unparsed_fns;
   unsigned num_bbs;
   unsigned num_succs;
   unsigned num_preds;
   
   parsingTotals() {
      num_code_bytes = 0;
      num_fns = 0;
      num_unparsed_fns = 0;
      num_bbs = 0;
      num_succs = 0;
      num_preds = 0;
   }
   void operator+=(const parsingTotals &other) {
      num_code_bytes += other.num_code_bytes;
      num_fns += other.num_fns;
      num_unparsed_fns += other.num_unparsed_fns;
      num_bbs += other.num_bbs;
      num_succs += other.num_succs;
      num_preds += other.num_preds;
   }
};

static void calc_one(const loadedModule *mod,
                     parsingTotals &modTotals) {
   unsigned checkNumFns = mod->numFns();
   
   loadedModule::const_iterator fniter = mod->begin();
   loadedModule::const_iterator fnfinish = mod->end();

   for (; fniter != fnfinish; ++fniter) {
      const function_t *fn = *fniter;

      modTotals.num_fns++;
      
      if (!fn->isUnparsed()) {
         modTotals.num_code_bytes += fn->totalNumBytes_fromBlocks();
         modTotals.num_bbs += fn->numBBs();

         function_t::const_iterator bbiter = fn->begin();
         function_t::const_iterator bbfinish = fn->end();
         for (; bbiter != bbfinish; ++bbiter) {
            const basicblock_t *bb = *bbiter;

            modTotals.num_succs += bb->numSuccessors();
            modTotals.num_preds += bb->numPredecessors();
         }
      }
      else
         modTotals.num_unparsed_fns++;
      
      --checkNumFns;
   }
   assert(checkNumFns == 0);
}

static void print_one(const pdstring &modNameAndDescription, // or "Totals:"
                      parsingTotals &theModTotals) {
   cout << modNameAndDescription << '\t';

   cout << theModTotals.num_code_bytes << '\t';
   cout << theModTotals.num_fns - theModTotals.num_unparsed_fns;
   if (theModTotals.num_unparsed_fns > 0) {
      cout << " (+" << theModTotals.num_unparsed_fns << ")";
   }
   cout << '\t';
   
   cout << theModTotals.num_bbs  << '\t';
   cout << std::setprecision(3)
        << (double)theModTotals.num_bbs /
           (double)(theModTotals.num_fns-theModTotals.num_unparsed_fns)
        << '\t';
   cout << std::setprecision(2)
        << (double)(theModTotals.num_code_bytes / 4) /
           (double)theModTotals.num_bbs << '\t';
   cout << std::setprecision(2)
        << (double)theModTotals.num_succs / (double)theModTotals.num_bbs << '\t';
   cout << std::setprecision(2)
        << (double)theModTotals.num_preds / (double)theModTotals.num_bbs;
   cout << endl;
}

struct deadRegTotals {
   // All numbers of registers are for integer register only.

   unsigned numEntryPtsWithSave;
   unsigned totalNumDeadRegsAtEntryPtsWithSave;
   
   unsigned numEntryPtsNoSave;
   unsigned totalNumDeadRegsAtEntryPtsNoSave;
   
   unsigned numExitPts;
   unsigned totalNumDeadRegsAtExitPts;
   
   unsigned numIndividualPts;
   unsigned totalNumDeadRegsAtIndividualPts;

   deadRegTotals() {
      numEntryPtsWithSave = 0;
      totalNumDeadRegsAtEntryPtsWithSave = 0;

      numEntryPtsNoSave = 0;
      totalNumDeadRegsAtEntryPtsNoSave = 0;

      numExitPts = 0;
      totalNumDeadRegsAtExitPts = 0;

      numIndividualPts = 0;
      totalNumDeadRegsAtIndividualPts = 0;
   }
   void operator+=(const deadRegTotals &other) {
      numEntryPtsWithSave += other.numEntryPtsWithSave;
      totalNumDeadRegsAtEntryPtsWithSave += other.totalNumDeadRegsAtEntryPtsWithSave;

      numEntryPtsNoSave += other.numEntryPtsNoSave;
      totalNumDeadRegsAtEntryPtsNoSave += other.totalNumDeadRegsAtEntryPtsNoSave;

      numExitPts += other.numExitPts;
      totalNumDeadRegsAtExitPts += other.totalNumDeadRegsAtExitPts;

      numIndividualPts += other.numIndividualPts;
      totalNumDeadRegsAtIndividualPts += other.totalNumDeadRegsAtIndividualPts;
   }
};

class sort_modname_by_mod_numbytes {
 private:
   const dictionary_hash<pdstring, unsigned> &modName2NumBytes;
   
 public:
   sort_modname_by_mod_numbytes(const dictionary_hash<pdstring, unsigned> &imodName2NumBytes) :
      modName2NumBytes(imodName2NumBytes) {
   }
   
   bool operator()(const pdstring &modname1, const pdstring &modname2) const {
      // note the ">": we want bigger numbers to appear first
      return (modName2NumBytes.get(modname1) > modName2NumBytes.get(modname2));
   }
};

// ----------------------------------------------------------------------

static void calc_one(const loadedModule *mod,
                     deadRegTotals &modTotals) {
   extern moduleMgr *global_moduleMgr;
   const moduleMgr &theModuleMgr = *global_moduleMgr;
      
   loadedModule::const_iterator fniter = mod->begin();
   loadedModule::const_iterator fnfinish = mod->end();
   
   for (; fniter != fnfinish; ++fniter) {
      const function_t *fn = *fniter;
      if (fn->isUnparsed())
         continue;

      // Function was successfully parsed into CFG.

      const pdvector<kptr_t> entryPoints = fn->getCurrEntryPoints();
      assert(entryPoints.size() == 1);

      regset_t *deadRegsAtEntry =
         theModuleMgr.getDeadRegsBeforeInsnAddr(*fn, entryPoints[0], false);
         // false --> not verbose
         // Note: the result already has sp, fp, g7 stripped off
#ifdef sparc_sun_solaris2_7
      assert(!deadRegsAtEntry->exists(sparc_reg::g0));
#endif
      const unsigned numDeadRegsAtEntry = deadRegsAtEntry->countIntRegs();
      delete deadRegsAtEntry;
#ifdef sparc_sun_solaris2_7
      if (fn->get1OrigInsn(entryPoints[0])->isSave()) {
         ++modTotals.numEntryPtsWithSave;
         modTotals.totalNumDeadRegsAtEntryPtsWithSave += numDeadRegsAtEntry;
      }
      else {
         ++modTotals.numEntryPtsNoSave;
         modTotals.totalNumDeadRegsAtEntryPtsNoSave += numDeadRegsAtEntry;
      }
#endif

      // Exit points:
      const pdvector<kptr_t> exitPoints = fn->getCurrExitPoints();
      pdvector<kptr_t>::const_iterator exitPointIter = exitPoints.begin();
      pdvector<kptr_t>::const_iterator exitPointFinish = exitPoints.end();
      for (; exitPointIter != exitPointFinish; ++exitPointIter) {
         const kptr_t theExitPointAddr = *exitPointIter;
         regset_t *deadRegsAtExitPoint =
            theModuleMgr.getDeadRegsAtPoint(*fn, theExitPointAddr, true);
            // true --> preReturn
#ifdef sparc_sun_solaris2_7
         assert(!deadRegsAtExitPoint->exists(sparc_reg::g0));
#endif

         ++modTotals.numExitPts;
         modTotals.totalNumDeadRegsAtExitPts += deadRegsAtExitPoint->countIntRegs();
	 delete deadRegsAtExitPoint;
      }

      // Individual points:
      function_t::const_iterator bbiter = fn->begin();
      function_t::const_iterator bbfinish = fn->end();
      for (; bbiter != bbfinish; ++bbiter) {
         const basicblock_t *bb = *bbiter;
         
         const kptr_t bbStartAddr = bb->getStartAddr();
         const kptr_t bbFinishAddr = bb->getEndAddrPlus1();
         for (kptr_t insnAddr = bbStartAddr; insnAddr < bbFinishAddr; ) {
            instr_t *i = fn->get1OrigInsn(insnAddr);
            
            regset_t *deadRegsBeforeInsn =
               theModuleMgr.getDeadRegsBeforeInsnAddr(*fn, insnAddr, false);
               // false --> not verbose

            ++modTotals.numIndividualPts; // we won't print this out, though
            modTotals.totalNumDeadRegsAtIndividualPts += deadRegsBeforeInsn->countIntRegs();
            
            insnAddr += i->getNumBytes();
	    delete deadRegsBeforeInsn;
         }
      }
   }
   
}

static void print_one(const pdstring &modNameAndDescription, // or "Totals:"
                      deadRegTotals &theModTotals) {
   cout << modNameAndDescription << '\t';

   ios::fmtflags save_format_flags = cout.flags();

   cout.setf(ios::fixed); // changes the semantics of setprecision(), below
   cout.setf(ios::right); // right pad...
   cout << std::setfill('0'); // ...with zeros
   
   cout << theModTotals.numEntryPtsWithSave << '\t';
   if (theModTotals.numEntryPtsWithSave > 0)
      cout << std::setprecision(1)
           << (double)theModTotals.totalNumDeadRegsAtEntryPtsWithSave /
              (double)theModTotals.numEntryPtsWithSave;
   else
      cout << "--";
   
   cout << '\t';

   cout << theModTotals.numEntryPtsNoSave << '\t';
   if (theModTotals.numEntryPtsNoSave > 0)
      cout << std::setprecision(1)
           << (double)theModTotals.totalNumDeadRegsAtEntryPtsNoSave /
              (double)theModTotals.numEntryPtsNoSave;
   else
      cout << "--";
   
   cout << '\t';

   cout << theModTotals.numExitPts << '\t';
   if (theModTotals.numExitPts > 0)
      cout << std::setprecision(1)
           << (double)theModTotals.totalNumDeadRegsAtExitPts /
              (double)theModTotals.numExitPts;
   else
      cout << "--";
   
   cout << '\t';

   //cout << theModTotals.numIndividualPts << '\t';
   // We don't want to print out # individual pts, right?

   if (theModTotals.numIndividualPts > 0)
      cout << std::setprecision(1)
           << (double)theModTotals.totalNumDeadRegsAtIndividualPts /
              (double)theModTotals.numIndividualPts;
   else
      cout << "--";

   cout << endl;

   cout.flags(save_format_flags);
}

static void print_one_module_callgraph_stats(const pdstring &modNameAndDescription,
                                             const sampleStatistics &s,
                                             bool std_dev_too) {
   cout << modNameAndDescription << '\t';

   ios::fmtflags save_format_flags = cout.flags();
   cout.setf(ios::fixed); // changes the semantics of setprecision() below
   cout.setf(ios::right); // right pad... 
   cout << std::setfill('0'); // ...with zeros
   
   cout << s.getNumSamples() << '\t'; // num fns
   const double mean = s.getMean();
   cout << std::setprecision(1) << mean;
   if (std_dev_too) {
      const double sd = s.getStdDeviationOfTheMean(); // lower than the sample std dev
      cout << " (std dev:";
      cout << std::setprecision(1) << sd;
      cout << ")";
   }
   
   cout << '\t'; // mean & std dev (if any)
   cout << s.getMax();
   cout << endl;

   cout.flags(save_format_flags);
}

static void doStatsOnCallGraph(const pdvector<pdstring> &allModNamesSorted) {
   // Ignores calls to unparsed fns.
   extern moduleMgr *global_moduleMgr;
   const moduleMgr &theModuleMgr = *global_moduleMgr;

   dictionary_hash<pdstring, sampleStatistics> statsByModule(stringHash);
   
   moduleMgr::const_iterator moditer = theModuleMgr.begin();
   moduleMgr::const_iterator modfinish = theModuleMgr.end();
   for (; moditer != modfinish; ++moditer) {
      const loadedModule *mod = *moditer;

      sampleStatistics statsThisMod;
      
      loadedModule::const_iterator fniter = mod->begin();
      loadedModule::const_iterator fnfinish = mod->end();
      for (; fniter != fnfinish; ++fniter) {
         const function_t *fn = *fniter;
         if (fn->isUnparsed())
            continue;
         
         const kptr_t fnEntryAddr = fn->getEntryAddr();

         bool ignore_isSorted;
         const unsigned numTimesThisFnIsCalled =
            theModuleMgr.getCallSitesTo(fnEntryAddr, ignore_isSorted).size();

         statsThisMod.addSample(numTimesThisFnIsCalled); // could be zero
      }

      statsByModule.set(mod->getName(), statsThisMod);
   }

   sampleStatistics total_s;
   dout << "Module\t#Functions Parsed\tAverage # of calls made to a function in this module\tMax # of calls made to a function in this module" << endl;
   pdvector<pdstring>::const_iterator sorted_modname_iter = allModNamesSorted.begin();
   pdvector<pdstring>::const_iterator sorted_modname_finish = allModNamesSorted.end();
   for (; sorted_modname_iter != sorted_modname_finish; ++sorted_modname_iter) {
      const pdstring &modName = *sorted_modname_iter;
      const loadedModule *mod = theModuleMgr.find(modName);
      assert(mod);
      const pdstring &modDescription = mod->getDescription();

      pdstring modNameAndDescription = modName;
      if (modDescription.length() > 0) {
         modNameAndDescription += " (";
         modNameAndDescription += modDescription;
         modNameAndDescription += ")";
      }
      
      const sampleStatistics &s = statsByModule.get(modName);
      print_one_module_callgraph_stats(modNameAndDescription, s,
                                       false);
         // false --> no std deviation

      total_s += s;
   }

   print_one_module_callgraph_stats("Kernel-wide:", total_s,
                                    true);
      // true --> w/std dev too
}

struct indirectCallsStats {
   sampleStatistics regularCallsAndInterProcBranches;
   sampleStatistics unanalyzableCalls;

   void operator+=(const indirectCallsStats &other) {
      regularCallsAndInterProcBranches += other.regularCallsAndInterProcBranches;
      unanalyzableCalls += other.unanalyzableCalls;
   }
};

static void print_one_module_indirect_calls_stats(const pdstring &modNameAndDescription,
                                                  const indirectCallsStats &s) {
   cout << modNameAndDescription << '\t';

   ios::fmtflags save_format_flags = cout.flags();
   cout.setf(ios::fixed); // changes the semantics of setprecision() below
   cout.setf(ios::right); // right pad... 
   cout << std::setfill('0'); // ...with zeros

   assert(s.regularCallsAndInterProcBranches.getNumSamples() ==
          s.unanalyzableCalls.getNumSamples());
   
   cout << s.regularCallsAndInterProcBranches.getNumSamples() << '\t'; // num fns

   // Avg # of direct calls/interproc branches made by a fn in this module:
   cout << std::setprecision(1)
        << s.regularCallsAndInterProcBranches.getMean() << '\t';
   
   // Avg # of indirect calls made by a fn in this module:
   cout << std::setprecision(1)
        << s.unanalyzableCalls.getMean();

   cout << endl;

   cout.flags(save_format_flags);
}

static void doStatsOnIndirectCalls(const pdvector<pdstring> &allModNamesSorted) {
   // Ignores calls to unparsed fns.
   extern moduleMgr *global_moduleMgr;
   const moduleMgr &theModuleMgr = *global_moduleMgr;

   dictionary_hash<pdstring, indirectCallsStats> statsByModule(stringHash);
   
   moduleMgr::const_iterator moditer = theModuleMgr.begin();
   moduleMgr::const_iterator modfinish = theModuleMgr.end();
   for (; moditer != modfinish; ++moditer) {
      const loadedModule *mod = *moditer;

      indirectCallsStats statsThisMod;
      
      loadedModule::const_iterator fniter = mod->begin();
      loadedModule::const_iterator fnfinish = mod->end();
      for (; fniter != fnfinish; ++fniter) {
         const function_t *fn = *fniter;
         if (fn->isUnparsed())
            continue;
         
         pdvector< pair<kptr_t, kptr_t> > regularCalls;
         pdvector< pair<kptr_t, kptr_t> > interProcBranches;
         pdvector<kptr_t> unAnalyzableCallSites; // instruction locations
         
         fn->getCallsMadeBy_AsOrigParsed(regularCalls,
                                         true,
                                         interProcBranches,
                                         true,
                                         unAnalyzableCallSites);

         // regular calls and interproc branches:
         statsThisMod.regularCallsAndInterProcBranches.addSample(regularCalls.size() + interProcBranches.size());

         // unanalyzable calls:
         statsThisMod.unanalyzableCalls.addSample(unAnalyzableCallSites.size());
      }

      statsByModule.set(mod->getName(), statsThisMod);
   }

   indirectCallsStats total_s;
   cout << "Module\t#Functions Parsed\tAverage # of direct calls or indirect branches by a function in this module\tAverage # of indirect calls made by a function in this module" << endl;
   pdvector<pdstring>::const_iterator sorted_modname_iter = allModNamesSorted.begin();
   pdvector<pdstring>::const_iterator sorted_modname_finish = allModNamesSorted.end();
   for (; sorted_modname_iter != sorted_modname_finish; ++sorted_modname_iter) {
      const pdstring &modName = *sorted_modname_iter;
      const loadedModule *mod = theModuleMgr.find(modName);
      assert(mod);
      const pdstring &modDescription = mod->getDescription();

      pdstring modNameAndDescription = modName;
      if (modDescription.length() > 0) {
         modNameAndDescription += " (";
         modNameAndDescription += modDescription;
         modNameAndDescription += ")";
      }
      
      const indirectCallsStats &s = statsByModule.get(modName);
      print_one_module_indirect_calls_stats(modNameAndDescription, s);

      total_s += s;
   }

   print_one_module_indirect_calls_stats("Kernel-wide:", total_s);
}

struct moduleBinomialStats {
   unsigned numFnsParsed;
   unsigned numEdges;
   
   binomialDistribStats wholeFnEdgeStats; // per-fn sample
   binomialDistribStats individualEdgesStatsThisMod; // per-edge sample

   moduleBinomialStats() { // all zeros
      numFnsParsed = 0;
      numEdges = 0;
   }
   moduleBinomialStats(unsigned inumFnsParsed,
                       unsigned inumEdges,
                       const binomialDistribStats &iwholeFnEdgeStats,
                       const binomialDistribStats &iindividualEdgesStatsThisMod) :
      numFnsParsed(inumFnsParsed),
      numEdges(inumEdges),
      wholeFnEdgeStats(iwholeFnEdgeStats),
      individualEdgesStatsThisMod(iindividualEdgesStatsThisMod)
   {
   }

   void operator+=(const moduleBinomialStats &other) {
      numFnsParsed += other.numFnsParsed;
      numEdges += other.numEdges;
      wholeFnEdgeStats += other.wholeFnEdgeStats;
      individualEdgesStatsThisMod += other.individualEdgesStatsThisMod;
   }
};

static void print_one_binomial_avg_and_stddev(ostream &os,
                                              const binomialDistribStats &s,
                                              bool withStdDev) {
   const double p = s.getSampleProbability();
   const double sd = s.getEstimateOfStdDevOfEstimator();
   
   os << std::setprecision(3) << p;
   if (withStdDev)
      os << " (std dev:" << std::setprecision(3) << sd << ")";
}

static void print_one_module_binomial_stats(const pdstring &modNameAndDescription,
                                            const moduleBinomialStats &s,
                                            bool withStdDev) {
   cout << modNameAndDescription << '\t';
   
   ios::fmtflags save_format_flags = cout.flags();
   cout.setf(ios::fixed); // changes the semantics of setprecision() below
   cout.setf(ios::right); // right pad... 
   cout << std::setfill('0'); // ...with zeros

   cout << s.numFnsParsed << '\t';
   cout << s.numEdges << '\t';

   // prob. that any one edge could be calculated:
   print_one_binomial_avg_and_stddev(cout, s.individualEdgesStatsThisMod,
                                     withStdDev);

   cout << '\t';
   
   // prob. that every edge in a fn could be calculated:
   print_one_binomial_avg_and_stddev(cout, s.wholeFnEdgeStats,
                                     withStdDev);

   cout << endl;
   cout.flags(save_format_flags);
}

static void doStatsOnEdgeCountCalculation(const pdvector<pdstring> &allModNamesSorted) {
   // Ignores unparsed fns
   
   extern moduleMgr *global_moduleMgr;
   const moduleMgr &theModuleMgr = *global_moduleMgr;

   dictionary_hash<pdstring, moduleBinomialStats> statsByModule(stringHash);

   unsigned total_num_fns_with_preds_of_entryblock = 0;
   // preds besides the implicit "dummy" entry basic block, that is
   pdvector<const function_t*> fnsWhoseCallCountsCannotBeDetermined;

   moduleMgr::const_iterator moditer = theModuleMgr.begin();
   moduleMgr::const_iterator modfinish = theModuleMgr.end();
   for (; moditer != modfinish; ++moditer) {
      const loadedModule *mod = *moditer;

      unsigned modNumFnsParsed = 0;
      unsigned modNumEdges = 0;

      binomialDistribStats wholeFunctionEdgesStatsThisMod;
      // each sample is a yes/no on whether an entire function could
      // calculate edge counts given only block counts.

      binomialDistribStats individualEdgesStatsThisMod;
      // each sample is a yes/no on whether a given edge could have its
      // count calculated given block counts.

      loadedModule::const_iterator fniter = mod->begin();
      loadedModule::const_iterator fnfinish = mod->end();
      for (; fniter != fnfinish; ++fniter) {
         const function_t *fn = *fniter;
         if (fn->isUnparsed())
            continue;

         ++modNumFnsParsed;

         // In passing block counts, we simply pass 100 for every function.  It doesn't
         // really matter what block count values are passed.
         const fnBlockAndEdgeCounts theCounts(*fn, pdvector<unsigned>(fn->numBBs(), 100));

         modNumEdges += theCounts.getTotalNumEdges();

         const bool allEdgesOfFunctionAreCalculable = theCounts.getNumFudgedEdges()==0;
         wholeFunctionEdgesStatsThisMod.addSample(allEdgesOfFunctionAreCalculable);
         
         const bool fnEntryCountIsCalculable = !theCounts.hadToFudgeEntryBlockCount();
         if (!fnEntryCountIsCalculable)
            fnsWhoseCallCountsCannotBeDetermined += fn;

         const bool fnEntryBlockHasPredecessors =
            fn->getBasicBlockFromId(fn->xgetEntryBB())->numPredecessors()>0;
         if (fnEntryBlockHasPredecessors)
            ++total_num_fns_with_preds_of_entryblock;
         
         const unsigned fnNumEdges = theCounts.getTotalNumEdges();
         unsigned fnNumFudgedEdges = theCounts.getNumFudgedEdges();
         unsigned fnNumUnfudgedEdges = fnNumEdges - fnNumFudgedEdges;
         
         while (fnNumUnfudgedEdges--)
            // success
            individualEdgesStatsThisMod.addSample(true);

         while (fnNumFudgedEdges--)
            // failure
            individualEdgesStatsThisMod.addSample(false);
      }

      statsByModule.set(mod->getName(), moduleBinomialStats(modNumFnsParsed, modNumEdges, wholeFunctionEdgesStatsThisMod, individualEdgesStatsThisMod));
   }

   moduleBinomialStats total_s;
   cout << "Module\t";
   cout << "#Fns parsed\t";
   cout << "#Edges\t";
   cout << "Frac. of individual edges whose counts can be calculated\t";
   cout << "Frac. of fns whose entire edge counts can be calculated";
   cout << endl;

   pdvector<pdstring>::const_iterator sorted_modname_iter = allModNamesSorted.begin();
   pdvector<pdstring>::const_iterator sorted_modname_finish = allModNamesSorted.end();
   for (; sorted_modname_iter != sorted_modname_finish; ++sorted_modname_iter) {
      const pdstring &modName = *sorted_modname_iter;
      const loadedModule *mod = theModuleMgr.find(modName);
      assert(mod);
      const pdstring &modDescription = mod->getDescription();

      pdstring modNameAndDescription = modName;
      if (modDescription.length() > 0) {
         modNameAndDescription += " (";
         modNameAndDescription += modDescription;
         modNameAndDescription += ")";
      }

      const moduleBinomialStats &s = statsByModule.get(modName);
      print_one_module_binomial_stats(modNameAndDescription, s, false);
      // false --> no std deviation (leave that for the summary alone)

      total_s += s;
   }

   print_one_module_binomial_stats("Kernel-wide:",
                                   total_s, true);
   // true --> yes, we want std dev

   cout << endl;
   cout << "Number of functions whose entry block has predecessors: "
        << total_num_fns_with_preds_of_entryblock << endl;
   cout << "Obtaining an accurate invocation count will not be possible for " << fnsWhoseCallCountsCannotBeDetermined.size() << " of these";
   if (fnsWhoseCallCountsCannotBeDetermined.size()) {
      cout << ":" << endl;
      pdvector<const function_t*>::const_iterator iter =
         fnsWhoseCallCountsCannotBeDetermined.begin();
      pdvector<const function_t*>::const_iterator finish =
         fnsWhoseCallCountsCannotBeDetermined.end();
      for (; iter != finish; ++iter) {
         const function_t *fn = *iter;
         cout << "   " << addr2hex(fn->getEntryAddr()) << " "
              << fn->getPrimaryName().c_str() << endl;
      }
   }
   else
      cout << endl;
}

void gather2(bool verbose_memUsage,
             bool verbose_summaryFnParse,
             bool verbose_fnParseTiming, // also passed to gather()
             bool verbose_resolveInterprocJumps,
             bool verbose_bbDFS,
             bool verbose_interProcRegAnalysis,
             bool verbose_interProcRegAnalysisTiming,
             bool verbose_numSuccsPreds,
             bool /*verbose_fnsFiddlingWithPrivRegs*/,
             moduleMgr &theModuleMgr,
             dictionary_hash<kptr_t,bool> &interProcJumps) {
   // finish what gather() started.  The difference is that the input file
   // (kernel symbol table and read-in raw contents of kernel code) is no longer
   // needed.

   if (verbose_resolveInterprocJumps) {
      cout << "Resolving interprocedural jumps now" << endl;
      cout << "(Ensuring they jump to basic block entries)" << endl;
   }

   const moduleMgr &theModuleMgrAsConst = theModuleMgr;
   
   // Loop thru interProcJumps (excluding duplicates)...for each unique entry
   // (which is a destination address within some fn), find the fn it belongs to,
   // and make sure that it has some basic block with that start address.  If it
   // doesn't, then split() the containing basic block as appropriate.

   kptr_t last_dest_addr = (kptr_t)(-1);
   for (dictionary_hash<kptr_t,bool>::const_iterator iter = interProcJumps.begin();
        iter != interProcJumps.end(); ++iter) {
      const kptr_t interProcAddr = iter.currkey();
      
      if (interProcAddr == last_dest_addr)
	 continue;

      if (verbose_resolveInterprocJumps)
         cout << "Resolving interprocedural jump to addr " << addr2hex(interProcAddr)
              << endl;

      theModuleMgr.ensure_start_of_bb(interProcAddr);

      last_dest_addr = interProcAddr;
   }

   if (verbose_resolveInterprocJumps)
      cout << "finished resolving interprocedural jumps" << endl;

   if (verbose_fnParseTiming) {
      mrtime_t time2 = getmrtime();
      mrtime_t time_difference = time2 - begin_parsing_time;

      cout << "to parse functions into basic blocks: " << time_difference << " nsecs"
           << endl;
      cout << "or, " << time_difference / 1000 << " usecs" << endl;
      cout << "or, " << time_difference / 1000000 << " msecs" << endl;
      cout << "or, " << time_difference / 1000000000 << " secs" << endl;
      cout << endl;
      
      cout << "Total time to find next symbol (precursor to fillin from kernel):"
           << endl;
      cout << findNextSymbolStartAddr_totalTime << " nsecs" << endl;
      cout << "or, " << findNextSymbolStartAddr_totalTime / 1000 << " usecs" << endl;
      cout << "or, " << findNextSymbolStartAddr_totalTime / 1000000 << " msecs" << endl;
      cout << "or, " << findNextSymbolStartAddr_totalTime / 1000000000 << " secs" << endl;
      cout << endl;

      cout << "Time to fillin from the kernel:" << endl;
      cout << fillInFromKernel_totalTime << " nsecs" << endl;
      cout << "or, " << fillInFromKernel_totalTime / 1000 << " usecs" << endl;
      cout << "or, " << fillInFromKernel_totalTime / 1000000 << " msecs" << endl;
      cout << "or, " << fillInFromKernel_totalTime / 1000000000 << " secs" << endl;
      cout << endl;
      
      cout << "Total time to add chunks to function (just prior to parsing it)" 
           << endl;
      cout << addChunks_totalTime << " nsecs" << endl;
      cout << "or, " << addChunks_totalTime / 1000 << " usecs" << endl;
      cout << "or, " << addChunks_totalTime / 1000000 << " msecs" << endl;
      cout << "or, " << addChunks_totalTime / 1000000000 << " secs" << endl;
      cout << endl;

      cout << "Time to parse function CFGs (incl function finder, call graph):" << endl;
      cout << time_parseCFGForFn_total << " nsecs" << endl;
      cout << "or, " << time_parseCFGForFn_total / 1000 << " usecs" << endl;
      cout << "or, " << time_parseCFGForFn_total / 1000000 << " msecs" << endl;
      cout << "or, " << time_parseCFGForFn_total / 1000000000 << " secs" << endl;
      cout << endl;

      const mrtime_t parseCFGTime = 
         theModuleMgr.getParseCFGsTotalTime();
      cout << "Time to parse CFGs (but NOT update fn finder, call graph): " << endl;
      cout << parseCFGTime << " nsecs" << endl;
      cout << parseCFGTime/1000 << " usecs" << endl;
      cout << parseCFGTime/1000000 << " msecs" << endl;

      const mrtime_t addToFunctionFinderTime = 
         theModuleMgr.getAddToFunctionFinderTotalTime();
      cout << "Time to add to function finder: " << endl;
      cout << addToFunctionFinderTime << " nsecs" << endl;
      cout << addToFunctionFinderTime/1000 << " usecs" << endl;
      cout << addToFunctionFinderTime/1000000 << " msecs" << endl;

      const mrtime_t addToCallGraphTotalTime = 
         theModuleMgr.getAddToCallGraphTotalTime();
      cout << "Time to update call graph (incl. searching): " << endl;
      cout << addToCallGraphTotalTime << " nsecs" << endl;
      cout << addToCallGraphTotalTime/1000 << " usecs" << endl;
      cout << addToCallGraphTotalTime/1000000 << " msecs" << endl;
   }

   const bbParsingStats &theBBParsingStats = theModuleMgr.getTotalBBParsingStats();

   if (verbose_summaryFnParse) {
      cout << "Some parsing statistics:" << endl;

      cout << "stumbles onto existing block: "
           << theBBParsingStats.numStumblesOntoExistingBlock << endl;
      cout << "interprocedural stumbles [except as fall-thru of a cond branch]: "
           << theBBParsingStats.numInterProceduralStumbles << endl;

      cout << "ret/retl: " << theBBParsingStats.numRetOrRetl << endl;
      cout << "v9 return: " << theBBParsingStats.numV9Returns << endl;
      cout << "done/retry: " << theBBParsingStats.numDoneOrRetry << endl;

      cout << "normal (intra-procedural) branch-always: "
           << theBBParsingStats.numIntraProcBranchAlways << endl;
      cout << "normal (intra-procedural) branch-nevers: "
           << theBBParsingStats.numIntraProcBranchNevers << endl;
      cout << "normal (intra-procedural) conditional branches: "
           << theBBParsingStats.numIntraProcCondBranches << endl;
      cout << "normal (intra-procedural) conditional branches having annul bit: "
           << theBBParsingStats.numIntraProcCondBranchesWithAnnulBit << endl;
      cout << "normal (intra-procedural) conditional branches having annul bit and save/restore in delay: "
           << theBBParsingStats.numIntraProcCondBranchesWithAnnulBitAndSaveOrRestoreInDelay << endl;

      cout << "interprocedural branch-always: "
           << theBBParsingStats.numInterProcBranchAlways << endl;
      cout << "interprocedural conditional branches: "
           << theBBParsingStats.numInterProcCondBranches << endl;
      cout << "interprocedural conditional branches having annul bit: "
           << theBBParsingStats.numInterProcCondBranchesWithAnnulBit << endl;
      cout << "interprocedural conditional branches having annul bit and save/restore in delay: "
           << theBBParsingStats.numInterProcCondBranchesWithAnnulBitAndSaveOrRestoreInDelay << endl;

      cout << "interprocedural stumbles as result of cond branch: "
           << theBBParsingStats.numCondBranchFallThrusThatDoInterProceduralStumble
           << endl;
   
      cout << "jumps to a reg holding fixed addr (intraprocedural): "
           << theBBParsingStats.numJumpToRegFixedAddr_IntraProc << endl;
      cout << "jumps to a reg holding fixed addr (interprocedural): "
           << theBBParsingStats.numJumpToRegFixedAddr_InterProc << endl;

      cout << "jmp insns behaving as retl: "
           << theBBParsingStats.numJmpBehavingAsRetl << endl;

      cout << "normal call (call; delay; not tail optimized): "
           << theBBParsingStats.numTrueCalls_notTail << endl;
      cout << "normal call (jmpl; delay; not tail optimized): "
           << theBBParsingStats.numJmplCalls_notTail << endl;
      cout << "tail calls (call; restore): "
           << theBBParsingStats.numTailCalls_TrueCallRestore << endl;
      cout << "tail calls (jmpl; restore): "
           << theBBParsingStats.numTailCalls_JmplRestore << endl;
      cout << "tail calls (call; write to %o7): "
           << theBBParsingStats.numTailCalls_trueCallThatWritesToO7InDelay << endl;
      cout << "tail calls (jmpl; write to %o7): "
           << theBBParsingStats.numTailCalls_jmplThatWritesToO7InDelay << endl;

      cout << "calls just to obtain PC: "
           << theBBParsingStats.numCallsJustToObtainPC << endl;
      cout << "recursive calls: " 
           << theBBParsingStats.numCallRecursion << endl;
      cout << "hidden intra-proc calls: "
           << theBBParsingStats.numHiddenIntraProcFnCalls << endl;

      cout << "simple jump tables: "
           << theBBParsingStats.numSimpleJumpTables
           << ", averaging "
           << (double)theBBParsingStats.numSimpleJumpTableTotalNonDupEntries / (double)theBBParsingStats.numSimpleJumpTables 
           << " non-duplicate entries" << endl;
      cout << "tagged jump tables: "
           << theBBParsingStats.numTaggedJumpTables
           << ", averaging "
           << (double)theBBParsingStats.numTaggedJumpTableTotalNonDupEntries / (double)theBBParsingStats.numTaggedJumpTables
           << " non-duplicate entries" << endl;
      cout << "non-loading backwards jump tables: "
           << theBBParsingStats.numNonLoadingBackwardsJumpTables
           << ", averaging "
           << (double)theBBParsingStats.numNonLoadingBackwardsJumpTableTotalNonDupEntries / (double)theBBParsingStats.numNonLoadingBackwardsJumpTables
           << " non-duplicate entries" << endl;
      cout << "offset jump tables: "
           << theBBParsingStats.numOffsetJumpTables
           << ", averaging "
           << (double)theBBParsingStats.numOffsetJumpTableTotalNonDupEntries / (double)theBBParsingStats.numOffsetJumpTables
           << endl;

      cout << endl;
   }

   if (verbose_bbDFS)
      cout << "starting 5th pass (dfs ONLY)" << endl;

   unsigned long total_num_bbs = 0;

   unsigned long numCallsAnywhereInBasicBlock = 0;
   unsigned long numCallsInMiddleOfBasicBlock = 0;
      
   unsigned long total_num_successors = 0;
   unsigned long total_num_predecessors = 0;
   unsigned long num_succ_0 = 0;
   unsigned long num_succ_1 = 0;
   unsigned long num_succ_2 = 0;
   unsigned long num_succ_other = 0;

   unsigned long num_pred_0 = 0;
   unsigned long num_pred_1 = 0;
   unsigned long num_pred_2 = 0;
   unsigned long num_pred_3 = 0;
   unsigned long num_pred_4 = 0;
   unsigned long num_pred_other = 0;
   
   unsigned num_fns_starting_with_save = 0;
   unsigned num_fns_starting_without_save = 0;

   pdvector<const function_t*> fnsUsingPrivRegs;

   moduleMgr::const_iterator endmoditer = theModuleMgrAsConst.end();
   for (moduleMgr::const_iterator moditer=theModuleMgrAsConst.begin();
        moditer != endmoditer; ++moditer) {
      const loadedModule *mod = moditer.currval();
      
      // we have a module.  now loop thru the fns of this module.

      if (verbose_bbDFS)
         cout << "basic block DFS marking pass for module "
              << mod->getName() << endl;

      loadedModule::const_iterator lastfniter = mod->end();
      for (loadedModule::const_iterator fniter = mod->begin(); fniter != lastfniter;
           fniter++) {
         const function_t *fn = *fniter;

         if (bypass_parsing(mod->getName(),
                            fn->getPrimaryName(), fn->getAllAliasNames(),
                            fn->getNumAliases())) {
            assert(fn->numBBs() == 0);
            continue;
         }

         if (fn->numBBs() == 0)
            continue;
         
         if (verbose_bbDFS)
            cout << "@" << addr2hex(fn->getEntryAddr()) << " name="
                 << fn->getPrimaryName() << " marking bb's with DFS numbers"
                 << endl;

         const function_t::fnCode_t &theFnCode = fn->getOrigCode();

         // Torture test: make sure that calls to getCurrEntryPoints() and
         // getCurrExitPoints() won't choke.  (There are some pretty strong asserts
         // in those routines, so this is honestly a good torture test.  Better to find
         // out now if something's wrong than wait until we try to instrument a
         // particular instr'n point)
         pdvector<kptr_t> some_points;
         if (!fn->isUnparsed()) {
            some_points = fn->getCurrEntryPoints(); // skip save, if appropriate
            assert(some_points.size() > 0);

            some_points = fn->getCurrExitPoints();
            if (some_points.size() == 0) {
               dout << "Warning: fn "
                    << mod->getName() << '/'
                    << fn->getPrimaryName().c_str()
                    << " at " << addr2hex(fn->getEntryAddr())
                    << " has no exit points."
                    << endl;
               dout << "   (Expected, and OK, if the function has an infinite loop; e.g. a daemon thread)" << endl;
            }
         }

         if (verbose_bbDFS)
            cout << '.' << endl;

#ifdef sparc_sun_solaris2_7   
         // Does this fn start with a save?
         //const insnVec_t &insns = fn->getOrigInsns();
         instr_t::registerUsage *ru = new sparc_instr::sparc_ru();
         fn->get1OrigInsn(fn->getEntryAddr())->getRegisterUsage(ru);
         if (((sparc_instr::sparc_ru*)ru)->sr.isSave)
            num_fns_starting_with_save++;
         else
            num_fns_starting_without_save++;
         delete ru; 
#endif

//           // Experiment: how many functions fiddle with privileged registers, like
//           // traps?
//           if (verbose_fnsFiddlingWithPrivRegs) {
//              for (unsigned insnAddr = fn->getStartAddr();
//                   insnAddr <= fn->getEndAddr(); insnAddr += 4) {
//                 const sparc_instr insn = fn->get1OrigInsn(insnAddr);
//                 if (insn.isRdPr() || insn.isWrPr() || insn.isDone() || insn.isRetry()) {
//                    fnsUsingPrivRegs += fn;
//                    break;
//                 }
//              }
//           }
         
         total_num_bbs += fn->numBBs();

         // iterate thru the bbs of this function, noting how many successors/preds
         // there are, whether we see any v9return insns, bbs ending in a DCTI, 
         function_t::const_iterator bbiter = fn->begin();
         function_t::const_iterator bbfinish = fn->end();
         
         for (; bbiter != bbfinish; ++bbiter) {
            const basicblock_t *bb = *bbiter;

            const function_t::fnCode_t::const_iterator chunk_iter = theFnCode.findChunk(bb->getStartAddr(), false);
            assert(chunk_iter != theFnCode.end());
            const function_t::fnCode_t::codeChunk &theCodeChunk = *chunk_iter;

            if (verbose_numSuccsPreds) {
               const unsigned num_succ = bb->numSuccessors();
               if (num_succ == 0)
                  num_succ_0 ++;
               else if (num_succ == 1)
                  num_succ_1 ++;
               else if (num_succ == 2)
                  num_succ_2 ++;
               else
                  num_succ_other++;
               total_num_successors += num_succ;

               const unsigned num_pred = bb->numPredecessors();
               if (num_pred == 0)
                  num_pred_0++;
               else if (num_pred == 1)
                  num_pred_1++;
               else if (num_pred == 2)
                  num_pred_2++;
               else if (num_pred == 3)
                  num_pred_3++;
               else if (num_pred == 4)
                  num_pred_4++;
               else
                  num_pred_other ++;
               total_num_predecessors += num_pred;
            }

#ifdef sparc_sun_solaris2_7   
            // Experiment: any basic blocks having a v9return instruction?
            for (kptr_t insnAddr = bb->getStartAddr();
                 insnAddr <= bb->getEndAddr(); insnAddr += 4) {
               instr_t *insn = theCodeChunk.get1Insn(insnAddr);
               if (((sparc_instr*)insn)->isV9ReturnInstr())
                  dout << "Note: there is a v9 return instruction at "
                       << addr2hex(insnAddr) << " within " << fn->getPrimaryName()
                       << endl;
            }

            // Experiment: are there any basic blocks whose very last instruction
            // is a conditional branch (not branch-always)?
            // [We exclude the case where the delay insn is a save/restore and
            // the annul bit was set in the branch, because
            // we already know, for other reasons, that we intentionally put
            // save/restore in its own 1-instruction basic block reachable only
            // along the if-taken branch out of the basic block.  By the way,
            // that situation occurs a lot of times in the Solaris 7 kernel
            // (291 times I believe), due to its presense in some asm macros
            // related to auto-loading modules or something like that.]

            const unsigned bbNumInsns =
               theCodeChunk.calcNumInsnsWithinBasicBlock(bb->getStartAddr(),
                                                         bb->getEndAddrPlus1());
            
            kptr_t insnAddr = bb->getStartAddr() + 4*(bbNumInsns-1);
            instr_t *insn = theCodeChunk.get1Insn(insnAddr);
               // not a const reference since the 'complementary experiment'
               // below will re-set this vrble.  No big deal.
            sparc_instr::sparc_cfi cfi;
            insn->getControlFlowInfo(&cfi);
            if (cfi.fields.controlFlowInstruction &&
                cfi.delaySlot != instr_t::dsNone) {
               instr_t *dsInsn =
                  (theCodeChunk.enclosesAddr(insnAddr+4) ?
                   theCodeChunk.get1Insn(insnAddr+4) :
                   (fn->containsAddr(insnAddr+4) ? fn->get1OrigInsn(insnAddr+4) :
                    (theModuleMgrAsConst.findFn(insnAddr+4, false).get1OrigInsn(insnAddr+4))));

               // Exclude from warning in the case where insn is a conditional branch
               // with the annul bit set, and the delay insn is a save or restore.
               if (dsInsn->isSave() || dsInsn->isRestore())
                  // skip the warning
                  ;
               else
                  // do the warning
                  dout << "Note: bb end insn having a delay slot at "
                       << addr2hex(insnAddr) << endl;
            }

            // Complementary experiment: any basic blocks whose 1st insn is a delay
            // slot?  But skip warning in the case where a basic block is a single
            // instruction, save or restore, and where its parent insn is a
            // conditionally annulled branch instruction.  This is because we fully
            // expect to see such cases.
            // 
            // While we're at it, assert that any basic block whose 1st insn is
            // a delay slot is also the ONLY instruction in that block.  This is
            // going to be absolutely necessary when we replace the generic
            // ExitBBId with more specific ones, including interproc branches.
            // (Actually I'm no longer sure that it's required; we may have to go
            // with a more grand solution that duplicates blocks).
            
            insnAddr = bb->getStartAddr();
            insn = theCodeChunk.get1Insn(insnAddr);
            if (insnAddr > fn->getEntryAddr()) {
               const kptr_t previnsnaddr = insnAddr-4;

               if (fn->containsAddr(previnsnaddr)) {
                  instr_t *previnsn = fn->get1OrigInsn(previnsnaddr);
                  // can't check the chunk since it may be in a different one
                  sparc_instr::sparc_cfi cfi;
                  previnsn->getControlFlowInfo(&cfi);
                  if (cfi.fields.controlFlowInstruction &&
                      cfi.delaySlot != instr_t::dsNone) {

                     // "insn" at "insnAddr" is indeed the first insn of a basic
                     // block, while being the delay slot of some other instruction.
                     assert(bb->getNumInsnBytes() == 4 &&
                            "delay slot instruction that begins a basic block MUST be the only instruction in that basic block; otherwise, outgoing edges cannot correctly take into effect the destination of the original control transfer instruction!");
                     
                     // But before warning, check insn for save/restore in its
                     // own basic block.
                     if ((insn->isSave() || insn->isRestore()) &&
                         bb->getNumInsnBytes() == 4)
                        // skip the warning
                        ;
                     else {
                        // do the warning
                        dout << "Note: bb start insn at " << addr2hex(previnsnaddr)
                             << " is a delay slot instruction." << endl;
                        unsigned previnsn_bb_id = fn->searchForBB(previnsnaddr);
                        assert(previnsn_bb_id != -1U);
                        const basicblock_t *previnsn_bb =
                           fn->getBasicBlockFromId(previnsn_bb_id);
                        assert(previnsn_bb->getStartAddr() + previnsn_bb->getNumInsnBytes() == insnAddr);
                     }
                  }
               }
            }

            // Experiment: how many calls (true or jmpl) are there that reside in
            // the middle of a basic block?  This should give us an idea of
            // what the effect of changing bbEndsOnANormalCall
            // (see sparc_controlFlow.C, top of file, for that flag).
            // 
            // While we're here, collect a count of the number of times we see
            // a DCTI in the delay slot of a DCTI.  Hopefully that's really rare.
            // To do this check, look for a current insn being any kind of DCTI
            // (with or without delay slot), and for a prev insn being a DCTI having
            // delaySlot != dsNone.

            for (kptr_t insnAddr = bb->getStartAddr();
                 insnAddr < bb->getEndAddrPlus1(); insnAddr += 4) {
               instr_t *insn = theCodeChunk.get1Insn(insnAddr);
               if (insn->isCall()) {
                  ++numCallsAnywhereInBasicBlock;
                  if (insnAddr < bb->getEndAddrPlus1()-8) // yes, <, not <=
                     ++numCallsInMiddleOfBasicBlock;
               }

               sparc_instr::sparc_cfi cfi;
               insn->getControlFlowInfo(&cfi);
               
               if (cfi.fields.controlFlowInstruction) {
                  const kptr_t prev_insn_addr = insnAddr-4;

                  // Note: the following test isn't perfect; it gives up if prev insn
                  // isn't part of this function.
                  try {
                     instr_t *prev_insn =
                        theCodeChunk.enclosesAddr(prev_insn_addr) ?
                        theCodeChunk.get1Insn(prev_insn_addr) :
                        fn->get1OrigInsn(prev_insn_addr); // could throw fnCode::BadAddr()
                     
                     sparc_instr::sparc_cfi prev_cfi;
                     prev_insn->getControlFlowInfo(&prev_cfi);
                     
                     if (prev_cfi.fields.controlFlowInstruction &&
                         prev_cfi.delaySlot != instr_t::dsNone) {
                        sparc_instr::sparc_cfi fff_cfi;
                        insn->getControlFlowInfo(&fff_cfi);
                        sparc_instr::sparc_cfi fff_prev_cfi;
                        insn->getControlFlowInfo(&fff_prev_cfi);
                        
                        cout << "Found CFI in delay slot of CFI at "
                             << addr2hex(insnAddr) << "!" << endl;
                     }
                  }
                  catch (fnCode::BadAddr) {
                  }
               }
            }
#endif // sparc_sun_solaris            
         } // loop thru bbs of this fn
      } // loop thru fns in this module
   } // loop thru modules

   if (verbose_bbDFS)
      cout << "done with basic block DFS marking." << endl;

   if (verbose_summaryFnParse) {
      cout << "NOTE: there are " << total_num_bbs << " basic blocks in all" << endl;

      cout << "There are " << numCallsInMiddleOfBasicBlock
           << " calls (true call or jmpl <addr>, %o7)" << endl
           << "That take place in the middle of a basic block" << endl;
      cout << "This is out of " << numCallsAnywhereInBasicBlock << " total calls"
           << endl;

//     if (verbose_fnsFiddlingWithPrivRegs) {
//        cout << "There are " << fnsUsingPrivRegs.size() << " functions that fiddle with privileged registers:" << endl;
//        for (pdvector<const function_t*>::const_iterator iter = fnsUsingPrivRegs.begin();
//             iter != fnsUsingPrivRegs.end(); ++iter) {
//           const function_t *fn = *iter;
//           cout << addr2hex(fn->getEntryAddr()) << ' ' << fn->getPrimaryName() << endl;
//        }
//        cout << endl;
//     }
   }
   
   if (verbose_numSuccsPreds) {
      cout << total_num_successors << " successors in all" << endl;
      cout << num_succ_0 << " nodes having 0 successors" << endl;
      cout << num_succ_1 << " nodes having 1 successor" << endl;
      cout << num_succ_2 << " nodes having 2 successors" << endl;
      cout << num_succ_other << " nodes having >2 successors" << endl;
      cout << total_num_predecessors << " predecessors in all" << endl;
      cout << num_pred_0 << " nodes having 0 predecessors" << endl;
      cout << num_pred_1 << " nodes having 1 predecessor" << endl;
      cout << num_pred_2 << " nodes having 2 predecessors" << endl;
      cout << num_pred_3 << " nodes having 3 predecessors" << endl;
      cout << num_pred_4 << " nodes having 4 predecessors" << endl;
      cout << num_pred_other << " nodes having >4 predecessors" << endl;
   }
   
#ifdef sparc_sun_solaris2_7
   dout << "# fns starting with save is " << num_fns_starting_with_save << endl;
   dout << "# fns starting without save is " << num_fns_starting_without_save << endl;
#endif   

   // How many modules, fns, bytes of code
   dout << "There are " << theModuleMgr.numModules() << " modules" << endl;
   unsigned long num_fns = 0;
   unsigned long num_parsed_fns = 0;
   unsigned long num_fn_insnbytes=0;
   unsigned long num_insnbytes_via_bb = 0;
   unsigned long num_bbs_via_iteration = 0;
   
   {
      const moduleMgr &theModuleMgr_const = theModuleMgr;
      for (moduleMgr::const_iterator moditer = theModuleMgr_const.begin();
           moditer != theModuleMgr_const.end(); ++moditer) {
         const loadedModule &mod = *moditer.currval();

         num_fns += mod.numFns(); // does this count fried fns?  Should it?

         for (loadedModule::const_iterator fniter = mod.begin();
              fniter != mod.end(); ++fniter) {
            const function_t *fnptr = *fniter;
            const function_t &fn = *fnptr;

            if (fn.isUnparsed())
               continue;

            num_parsed_fns++;

            const function_t::fnCode_t &fnCode = fn.getOrigCode();
            for (function_t::fnCode_t::const_iterator citer = fnCode.begin();
                 citer != fnCode.end(); ++citer)
               num_fn_insnbytes += citer->theCode->numInsnBytes();

            for (function_t::const_iterator iter = fn.begin();
                 iter != fn.end(); ++iter) {
               const basicblock_t *bb = *iter;
               num_bbs_via_iteration++;
               num_insnbytes_via_bb += bb->getNumInsnBytes();
            }
         }
      }
   }

   if (verbose_summaryFnParse) {
      cout << "There are " << num_bbs_via_iteration << " total bbs via iteration"
           << endl;
      cout << "There are " << num_fns << " total fns" << endl;
      cout << "Of which " << num_parsed_fns << " are parsed" << endl;
      cout << "There are " << num_fn_insnbytes << " total insn bytes" << endl;
      cout << "There are " << num_insnbytes_via_bb << " total insn bytes via bbs"
           << endl << endl;
   }
   
   // Before we start interproc reg analysis, we must update
   // functionsWithKnownNumLevelsByAddr[]
   assert(functionsWithKnownNumLevelsByAddr.size() == 0);
   fillin_functionsWithKnownNumLevelsByAddr();
   theModuleMgr.addFunctionsWithKnownNumLevels(functionsWithKnownNumLevelsByAddr);

   // Interprocedural register analysis
   // Strategy: a depth-first search through all functions (of all modules).

   if (verbose_interProcRegAnalysis)
      cout << "beginning 6th pass: interprocedural register analysis" << endl;

   const mrtime_t time1 = verbose_interProcRegAnalysisTiming ? getmrtime() : 0;

   theModuleMgr.doGlobalRegisterAnalysis(verbose_interProcRegAnalysis);

   if (verbose_interProcRegAnalysisTiming) {
      const mrtime_t time2 = getmrtime();
      const mrtime_t time_difference = time2-time1;

      cout << "to perform register analysis: "
           << time_difference / 1000 << " usecs" << endl;
      cout << "or, " << time_difference / 1000000 << " msecs" << endl;
   }

   // Experiment: see how many ambiguous names there are; in other words, how many
   // names within a given module that point to distinct functions.
   check_ambiguities();
   
   pdvector<pdstring> allModNamesSorted; // may be used below

   extern bool verbose_callGraphStats;
   extern bool verbose_edgeCountCalculation;
   extern bool verbose_indirectCallsStats;

   if (verbose_summaryFnParse || verbose_callGraphStats ||
       verbose_edgeCountCalculation ||
       verbose_indirectCallsStats) {
      // First gather all module names so we can sort them
      dictionary_hash<pdstring,unsigned> modName2NumBytes(stringHash);
      
      moduleMgr::const_iterator moditer = theModuleMgr.begin();
      moduleMgr::const_iterator modfinish = theModuleMgr.end();
      for (; moditer != modfinish; ++moditer) {
         const loadedModule *mod = *moditer;
         const pdstring &modName = mod->getName();
         unsigned modNumBytes = 0;
         
         loadedModule::const_iterator fniter = mod->begin();
         loadedModule::const_iterator fnfinish = mod->end();
         for (; fniter != fnfinish; ++fniter) {
            const function_t *fn = *fniter;
            
            function_t::const_iterator bbiter = fn->begin();
            function_t::const_iterator bbfinish = fn->end();
            for (; bbiter != bbfinish; ++bbiter) {
               const basicblock_t *bb = *bbiter;
               modNumBytes += bb->getNumInsnBytes();
            }
         }
         
         allModNamesSorted += modName;
         modName2NumBytes.set(modName, modNumBytes);
      }

      std::sort(allModNamesSorted.begin(), allModNamesSorted.end(),
           sort_modname_by_mod_numbytes(modName2NumBytes)); // STL's sort
   }
   
   if (verbose_summaryFnParse) {
      // Summary information about each module:
      // modnum, modname, moddescription, #fns, [avg #backedges per fn?],
      // #codebytes, #bbs,
      // avg #insns per bb, avg #successors per bb, avg #preds per bb,

      // Banner:
      cout << "Module\t#bytes\t# fns (+unparsed)\t# bbs\tbbs/fn\tinsns/bb\tsuccs/bb\tpreds/bb" << endl;

      parsingTotals grandTotals;
      
      // Now loop thru modules, sorted by name
      pdvector<pdstring>::const_iterator modNameIter = allModNamesSorted.begin();
      pdvector<pdstring>::const_iterator modNameFinish = allModNamesSorted.end();
      for (; modNameIter != modNameFinish; ++modNameIter) {
         const pdstring &modName = *modNameIter;
         const loadedModule *mod = theModuleMgr.find(modName);
         const pdstring &modDescription = mod->getDescription();
         
         pdstring modNameAndDescription = modName;
         if (modDescription.length() > 0) {
            modNameAndDescription += " (";
            modNameAndDescription += modDescription;
            modNameAndDescription += ")";
         }

         parsingTotals modTotals;
         calc_one(mod, modTotals);

         print_one(modNameAndDescription, modTotals);
         
         grandTotals += modTotals;
      }

      print_one("Totals:", grandTotals);
   }

   if (verbose_memUsage) {
      unsigned memUsageForBasicBlocks=0;
      unsigned memUsageForOrigCode=0;
      unsigned memUsageForFnStructAndSmallOther = 0;
      unsigned memUsageForStringPools=0;
      unsigned forLiveRegAnalysisInfo = 0;
      unsigned forLiveRegAnalysisInfo_1heap = 0;
      unsigned forLiveRegAnalysisInfo_2heap = 0;
      unsigned forLiveRegAnalysisInfo_num1heap = 0;
      unsigned forLiveRegAnalysisInfo_num2heap = 0;
      unsigned forLiveRegAnalysisInfo_numOtherHeap = 0;
      unsigned memUsageForCallGraph = 0;
      
      theModuleMgr.getMemUsage(memUsageForBasicBlocks,
                               memUsageForOrigCode,
                               memUsageForFnStructAndSmallOther,
                               memUsageForStringPools,
                               forLiveRegAnalysisInfo,
                               forLiveRegAnalysisInfo_1heap,
                               forLiveRegAnalysisInfo_2heap,
                               forLiveRegAnalysisInfo_num1heap,
                               forLiveRegAnalysisInfo_num2heap,
                               forLiveRegAnalysisInfo_numOtherHeap,
                               memUsageForCallGraph
                               );

      cout << endl << "Kerninstd Memory Usage Summary: (all figures in bytes)\n";
      cout << "- basic blocks: " << memUsageForBasicBlocks << endl;
      cout << "- function OrigCode structures: " << memUsageForOrigCode << endl;
      cout << "- function structure (& minor sub-structures): "
           << memUsageForFnStructAndSmallOther << endl;
      cout << "- module string pools: " << memUsageForStringPools << endl;
      cout << "- call graph: " << memUsageForCallGraph << endl;
      cout << "- live reg analysis info: " << forLiveRegAnalysisInfo << endl;
      cout << "- live reg analysis info (1 heap only): "
           << forLiveRegAnalysisInfo_1heap << endl;
      cout << "- live reg analysis info (2 heap only): "
           << forLiveRegAnalysisInfo_2heap << endl;
      cout << "- There are " << forLiveRegAnalysisInfo_num1heap 
           << " 1heap entries" << endl;
      cout << "- There are " << forLiveRegAnalysisInfo_num2heap 
           << " 2heap entries" << endl;
      cout << "- There are " << forLiveRegAnalysisInfo_numOtherHeap
           << " other-sized heap entries" << endl;

      // can't check mapped memory usage from the patch heap since it's not
      // mapped into kerninstd space after all.
      
      cout << "Note: does not include any internal fragmentation\n\n";
   }

#ifndef i386_unknown_linux2_4
   extern bool interprocedural_register_analysis;
   if (verbose_summaryFnParse && interprocedural_register_analysis) {
      // Live register analysis summary.  It currently chokes when not doing
      // an interprocedural analysis, hence the check for that flag above.
      // 
      // Broken down by module, we have:
      // a) avg # int regs free at entry (for fns with save as 1st insn)
      // b) avg # int regs free at entry (for fns w/o save as 1st insn)
      // c) avg # int regs free before exit (includes logic for unwinding
      //    tail call, and so forth).  We'll use getExitPoints() then
      //    getDeadRegsAtPoint()
      // d) avg # regs free before each insn (no unwinding logic, just plain)
      //    We'll use moduleMgr's getDeadRegsBeforeInsnAddr() for this one.

      // Banner:
      cout << "Module\t#save entry points\tAvg. dead regs\t#non-save entry points\tAvg. dead regs\t#exit points\tAvg. dead regs\tAvg. dead regs for every insn" << endl;

      deadRegTotals grandTotals;
      
      // Now loop thru modules, sorted by name
      pdvector<pdstring>::const_iterator modNameIter = allModNamesSorted.begin();
      pdvector<pdstring>::const_iterator modNameFinish = allModNamesSorted.end();
      for (; modNameIter != modNameFinish; ++modNameIter) {
         const pdstring &modName = *modNameIter;
         const loadedModule *mod = theModuleMgr.find(modName);
         const pdstring &modDescription = mod->getDescription();
         
         pdstring modNameAndDescription = modName;
         if (modDescription.length() > 0) {
            modNameAndDescription += " (";
            modNameAndDescription += modDescription;
            modNameAndDescription += ")";
         }

         deadRegTotals modTotals;
         calc_one(mod, modTotals);

         print_one(modNameAndDescription, modTotals);
         
         grandTotals += modTotals;
      }

      print_one("Totals:", grandTotals);
   }
#endif // i386_unknown_linux

   // Stats on the call graph:
   if (verbose_callGraphStats)
      doStatsOnCallGraph(allModNamesSorted);

   if (verbose_edgeCountCalculation)
      doStatsOnEdgeCountCalculation(allModNamesSorted);

   if (verbose_indirectCallsStats)
      doStatsOnIndirectCalls(allModNamesSorted);
}
