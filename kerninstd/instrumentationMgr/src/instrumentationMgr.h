// instrumentationMgr.h
// These routine should more or less match up with incoming igen calls
// that we need to process.  For some calls, we simply make a same-named
// method call within class client_connection.  But for others (splicing
// and replaceFunction), we keep some crucial bookkeeping info in this
// class.

#ifndef _INSTRUMENTATION_MGR_H_
#define _INSTRUMENTATION_MGR_H_

#include "clientConn.h"
#include "util/h/Dictionary.h"
#include "launchSite.h"
#include "replaceFunction.h"
#include "replaceFunctionCall.h"
#include "funkshun.h"
#include "fnCode.h"

class instrumentationMgr {
 public:
   typedef function_t::fnCode_t fnCode_t;
   typedef client_connection::downloadedIntoKernelEntry1Fn downloadedIntoKernelEntry1Fn;

 private:
   dictionary_hash<kptr_t, LaunchSite*> theLaunchSites;
   // yep, this class actually manages the allocation & deletion of the
   // launch sites. (Explains the class' name)

   dictionary_hash<kptr_t, replaceFunction*> replacedFnsMap;
   // key: start addr of fn that is replaced
   dictionary_hash<kptr_t, replaceFunction*> replacementFnsMap;
   // key: start address of the function that's doing the replacing

   dictionary_hash<kptr_t, replaceFunctionCall*> replacedCallsMap;
   // key: call site address

   bool remove1DownloadedToKernel_preliminary(kptr_t startAddr,
                                              const function_t *parsedFnIfAny,
                                              const pdvector<const function_t*> &allFnsAboutToBeRemoved);
      // returns true iff OK, and the code may now safely be fried.  See the .C file
      // for what this routine does.  (What it *doesn't* do is actually remove
      // this fn; it just does some useful "preparatory" work to make sure
      // that removing the fn is an OK thing to do.)

   // private so they're not invoked:
   instrumentationMgr(const instrumentationMgr &);
   instrumentationMgr &operator=(const instrumentationMgr &);

   LaunchSite* getLaunchSiteForSplice(client_connection &, unsigned reqid,
                                      kptr_t spliceAddr);
   // creates launch site if needed (and stores in "theLaunchSites[]")
   // Also tells the client about this reqid/spliceAddr so it can keep track.

 public:
   instrumentationMgr();
  ~instrumentationMgr();

   void kerninstdIsGoingDownNow();
      // Since we're about to die, this is our last opportunity to
      // restore the original instrumentation code!  That is, this is our
      // last opportunity to unsplice anything still spliced!

   void clientIsGoingDownNow(client_connection &);

   void replaceAFunction(client_connection &,
                         unsigned reqid,
                         kptr_t oldFunctionEntryAddr,
                         kptr_t newFunctionEntryAddr,
                         bool replaceCallSitesToo);
      // can't call this method "replaceFunction" since that name's in use
      // (another class)
   void unReplaceAFunction(client_connection &,
                           unsigned reqid);

   void replaceAFunctionCall(client_connection &,
                             unsigned reqid,
                             kptr_t callSiteAddr,
                             kptr_t newFunctionEntryAddr);
   void unReplaceAFunctionCall(client_connection &,
                               unsigned reqid);

   void splice_preinsn(client_connection &, unsigned reqid,
                       kptr_t spliceAddr,
                       unsigned conservative_numInt32RegsNeeded,
                       unsigned conservative_numInt64RegsNeeded,
                       const relocatableCode_t *relocatable_code_ifnosave,
                       const relocatableCode_t *relocatable_code_ifsave);
   void splice_prereturn(client_connection &, unsigned reqid,
			 kptr_t spliceAddr,
                         unsigned conservative_numInt32RegsNeeded,
                         unsigned conservative_numInt64RegsNeeded,
                         const relocatableCode_t *relocatable_code_prereturn_ifnosave,
                         const relocatableCode_t *relocatable_code_prereturn_ifsave,
                         const relocatableCode_t *relocatable_code_prereturn_ifnosave,
                         const relocatableCode_t *relocatable_code_prereturn_ifsave);

   void unSplice(client_connection &, unsigned spliceReqId);

   void requestReadOnlyMMapOfKernelSymIntoKerninstd(client_connection &,
                                                    unsigned reqid,
                                                    const pdstring &symName,
                                                    unsigned nbytes);
   
   void unmapKernelSymInKerninstd(client_connection &, unsigned reqid);

   // ---------------------------------------------------------------------
   // ------------------ Allocating Unmapped Kernel Memory ----------------
   // ---------------------------------------------------------------------

