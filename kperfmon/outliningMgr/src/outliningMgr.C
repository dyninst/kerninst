// outliningMgr.C

#include "outliningMgr.h"
#include "blockCounters.h"

#include "util/h/hashFns.h"
#include "util/h/rope-utils.h" // addr2hex()
#include "kapi.h"

// --------------------

extern kapi_manager kmgr;

outliningMgr::outliningMgr(bool iDoTheReplaceFunction,
                           bool iReplaceFunctionPatchUpCallSitesToo,
                           unsigned iOutliningBlockCountDelay,
                           bool idoChildrenToo,
                           bool iforceOutliningOfAllChildren,
                           kapi_manager::blockPlacementPrefs p,
                           bool iusingFixedDescendants,
                           kapi_manager::ThresholdChoices thPrefs) :
      doTheReplaceFunction(iDoTheReplaceFunction),
      replaceFunctionPatchUpCallSitesToo(iReplaceFunctionPatchUpCallSitesToo),
      outliningBlockCountDelay(iOutliningBlockCountDelay),
      doChildrenToo(idoChildrenToo),
      forceOutliningOfAllChildren(iforceOutliningOfAllChildren),
      usingFixedDescendants(iusingFixedDescendants),
      theHotBlockThresholdPrefs(thPrefs),
      allBlockCounters(addrHash4) // initially an empty dictionary
{
   changeOutliningBlockPlacementTo(p);
}

outliningMgr::~outliningMgr() {
   // Note: instead of doing the following work, we could probably just
   // assert that allBlockCounters[].size() is zero, which is simply asserting
   // that kperfmonIsGoingDownNow() was properly called.
   dictionary_hash<kptr_t, blockCounters*>::const_iterator biter = 
      allBlockCounters.begin();
   dictionary_hash<kptr_t, blockCounters*>::const_iterator bfinish = 
      allBlockCounters.end();
   for (; biter != bfinish; ++biter) {
      blockCounters *theCounters = *biter;
      delete theCounters;
   }
}

void outliningMgr::kperfmonIsGoingDownNow() {
   // Fry the counters
   dictionary_hash<kptr_t, blockCounters*>::const_iterator biter = 
      allBlockCounters.begin();
   dictionary_hash<kptr_t, blockCounters*>::const_iterator bfinish = 
      allBlockCounters.end();
   for (; biter != bfinish; ++biter) {
      blockCounters *theCounters = *biter;
      theCounters->kperfmonIsGoingDownNow(); // emergency cleanup/uninstall
      delete theCounters;
   }
   allBlockCounters.clear();

#ifdef sparc_sun_solaris2_7
   kmgr.unOutlineAll(); // if any
#endif
}

// -------------------- prefs --------------------

void outliningMgr::changeOutliningDoTheReplaceFunctionFlag(bool flag) {
   doTheReplaceFunction = flag;
   cout << "Future groups will be created with doTheReplaceFunction = "
	<< flag << endl;
}

bool outliningMgr::getOutliningDoTheReplaceFunctionFlag() const {
   return doTheReplaceFunction;
}

void outliningMgr::changeOutliningReplaceFunctionPatchUpCallSitesTooFlag(
    bool flag)
{
   replaceFunctionPatchUpCallSitesToo = flag;
   cout << "Future groups will be created with "
	<< "replaceFunctionPatchUpCallSitesToo = " << flag << endl;
}

bool outliningMgr::getOutliningReplaceFunctionPatchUpCallSitesTooFlag() const {
   return replaceFunctionPatchUpCallSitesToo;
}

void outliningMgr::changeOutliningBlockCountDelay(unsigned msecs) {
   outliningBlockCountDelay = msecs;

   dictionary_hash<kptr_t, blockCounters*>::const_iterator iter =
       allBlockCounters.begin();
   dictionary_hash<kptr_t, blockCounters*>::const_iterator finish =
       allBlockCounters.end();
   for (; iter != finish; ++iter) {
      blockCounters *theOutlinedGroup = iter.currval();
      theOutlinedGroup->changeOutliningBlockCountDelay(msecs);
   }
}

