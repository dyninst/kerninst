/* kerninstdClient */

/* The "normal" directional flow is from applications to kerninstd.
   So "normal" (not $upcall) methods are in that direction.
   $upcall methods are from kerninstd back to the application
 */

#include "util/h/kdrvtypes.h"
#include "util/h/xdr_send_recv.h" // to ensure instantiation of templated P_xdr_send()'s
#include "xdr_send_recv_common.h" // to ensure instantiation of templated P_xdr_send()'s
#include "fnCodeObjects.h"
#include "downloadToKernel.h"
   // downloadToKernelFn, downloadToKernelFns, and emitOrdering

// chunkRange needs to be typedef'd before we #include "parseNewFunction", which
// relies on it being defined:
$remote $nobundle class chunkRange;
typedef std::pair<kptr_t, unsigned> chunkRange; // addr, numbytes

#include "parseNewFunction.h" // parseNewFunction and parseNewFunctions

$remote $nobundle class trick_regset;
$remote $nobundle class trick_fn;
$remote $nobundle class trick_mbdf;
$remote $nobundle class trick_relocatableCode;
$remote class fnCodeObjects;
$remote $nobundle class downloadToKernelFns; // downloadToKernel.h
$remote $nobundle class emitOrdering; // downloadToKernel.h
$remote $nobundle class parseNewFunctions; // parseNewFunction.h

$remote $nobundle class kdownloadedLocations1Fn;
typedef std::pair<pdvector<kptr_t>, unsigned> kdownloadedLocations1Fn;
   // .first: chunk entry points
   // .second: which chunk ndx is the entry chunk
   // Note that this structure serves for both a simple (single-fn) download
   // and a deluxe download (several functions with a complex interleaved emit ordering)
   // Even if there are several functions, there's just one entry point for the
   // downloaded blob (used in case we do a replaceFunction, right?)

$remote $nobundle class ddownloadedLocations;
typedef std::pair<pdvector<dptr_t>, unsigned> ddownloadedLocations;
   // pdvector<kptr_t>: start addrs of each chunk
   // unsigned: which chunk (an ndx into the vector) is the entry point

$remote $nobundle class chunkData;
typedef std::pair<kptr_t, pdvector<uint32_t> > chunkData;
$remote $nobundle class fnAddrBeingRelocated;
typedef std::pair<kptr_t, pdstring> fnAddrBeingRelocated;


// used by {get/set}P4PerfCtrState() below (in Misc section)
$remote $nobundle class P4PerfCtrState;
typedef pdvector<std::pair<unsigned, unsigned> > P4PerfCtrState;

// The following allows us to avoid a copy-ctor call to class loadedModule
// in kperfmon.  It's needed if we "naively" receive a loadedModule, which
// will get allocated in the usual igen way.  But in the end, we'd like to have
// it as a new'd pointer.  This dummy class allows us to do exactly that.
$remote $nobundle class trick_module; // avoids a copy ctor

$remote struct oneRegAnalysisReqInfo {
   kptr_t address;
   bool before;
      // true --> before the instruction; false --> after the instruction
      // false is really only needed in 1 case: when address is the last instruction
      // in the basic block
};

$remote class dataflowFn1Insn {
   oneRegAnalysisReqInfo where;
   trick_mbdf fn;

$ignore
public:
   dataflowFn1Insn(oneRegAnalysisReqInfo iwhere,
                   trick_mbdf ifn) :
        where(iwhere), fn(ifn) {}
   dataflowFn1Insn(const dataflowFn1Insn &src) :
        where(src.where), fn(src.fn) {}
$ignore
};


$remote $nobundle class sample_type;
typedef uint64_t sample_type;

$remote struct one_sample_data {
   uint64_t timeOfSample;
      // in cycles, as kept by the sampling cpu.  Be advised that
      // it won't start out as 0, but rather whatever the value of the tick
      // register is...probably some humongous integer value.  Recipient
      // must be prepared for this.

   unsigned sampling_cpu_id;

   unsigned uniqSampleReqId; // as chosen by app

   pdvector<sample_type> values;
      // usually just 1 item in the vector, but there can be more, depending
      // on the metric.  For example, an ipc metric needs to sample (and ship)
      // two raw totals: total # of insns, and total # of cycles
};

$remote struct cpu_info {
   unsigned cpuid; // e.g. cpu2 can have id=5
   unsigned megahertz; // e.g. 167 for an Ultra 1/167 (duh)
};

