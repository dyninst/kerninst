// clientConn.C

#if defined (i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4)
#define _XOPEN_SOURCE 500 // for POLLRDNORM in <poll.h>
#define LLONG_MAX 9223372036854775807LL
#endif

#include "clientConn.h"
#include <poll.h>
#include "util/h/minmax.h"
#include "util/h/hashFns.h" // addrHash4()
#include "util/h/rope-utils.h" // addr2hex()
#include "util/h/mrtime.h"
#include <limits.h> // LLONG_MAX
//#include <stropts.h> // INFTIM
#include "util/h/out_streams.h"
#include "kerninstdClient.xdr.h" // T_kerninstdClient
#include "patchHeap.h"
#include "kernelResolver.h" // for an oracle
#include "kerninstdResolver.h" // for an oracle
#include "directToMemoryEmitter.h"
#include "kernelDriver.h"
#include "kmem.h"
#include "moduleMgr.h"
#include "regAnalysisOracles.h" // RegAnalysisHandleCall_AlreadyKnown
#include "instrumentationMgr.h" // recursive #include problem perhaps?
#include <algorithm> // STL's sort()

extern kernelDriver *global_kernelDriver;
extern instrumentationMgr *global_instrumentationMgr;
extern abi *theKernelABI;

static unsigned nsec2msec_roundup(mrtime_t nsec) {
   // nano to milli; rounds up if any leftover
   // msecs = nsecs / million

   return nsec / 1000000 + ((nsec % 1000000 > 0) ? 1 : 0); 
}

unsigned client_connection::spliceReqIdHash(const unsigned &spliceReqId) {
   // a reasonable hash function, since the low bits are quite unique in
   // an id that starts at 0 and gets bumped by 1
   return spliceReqId;
}

client_connection::client_connection(int fd_returned_by_accept,
                                     unsigned iclientConnId) :
         igenHandle(fd_returned_by_accept),
         spliceReqIdMap(spliceReqIdHash),
         replaceFnIdMap(unsignedIdentityHash),
         replaceCallIdMap(unsignedIdentityHash),
         kernelSymMMapsIntoKerninstd(unsignedIdentityHash),
         allocatedUnmappedKernelMemoryMap(unsignedIdentityHash),
         allocatedKerninstdMemoryMap(unsignedIdentityHash),
         allocatedMappedKernelMemoryMap(unsignedIdentityHash),
         downloadedIntoKernelMap(unsignedIdentityHash),
         downloadedIntoKerninstdMap(unsignedIdentityHash),
         parsedKernelCodeFromClient(addrHash4),
         sampleRequestMap(unsignedIdentityHash),
         periodicStuffToDo(unsignedIdentityHash)
{
   clientConnId = iclientConnId;

   samplingIsPresentlyEnabled = true;
   
   // Make a note that we haven't yet done periodic code for this client:
   lastDoTime = 0;

   recalcPeriodicToDoInterval();
}

client_connection::~client_connection() {
   // close igen handle

   // Strong asserts, perhaps too strong:
   assert(spliceReqIdMap.size() == 0);
   assert(replaceFnIdMap.size() == 0);
   assert(replaceCallIdMap.size() == 0);
   assert(allocatedMappedKernelMemoryMap.size() == 0);
   assert(allocatedUnmappedKernelMemoryMap.size() == 0);
   assert(allocatedKerninstdMemoryMap.size() == 0);
   assert(downloadedIntoKernelMap.size() == 0);
   assert(downloadedIntoKerninstdMap.size() == 0);
   assert(parsedKernelCodeFromClient.size() == 0);
   assert(sampleRequestMap.size() == 0);
   assert(periodicStuffToDo.size() == 0);
}

void client_connection::update_poll_info(pollfd &info, const mrtime_t currtime,
                                         int &timeout) {
   info.fd = igenHandle.get_sock();
   info.events = POLLRDNORM;

   if (periodicStuffToDo.size() > 0) { 
      // there's something to do
      
      const mrtime_t nextDoTime = lastDoTime + periodicToDoInterval;
      
      if (currtime >= nextDoTime)
         // time to sample right now...use timeout=0
         timeout = 0;
      else {
         unsigned msecs = nsec2msec_roundup(nextDoTime - currtime);
         int msecs_as_int = msecs;
         if (timeout == -1)
            // timeout hasn't yet been minimized to anything, and ipmin()
            // won't work since timeout not a big positive integer
            timeout = msecs_as_int;
         else
            ipmin(timeout, msecs_as_int);
      }
   }
}

void client_connection::
process_poll_response(int revents) throw(client_connection_error) {
   // If an incoming msg has arrived, process it fully...don't sample
   // until there are no incoming msgs.  This should help avoid deadlock.
   // So this routine is for incoming msgs only.  Don't sample here.

   if (revents & POLLERR)
      throw client_connection_error();
   if (revents & POLLHUP)
      throw client_connection_error();
   if (revents & POLLNVAL)
      throw client_connection_error();

   if (revents & POLLRDNORM) {
      // an igen message has arrived.  Process it.
      try {
         process_incoming_igen(); // can throw a client_connection_error
      }
      catch (client_connection_error) {
         throw client_connection_error();
      }
   }
}

void client_connection::process_incoming_igen() throw(client_connection_error) {
   // We handle buffered request BEFORE handling newly incoming messages.
   // This is important to avoid unintended reordering of messages!
   while (igenHandle.buffered_requests()) {
      T_kerninstdClient::message_tags ret = igenHandle.process_buffered();
      if (ret == T_kerninstdClient::error)
         throw client_connection_error();
   }

   do {
      T_kerninstdClient::message_tags ret = igenHandle.waitLoop();
      if (ret == T_kerninstdClient::error)
         throw client_connection_error();
   } while (!xdrrec_eof(igenHandle.net_obj()));
}

// --------------------

void client_connection::
hereIsANewReplaceFnId(unsigned reqid, kptr_t from, kptr_t to) {
   replaceFnIdMap.set(reqid, make_pair(from, to));
}

kptr_t client_connection::
replaceFnReqId2ReplacedFnEntryAddr(unsigned reqid) const {
   return replaceFnIdMap.get(reqid).first;
}
kptr_t client_connection::
replaceFnReqId2ReplacementFnEntryAddr(unsigned reqid) const {
   return replaceFnIdMap.get(reqid).second;
}

pdvector< pair<unsigned, pair<kptr_t, kptr_t> > >
client_connection::getAllReplaceFnRequests() const {
   return replaceFnIdMap.keysAndValues();
}

void client_connection::
destroyReplaceFnReqId(unsigned reqid) {
   (void)replaceFnIdMap.get_and_remove(reqid);
}

void client_connection::
hereIsANewReplaceCallId(unsigned reqid, kptr_t callsite, kptr_t callee) {
   replaceCallIdMap.set(reqid, make_pair(callsite, callee));
}

kptr_t client_connection::
replaceCallReqId2ReplacedCallSiteAddr(unsigned reqid) const {
   return replaceCallIdMap.get(reqid).first;
}
kptr_t client_connection::
replaceCallReqId2ReplacedCalleeEntryAddr(unsigned reqid) const {
   return replaceCallIdMap.get(reqid).second;
}

pdvector< pair<unsigned, pair<kptr_t, kptr_t> > >
client_connection::getAllReplaceCallRequests() const {
   return replaceCallIdMap.keysAndValues();
}

