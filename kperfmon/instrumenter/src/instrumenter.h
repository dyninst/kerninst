// instrumenter.h
// This class manages the async igen interactions with kerninstd, which because
// they can get kind of ugly, is a good thing to isolate.  Hence, this class is
// your friend.  The outside classes shouldn't have to know about igen.

#ifndef _INSTRUMENTER_H_
#define _INSTRUMENTER_H_

#include <inttypes.h> // for uint64_t, etc.
#include "util/h/Dictionary.h"
#include "kerninstdClient.xdr.CLNT.h"

#ifdef sparc_sun_solaris2_7
#include "downloadToKernel.h" // in kerninstd/igen
#include "nucleusDetector.h" // in kerninstd/kernelDriver
#endif

// fwd decls are cheaper than #include (.o file size-wise); use where possible:
class relocatableCode_t; 
class abi;
class snippet;
class simpleMetricFocus;
class complexMetricFocus;
class sampleHandler;

class instrumenter {
 private:
   kerninstdClientUser *connection_to_kerninstd;
      // just a ptr-copy; i.e. our dtor shouldn't delete this object

   abi *kerninstdABI;
   abi *kernelABI;

   unsigned unique_replacefunction_reqid; // managed by us
   
   unsigned unique_splice_reqid; // chosen by us

   unsigned downloadedToKerninstdCodeId; // managed by us

   unsigned downloadedToKernelCodeId; // managed by us

   unsigned allocateMappedKernelMemoryReqId; // managed by us
   dictionary_hash<unsigned, kptr_t> mappedKernelMemoryRequestsMap;
      // key: reqid; value: kernel addr (can use as key to following:)
   dictionary_hash<kptr_t, pair<dptr_t, unsigned> >
      mappedKernelMemoryRequestsExtraInfoMap;
      // key: address in kernel
      // value.first: address in kerninstd
      // value.second: reqid

   unsigned allocateKerninstdMemoryReqId;
   dictionary_hash<dptr_t, unsigned> allocatedKerninstdRequestsMap;
      // maps kerninstd addresses to reqid's
   
   // prevent copying:
   instrumenter(const instrumenter &);
   instrumenter& operator=(const instrumenter &);

   static unsigned request_id_hash(const unsigned &id);

   void splice_preinsn(unsigned reqid, kptr_t spliceAddr,
                       unsigned numint32regsneeded,
                       unsigned numint64regsneeded,
                       const relocatableCode_t &code_ifnosaveneeded,
                       const relocatableCode_t &code_ifsaveneeded);
   void splice_prereturn(unsigned reqid, kptr_t spliceAddr,
                         unsigned numint32regsneeded,
                         unsigned numint64regsneeded,
                         const relocatableCode_t &code_prereturn_ifnosaveneeded,
                         const relocatableCode_t &code_prereturn_ifsaveneeded,
                         const relocatableCode_t &code_preinsn_ifnosaveneeded,
                         const relocatableCode_t &code_preinsn_ifsaveneeded);

 public:
   instrumenter(kerninstdClientUser *);
  ~instrumenter();

   regset_t*
   getAvailRegsForDownloadedToKerninstdCode(bool ignoreClientIdParam) const;
      // on sparc, returns the %o0 regs (except %sp and %o7 naturally, and if
      // !ignoreClientIdParam, then also except %o0).  If you need more, your
      // code can emit a save and quickly get about 16 additional regs.

   // Q: which is preferable as a parameter: relocatableCode_t or
   // tempBufferEmitter?
   unsigned downloadToKerninstd(const relocatableCode_t &,
                                unsigned entry_chunk_ndx); // returns reqid
   unsigned downloadToKerninstd(const relocatableCode_t &rc); // returns reqid
   unsigned downloadToKerninstd(const tempBufferEmitter &em); // returns reqid
   void downloadToKerninstd(unsigned reqid, const relocatableCode_t &rc);
   void removeDownloadedToKerninstd(unsigned reqid);
      // doesn't try to stop sampling or anything like that
   void periodicallyDoDownloadedToKerninstdCode(unsigned reqid,
                                                unsigned period_millisecs);
   void stopPeriodicallyDoingDownloadedToKerninstdCode(unsigned reqid);
   