$remote struct presentedMachineInfo {
   // abiCode: 0 for SPARC v8plus, 1 for SPARC v9 (x86???)
   // (of course, it should be an enumerated type instead; does igen support enums?)
   unsigned kerninstdABICode;
   unsigned kernelABICode;
   unsigned os_version;     // 6 == Solaris 2.6, 7 == Solaris 7, 8 == Solaris 8
   unsigned has_stick;      // iff ultra-3
   unsigned system_freq;    // Hz
   unsigned disp_level;     // Interrupt level for running the cswitch code
   unsigned t_cpu_offset;   // Offset of the t_cpu field in the kthread struct
   unsigned t_procp_offset; // Offset of t_procp field in the kthread struct
   unsigned t_size;         // size of kthread struct
   unsigned p_pidp_offset;  // Offset of p_pidp field in the proc_t struct
   unsigned pid_id_offset;  // Offset of pid_id field in the pid struct
   pdvector<cpu_info> theCpusInfo;
   kptr_t nucleusTextBoundsLow;
   kptr_t nucleusTextBoundsHi;
   unsigned t_pacacurrent_offset; //offset off local_paca for current pointer
   unsigned paca_access;
};

$remote struct pcrData {
   unsigned s1; // Event monitored by pic1
   unsigned s0; // Event monitored by pic0
   bool ut;
   bool st;
   bool priv;
};	