void client_connection::
destroyReplaceCallReqId(unsigned reqid) {
   (void)replaceCallIdMap.get_and_remove(reqid);
}

// --------------------

void client_connection::hereIsANewSpliceReqId(unsigned spliceReqId,
                                              kptr_t spliceAddr) {
   spliceReqIdMap.set(spliceReqId, spliceAddr);
}

kptr_t client_connection::spliceReqId2SpliceAddr(unsigned spliceReqId) const {
   return spliceReqIdMap.get(spliceReqId);
}

void client_connection::destroySpliceReqId(unsigned spliceReqId) {
   (void)spliceReqIdMap.get_and_remove(spliceReqId);
}

// --------------------

void client_connection::noteMMapOfAKernelSymIntoKerninstd(unsigned reqid,
                                                          const pdstring &symName,
                                                          unsigned nbytes) {
   kernelSymMMapsIntoKerninstd.set(reqid, make_pair(symName, nbytes));
}

pair<pdstring, unsigned>
client_connection::getMMapOfAKernelSymInKerninstdInfo(unsigned reqid) {
   return kernelSymMMapsIntoKerninstd.get(reqid);
}


void client_connection::mMapOfKernelSymIntoKerninstdResponse(unsigned reqid,
                                                             kptr_t addr) {
   igenHandle.mMapOfKernelSymIntoKerninstdResponse(reqid, addr);
}


extern "C" uint64_t ari_get_tick_raw();

void client_connection::doPeriodicNow(mrtime_t currTime) {
   dictionary_hash<unsigned, oneThingToDoPeriodically>::const_iterator iter = periodicStuffToDo.begin();
   dictionary_hash<unsigned, oneThingToDoPeriodically>::const_iterator finish = periodicStuffToDo.end();

   for (; iter != finish; ++iter) {
      // too bad about the const but dictionary_hash doesn't have a non-const iterator.
      const oneThingToDoPeriodically &thing = iter.currval();
      
      if (currTime >= thing.lastTimeThisWasDone + thing.period_nanosecs) {
         const unsigned id = iter.currkey();
         
         periodicStuffToDo.get(id).lastTimeThisWasDone = currTime;
            // should this go first or last?

         if(downloadedIntoKerninstdMap.defines(id)) {
            doDownloadedToKerninstdCodeOnceNow(id);
               // (nice to make double use of this public method)
         }
         else if(sampleRequestMap.defines(id)) {
            doSamplingOnceNow(id);
         }
         else assert(!"unknown id in client_connection::doPeriodicNow()\n");
      }
   }

   // sampling-specific kludge -- we didn't get rid of all of them when we went to
   // downloaded code :)
   ship_any_buffered_samples();
   
   lastDoTime = currTime;
}

void client_connection::do_periodic_if_appropriate() {
   const mrtime_t currtime = getmrtime();
   if (currtime - lastDoTime > periodicToDoInterval)
      doPeriodicNow(currtime);
}

void client_connection::ship_any_buffered_samples() {
   if (reusableSampleDataToShipBuffer.size()) {
      igenHandle.presentSampleData(reusableSampleDataToShipBuffer);
      reusableSampleDataToShipBuffer.clear();
   }
}

void client_connection::recalcPeriodicToDoInterval() {
   periodicToDoInterval = LLONG_MAX; // limits.h (periodicToDoInterval is a long long)

   for (dictionary_hash<unsigned, oneThingToDoPeriodically>::const_iterator iter = periodicStuffToDo.begin(); iter != periodicStuffToDo.end(); ++iter) {
      const unsigned period_millisecs = iter.currval().period_millisecs;
      mrtime_t period_nanosecs = period_millisecs; // mrtime_t is "long long"
      period_nanosecs *= 1000000;
      
      ipmin(periodicToDoInterval, period_nanosecs);
   }
}

pdvector<pair<pdstring, unsigned> >
client_connection::getAllKernelSymMMaps() const {
   return kernelSymMMapsIntoKerninstd.values();
}

void client_connection::destroyKernelSymMMaps() {
   kernelSymMMapsIntoKerninstd.clear();
}

void client_connection::destroyDownloadedToKernelCode() {
   // have to look thru keys since removeDownloadedToKernel() would invalidate
   // iterators
   pdvector<unsigned> reqids = downloadedIntoKernelMap.keys();
   for (pdvector<unsigned>::const_iterator iter = reqids.begin();
        iter != reqids.end(); ++iter) {
      removeDownloadedToKernel(*iter); // reqid
   }
   assert(downloadedIntoKernelMap.size() == 0);
}

void client_connection::destroyDownloadedToKerninstdCode() {
   dictionary_hash<unsigned, pair<fnCode_t*,unsigned> >::const_iterator iter =
      downloadedIntoKerninstdMap.begin();
   dictionary_hash<unsigned, pair<fnCode_t*,unsigned> >::const_iterator finish =
      downloadedIntoKerninstdMap.end();

   for (; iter != finish; ++iter) {
      const pair<fnCode_t*, unsigned> &info = iter.currval();
      
      const fnCode_t *theCode = info.first;

      // For each chunk, call free() to free up kerninstd memory as appropriate.
      // As a reminder, if the chunk size was 0 (and yes, it happens) then we didn't
      // allocate anything after all.
      fnCode_t::const_iterator citer = theCode->begin();
      fnCode_t::const_iterator cfinish = theCode->end();
      for (; citer != cfinish; ++citer) {
         dptr_t addr = citer->startAddr;
         const unsigned nbytes = citer->numBytes();
         if (nbytes > 0)
            free((void*)addr); // we used malloc when creating, so use free now
      }

      delete theCode;
   }

   downloadedIntoKerninstdMap.clear();

   periodicStuffToDo.clear();
   
   recalcPeriodicToDoInterval();
}

void client_connection::destroy1ParsedKernelCode(kptr_t addr) {
   const function_t *pseudo_fn =
      parsedKernelCodeFromClient.get_and_remove(addr);
   assert(pseudo_fn); 
   assert(pseudo_fn->getEntryAddr() == addr);

   // Assert that this function isn't the target of any replaceFunction.
   // For example, if this function is NEW_kmem_alloc, then
   // we have a problem -- someone should have un-replacefn'd first.
   pdvector< pair<kptr_t,kptr_t> > allReplaceFnsThisClient =
      replaceFnIdMap.values(); // TODO -- this is too slow
   pdvector< pair<kptr_t,kptr_t> >::const_iterator iter = allReplaceFnsThisClient.begin();
   pdvector< pair<kptr_t,kptr_t> >::const_iterator finish = allReplaceFnsThisClient.end();
   for (; iter != finish; ++iter) {
      assert(addr != iter->second && "trying to remove a function that is still a replacement for some other function; certainly unsafe!");
      assert(addr != iter->first && "trying to remove a function that is being replaced by some other function; do the proper un-replacement first");
   }
      
   // WARNING: don't call delete on pseudo_fn; class loadedModule prefers to
   // handle its own allocation & deallocation of functions, and has its own
   // HomogenousHeapUni.  Call fry1fn() instead.
   extern moduleMgr *global_moduleMgr;

   const fnCode_t currentFnContents = global_instrumentationMgr->peekCurrFnCode(*pseudo_fn);

   global_moduleMgr->fry1fnAndModuleIfNowEmpty(addr, currentFnContents);
      // Important to pass the actual present contents of the function from memory;
      // don't use the originally parsed insns, since the fn could have been altered
      // with, e.g. updated call sites (replaceFn).
   
   // Inform client that we have removed a function (e.g. so kperfmon can
   // update its where axis display to remove this fn)
   igenHandle.delete_one_function(addr);

   pseudo_fn = NULL; // help purify find memory leaks
}

