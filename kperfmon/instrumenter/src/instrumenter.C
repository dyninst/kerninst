// instrumenter.C

#include "instrumenter.h"
#include "util/h/rope-utils.h"
#include "relocatableCode.h"

#include "util/h/out_streams.h"
#include "util/h/hashFns.h"

#ifdef sparc_sun_solaris2_7
#include "sparcv8plus_abi.h"
#include "sparcv9_abi.h"
#elif defined(i386_unknown_linux2_4)
#include "x86_abi.h"
#elif defined(ppc64_unknown_linux2_4)
#include "power64_abi.h"
#endif

instrumenter::instrumenter(kerninstdClientUser *iconnection_to_kerninstd) :
      connection_to_kerninstd(iconnection_to_kerninstd),
      mappedKernelMemoryRequestsMap(request_id_hash),
      mappedKernelMemoryRequestsExtraInfoMap(addrHash4),
      allocatedKerninstdRequestsMap(addrHash4)
{
   kerninstdABI = NULL;
   kernelABI = NULL;
   unique_replacefunction_reqid = 0;
   unique_splice_reqid = 0;
   downloadedToKerninstdCodeId = 1000; // not 0 for debugging purposes
   downloadedToKernelCodeId = 2000; // not 0 for debugging purposes
   allocateMappedKernelMemoryReqId = 0;
   allocateKerninstdMemoryReqId = 0;
}

instrumenter::~instrumenter() {
   delete kerninstdABI;
   delete kernelABI;
}

unsigned instrumenter::request_id_hash(const unsigned &iid) {
   // a static method
   unsigned id = iid;
   
   return id; // should work well enough given that ids are simply increased by
              // 1 all of the time
}

#ifdef sparc_sun_solaris2_7
sparc_reg_set sanitize_freeregs_atpoint(const sparc_reg_set &src) {
   sparc_reg_set result = src;

   // int regs only please
   result &= sparc_reg_set::allIntRegs;

   // Never use the following 3 regs, for safety:
   result -= sparc_reg::o6;
   result -= sparc_reg::i6;
   result -= sparc_reg::g7;

   // Never try to use %g0:
   result -= sparc_reg::g0;

   return result;
}
#endif

/* ******************************************************************* */

regset_t* instrumenter::
getAvailRegsForDownloadedToKerninstdCode(bool ignoreClientIdParam) const {
#ifdef sparc_sun_solaris2_7
   sparc_reg_set *result = (sparc_reg_set*)kerninstdABI->getCallerSavedRegs();
   assert(!result->exists(sparc_reg::sp));
   assert(!result->exists(sparc_reg::o7));

   if (!ignoreClientIdParam) {
      // %o0 is client-id or something like that, so we need to remove it from the set.
      assert(result->exists(sparc_reg::o0));
      (*result) -= sparc_reg::o0;
   }
#elif defined(i386_unknown_linux2_4)
   x86_reg_set *result = (x86_reg_set*)kerninstdABI->getCallerSavedRegs();
   // on x86, the client-id (or whatever) would be on the stack, not in a reg.
   // thus, we ignore ignoreClientIdParam
#elif defined(ppc64_unknown_linux2_4)
   power_reg_set *result = (power_reg_set *)kerninstdABI->getCallerSavedRegs();
  //no such thing as clientID on power, as far as I know
#endif

   return result;
}

unsigned instrumenter::downloadToKerninstd(const relocatableCode_t &theCode,
                                           unsigned entry_ndx) {
   const unsigned reqid = downloadedToKerninstdCodeId++;
   trick_relocatableCode tr(&theCode);
   connection_to_kerninstd->downloadToKerninstd(reqid, tr, entry_ndx);

   return reqid;
}

unsigned instrumenter::
downloadToKerninstd(const tempBufferEmitter &em) { // returns reqid
   assert(em.numChunks() == 1);
#ifdef sparc_sun_solaris2_7
   sparc_relocatableCode rc(em);
#elif defined(i386_unknown_linux2_4)
   x86_relocatableCode rc(em);
#elif defined(ppc64_unknown_linux2_4)
   power_relocatableCode rc(em);
#endif
   return downloadToKerninstd(rc);
}

