// clientConn.h

#ifndef _CLIENT_CONN_H_
#define _CLIENT_CONN_H_

#include "common/h/Vector.h"
#include <poll.h> // pollfd
#include "kerninstdClient.xdr.SRVR.h" // kerninstdClient
#include "reg_set.h"
#include "util/h/Dictionary.h"
#include "util/h/mrtime.h"
#include "relocatableCode.h"
#include "funkshun.h"

class kerninstdClient_withErrorHandle : public kerninstdClient {
 public:
   kerninstdClient_withErrorHandle(int fd) :
      kerninstdClient(fd, NULL, NULL, 0) {
   }
  ~kerninstdClient_withErrorHandle() {}

  void handle_error() {
     cerr << "kerninstdClient: igen error " << get_err_state() << endl;
  }
};

class client_connection {
   typedef function_t::fnCode_t fnCode_t;
   
 public:
   class client_connection_error {};

   struct downloadedIntoKernelEntry1Fn {
      pdvector<kptr_t> logicalChunks; // NOTE: may or may not be contiguous!
      unsigned entry_chunk_ndx;
      
      downloadedIntoKernelEntry1Fn(const pdvector<kptr_t> &ilogicalChunks,
                                   unsigned ientry_chunk_ndx) :
         logicalChunks(ilogicalChunks), entry_chunk_ndx(ientry_chunk_ndx) {
      }
   };
   
 private:
   kerninstdClient_withErrorHandle igenHandle;
   unsigned clientConnId; // unique

   dictionary_hash<unsigned, kptr_t> spliceReqIdMap;
      // hashes spliceReqId (unique for this client) to the address of the
      // instrumentation point

   dictionary_hash<unsigned, pair<kptr_t, kptr_t> > replaceFnIdMap;
      // reqid to pair: old fn (replaced) and new fn (the replacement)

   dictionary_hash<unsigned, pair<kptr_t, kptr_t> > replaceCallIdMap;
      // reqid to pair: callsite and new dest fn

   dictionary_hash<unsigned, pair<pdstring, unsigned> > kernelSymMMapsIntoKerninstd;
      // key: reqid
      // value.first: name of kernel symbol
      // value.second: nbytes of it that are mapped

   struct unmappedKernelMemInfo {
      unsigned nbytes;
      kptr_t kernelAddr;
      kptr_t kernelAddrMappable;
         // usually same as 'kernelAddr'; we don't use it but we need to keep it around
         // for passing to the 'free' routine

      unmappedKernelMemInfo(unsigned inbytes,
                            kptr_t ikernelAddr,
                            kptr_t ikernelAddrMappable) :
         nbytes(inbytes), kernelAddr(ikernelAddr),
         kernelAddrMappable(ikernelAddrMappable) {
      }
   };
   dictionary_hash<unsigned, unmappedKernelMemInfo> allocatedUnmappedKernelMemoryMap;
      // key: reqid
      // value.first: nbytes
      // value.second: kernel address

   dictionary_hash<unsigned, pair<unsigned, void*> > allocatedKerninstdMemoryMap;
      // key: reqid
      // value.first: nbytes
      // value.second: kernel address

   struct mappedKernelMemInfo {
      unsigned nbytes;
      kptr_t kernelAddr;
      kptr_t kernelAddrMappable;
      void *kerninstdAddr;
      mappedKernelMemInfo(unsigned inbytes, kptr_t ikernelAddr,
                          kptr_t ikernelAddrMappable,
                          void *ikerninstdAddr) :
         nbytes(inbytes), kernelAddr(ikernelAddr),
         kernelAddrMappable(ikernelAddrMappable),
         kerninstdAddr(ikerninstdAddr) {
      }
   };
   dictionary_hash<unsigned, mappedKernelMemInfo> allocatedMappedKernelMemoryMap;
      // key: reqid
      // value: nbytes, in-kernel address, in-kerninstd address

   struct downloadedIntoKernelEntry {
      pdvector<downloadedIntoKernelEntry1Fn> fninfo;
      