void client_connection::destroyAllParsedKernelCode() {
   const pdvector<kptr_t> allParsedKernelCode = parsedKernelCodeFromClient.keys();
   
   pdvector<kptr_t>::const_iterator iter = allParsedKernelCode.begin();
   pdvector<kptr_t>::const_iterator finish = allParsedKernelCode.end();
   for (; iter != finish; ++iter) {
      const kptr_t addr = *iter;
      
      destroy1ParsedKernelCode(addr);
   }
}

void client_connection::destroySampleRequests() {
   const pdvector<unsigned> theReqs = sampleRequestMap.keys();
   pdvector<unsigned>::const_iterator iter = theReqs.begin();
   pdvector<unsigned>::const_iterator finish = theReqs.end();

   for (; iter != finish; ++iter) {
      const unsigned reqid = *iter;
      (void)sampleRequestMap.get_and_remove(reqid);
   }
}


// -----------------------------------------------------------------------------
// ----------------------- Allocated, Unmapped Kernel Memory ---------------------
// -----------------------------------------------------------------------------

void client_connection::allocateUnmappedKernelMemory(unsigned reqid,
                                                     unsigned nbytes,
                                                     bool tryForNucleusText) {
   pair<kptr_t, kptr_t> kernelAddrs =
      global_kernelDriver->allocate_kernel_block(nbytes,
                                                 8, // no unusual alignment requirements
                                                 tryForNucleusText);
   // .second contains a mappable kernel address, which by definition we don't
   // care about in this routine.
   assert(kernelAddrs.first != 0 && kernelAddrs.second != 0);

   allocatedUnmappedKernelMemoryMap.set(reqid, unmappedKernelMemInfo(nbytes, kernelAddrs.first, kernelAddrs.second));
}

kptr_t client_connection::queryAllocatedUnmappedKernelMemoryAddr(unsigned reqid) {
   return allocatedUnmappedKernelMemoryMap.get(reqid).kernelAddr;
}

void client_connection::freeAllocatedUnmappedKernelMemory(unsigned reqid) {
   const unmappedKernelMemInfo &info = allocatedUnmappedKernelMemoryMap.get_and_remove(reqid);
   global_kernelDriver->free_kernel_block(info.kernelAddr, info.kernelAddrMappable, info.nbytes);
}

void client_connection::freeAllAllocatedUnmappedKernelMemory() {
   for (dictionary_hash<unsigned, unmappedKernelMemInfo>::const_iterator iter =
           allocatedUnmappedKernelMemoryMap.begin();
        iter != allocatedUnmappedKernelMemoryMap.end(); ++iter) {
      const unmappedKernelMemInfo &info = iter.currval();
      global_kernelDriver->free_kernel_block(info.kernelAddr,
                                             info.kernelAddrMappable,
                                             info.nbytes);
   }

   allocatedUnmappedKernelMemoryMap.clear();
}

// -----------------------------------------------------------------------------
// ----------------------- Kerninstd-only Memory -------------------------------
// -----------------------------------------------------------------------------

void client_connection::allocateKerninstdMemory(unsigned reqid, unsigned nbytes) {
   void *ptr = malloc(nbytes);
   allocatedKerninstdMemoryMap.set(reqid, make_pair(nbytes, ptr));
}

void *client_connection::queryAllocatedKerninstdMemoryAddr(unsigned reqid) {
   return allocatedKerninstdMemoryMap.get(reqid).second;
}

void client_connection::freeKerninstdMemory(unsigned reqid) {
   pair<unsigned, void*> result = allocatedKerninstdMemoryMap.get(reqid);
   free(result.second);
   allocatedKerninstdMemoryMap.undef(reqid);
}

void client_connection::freeAllAllocatedKerninstdMemory() {
   for (dictionary_hash<unsigned, pair<unsigned,void*> >::const_iterator iter =
           allocatedKerninstdMemoryMap.begin();
        iter != allocatedKerninstdMemoryMap.end(); ++iter) {
      pair<unsigned, void*> val = iter.currval();
      //const unsigned nbytes = val.first;
      void *addr = val.second;

      free(addr);
   }

   allocatedKerninstdMemoryMap.clear();
}

// -----------------------------------------------------------------------------
// ----------------------- Allocated, Mapped Kernel Memory ---------------------
// -----------------------------------------------------------------------------

// Keeping track of allocated mapped kernel memory:
void client_connection::allocateMappedKernelMemory(unsigned reqid, unsigned inbytes,
                                                   bool tryForNucleusText) {
   // First, round up the request to a multiple of page length (necessary
   // for mapping).

   const uint32_t nbytes = kernelDriver::roundUpToMultipleOfPageSize(inbytes);
   assert(nbytes >= inbytes);

   pair<kptr_t, kptr_t> kernelAddrs = 
      global_kernelDriver->allocate_kernel_block(nbytes,
                                                 8,
                                                 // we'd love for 8192 (what mmap
                                                 // will prefer) but that's
                                                 // unobtainable in practice.  We
                                                 // can mmap just fine (more or less)
                                                 // with any alignment by mapping
                                                 // an extra page
                                                 tryForNucleusText);
      // .first: the result you'd expect
      // .second: a kernel address perhaps more amenable for mapping
   assert(kernelAddrs.first != 0 && kernelAddrs.second != 0);

   void *inkerninstdBlock = NULL;
   
   inkerninstdBlock = global_kernelDriver->map_for_rw(kernelAddrs.second,
                                                         // yes, .second!!!
                                                      nbytes,
                                                      false);
      // false --> be lenient of non-page-aligned and/or non-page-sized ranges
   
   allocatedMappedKernelMemoryMap.set(reqid,
                                      mappedKernelMemInfo(nbytes,
                                                          kernelAddrs.first,
                                                          kernelAddrs.second,
                                                          inkerninstdBlock));
}

unsigned
client_connection::queryAllocatedMappedKernelMemoryNumBytes(unsigned reqid) {
   return allocatedMappedKernelMemoryMap.get(reqid).nbytes;
}

kptr_t
client_connection::queryAllocatedMappedKernelMemoryAddrInKernel(unsigned reqid) {
   return allocatedMappedKernelMemoryMap.get(reqid).kernelAddr;
}

void *
client_connection::queryAllocatedMappedKernelMemoryAddrInKerninstd(unsigned reqid) {
   return allocatedMappedKernelMemoryMap.get(reqid).kerninstdAddr;
}

void client_connection::freeAllocatedMappedKernelMemory(unsigned reqid) {
   // First unmap, then deallocate.

   const mappedKernelMemInfo &info =
      allocatedMappedKernelMemoryMap.get_and_remove(reqid);
   
   global_kernelDriver->unmap_from_rw(info.kerninstdAddr, info.nbytes);
   global_kernelDriver->free_kernel_block(info.kernelAddr,
                                          info.kernelAddrMappable,
                                          info.nbytes);
}