$remote kerninstdClient {
   $base 6000;
   $version 18;

   // ---------- Asking for Info about the Kerninstd Machine ----------

   presentedMachineInfo getKerninstdMachineInfo();
   uint64_t getCurrentKerninstdTime();

   // ---------------- Module, Fn, Basic Block Information ----------------

   // ...Module and function information...
   $async void request_for_modules();

   // kerninstd responding with module information:
   $upcall $async void begin_module_information();
   $upcall $async void module_information(const trick_module &);
   $upcall $async void end_module_information();

   $upcall $async void one_function_information(const pdstring &modName,
                                                const trick_fn &);
      // NOTE: A module with name "modName" *must* already exist; create it with
      // the igen call module_information() (even if at that time it's just a blank
      // module w/o any functions).

   $upcall $async void delete_one_function(kptr_t);
      // sort of the opposite of the above routine

   // ---------------- Parsing Functions into CodeObjects ----------------
   $async void reqParseFnIntoCodeObjects(unsigned reqid, kptr_t fnStartAddr,
                                         const pdvector<fnAddrBeingRelocated> &fnAddrsBeingRelocated);
      // fnAddrsBeingRelocated is a bit tricky to understand, so listen up: as we parse
      // the code objects of the function beginning at fnStartAddr, we'll have occasion
      // to come across, say, some call instructions (calls to an address, not a name).
      // If said call is to one of the addresses listed in "fnAddrsBeingRelocated[]",
      // then the code object for that call will be, instead of the usual
      // "interProcCallToAddrCodeObject", parsed as "interProcCallToNameCodeObject",
      // with the name changed to .second of the pair.
      // Similar for interprocedural branches ("interProcBranchToAddrCodeObject" in the
      // normal case, but changed to "interProcBranchToNameCodeObject" if the dest addr
      // is within "fnAddrsBeingRelocated").
      // The changes are:
      // interProcCallToAddrCodeObject --> interProcCallToNameCodeObject
      // interProcTailCallToAddrCodeObject --> interProcTailCallToNameCodeObject
      // interProcBranchToAddrCodeObject --> interProcBranchToNameCodeObject
      // (Note that recursiveCallCodeObject doesn't need a second version)

   $upcall $async void parseFnIntoCodeObjectsResponse(unsigned reqid,
                                                      const fnCodeObjects &);

   fnCodeObjects syncParseFnIntoCodeObjects(kptr_t fnStartAddr,
                                            const pdvector<fnAddrBeingRelocated> &fnAddrsBeingRelocated);
      // Note that a sentinel can be returned (see fnCodeObjects class for details)
      // because some code objects are not yet implemented.  For example,
      // the nonLoadingBackwardsJumpTableCodeObject used by blkclr() will
      // be very hard to implement.

   // ---------- Reading/Writing the Contents of Kernel Memory -----------

   $async void request_for_peek(unsigned request_id,
                                const pdvector<chunkRange> &);

   $upcall $async void peek_response(unsigned request_id, const pdvector<chunkData> &);

   pdvector<uint32_t> peek_kernel_contig(kptr_t, unsigned);
   
   int poke_kernel_contig(const pdvector<uint32_t> &, kptr_t, unsigned);   

   // -------------- Reading the Contents of Kerninstd Memory ---------------

   pdvector<uint32_t> peek_kerninstd_contig(dptr_t, unsigned);

   // -------------------------- Live Register Analysis -----------------------------

   $async void requestRegAnalysis(unsigned reqid,
                                  const pdvector<oneRegAnalysisReqInfo> &);

   $upcall $async void regAnalysisResponse(unsigned reqid,
                                           const pdvector<dataflowFn1Insn> &result);
   // Same as the above two, but synchronous for simplicity
   pdvector<dataflowFn1Insn> syncRegisterAnalysis(const pdvector<oneRegAnalysisReqInfo> &);

   trick_regset getFreeRegsAtPoint(kptr_t addr, bool preReturn);

   // --------------------------------------------------------------------------------
   // ---------- Allocating Kernel Memory (not mapped into kerninstd space) ----------
   // --------------------------------------------------------------------------------

   $async void allocateUnmappedKernelMemory(unsigned reqid,
                                            unsigned nbytes,
                                            bool tryForNucleusText);
   kptr_t queryAllocatedUnmappedKernelMemoryAddr(unsigned reqid);
   $async void freeAllocatedUnmappedKernelMemory(unsigned reqid);

   // --------------------------------------------------------------------------------
   // -------------------------- Allocating Kerninstd Memory -------------------------
   // ---------------------- (not also mapped into the kernel) -----------------------
   // -------------------------- This is infrequently Needed -------------------------
   // --------------------------------------------------------------------------------

   $async void allocateKerninstdMemory(unsigned reqid, unsigned nbytes);
   dptr_t queryAllocatedKerninstdMemoryAddr(unsigned reqid);
   $async void freeAllocatedKerninstdMemory(unsigned reqid);

   // --------------------------------------------------------------------------------
   // ------------------------ Allocating Mapped Kernel Memory -----------------------
   // --------------------------------------------------------------------------------

   $async void allocateMappedKernelMemory(unsigned reqid,
                                          unsigned nbytes,
                                          bool tryForNucleusText);
      // you cannot initialize this kernel memory in this igen call, but since 
      // it's mmapped into kerninstd's space, a downloadToKerninstd() of some
      // initialization code of your choosing plus doDownloadedToKerninstdCodeOnceNow()
      // allows you to initialize it to your liking.
      // Note: you're probably baffled as to why this routine doesn't return the
      // address of the allocated object (and in fact there are 2 such addresses: one
      // in the kernel and one mapped into kerninstd).  No problem; the following
      // 2 igen calls do the trick.  Just don't forget the reqid; you'll need it.
      // Note: because kerninstd is going to mmap the kernel block into its space,
      // and the kernel requires mmaps to be a multiple of page length, please
      // use this routine for relatively large allocation requests (>=8K, not, say
      // for each 64-bit integer counter that you want to allocate).  We'll round
      // up nbytes to a page-length multiple behind the scenes.

   kptr_t queryAllocatedMappedKernelMemoryAddrInKernel(unsigned reqid);
   dptr_t queryAllocatedMappedKernelMemoryAddrInKerninstd(unsigned reqid);

   $async void freeMappedKernelMemory(unsigned reqid);   

   // ---------------------- MMapping Kernel Symbols into Kerninstd ------------------

   $async void requestReadOnlyMMapOfKernelSymIntoKerninstd(unsigned reqId,
                                                           const pdstring &symName,
                                                           unsigned nbytes);
   $upcall $async void mMapOfKernelSymIntoKerninstdResponse(unsigned reqId,
                                                            dptr_t addrInKerninstd); 

   $async void unmapKernelSymInKerninstd(unsigned reqId);

   // ---------------------------- Replace-Function -----------------------------

   $async void replaceAFunction(unsigned reqId,
                                kptr_t oldFnStartAddr,
                                kptr_t newFnStartAddr,
                                bool replaceCallSitesToo);
   $async void unReplaceAFunction(unsigned reqId);

   $async void replaceAFunctionCall(unsigned reqId,
                                    kptr_t callSiteAddr,
                                    kptr_t newFnStartAddr);
   $async void unReplaceAFunctionCall(unsigned reqId);

   // ---------------------------- Splicing -----------------------------

   $async void
   splice_preinsn(unsigned reqid, kptr_t spliceAddr,
                  unsigned conservative_numInt32RegsNeeded,
                  unsigned conservative_numInt64RegsNeeded,
                  // The remaining arguments describe the code being downloaded.
                  // It's no coincidence that they're the same as the final args
                  // to downloadCode(), below.  However, there's 1 key difference:
                  // we need to send 2 copies of the relocatable code, one in
                  // case no save/restore is needed around our code, and another
                  // if a save/restore *is* (later) determined to be needed around
                  // the client's code.  The client can't know ahead of time
                  // whether kerninstd will choose to bracket its code with
                  // save/restore, because it partially depends on the contents of
                  // other code snippets inserted at the same location.
                  trick_relocatableCode relocatable_code_ifnosave,
                  trick_relocatableCode relocatable_code_ifsave
                  );
   $async void
   splice_prereturn(unsigned reqid, kptr_t spliceAddr,
                    unsigned conservative_numInt32RegsNeeded,
                    unsigned conservative_numInt64RegsNeeded,
                    trick_relocatableCode relocatable_code_prereturn_ifnosave,
                    trick_relocatableCode relocatable_code_prereturn_ifsave,
                    // The following 2 args are needed because preReturn snippets
                    // may, unbeknownst to the client, be changed to preInsn snippets.
                    // This presents a different set of free regs and thus
                    // necessitates different code to be generated.
                    trick_relocatableCode relocatable_code_preinsn_ifnosave,
                    trick_relocatableCode relocatable_code_preinsn_ifsave
                    );
      // Application is presenting code, and asking kerninstd to splice it into the
      // kernel.   Not to be confused with downloadCode(), which is for putting code
      // into the kerninstd application, not the kernel.
      // The client presents the code to be spliced into the kernel.  Of course, since
      // the application doesn't know ahead of time where the code patch will reside,
      // the client must present *relocatable* code.  There can be unresolved calls
      // (to kernel fns, not to kerninstd fns...remember, don't confuse with
      // downloadCode()) and unresolved sethi/bsets of imm32s.  kerninstd will
      // resolve those as necessary.  Note that at present we have sort of a poor
      // man's version of relocatable code.  A fancier version would allow the whole
      // shebang of the elf format's relocatable features.  (Check out the .rela.text
      // section of any sparc elf .o file)  But for now, what we have will do.

   $async void unSplice(unsigned request_id);
      // tells kerninstd to please remove this code.

   // -------------------------------------------------------------------------------
   // ------------ Downloading Code (& Data) Into Kerninstd (not the kernel)---------
   // ------------------------- Useful for Sampling, e.g. ---------------------------
   // -------------------------------------------------------------------------------

   $async void downloadToKerninstd(unsigned reqid, // chosen by app
                                   trick_relocatableCode,
                                   unsigned entry_chunk_ndx);
      // kpermon sends this msg.  kerninstd receives the code, resolves calls, and
      // just stores it (doesn't start executing it yet).

   $async void removeDownloadedToKerninstd(unsigned reqid);
        // doesn't try to stop sampling or anything like that

   ddownloadedLocations queryDownloadedToKerninstdAddresses(unsigned reqid);    

   $async void doDownloadedToKerninstdCodeOnceNow(unsigned uniqDownloadedCodeId);
      // like an inferior-rpc: kperfmon is telling kerninstd to execute the code now.

   $async void periodicallyDoDownloadedToKerninstdCode(unsigned uniqDownloadedCodeId,
                                                       unsigned period_millisecs);
      // note: can be used to (initially) set a sampling interval, or to later
      // change said interval.

   $async void stopPeriodicallyDoingDownloadedToKerninstdCode(unsigned uniqDownloadedCodeId);

   // -------------------------------------------------------------------------------
   // ---------- Downloading Code (& Data) Into The Kernel (not to kerninstd) -------
   // --------------- Perhaps one day, splicing will use these routines -------------
   // ---------- But it can't do that yet: in this scenario, code that gets ---------
   // ---------- downloaded into the kernel stays put, whereas instrumentation ------
   // ---------- code can move about if other instrumentation code is inserted ------
   // ---------- at the instrumentation point ---------------------------------------
   // -------------------------------------------------------------------------------

   $async void downloadToKernel(unsigned reqid, // chosen by app
                                trick_relocatableCode,
                                unsigned entry_chunk_ndx,
                                bool tryForNucleus);

   $async void download1FnToKernelAndParse(unsigned reqid, // chosen by app
       	                                   const pdstring &modName,
	                                   const pdstring &modDescriptionIfNew,
	                                   const pdstring &fnName,
	                                   trick_relocatableCode,
                                           unsigned entry_chunk_ndx,
                                           unsigned preFnDataChunkIfAny,
                                              // -1U if none
                                           bool tryForNucleus
                                           );
      // Kerninstd receives code, resolves unresolved stuff, and just stores it
      // into some available space in the kernel.  The code won't start running.
      // This isn't a splice request.  You can query where kerninstd has decided
      // to place this code by making the following igen call.
      // Note: can also be used to download data, in practice.
      // Note: this is the vanilla version, for downloading a single function

   $async void downloadSeveralFnsToKernelAndParse(unsigned reqid,
    	                                          const downloadToKernelFns &,
                                                  const emitOrdering &,
                                                  bool tryForNucleus);
      // This is the deluxe version, where you specify an emit ordering.
      // outlining uses this version.

   kdownloadedLocations1Fn queryDownloadedToKernelAddresses1Fn(unsigned reqid);
   pdvector<kdownloadedLocations1Fn> queryDownloadedToKernelAddresses(unsigned reqid);

   $async void removeDownloadedToKernel(unsigned reqid);
      // Reminder: caller should be sure that the kernel is not presently executing this
      // code, or may return to this code, before removing.

   // The following is sort of a late version of parsing downloaded-to-kernel
   // code.  It assumes that the code already exists in the kernel.
   $async void parseKernelRangesAsFns(const parseNewFunctions &);

   // -------------------------------------------------------------------------
   // ---------------------------- Sampling -----------------------------------
   // -------------------------------------------------------------------------

   $async void requestSampleData(unsigned sampleReqId, 
                                 const pdvector<kptr_t> &addrs);
   $async void removeSampleRequest(unsigned sampleReqId);
   $async void periodicallyDoSampling(unsigned sampleReqId, unsigned ms);
   $async void stopPeriodicallyDoingSampling(unsigned sampleReqId);
   $async void doSamplingOnceNow(unsigned sampleReqId);
   $upcall $async void presentSampleData(const pdvector<one_sample_data> &data);
   // The following two are kludge routines needed to avoid deadlock, which occurs
   // as follows: instrument every basic block of a huge routine.  Halfway thru
   // instrumentation, sampling data for those blocks already instrumented is being
   // upcalled back to kperfmon.  Eventually, so much data is being sent, so quickly,
   // that kerninstd blocks due to a full socket buffer.  Normally that's OK, but
   // kperfmon will never drain any of that data because it is blocked for a different
   // reason: it has just made a sync igen call to kerninstd to, say, getFreeRegsAtPoint
   // The solution is to temporarily stop the swamping of upcalls.
   $async void temporarilyTurnOffSampling();
   $async void resumeSampling();

   // ---------------------------- Misc -----------------------------

   $async void setJustTestingFlag(bool);
      // true --> allocate data, create & fill code patch and springboard,
      // but don't jump to the springboard, so the code will never be executed.

// BEGIN Sparc/Solaris Specific Functions
   pcrData getPcrData();
   $async void setPcr(unsigned pic0, unsigned pic1,
                      bool user, bool sys, bool priv);
   $async void print_pic();
   $async void clear_pic();
// END Sparc/Solaris Specific Functions
        
// BEGIN x86/Linux Specific Functions
   unsigned getUd2BugSize();
   P4PerfCtrState getP4PerfCtrState();
   void setP4PerfCtrState(const P4PerfCtrState &);
// END x86/Linux Specific Functions

// BEGIN ppc64/Linux Specific Functions
   pdvector<uint64_t> getPowerPerfCtrState();
   void setPowerPerfCtrState(const pdvector<uint64_t> & );
// END ppc64/Linux Specific Functions
   

// BEGIN Linux Specific Functions
   void toggle_debug_bits(uint32_t bitmask);
   void toggle_enable_bits(uint32_t bitmask);
// END Linux Specific Functions

   kptr_t getAllVTimersAddr();
   $async void addTimerToAllVTimers(kptr_t);
   $async void removeTimerFromAllVTimers(kptr_t);   

   $async void call_once();

   // Return the already-collected callee information for a given call site
   // to the client
   pdvector<kptr_t> getCalleesForSite(kptr_t);

   // Read the value of a hardware counter across all cpus
   pdvector<sample_type> readHwcAllCPUs(unsigned);
};
