// kerninstdClient.C
// implementation of server side of igen-generated class kerninstdClient

#include "kerninstdClient.xdr.SRVR.h"
#include "moduleMgr.h"
#include "kernelDriver.h"
#include "instrumentationMgr.h"
#include <inttypes.h>
#include "fillin.h"
#include "interProcRegAnalysis.h"
#include "regAnalysisOracles.h"
#include <string.h> // memset()
#include "util/h/hashFns.h"
#include "util/h/mrtime.h"
#include "kmem.h"
#include "codeObjectCreationOracle.h"
#include "machineInfo.h"

#ifdef sparc_sun_solaris2_7
#include "cpcInterface.h"
#include "pcrRegister.h"
#include "picRegister.h"
#include "sparcv8plus_abi.h"
#include "sparcv9_abi.h"
#include <sys/processor.h> // processor_info()
#include <sys/systeminfo.h> // sysinfo()
#include <sys/sysmacros.h> // roundup()
extern cpcInterface *global_cpcInterface;
#endif

extern kernelDriver *global_kernelDriver;
extern instrumentationMgr *global_instrumentationMgr;
extern machineInfo *global_machineInfo;
extern dictionary_hash<kerninstdClient*, client_connection*> igenHandle2ClientConn;
extern moduleMgr *global_moduleMgr;

uint64_t kerninstdClient::getCurrentKerninstdTime() {
    assert(global_machineInfo != 0);
    if (global_machineInfo->hasStick()) {
	return global_kernelDriver->getStickRaw();
    }
    else {
	return global_kernelDriver->getTickRaw();
    }
}

void kerninstdClient::request_for_modules() {
   // a client is (asynchronously) asking for module information
   //cout << "Welcome to request_for_modules()" << endl;
   cout << "Packaging up & sending module information...\n";

   mrtime_t time1 = getmrtime();
   
   extern moduleMgr *global_moduleMgr;
   assert(global_moduleMgr);
   
   const moduleMgr &theModuleMgr = *global_moduleMgr;

   begin_module_information(); // an igen message

   for (moduleMgr::const_iterator moditer = theModuleMgr.begin();
        moditer != theModuleMgr.end(); ++moditer) {
      const loadedModule *mod = *moditer;

      dout << mod->getName() << "..." << flush;

      module_information(mod);
   }
   dout << endl;

   cout << "Kerninstd ready\n";
   
   end_module_information(); // an igen message
   
   mrtime_t time2 = getmrtime();
   mrtime_t time_diff = time2 - time1;
   dout << "Took " << time_diff/1000000 << " msecs to package & send" << endl;
}

void kerninstdClient::
reqParseFnIntoCodeObjects(unsigned reqid,
                          kptr_t fnEntryAddr,
                          const pdvector<fnAddrBeingRelocated> &fnAddrsBeingRelocated
                          ) {
   // fnAddrsBeingRelocated.  Vector of pair: .first is the (old) address of a fn;
   // .second is the new name of this function, format: "modName/fnName"
   const moduleMgr &theModuleMgr = *global_moduleMgr;

   pair<pdstring, const function_t*> modAndFn = theModuleMgr.findModAndFnOrNULL(fnEntryAddr, true);

   if (modAndFn.second == NULL) {
      cout << "Cannot parse code objects since can't find "
           << addr2hex(fnEntryAddr) << " as the start of a function" << endl;
      return;
   }
   function_t &fn = const_cast<function_t &>(*modAndFn.second);

   fnCodeObjects theFnCodeObjects; // no args to ctor needed
   const bool verbose = false; // correct?
   theFnCodeObjects.parse(theModuleMgr, &fn, fnAddrsBeingRelocated, verbose);

   dout << "kerninstd done parsing into codeObjects; sending to kperfmon now"
        << endl;
   
   parseFnIntoCodeObjectsResponse(reqid, theFnCodeObjects);
}