unsigned outliningMgr::getOutliningBlockCountDelay() const {
   return outliningBlockCountDelay; // in millisecs
}

void outliningMgr::changeOutlineChildrenTooFlag(bool newVal) {
   doChildrenToo = newVal;

   dictionary_hash<kptr_t, blockCounters*>::const_iterator iter = 
       allBlockCounters.begin();
   dictionary_hash<kptr_t, blockCounters*>::const_iterator finish =
       allBlockCounters.end();
   for (; iter != finish; ++iter) {
      blockCounters *theCounters = iter.currval();
      theCounters->changeOutlineChildrenTooFlag(newVal);
   }
}

bool outliningMgr::getOutlineChildrenTooFlag() const {
   return doChildrenToo;
}

void outliningMgr::changeForceOutliningOfAllChildrenFlag(bool newVal) {
   forceOutliningOfAllChildren = newVal;

   dictionary_hash<kptr_t, blockCounters*>::const_iterator iter =
       allBlockCounters.begin();
   dictionary_hash<kptr_t, blockCounters*>::const_iterator finish =
       allBlockCounters.end();
   for (; iter != finish; ++iter) {
      blockCounters *theCounters = iter.currval();
      theCounters->changeForceOutliningOfAllChildrenFlag(newVal);
   }
}

bool outliningMgr::getForceOutliningOfAllChildrenFlag() const {
   return forceOutliningOfAllChildren;
}

void outliningMgr::
changeOutliningBlockPlacementTo(kapi_manager::blockPlacementPrefs p) {
   theBlockPlacementPrefs = p;
   cout << "Future groups will be created with BlockPlacement = " << p << endl;
}

void outliningMgr::changeOutliningBlockPlacementToOrigSeq() {
   changeOutliningBlockPlacementTo(kapi_manager::origSeq);
}

void outliningMgr::changeOutliningBlockPlacementToChains() {
   changeOutliningBlockPlacementTo(kapi_manager::chains);
}

void outliningMgr::changeUsingFixedDescendantsFlag(bool b) {
   usingFixedDescendants = b;
}

bool outliningMgr::getUsingFixedDescendantsFlag() const {
   return usingFixedDescendants;
}

void outliningMgr::
changeOutliningHotBlocksThreshold(kapi_manager::ThresholdChoices tc) {
   theHotBlockThresholdPrefs = tc;
}

kapi_manager::ThresholdChoices
outliningMgr::getOutliningHotBlocksThreshold() const {
   return theHotBlockThresholdPrefs;
}

// --------------------