      kptr_t allocationKernelAddr; // allocation took place here
      unsigned nbytes; // and it was one big blob of this many bytes
         // (irrespective of logicalChunks.size())

      downloadedIntoKernelEntry(const pdvector<downloadedIntoKernelEntry1Fn> &ifns,
                                kptr_t iallocationKernelAddr,
                                unsigned inbytes) :
         fninfo(ifns),
         allocationKernelAddr(iallocationKernelAddr), nbytes(inbytes) {
      }
   };
   dictionary_hash<unsigned, downloadedIntoKernelEntry> downloadedIntoKernelMap;
      // key: reqid (unique for download-into-kernel requests among this client)

   dictionary_hash<unsigned, pair<fnCode_t*, unsigned> > downloadedIntoKerninstdMap;
      // see downloadedIntoKernelMap above for details.

   dictionary_hash<kptr_t, const function_t*> parsedKernelCodeFromClient;
      // Whether "downloaded" into the kernel or put in via a "splice", a copy of
      // the machine code gets stored here so we can, e.g., do a live reg analysis
      // to safety-check that it only overwrites certain regs.

   dictionary_hash<unsigned, pdvector<kptr_t> > sampleRequestMap;
      // key: reqid (unique for sampling requests from this client)

   struct oneThingToDoPeriodically {
      unsigned period_millisecs;
      mrtime_t period_nanosecs;
      mrtime_t lastTimeThisWasDone;

      oneThingToDoPeriodically(unsigned iperiod_millisecs) {
         assert(iperiod_millisecs != 0);
         period_millisecs = iperiod_millisecs;
         period_nanosecs = iperiod_millisecs * 1000000;
         lastTimeThisWasDone = 0;
      }
   };

   dictionary_hash<unsigned, oneThingToDoPeriodically> periodicStuffToDo;
   
   mrtime_t periodicToDoInterval; // in nanosecs
   mrtime_t lastDoTime;
   
   bool samplingIsPresentlyEnabled;
      // usually true; false avoids flooding kperfmon w/ $upcalls; can avoid deadlock
      // in igen.
   
   pdvector<one_sample_data> reusableSampleDataToShipBuffer;

   void process_incoming_igen() throw(client_connection_error);
   void doPeriodicNow(mrtime_t currTime);

   void recalcPeriodicToDoInterval();
      // call, e.g., after something's been deleted from stuffToSample[]

   static unsigned spliceReqIdHash(const unsigned &spliceReqId);

   void downloadToKernel_common(unsigned reqid,
                                const pdvector<downloadToKernelFn> &theFns,
                                   // the above does NOT give an emit ordering, however
                                const pdvector< pair<unsigned, unsigned> > &emitOrdering,
                                   // .first: fn ndx    .second: chunk ndx w/in that fn
                                bool tryForNucleus
                                );
   void destroy1ParsedKernelCode(kptr_t addr);

   client_connection(const client_connection &src);
   client_connection& operator=(const client_connection &src);

 public:

   client_connection(int fd_returned_by_accept, unsigned clientConnId);
  ~client_connection();

   pdvector< pair<unsigned, kptr_t> > getActiveInstrumentationInfo() const {
      // vector of [splice id, splice addr]
      return spliceReqIdMap.keysAndValues();
   }
   
   kerninstdClient &getIGenHandle() {
      return igenHandle;
   }
   
   unsigned getUniqueId() const {
      return clientConnId;
   }
   
   void update_poll_info(pollfd &info, const mrtime_t currtime, int &timeout);

   void process_poll_response(int revents) throw(client_connection_error);

   void do_periodic_if_appropriate();

   void ship_any_buffered_samples();
      // flush.  Useful for, e.g., doDownloadedCodeOnceNow() that has to
      // do with sampling (genesis sample).

   // --------------------

   void hereIsANewReplaceFnId(unsigned replaceFnReqId,
                              kptr_t replacedFnEntryAddr,
                              kptr_t replacementFnEntryAddr);
   kptr_t replaceFnReqId2ReplacedFnEntryAddr(unsigned) const;
   kptr_t replaceFnReqId2ReplacementFnEntryAddr(unsigned) const;
   pdvector< pair<unsigned, pair<kptr_t, kptr_t> > > getAllReplaceFnRequests() const;
   void destroyReplaceFnReqId(unsigned);