fnCodeObjects kerninstdClient::
syncParseFnIntoCodeObjects(kptr_t fnEntryAddr,
                           const pdvector<fnAddrBeingRelocated> &fnAddrsBeingRelocated) {
   // If parsing fails, then we need to return a sentinel, which will be designated
   // as an empty fnCodeObjects.
   const moduleMgr &theModuleMgr = *global_moduleMgr;
   
   pair<pdstring, const function_t*> modAndFn =
      theModuleMgr.findModAndFnOrNULL(fnEntryAddr, true);
   assert(modAndFn.second);

   function_t &fn = const_cast<function_t &>(*modAndFn.second);
   
   fnCodeObjects result; // no args to ctor needed
   const bool verbose = false;

   result.parse(theModuleMgr, &fn, fnAddrsBeingRelocated, verbose);

   return result;
}

void kerninstdClient::request_for_peek(unsigned reqid,
                                       const pdvector<chunkRange> &ranges) {
   kernelDriver &theKernelDriver = *global_kernelDriver;

   pdvector<chunkData> responseData;
   
   pdvector<chunkRange>::const_iterator iter = ranges.begin();
   pdvector<chunkRange>::const_iterator finish = ranges.end();
   for (; iter != finish; ++iter) {
      kptr_t chunkStartAddr = iter->first;
      unsigned chunkNumBytes = roundup(iter->second, 4);

      pdvector<uint32_t> buffer(chunkNumBytes / 4);
   
      unsigned numBytesActuallyRead = 
	  theKernelDriver.peek_block(buffer.begin(), chunkStartAddr,
				     chunkNumBytes, true); // reread on fault
      // note: above returns 0 on error

      if (numBytesActuallyRead != chunkNumBytes)
         buffer.shrink(numBytesActuallyRead / 4);

      responseData += make_pair(chunkStartAddr, buffer);
   }
   
   peek_response(reqid, responseData); // igen upcall
}

pdvector<uint32_t> kerninstdClient::peek_kernel_contig(kptr_t startAddr,
                                                     unsigned nbytes) {
   kernelDriver &theKernelDriver = *global_kernelDriver;

   nbytes = roundup(nbytes, 4);
   assert(nbytes % 4 == 0);

   pdvector<uint32_t> buffer(nbytes / 4);
   
   unsigned numBytesActuallyRead =
       theKernelDriver.peek_block(&buffer[0], startAddr, nbytes,
				  true); // reread on fault
   if (numBytesActuallyRead != nbytes)
      buffer.shrink(numBytesActuallyRead / 4);
   
   return buffer;
}

int kerninstdClient::poke_kernel_contig(const pdvector<uint32_t> &data,
					kptr_t startAddr, unsigned nbytes) 
{
   kernelDriver &theKernelDriver = *global_kernelDriver;
   theKernelDriver.poke_block(startAddr, nbytes, &data[0]);

   return 0;
}


pdvector<uint32_t> kerninstdClient::peek_kerninstd_contig(dptr_t startAddr,
                                                        unsigned nbytes) {
   nbytes = roundup(nbytes, 4); // <sys/sysmacros.h>
   assert(nbytes % 4 == 0);

   // We read from /proc/self instead of using memcpy, to prevent crashes when
   // reading bad addresses.
#ifdef sparc_sun_solaris2_7
   int fd = open("/proc/self/as", O_RDONLY);
#else // Linux - both x86 & power
   int fd = open("/proc/self/mem", O_RDONLY);
#endif
   assert(fd != -1);

   pdvector<unsigned> buffer(nbytes / 4);
   ssize_t numBytesActuallyRead = 0;
#ifndef i386_unknown_linux2_4
   numBytesActuallyRead = pread(fd, &buffer[0], nbytes, startAddr);
#else
   if( lseek(fd, (off_t)startAddr, SEEK_SET) != (off_t)-1 ) {
      numBytesActuallyRead = read(fd, (void*)&buffer[0], nbytes);
   }
   else
     perror("kerninstdClient::peek_kerninstd_contig - unsuccessful lseek");
     
#endif

   // error check:
   if (numBytesActuallyRead == -1)
      numBytesActuallyRead = 0;
   
   (void)close(fd);

   assert(numBytesActuallyRead >= 0);
   if ((unsigned)numBytesActuallyRead < nbytes)
      buffer.shrink(numBytesActuallyRead / 4);

   return buffer;
}