unsigned instrumenter::
downloadToKerninstd(const relocatableCode_t &rc) { // returns reqid
   assert(rc.numChunks() == 1);
   return downloadToKerninstd(rc, 0);
}

void instrumenter::downloadToKerninstd(unsigned reqid,
				       const relocatableCode_t &rc)
{
   trick_relocatableCode trick(&rc);
   connection_to_kerninstd->downloadToKerninstd(reqid, trick, 0);
}

void instrumenter::removeDownloadedToKerninstd(unsigned reqid) {
   // doesn't try to stop sampling or anything like that
   connection_to_kerninstd->removeDownloadedToKerninstd(reqid);
}

void instrumenter::periodicallyDoDownloadedToKerninstdCode(unsigned reqid,
							   unsigned millisecs)
{
   connection_to_kerninstd->
      periodicallyDoDownloadedToKerninstdCode(reqid, millisecs);
}

void instrumenter::stopPeriodicallyDoingDownloadedToKerninstdCode(
    unsigned reqid)
{
   connection_to_kerninstd->
      stopPeriodicallyDoingDownloadedToKerninstdCode(reqid);
}

pair<pdvector<dptr_t>, unsigned> // .first: chunks  .second: which chunk is the entry
instrumenter::queryDownloadedToKerninstdAddresses(unsigned reqid) {
   return connection_to_kerninstd->queryDownloadedToKerninstdAddresses(reqid);
}

#ifdef sparc_sun_solaris2_7
unsigned instrumenter::downloadToKernel(const relocatableCode_t &theCode,
                                        unsigned entry_chunk_ndx,
                                        bool tryForNucleus) {
   const unsigned reqid = downloadedToKernelCodeId++;
   assert(entry_chunk_ndx < theCode.numChunks());
   trick_relocatableCode trick(&theCode);

   connection_to_kerninstd->downloadToKernel(reqid, trick, entry_chunk_ndx,
                                             tryForNucleus);

   return reqid;
}

unsigned instrumenter::downloadToKernel(const relocatableCode_t &theCode,
                                        bool tryForNucleus) {
   assert(theCode.numChunks() == 1);
   return downloadToKernel(theCode, 0, tryForNucleus);
}

unsigned instrumenter::
downloadToKernel(const tempBufferEmitter &em, bool tryForNucleus) { 
   // returns reqid
   assert(em.numChunks() == 1);
   sparc_relocatableCode rc(em);
   assert(rc.numChunks() == 1);
   return downloadToKernel(rc, tryForNucleus);
}

unsigned instrumenter::downloadToKernelAndParse(const pdstring &modName,
                                                const pdstring &modDescriptionIfNew,
                                                const pdstring &fnName,
                                                const relocatableCode_t &theCode,
                                                unsigned entry_chunk_ndx,
                                                unsigned preFnDataChunkIfAny,
                                                bool tryForNucleus
                                                ) {
   assert(entry_chunk_ndx < theCode.numChunks());
   
   const unsigned reqid = downloadedToKernelCodeId++;
   trick_relocatableCode trick(&theCode);
   
   connection_to_kerninstd->download1FnToKernelAndParse(reqid, modName, 
							modDescriptionIfNew,
                                                        fnName, trick, 
							entry_chunk_ndx,
                                                        preFnDataChunkIfAny,
                                                        tryForNucleus);

   return reqid;
}

unsigned instrumenter::downloadToKernelAndParse(const downloadToKernelFns &fns,
                                                const emitOrdering &eo,
                                                bool tryForNucleus) {
   const unsigned reqid = downloadedToKernelCodeId++;
   
   connection_to_kerninstd->downloadSeveralFnsToKernelAndParse(reqid, fns, eo,
                                                               tryForNucleus);
   
   return reqid;
}

void instrumenter::removeDownloadedToKernel(unsigned reqid) {
   connection_to_kerninstd->removeDownloadedToKernel(reqid);
}

pdvector< pair<pdvector<kptr_t>, unsigned> >
instrumenter::queryDownloadedToKernelAddresses(unsigned reqid) {
   return connection_to_kerninstd->queryDownloadedToKernelAddresses(reqid);
}