   pair<pdvector<dptr_t>, unsigned> queryDownloadedToKerninstdAddresses(unsigned reqid);
      // result.first: chunks   result.second: ndx of the entry chunk
   unsigned reserveNextDownloadedToKerninstdCodeId() {
      // Not a kludge routine: folks who want to emit code to be downloaded into
      // kerninstd may need to know the id ahead of time, if it's to be put in the
      // downloaded code itself, which is indeed the case when generating sampling code.
      return downloadedToKerninstdCodeId++;
   }
   void downloadToKerninstdAndExecuteOnce(relocatableCode_t &);
   void downloadToKerninstdAndExecuteOnce(const tempBufferEmitter &);
   void doDownloadedToKerninstdCodeOnceNow(unsigned reqid);

#ifdef sparc_sun_solaris2_7
   unsigned downloadToKernel(const relocatableCode_t &,
                             unsigned entry_chunk_ndx,
                             bool tryForNucleus);
   unsigned downloadToKernel(const relocatableCode_t &theCode,
                             bool tryForNucleus);
   unsigned downloadToKernel(const tempBufferEmitter &em,
                             bool tryForNucleus); // returns reqid
   unsigned downloadToKernelAndParse(const pdstring &modName,
                                     const pdstring &modDescriptionIfNew,
                                     const pdstring &fnName,
                                     const relocatableCode_t &,
                                     unsigned entry_ndx,
                                     unsigned preFnDataChunkNdxIfAny, // -1U if none
                                     bool tryForNucleus
                                     ); // returns reqid
   unsigned downloadToKernelAndParse(const downloadToKernelFns &,
                                     const emitOrdering &,
                                     bool tryForNucleus);
      // This is the deluxe version, where you specify an emit ordering
      // Outlining uses this version.

   void removeDownloadedToKernel(unsigned reqid);

   pdvector< pair<pdvector<kptr_t>, unsigned> >
   queryDownloadedToKernelAddresses(unsigned reqid);
      // vector: one entry for "fn" that was downloaded (since several can be downloaded
      // at once).
      // result.first: chunks  result.second: ndx of the entry chunk
   pair<pdvector<kptr_t>, unsigned>
   queryDownloadedToKernelAddresses1Fn(unsigned reqid);
      // like above, but asserts that only one "fn" was downloaded for this reqid,
      // and returns that info
#endif

   pdvector<uint32_t> peek_kernel_contig(kptr_t, unsigned);
   pdvector<uint32_t> peek_kerninstd_contig(dptr_t, unsigned);
   int poke_kernel_contig(const pdvector<uint32_t> &, kptr_t, unsigned);

   regset_t* getFreeRegsAtPoint(kptr_t kernelAddr, bool preReturn);

   pair<kptr_t, dptr_t> allocateMappedKernelMemory(unsigned nbytes,
						   bool tryForNucleusText);
      // tryForNucleusText: if true, we try hard to allocate within
      // the kernel's nucleus text, for I-TLB performance.
      // result.first: addr in kernel; result.second: mmapped addr in kerninstd

   dptr_t convertAllocatedMappedKernelAddr2ItsKerninstdAddr(kptr_t kernelAddr) const;
   void freeMappedKernelMemory(kptr_t kernelAddr, dptr_t kerninstdAddr,
                               unsigned nbytes);
   void freeMappedKernelMemory(kptr_t kernelAddr, unsigned nbytes);
      // use this second version in case you forgot the kerninstd address.

   dptr_t allocateKerninstdMemory(unsigned nbytes);
   void freeKerninstdMemory(dptr_t kerninstdAddr, unsigned nbytes);

   pdvector<sample_type> readHwcAllCPUs(unsigned type) const {
      return connection_to_kerninstd->readHwcAllCPUs(type);
   }