void client_connection::freeAllAllocatedMappedKernelMemory() {
   pdvector<unsigned> keys = allocatedMappedKernelMemoryMap.keys();
   for (pdvector<unsigned>::const_iterator iter = keys.begin();
        iter != keys.end(); ++iter) {
      unsigned reqid = *iter;
      freeAllocatedMappedKernelMemory(reqid);
   }

   assert(allocatedMappedKernelMemoryMap.size() == 0);
}



// -----------------------------------------------------------------------
// ----------------------- Downloading into Kernel -----------------------
// -----------------------------------------------------------------------

void client_connection::downloadToKernel(unsigned reqid, 
                                         const relocatableCode_t *theCode,
                                         unsigned entry_chunk_ndx,
                                         bool tryForNucleus) {
   pdvector<downloadToKernelFn> fns;
   downloadToKernelFn *fn = fns.append_with_inplace_construction();

   static const pdstring dummyString; // empty string
   new((void*)fn)downloadToKernelFn(dummyString, dummyString, dummyString,
                                    theCode, entry_chunk_ndx,
                                    -1U, // unused since the next param (parse) is false
                                    false);

   // the emit ordering is vanilla:
   pdvector< pair<unsigned,unsigned> > theEmitOrdering;
   const unsigned num_chunks = theCode->numChunks();
   for (unsigned chunk_ndx=0; chunk_ndx < num_chunks; ++chunk_ndx) {
      theEmitOrdering += make_pair(0, chunk_ndx);
   }
   
   downloadToKernel_common(reqid, fns, theEmitOrdering, tryForNucleus);
}

void client_connection::
downloadToKernelAndParse(unsigned reqid, 
                         const pdstring &modName,
                         const pdstring &modDescriptionIfNew,
                         const pdstring &fnName,
                         const relocatableCode_t *theCode,
                         unsigned entry_chunk_ndx,
                         unsigned chunkNdxContainingDataIfAny,
                         bool tryForNucleus
                         ) {
   pdvector<downloadToKernelFn> fns;
   downloadToKernelFn *fn = fns.append_with_inplace_construction();

   new((void*)fn)downloadToKernelFn(modName, modDescriptionIfNew, fnName,
                                    theCode, entry_chunk_ndx,
                                    chunkNdxContainingDataIfAny,
                                    true);

   // the emit ordering is vanilla:
   pdvector< pair<unsigned,unsigned> > theEmitOrdering;
      // .first: fn ndx   .second: chunk ndx w/in that fn

   const unsigned num_chunks = theCode->numChunks();
   for (unsigned chunk_ndx=0; chunk_ndx < num_chunks; ++chunk_ndx)
      theEmitOrdering += make_pair(0, chunk_ndx);
   
   downloadToKernel_common(reqid, fns, theEmitOrdering, tryForNucleus);
}

void client_connection::
downloadToKernelAndParse(unsigned reqid,
                         const pdvector<downloadToKernelFn> &fns,
                         const pdvector< pair<unsigned,unsigned> > &emitOrdering,
                            // .first: fn ndx   .second: chunk ndx w/in that fn
                         bool tryForNucleus
                         ) {
   // This is the deluxe version, where everything is supplied in parameters
   downloadToKernel_common(reqid, fns, emitOrdering, tryForNucleus);
}

// eo_cmp: "emitOrdering comparitor"
static bool eo_cmp(const pair<unsigned,unsigned> &eo1,
                   const pair<unsigned,unsigned> &eo2) {
   if (eo1.first < eo2.first)
      return true;
   if (eo1.first > eo2.first)
      return false;

   if (eo1.second < eo2.second)
      return true;
   return false;
}