   void hereIsANewReplaceCallId(unsigned replaceCallReqId,
                                kptr_t callSiteAddr,
                                kptr_t calleeEntryAddr);
   kptr_t replaceCallReqId2ReplacedCallSiteAddr(unsigned) const;
   kptr_t replaceCallReqId2ReplacedCalleeEntryAddr(unsigned) const;
   pdvector< pair<unsigned, pair<kptr_t, kptr_t> > > getAllReplaceCallRequests() const;
   void destroyReplaceCallReqId(unsigned);

   // --------------------

   void hereIsANewSpliceReqId(unsigned spliceReqId, kptr_t spliceAddr);
   kptr_t spliceReqId2SpliceAddr(unsigned spliceReqId) const;
   void destroySpliceReqId(unsigned spliceReqId);

   // --------------------

   void noteMMapOfAKernelSymIntoKerninstd(unsigned reqid, const pdstring &symName,
                                          unsigned nbytes);
   pair<pdstring, unsigned> getMMapOfAKernelSymInKerninstdInfo(unsigned reqid);
   void mMapOfKernelSymIntoKerninstdResponse(unsigned reqid, kptr_t addr);
   pdvector<pair<pdstring,unsigned> > getAllKernelSymMMaps() const;
      // This last one is usually called when the client is going down, and you
      // want to clean things up.

   
   void destroyKernelSymMMaps();

   void destroyDownloadedToKernelCode();
   void destroyDownloadedToKerninstdCode();

   void destroyAllParsedKernelCode();

   void destroySampleRequests();

   // ------------------------------------------------------------------------
   // -------------------- Allocate Unmapped Kernel Memory -------------------
   // ------------------------------------------------------------------------
   
   // Since we're not going to mmap this memory, we don't round up to page len multiple
   void allocateUnmappedKernelMemory(unsigned reqid,
                                     unsigned nbytes,
                                     bool tryForNucleusText);
   kptr_t queryAllocatedUnmappedKernelMemoryAddr(unsigned reqid);
   void freeAllocatedUnmappedKernelMemory(unsigned reqid);
   void freeAllAllocatedUnmappedKernelMemory();

   // ------------------------------------------------------------------------
   // -------------------- Allocate Kerninstd-only Memory --------------------
   // ------------------------------------------------------------------------
   
   void allocateKerninstdMemory(unsigned reqid, unsigned nbytes);
   void *queryAllocatedKerninstdMemoryAddr(unsigned reqid);
   void freeKerninstdMemory(unsigned reqid);
   void freeAllAllocatedKerninstdMemory();

   // ------------------------------------------------------------------------
   // -------------------- Allocate Mapped Kernel Memory ---------------------
   // ------------------------------------------------------------------------
   
   void allocateMappedKernelMemory(unsigned reqid, unsigned inbytes,
                                   bool tryForNucleusText);
   unsigned queryAllocatedMappedKernelMemoryNumBytes(unsigned reqid);
   kptr_t queryAllocatedMappedKernelMemoryAddrInKernel(unsigned reqid);
   void *queryAllocatedMappedKernelMemoryAddrInKerninstd(unsigned reqid);
   void freeAllocatedMappedKernelMemory(unsigned reqid);
   void freeAllAllocatedMappedKernelMemory();
      // called, e.g., when we detect that a kperfmon has died
   
   // ------------------------------------------------------------------------
   // -------------------- Downloading Code to Kernel ------------------------
   // ------------------------------------------------------------------------

   void downloadToKernel(unsigned reqid,
                         const relocatableCode_t *theCode,
                         unsigned entry_chunk_ndx,
                         bool tryForNucleus);