static dataflowFn1Insn
mbdf_convert(kptr_t insnAddr, bool beforeInsn,
             monotone_bitwise_dataflow_fn *fn) {

   oneRegAnalysisReqInfo where;
   where.address = insnAddr;
   where.before = beforeInsn;

   trick_mbdf tf(fn);
   dataflowFn1Insn result(where, tf);
   
   return result;
}

trick_regset
kerninstdClient::getFreeRegsAtPoint(kptr_t addr, bool preReturn) {
   // NOTE: The following routine is very slow!

   const moduleMgr &theModuleMgr = *global_moduleMgr;
   const function_t &fn = theModuleMgr.findFn(addr, false);
   return trick_regset(global_moduleMgr->getDeadRegsAtPoint(fn, addr, preReturn));
}

void kerninstdClient::
requestRegAnalysis(unsigned reqid,
                   const pdvector<oneRegAnalysisReqInfo> &addresses) {
   pdvector<dataflowFn1Insn> result;
   
   for (pdvector<oneRegAnalysisReqInfo>::const_iterator iter = addresses.begin(); iter != addresses.end(); ++iter) {
      const kptr_t addr = iter->address;
      const bool beforeInsn = iter->before;

      const moduleMgr &theModuleMgr = *global_moduleMgr;

      const function_t &fn = theModuleMgr.findFn(addr, false);
      
      monotone_bitwise_dataflow_fn *one_result = beforeInsn ?
         global_moduleMgr->getLiveRegFnBeforeInsnAddr(fn, addr, false) :
         global_moduleMgr->getLiveRegFnAfterInsnAddr(fn, addr, false);
         // false --> not verbose (?)

      result += mbdf_convert(addr, beforeInsn, one_result);
   }

   regAnalysisResponse(reqid, result);
}

// Same as above, but used in a synchronous igen call
pdvector<dataflowFn1Insn> kerninstdClient::syncRegisterAnalysis(
    const pdvector<oneRegAnalysisReqInfo> &addresses) 
{
    pdvector<dataflowFn1Insn> result;
    pdvector<oneRegAnalysisReqInfo>::const_iterator iter = addresses.begin();
    for (; iter != addresses.end(); ++iter) {
	const kptr_t addr = iter->address;
	const bool beforeInsn = iter->before;

	const moduleMgr &theModuleMgr = *global_moduleMgr;

	const function_t &fn = theModuleMgr.findFn(addr, false);
      
	monotone_bitwise_dataflow_fn *one_result = beforeInsn ?
	    global_moduleMgr->getLiveRegFnBeforeInsnAddr(fn, addr, false) :
	    global_moduleMgr->getLiveRegFnAfterInsnAddr(fn, addr, false);
	// false --> not verbose (?)

	result += mbdf_convert(addr, beforeInsn, one_result);
    }

    return result;
}

// --------------------------------------------------------------------------
// ---------------------- Allocated, Unmapped Kernel Memory -----------------
// --------------------------------------------------------------------------

void kerninstdClient::allocateUnmappedKernelMemory(unsigned reqid,
                                                   unsigned nbytes,
                                                   bool tryForNucleusText) {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);
   
   instrumentationMgr &theInstMgr = *global_instrumentationMgr;
   theInstMgr.allocateUnmappedKernelMemory(*theClient, reqid,
                                                      nbytes,
                                                      tryForNucleusText);
}

kptr_t kerninstdClient::queryAllocatedUnmappedKernelMemoryAddr(unsigned reqid) {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);
   
   instrumentationMgr &theInstMgr = *global_instrumentationMgr;
   return theInstMgr.queryAllocatedUnmappedKernelMemoryAddr(*theClient,
                                                                       reqid);
}

void kerninstdClient::freeAllocatedUnmappedKernelMemory(unsigned reqid) {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);
   
   instrumentationMgr &theInstMgr = *global_instrumentationMgr;
   theInstMgr.freeAllocatedUnmappedKernelMemory(*theClient, reqid);
}