void client_connection::
downloadToKernel_common(unsigned reqid,
                        const pdvector<downloadToKernelFn> &fns,
                           // the above does NOT give an emit ordering
                        const pdvector< pair<unsigned,unsigned> > &emitOrdering,
                           // .first: fn ndx    .second: chunk ndx w/in that fn
                        bool tryForNucleus
   ) {
   extern patchHeap *global_patchHeap; // sorry for the hack
   patchHeap &thePatchHeap = *global_patchHeap;

   // STEP 0: some assertions (emitOrdering.size() should equal the total number
   // of chunks)
   unsigned totalNumChunks = 0;
   pdvector<downloadToKernelFn>::const_iterator fns_iter = fns.begin();
   pdvector<downloadToKernelFn>::const_iterator fns_finish = fns.end();
   for (; fns_iter != fns_finish; ++fns_iter) {
      const downloadToKernelFn &fn = *fns_iter;
      totalNumChunks += fn.numChunks();
   }
   assert(emitOrdering.size() == totalNumChunks);

   // Assert that there are no duplicates in emitOrdering:
   pdvector< pair<unsigned, unsigned> > copyof_emitOrdering(emitOrdering);
   sort(copyof_emitOrdering.begin(), copyof_emitOrdering.end(), eo_cmp);
   assert(copyof_emitOrdering.end() ==
          std::adjacent_find(copyof_emitOrdering.begin(),
                             copyof_emitOrdering.end()));

   // STEP 1: figure out the necessary allocation nbytes
   unsigned totalNumBytes = 0;
   fns_iter = fns.begin();
   for (; fns_iter != fns_finish; ++fns_iter) {
      const downloadToKernelFn &fn = *fns_iter;
      totalNumBytes += fn.totalNumBytes(); // should we align?
   }
   
   // STEP 2: Allocate kernel space (one contiguous allocation no matter how many "fns")
   kptr_t addrInKernel = thePatchHeap.alloc(totalNumBytes, tryForNucleus);
   assert(addrInKernel != 0);

   void *addrInKerninstd =
      global_kernelDriver->map_for_rw(addrInKernel,
                                      totalNumBytes,
                                      false // don't assert already page-aligned
                                      );
   assert(addrInKerninstd != NULL);
   
   // STEP 3: calculate kernelFnAddrs[]: the address of each chunk of each downloaded fn
   pdvector< pdvector<kptr_t> > kernelFnAddrs;
      // outer index is the function (relocatableCode_t) ndx: from 0 to fns.size()-1
      // inner index is the chunk ndx of a given relocatableCode_t
   kernelFnAddrs.reserve_exact(fns.size());

   fns_iter = fns.begin();
   for (; fns_iter != fns_finish; ++fns_iter) {
      const downloadToKernelFn &fn = *fns_iter;
      const unsigned fnNumChunks = fn.numChunks();
      assert(fnNumChunks > 0); // too harsh an assert?  Probably not.

      pdvector<kptr_t> *chunkAddrs = kernelFnAddrs.append_with_inplace_construction();
      new((void*)chunkAddrs)pdvector<kptr_t>(fnNumChunks, 0);
   }
   
   pdvector< pair<unsigned,unsigned> >::const_iterator ord_iter = emitOrdering.begin();
   pdvector< pair<unsigned,unsigned> >::const_iterator ord_finish = emitOrdering.end();
   kptr_t currEmitAddr = addrInKernel;
   for (; ord_iter != ord_finish; ++ord_iter) {
      const unsigned fn_ndx = ord_iter->first;
      const unsigned chunk_ndx = ord_iter->second;
      
      assert(kernelFnAddrs[fn_ndx][chunk_ndx] == 0);
      kernelFnAddrs[fn_ndx][chunk_ndx] = currEmitAddr;

      const unsigned chunkNumBytes = fns[fn_ndx].chunkNumBytes(chunk_ndx);
      currEmitAddr += chunkNumBytes; // should we align? (if so coordinate w/ step 1)
   }
   assert(currEmitAddr == addrInKernel + totalNumBytes);

   // STEP 4: fully resolve all fn's (all relocatableCode_t's) and emit into the kernel
   dictionary_hash<pdstring, kptr_t> knownDownloadedCodeAddrs(stringHash);
   fns_iter = fns.begin();
   for (; fns_iter != fns_finish; ++fns_iter) {
      const downloadToKernelFn &fn = *fns_iter;
      if (fn.parseAndAddToModule) {
         pdstring fullName = fn.modName;
         fullName += "/";
         fullName += fn.fnName;

         const kptr_t fnEntryAddr = kernelFnAddrs[fns_iter - fns.begin()][fn.entry_chunk_ndx];
         assert(fnEntryAddr >= addrInKernel && fnEntryAddr < addrInKernel + totalNumBytes);
         knownDownloadedCodeAddrs.set(fullName, fnEntryAddr);
         
         dout << "Known fn name: \"" << fullName << "\" at "
              << addr2hex(fnEntryAddr) << endl;
      }
   }
   
   kernelResolver theKernelResolver(knownDownloadedCodeAddrs);
   pdvector<fnCode_t*> allFnsCodeForParsing;
   allFnsCodeForParsing.reserve_exact(fns.size());
      // an entry will be NULL if we're not parsing a given downloaded fn

   for (unsigned fnlcv=0; fnlcv < fns.size(); ++fnlcv) {
      const downloadToKernelFn &fn = fns[fnlcv];
      fnCode_t *thisFnCode =
         fn.parseAndAddToModule ? new fnCode_t(fnCode_t::empty) : NULL;
      if (fn.parseAndAddToModule)
         assert(thisFnCode);

      const pdvector<kptr_t> &thisFnKernelChunkAddrs = kernelFnAddrs[fnlcv];

      const pdvector<insnVec_t*> &resolved_chunks = 
         fn.theCode->obtainFullyResolvedCode(thisFnKernelChunkAddrs, theKernelResolver);
      assert(resolved_chunks.size() == fn.theCode->numChunks());

      // Emit the chunks into the kernel (must do this now, before parsing)
      for (unsigned chunk_lcv=0; chunk_lcv < resolved_chunks.size(); ++chunk_lcv) {
         const kptr_t chunkAddrInKernel = thisFnKernelChunkAddrs[chunk_lcv];

         const unsigned offsetFromStart = chunkAddrInKernel - addrInKernel;
         assert(offsetFromStart <= totalNumBytes);
            // <= needed due to 0-sized chunks, otherwise I'd make it '<'

         const unsigned chunkNumBytes = fn.theCode->chunkNumBytes(chunk_lcv);
         assert(chunkAddrInKernel + chunkNumBytes <=
                addrInKernel + offsetFromStart + totalNumBytes);
         
         const dptr_t chunkAddrInKerninstd = (dptr_t)addrInKerninstd + offsetFromStart;

         insnVec_t *theResolvedChunk = resolved_chunks[chunk_lcv];
         
         directToMemoryEmitter_t *em =
	   directToMemoryEmitter_t::getDirectToMemoryEmitter(*theKernelABI,
							     (dptr_t)chunkAddrInKerninstd,
							     // in-kerninstd addr
							     chunkAddrInKernel,
							     // in-kernel addr
							     theResolvedChunk->numInsnBytes());
	 
	 
         insnVec_t::const_iterator src_iter = theResolvedChunk->begin();
         insnVec_t::const_iterator src_finish = theResolvedChunk->end();
         while (src_iter != src_finish) {
            em->emit(instr_t::getInstr(**src_iter));
            src_iter++;
         }
         em->complete();
         delete em;
         // If we're parsing this fn, and if the chunk isn't zero-sized, then add
         // it to the fnCode_t.
         if (fn.parseAndAddToModule) {
            assert(thisFnCode);
            if (chunkNumBytes > 0) {
               thisFnCode->addChunk(chunkAddrInKernel, theResolvedChunk, false);
                  // false --> don't resort chunks now
            }
            else
               // We didn't add this chunk; make sure it wasn't the entry ndx
               assert(chunk_lcv != fn.entry_chunk_ndx);
         }
      }

      if (fn.parseAndAddToModule) {
         assert(thisFnCode->numChunks() <= fn.theCode->numChunks());
            // sorry, can't assert '==' because we didn't add 0-sized chunks above
         assert(thisFnCode->numChunks() > 0);
         thisFnCode->sortChunksNow();
      }

      allFnsCodeForParsing += thisFnCode; // could be NULL (if not parsing this fn)
   }

   global_kernelDriver->unmap_from_rw(addrInKerninstd, totalNumBytes);

   // STEP 5: parse fns, where requested
   pdvector<parseKernelRangeAs1FnInfo> parseInfo;
   for (unsigned fnlcv=0; fnlcv < fns.size(); ++fnlcv) {
      const downloadToKernelFn &fn = fns[fnlcv];
      
      if (!fn.parseAndAddToModule)
         continue;

      pdvector<chunkRange> fnChunkRanges;
         // chunkRange is typedef'd to a pair: kptr_t and unsigned (nbytes)
      for (unsigned chunklcv=0; chunklcv < fn.numChunks(); ++chunklcv) {
         const kptr_t chunkStartAddr = kernelFnAddrs[fnlcv][chunklcv];
         assert(chunkStartAddr != 0);
         const unsigned chunkNumBytes = fn.chunkNumBytes(chunklcv);

         chunkRange *cr = fnChunkRanges.append_with_inplace_construction();
         new((void*)cr)chunkRange(chunkStartAddr, chunkNumBytes);
      }
      const parseNewFunction theFnParseInfo(fn.modName,
                                            fn.modDescriptionIfNew,
                                            fn.fnName,
                                            fnChunkRanges,
                                            fn.entry_chunk_ndx,
                                            fn.chunkNdxContainingDataIfAny
                                            );

      assert(allFnsCodeForParsing[fnlcv] != NULL);
      
      parseKernelRangeAs1FnInfo *i = parseInfo.append_with_inplace_construction();
      new((void*)i)parseKernelRangeAs1FnInfo(theFnParseInfo,
                                             *allFnsCodeForParsing[fnlcv]);
   }
   parseKernelRangesAsFns(parseInfo);

   // STEP 6: clean up allFnsCodeForParsing[]:
   for (unsigned fnlcv=0; fnlcv < fns.size(); ++fnlcv) {
      fnCode_t *theFnCode = allFnsCodeForParsing[fnlcv];
      if (!fns[fnlcv].parseAndAddToModule) {
         assert(theFnCode == NULL);
         continue;
      }
      else
         assert(theFnCode);
      
      delete theFnCode;
      allFnsCodeForParsing[fnlcv] = NULL; // help purify find mem leaks
   }

   if (true) {
      const unsigned numFns = fns.size();
      dout << "Downloaded " << numFns << " \"function(s)\" into the kernel" << endl;
      dout << "starting at kernel addr " << addr2hex(addrInKernel)
           << ", " << totalNumBytes << " total num bytes." << endl;

      for (unsigned fnlcv=0; fnlcv < numFns; ++fnlcv) {
         const downloadToKernelFn &fn = fns[fnlcv];
         dout << "fn #" << fnlcv << ": ";
         if (!fn.parseAndAddToModule) {
            dout << "[no name]";
         }
         else
            dout << "\"" << fn.modName << '/' << fn.fnName;
         dout << "\" " << fn.theCode->numChunks() << " chunk(s)" << endl;
      }

      dout << "Chunk ordering [fn index, chunk index w/in that fn]" << endl;
      pdvector< pair<unsigned,unsigned> >::const_iterator eo_iter = emitOrdering.begin(); 
      pdvector< pair<unsigned,unsigned> >::const_iterator eo_finish = emitOrdering.end();
      kptr_t currKernelAddr = addrInKernel;
      for (; eo_iter != eo_finish; ++eo_iter) {
         const pair<unsigned,unsigned> &eo = *eo_iter;

         dout << addr2hex(currKernelAddr) << ' ';
         dout << '[' << eo.first << ", " << eo.second << "]: ";
         const unsigned chunkNumBytes = fns[eo.first].theCode->chunkNumBytes(eo.second);
         dout << chunkNumBytes << " bytes";
         
         if (eo.second == fns[eo.first].entry_chunk_ndx)
            dout << " (the entry chunk of this fn)";
         
         dout << endl;

         currKernelAddr += chunkNumBytes;
      }
      assert(currKernelAddr == addrInKernel + totalNumBytes);
   }

   // STEP 7: add to "downloadedIntoKernelMap"
   {
      pdvector<downloadedIntoKernelEntry1Fn> fnsinfo; // includes 0-sized chunks if any
      for (unsigned fnlcv=0; fnlcv < fns.size(); ++fnlcv) {
         downloadedIntoKernelEntry1Fn *f = fnsinfo.append_with_inplace_construction();
         new((void*)f)downloadedIntoKernelEntry1Fn(kernelFnAddrs[fnlcv],
                                                   fns[fnlcv].entry_chunk_ndx);
      }

      downloadedIntoKernelEntry e(fnsinfo,
                                  addrInKernel, // allocation took place here...
                                  totalNumBytes // ...with this many bytes
                                  );
      downloadedIntoKernelMap.set(reqid, e);
   }
}