   void downloadToKernelAndParse(unsigned reqid,
                                 const pdstring &modName,
                                 const pdstring &modDescriptionIfNew,
                                 const pdstring &fnName,
                                 const relocatableCode_t *theCode,
                                 unsigned entry_chunk_ndx, // ndx into above vector
                                 unsigned chunkNdxContainingDataIfAny,
                                 bool tryForNucleus
                                 );
      // emit ordering will be the chunks of "theCode", from 0 to nchunks-1 (plain)

   void downloadToKernelAndParse(unsigned reqid,
                                 const pdvector<downloadToKernelFn> &fns,
                                 const pdvector< pair<unsigned,unsigned> > &emitOrdering,
                                 bool tryForNucleus
                                 );
      // this is the deluxe version; it allows multiple fns to be emitted, *and*
      // it provides for fine-grained emit ordering (on the granularity of chunks).
      // Within emitOrdering, .first is an ndx with "fns", and .second is a chunk ndx
      // of that "fn".
   
   void removeDownloadedToKernel(unsigned reqid);

   const pdvector<downloadedIntoKernelEntry1Fn> &
   queryDownloadedToKernelAddresses(unsigned reqid) const;

   const downloadedIntoKernelEntry1Fn &
   queryDownloadedToKernelAddresses1Fn(unsigned reqid) const;
      // .first: all code chunks (incl. data if any)
      // .second: which code chunk is the entry point

   // --------------------

   struct parseKernelRangeAs1FnInfo {
      // can't be the same as downloadToKernelFn from downloadToKernel.h, because
      // that version uses a relocatableCode_t.  Can't be the same as
      // parseNewFunction from parseNewFunction.h, since that version
      // holds chunk addrs but not fnCode_t.  We augment parseNewFunction:

      parseNewFunction theParseInfo; // see parseNewFunction.h
      fnCode_t theFnCode;

      parseKernelRangeAs1FnInfo(const parseNewFunction &itheParseInfo,
                                const fnCode_t &iFnCode) :
         theParseInfo(itheParseInfo), theFnCode(iFnCode) {
      }
   };
   
   void parseKernelRangesAsFns(const pdvector<parseKernelRangeAs1FnInfo> &);
   
   const function_t *downloadedToKernel2ParsedFnOrNULL(kptr_t fnEntryAddr);
      // returns NULL if this fn isn't being parsed by this client

   // ------------------------------------------------------------------------
   // -------------------- Downloading Code to Kerninstd ---------------------
   // ------------------------------------------------------------------------
   
   void downloadToKerninstd(unsigned reqid, // only unique for this client
                            const relocatableCode_t *theCode,
                            unsigned entry_chunk_ndx);

   void removeDownloadedToKerninstd(unsigned reqid);
      // doesn't try to stop sampling or anything like that.

   pair<pdvector<dptr_t>, unsigned> queryDownloadedToKerninstdAddresses(unsigned reqid);
      // .first: all of the chunks (code or data)
      // .second: which chunk ndx is the entry point

   void doDownloadedToKerninstdCodeOnceNow(unsigned uniqDownloadedCodeId);

   void periodicallyDoDownloadedToKerninstdCode(unsigned uniqDownloadedCodeId,
                                                unsigned period_millisecs);
      // can be used to set or change an interval

   void stopPeriodicallyDoingDownloadedToKerninstdCode(unsigned uniqDownloadedCodeId);

   // --------------------

   void requestSampleData(unsigned uniqSampleReqId, 
                          const pdvector<kptr_t> &addrs);
   void removeSampleRequest(unsigned uniqSampleReqId);
   void periodicallyDoSampling(unsigned uniqSampleReqId, 
                               unsigned period_millisecs);
   void stopPeriodicallyDoingSampling(unsigned uniqSampleReqId);
   void doSamplingOnceNow(unsigned uniqSampleReqId);

   void temporarilyTurnOffSampling();
   void resumeSampling();
   
   void presentSampleData1Value(unsigned sampReqId,
                                uint64_t sampCycles,
                                sample_type sampValue);
   void presentSampleDataSeveralValues(unsigned sampReqId,
                                       uint64_t sampCycles,
                                       unsigned numSampleValues,
                                       sample_type *theSampleValues);
};

#endif