// --------------------------------------------------------------------------
// ---------------------- Allocated Kerninstd-only Memory -------------------
// --------------------------------------------------------------------------

void kerninstdClient::allocateKerninstdMemory(unsigned reqid, unsigned nbytes) {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);
   
   instrumentationMgr &theInstMgr = *global_instrumentationMgr;
   theInstMgr.allocateKerninstdMemory(*theClient, reqid, nbytes);
}

dptr_t kerninstdClient::queryAllocatedKerninstdMemoryAddr(unsigned reqid) {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);
   
   instrumentationMgr &theInstMgr = *global_instrumentationMgr;
   void *result = theInstMgr.queryAllocatedKerninstdMemoryAddr(*theClient, reqid);
   return (dptr_t)result;
}

void kerninstdClient::freeAllocatedKerninstdMemory(unsigned reqid) {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);
   
   instrumentationMgr &theInstMgr = *global_instrumentationMgr;
   theInstMgr.freeKerninstdMemory(*theClient, reqid);
}


// --------------------------------------------------------------------------
// ---------------------- Allocated, Mapped Kernel Memory -----------------
// --------------------------------------------------------------------------

void kerninstdClient::allocateMappedKernelMemory(unsigned reqid,
                                                 unsigned nbytes,
                                                 bool tryForNucleusText) {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);
   
   instrumentationMgr &theInstMgr = *global_instrumentationMgr;
   theInstMgr.allocateMappedKernelMemory(*theClient, reqid,
                                                    nbytes,
                                                    tryForNucleusText);
}

kptr_t
kerninstdClient::queryAllocatedMappedKernelMemoryAddrInKernel(unsigned reqid) {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);
   
   instrumentationMgr &theInstMgr = *global_instrumentationMgr;
   return theInstMgr.
      queryAllocatedMappedKernelMemoryAddrInKernel(*theClient, reqid);
}

dptr_t
kerninstdClient::queryAllocatedMappedKernelMemoryAddrInKerninstd(unsigned reqid) {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);
   
   instrumentationMgr &theInstMgr = *global_instrumentationMgr;
   void *result = theInstMgr.
      queryAllocatedMappedKernelMemoryAddrInKerninstd(*theClient, reqid);

   return (dptr_t)result;
}

void kerninstdClient::freeMappedKernelMemory(unsigned reqid) {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);
   
   instrumentationMgr &theInstMgr = *global_instrumentationMgr;
   theInstMgr.freeMappedKernelMemory(*theClient, reqid);
}


void kerninstdClient::requestReadOnlyMMapOfKernelSymIntoKerninstd(unsigned reqId,
                                                                  const pdstring &symName,
                                                                  unsigned nbytes) {
   client_connection *theClient = igenHandle2ClientConn.get(this);
   global_instrumentationMgr->requestReadOnlyMMapOfKernelSymIntoKerninstd(*theClient,
                                                                          reqId,
                                                                          symName,
                                                                          nbytes);
}

void kerninstdClient::unmapKernelSymInKerninstd(unsigned reqId) {
   client_connection *theClient = igenHandle2ClientConn.get(this);
   global_instrumentationMgr->unmapKernelSymInKerninstd(*theClient, reqId);
}

// --------------------

void kerninstdClient::
replaceAFunction(unsigned reqId,
                 kptr_t oldFnStartAddr,
                 kptr_t newFnStartAddr,
                 bool replaceCallSitesToo) {
   client_connection *theClient = igenHandle2ClientConn.get(this);
   global_instrumentationMgr->replaceAFunction(*theClient, reqId,
                                               oldFnStartAddr, newFnStartAddr,
                                               replaceCallSitesToo);
}

void kerninstdClient::unReplaceAFunction(unsigned reqId) {
   client_connection *theClient = igenHandle2ClientConn.get(this);
   global_instrumentationMgr->unReplaceAFunction(*theClient, reqId);
}