const pdvector<client_connection::downloadedIntoKernelEntry1Fn> &
client_connection::queryDownloadedToKernelAddresses(unsigned reqid) const {
   // query the meta-data.  Will include zero-sized chunks, if any
   
   return downloadedIntoKernelMap.get(reqid).fninfo;
}

const client_connection::downloadedIntoKernelEntry1Fn &
client_connection::queryDownloadedToKernelAddresses1Fn(unsigned reqid) const {
   // query the meta-data.  Will include zero-sized chunks, if any
   const pdvector<downloadedIntoKernelEntry1Fn> &initial_result =
      queryDownloadedToKernelAddresses(reqid);
   assert(initial_result.size() == 1);
   return initial_result[0];
}

void client_connection::removeDownloadedToKernel(unsigned reqid) {
   const downloadedIntoKernelEntry &e = 
      downloadedIntoKernelMap.get_and_remove(reqid);

   // deallocate kernel memory:
   const kptr_t kernelAllocationAddr = e.allocationKernelAddr;
   const unsigned allocatedNumBytes = e.nbytes;

   extern patchHeap *global_patchHeap; // sorry for the hack
   global_patchHeap->free(kernelAllocationAddr, allocatedNumBytes);

   // Each "function" may (or may not) have been parsed; if so, fry the appropriate
   // fnCode_t structures:
   for (unsigned fnlcv=0; fnlcv < e.fninfo.size(); ++fnlcv) {
      const downloadedIntoKernelEntry1Fn &fninfo = e.fninfo[fnlcv];
      const kptr_t entryAddr = fninfo.logicalChunks[fninfo.entry_chunk_ndx];
      
      if (parsedKernelCodeFromClient.defines(entryAddr))
         destroy1ParsedKernelCode(entryAddr);
   }
}

// --------------------