pair<pdvector<kptr_t>, unsigned>
instrumenter::queryDownloadedToKernelAddresses1Fn(unsigned reqid) {
   pdvector< pair<pdvector<kptr_t>, unsigned> > result =
      connection_to_kerninstd->queryDownloadedToKernelAddresses(reqid);
   assert(result.size() == 1);
   return result[0];
}
#endif

pdvector<uint32_t> instrumenter::peek_kernel_contig(kptr_t start, unsigned len) {
   return connection_to_kerninstd->peek_kernel_contig(start, len);
}

pdvector<uint32_t> instrumenter::peek_kerninstd_contig(dptr_t startAddr, unsigned len) {
   return connection_to_kerninstd->peek_kerninstd_contig(startAddr, len);
}

int instrumenter::poke_kernel_contig(const pdvector<uint32_t> &data, 
				     kptr_t addr, unsigned len)
{
    return connection_to_kerninstd->poke_kernel_contig(data, addr, len);
}

regset_t* instrumenter::getFreeRegsAtPoint(kptr_t kernelAddr,
                                               bool preReturn) {
   trick_regset trick = 
      connection_to_kerninstd->getFreeRegsAtPoint(kernelAddr, preReturn);
#ifdef sparc_sun_solaris2_7
   regset_t *result = regset_t::getRegSet(*(const sparc_reg_set*)trick.get());
#elif defined(i386_unknown_linux2_4)
   regset_t *result = regset_t::getRegSet(*(const x86_reg_set*)trick.get());
#elif defined(ppc64_unknown_linux2_4)
   regset_t *result = regset_t::getRegSet(*(const power_reg_set*)trick.get());
#endif
   delete trick.get();
   return result;
}

dptr_t instrumenter::allocateKerninstdMemory(unsigned nbytes) {
   unsigned reqid = allocateKerninstdMemoryReqId++;
   connection_to_kerninstd->allocateKerninstdMemory(reqid, nbytes);
   unsigned addrInKerninstd =
      connection_to_kerninstd->queryAllocatedKerninstdMemoryAddr(reqid);
   
   allocatedKerninstdRequestsMap.set(addrInKerninstd, reqid);

   return addrInKerninstd;
}

void instrumenter::freeKerninstdMemory(dptr_t addrInKerninstd, 
				       unsigned /*nbytes*/) {
   unsigned reqid = allocatedKerninstdRequestsMap.get(addrInKerninstd);

   connection_to_kerninstd->freeAllocatedKerninstdMemory(reqid);

   allocatedKerninstdRequestsMap.undef(addrInKerninstd);
}



pair<kptr_t, dptr_t>
instrumenter::allocateMappedKernelMemory(unsigned nbytes,
                                         bool tryForNucleusText) {
   // tryForNucleusText: if true, we try hard to allocate within
   // the kernel's nucleus text, for I-TLB performance.

   unsigned reqid = allocateMappedKernelMemoryReqId++;
   connection_to_kerninstd->allocateMappedKernelMemory(reqid, nbytes,
                                                       tryForNucleusText);

   kptr_t addrInKernel =
      connection_to_kerninstd->queryAllocatedMappedKernelMemoryAddrInKernel(reqid);
   dptr_t addrInKerninstd =
      connection_to_kerninstd->queryAllocatedMappedKernelMemoryAddrInKerninstd(reqid);

   // update some meta-data we keep concerning mapped kernel memory
   mappedKernelMemoryRequestsMap.set(reqid, addrInKernel);
   mappedKernelMemoryRequestsExtraInfoMap.set(addrInKernel,
                                              make_pair(addrInKerninstd, reqid));
   
   return make_pair(addrInKernel, addrInKerninstd);
}

dptr_t instrumenter::
convertAllocatedMappedKernelAddr2ItsKerninstdAddr(kptr_t kernelAddr) const {
   return mappedKernelMemoryRequestsExtraInfoMap.get(kernelAddr).first;
}

void instrumenter::freeMappedKernelMemory(kptr_t addrInKernel,
                                          dptr_t addrInKerninstd,
                                          unsigned /*nbytes*/) {
   pair<dptr_t, unsigned> info = 
      mappedKernelMemoryRequestsExtraInfoMap.get(addrInKernel);

   const unsigned reqid = info.second;
   assert(info.first == addrInKerninstd);
   
   assert(mappedKernelMemoryRequestsMap.get(reqid) == addrInKernel);
   
   connection_to_kerninstd->freeMappedKernelMemory(reqid);
   
   mappedKernelMemoryRequestsMap.undef(reqid);
   mappedKernelMemoryRequestsExtraInfoMap.undef(addrInKernel);
}