// Save the profiling information to disk, so that the profiling stage may be
// skipped the next time
static void dumpBlockCounters(
    bool doTheReplaceFunction,
    bool replaceFunctionPatchUpCallSitesToo,
    kapi_manager::blockPlacementPrefs kapiPlacementPrefs,
    kapi_manager::ThresholdChoices kapiThresholdPrefs,
    kptr_t rootFnAddr,
    const dictionary_hash<kptr_t, kapi_vector<unsigned> > &fnBlockCounters,
    const kapi_vector<kptr_t> forceIncludeDescendants)
{
    const unsigned maxlen = 255;
    char modName[maxlen];
    char funcName[maxlen];
    kapi_module kRootMod;
    kapi_function kRootFunc;
    if (kmgr.findModuleAndFunctionByAddress(rootFnAddr,
					    &kRootMod, &kRootFunc) < 0) {
	assert(false && "module or function not found");
    }
    if (kRootMod.getName(modName, maxlen) < 0) {
	assert(false && "Module name is too long");
    }
    if (kRootFunc.getName(funcName, maxlen) < 0) {
	assert(false && "Function name is too long");
    }

    FILE *f = fopen("dumpBlockCounters.txt", "wt");
    assert(f);
    fprintf(f, "doTheReplaceFunction=%d\nreplaceFunctionPatchUpCallSites=%d\n"
	    "kapiPlacementPrefs=%d\nkapiThresholdPrefs=%d\n"
	    "rootModName=%s rootFuncName=%s rootFnAddr=0x%llx\n",
	    doTheReplaceFunction, replaceFunctionPatchUpCallSitesToo,
	    kapiPlacementPrefs, kapiThresholdPrefs,
	    modName, funcName, rootFnAddr);
    
    dictionary_hash<kptr_t, kapi_vector<unsigned> >::const_iterator iter = 
	fnBlockCounters.begin();
    for (; iter != fnBlockCounters.end(); iter++) {
	kptr_t fnAddr = iter.currkey();
	kapi_module kmod;
	kapi_function kfunc;
	if (kmgr.findModuleAndFunctionByAddress(fnAddr, &kmod, &kfunc) < 0) {
	    assert(false && "Module or Function not found");
	}
	if (kmod.getName(modName, maxlen) < 0) {
	    assert(false && "Module name is too long");
	}
	if (kfunc.getName(funcName, maxlen) < 0) {
	    assert(false && "Function name is too long");
	}

	const kapi_vector<unsigned> &kCounts = iter.currval();
	const unsigned numBlocks = kCounts.size();
	fprintf(f, "modName=%s funcName=%s fnAddr=0x%llx numBlocks=%u\n",
		modName, funcName, fnAddr, numBlocks);
	kapi_vector<unsigned>::const_iterator icount = kCounts.begin();
	for (; icount != kCounts.end(); icount++) {
	    fprintf(f, "%u ", *icount);
	}
	fprintf(f, "\n");
    }
    fprintf(f, "numDescendants=%u\n", forceIncludeDescendants.size());
    kapi_vector<kptr_t>::const_iterator iforce =
	forceIncludeDescendants.begin();
    for (; iforce != forceIncludeDescendants.end(); iforce++) {
	fprintf(f, "0x%llx ", *iforce);
    }
    fprintf(f, "\n");
    fclose(f);
}

// Will be called when all block counts for a group of functions have
// been received
void outliningMgr::doneCollectingBlockCounts(
    kptr_t fnEntryAddr,
    const dictionary_hash<kptr_t, outlining1FnCounts*> &fnBlockCounters,
    const kapi_vector<kptr_t> forceIncludeDescendants)
{
    cout << "Start collecting code objects...\n";

    // Convert fnBlockCounters to the form acceptable by KAPI
    dictionary_hash<kptr_t, kapi_vector<unsigned> > blockCounters(addrHash4);
    dictionary_hash<kptr_t, outlining1FnCounts*>::const_iterator ifunc = 
	fnBlockCounters.begin();
    for (; ifunc != fnBlockCounters.end(); ifunc++) {
	const pdvector<unsigned> &bbCounts =
	    ifunc.currval()->getAllBlockCounts();
	kapi_vector<unsigned> kCounts;
	pdvector<unsigned>::const_iterator iblock = bbCounts.begin();
	for (; iblock != bbCounts.end(); iblock++) {
	    kCounts.push_back(*iblock);
	}

	kptr_t fnAddr = ifunc.currkey();
	blockCounters.set(fnAddr, kCounts);
    }
    dumpBlockCounters(doTheReplaceFunction, replaceFunctionPatchUpCallSitesToo,
		      theBlockPlacementPrefs, theHotBlockThresholdPrefs,
		      fnEntryAddr, blockCounters,
		      forceIncludeDescendants);

#ifdef sparc_sun_solaris2_7   
    kmgr.asyncOutlineGroup(doTheReplaceFunction,
			   replaceFunctionPatchUpCallSitesToo,
			   theBlockPlacementPrefs,
			   theHotBlockThresholdPrefs,
			   fnEntryAddr, blockCounters,
			   forceIncludeDescendants);
#endif
}