void kerninstdClient::
replaceAFunctionCall(unsigned reqId,
                     kptr_t callSiteAddr,
                     kptr_t newFnStartAddr) {
   client_connection *theClient = igenHandle2ClientConn.get(this);
   global_instrumentationMgr->replaceAFunctionCall(*theClient, reqId,
                                                   callSiteAddr, 
                                                   newFnStartAddr);
}

void kerninstdClient::unReplaceAFunctionCall(unsigned reqId) {
   client_connection *theClient = igenHandle2ClientConn.get(this);
   global_instrumentationMgr->unReplaceAFunctionCall(*theClient, reqId);
}

// --------------------

void kerninstdClient::
splice_preinsn(unsigned reqid, kptr_t spliceAddr,
               unsigned conservative_numInt32RegsNeeded,
               unsigned conservative_numInt64RegsNeeded,
               // The code, if no save/restore is needed:
               trick_relocatableCode relocatable_code_ifnosave,
               trick_relocatableCode relocatable_code_ifsave) {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);

   instrumentationMgr &theInstMgr = *global_instrumentationMgr;
   theInstMgr.splice_preinsn(*theClient, reqid, spliceAddr,
                                        conservative_numInt32RegsNeeded,
                                        conservative_numInt64RegsNeeded,
                                        relocatable_code_ifnosave.get(),
                                        relocatable_code_ifsave.get());
}

void kerninstdClient::
splice_prereturn(unsigned reqid, kptr_t spliceAddr,
                 unsigned conservative_numInt32RegsNeeded,
                 unsigned conservative_numInt64RegsNeeded,
                 // The code, if no save/restore is needed:
                 trick_relocatableCode relocatable_code_prereturn_ifnosave,
                 trick_relocatableCode relocatable_code_prereturn_ifsave,
                 trick_relocatableCode relocatable_code_preinsn_ifnosave,
                 trick_relocatableCode relocatable_code_preinsn_ifsave)
{
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);

   instrumentationMgr &theInstMgr = *global_instrumentationMgr;
   theInstMgr.splice_prereturn(*theClient, reqid, spliceAddr,
                                          conservative_numInt32RegsNeeded,
                                          conservative_numInt64RegsNeeded,
                                          relocatable_code_prereturn_ifnosave.get(),
                                          relocatable_code_prereturn_ifsave.get(),
                                          relocatable_code_preinsn_ifnosave.get(),
                                          relocatable_code_preinsn_ifsave.get());
}

void kerninstdClient::unSplice(unsigned request_id) {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);

   instrumentationMgr &theInstMgr = *global_instrumentationMgr;
   theInstMgr.unSplice(*theClient, request_id);
      // true --> re-instrument now
}

// ----------------------- Downloading to Kerninstd -----------------------

void kerninstdClient::
downloadToKerninstd(unsigned reqid, // chosen by app
                    trick_relocatableCode icode,
                    unsigned entry_chunk_ndx) {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);

   instrumentationMgr &theInstMgr = *global_instrumentationMgr;

   theInstMgr.downloadToKerninstd(*theClient, reqid,
                                             icode.get(), entry_chunk_ndx);
}

void kerninstdClient::removeDownloadedToKerninstd(unsigned reqid) {
   // doesn't try to stop sampling or anything like that
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);

   instrumentationMgr &theInstMgr = *global_instrumentationMgr;

   theInstMgr.removeDownloadedToKerninstd(*theClient, reqid);
}

pair<pdvector<dptr_t>, unsigned>
kerninstdClient::queryDownloadedToKerninstdAddresses(unsigned reqid) {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);

   instrumentationMgr &theInstMgr = *global_instrumentationMgr;

   return theInstMgr.queryDownloadedToKerninstdAddresses(*theClient, reqid);
}

void kerninstdClient::doDownloadedToKerninstdCodeOnceNow(unsigned uniqDownloadedCodeId) {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);

   instrumentationMgr &theInstMgr = *global_instrumentationMgr;

   theInstMgr.doDownloadedToKerninstdCodeOnceNow(*theClient, uniqDownloadedCodeId);
}