   void allocateUnmappedKernelMemory(client_connection &,
                                     unsigned reqid,
                                     unsigned nbytes,
                                     bool tryForNucleusText);
   kptr_t queryAllocatedUnmappedKernelMemoryAddr(client_connection &, 
						 unsigned reqid);
   void freeAllocatedUnmappedKernelMemory(client_connection &, unsigned reqid);

   // ---------------------------------------------------------------------
   // ------------------ Allocating Kerninstd-only Memory -----------------
   // ---------------------------------------------------------------------

   void allocateKerninstdMemory(client_connection &,
                                unsigned reqid, unsigned nbytes);
   void *queryAllocatedKerninstdMemoryAddr(client_connection &, unsigned reqid);
   void freeKerninstdMemory(client_connection &, unsigned reqid);

   // ---------------------------------------------------------------------
   // ------------------ Allocating Mapped Kernel Memory ----------------
   // ---------------------------------------------------------------------

   void allocateMappedKernelMemory(client_connection &,
                                   unsigned reqid, unsigned nbytes,
                                   bool tryForNucleusText);
   kptr_t queryAllocatedMappedKernelMemoryAddrInKernel(client_connection &,
						       unsigned reqid);
   void *queryAllocatedMappedKernelMemoryAddrInKerninstd(client_connection &,
							 unsigned reqid);
   void freeMappedKernelMemory(client_connection &, unsigned reqid);

   // --------------------

   void downloadToKernelAndParse(client_connection &, unsigned reqid,
                                 const pdstring &modName,
                                 const pdstring &modDescriptionIfNew,
                                 const pdstring &fnName,
                                 const relocatableCode_t *,
                                 unsigned entry_chunk_ndx,
                                 unsigned chunkNdxContainingDataIfAny,
                                 bool tryForNucleus
                                 );
   void downloadToKernelAndParse(client_connection &, unsigned reqid,
                                 const pdvector<downloadToKernelFn> &fns,
                                    // the above does NOT specify an emit ordering
                                 const pdvector< pair<unsigned,unsigned> > &emitOrdering,
                                   // .first: fn ndx   .second: chunk ndx w/in that fn
                                 bool tryForNucleus
                                 );

   void downloadToKernel(client_connection &, unsigned reqid,
                         const relocatableCode_t *,
                         unsigned entry_chunk_ndx,
                         bool tryForNucleus);

   void removeDownloadedToKernel(client_connection &, unsigned reqid);

   pdvector<kdownloadedLocations1Fn> // see the .I file for a definition
   queryDownloadedToKernelAddresses(client_connection &, unsigned reqid) const;

   kdownloadedLocations1Fn // see the .I file for a definition
   queryDownloadedToKernelAddresses1Fn(client_connection &, unsigned reqid) const;

   // --------------------

   void parseKernelRangesAsFns(client_connection &,
                               const parseNewFunctions &fnsInfo);

   fnCode_t peekCurrFnCode(const function_t &);

   // --------------------

   // Download into kerninstd: (e.g. sampling code)
   void downloadToKerninstd(client_connection &,
                            unsigned reqid, // uniq for this client/this igen call
                            const relocatableCode_t *,
                            unsigned entry_chunk_ndx);
   void removeDownloadedToKerninstd(client_connection &,
                                    unsigned reqid);
      // doesn't try to stop sampling or anything like that

   pair<pdvector<dptr_t>, unsigned>
   queryDownloadedToKerninstdAddresses(client_connection &, 
				       unsigned reqid) const;
      // .first: all of the chunks (incl. data if any)
      // .second: which chunk is the entry point

   void doDownloadedToKerninstdCodeOnceNow(client_connection &,
                                           unsigned uniqDownloadedCodeId);

   void periodicallyDoDownloadedToKerninstdCode(client_connection &,
                                                unsigned uniqDownloadedCodeId,
                                                unsigned period_millisecs);
      // can be used to set or change an interval

   void stopPeriodicallyDoingDownloadedToKerninstdCode(client_connection &,
                                                       unsigned uniqDownloadedCodeId);


   void periodicallyDoSampling(client_connection &, unsigned uniqSampleReqId,
                               unsigned period_millisecs);
   void stopPeriodicallyDoingSampling(client_connection &, 
                                      unsigned uniqSampleReqId);
   void doSamplingOnceNow(client_connection &, unsigned uniqSampleReqId);
   void temporarilyTurnOffSampling(client_connection &);
   void resumeSampling(client_connection &);
   
   void presentSampleData1Value(client_connection &,
                                unsigned sampReqId,
                                uint64_t sampCycles,
                                sample_type sampValue);

   void presentSampleDataSeveralValues(client_connection &theClient,
                                       unsigned sampReqId,
                                       uint64_t sampCycles,
                                       unsigned numSampleValues,
                                       sample_type *theSampleValues);
};

#endif