void outliningMgr::asyncOutlineFn(
    const pdstring &modName, const kapi_function &fn,
    const pdvector< pair<pdstring, kapi_function> > &fixedDescendants,
    const pdvector< pair<pdstring, kapi_function> > &forceIncludeDescendants,
    const pdvector< pair<pdstring, kapi_function> > &forceExcludeDescendants)
{
   // The process of outlining begins with collecting basic block counts for
   // the "root" function.

   if (fn.isUnparsed()) {
      cout << "Sorry, cannot outline fn at " << addr2hex(fn.getEntryAddr())
           << endl
           << " since it was not successfully parsed into basic blocks."
           << endl;
      return;
   }

   if (allBlockCounters.defines(fn.getEntryAddr())) {
      cout << "That function is already being outlined.  If you want to redo"
           << endl
           << "the outlining, first \"unOutline\" that function." << endl;
      return;
   }
   
   blockCounters *theCounters = new blockCounters(outliningBlockCountDelay, 
						  doChildrenToo,
						  forceOutliningOfAllChildren,
						  modName, fn,
						  usingFixedDescendants,
						  fixedDescendants,
						  forceIncludeDescendants,
						  forceExcludeDescendants,
						  *this);
   assert(theCounters);

   allBlockCounters.set(fn.getEntryAddr(), theCounters);

   cout << "Welcome to outlining; outliningBlockCountDelay is "
        << outliningBlockCountDelay << " msecs" << endl;

   theCounters->asyncCollectBlockCounts();
}

// Read the profile from disk and outline the functions specified there
void outliningMgr::asyncOutlineUsingProfile(const char *profileFileName)
{
    int iDoTheReplaceFunction;
    bool doTheReplaceFunction;
    int iReplaceFunctionPatchUpCallSitesToo;
    bool replaceFunctionPatchUpCallSitesToo;
    kapi_manager::blockPlacementPrefs kapiPlacementPrefs;
    kapi_manager::ThresholdChoices kapiThresholdPrefs;
    kptr_t rootFnAddr;
    dictionary_hash<kptr_t, kapi_vector<unsigned> > blockCounters(addrHash4);
    kapi_vector<kptr_t> forceIncludeDescendants;
    unsigned numDescendants;

    const unsigned maxlen = 255;
    char modName[maxlen];
    char funcName[maxlen];
    kapi_module kRootMod;
    kapi_function kRootFunc;

    FILE *f = fopen(profileFileName, "rt");
    assert(f);
    if (fscanf(f,
	       "doTheReplaceFunction=%d\nreplaceFunctionPatchUpCallSites=%d\n"
	       "kapiPlacementPrefs=%d\nkapiThresholdPrefs=%d\n"
	       "rootModName=%s rootFuncName=%s rootFnAddr=0x%llx\n",
	       &iDoTheReplaceFunction, &iReplaceFunctionPatchUpCallSitesToo,
	       &kapiPlacementPrefs, &kapiThresholdPrefs,
	       modName, funcName, &rootFnAddr) != 7) {
	assert(false && "profile format error");
    }
    doTheReplaceFunction = (iDoTheReplaceFunction == 1);
    replaceFunctionPatchUpCallSitesToo = 
	(iReplaceFunctionPatchUpCallSitesToo == 1);
    if (kmgr.findModule(modName, &kRootMod) < 0) {
	assert(false && "Module not found");
    }
    if (kRootMod.findFunction(funcName, &kRootFunc) < 0) {
	assert(false && "Function not found");
    }
    if (kRootFunc.getEntryAddr() != rootFnAddr) {
	cout << "Old (" << addr2hex(rootFnAddr) << ") and new ("
	     << addr2hex(kRootFunc.getEntryAddr()) << ") addrs do not match\n";
	rootFnAddr = kRootFunc.getEntryAddr();
    }
    kptr_t fnAddr;
    unsigned numBlocks;
    
    while (fscanf(f, "modName=%s funcName=%s fnAddr=0x%llx numBlocks=%u\n",
		  modName, funcName, &fnAddr, &numBlocks) == 4) {
	kapi_module kmod;
	if (kmgr.findModule(modName, &kmod) < 0) {
	    assert(false && "Module not found");
	}
	kapi_function kfunc;
	if (kmod.findFunction(funcName, &kfunc) < 0) {
	    assert(false && "Function not found");
	}
	if (kfunc.getEntryAddr() != fnAddr) {
	    cout << "Old (" << addr2hex(fnAddr) << ") and new ("
		 << addr2hex(kfunc.getEntryAddr()) << ") addrs do not match\n";
	    fnAddr = kfunc.getEntryAddr();
	}
	kapi_vector<unsigned> kCounts;

	unsigned actualNumBlocks = kfunc.getNumBasicBlocks();
	// It could be that the profile of this function does not match
	// the kernel (e.g., function has been recompiled since then)
	// We'll try to detect this condition and zero-out the counts.
	if (numBlocks != actualNumBlocks) {
	    cout << "Function " << addr2hex(kfunc.getEntryAddr()) 
		 << " has been modified since profiling, ignoring its counts"
		 << endl;
	    const unsigned maxlen = 4096;
	    char buffer[maxlen];
	    if (fgets(buffer, maxlen, f) == NULL) {
		assert(false && "profile format error");
	    }
	    for (unsigned i=0; i<actualNumBlocks; i++) {
		kCounts.push_back(0);
	    }
	}
	else {
	    for (unsigned i=0; i<numBlocks; i++) {
		unsigned count;
		if (fscanf(f, "%u ", &count) != 1) {
		    assert(false && "profile format error");
		}
		kCounts.push_back(count);
	    }
	}
	blockCounters.set(fnAddr, kCounts);
    }

    if (fscanf(f, "numDescendants=%u\n", &numDescendants) != 1) {
	assert(false && "profile format error");
    }
    for (unsigned i=0; i<numDescendants; i++) {
	kptr_t val;
	if (fscanf(f, "0x%llx ", &val) != 1) {
	    assert(false && "profile format error");
	}
	forceIncludeDescendants.push_back(val);
    }
    fclose(f);

#ifdef sparc_sun_solaris2_7   
    kmgr.asyncOutlineGroup(doTheReplaceFunction,
			   replaceFunctionPatchUpCallSitesToo,
			   kapiPlacementPrefs,
			   kapiThresholdPrefs,
			   rootFnAddr, blockCounters,
			   forceIncludeDescendants);
#endif

#if 0
    // Free the allocated memory
    delete[] forceIncludeDescendants;

    dictionary_hash<kptr_t, const unsigned *>::const_iterator citer =
	pBlockCounters->begin();
    for (; citer != pBlockCounters->end(); citer++) {
	delete[] citer.currval();
    }
    delete pBlockCounters;
#endif
}