void kerninstdClient::
periodicallyDoDownloadedToKerninstdCode(unsigned uniqDownloadedCodeId,
                                        unsigned period_millisecs) {
   // can be used to set or change an interval
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);

   instrumentationMgr &theInstMgr = *global_instrumentationMgr;

   theInstMgr.periodicallyDoDownloadedToKerninstdCode(*theClient,
                                                                 uniqDownloadedCodeId,
                                                                 period_millisecs);
}

void kerninstdClient::
stopPeriodicallyDoingDownloadedToKerninstdCode(unsigned uniqDownloadedCodeId) {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);

   instrumentationMgr &theInstMgr = *global_instrumentationMgr;

   theInstMgr.stopPeriodicallyDoingDownloadedToKerninstdCode(*theClient,
                                                                        uniqDownloadedCodeId);
}

// --------------------- Downloading into Kernel --------------------

void kerninstdClient::
downloadToKernel(unsigned reqid,
                 trick_relocatableCode iRelocatableCode,
                 unsigned entry_chunk_ndx,
                 bool tryForNucleus) {
   // theRelocatableCode may contain unresolved references (calls, imm32s).
   // We'll resolve them, and then install the code.

   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);

   instrumentationMgr &theInstMgr = *global_instrumentationMgr;

   theInstMgr.downloadToKernel(*theClient, reqid, 
					  iRelocatableCode.get(),
                                          entry_chunk_ndx, tryForNucleus);
}

void kerninstdClient::
download1FnToKernelAndParse(unsigned reqid,
                            const pdstring &modName,
                            const pdstring &modDescriptionIfNew,
                            const pdstring &fnName,
                            trick_relocatableCode iRelocatableCode,
                            unsigned entry_chunk_ndx,
                            unsigned preFnDataChunkIfAny,
                            bool tryForNucleus) {
   // theRelocatableCode may contain unresolved references (calls, imm32s).
   // We'll resolve them, and then install the code.

   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);

   instrumentationMgr &theInstMgr = *global_instrumentationMgr;

   theInstMgr.downloadToKernelAndParse(*theClient, reqid,
                                                  modName, modDescriptionIfNew,
                                                  fnName,
                                                  iRelocatableCode.get(), 
						  entry_chunk_ndx,
                                                  preFnDataChunkIfAny,
                                                  tryForNucleus);
}

void kerninstdClient::
downloadSeveralFnsToKernelAndParse(unsigned reqid,
                                   const downloadToKernelFns &fns,
                                   const emitOrdering &theEmitOrdering,
                                   bool tryForNucleus) {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);

   instrumentationMgr &theInstMgr = *global_instrumentationMgr;

   theInstMgr.downloadToKernelAndParse(*theClient, reqid, fns,
                                                  theEmitOrdering,
                                                  tryForNucleus);
}

void kerninstdClient::removeDownloadedToKernel(unsigned reqid) {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);

   instrumentationMgr &theInstMgr = *global_instrumentationMgr;

   theInstMgr.removeDownloadedToKernel(*theClient, reqid);
}

pdvector<kdownloadedLocations1Fn> // see the .I file for this definition
kerninstdClient::queryDownloadedToKernelAddresses(unsigned reqid) {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);

   instrumentationMgr &theInstMgr = *global_instrumentationMgr;
   return theInstMgr.queryDownloadedToKernelAddresses(*theClient, reqid);
}

kdownloadedLocations1Fn
kerninstdClient::queryDownloadedToKernelAddresses1Fn(unsigned reqid) {
   pdvector<kdownloadedLocations1Fn> result =
      queryDownloadedToKernelAddresses(reqid);
   assert(result.size() == 1);
   return result[0];
}

void kerninstdClient::parseKernelRangesAsFns(const parseNewFunctions &fnsInfo) {
   // "parseNewFunctions" is typedef'd in .I file to be a vector of "parseNewFunction",
   // which in turn is a struct containing modname, mod description, fn name,
   // chunkAddrs, and entry-chunk-ndx

   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);

   instrumentationMgr &theInstMgr = *global_instrumentationMgr;

   theInstMgr.parseKernelRangesAsFns(*theClient, fnsInfo);
}