void instrumenter::freeMappedKernelMemory(kptr_t addrInKernel,
                                          unsigned nbytes) {
   freeMappedKernelMemory(addrInKernel,
                          mappedKernelMemoryRequestsExtraInfoMap.get(addrInKernel).first,
                          nbytes);
}

// --------------------
unsigned instrumenter::replaceAFunction(kptr_t oldFnStartAddr,
                                        kptr_t newFnStartAddr,
                                        bool replaceCallSitesToo) {
   assert(oldFnStartAddr != 0 && newFnStartAddr != 0);
   const unsigned reqid = unique_replacefunction_reqid++;
   connection_to_kerninstd->replaceAFunction(reqid, oldFnStartAddr, 
					     newFnStartAddr,
                                             replaceCallSitesToo);
   return reqid;
}

void instrumenter::unReplaceAFunction(unsigned reqid) {
   connection_to_kerninstd->unReplaceAFunction(reqid);
}

unsigned instrumenter::replaceAFunctionCall(kptr_t callSiteAddr,
                                            kptr_t newFnStartAddr) {
   assert(callSiteAddr != 0 && newFnStartAddr != 0);
   const unsigned reqid = unique_replacefunction_reqid++;
   connection_to_kerninstd->replaceAFunctionCall(reqid, callSiteAddr, 
                                                 newFnStartAddr);
   return reqid;
}

void instrumenter::unReplaceAFunctionCall(unsigned reqid) {
   connection_to_kerninstd->unReplaceAFunctionCall(reqid);
}

void instrumenter::splice_preinsn(unsigned reqid, kptr_t spliceAddr,
                                  unsigned numint32regsneeded,
                                  unsigned numint64regsneeded,
                                  const relocatableCode_t &code_ifnosaveneeded,
                                  const relocatableCode_t &code_ifsaveneeded) {
   trick_relocatableCode nosave(&code_ifnosaveneeded);
   trick_relocatableCode save(&code_ifsaveneeded);
   connection_to_kerninstd->splice_preinsn(reqid, spliceAddr,
					   numint32regsneeded,
					   numint64regsneeded,
					   nosave, 
					   save);
}

void instrumenter::
splice_prereturn(unsigned reqid, kptr_t spliceAddr,
                 unsigned numint32regsneeded,
                 unsigned numint64regsneeded,
                 const relocatableCode_t &code_prereturn_ifnosaveneeded,
                 const relocatableCode_t &code_prereturn_ifsaveneeded,
                 const relocatableCode_t &code_preinsn_ifnosaveneeded,
                 const relocatableCode_t &code_preinsn_ifsaveneeded) {
   trick_relocatableCode preret_nosave(&code_prereturn_ifnosaveneeded);
   trick_relocatableCode preret_save(&code_prereturn_ifsaveneeded);
   trick_relocatableCode preinsn_nosave(&code_preinsn_ifnosaveneeded);
   trick_relocatableCode preinsn_save(&code_preinsn_ifsaveneeded);
   connection_to_kerninstd->splice_prereturn(reqid, spliceAddr,
					     numint32regsneeded,
					     numint64regsneeded,
					     preret_nosave,
					     preret_save,
					     preinsn_nosave,
					     preinsn_save);
}

void instrumenter::unSplice(unsigned splicereqid) {
   connection_to_kerninstd->unSplice(splicereqid);
}

void instrumenter::requestSampleData(unsigned sampleReqId, 
                                     const pdvector<kptr_t> &addrs) {
   connection_to_kerninstd->requestSampleData(sampleReqId, addrs);
}

void instrumenter::removeSampleRequest(unsigned sampleReqId) {
   connection_to_kerninstd->removeSampleRequest(sampleReqId);
}

void instrumenter::periodicallyDoSampling(unsigned sampleReqId, unsigned ms) {
   connection_to_kerninstd->periodicallyDoSampling(sampleReqId, ms);
}

