// main.C
// KernInstd

#include <unistd.h> // getuid() & co.
#include <stdio.h> // perror()
#include <signal.h> // sigaction
#include <sched.h> // sched_setscheduler
#include <inttypes.h>
#include <sys/stat.h>
#include <fcntl.h> // open()
#include <iostream.h>

#include "util/h/minmax.h"
#include "common/h/Ident.h"
#include "util/h/hashFns.h"
#include "util/h/out_streams.h"

#include "driver.h" // doStuff()
#include "patchHeap.h"
#include "launchSite.h"
#include "mappedKernelSymbols.h"
#include "instrumentationMgr.h"
#include "loop.h" // main loop
#include "machineInfo.h"

#ifdef sparc_sun_solaris2_7
#include <sys/processor.h> // processor_info()
#include <sys/systeminfo.h> // sysinfo()
#include "sparcv8plus_abi.h"
#include "sparcv9_abi.h"
#include "cpcInterface.h"
#elif defined(i386_unknown_linux2_4)
#include "x86_abi.h"
#elif defined(ppc64_unknown_linux2_4)
#include "power64_abi.h"
#endif

const char *global_infilename;
moduleMgr *global_moduleMgr;
kernelDriver *global_kernelDriver;
instrumentationMgr *global_instrumentationMgr;
machineInfo *global_machineInfo;
mappedKernelSymbols *global_mappedKernelSymbols;
patchHeap *global_patchHeap;
SpringBoardHeap *global_springboardHeap;
abi *theKernelABI; // as determined by the strongest among isalist(5)
abi *theKerninstdABI; // pretty much hard-coded; I don't know how to check this

#ifdef sparc_sun_solaris2_7
cpcInterface *global_cpcInterface;
#endif

static const function_t *
findFunctionInModule(const loadedModule &mod, const pdstring &fnName,
                     function_t* /* dummy arg for template resolving */) {
   for (loadedModule::const_iterator fniter = mod.begin();
        fniter != mod.end(); fniter++) {
      const function_t *fn = *fniter;
      if (0==strcmp(fn->getPrimaryName().c_str(), fnName.c_str()))
         return fn;
      for (unsigned lcv=0; lcv < fn->getNumAliases(); lcv++)
         if (0==strcmp(fn->getAliasName(lcv).c_str(), fnName.c_str()))
            return fn;
   }
   return NULL;
}

static const function_t *
findFunction(const pdstring &modName,
             const pdstring &fnName,
             const function_t * /* dummy */) {
   const moduleMgr &mm = *global_moduleMgr;
   const loadedModule *mod = mm.find(modName);
   if (mod == NULL)
      return NULL;

   return findFunctionInModule(*mod, fnName, (function_t*)NULL);
}

static unsigned addFnToSpringboardHeap(const pdstring &modName,
                                       const function_t &fn,
                                       bool verbose_springboardHeap) {
   unsigned result = 0;
   
   const function_t::fnCode_t &theFnCode = fn.getOrigCode();
   
   for (function_t::fnCode_t::const_iterator iter = theFnCode.begin();
        iter != theFnCode.end(); ++iter) {
      const kptr_t startAddr = iter->startAddr;
      const unsigned nbytes = iter->theCode->numInsnBytes();
      
      if (nbytes >= springboardChunk::sizeOfSpringBoardItem()) {
         global_springboardHeap->addChunk(startAddr, nbytes);
         if (verbose_springboardHeap)
            dout << "Adding " << modName << '/' << fn.getPrimaryName().c_str()
                 << " to sb heap: " << nbytes << " bytes" << endl;
         result += nbytes;
      }
   }

   return result;
}