// --------------------------------------------------------------------------------
void kerninstdClient::requestSampleData(unsigned sampleReqId, 
                                        const pdvector<kptr_t> &addrs) {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);

   theClient->requestSampleData(sampleReqId, addrs);
}

void kerninstdClient::removeSampleRequest(unsigned sampleReqId) {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);

   theClient->removeSampleRequest(sampleReqId);
}

void kerninstdClient::periodicallyDoSampling(unsigned sampleReqId, 
                                             unsigned ms) {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);

   instrumentationMgr &theInstMgr = *global_instrumentationMgr;
   theInstMgr.periodicallyDoSampling(*theClient, sampleReqId, ms);
}

void kerninstdClient::stopPeriodicallyDoingSampling(unsigned sampleReqId) {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);

   instrumentationMgr &theInstMgr = *global_instrumentationMgr;
   theInstMgr.stopPeriodicallyDoingSampling(*theClient, sampleReqId);
}

void kerninstdClient::doSamplingOnceNow(unsigned sampleReqId) {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);

   instrumentationMgr &theInstMgr = *global_instrumentationMgr;
   theInstMgr.doSamplingOnceNow(*theClient, sampleReqId);
}

void kerninstdClient::temporarilyTurnOffSampling() {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);

   instrumentationMgr &theInstMgr = *global_instrumentationMgr;
   theInstMgr.temporarilyTurnOffSampling(*theClient);
}

void kerninstdClient::resumeSampling() {
   client_connection *theClient;
   if (!igenHandle2ClientConn.find(this, theClient))
      assert(false);

   instrumentationMgr &theInstMgr = *global_instrumentationMgr;
   theInstMgr.resumeSampling(*theClient);
}

presentedMachineInfo
kerninstdClient::getKerninstdMachineInfo() {
   presentedMachineInfo result;
   assert(global_machineInfo != 0);
   global_machineInfo->convertTo(&result);

   return result;
}

extern bool justTesting; // launchSite.C
void kerninstdClient::setJustTestingFlag(bool flag) {
   justTesting = flag;
   cout << "Just testing flag set to " << flag << endl;
}

pcrData kerninstdClient::getPcrData() 
{
#ifdef sparc_sun_solaris2_7
    const uint64_t pcr_raw = global_kernelDriver->getPcrRawCurrentCPU();
    pcr_union f(pcr_raw);
    pcrData rv;
    rv.s0 = f.f.s0;
    rv.s1 = f.f.s1;
    rv.ut = (f.f.ut == 1);
    rv.st = (f.f.st == 1);
    rv.priv = (f.f.priv == 1);
#else
    assert(!"kerninstdClient::getPcrData() unsupported on this platform\n");
    pcrData rv;
#endif
    return rv;
}

void kerninstdClient::setPcr(unsigned pic0, unsigned pic1,
                             bool user, bool sys,
                             bool priv) {
#ifdef sparc_sun_solaris2_7
   const uint64_t orig_pcr_raw = global_kernelDriver->getPcrRawCurrentCPU();
   
   pcr_union f(orig_pcr_raw);

//   cout << "kerninstdClient::setPcr: orig=" << (void*)orig_pcr_raw << endl;
//   cout << f.f << endl;

   f.f.s0 = pic0;
   f.f.s1 = pic1;
   f.f.ut = user;
   f.f.st = sys;
   f.f.priv = priv;
   
   global_cpcInterface->changePcrAllCPUs(f);
   
   const uint64_t new_pcr_raw = global_kernelDriver->getPcrRawCurrentCPU();
   f.rawdata = new_pcr_raw;

//   cout << "kerninstdClient::setPcr: new=" << (void*)new_pcr_raw << endl;
//   cout << f.f << endl;
#else
   assert(!"kerninstdClient::setPcr() not supported on this platform\n");
#endif
}