void instrumenter::stopPeriodicallyDoingSampling(unsigned sampleReqId) {
   connection_to_kerninstd->stopPeriodicallyDoingSampling(sampleReqId);
}

void instrumenter::doSamplingOnceNow(unsigned sampleReqId) {
   connection_to_kerninstd->doSamplingOnceNow(sampleReqId);
}

void instrumenter::temporarilyTurnOffSampling() {
   connection_to_kerninstd->temporarilyTurnOffSampling();
}

void instrumenter::resumeSampling() {
   connection_to_kerninstd->resumeSampling();
}

static abi *makeABI(unsigned abiCode) {
#ifdef i386_unknown_linux2_4
   return new x86_abi();
#elif defined(ppc64_unknown_linux2_4)
   return new power64_abi();
#elif defined(sparc_sun_solaris2_7)
   switch (abiCode) {
      case 0:
         return new sparcv8plus_abi();
      case 1:
         return new sparcv9_abi();
      default:
         assert(false && "unrecognized abiCode");
   }
#endif
}
   
void instrumenter::
presentMachineInfo(unsigned kerninstdABICode,
                   unsigned kernelABICode) {
   assert(kerninstdABI == NULL);
   kerninstdABI = makeABI(kerninstdABICode);

   assert(kernelABI == NULL);
   kernelABI = makeABI(kernelABICode);
}

void instrumenter::downloadToKerninstdAndExecuteOnce(relocatableCode_t &the_code) {
   const unsigned reqid = downloadedToKerninstdCodeId++;
   trick_relocatableCode trick(&the_code);

   // All of the following igen calls are asyncronous, so we can't be sure exactly
   // when they take effect in kerninstd.  However (think CATOCS) since there's just
   // one recipient thread that both receives and processes incoming igen messages,
   // we can be sure that, in particular, the doDownloadedToKerninstdCodeOnceNow()
   // message is received and fully processed before the
   // removeDownloadedToKerninstdCode() once is processed.  Thus, we can be sure
   // that by the time this routine returns, the downloaded code that we want to
   // execute once has indeed taken effect.
   assert(the_code.numChunks() == 1);
   connection_to_kerninstd->downloadToKerninstd(reqid, trick, 0);
   connection_to_kerninstd->doDownloadedToKerninstdCodeOnceNow(reqid);
   connection_to_kerninstd->removeDownloadedToKerninstd(reqid);
}

void instrumenter::downloadToKerninstdAndExecuteOnce(const tempBufferEmitter &em) {
#ifdef sparc_sun_solaris2_7
   sparc_relocatableCode theCode(em);
#elif defined(i386_unknown_linux2_4)
   x86_relocatableCode theCode(em);
#elif defined(ppc64_unknown_linux2_4)
   power_relocatableCode theCode(em);
#endif
   downloadToKerninstdAndExecuteOnce(theCode);
}

void instrumenter::doDownloadedToKerninstdCodeOnceNow(unsigned reqid)
{
    connection_to_kerninstd->doDownloadedToKerninstdCodeOnceNow(reqid);
}

#ifdef sparc_sun_solaris2_7
pcrData instrumenter::getPcr() const {
   pcrData rv = connection_to_kerninstd->getPcrData();

   return rv;
}

void instrumenter::setPcr(const pcrData &newPcrVal) {
   connection_to_kerninstd->setPcr(newPcrVal.s0, newPcrVal.s1,
                                   newPcrVal.ut, newPcrVal.st,
                                   newPcrVal.priv);
}
#elif defined(i386_unknown_linux2_4)
P4PerfCtrState instrumenter::getP4PerfCtrState() {
   P4PerfCtrState rv = connection_to_kerninstd->getP4PerfCtrState();
   return rv;
}

void instrumenter::setP4PerfCtrState(const P4PerfCtrState &newState) {
   connection_to_kerninstd->setP4PerfCtrState(newState);
}
#elif defined(ppc64_unknown_linux2_4)
pdvector<uint64_t> instrumenter::getPowerPerfCtrState() {
   return connection_to_kerninstd->getPowerPerfCtrState();
}

void instrumenter::setPowerPerfCtrState(const pdvector <uint64_t> &newState) {
   connection_to_kerninstd->setPowerPerfCtrState(newState);
}
#endif