static
void firstTimeParseRoutine(bool verbose_memUsage,
                           bool verbose_skips,
                           bool verbose_returningFuncs,
                           bool verbose_symbolTable,
                           bool verbose_symbolTableTiming, // subset of above
                           bool verbose_removeAliases,
                           bool verbose_summaryFnParse,
                           bool verbose_fnParse,
                           bool verbose_fnParseTiming,
                           bool verbose_hiddenFns,
                           bool verbose_resolveInterprocJumps,
                           bool verbose_bbDFS,
                           bool verbose_interProcRegAnalysis,
                           bool verbose_interProcRegAnalysisTiming,
                           bool verbose_numSuccsPreds,
                           bool verbose_fnsFiddlingWithPrivRegs,
                           bool verbose_springboardHeap
                           ) {
   assert(global_moduleMgr);

   dictionary_hash<kptr_t, bool> interProcPoints(addrHash4);
      // the key is what matters; the value field is a dummy placeholder.
   pdvector<hiddenFn> hiddenFns;

   gather(verbose_memUsage,
          verbose_skips,
          verbose_returningFuncs,
          verbose_symbolTable,
          verbose_symbolTableTiming, // subset of above
          verbose_removeAliases,
          verbose_fnParse,
          verbose_fnParseTiming,
          verbose_summaryFnParse,
          verbose_hiddenFns,
          *global_kernelDriver,
          *global_moduleMgr,
          interProcPoints,
          hiddenFns);

   gather2(verbose_memUsage,
           verbose_summaryFnParse,
           verbose_fnParseTiming,
           verbose_resolveInterprocJumps,
           verbose_bbDFS,
           verbose_interProcRegAnalysis,
           verbose_interProcRegAnalysisTiming,
           verbose_numSuccsPreds,
           verbose_fnsFiddlingWithPrivRegs,
           *global_moduleMgr, interProcPoints);


   assert(global_springboardHeap != NULL);
//   assert(global_springboardHeap->numChunks() == 0);

#ifdef sparc_sun_solaris2_7

   unsigned num_springboard_bytes_nucleus = 0;

   const function_t *fn = NULL;
   fn = findFunction("genunix", "main", (function_t*)NULL);
   assert(fn);
   num_springboard_bytes_nucleus += addFnToSpringboardHeap("genunix", *fn,
                                                           verbose_springboardHeap);

   fn = findFunction("unix", "_start", (function_t*)NULL);
   assert(fn);
   num_springboard_bytes_nucleus += addFnToSpringboardHeap("unix", *fn,
                                                           verbose_springboardHeap);

   unsigned num_springboard_bytes_beyondnucleus = 0;
   const moduleMgr &mm = *global_moduleMgr;

   for (moduleMgr::const_iterator moditer = mm.begin(); // OK no longer skipping nucleus???
        moditer != mm.end(); ++moditer) {
      const loadedModule *mod = moditer.currval();

      fn = findFunctionInModule(*mod, "_init", (function_t*)NULL);
      if (fn == NULL) {
         // yikes; some module doesn't have an _init.  This doesn't seem to
         // occur in 2.5.1, but in module platmod, at least, it happens on 2.6
         if (verbose_springboardHeap)
            cout << "WARNING: module " << mod->getName()
                 << " does not seem to have an _init" << endl;
      }
      else {
         assert(fn);

         num_springboard_bytes_beyondnucleus += addFnToSpringboardHeap(mod->getName(),
                                                                       *fn,
                                                                       verbose_springboardHeap);
      }
      
      // Not all kernel modules have a _fini routine.  See, for example,
      // common/fs/proc/prvfsops.c.
      fn = findFunctionInModule(*mod, "_fini", (function_t*)NULL);
      if (fn == NULL) {
         if (verbose_springboardHeap)
            cout << "NOTE: No " << mod->getName() << "/_fini" << endl;
      }
      else {
         num_springboard_bytes_beyondnucleus += addFnToSpringboardHeap(mod->getName(),
                                                                       *fn,
                                                                       verbose_springboardHeap);
      }
   }

#endif // solaris2_7

   global_springboardHeap->sortChunks();
      // probably not needed here, as it's done back in main() after we return,
      // right?
}

bool interprocedural_register_analysis = true;
   // used in regAnalysisOracles.h