   uint64_t getKerninstdMachineTime() const {
      return connection_to_kerninstd->getCurrentKerninstdTime();
   }
   const abi &getKerninstdABI() const {
      assert(kerninstdABI);
      return *kerninstdABI;
   }
   const abi &getKernelABI() const {
      assert(kernelABI);
      return *kernelABI;
   }

   // --------------------

   unsigned replaceAFunction(kptr_t oldFnStartAddr,
                             kptr_t newFnStartAddr,
                             bool replaceCallSitesToo);
      // returns reqid

   void unReplaceAFunction(unsigned reqid);
   
   unsigned replaceAFunctionCall(kptr_t callSiteAddr,
                                 kptr_t newFnStartAddr);
      // returns reqid

   void unReplaceAFunctionCall(unsigned reqid);

   // --------------------

   // These two are fairly low-level; prefer the higher-level
   // "spliceAndSampleInstantiatedMetricFocus" when applicable.
   unsigned splice_preinsn(kptr_t spliceAddr,
                           unsigned numint32regsneeded,
                           unsigned numint64regsneeded,
                           const relocatableCode_t &code_ifnosaveneeded,
                           const relocatableCode_t &code_ifsaveneeded) {
      // no prereturn version yet because I haven't yet found a need for it.
      // (remember, spliceAndSampleInstantiatedMetricFocus() is usually called
      // instead of this routine, anyway).
      unsigned result = unique_splice_reqid++;

      splice_preinsn(result, spliceAddr,
                     numint32regsneeded,
                     numint64regsneeded,
                     code_ifnosaveneeded,
                     code_ifsaveneeded);

      return result;
   }
   
   // These two are fairly low-level; prefer the higher-level
   // "spliceAndSampleInstantiatedMetricFocus" when applicable.
   unsigned splice_prereturn(kptr_t spliceAddr,
			     unsigned numint32regsneeded,
			     unsigned numint64regsneeded,
			     const relocatableCode_t &prereturn_ifnosaveneeded,
			     const relocatableCode_t &prereturn_ifsaveneeded,
			     const relocatableCode_t &preinsn_ifnosaveneeded,
			     const relocatableCode_t &preinsn_ifsaveneeded) {
      // no prereturn version yet because I haven't yet found a need for it.
      // (remember, spliceAndSampleInstantiatedMetricFocus() is usually called
      // instead of this routine, anyway).
      unsigned result = unique_splice_reqid++;

      splice_prereturn(result, spliceAddr,
		       numint32regsneeded,
		       numint64regsneeded,
		       prereturn_ifnosaveneeded,
		       prereturn_ifsaveneeded,
		       preinsn_ifnosaveneeded,
		       preinsn_ifsaveneeded);

      return result;
   }
   
   void unSplice(unsigned splicereqid);

   void requestSampleData(unsigned sampleReqId, const pdvector<kptr_t> &addrs);
   void removeSampleRequest(unsigned sampleReqId);
   void periodicallyDoSampling(unsigned sampleReqId, unsigned ms);
   void stopPeriodicallyDoingSampling(unsigned sampleReqId);
   void doSamplingOnceNow(unsigned sampleReqId);
   void temporarilyTurnOffSampling();
   void resumeSampling();
   
   void presentMachineInfo(unsigned kerninstdABICode,
                           unsigned kernelABICode);
      // abiCode: 0 for SPARC v8plus, 1 for SPARC v9 (x86???)
      // TODO: nucleus info is obviously a little too sparc-specific

#ifdef sparc_sun_solaris2_7
   pcrData getPcr() const;
   void setPcr(const pcrData &newPcrVal);
#elif defined(i386_unknown_linux2_4)
   P4PerfCtrState getP4PerfCtrState();
   void setP4PerfCtrState(const P4PerfCtrState &);
#elif defined(ppc64_unknown_linux2_4)
   pdvector<uint64_t> getPowerPerfCtrState();
   void setPowerPerfCtrState(const pdvector<uint64_t> &newState);
#endif
};

#endif