// ----------------------------------------------------------------------

//  relocatableCode outliningMgr::
//  emitBestOutliningCode(const function_t &fn,
//                        outlinedGroup *theOutlinedGroup) {
//     extern instrumenter *theGlobalInstrumenter;

//     cout << "Stats for outlining of " << fn.getPrimaryName().c_str() << ':' << endl;

//     const abi &theKernelABI = theGlobalInstrumenter->getKernelABI();

//     const unsigned numBytesForPreFnData =
//        theOutlinedGroup->exactNumBytesForPreFunctionData();
   
//     tempBufferEmitter em_origseq(theKernelABI);
//     theOutlinedGroup->outlineCoupled_usingOrigSeq(em_origseq);
//     assert(em_origseq.numChunks() == 3); // pre-fn-data, inlined stuff, outlined stuff
//     assert(em_origseq.getChunk(0).numInsnBytes() == numBytesForPreFnData);
//        // 0 --> chunk 0 is pre-fn data

//     const unsigned inlinedPortionNumInsnBytes_usingOrigSeq = 
//        em_origseq.getChunk(1).numInsnBytes();

//     const unsigned inlinedPortionNumICacheBlocks_usingOrigSeq =
//        inlinedPortionNumInsnBytes_usingOrigSeq / 32 + (inlinedPortionNumInsnBytes_usingOrigSeq % 32 != 0 ? 1 : 0);
   
//     cout << "UsingOrigSeq: Emitted " << inlinedPortionNumInsnBytes_usingOrigSeq
//          << " inlined bytes in " << inlinedPortionNumICacheBlocks_usingOrigSeq 
//          << " i-$ blocks" << endl << "("
//          << 100.0*inlinedPortionNumInsnBytes_usingOrigSeq / fn.getOrigCode().totalNumBytes()
//          << "% of orig code)" << endl;
   
//     // ----------------------------------------

//     tempBufferEmitter em_longestpaths(theKernelABI);