// These two have to be declared up here because they're assumed to be defined globals
// in other .C files:
bool verbose_fnParse=false;
bool verbose_summaryFnParse=false;
bool verbose_hiddenFns=false;
bool verbose_runtimeRegisterAnalysisTimings=false;
bool replaceFunctionSummaryVerboseTimings=false;
bool replaceFunctionIndividualVerboseTimings=false;
bool verbose_callGraphStats = false;
bool verbose_edgeCountCalculation = false;
bool verbose_indirectCallsStats = false;

#if defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4)
char global_SystemMapFile[256];
bool global_SystemMap_provided = false;
int real_time_priority = 0;
#endif

extern const char V_kerninstd[];
extern const Ident kerninstd_ident(V_kerninstd, "KernInst");

int main(int argc, char **argv) {
   cout << "Welcome to Kerninstd, " << kerninstd_ident.release() << endl;

   if (geteuid() != 0) {
      cout << "kerninstd: you must be root to run this program" << endl;
      cout << "effective user id as reported by geteuid() is " << geteuid()
           << "; needs to be zero." << endl;
      
      return 5;
   }
   
   bool verbose_memUsage=false;
   bool verbose_skips=false;
   bool verbose_returningFuncs=false;
   bool verbose_symbolTable=false;
   bool verbose_symbolTableTiming=false; // subset of above
   bool verbose_fnParseTiming = false;
   bool verbose_removeAliases=false;
   bool verbose_resolveInterprocJumps=false;
   bool verbose_bbDFS=false;
   bool verbose_interProcRegAnalysis=false;
   bool verbose_interProcRegAnalysisTiming=false;
   bool verbose_numSuccsPreds=false;
   bool verbose_fnsFiddlingWithPrivRegs=false;
   bool verbose_springboardHeap=false;
   bool debug_output = true;

   const unsigned nargs = argc;
   
   for (unsigned lcv=1; lcv < nargs; lcv++) {
      const char *arg = argv[lcv];

      if (lcv == 1 && nargs == 2 &&
          (0==strcmp(arg, "-help") || 0==strcmp(arg, "help"))) {
         // help
#if defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4)
         cout << "-sm or -system_map <filename> --> use <filename> as System.map file for kernel symbol lookup" << endl;
#endif
         cout << "-i or -interprocedural_register_analysis --> turn on interprocedural"
              << endl << "register analysis (default=";
         if (interprocedural_register_analysis)
            cout << "on";
         else
            cout << "off";
         cout << ")" << endl;
         cout << "-ni or -no-interprocedural_register_analysis --> opposite of above"
              << endl;
         cout << endl;
         cout << "-v --> verbose everything" << endl;
         cout << "-verbose-springboardheap --> info about the springboard heap"
              << endl;
         cout << "-verbose-springboard-timings --> how long it takes to allocate springboards" << endl;
         cout << "-verbose-summary --> verbose summary of parsing (less info than -v)"
              << endl;
         cout << "-verbose-memusage --> info on memory usage" << endl;
         cout << "-verbose-skips --> verbose info on the skips.txt file"
              << endl;
         cout << "-verbose-startup-timings --> info on how long it took to parse, during startup"
              << endl;
         cout << "-verbose-runtime-register-analysis-timings --> info on how longs it takes to query at runtime, dead registers at a given point" << endl;
         cout << "-verbose-symboltable --> info on each module"
              << endl;
         cout << "-verbose-launchSite-timings-summary" << endl;
         cout << "-verbose-launchSite-timings-detailed" << endl;
         cout << "-verbose-replaceFunction-timings-summary" << endl;
         cout << "-verbose-replaceFunction-timings-detailed" << endl;
         cout << "-verbose-callgraph-stats" << endl;
         cout << "-verbose-edgecount-calculation" << endl;
         cout << "-verbose-indirect-calls-stats" << endl;
         cout << "-d or -debug-output --> turn on debug output" << endl;
         cout << "-nd or -no-debug-output --> turn off debug output" << endl;

         exit(5);
      }
      else if (0==strcmp(arg, "-i") ||
               0==strcmp(arg, "-interprocedural_register_analysis")) {
         interprocedural_register_analysis = true;
      }
      else if (0==strcmp(arg, "-ni") ||
               0==strcmp(arg, "-no-interprocedural_register_analysis")) {
         interprocedural_register_analysis = false;
      }
#if defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4)
      else if (0==strcmp(arg, "-sm") ||
               0==strcmp(arg, "-system_map")) {
         if(lcv==nargs-1 || 0==strncmp(argv[lcv+1], "-", 1)) {
            cerr << "Must specify a filename for -system_map option\n";
            exit(5);
         }
         global_SystemMap_provided = true;
         strcpy(&global_SystemMapFile[0], argv[++lcv]);
         cout << "Using file " << &global_SystemMapFile[0] << " as System.map\n";
      }
      else if(0==strcmp(arg, "-rt") ||
              0==strcmp(arg, "-real_time")) {
         if(lcv==nargs-1 || 0==strncmp(argv[lcv+1], "-", 1)) {
	    cerr << "Must specify a priority for -real_time option\n";
	    exit(5);
	 }
	 real_time_priority = atoi(argv[++lcv]);
	 if((real_time_priority < 1) || (real_time_priority > 99)) {
	    cerr << "Must specify a priority between 1 and 99\n";
	    exit(5);
	 }
      }
#endif
      else if (0==strcmp(arg, "-verbose") || 0==strcmp(arg, "-v")) {
         verbose_memUsage = true;
         
         verbose_skips=true;
         verbose_returningFuncs=true;
         verbose_removeAliases=true;
         verbose_fnParse=true;
         verbose_summaryFnParse=true;

         verbose_symbolTable=true;
         verbose_symbolTableTiming=true;
         verbose_fnParseTiming = true;
         verbose_interProcRegAnalysisTiming = true;

         verbose_hiddenFns=true;
         verbose_resolveInterprocJumps=true;
         verbose_bbDFS=true;
         verbose_interProcRegAnalysis=true;
         verbose_numSuccsPreds=true;
         verbose_fnsFiddlingWithPrivRegs=true;
         verbose_springboardHeap=true;

         verbose_callGraphStats = true;
         verbose_edgeCountCalculation = true;
      }
      else if (0==strcmp(arg, "-verbose-springboardheap")) {
         verbose_springboardHeap = true;
      }
      else if (0==strcmp(arg, "-verbose-springboard-timings")) {
         SpringBoardHeap::setVerboseTimings();
      }
      else if (0==strcmp(arg, "-verbose-summary")) {
         verbose_memUsage = true;
         verbose_summaryFnParse = true;
         verbose_numSuccsPreds = true;
         verbose_callGraphStats = true;
         verbose_edgeCountCalculation = true;
      }
      else if (0==strcmp(arg, "-verbose-symboltable")) {
         verbose_symbolTable = true;
      }
      else if (0==strcmp(arg, "-verbose-memusage")) {
         verbose_memUsage = true;
      }
      else if (0==strcmp(arg, "-verbose-skips")) {
         verbose_skips = true;
      }
      else if (0==strcmp(arg, "-verbose-startup-timings")) {
         verbose_symbolTableTiming = true; // but not verbose_symbolTable
         verbose_fnParseTiming = true;
         verbose_interProcRegAnalysisTiming = true;
      }
      else if (0==strcmp(arg, "-verbose-runtime-register-analysis-timings")) {
         verbose_runtimeRegisterAnalysisTimings = true;
      }
      else if (0==strcmp(arg, "-verbose-launchSite-timings-summary")) {
         LaunchSite::setVerboseTimingsSummary();
      }
      else if (0==strcmp(arg, "-verbose-launchSite-timings-detailed")) {
         LaunchSite::setVerboseTimingsDetailed();
      }
      else if (0==strcmp(arg, "-verbose-replaceFunction-timings-summary")) {
         replaceFunctionSummaryVerboseTimings=true;
      }
      else if (0==strcmp(arg, "-verbose-replaceFunction-timings-detailed")) {
         replaceFunctionIndividualVerboseTimings = true;
      }
      else if (0==strcmp(arg, "-verbose-callgraph-stats")) {
         verbose_callGraphStats = true;
      }
      else if (0==strcmp(arg, "-verbose-edgecount-calculation")) {
         verbose_edgeCountCalculation = true;
      }
      else if (0==strcmp(arg, "-verbose-indirect-calls-stats")) {
         verbose_indirectCallsStats = true;
      }
      else if (0==strcmp(arg, "-d") ||
	       0==strcmp(arg, "-debug-output")) {
         debug_output = true;
      }
      else if (0==strcmp(arg, "-nd") ||
	       0==strcmp(arg, "-no-debug-output")) {
         debug_output = false;
      }
      else if (strlen(arg) > 0 && arg[0] != '-' && NULL == global_infilename)
         global_infilename = arg;
      else {
         cerr << "unknown argument: \"" << arg << "\"" << endl;
         return 5;
      }
   }

   init_out_streams(debug_output);

   // global_kernelDriver needs initialized before initializeKernInst is eval'd
   const char *drivername = "/dev/kerninst";
   try {
      global_kernelDriver = new kernelDriver(drivername);
   }
   catch (kernelDriver::OpenFailed) {
      cerr << "KernInst: failed to open /dev/kerninst; exiting" << endl;
      exit(5);
   }

#ifdef sparc_sun_solaris2_7
   theKerninstdABI = new sparcv8plus_abi();
      // We should be able to automatically detect this!

   // What's the ABI for the kernel?  Use "isalist" equivalent
   char buffer[257];
   long result = sysinfo(SI_ISALIST, buffer, 257);
   if (result == -1) {
      perror("sysinfo SI_ISALIST");
      assert(false);
   }
   if (NULL != strstr(buffer, "sparcv9"))
      theKernelABI = new sparcv9_abi();
   else if (NULL != strstr(buffer, "sparcv8plus"))
      theKernelABI = new sparcv8plus_abi();
   else
      assert(false && "isalist(5) doesn't show sparcv8plus or sparcv9 -- not sure what kind of kernel is running.");
#elif defined(i386_unknown_linux2_4)
   theKerninstdABI = new x86_abi();
   theKernelABI = new x86_abi();
#elif defined(ppc64_unknown_linux2_4)
   theKerninstdABI = new power64_abi();
   theKernelABI = new power64_abi();
#endif

   presentedMachineInfo pmi;
   global_kernelDriver->getPresentedMachineInfo(*theKernelABI, 
						*theKerninstdABI, &pmi);
   global_machineInfo = new machineInfo(pmi);

   global_moduleMgr = new moduleMgr;
   
   global_mappedKernelSymbols = new mappedKernelSymbols(*global_kernelDriver);
   global_patchHeap = new patchHeap(*global_kernelDriver);
   global_springboardHeap = new SpringBoardHeap();
      // NOTE: for now, the springboard heap is empty since we need to find start(), main(),
      // and all of the _init()'s to make up the components of the springboard heap.

   global_instrumentationMgr = new instrumentationMgr;
   assert(global_instrumentationMgr);

#ifdef sparc_sun_solaris2_7
   global_cpcInterface = new cpcInterface(*global_machineInfo);
   uint64_t pcr_raw = global_kernelDriver->getPcrRawCurrentCPU();
   pcr_union punion(pcr_raw);
   punion.f.s1 = 0;
   punion.f.s0 = 0;
   global_cpcInterface->changePcrAllCPUs(punion);
#endif

   cout	<< "Analyzing kernel image..." << endl;

   firstTimeParseRoutine(verbose_memUsage,
                         verbose_skips,
                         verbose_returningFuncs,
                         verbose_symbolTable,
                         verbose_symbolTableTiming,
                         verbose_removeAliases,
                         verbose_summaryFnParse,
                         verbose_fnParse,
                         verbose_fnParseTiming,
                         verbose_hiddenFns,
                         verbose_resolveInterprocJumps,
                         verbose_bbDFS,
                         verbose_interProcRegAnalysis,
                         verbose_interProcRegAnalysisTiming,
                         verbose_numSuccsPreds,
                         verbose_fnsFiddlingWithPrivRegs,
                         verbose_springboardHeap
                         ); // gather(), gather2(), find springboards, 

#ifdef sparc_sun_solaris2_7
   // Now some kludging to add yet more to the springboard heap, this time near
   // kmem_alloc()'d memory (so we pass false to allocate_kernel_block(): don't try
   // to allocate w/in kernel's nucleus text).
   kptr_t extra_springboard_8k = global_kernelDriver->allocate_kmem32(8192);
   global_springboardHeap->addChunk(extra_springboard_8k, 8192);
   global_springboardHeap->sortChunks();

   if (verbose_springboardHeap) {
      cout << "Added 8k at " << addr2hex(extra_springboard_8k)
           << " to springboard heap" << endl;

      pair<uint32_t,uint32_t> nbytes =
         global_springboardHeap->calcNumBytesInHeap(*global_kernelDriver);
      cout << "Total springboard space (includes the kludge symbol(s)):\n"
	   << "in nucleus: " << nbytes.first << " bytes" << endl
	   << "outside of nucleus: " << nbytes.second << " bytes" << endl;
   }
#endif

   // Protect ourselves from client crashes -- block SIGPIPE. Otherwise,
   // writing to the igen socket after a client terminates will kill us.
   // Blocking SIGPIPE is perfectly safe -- the write error will be detected
   // by examining the return value / errno
   struct sigaction act;
   act.sa_handler = SIG_IGN;
   if (sigaction(SIGPIPE, &act, 0) < 0) {
       perror("sigaction error");
       assert(false);
   }

#if defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4)
   // If the user asked us to, make kerninstd a real-time process of the 
   // specified priority (1-99)
   if(real_time_priority) {
      struct sched_param sp;
      sp.sched_priority = real_time_priority;
      if(sched_setscheduler(0, SCHED_FIFO, &sp) < 0) {
         perror("sched_setscheduler error");
         assert(false);
      }
   }
#endif

   cout << "kerninstd -- finished with startup -- entering main loop now" << endl;
   cout << "Reminder: press ^C in this window to exit kerninstd at any time..." << endl;
   kerninstd_mainloop();
   cout << "kerninstd -- finished with main loop -- beginning cleanup" << endl;

   // XXX question: what to do about any remaining "internalEvent"s that are
   // normally drained and processed in the main loop?

   cout << "kerninstd removing any remaining instrumentation" << endl;
   
   global_instrumentationMgr->kerninstdIsGoingDownNow();
      // feverishly uninstruments any remaining instrumentation code by replacing
      // the original overwritten instruction at all instrumentation points.
      // Must be done first (e.g., before the heaps are freed, below)!!!

   sleep(3);
   
   dout << "kerninstd freeing up kernel heaps" << endl;

#ifdef sparc_sun_solaris2_7
   // The following line is probably unnecessary since we're about to
   // fry the springboard heap anyway!
   global_springboardHeap->removeChunk(extra_springboard_8k);
   global_kernelDriver->free_kmem32(extra_springboard_8k, 8192);
#endif

   delete global_springboardHeap; // dtor doesn't really do anything
   global_springboardHeap = NULL; // help purify find mem leaks

   delete global_patchHeap; // needs kernel driver (free_kmem)
   global_patchHeap = NULL; // help purify find mem leaks
   
   delete global_kernelDriver;
   global_kernelDriver = NULL; // help purify find mem leaks

   dout << "kerninstd freeing up module manager" << endl;

   delete global_moduleMgr;
   global_moduleMgr = NULL; // help purify find mem leaks

   delete global_machineInfo;
   global_machineInfo = NULL; // help purify find mem leaks

#ifdef sparc_sun_solaris2_7
   delete global_cpcInterface;
   global_cpcInterface = NULL; // help purify find mem leaks
#endif

   cout << "kerninstd: bye" << endl;
   return 0;
}