void client_connection::
parseKernelRangesAsFns(const pdvector<parseKernelRangeAs1FnInfo> &info) {
   // parseKernelRangeAs1FnInfo is typedef'd in the .h file

   extern moduleMgr *global_moduleMgr;
   const moduleMgr &theModuleMgr = *global_moduleMgr;

   // STEP 1: Parse all fns being downloaded, without doing live reg analysis on any
   // of them.  This is needed lest live reg analysis not recognize that a call to
   // another function w/in this downloaded-fns-group is a legit call, since the
   // callee hasn't yet been parsed.  Also collect the boolean vector
   // "willBeFirstFnInModule", needed to distinguish whether we should send a
   // module_information() verses a one_function_information() later on in this routine.

   pdvector<pdstring> entireModulesBeingSent;
      // modName
   
   pdvector<parseKernelRangeAs1FnInfo>::const_iterator fns_iter = info.begin();
   pdvector<parseKernelRangeAs1FnInfo>::const_iterator fns_finish = info.end();
   for (; fns_iter != fns_finish; ++fns_iter) {
      const parseNewFunction &fnParseInfo = fns_iter->theParseInfo;
      const fnCode_t &theFnCode = fns_iter->theFnCode;

      const kptr_t fnEntryAddr =
         fnParseInfo.chunkAddrs[fnParseInfo.entry_chunk_ndx].first;
      
      assert(theFnCode.enclosesAddr(fnEntryAddr));

      const pdstring &modName = fnParseInfo.modName;
      const pdstring &modDescriptionIfNew = fnParseInfo.modDescriptionIfNew;
      
      // Does "modName" already exist?
      if (theModuleMgr.find(modName) == NULL) {
#ifdef ppc64_unknown_linux2_4
//there is no reason this code should ever be executed on power,
//but if it is, we need to properly adjust TOCPtr in the call below.  
//Use kerninstapi instead of this code. 
assert(false && "downloading functions to kernel unsupported on power");
#endif      
         // Create a new module, using "modName" and "modDescriptionIfNew"
         global_moduleMgr->addEmptyModule(modName,
                                          modDescriptionIfNew,
                                          NULL, // no string pool for us
                                          0, // 0 bytes in string pool
                                          0 //TOCPtr unknown
                                          );
         assert(theModuleMgr.find(modName) != NULL);

         entireModulesBeingSent += modName;
      }

      const pdstring &fnName = fnParseInfo.fnName;
      
      const function_t *fn = global_moduleMgr->
         addFnToModuleButDontParseYet(modName,
                                      fnEntryAddr,
                                      fnName,
                                      false // don't also remove aliases now
                                      );
         // doesn't add to functionFinder, so this fn can't be searched for (yet)

      try {
         dictionary_hash<kptr_t,bool> ignore_interProcJumps(addrHash4);
         pdvector<hiddenFn> ignore_hiddenFns; // in theory, we shouldn't ignore these

         global_moduleMgr->parseCFG_forFn(modName,
                                          *fn,
                                          theFnCode,
                                          fns_iter->theParseInfo.chunk_ndx_containing_data_if_any,
                                             // -1U if none
                                          ignore_interProcJumps,
                                          ignore_hiddenFns,
                                          true // strong asserts when adding to call graph
                                          );
            // This will add the function to "functionFinder"

         parsedKernelCodeFromClient.set(fnEntryAddr, fn);
      }
      catch (function_t::parsefailed) {
         cout << "could not parse downloaded-to-kernel code as a new function" << endl;
      }

      // Now assert fn is searchable w/in functionFinder (should hold even if parsing
      // failed)
      assert(&theModuleMgr.findFn(fnEntryAddr, true) == fn);
   }
   
   // STEP 2: Do live register analysis.  This had to wait until all parsing was done,
   // lest a call to a not-yet-parsed fn within this group seem invalid.
   for (fns_iter = info.begin(); fns_iter != fns_finish; ++fns_iter) {
      const parseNewFunction &fnParseInfo = fns_iter->theParseInfo;

      const kptr_t fnEntryAddr =
         fnParseInfo.chunkAddrs[fnParseInfo.entry_chunk_ndx].first;

      // no need to bracket the following call with try; all exceptions are
      // caught & handled appropriatly for us
      global_moduleMgr->
         doInterProcRegisterAnalysis1FnOnly_CallsAlreadyKnown_BailOnRecursion
         (fnEntryAddr, false); // not verbose
   }

   // STEP 3: let kperfmon know about these new function(s), even if parsing failed
   // for one or more of them.  Start by sending entire modules for all modules
   // within "entireModulesBeingSent[]".
   for (pdvector<pdstring>::const_iterator modname_iter = entireModulesBeingSent.begin();
        modname_iter != entireModulesBeingSent.end(); ++modname_iter) {
      const pdstring &modName = *modname_iter;
      
      const loadedModule *mod = global_moduleMgr->find(modName);
      assert(mod);
      
      igenHandle.begin_module_information();
      igenHandle.module_information(trick_module(mod));
      igenHandle.end_module_information();
   }
   
   for (fns_iter = info.begin(); fns_iter != fns_finish; ++fns_iter) {
      const parseNewFunction &fnParseInfo = fns_iter->theParseInfo;
      
      const pdstring &modName = fnParseInfo.modName;

      if (std::find(entireModulesBeingSent.begin(), entireModulesBeingSent.end(),
               modName) != entireModulesBeingSent.end())
         // we already sent all fns of this module, so we certainly don't want
         // to re-send this particular fn via a one_function_information() igen call.
         continue;
      
      const kptr_t fnEntryAddr =
         fnParseInfo.chunkAddrs[fnParseInfo.entry_chunk_ndx].first;

      const pair<pdstring, const function_t*> modAndFn =
         global_moduleMgr->findModAndFn(fnEntryAddr, true); // true --> start of fn only
      assert(modAndFn.first == modName);
      assert(modAndFn.second->getEntryAddr() == fnEntryAddr);

      const loadedModule *mod = theModuleMgr.find(modName);
      assert(mod);

      assert(mod->numFns() > 0);

      igenHandle.one_function_information(mod->getName(), trick_fn(modAndFn.second));

//        cout << "kerninstd just did a one_function_information() igen call for "
//             << addr2hex(fnEntryAddr) << endl;
   }
}

const function_t *
client_connection::downloadedToKernel2ParsedFnOrNULL(kptr_t fnEntryAddr) {
   const function_t *result = NULL;
   if (parsedKernelCodeFromClient.find(fnEntryAddr, result))
      return result;
   else
      return NULL;
}

// -----------------------------------------------------------------------
// ----------------------- Downloading into Kerninstd --------------------
// -----------------------------------------------------------------------

void client_connection::
downloadToKerninstd(unsigned uniqDownloadedCodeId,
                    const relocatableCode_t *theCode,
                    unsigned entry_chunk_ndx) {
   fnCode_t *theResolvedChunks = new fnCode_t(fnCode_t::empty);

   const unsigned numChunks = theCode->numChunks();

   pdvector<kptr_t> kerninstdAddrs;

   // Allocate
   for (unsigned chunk_lcv=0; chunk_lcv < numChunks; ++chunk_lcv) {
      const unsigned chunk_nbytes = theCode->chunkNumBytes(chunk_lcv);

      void *theCodePtr = NULL;
      if (chunk_nbytes > 0) {
         theCodePtr = malloc(chunk_nbytes);
         assert(theCodePtr);
      }

      kerninstdAddrs += (dptr_t)theCodePtr;
   }

   // Resolve
   kerninstdResolver theResolver; // oracle
   const pdvector<insnVec_t*> &resolved_code =
      theCode->obtainFullyResolvedCode(kerninstdAddrs, theResolver);

   // Emit
   for (unsigned chunk_lcv=0; chunk_lcv < numChunks; ++chunk_lcv) {
      extern abi *theKerninstdABI; // main.C
      directToMemoryEmitter_t *em = directToMemoryEmitter_t::getDirectToMemoryEmitter(
                                                                                    *theKerninstdABI,
                                                                                    kerninstdAddrs[chunk_lcv],
                                                                                    0, theCode->chunkNumBytes(chunk_lcv));

      insnVec_t *resolved_chunk = resolved_code[chunk_lcv];
      assert(resolved_chunk->numInsnBytes() == theCode->chunkNumBytes(chunk_lcv));

      insnVec_t::const_iterator iter = resolved_chunk->begin();
      insnVec_t::const_iterator finish = resolved_chunk->end();
      for (; iter != finish; ++iter) {
         instr_t *instr = instr_t::getInstr(**iter);
         em->emit(instr);
      }
      em->complete(); // flushes icache

      // theCodePtr points to pre-code data, if any.  Allocation began there.
      // That's what we want to store into downloadedIntoKerninstdMap.

//     dout << "Downloaded [to kerninstd] code at " << theCodePtr << ", lasts "
//          << em->numInsnBytesEmitted() << " bytes" << endl;

      delete em;

      theResolvedChunks->addChunk(kerninstdAddrs[chunk_lcv], // addr in kerninstd
                                  resolved_chunk, false);
         // false --> don't resort now
   }
   theResolvedChunks->sortChunksNow();
   
   assert(theResolvedChunks->numChunks() == theCode->numChunks());

   downloadedIntoKerninstdMap.set(uniqDownloadedCodeId,
                                  make_pair(theResolvedChunks, entry_chunk_ndx));
}

void client_connection::removeDownloadedToKerninstd(unsigned reqid) {
   // doesn't try to stop sampling or anything like that

   const pair<fnCode_t*, unsigned> &info = 
      downloadedIntoKerninstdMap.get_and_remove(reqid);

   // free() each chunk:
   fnCode_t::const_iterator citer = info.first->begin();
   fnCode_t::const_iterator cfinish = info.first->end();
   for (; citer != cfinish; ++citer) {
      dptr_t addr = citer->startAddr;
      const unsigned nbytes = citer->theCode->numInsnBytes();

      // reminder: if nbytes was 0 (and yes, it happens) then we didn't
      // allocate anything.
      if (nbytes > 0)
         free((void*)addr);
   }
   
   // fry the fnCode_t structure:
   delete info.first;
}