//     theOutlinedGroup->outlineCoupled_usingLongestPaths(em_longestpaths);
//     assert(em_longestpaths.numChunks() == 3);
//     assert(em_longestpaths.getChunk(0).numInsnBytes() == numBytesForPreFnData);

//     const unsigned inlinedPortionNumInsnBytes_usingLongestPaths = 
//        em_longestpaths.getChunk(1).numInsnBytes();

//     const unsigned inlinedPortionNumICacheBlocks_usingLongestPaths = 
//        inlinedPortionNumInsnBytes_usingLongestPaths / 32 + (inlinedPortionNumInsnBytes_usingLongestPaths % 32 != 0 ? 1 : 0);
   
//     cout << "UsingLongestPaths: Emitted " << inlinedPortionNumInsnBytes_usingLongestPaths
//          << " inlined bytes in " << inlinedPortionNumICacheBlocks_usingLongestPaths
//          << " i-$ blocks" << endl << "("
//          << 100.0*inlinedPortionNumInsnBytes_usingLongestPaths / fn.getOrigCode().totalNumBytes()
//          << "% of orig code)" << endl;
   
//     // ----------------------------------------

//     tempBufferEmitter em_greedyhp(theKernelABI);
   
//     theOutlinedGroup->outlineCoupled_usingGreedyHottestPaths(em_greedyhp);
//     assert(em_greedyhp.numChunks() == 3);
//     assert(em_greedyhp.getChunk(0).numInsnBytes() == numBytesForPreFnData);

//     const unsigned inlinedPortionNumInsnBytes_usingGreedyHottestPaths =
//        em_greedyhp.getChunk(1).numInsnBytes();

//     const unsigned inlinedPortionNumICacheBlocks_usingGreedyHottestPaths =
//        inlinedPortionNumInsnBytes_usingGreedyHottestPaths / 32 + (inlinedPortionNumInsnBytes_usingGreedyHottestPaths % 32 != 0 ? 1 : 0);
   
//     cout << "UsingGreedyHottestPaths: Emitted "
//          << inlinedPortionNumInsnBytes_usingGreedyHottestPaths
//          << " inlined bytes in "
//          << inlinedPortionNumICacheBlocks_usingGreedyHottestPaths << " i-$ blocks"
//          << endl << "("
//          << 100.0*inlinedPortionNumInsnBytes_usingGreedyHottestPaths / fn.getOrigCode().totalNumBytes()
//          << "% of orig code)" << endl;
   
//     // ----------------------------------------

//     // Who won?
//     if (inlinedPortionNumICacheBlocks_usingOrigSeq < inlinedPortionNumICacheBlocks_usingLongestPaths && inlinedPortionNumICacheBlocks_usingOrigSeq < inlinedPortionNumICacheBlocks_usingGreedyHottestPaths) {
//        cout << "And the winner is UsingOrigSeq" << endl;

//        return relocatableCode(em_origseq);
//     }
//     else if (inlinedPortionNumICacheBlocks_usingLongestPaths < inlinedPortionNumICacheBlocks_usingGreedyHottestPaths) {
//        cout << "And the winner is UsingLongestPaths" << endl;

//        return relocatableCode(em_longestpaths);
//     }
//     else {
//        cout << "And the winner is UsingGreedyHottestPaths" << endl;

//        return relocatableCode(em_greedyhp);
//     }
//  }

// ----------------------------------------------------------------------

void outliningMgr::unOutlineGroup(const pdstring &/*modName*/,
				  const kapi_function &fn)
{
   const kptr_t rootFnEntryAddr = fn.getEntryAddr();

   // First, fry the block-count instrumentation
   blockCounters *theCounters = NULL;
   if (allBlockCounters.find(rootFnEntryAddr, theCounters)) {
       assert(theCounters != NULL);
       theCounters->kperfmonIsGoingDownNow();
       delete theCounters;
       allBlockCounters.get_and_remove(rootFnEntryAddr);
   }
   
   // Then, fry the already-outlined code
   cout << "FIXME: disable all CMFs that a user may have enabled for the"
	<< " outlined function\n";

#ifdef sparc_sun_solaris2_7   
   kmgr.unOutlineGroup(rootFnEntryAddr);
#endif
}