void kerninstdClient::print_pic() {
#ifdef sparc_sun_solaris2_7
   uint64_t pic_raw = global_kernelDriver->getPicRaw();
   cout << "PIC0=" << (void*)((pic_raw << 32) >> 32) << endl;
   cout << "PIC1=" << (void*)(pic_raw >> 32) << endl;
#else
   assert(!"kerninstdClient::print_pic() not supported on this platform\n");
#endif
}

void kerninstdClient::clear_pic() {
#ifdef sparc_sun_solaris2_7
   pic_union f;
   assert(sizeof(f) == 8);
   f.f.pic0 = 0;
   f.f.pic1 = 1;
   global_kernelDriver->setPicRaw(f.raw);

   cout << "PIC0/1 zerod out as requested" << endl;
#else
   assert(!"kerninstdClient::print_pic() not supported on this platform\n");
#endif
}

P4PerfCtrState kerninstdClient::getP4PerfCtrState() {
#ifdef i386_unknown_linux2_4
   return global_kernelDriver->getP4PerfCtrState();
#else
   assert(!"kerninstdClient::getP4PerfCtrState() not supported on this platform\n");
#endif
}

void kerninstdClient::setP4PerfCtrState(const P4PerfCtrState &newState) {
#ifdef i386_unknown_linux2_4
   global_kernelDriver->setP4PerfCtrState(newState);
#else
   assert(!"kerninstdClient::setP4PerfCtrState() not supported on this platform\n");
#endif
}

pdvector<uint64_t> kerninstdClient::getPowerPerfCtrState() {
#ifdef ppc64_unknown_linux2_4
   pdvector<uint64_t> curr_state(3);
   global_kernelDriver->getPowerPerfCtrState(curr_state);
   return curr_state;
#else
   assert(!"kerninstdClient::getPowerPerfCtrState() not supported on this architecture\n");
#endif
}
void kerninstdClient::setPowerPerfCtrState(const pdvector<uint64_t> &newState) {
#ifdef ppc64_unknown_linux2_4
   global_kernelDriver->setPowerPerfCtrState(newState);
#else
   assert(!"kerninstdClient::setPowerPerfCtrState() not supported on this architecture\n");
#endif
}

#ifdef i386_unknown_linux2_4
extern unsigned ud2_bug_size; // defined in x86_instr.C
#endif

unsigned kerninstdClient::getUd2BugSize()
{
#ifdef i386_unknown_linux2_4
   return ud2_bug_size;
#else
   assert(!"kerninstdClient::getUd2BugSize() not supported on this platform\n");
   return 0;
#endif
}

kptr_t kerninstdClient::getAllVTimersAddr()
{
   return global_kernelDriver->getAllVTimersAddr();
}

void kerninstdClient::addTimerToAllVTimers(kptr_t timer_addr)
{
   global_kernelDriver->addTimerToAllVTimers(timer_addr);
}

void kerninstdClient::removeTimerFromAllVTimers(kptr_t timer_addr)
{
   global_kernelDriver->removeTimerFromAllVTimers(timer_addr);
}

void kerninstdClient::call_once() {
   global_kernelDriver->callOnce();
}

void kerninstdClient::toggle_debug_bits(uint32_t bitmask)
{
#if (defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4))
   global_kernelDriver->toggleDebugBits(bitmask);
#else
   assert(!"kerninstdClient::toggle_debug_bits() not supported on this platform\n");
#endif
}

void kerninstdClient::toggle_enable_bits(uint32_t bitmask)
{
#if (defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4))
   global_kernelDriver->toggleEnableBits(bitmask);
#else
   assert(!"kerninstdClient::toggle_enable_bits() not supported on this platform\n");
#endif
}

pdvector<kptr_t> kerninstdClient::getCalleesForSite(kptr_t siteAddr)
{
   return global_kernelDriver->getCalleesForSite(siteAddr);
}

// Read the value of a hardware counter across all cpus
pdvector<sample_type> kerninstdClient::readHwcAllCPUs(unsigned type)
{
   return global_kernelDriver->readHwcAllCPUs(type);
}