pair<pdvector<dptr_t>, unsigned>
client_connection::queryDownloadedToKerninstdAddresses(unsigned reqid) {
   const pair<fnCode_t*, unsigned> &info = downloadedIntoKerninstdMap.get(reqid);

   pdvector<dptr_t> result;
   fnCode_t::const_iterator citer = info.first->begin();
   fnCode_t::const_iterator cfinish = info.first->end();
   for (; citer != cfinish; ++citer)
      result += citer->startAddr;
   
   return make_pair(result, info.second);
}

void client_connection::
doDownloadedToKerninstdCodeOnceNow(unsigned uniqDownloadedCodeId) {
   const pair<fnCode_t*, unsigned> &info = downloadedIntoKerninstdMap.get(uniqDownloadedCodeId);
      // info.second holds the ndx of the entry chunk

   dptr_t entryAddr = (info.first->begin() + info.second)->startAddr;
   void (*theCodePtr)(kerninstdClient*) = (void (*)(kerninstdClient*))entryAddr;

   theCodePtr(&igenHandle);
}

void client_connection::
periodicallyDoDownloadedToKerninstdCode(unsigned uniqDownloadedCodeId,
                                        unsigned period_millisecs) {
   if (!downloadedIntoKerninstdMap.defines(uniqDownloadedCodeId)) {
      cout << "periodicallyDoDownloadedCode: no such downloadedCodeId -- ignoring"
           << endl;
      return;
   }
   assert(period_millisecs != 0);
   
   if (periodicStuffToDo.defines(uniqDownloadedCodeId)) {
      // change the periodic interval
       oneThingToDoPeriodically &job = 
	   periodicStuffToDo.get(uniqDownloadedCodeId);
       job.period_millisecs = period_millisecs;
       job.period_nanosecs = 1000000 * period_millisecs;
   }
   else
      // set periodic interval for the first time
      periodicStuffToDo.set(uniqDownloadedCodeId,
                            oneThingToDoPeriodically(period_millisecs));

   recalcPeriodicToDoInterval();
}

void client_connection::
stopPeriodicallyDoingDownloadedToKerninstdCode(unsigned uniqDownloadedCodeId) {
   periodicStuffToDo.undef(uniqDownloadedCodeId);

   recalcPeriodicToDoInterval();
}

void client_connection::requestSampleData(unsigned uniqSampleReqId, 
                                          const pdvector<kptr_t> &addrs) {
   sampleRequestMap.set(uniqSampleReqId, addrs);
}

void client_connection::removeSampleRequest(unsigned uniqSampleReqId) {
   (void)sampleRequestMap.get_and_remove(uniqSampleReqId);
}

void client_connection::periodicallyDoSampling(unsigned uniqSampleReqId,
                                               unsigned period_millisecs) {
   
   if (!sampleRequestMap.defines(uniqSampleReqId)) {
      cout << "periodicallyDoSampling: no such sampleRequestId -- ignoring"
           << endl;
      return;
   }
   
   if (periodicStuffToDo.defines(uniqSampleReqId)) {
      // change the periodic interval
       oneThingToDoPeriodically &job = 
	   periodicStuffToDo.get(uniqSampleReqId);
       job.period_millisecs = period_millisecs;
       job.period_nanosecs = 1000000 * period_millisecs;
   }
   else
      // set periodic interval for the first time
      periodicStuffToDo.set(uniqSampleReqId,
                            oneThingToDoPeriodically(period_millisecs));

   recalcPeriodicToDoInterval();
}

void client_connection::
stopPeriodicallyDoingSampling(unsigned uniqSampleReqId) {
   periodicStuffToDo.undef(uniqSampleReqId);

   recalcPeriodicToDoInterval();
}

static uint64_t firstSampCycles = 0;

void client_connection::
doSamplingOnceNow(unsigned uniqSampleReqId) {
   const pdvector<kptr_t> &addrs = sampleRequestMap.get(uniqSampleReqId);

   uint64_t sampleCycles = igenHandle.getCurrentKerninstdTime();
   if (firstSampCycles == 0)
      firstSampCycles = sampleCycles;

   if(sampleCycles < firstSampCycles)      
      assert(!"rollback in client_connection::doSamplingOnceNow()\n");

   unsigned numSampleValues = addrs.size();
   sample_type *theSampleValues = (sample_type*) malloc(numSampleValues*sizeof(sample_type));
   if(theSampleValues == NULL) assert(false);

   sample_type *tmp = theSampleValues;
   assert((sizeof(sample_type) == 8) && "sample_type size is not as expected");
   for(unsigned i=0; i<numSampleValues; i++, tmp++) {
#ifndef sparc_sun_solaris2_7
      // when not on sparc/solaris, sample data not mapped into kerninstd,
      // so we need to read kernel mem
      uint32_t nbytes_read = global_kernelDriver->peek_block(tmp, addrs[i], 8, false);
      assert(nbytes_read == 8);
#else
      *tmp = *(sample_type*)addrs[i];
#endif
   }

   presentSampleDataSeveralValues(uniqSampleReqId, sampleCycles, 
                                  numSampleValues, theSampleValues);
}

void client_connection::temporarilyTurnOffSampling() {
   samplingIsPresentlyEnabled = false;
}

void client_connection::resumeSampling() {
   samplingIsPresentlyEnabled = true;
}


static uint64_t reusable_buffer_last_time = 0;
static const unsigned maxReusableSampleDataToShipEntries = 50;

void client_connection::presentSampleData1Value(unsigned sampReqId,
                                                uint64_t sampCycles,
                                                sample_type sampValue) {
   if (!samplingIsPresentlyEnabled)
      return;
   
   // check for rollback:
   if (reusable_buffer_last_time == 0)
      reusable_buffer_last_time = sampCycles;
   assert(sampCycles >= reusable_buffer_last_time &&
          "rollback in client_connection::presentSampleData1Value");

   // add to 'reusableSampleDataToShipBuffer'
   one_sample_data item;
   item.timeOfSample = sampCycles;
   item.sampling_cpu_id = 0; // for now
   item.uniqSampleReqId = sampReqId;
   item.values.reserve_exact(1);
   item.values += sampValue;
   assert(item.values.size() == 1);
   assert(item.values[0] == sampValue);

   //dout << "Buffered up value " << sampValue << endl;

   reusableSampleDataToShipBuffer += item;

   if (reusableSampleDataToShipBuffer.size() >= maxReusableSampleDataToShipEntries) {
      igenHandle.presentSampleData(reusableSampleDataToShipBuffer);
      reusableSampleDataToShipBuffer.clear();
   }
}

void client_connection::
presentSampleDataSeveralValues(unsigned sampReqId,
                               uint64_t sampCycles,
                               unsigned numSampleValues,
                               sample_type *theSampleValues) {
   if (!samplingIsPresentlyEnabled)
      return;
   
   // check for rollback:
   if (reusable_buffer_last_time == 0)
      reusable_buffer_last_time = sampCycles;
   assert(sampCycles >= reusable_buffer_last_time &&
          "rollback in client_connection::presentSampleDataSeveralValues");

   // add to 'reusableSampleDataToShipBuffer'
   one_sample_data item;
   item.timeOfSample = sampCycles;
   item.sampling_cpu_id = 0; // for now
   item.uniqSampleReqId = sampReqId;
   item.values.reserve_exact(numSampleValues);

   while (numSampleValues--)
      item.values += *theSampleValues++;
   
   reusableSampleDataToShipBuffer += item;
}
