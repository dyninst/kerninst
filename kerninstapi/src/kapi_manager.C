// kapi_manager.C
// A client of kerninstd

#include <unistd.h>
#include <stdio.h> // perror()
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <fcntl.h> // open()
#include <iostream.h>
#include <limits.h>

#include "util/h/minmax.h"
#include "util/h/rope-utils.h" // num2string()
#include "util/h/mrtime.h"
#include "util/h/hashFns.h"
#include "util/h/out_streams.h"

#ifdef sparc_sun_solaris2_7
#include "traceBuffer.h"
#include "outliningMisc.h"
#include "outliningTest.h"
#include "groupMgr.h"
#include "regSetManager.h"
#include "vtimerMgr.h"
#elif ppc64_unknown_linux2_4
#include "power64_abi.h"
#include <stack> //used in convertSetToVector
#endif

#include "moduleMgr.h"
#include "instrumenter.h"
#include "kerninstdClient.xdr.CLNT.h"
#include "common/h/Ident.h"
#include "machineInfo.h"

#include "relocatableCode.h"
#include "tempBufferEmitter.h"

#include "kapi.h"
#include "ast.h"
#include "kludges.h"
#include "memAllocator.h"

using namespace kcodegen;

moduleMgr *global_moduleMgr;

extern const char V_libkerninstapi[];
extern const Ident kerninstapi_ident(V_libkerninstapi, "KernInst");
   // extern needed lest the variable have a static definition

instrumenter *theGlobalInstrumenter = 0;
machineInfo *theGlobalKerninstdMachineInfo = 0;

#ifdef sparc_sun_solaris2_7
vtimerMgr *theGlobalVTimerMgr = 0;
groupMgr *theGlobalGroupMgr = 0;
traceBuffer *cswitchTraceBuffer = NULL;
#elif defined(i386_unknown_linux2_4)
extern unsigned emit_64bit_code;
#endif

kerninstdClientUser *connection_to_kerninstd = 0;

bool daemonIsSameEndian(const machineInfo &/*mi*/)
{
#ifdef sparc_sun_solaris2_7
   struct utsname uts;
   if (uname(&uts) < 0) {
      perror("uname() error");
      assert(false);
   }
   // Assume the daemon is running on Solaris. Would need to
   // change once we port to x86
   return (!strcmp(uts.sysname, "SunOS"));
#else
   return true;
#endif
}

class kerninstdClientUserGentleError : public kerninstdClientUser {
 public:
   kerninstdClientUserGentleError(int family, int port, int type, const pdstring host,
                                  int (*r)(void*,caddr_t,int),
                                  int (*w)(void*,caddr_t,int),
                                  int nblock) :
      kerninstdClientUser(family, port, type, host, r, w, nblock) {}
   void handle_error() {}
};

kapi_manager::kapi_manager() : err_code(no_error), memAlloc(0),
			       callbackMap(0), samplingIntervals(0), 
			       switchin_reqid(0), switchout_reqid(0),
			       dynload_cb(0), calleeWatchers(0)
{
}

void setCallbackDispatcher(kapi_manager *iCallbackDispatcher);

#ifdef i386_unknown_linux2_4
extern unsigned ud2_bug_size; // defined in x86_instr.C
#endif

ierr kapi_manager::attach(const char *kerninstd_machine_name, 
			  unsigned kerninstd_port_num)
{
   bool debug_output = true;

   init_out_streams(debug_output);

   setCallbackDispatcher(this); // to let Kerninst invoke us on callbacks
   cout << "Using KerninstAPI, " << kerninstapi_ident.release() << endl;
   
   cout << "kerninstapi: attempting connection to kerninstd on " << endl;
   cout << "machine \"" << kerninstd_machine_name << "\", portnum "
        << kerninstd_port_num << endl;

   connection_to_kerninstd = new kerninstdClientUserGentleError(PF_INET,
                                                                kerninstd_port_num,
                                                                SOCK_STREAM,
                                                                kerninstd_machine_name,
                                                                NULL, NULL, 0);
      // GentleError() -- overrides the stupid default virtual method
      // handle_error() (which prints a horribly ugly warning)

   assert(connection_to_kerninstd);
   if (connection_to_kerninstd->errorConditionFound) {
      cerr << "Could not make igen connection to kerninstd" << endl;
      cerr << "Make sure kerninstd is running and that you have used the correct"
           << " port number" << endl;
      return (-5);
   }

#ifdef i386_unknown_linux2_4
   // Initialize size of a ud2 in BUG() macro if not already done
   if(!ud2_bug_size) {
      unsigned ud2_size = connection_to_kerninstd->getUd2BugSize();
      ud2_bug_size = ud2_size;
      assert(ud2_bug_size && "x86_instr::ud2_bug_size still 0 after xdr init");
   }
#endif

   // theGlobalInstrumenter is used by traceBuffer, so we must initialize it now,
   // before cswitchTraceBuffer is constructed.
   theGlobalInstrumenter = new instrumenter(connection_to_kerninstd);
   assert(theGlobalInstrumenter);

   presentedMachineInfo pmi = connection_to_kerninstd->getKerninstdMachineInfo();
   // create machineInfo from the data shipped by kerninstd
   theGlobalKerninstdMachineInfo = new machineInfo(pmi);

   theGlobalInstrumenter->presentMachineInfo(pmi.kerninstdABICode,
                                             pmi.kernelABICode);

   memAlloc = new memAllocator(theGlobalInstrumenter);

   callbackMap = 
       new dictionary_hash<unsigned, data_callback_t>(unsignedIdentityHash);
   samplingIntervals = 
       new dictionary_hash<unsigned, unsigned>(unsignedIdentityHash);
   last_timeOfSample = (uint64_t)(-1);

   calleeWatchers = new dictionary_hash<kptr_t, unsigned>(addrHash4);

#ifdef sparc_sun_solaris2_7
   if (false) { // don't set back to true until bit-rot has been removed!!!
      const unsigned num_entries = 1024; // must be a power of two
      cswitchTraceBuffer = new traceBuffer(num_entries, false);
         // false --> not kerninstd-only
   }
#endif
      
   global_moduleMgr = new moduleMgr;

#ifdef sparc_sun_solaris2_7
   theGlobalVTimerMgr = new vtimerMgr(cswitchTraceBuffer,
                                      1, 2, // switchout start/finish
                                      3, 4); // switchin start/finish
   assert(theGlobalVTimerMgr);

   theGlobalGroupMgr = new groupMgr(connection_to_kerninstd);
   assert(theGlobalGroupMgr);
#endif

   // send a request_for_modules() igen call to kerninstd.
   connection_to_kerninstd->request_for_modules();

   extern bool receivedModuleInformation;
   while (!receivedModuleInformation) {
       ierr rv;
       if ((rv = waitForEvents(UINT_MAX)) < 0 ||
	   (rv = handleEvents()) < 0) {
	   return rv;
       }
   }
   return 0;
}

// Wait for incoming events (callbacks and data samples).
// timeoutMilliseconds specifies the maximum amount of time you want to
// wait (set it to 0 if you do not want to block or to UINT_MAX to wait
// forever). Return 1 if events are pending, 0 if timed out, <0 on error
ierr kapi_manager::waitForEvents(unsigned timeoutMilliseconds)
{
    int fd = connection_to_kerninstd->get_sock();

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    fd_set efds;
    FD_ZERO(&efds);
    FD_SET(fd, &efds);

    struct timeval tv, *ptv = &tv;
    tv.tv_sec = 0;
    tv.tv_usec = timeoutMilliseconds;
    if (timeoutMilliseconds == UINT_MAX) {
	ptv = NULL;
    }

    if (select(fd+1, &rfds, NULL, &efds, ptv) < 0 ||
	FD_ISSET(fd, &efds)) {
	return communication_error;
    }
    if (FD_ISSET(fd, &rfds)) {
	return 1; // Data available
    }
    return 0; // No data available, timeout
}
    
// Alternative way of waiting for events: call getEventQueueDescriptor and
// then select() on it (read and error descriptors). You must then
// call handleEvents to process them
int kapi_manager::getEventQueueDescriptor()
{
    return connection_to_kerninstd->get_sock();
}

ierr kapi_manager::handleEvents()
{
    // It's CRUCIAL to process already-buffered messages before handling
    // new messages, lest messages be handled out of order!  [causing
    // rollback assert fails in the case of handling sample data messages]
    while (connection_to_kerninstd->buffered_requests()) {
	T_kerninstdClient::message_tags ret =
	    connection_to_kerninstd->process_buffered();
	if (ret == T_kerninstdClient::error) {
            return communication_error;
	}
    }

    // Check to see if any messages came from the network
    int rv = waitForEvents(0); // 0 -> no blocking
    if (rv <= 0) {
	return rv; // error or no events pending
    }

    // Now we know that there are messages pending, read and process them
    do {
	T_kerninstdClient::message_tags ret =
	    connection_to_kerninstd->waitLoop();
	if (ret == T_kerninstdClient::error) {
	    return communication_error;
	}
    } while (!xdrrec_eof(connection_to_kerninstd->net_obj()));

    // We've handled at least one incoming igen call and processed it.
    // But it's possible that while we were in the midst of waitLoop() above,
    // say, waiting for the result of a sync igen call we make to kerninstd,
    // kerninstd sent us some other igen call, which is now buffered up.  The
    // point is that this other igen message, if any, was buffered up.  It's
    // been read.  So it will no longer appear to select()/poll() that there
    // is a message waiting on this socket.  So if we do nothing, the message
    // will be buffered OK but not handled.  So we need to handle any
    // messages that happened to be pulled off the socket & buffered up
    // during waitLoop().  Tricky bug.
    while (connection_to_kerninstd->buffered_requests()) {
	T_kerninstdClient::message_tags ret =
	    connection_to_kerninstd->process_buffered();
	if (ret == T_kerninstdClient::error) {
	    return communication_error;
	}
    }

    return 0;
}

ierr kapi_manager::detach()
{
   dout << "kerninstapi: cleaning up & exiting now..." << endl;

#ifdef sparc_sun_solaris2_7
   delete theGlobalGroupMgr;
   theGlobalGroupMgr = NULL; // help purify find mem leaks

   delete theGlobalVTimerMgr;
   theGlobalVTimerMgr = NULL; // help purify find mem leaks

   delete cswitchTraceBuffer; // harmless if NULL
   cswitchTraceBuffer = NULL;
#endif

   delete theGlobalKerninstdMachineInfo;
   theGlobalKerninstdMachineInfo = NULL;

   // can't fry theGlobalInstrumenter before the above
   // visi-cleanup-&-uninstrument, because that code may 
   // use theGlobalInstrumenter!
   delete theGlobalInstrumenter;
   theGlobalInstrumenter = NULL; // help purify find mem leaks

   delete global_moduleMgr;
   global_moduleMgr = NULL; // help purify find mem leaks
   
   delete connection_to_kerninstd;
   connection_to_kerninstd = NULL; // help purify find mem leaks

   delete calleeWatchers;
   calleeWatchers = NULL;

   delete samplingIntervals;
   samplingIntervals = NULL;

   delete callbackMap;
   callbackMap = NULL;

   delete memAlloc;
   memAlloc = NULL;

   cout << "kerninstapi: Goodbye" << endl;

   return 0;
}

// Return the total number of modules loaded in the kernel
unsigned kapi_manager::getNumModules() const
{
   return global_moduleMgr->numModules();
}

ierr kapi_manager::findModule(const char *mod_name, kapi_module *kmod)
{
   loadedModule *mod = global_moduleMgr->find(mod_name);
   if (mod == 0) {
      return module_not_found;
   }
   // Initialize a wrapper for the loaded module
   *kmod = kapi_module(mod, this);
   return 0;
}

ierr kapi_manager::findFunctionByAddress(kptr_t addr, kapi_function *kfunc)
{
   const moduleMgr *constMgr = global_moduleMgr; // hack around private func
   const function_t* func = constMgr->findFnOrNULL(addr, true);
   if (func == 0) {
      return function_not_found;
   }
   // Initialize a wrapper for func
   *kfunc = kapi_function(func);
   return 0;
}

#ifdef sparc_sun_solaris2_7
// Extract all 64-bit-safe registers from a set and return them as a vector
static pdvector<Register> convertSetToVector(const abi &theABI,
                                             const sparc_reg_set &regset)
{
   pdvector<Register> rv;

   sparc_reg_set::const_iterator ireg = regset.begin_intregs();
   for (; ireg != regset.end_intregs(); ireg++) {
       const sparc_reg &reg = (const sparc_reg&)*ireg;
       if (theABI.isReg64Safe(reg)) {
          unsigned num = reg.getIntNum();
          rv.push_back(num);
       }
   }
   return rv;
}

// Save live G's in the space for local variables on the stack.
// kerninstd will allocate a frame large enough to save 16 64-bit regs
static void emitSaveGs(const sparc_reg_set &liveGs, const abi &theABI,
		       tempBufferEmitter *em)
{
   const int stackBias = theABI.getStackBias();
   int regOffset = stackBias - 8; // first reg goes at [%fp - 8]
    
   for (sparc_reg_set::const_iterator ireg = liveGs.begin(); 
        ireg != liveGs.end(); ireg++) {
      instr_t *i = new sparc_instr(sparc_instr::store, 
                                   sparc_instr::stExtended,
                                   (const sparc_reg&)*ireg, 
                                   sparc_reg::fp, regOffset);
      em->emit(i);
      regOffset -= 8;
   }
}

// The opposite of the above
static void emitRestoreGs(const sparc_reg_set &liveGs, const abi &theABI,
			  tempBufferEmitter *em)
{
   const int stackBias = theABI.getStackBias();
   int regOffset = stackBias - 8; // first reg goes at [%fp - 8]
    
   for (sparc_reg_set::const_iterator ireg = liveGs.begin(); 
        ireg != liveGs.end(); ireg++) {
      instr_t *i = new sparc_instr(sparc_instr::load, 
                                   sparc_instr::ldExtendedWord,
                                   sparc_reg::fp, regOffset, 
                                   (const sparc_reg&)*ireg);
      em->emit(i);
      regOffset -= 8;
   }
}
#elif defined(i386_unknown_linux2_4)
static pdvector<Register> convertSetToVector(const abi &,
                                             const x86_reg_set &regset)
{
   pdvector<Register> rv;

   x86_reg_set::const_iterator ireg = regset.begin_intregs();
   for (; ireg != regset.end_intregs(); ireg++) {
      const x86_reg &the_reg = (const x86_reg&)*ireg;
      if(the_reg.is32BitIntReg()) {
         // only keep track of 32-bit reg usage for use in instrumentation
         rv.push_back(the_reg.getIntNum());
      }
   }
   return rv;
}
#elif defined(ppc64_unknown_linux2_4)

//Because on power we only save those registers that must be saved,
//the vector of available registers needs to have the following order:
//registers that can be used without saving, THEN registers that 
//can be used, but only after a save.
static pdvector<Register> convertSetToVector(const abi &,
                                             const power_reg_set *deadregs,
                                             const power_reg_set *saveregs=NULL)
{
   pdvector<Register> rv;
   std::stack<Register> rstack;

   if (saveregs) {
      power_reg_set::const_iterator sreg = saveregs->begin_intregs();
      for (; sreg != saveregs->end_intregs(); sreg++) {
         rstack.push((*sreg).getIntNum());
      }
   }
   power_reg_set::const_iterator dreg = deadregs->begin_intregs();
   for (; dreg != deadregs->end_intregs(); dreg++) {
      rstack.push((*dreg).getIntNum());
   }
   
   while(!rstack.empty()) {
      rv.push_back(rstack.top());
      rstack.pop();
   }
   return rv;
}
// Save live G's in the space for local variables on the stack.
// kerninstd will allocate a frame large enough to save 16 64-bit regs
static void emitSaveVolatile(const power_reg_set &liveGs, const abi &theABI,
		       tempBufferEmitter *em)
{
   int regOffset =  ((power64_abi &)theABI).getLocalVariableSpaceFrameOffset();
   
   for (power_reg_set::const_iterator ireg = liveGs.begin(); 
        ireg != liveGs.end(); ireg++) {
      instr_t *i = new power_instr(power_instr::store, 
                                   power_instr::stDoubleDS,
				   (const power_reg &)*ireg,
				   power_reg::gpr1, //stack pointer
				   regOffset);
      em->emit(i);
      regOffset += GPRSIZE;
   }
}

// The opposite of the above
static void emitRestoreVolatile(const power_reg_set &liveGs, const abi &theABI,
			  tempBufferEmitter *em)
{
   int regOffset =  ((power64_abi &)theABI).getLocalVariableSpaceFrameOffset();
   
   for (power_reg_set::const_iterator ireg = liveGs.begin(); 
        ireg != liveGs.end(); ireg++) {
      instr_t *i = new power_instr(power_instr::load, 
                                   power_instr::ldDoubleDS,
				   (const power_reg&) *ireg,
                                   power_reg::gpr1, regOffset);
      em->emit(i);
      regOffset += GPRSIZE;
   }
}
static void emitSaveNonVolatile(unsigned numRegsToSave, const abi &theABI,
				tempBufferEmitter *em) {
    int regOffset =  ((power64_abi &)theABI).getGeneralRegSaveFrameOffset();
    
    //store registers from r31 down on the stack
    for (unsigned i = 0; i < numRegsToSave ; i++) {
	power_reg regToSave =  power_reg(power_reg::rawIntReg, 31-i); 
	instr_t *instr = new power_instr(power_instr::store, power_instr::stDoubleDS, 
				regToSave,  
				power_reg::gpr1, //stack pointer  
				regOffset + GPRSIZE*i //offset off SP 
	    );
	em->emit(instr);
  }
}
static void emitRestoreNonVolatile(unsigned numRegsToRestore, 
				   const abi &theABI,
				   tempBufferEmitter *em) {
    int regOffset =  ((power64_abi &)theABI).getGeneralRegSaveFrameOffset();
    
    //load registers from r31 down on the stack
    for (unsigned i = 0; i < numRegsToRestore ; i++) {
	power_reg regToSave =  power_reg(power_reg::rawIntReg, 31-i); 
	instr_t *instr = new power_instr(power_instr::load, 
					 power_instr::ldDoubleDS, 
					 regToSave,  
					 power_reg::gpr1, //stack pointer  
					 regOffset + GPRSIZE*i //offset off SP 
	    );
	em->emit(instr);
  }
}
#endif

// Given an AST, generates two versions of code: one assuming that no
// save has been emitted and the other assuming it has. "delete" them
// when done. Returns true if the save has been emitted
//
// The implementation is tricky. We have three options: 1) emit no
// save/restores, 2) emit save/restore, 3) emit save/restore and
// store/load some %g registers on the stack (super save). Option 3
// gives us the largest number of registers so we try it first and
// then decide if we can do any better. In practice, Option 3 is
// used only on the 32-bit platform.
static bool generateCodeFromAST(kapi_manager *,
                                AstNode *node, instPoint *splicePoint, 
				const regset_t *availSet,
				unsigned *pMaxRegsUsed,
				relocatableCode_t **pNoSaveCode,
				relocatableCode_t **pSaveCode)
{
#ifdef sparc_sun_solaris2_7
   bool forceSuperSave = false;

   const abi &theABI = theGlobalInstrumenter->getKernelABI();
   sparc_tempBufferEmitter emNoSave(theABI);
   sparc_tempBufferEmitter emSave(theABI);
   sparc_tempBufferEmitter emSuperSave(theABI); // Manually save/restore %G's
    
   // Find registers available if we emit a save instruction
   sparc_reg_set availSetSave(sparc_reg_set::save, 
                               *(const sparc_reg_set*)availSet, true);
   availSetSave -= sparc_reg::sp;
   availSetSave -= sparc_reg::fp;

   // Find registers available if we emit a save instruction and
   // save/restore live G registers on the stack
   sparc_reg_set availSetSuperSave = availSetSave +
      sparc_reg_set::allGs - sparc_reg::g0 - sparc_reg::g7;

   // Regs that are not free even after a regular save
   sparc_reg_set liveGs = availSetSuperSave - availSetSave;

   // Prepare to initialize AST's registerSpace. Notice that we may filter out
   // some free registers: in 32-bit sparc we use only 64-bit-safe registers
   pdvector<Register> availNoSave = convertSetToVector(theABI, 
                                                       *(const sparc_reg_set*)availSet);
   pdvector<Register> availSave = convertSetToVector(theABI, availSetSave);
   pdvector<Register> availSuperSave = convertSetToVector(theABI,
                                                          availSetSuperSave);
   const unsigned numAvailNoSave = availNoSave.size();
   const unsigned numAvailSave = availSave.size();
   const unsigned numAvailSuperSave = availSuperSave.size();

   // Generate code that should be wrapped with super save/restore
   registerSpace regspaceSuperSave(numAvailSuperSave,
                                   availSuperSave.begin(), 0, 0);
   emitSaveGs(liveGs, theABI,
              &emSuperSave); // a bunch of stx to [%fp - offs]

   HostAddress base = 0;
   node->setNoSaveRestoreFlag(false);
   node->generateCode(0, &regspaceSuperSave, (char *)&emSuperSave,
                      base, true, true, splicePoint);

   emitRestoreGs(liveGs, theABI,
                 &emSuperSave); // a bunch of ldx from [%fp - offs]
   emSuperSave.complete();

   // Check to see how many registers were actually used
   unsigned maxNumRegsUsed = regspaceSuperSave.getMaxNumUsed();
   // dout << "Used " << maxNumRegsUsed << " registers\n";

   // See if we can generate the regular-save version of the code
   if (maxNumRegsUsed <= numAvailSave) {
      registerSpace regspaceSave(numAvailSave, availSave.begin(), 0, 0);

      node->setNoSaveRestoreFlag(false);
      node->generateCode(0, &regspaceSave, (char *)&emSave,
                         base, true, true, splicePoint);
      emSave.complete();
      *pSaveCode = new sparc_relocatableCode(emSave);
      *pMaxRegsUsed = maxNumRegsUsed;
   }
   else {
      *pSaveCode = new sparc_relocatableCode(emSuperSave);
      // We have to fudge the number of registers used or kerninstd
      // will refuse to splice it in, since it does not know that we
      // manually saved/restored %g's
      *pMaxRegsUsed = numAvailSave; // not numAvailSuperSave!
      if (numAvailSave == numAvailNoSave) {
         // kerninstd will think that it has enough registers even
         // without a save and will attempt to emit the no-save
         // version, which is unfortunate
         forceSuperSave = true;
      }
   }
   //
   // Now, generate the no-save version of the code
   //
   if (forceSuperSave) {
      // We do need a save here, but kerninstd will not emit it for us,
      // so let's do that ourselves
      registerSpace regspace(numAvailSuperSave,
                             availSuperSave.begin(), 0, 0);
      const unsigned frameSize = theABI.getBigFrameAlignedNumBytes();
      instr_t *i = new sparc_instr(sparc_instr::save, -frameSize);
      emNoSave.emit(i);

      emitSaveGs(liveGs, theABI, &emNoSave);// a bunch of stx to [%fp - offs]

      HostAddress base = 0;
      node->setNoSaveRestoreFlag(false);
      node->generateCode(0, &regspace, (char *)&emNoSave,
                         base, true, true, splicePoint);

      emitRestoreGs(liveGs, theABI, &emNoSave); // a bunch of ldx
      i = new sparc_instr(sparc_instr::restore);
      emNoSave.emit(i);
      emNoSave.complete();
      *pNoSaveCode = new sparc_relocatableCode(emNoSave);
      return false; // do not ask kerninstd for another save
   }
   else if (node->containsCall()) {
      // If the code contains a function call, we will force a save
      // even if we seem to have enough regs available, since we need
      // a stack frame for preserving %g's and %o's across the call
      registerSpace callRegSpace(numAvailSave,
                                 availSave.begin(), 0, 0);

      const unsigned frameSize = theABI.getBigFrameAlignedNumBytes();
      instr_t *i = new sparc_instr(sparc_instr::save, -frameSize);
      emNoSave.emit(i);

      node->setNoSaveRestoreFlag(false);
      node->generateCode(0, &callRegSpace, (char *)&emNoSave,
                         base, true, true, splicePoint);

      i = new sparc_instr(sparc_instr::restore);
      emNoSave.emit(i);
      emNoSave.complete();
      *pNoSaveCode = new sparc_relocatableCode(emNoSave);
      return false; // do not ask kerninstd for another save
   }
   else if (maxNumRegsUsed <= numAvailNoSave) {
      // We have enough registers. Generate the no-save version of the code
      registerSpace regspace(numAvailNoSave, availNoSave.begin(), 0, 0);

      node->setNoSaveRestoreFlag(true);
      node->generateCode(0, &regspace, (char *)&emNoSave,
                         base, true, true, splicePoint);
      emNoSave.complete();
      *pNoSaveCode = new sparc_relocatableCode(emNoSave);
      return false; // no save is necessary
   }
   else {
      // Not enough free registers for the no-save version. Emit
      // dummy code. Will assert if attempted to use it
      *pNoSaveCode = new sparc_relocatableCode(relocatableCode_t::_dummy);
      return true; // ask kerninstd for a save
   }
#elif defined(i386_unknown_linux2_4)
   const abi &theABI = theGlobalInstrumenter->getKernelABI();
   x86_tempBufferEmitter emSave(theABI);
   x86_tempBufferEmitter emNoSave(theABI);

   pdvector<Register> availNoSave;
   pdvector<Register> availSave;

   // Check for 64-bit code
   if(node->is64BitCode()) {
      emit_64bit_code = 1;

      // NOTE: we leave availNoSave empty so that we're guaranteed to
      //       ask kerninstd for a save/restore.

      // Emit extra code to set up our own stack frame. We assume that
      // EBP has already been pushed by our pre-instrumentation 
      // and will be restored by our post-instrumentation
      instr_t *i;
      i = new x86_instr(x86_instr::mov, x86_reg::eSP, x86_reg::eBP);
      emSave.emit(i);
      i = new x86_instr(x86_instr::sub, (uint32_t)256, x86_reg::eSP);
      emSave.emit(i);

      // Set pseudoregister values to offset into local frame.
      for(int i=1; i<=32; i++)
         availSave.push_back((Register)(-8*i));      
   }
   else {
      emit_64bit_code = 0;

      // Find registers available if we emit a save instruction
      x86_reg_set availSetSave(x86_reg_set::save);

      // Prepare to initialize AST's registerSpace.
      availNoSave = convertSetToVector(theABI, 
                                       *(const x86_reg_set*)availSet);
      availSave = convertSetToVector(theABI, availSetSave);
   }

   const unsigned numAvailNoSave = availNoSave.size();
   const unsigned numAvailSave = availSave.size();
   HostAddress base = 0;
   registerSpace regspaceSave(numAvailSave, availSave.begin(), 0, 0);
   node->setNoSaveRestoreFlag(false);
   node->generateCode(0, &regspaceSave, (char *)&emSave,
                      base, true, true, splicePoint);

   if(emit_64bit_code == 1) {
     // Restore ESP
     instr_t *i = new x86_instr(x86_instr::mov, x86_reg::eBP, x86_reg::eSP);
     emSave.emit(i);
   }

   emSave.complete();
   *pSaveCode = new x86_relocatableCode(emSave);
      
   // Check to see how many registers were actually used
   unsigned maxNumRegsUsed = regspaceSave.getMaxNumUsed();

   if (!node->getNeedSaveRestoreFlag() && (maxNumRegsUsed <= numAvailNoSave)) {
      // There's no special-needs insns in the generated code and we have 
      // enough registers. Generate the no-save version of the code
      registerSpace regspace(numAvailNoSave, availNoSave.begin(), 0, 0);

      node->setNoSaveRestoreFlag(true);
      node->generateCode(0, &regspace, (char *)&emNoSave,
                         base, true, true, splicePoint);
      emNoSave.complete();
      *pNoSaveCode = new x86_relocatableCode(emNoSave);
      *pMaxRegsUsed = regspace.getMaxNumUsed();
      return false; // no save is necessary
   }
   else {
      *pNoSaveCode = new x86_relocatableCode(relocatableCode_t::_dummy);
      *pMaxRegsUsed = maxNumRegsUsed;
      return true; // ask kerninstd for a save
   }
#elif defined(ppc64_unknown_linux2_4)
   bool forceSuperSave = false;

   const abi &theABI = theGlobalInstrumenter->getKernelABI();
   power_tempBufferEmitter emNoSave(theABI);
   power_tempBufferEmitter emSave(theABI);
   power_tempBufferEmitter emSuperSave(theABI); 
// Manually save/restore volatile regs
     
    
   // Find max registers available if we save all nonvolatile regs 
   power_reg_set availSetSave(
      	*(const power_reg_set*)availSet +
       *(const power_reg_set *)(theABI.robustints_of_regset(
                                   &power_reg_set::allIntRegs))); 
			
   
   availSetSave -= power_reg_set::always_live_set;

   power_reg_set regsToSave =  
         *(const power_reg_set *)(theABI.robustints_of_regset(
                                   &power_reg_set::allIntRegs)) - 
         *(const power_reg_set *)availSet  ;


   // Find registers available if we save nonvolatile regs +
   //parameter regs (r3-r10)
   power_reg_set availSetSuperSave = availSetSave +
     *(const power_reg_set *)(theABI.volatileints_of_regset(&power_reg_set::allIntRegs));

   // Regs that are not free even after a regular save
   power_reg_set liveGs = availSetSuperSave - availSetSave;

   // Prepare to initialize AST's registerSpace.
   pdvector<Register> availNoSave = convertSetToVector(theABI, 
                                           (const power_reg_set *)availSet);
   //In this case, we only want to save the minimum number of registers
   //necessary, so in order to force AST to use dead regs first,
   //we specify them as the first and separate parameter to this function
   pdvector<Register> availSave = convertSetToVector(theABI, 
              (const power_reg_set *)availSet,
              (const power_reg_set *)&regsToSave ); 
   pdvector<Register> availSuperSave = convertSetToVector(theABI,
                                                          &availSetSuperSave);
   const unsigned numAvailNoSave = availNoSave.size();
   const unsigned numAvailSave = availSave.size();
   const unsigned numAvailSuperSave = availSuperSave.size();

   // Generate code that should be wrapped with super save/restore
   registerSpace regspaceSuperSave(numAvailSuperSave,
                                   availSuperSave.begin(), 0, 0);
   emitSaveVolatile(liveGs, theABI,
              &emSuperSave); // a bunch of std's to parameter save area

   HostAddress base = 0;
   node->setNoSaveRestoreFlag(false);
   node->generateCode(0, &regspaceSuperSave, (char *)&emSuperSave,
                      base, true, true, splicePoint);

   emitRestoreVolatile(liveGs, theABI,
                 &emSuperSave); // a bunch of ldd's from parameter save area
   emSuperSave.complete();

   // Check to see how many registers were actually used
   unsigned maxNumRegsUsed = regspaceSuperSave.getMaxNumUsed();
   // dout << "Used " << maxNumRegsUsed << " registers\n";

   // See if we can generate the regular-save version of the code
   if (maxNumRegsUsed <= numAvailSave) {
      registerSpace regspaceSave(numAvailSave, availSave.begin(), 0, 0);

      node->setNoSaveRestoreFlag(false);
      node->generateCode(0, &regspaceSave, (char *)&emSave,
                         base, true, true, splicePoint);
      emSave.complete();
      *pSaveCode = new power_relocatableCode(emSave);
      *pMaxRegsUsed = maxNumRegsUsed;
   }
   else {
      *pSaveCode = new power_relocatableCode(emSuperSave);
      // We have to fudge the number of registers used or kerninstd
      // will refuse to splice it in, since it does not know that we
      // manually saved/restored volatile regs
      *pMaxRegsUsed = numAvailSave; // not numAvailSuperSave!
      if (numAvailSave == numAvailNoSave) {
         // kerninstd will think that it has enough registers even
         // without a save and will attempt to emit the no-save
         // version, which is unfortunate
         forceSuperSave = true;
      }
   }
   //
   // Now, generate the no-save version of the code
   //
   if (forceSuperSave) {
      // We do need a save here, but kerninstd will not emit it for us,
      // so let's do that ourselves
      registerSpace regspace(numAvailSuperSave,
                             availSuperSave.begin(), 0, 0);
      const unsigned frameSize = theABI.getBigFrameAlignedNumBytes();
       //First, we need to generate a stack frame.
      instr_t *i = new power_instr(power_instr::store, 
				   power_instr::stDoubleUpdateDS, 
				   power_reg::gpr1, 
				   power_reg::gpr1, //stack pointer
				   -frameSize); 

      emNoSave.emit(i);

      emitSaveVolatile(liveGs, theABI, &emNoSave);
      emitSaveNonVolatile(maxNumRegsUsed, theABI, &emNoSave);

      HostAddress base = 0;
      node->setNoSaveRestoreFlag(false);
      node->generateCode(0, &regspace, (char *)&emNoSave,
                         base, true, true, splicePoint);

      emitRestoreVolatile(liveGs, theABI, &emNoSave); // a bunch of ldd
      emitRestoreNonVolatile(maxNumRegsUsed,theABI, &emNoSave);

      //restore the stack pointer
      i = new power_instr(power_instr::load, 
				   power_instr::ldDoubleX, 
				   power_reg::gpr1, 
				   power_reg::gpr0, //no second addr reg.
				   power_reg::gpr1); 
      emNoSave.emit(i);
      emNoSave.complete();
      *pNoSaveCode = new power_relocatableCode(emNoSave);
      return false; // do not ask kerninstd for another save
   }
   else if (node->containsCall()) {
      // If the code contains a function call, we will force a save
      // even if we seem to have enough regs available, since we need
      // a stack frame for preserving volatile regs across the call
      registerSpace callRegSpace(numAvailSave,
                                 availSave.begin(), 0, 0);

      const unsigned frameSize = theABI.getBigFrameAlignedNumBytes();

      //generate a stack frame.
      instr_t *i = new power_instr(power_instr::store, 
				   power_instr::stDoubleUpdateDS, 
				   power_reg::gpr1, 
				   power_reg::gpr1, //stack pointer
				   -frameSize); 
      emNoSave.emit(i);

      node->setNoSaveRestoreFlag(false);
      node->generateCode(0, &callRegSpace, (char *)&emNoSave,
                         base, true, true, splicePoint);

       //restore the stack pointer
      i = new power_instr(power_instr::load, 
			  power_instr::ldDoubleX, 
			  power_reg::gpr1, 
			  power_reg::gpr0, //no second addr reg.
			  power_reg::gpr1); 
      emNoSave.emit(i);
      emNoSave.complete();
      *pNoSaveCode = new power_relocatableCode(emNoSave);
      return false; // do not ask kerninstd for another save
   }
   else if (maxNumRegsUsed <= numAvailNoSave) {
      // We have enough registers. Generate the no-save version of the code
      registerSpace regspace(numAvailNoSave, availNoSave.begin(), 0, 0);

      node->setNoSaveRestoreFlag(true);
      node->generateCode(0, &regspace, (char *)&emNoSave,
                         base, true, true, splicePoint);
      emNoSave.complete();
      *pNoSaveCode = new power_relocatableCode(emNoSave);
      return false; // no save is necessary
   }
   else {
      // Not enough free registers for the no-save version. Emit
      // dummy code. Will assert if attempted to use it
      *pNoSaveCode = new power_relocatableCode(relocatableCode_t::_dummy);
      return true; // ask kerninstd for a save
   }
#endif
}

int kapi_manager::insertSnippet(const kapi_snippet &snippet,
				const kapi_point &point)
{
   kptr_t spliceAddr = point.getAddress();
   kapi_point_location type = point.getType();
   bool preReturn = (type == kapi_pre_return);

   instPoint splicePoint(spliceAddr);

   regset_t *availSet = theGlobalInstrumenter->getFreeRegsAtPoint(spliceAddr, preReturn);

   AstNode *node = snippet.getAst();
   unsigned maxNumRegsUsed;
   relocatableCode_t *noSaveCode, *saveCode;
   generateCodeFromAST(this, node, &splicePoint, availSet,
                       &maxNumRegsUsed, &noSaveCode, &saveCode);

   // Splice the code generated
   unsigned reqid;
   if (preReturn) {
      reqid = theGlobalInstrumenter->splice_prereturn(spliceAddr,
#ifndef i386_unknown_linux2_4
						    0, // this many int32's
						    maxNumRegsUsed,//int64
#else
                                                    maxNumRegsUsed, //int32
                                                    0, //int64
#endif
						    *noSaveCode,
						    *saveCode,
						    *noSaveCode,
						    *saveCode);
   }
   else {
      reqid = theGlobalInstrumenter->splice_preinsn(spliceAddr,
#ifndef i386_unknown_linux2_4
                                                    0, // this many int32's
                                                    maxNumRegsUsed,// int64's
#else
                                                    maxNumRegsUsed, //int32
                                                    0, //int64
#endif
                                                    *noSaveCode,
                                                    *saveCode);
   }

   delete noSaveCode;
   delete saveCode;

   return reqid;
}

ierr kapi_manager::removeSnippet(int handle)
{
   theGlobalInstrumenter->unSplice(handle);
   // hardware counters that were used by the snippet should be freed
   // by whoever calls this function, since they're most likely to
   // know what counters were used
   return 0;
}

// Toggle the testing flag. When this flag is set, we download the
// instrumentation to the kernel, but do not splice it in
ierr kapi_manager::setTestingFlag(bool flag)
{
   connection_to_kerninstd->setJustTestingFlag(flag);
   return 0;
}

// Meke the kernel invoke function kerninst_call_once in
// the /dev/kerninst driver. Useful for debugging.
ierr kapi_manager::callOnce() const
{
   connection_to_kerninstd->call_once();
   return 0;
}

kptr_t kapi_manager::malloc(unsigned size)
{
   return memAlloc->alloc(size);
}

ierr kapi_manager::free(kptr_t addr)
{
   memAlloc->dealloc(addr);
   return 0;
}

dptr_t kapi_manager::kernel_addr_to_kerninstd_addr(kptr_t addr)
{
   return memAlloc->kernelAddrToKerninstdAddr(addr);
}

ierr kapi_manager::meminit(kptr_t /*addr*/, int /*c*/, unsigned/*nbytes*/)
{
   assert(false && "Not implemented yet. Use memcopy instead");
   return 0;
}

ierr kapi_manager::memcopy(kptr_t from, void *to, unsigned nbytes)
{
   // Try not to overrun the output buffer if nbytes is not divisible by 4
   unsigned nmaxwords = (nbytes + 3) / 4;
   unsigned nfullwords = nbytes / 4;
   unsigned ntail = nbytes - nfullwords * 4;

   pdvector<uint32_t> words = 
      theGlobalInstrumenter->peek_kernel_contig(from, nbytes);
   assert(nmaxwords == words.size());

   int *dest = (int *)to;
   for (unsigned i=0; i<nfullwords; i++) {
      dest[i] = words[i];
   }
   if (ntail > 0) {
      unsigned lastword = words[nfullwords];
      memcpy(&dest[nfullwords], &lastword, ntail);
   }

   return 0;
}

ierr kapi_manager::memcopy(void *from, kptr_t to, unsigned nbytes)
{
   // Try not to overrun the output buffer if nbytes is not divisible by 4
   unsigned nfullwords = nbytes / 4;
   unsigned ntail = nbytes - nfullwords * 4;

   pdvector<uint32_t> words;
   int *src = (int *)from;
   for (unsigned i=0; i<nfullwords; i++) {
      words += src[i];
   }
   if (ntail > 0) {
      unsigned lastword = 0;
      memcpy(&lastword, &src[nfullwords], ntail);
      words += lastword;
   }
   if (theGlobalInstrumenter->poke_kernel_contig(words, to, nbytes) < 0) {
      return poke_failed;
   }
   return 0;
}

// This routine byteswaps the given memory region if client's ordering
// does not match that of the kernel. Currently, it assumes that the
// kernel is on SPARC, so it is big endian
void kapi_manager::to_kernel_byte_order(void *data, unsigned size)
{
#ifdef _LITTLE_ENDIAN_
   // Kludge: peek/poke_kernel_contig already swap bytes within words.
   // What we need to do is to swap words
   unsigned numWords = size / 4;
   assert(numWords * 4 == size);

   uint32_t *head = (uint32_t *)data;
   uint32_t *tail = head + numWords - 1;
   unsigned numPairs = numWords / 2;
   for (unsigned i=0; i<numPairs; i++) {
      uint32_t c = *head;
      *head = *tail;
      *tail = c;
      head++;
      tail--;
   }
#endif
}

void kapi_manager::to_client_byte_order(void *data, unsigned size)
{
   // works both ways
   to_kernel_byte_order(data, size);
}

// A few classes to support sampling
class itemToSampleAndReport {
 public:
   virtual itemToSampleAndReport *dup() const = 0;
   virtual void emitObtainValueToReport(tempBufferEmitter &,
					const reg_t &scratchReg,
					const reg_t &destReg) const = 0;
   // The following functions are used only by vectorToSampleAndReport
   // They should go away as soon as we implement proper code generation
   // in vectorToSampleAndReport
   virtual dptr_t getSamplingAddr() const {
      assert(false);
      return 0;
   }
   virtual unsigned getElemsPerVector() const {
      assert(false);
      return 0;
   }
   virtual unsigned getBytesPerStride() const {
      assert(false);
      return 0;
   }
};

class uint32ToSampleAndReport : public itemToSampleAndReport {
 private:
   dptr_t kerninstdAddrToSample;
 public:
   uint32ToSampleAndReport(dptr_t ikerninstdAddrToSample) :
      kerninstdAddrToSample(ikerninstdAddrToSample) {}
   itemToSampleAndReport *dup() const {
      return new uint32ToSampleAndReport(kerninstdAddrToSample);
   }
   void emitObtainValueToReport(tempBufferEmitter &em,
                                const reg_t &scratchReg,
                                const reg_t &destReg) const;
};

class vectorToSampleAndReport : public itemToSampleAndReport {
 private:
   dptr_t kerninstdBaseAddr;
   unsigned elemsPerVector;
   unsigned bytesPerStride;
 public:
   vectorToSampleAndReport(dptr_t ikerninstdAddrToSample,
                           unsigned iElemsPerVector,
                           unsigned iBytesPerStride) :
      kerninstdBaseAddr(ikerninstdAddrToSample), 
      elemsPerVector(iElemsPerVector),
      bytesPerStride(iBytesPerStride) {
   }
   itemToSampleAndReport *dup() const {
      return new vectorToSampleAndReport(kerninstdBaseAddr,
                                         elemsPerVector,
                                         bytesPerStride);
   }
   dptr_t getSamplingAddr() const {
      return kerninstdBaseAddr;
   }
   unsigned getElemsPerVector() const {
      return elemsPerVector;
   }
   unsigned getBytesPerStride() const {
      return bytesPerStride;
   }
   void emitObtainValueToReport(tempBufferEmitter & /*em*/,
                                const reg_t &/*scratchReg*/,
                                const reg_t &/*destReg*/) const {
      assert(false);
   }
};

class stuffToSampleAndReport {
 private:
   pdvector<itemToSampleAndReport*> stuff;
   stuffToSampleAndReport &operator=(const stuffToSampleAndReport &);
 public:
   stuffToSampleAndReport() {}
   stuffToSampleAndReport(const stuffToSampleAndReport &src) {
      stuff.reserve_exact(src.stuff.size());
      pdvector<itemToSampleAndReport*>::const_iterator iter=src.stuff.begin();
      pdvector<itemToSampleAndReport*>::const_iterator finish=src.stuff.end();
      for (; iter != finish; ++iter) {
         const itemToSampleAndReport *itemptr = *iter;
         stuff += itemptr->dup();
      }
   }
   ~stuffToSampleAndReport() {
      pdvector<itemToSampleAndReport*>::iterator iter = stuff.begin();
      pdvector<itemToSampleAndReport*>::iterator finish = stuff.end();
      for (; iter != finish; ++iter) {
         itemToSampleAndReport *item = *iter;
         delete item;
      }
   }
   unsigned size() const { return stuff.size(); }
   const itemToSampleAndReport &operator[](unsigned ndx) const {
      return *stuff[ndx];
   }
   void operator+=(const itemToSampleAndReport &theItem) {
      stuff += theItem.dup();
   }
};

void uint32ToSampleAndReport::emitObtainValueToReport(tempBufferEmitter &em,
                                                      const reg_t &scratchReg,
                                                      const reg_t &destReg) const {
   // In this class, OK for scratchReg to equal destReg64
#ifdef sparc_sun_solaris2_7
   instr_t *i = new sparc_instr(sparc_instr::sethi, sparc_instr::HiOf(),
				kerninstdAddrToSample, (const sparc_reg&)scratchReg);
   em.emit(i);
   i = new sparc_instr(sparc_instr::load, sparc_instr::ldExtendedWord,
                       (const sparc_reg&)scratchReg,
                       sparc_instr::lo_of_32(kerninstdAddrToSample),
                       (const sparc_reg&)destReg);
   em.emit(i);
#elif defined(i386_unknown_linux2_4)
   instr_t *i = new x86_instr(x86_instr::mov, 
                              (uint32_t)kerninstdAddrToSample, 
                              (const x86_reg&)scratchReg);
   em.emit(i);
   i = new x86_instr(x86_instr::load, (const x86_reg&)scratchReg, 
                     (const x86_reg&)destReg);
   em.emit(i);
#elif defined(ppc64_unknown_linux2_4)
   assert(false && "emitObtainValueToReport power nyi");
#endif
}

// Emit code that will sample a bunch of values in kerninstd and call
// a function to send these values to kperfmon
void emitSamplingCodeSeveralValues(tempBufferEmitter &em,
				   unsigned sampleRequestId,
				   const stuffToSampleAndReport &stuff)
{
#ifdef sparc_sun_solaris2_7
   // reminder: format for call to presentSampleDataSeveralValues is:
   // (1) client id [%o0]
   // (2) sample request id [%o1]
   // (3) number of sample values (two) [%o2]
   // (4) pointer to sample values [%o3]

   // Due to (4) above, we will always emit a save first.  After that, we initialize
   // the regSetManager.  Note that before the save, %o0 contains the client-id;
   // after the save, it'll be in %i0 -- we must remember to mov it to %o0.

   const sparc_reg_set *iavailRegs = (const sparc_reg_set*)
      theGlobalInstrumenter->getAvailRegsForDownloadedToKerninstdCode(false);
      // false --> don't overwrite the client-id param
   assert(!iavailRegs->exists(sparc_reg::o0)); // being used for client-id (passed in)

   const abi &kerninstdABI = em.getABI();
   
   // For the save: we need the usual minFrame plus [kerninstdAddrsToSample.size()]
   // 8-byte data values.
   const pair<unsigned, unsigned> stackFrameInfo =
       kerninstdABI.calculateStackFrameSize(8*stuff.size());
   const unsigned stackFrameNumBytes = stackFrameInfo.first;
   const unsigned stackFrameMyDataOffset = stackFrameInfo.second;

   instr_t *i = new sparc_instr(sparc_instr::save, -stackFrameNumBytes);
   em.emit(i);

   sparc_reg_set availRegs(sparc_reg_set::save, *iavailRegs, true);
   assert(!availRegs.exists(sparc_reg::i0)); // %o0 wasn't avail before the save...
   delete iavailRegs;

   const unsigned numvreg64sNeeded = 1;
   const unsigned numvreg32sNeeded = 1;
   const unsigned numrobustreg32sNeeded = 0;

   const regSetManager theRegSetMgr(availRegs,
                                    false, // don't save; assert that we have enough
                                    numvreg64sNeeded,
                                    numvreg32sNeeded,
                                    numrobustreg32sNeeded,
                                    sparc_reg_set(sparc_reg::o0) +
                                    sparc_reg::o1 + sparc_reg::o2 +
                                    sparc_reg::o3 + sparc_reg::o4 +
                                    sparc_reg::o5,
                                    kerninstdABI);
   // The explicit regs are needed because we're passing arguments to
   // a standard-ly compiled C function, presentSampleDataSeveralValues().
   // This, when coupled with the vregs that we need, would lead to a save.
   // To avoid this, we use the explicit %o regs as scratch registers, too.

   sparc_reg vreg32_0 = theRegSetMgr.getNthVReg32(0);
   const sparc_reg vreg64_0 = theRegSetMgr.getNthVReg64(0);

   // Prepare arguments to presentSampleDataSeveralValues():
   // (1) client id (as chosen by kerninstd) -- was already in %o0 (passed in to us)
   // before the save, so it's now in %i0.  Move it to %o0.
   i = new sparc_instr(sparc_instr::mov, sparc_reg::i0, sparc_reg::o0);
   em.emit(i);

   // (2) sample request id (as chosen by us)
   em.emitImm32(sampleRequestId, sparc_reg::o1);
      
   // (3) number of sample data values: stuff.size()
   em.emitImm32(stuff.size(), sparc_reg::o2);

   // (4) *pointer* to 8-byte data values
   // We need to write to the stack as follows:
   /* %fp -----------------> ----------------------------------------------------
                             padding to ensure that %sp is abi-aligned (8 or 16 bytes)
                             final 8-byte data value
      %sp + stackFrameMyDataOffset + 8*(kerninstdAddrsToSample.size()-1) -------------> 
                             second 8-byte data value
      %sp + stackFrameMyDataOffset + 8 -------------> 
                             first 8-byte data value
      %sp + stackFrameMyDataOffset -->
      %sp + MINFRAME ---------> ----------------------------------------------------
                              space to save sixteen 4-byte values in case of trap
                                      (reserved for OS use -- don't touch!)
                              plus 1-word hidden parameter plus 6 words for callee
                              to save its register arguments, plus enough padding
                              to ensure 8-byte alignment.
      %sp ----------------->  ----------------------------------------------------
   */

   // Sample each 64-bit data item
   unsigned item_offset = stackFrameMyDataOffset;
   for (unsigned itemlcv=0; itemlcv < stuff.size(); ++itemlcv) {
      const itemToSampleAndReport &item = stuff[itemlcv];
      item.emitObtainValueToReport(em,
                                   vreg32_0, // scratch reg
                                   vreg64_0 // dest reg
                                   );
      
      // Emit code to put the sampled 64-bit data item onto the stack
      i = new sparc_instr(sparc_instr::store, sparc_instr::stExtended,
                          vreg64_0, sparc_reg::sp, item_offset);
      em.emit(i);
      
      item_offset += 8;
   }
   
   // And don't forget to set [valuesPointerReg] to point to all of the above!
   // [%sp + stackFrameMyDataOffset].
   i = new sparc_instr(sparc_instr::add, sparc_reg::sp, stackFrameMyDataOffset,
                       sparc_reg::o3);
   em.emit(i);

   // Can't make a tail call (with restore in the delay slot), because that would
   // immediately remove our stack frame, which contains important data that
   // the callee needs to use!  Nuts.
   const pdstring presentSampleDataSeveralValues_string("presentSampleDataSeveralValues");
   
   em.emitSymAddr(presentSampleDataSeveralValues_string, vreg32_0);
   i = new sparc_instr(sparc_instr::callandlink, vreg32_0);
   em.emit(i);
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);

   // We need to do ret; restore since we explicitly did a save.  While we're at it,
   // assert that the regSetManager didn't do a(nother) save on our behalf.
   assert(!theRegSetMgr.needsSaveRestore());
   i = new sparc_instr(sparc_instr::ret);
   em.emit(i);
   i = new sparc_instr(sparc_instr::restore);
   em.emit(i);

#elif defined(i386_unknown_linux2_4)

   assert(!"this function's not used on x86\n");
   // Arguments to presentSampleDataSeveralValues are:
   // (1) client id [passed to us on stack]
   // (2) sample request id
   // (3) number of sample values
   // (4) pointer to sample values

   // We better act like a normal function, thus we need the standard prologue
   instr_t *i = new x86_instr(x86_instr::push, x86_reg::eBP);
   em.emit(i);
   i = new x86_instr(x86_instr::mov, x86_reg::eSP, x86_reg::eBP);
   em.emit(i);

   // Get the samples, storing them on stack
   for(int itemlcv = stuff.size() - 1; itemlcv >= 0; --itemlcv) {
      // we push in reverse order, so sample array is accessed as expected
      const itemToSampleAndReport &item = stuff[itemlcv];
      item.emitObtainValueToReport(em,
                                   x86_reg::eAX, // scratch reg
                                   x86_reg::eAX // dest reg
                                   );
      
      // Emit code to put the sampled data item onto the stack
      i = new x86_instr(x86_instr::push, x86_reg::eAX);
      em.emit(i);
   }

   // Record addr for start of samples, currently kept by stack pointer
   i = new x86_instr(x86_instr::mov, x86_reg::eSP, x86_reg::eCX);
   em.emit(i);
   
   // Push args to function:
   // (1) client id (as chosen by kerninstd) -- was passed to us on stack
   i = new x86_instr(x86_instr::load, (unsigned char)0x8, x86_reg::eBP, 
                     x86_reg::eAX);
   em.emit(i);
   i = new x86_instr(x86_instr::push, x86_reg::eAX);
   em.emit(i);
   // (2) sample request id (as chosen by us)
   em.emitImm32(sampleRequestId, x86_reg::eAX);
   i = new x86_instr(x86_instr::push, x86_reg::eAX);
   em.emit(i);   
   // (3) number of sample data values: stuff.size()
   em.emitImm32(stuff.size(), x86_reg::eAX);
   i = new x86_instr(x86_instr::push, x86_reg::eAX);
   em.emit(i);
   // (4) *pointer* to 4-byte data values
   i = new x86_instr(x86_instr::push, x86_reg::eCX);
   em.emit(i);
   
   // Call function
   const pdstring presentSampleDataSeveralValues_string("presentSampleDataSeveralValues");
   em.emitSymAddr(presentSampleDataSeveralValues_string, x86_reg::eAX);
   i = new x86_instr(x86_instr::call, x86_reg::eAX);
   em.emit(i);

   // All done, now we need the standard epilogue
   i = new x86_instr(x86_instr::leave);
   em.emit(i);
   i = new x86_instr(x86_instr::ret);
   em.emit(i);
#elif defined(ppc64_unknown_linux2_4)
assert(false && "should not be used on power, use kapi sampling interface that x86 uses");
#endif

   em.complete();
}

void kapi_manager::convertRegionsIntoStuffToSample(kapi_mem_region *data, 
                                                   unsigned n,
                                                   stuffToSampleAndReport *stuff)
{
   for (unsigned i=0; i<n; i++) {
      unsigned ndwords = 
         (data[i].nbytes + sizeof(sample_type) - 1) / sizeof(sample_type);
      dptr_t addr = kernel_addr_to_kerninstd_addr(data[i].addr);
      for (unsigned j=0; j<ndwords; j++) {
         (*stuff) += uint32ToSampleAndReport(addr); // no idea why uint32
         addr += sizeof(sample_type); // no pointer arith here
      }
   }
}

void kapi_manager::requestSampleSeveralValues(unsigned requestId,
                                              kapi_mem_region *data,
                                              unsigned n, unsigned ms) 
{
   pdvector<kptr_t> addrs;
   for (unsigned i=0; i<n; i++) {
      unsigned ndwords = 
         (data[i].nbytes + sizeof(sample_type) - 1) / sizeof(sample_type);
#ifndef sparc_sun_solaris2_7
      kptr_t addr = data[i].addr;
#else
      // convert kernel addr to in kerninstd addr (dptr_t to kptr_t cast is
      // ugly but necessary)
      dptr_t daddr = kernel_addr_to_kerninstd_addr(data[i].addr);
      kptr_t addr = daddr;
#endif
      for (unsigned j=0; j<ndwords; j++) {
         addrs += addr;
         addr += sizeof(sample_type); // no pointer arith here
      }
   }
   theGlobalInstrumenter->requestSampleData(requestId, addrs);
   if(ms != 0)
      theGlobalInstrumenter->periodicallyDoSampling(requestId, ms);
   else
      theGlobalInstrumenter->doSamplingOnceNow(requestId);
}

// Copy memory from the kernel periodically. Return a request handle.
// We assume that we are sampling a collection of 64-bit integers,
// so the data is converted accordingly if the daemon and the client
// run on different architectures
int kapi_manager::sample_periodically(kapi_mem_region *regions, unsigned n, 
				      data_callback_t to,  // arrival callback
				      unsigned ms) // sampling interval
{
   const unsigned sampleRequestId =
      theGlobalInstrumenter->reserveNextDownloadedToKerninstdCodeId();
   callbackMap->set(sampleRequestId, to);
   samplingIntervals->set(sampleRequestId, ms);
   requestSampleSeveralValues(sampleRequestId, regions, n, ms);
   return sampleRequestId;
}

// Stop the sampling request given a handle
ierr kapi_manager::stop_sampling(int reqid)
{
   theGlobalInstrumenter->stopPeriodicallyDoingSampling(reqid);
   theGlobalInstrumenter->removeSampleRequest(reqid);
   callbackMap->undef(reqid);
   samplingIntervals->undef(reqid);
   return 0;
}

// If new_ms == 0 -> stop sampling. If old_ms == 0, new_ms != 0 -> start
// sampling. In all other cases -> change the sampling interval
ierr kapi_manager::adjust_sampling_interval(int handle, unsigned new_ms)
{
   unsigned &current_ms = samplingIntervals->get(handle);

   if (new_ms != 0) {
      // Will change the interval or start sampling
      theGlobalInstrumenter->periodicallyDoSampling(handle, new_ms);
   }
   else if (current_ms != 0) {
      // Stop sampling, but do not remove the sampling code as
      // the stop_sampling routine would do.
      theGlobalInstrumenter->stopPeriodicallyDoingSampling(handle);
   }
   current_ms = new_ms; // Updates the dictionary

   return 0;
}

// For internal use only: gets called when a data sample arrives.
// Will trigger the appropriate callback
void kapi_manager::dispatchSampleData(const void *data)
{
   const pdvector<one_sample_data> *theData = 
      (const pdvector<one_sample_data> *)data;
   pdvector<one_sample_data>::const_iterator iter = theData->begin();
   pdvector<one_sample_data>::const_iterator finish = theData->end();

   for (; iter != finish; ++iter) {
      const one_sample_data &item = *iter;

      if (last_timeOfSample != (uint64_t)-1) {
         assert(item.timeOfSample >= last_timeOfSample && 
                "sample time rollback");
      }
      else {
         last_timeOfSample = item.timeOfSample;
      }
      
      const uint64_t timeOfSample = item.timeOfSample;

      const unsigned uniqSampleReqId = item.uniqSampleReqId;
      const pdvector<sample_t> &values = item.values;

      data_callback_t handler;
      if (!callbackMap->find(uniqSampleReqId, handler)) {
         dout << "Can not find a callback for given sample, ignoring\n";
      }
      else {
         handler(uniqSampleReqId, timeOfSample,
                 values.begin(), values.size());
      }
   }
}

// For internal use only: gets called when new modules/functions get
// loaded into the kernel.
void kapi_manager::dispatchDynloadEvent(kapi_dynload_event_t event,
					const loadedModule *mod,
					const function_t *func)
{
   if (dynload_cb != 0) {
      kapi_module *kmod = 0;
      kapi_function *kfunc = 0;
      if (mod != 0) {
         kmod = new kapi_module(mod, this);
      }
      if (func != 0) {
         kfunc = new kapi_function(func);
      }
      // Call the user-supplied callback
      dynload_cb(event, kmod, kfunc);

      if (kfunc != 0) {
         delete kfunc;
      }
      if (kmod != 0) {
         delete kmod;
      }
   }
}

#ifdef sparc_sun_solaris2_7
extern bool usesPic(kapi_hwcounter_kind, int);
#endif

// Read the value of a hardware counter across all cpus
ierr kapi_manager::readHwcAllCPUs(kapi_hwcounter_kind kind, 
                                  kapi_vector<sample_t> *cpu_samples)
{
   assert(theGlobalInstrumenter != 0);
   
   if(kind == HWC_NONE) {
      cerr << "ERROR: HWC_NONE passed to readHwcAllCPUs\n";
      return kapi_manager::unsupported_hwcounter;
   }

   // Annotate kind with specific pmc num, if necessary
   unsigned type = (unsigned)kind;
   if(kind != HWC_TICKS) {
#if defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4)
      kapi_hwcounter_set currPcrVal;
      if(currPcrVal.readConfig() < 0) {
         cerr << "ERROR: readHwcAllCPUs - unable to read performance counter settings\n";
         return kapi_manager::internal_error;
      }
      unsigned perfctr_num = currPcrVal.findCounter(kind);
      if(perfctr_num == (unsigned)-1) {
         cerr << "ERROR: readHwcAllCPUs - supplied kind not currently being counted\n";
         return kapi_manager::unsupported_hwcounter;
      }
      type |= (perfctr_num << 16);
#elif defined(sparc_sun_solaris2_7)
      if(usesPic(kind, 1))
         type |= (1 << 16);
#endif
   }

   // Do the read
   pdvector<sample_t> samples = theGlobalInstrumenter->readHwcAllCPUs(type);
   for(unsigned i = 0; i < samples.size(); i++)
      cpu_samples->push_back(samples[i]);
   return 0;
}

// Return the number of CPUs in the system being instrumented
unsigned kapi_manager::getNumCPUs() const
{
   assert(theGlobalKerninstdMachineInfo != 0);
   return theGlobalKerninstdMachineInfo->getNumCPUs();
}

// Return the maximum physical id among CPUs
unsigned kapi_manager::getMaxCPUId() const
{
   assert(theGlobalKerninstdMachineInfo != 0);
   return theGlobalKerninstdMachineInfo->getMaxCPUId();
}


// Return the physical id of the i-th CPU (may be different from "i")
unsigned kapi_manager::getIthCPUId(unsigned i) const
{
   assert(theGlobalKerninstdMachineInfo != 0);
   return theGlobalKerninstdMachineInfo->getIthCPUId(i);
}

// Return the frequency of the i-th CPU
unsigned kapi_manager::getCPUClockMHZ(unsigned icpu) const
{
   assert(theGlobalKerninstdMachineInfo != 0);
   return theGlobalKerninstdMachineInfo->getCPUClockMHZ(icpu);
}


// Return the system clock frequency (may not equal %tick frequency
// if %stick is present)
unsigned kapi_manager::getSystemClockMHZ() const
{
   assert(theGlobalKerninstdMachineInfo != 0);
   return theGlobalKerninstdMachineInfo->getSystemClockMHZ();
}

// Return a time stamp from the machine being instrumented
uint64_t kapi_manager::getKerninstdMachineTime() const
{
   assert(theGlobalInstrumenter != 0);
   return theGlobalInstrumenter->getKerninstdMachineTime();
}

// Return the priority interrupt level used for running the dispatcher (
// the context-switch code)
unsigned kapi_manager::getDispatcherLevel() const
{
   assert(theGlobalKerninstdMachineInfo != 0);
   return theGlobalKerninstdMachineInfo->getDispatcherLevel();
}

// Return the <kernel abi, kerninstd abi> pair
kapi_vector<unsigned> kapi_manager::getABIs() const
{
   assert(theGlobalKerninstdMachineInfo != 0);
   kapi_vector<unsigned> rv;
   rv.push_back(theGlobalKerninstdMachineInfo->getKernelABICode());
   rv.push_back(theGlobalKerninstdMachineInfo->getKerninstdABICode());
   return rv;
}

// Create a vector of all modules in the kernel
ierr kapi_manager::getAllModules(kapi_vector<kapi_module> *vec)
{
    const moduleMgr::const_iterator finish = global_moduleMgr->end();
    moduleMgr::const_iterator iter = global_moduleMgr->begin();
    for (; iter != finish; iter++) {
	const loadedModule *mod = *iter;
	vec->push_back(kapi_module(mod, this));
    }
    return 0;
}

// Register a callback for getting notifications about
// new code being loaded/removed
ierr kapi_manager::registerDynloadCallback(dynload_callback_t cb)
{
   dynload_cb = cb;
   return 0;
}

// Create instrumentation point at an arbitrary address
ierr kapi_manager::createInstPointAtAddr(kptr_t address, kapi_point *point)
{
   *point = kapi_point(address, kapi_pre_instruction);
   return 0;
}

// Find the module which has a function starting at addr
ierr kapi_manager::findModuleAndFunctionByAddress(kptr_t addr,
						  kapi_module *mod,
						  kapi_function *func)
{
   pair<pdstring, function_t*> p = global_moduleMgr->findModAndFnOrNULL(addr, false);
   if (p.second == 0 ||
       p.second->getEntryAddr() != addr) {//addr in the middle of the function
      return function_not_found;
   }
   int rv;
   if ((rv = findModule(p.first.c_str(), mod)) < 0) {
      return rv;
   }
   *func = kapi_function(p.second);
   return 0;
}

// Find instrumentation points that may be of interest to users: context
// switch code, system call path, ... Presently, only the switch-in-out
// points are implemented
ierr kapi_manager::findSpecialPoints(kapi_point_location type, 
				     kapi_vector<kapi_point> *pPoints) const
{
   static bool cswitchout_points_found = false;
   static pdvector<kptr_t> cswitchout_points; // cache the results
   static pdvector<kptr_t> cswitchin_points;

   if ((type == kapi_cswitch_out || type == kapi_cswitch_in) && 
       !cswitchout_points_found) {
#ifdef sparc_sun_solaris2_7
      pair< pair< pdvector<kptr_t>, pdvector<kptr_t> >, // category 1
         pair< pdvector<kptr_t>, pdvector<kptr_t> > > // category 2
         all_switch_points = 
            theGlobalVTimerMgr->obtainCSwitchOutAndInSplicePoints();
      cswitchout_points = all_switch_points.first.first;
      cswitchout_points += all_switch_points.second.first;
      cswitchin_points = all_switch_points.first.second;
      cswitchin_points += all_switch_points.second.second;
#elif defined(i386_unknown_linux2_4)
      extern moduleMgr *global_moduleMgr;
      const moduleMgr &theModuleMgr = *global_moduleMgr;
      const loadedModule &kernel_mod = *theModuleMgr.find("kernel");
      const function_t &schedule_fn =
         theModuleMgr.findFn(kernel_mod.find_1fn_by_name("schedule"), true);
      const function_t &switchto_fn =
         theModuleMgr.findFn(kernel_mod.find_1fn_by_name("__switch_to"), true);
      
      // the jmp to __switch_to happens after the real "switch" has been made,
      // so it is a perfect candidate for the switch-in point
      kptr_t st_entry = switchto_fn.getEntryAddr();
      cswitchin_points = schedule_fn.getCallsMadeTo_asOrigParsed(st_entry, true); // true --> include interproc *branches*
      if(cswitchin_points.size() != 1) {
         const function_t *context_switch_fn = theModuleMgr.findFnOrNULL(kernel_mod.find_1fn_by_name("context_switch"), true);
         if(context_switch_fn != NULL) {
            cswitchin_points = context_switch_fn->getCallsMadeTo_asOrigParsed(st_entry, true); // true --> include interproc *branches*
         }
         if(cswitchin_points.size() != 1) {
            cerr << "kapi_manager::findSpecialPoints - unable to locate context-switch points since could not find call to __switch_to() in either schedule() or context_switch()\n";
            return points_not_found;
         }
      }

      // the switch-out point was chosen by finding the first instruction
      // before the real "switch" that was larger than five bytes, so we
      // could ensure that we're not using trap-based instrumentation.
      // the instruction chosen just happens to be 22 bytes before the
      // switch-in point, thus the "cswitchin_points[0]-22"
      cswitchout_points.push_back(cswitchin_points[0]-22);
#elif defined(ppc64_unknown_linux2_4)
      extern moduleMgr *global_moduleMgr;
      const moduleMgr &theModuleMgr = *global_moduleMgr;
      const loadedModule &kernel_mod = *theModuleMgr.find("kernel");
      kptr_t schedule_fn_addr = kernel_mod.find_1fn_by_name("schedule");
      if (! schedule_fn_addr ) {
         //kgdb patch renames schedule() into do_schedule()
         schedule_fn_addr = kernel_mod.find_1fn_by_name("do_schedule");
      }
      const function_t &schedule_fn =
         theModuleMgr.findFn(schedule_fn_addr, true);
     
      kptr_t switchto_addr = kernel_mod.find_1fn_by_name("_switch_to");
      if (switchto_addr == 0)
         switchto_addr = kernel_mod.find_1fn_by_name("__switch_to");
      const function_t &switchto_fn =
         theModuleMgr.findFn(switchto_addr, true);
      const function_t &switch_fn =
         theModuleMgr.findFn(kernel_mod.find_1fn_by_name("_switch"), true);
      
      cswitchin_points = switchto_fn.getCallsMadeTo_asOrigParsed(switch_fn.getEntryAddr(), true); // true --> include interproc *branches*
      assert(cswitchin_points.size() == 1);
      cswitchout_points.push_back(cswitchin_points[0]-4);
      cswitchin_points[0] += 4;//right past the call, i.e. to return point
#endif
      cswitchout_points_found = true;
   }

   pdvector<kptr_t>::const_iterator iter;
   switch(type) {
   case kapi_cswitch_out:
      iter = cswitchout_points.begin();
      for (unsigned i = 0; iter != cswitchout_points.end(); iter++, i++) {
         pPoints->push_back(kapi_point(*iter, kapi_cswitch_out));
      }
      break;
   case kapi_cswitch_in:
      iter = cswitchin_points.begin();
      for (unsigned i = 0; iter != cswitchin_points.end(); iter++, i++) {
         pPoints->push_back(kapi_point(*iter, kapi_cswitch_in));
      }
      break;
   default:
      assert(false && "Unsupported point type");
   }
   return 0;
}

// Wrap the given snippet with preemption protection code - useful when
// keeping per-CPU data
kapi_snippet
kapi_manager::getPreemptionProtectedCode(const kapi_snippet &code) const
{
   kapi_vector<kapi_snippet> lock_code_unlock;

#ifdef sparc_sun_solaris2_7

   const unsigned disp_level = getDispatcherLevel();
    
   kapi_getpil_expr orig_level;
   kapi_bool_expr is_below_disp_level(kapi_lt, orig_level,
                                      kapi_const_expr(disp_level));
   kapi_if_expr raise_level_if_low(is_below_disp_level, 
                                   kapi_setpil_expr(kapi_const_expr(disp_level)));

   kapi_setpil_expr restore_level(orig_level);

   lock_code_unlock.push_back(orig_level);
   lock_code_unlock.push_back(raise_level_if_low);
   lock_code_unlock.push_back(code);
   lock_code_unlock.push_back(restore_level);
    
#elif defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4)

   kapi_disableint_expr clear_local_interrupts;
   kapi_enableint_expr set_local_interrupts;
   
   lock_code_unlock.push_back(clear_local_interrupts);
   lock_code_unlock.push_back(code);
   lock_code_unlock.push_back(set_local_interrupts);
    
#endif

   return kapi_sequence_expr(lock_code_unlock);
}

#ifdef sparc_sun_solaris2_7
sparc_reg_set getFreeRegsMultiplePoints(const kapi_point *points,
					unsigned numPoints)
{
   sparc_reg_set availSet = sparc_reg_set::allIntRegs - sparc_reg::fp - 
      sparc_reg::sp - sparc_reg::g7;
   for (unsigned i=0; i<numPoints; i++) {
      kptr_t addr = points[i].getAddress();
      // The point is not shielded with save/restore, report the
      // registers as they are.
      regset_t *freeregs = theGlobalInstrumenter->getFreeRegsAtPoint(addr, false);
      const sparc_reg_set this_point_result = *(sparc_reg_set*)freeregs;
      delete freeregs;
      availSet &= this_point_result;
   }
   return availSet;
}

// Upload a snippet into the kernel, but do not splice it yet. It
// can later be spliced at any of the given points. The points vector
// may be empty (which will generate code that can be spliced
// anywhere) at the expense of having less efficient instrumentation.
int kapi_manager::uploadSnippet(const kapi_snippet &snippet, 
				const kapi_vector<kapi_point> &points)
{
   // kludge: kperfmon-specific context switch instrumentation is
   // uploaded in a special way, sorry.
   kapi_snippet_type type = snippet.getType();
   if (type == kapi_snippet_vtimer_in) {
      const kapi_vtimer_cswitch_expr &vsnippet = 
         static_cast<const kapi_vtimer_cswitch_expr&>(snippet);
      kptr_t all_vtimers_addr = vsnippet.getAllVtimersAddr();
      pair<unsigned,unsigned> reqids = 
         theGlobalVTimerMgr->referenceVirtualizationCode(all_vtimers_addr);
      switchin_reqid =  reqids.first;
      return switchin_reqid;
   }
   else if (type == kapi_snippet_vtimer_out) {
      // referenceVirtualizationCode actually uploads both the switch-in and
      // switch-out snippets, so the switch-out code has probably 
      // been uploaded by the call above. Yet, it is safe to call it again,
      // it will simply bump up the ref count. Remember to do
      // dereferenceVirtualizationCode twice to compensate for that.
      const kapi_vtimer_cswitch_expr &vsnippet = 
         static_cast<const kapi_vtimer_cswitch_expr&>(snippet);
      kptr_t all_vtimers_addr = vsnippet.getAllVtimersAddr();
      pair<unsigned,unsigned> reqids = 
         theGlobalVTimerMgr->referenceVirtualizationCode(all_vtimers_addr);
      switchout_reqid = reqids.second;
      return switchout_reqid;
   }

   sparc_reg_set availSet; // empty set
   if (points.size()) {
      // One more kludge: if we want to insert the code at 
      // kapi_v*_routine -> get registers available through
      // vtimerMgr, not through instrumenter
      if (points[0].getType() == kapi_vrestart_routine) {
         availSet = theGlobalVTimerMgr->getVRestartAvailRegs();
      }
      else if (points[0].getType() == kapi_vstop_routine) {
         availSet = theGlobalVTimerMgr->getVStopAvailRegs();
      }
      else {
         // Need to analyze point after point and intersect their
         // available sets
         assert(false && "Not yet implemented");
      }
   }

   // Time to generate the code
   AstNode *node = snippet.getAst();
   unsigned maxNumRegsUsed;
   relocatableCode_t *noSaveCode, *saveCode;
   bool needsSave = generateCodeFromAST(this, node, 0, &availSet, 
                                        &maxNumRegsUsed,
                                        &noSaveCode, &saveCode);
   unsigned reqid;
   if (needsSave) {
      // Need to wrap *saveCode with save/restore and then upload it
      assert(false && "Not yet implemented");

      // Assert that vroutines do not need the save. Otherwise, their input
      // arguments suddenly become unavailable
      assert(points.size() == 0 || 
             (points[0].getType() != kapi_vrestart_routine &&
              points[0].getType() != kapi_vstop_routine));
   }
   else {
      reqid = theGlobalInstrumenter->downloadToKernel(*noSaveCode, 
                                                      true); // try nucleus
   }

   delete noSaveCode;
   delete saveCode;
    
   return reqid;
}

// Remote a previously-uploaded snippet
ierr kapi_manager::removeUploadedSnippet(unsigned handle)
{
   if ((switchin_reqid != 0 && handle == switchin_reqid) ||
       (switchout_reqid != 0 && handle == switchout_reqid)) {
      theGlobalVTimerMgr->dereferenceVirtualizationCode();
      return 0;
   }
   theGlobalInstrumenter->removeDownloadedToKernel(handle);
   return 0;
}

// Find where the snippet has been uploaded
kptr_t kapi_manager::getUploadedAddress(unsigned handle)
{
   pair<pdvector<kptr_t>,unsigned> theAddrs = theGlobalInstrumenter->
      queryDownloadedToKernelAddresses1Fn(handle);
   return theAddrs.first[theAddrs.second];
}

#elif (defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4))

// Toggle the debug bits in the kerninst driver by XOR'ing the bitfield
// with the supplied bitmask, which should set each bit to be toggled
ierr kapi_manager::toggleDebugBits(uint32_t bitmask) const
{
   connection_to_kerninstd->toggle_debug_bits(bitmask);
   return 0;
}

// Toggle the enable bits in the kerninst driver by XOR'ing the bitfield
// with the supplied bitmask, which should set each bit to be toggled
ierr kapi_manager::toggleEnableBits(uint32_t bitmask) const
{
   connection_to_kerninstd->toggle_enable_bits(bitmask);
   return 0;
}

#endif // platform-defines

kptr_t kapi_manager::getAllVTimersAddr()
{
   return connection_to_kerninstd->getAllVTimersAddr();
}

void kapi_manager::addTimerToAllVTimers(kptr_t timer_addr)
{
   connection_to_kerninstd->addTimerToAllVTimers(timer_addr);
}

void kapi_manager::removeTimerFromAllVTimers(kptr_t timer_addr)
{
   connection_to_kerninstd->removeTimerFromAllVTimers(timer_addr);
}

// Return a string representation of register usage of instruction at addr
static int reqRegAnalInfoForSingleInsn(kptr_t addr, pdstring *result)
{
   const moduleMgr &theModuleMgr = *global_moduleMgr;

   const pair<pdstring, const function_t*> modAndFn =
      theModuleMgr.findModAndFnOrNULL(addr, false); 
      // false --> not start of fn only
   if (modAndFn.second == NULL) {
      return (-1);
   }

   const function_t *fn = modAndFn.second;

#ifdef sparc_sun_solaris2_7
   sparc_instr::sparc_ru ru;
#else
   instr_t::registerUsage ru;
#endif

   instr_t *insn = fn->get1OrigInsn(addr);
   assert(insn);
   insn->getRegisterUsage(&ru);
   
   pdstring str;

   bool anythingYet = false;

   assert(ru.definitelyWritten);
   assert(ru.maybeWritten);
   assert(ru.definitelyUsedBeforeWritten);
   assert(ru.maybeUsedBeforeWritten);
   
   if (ru.definitelyWritten->count() > 0) {
      str += "kills: ";
      str += ru.definitelyWritten->disassx();
      anythingYet = true;
   }

   if (ru.maybeWritten->count() > 0) {
      if (anythingYet)
           str += " ";
      str += "maybeKills: ";
      str += ru.maybeWritten->disassx();
      anythingYet = true;
   }

   if (ru.definitelyUsedBeforeWritten->count() > 0) {
      if (anythingYet)
         str += " ";
      str += "made live: ";
      str += ru.definitelyUsedBeforeWritten->disassx();
      anythingYet = true;
   }

   if (ru.maybeUsedBeforeWritten->count() > 0) {
      if (anythingYet)
         str += " ";
      str += "maybeMadeLive: ";
      str += ru.maybeUsedBeforeWritten->disassx();
   }
   *result = str;
   return 0;
}

// Convert the dataflow info into its human-readable representation
static pdstring parse_dataflow_info_1insn(const dataflowFn1Insn &info) {
   return (info.fn.get())->sprintf("made live", "killed", false);
      // false --> print the killed part first
}

// Get a printable string of register analysis info for a given
// address. If beforeInsn is false, we include the effects of the
// instruction at addr in the analysis. If globalAnalysis is false, the
// results for this instruction alone are reported.
ierr kapi_manager::getRegAnalysisResults(kptr_t addr, bool beforeInsn,
					 bool globalAnalysis,
					 char *buffer, unsigned maxBytes)
{
   pdstring str;
   if (globalAnalysis) {
      // We need to be patient, and first ask kerninstd to perform the needed
      // live register analysis:
      pdvector<oneRegAnalysisReqInfo> request;
      oneRegAnalysisReqInfo one = {addr, beforeInsn};
      request += one;
       
      pdvector<dataflowFn1Insn> info =
         connection_to_kerninstd->syncRegisterAnalysis(request);
      assert(info.size() == 1);
      assert(addr == info[0].where.address);
      assert(beforeInsn == info[0].where.before);
 
      str = parse_dataflow_info_1insn(info[0]);
   }
   else {
      // User is just asking for register usage for this single
      // instruction.  That's a piece of cake, since we have direct access to
      // kerninstd's instr_t class and its getInformation() method.
      if (reqRegAnalInfoForSingleInsn(addr, &str) < 0) {
         return function_not_found;
      }
   }
   if (str.length() + 1 > maxBytes) {
      return not_enough_space;
   }
   strncpy(buffer, str.c_str(), str.length());
   buffer[str.length()] = '\0';
   return 0;
}

#ifdef sparc_sun_solaris2_7
// Convert a function into its outlined group id (UINT_MAX if none)
unsigned kapi_manager::parseOutlinedLevel(kptr_t fnEntryAddr) const
{
   pair<pdstring, function_t*> p =
      global_moduleMgr->findModAndFn(fnEntryAddr, true);
   assert(p.second != 0);
   return outliningMisc::parseOutlinedLevel(p.first, *p.second);
}

// converts current fnEntryAddr to its original version
kptr_t kapi_manager::getOutlinedFnOriginalFn(kptr_t fnEntryAddr) const
{
   return outliningMisc::getOutlinedFnOriginalFn(fnEntryAddr);
}

// The next fn is complex, so pay attention: it uses an addr
// of some function to figure out what group that function belonged to,
// if any.  With that info in mind, it then returns the addr of a
// similarly-outlined function representing the second arg.
kptr_t kapi_manager::getSameOutlinedLevelFunctionAs(
    kptr_t guideFnOutlinedAddr, kptr_t desiredFnOrigAddr) const
{
   pair<pdstring, function_t*> p =
      global_moduleMgr->findModAndFn(guideFnOutlinedAddr, true);
   assert(p.second != 0);

   return outliningMisc::getSameOutlinedLevelFunctionAs(p.first, *p.second,
                                                        desiredFnOrigAddr);
}

ierr kapi_manager::testOutliningOneFn(
    const char *modname, kptr_t fnEntryAddr,
    const dictionary_hash<const char*,kptr_t> &iknownDownloadedCodeAddrs)
{
   dictionary_hash<pdstring,kptr_t> knownDownloadedCodeAddrs(stringHash);
   dictionary_hash<const char*,kptr_t>::const_iterator iter = 
      iknownDownloadedCodeAddrs.begin();
   for (; iter != iknownDownloadedCodeAddrs.end(); iter++) {
      knownDownloadedCodeAddrs.set(pdstring(iter.currkey()), iter.currval());
   }

   const moduleMgr &theModuleMgr = *global_moduleMgr;
   const function_t &fn = theModuleMgr.findFn(fnEntryAddr, true);

   if (testOutliningOneFnInIsolation(pdstring(modname), fn,
				     knownDownloadedCodeAddrs) != 0) {
      return internal_error;
   }
   return 0;
}

ierr kapi_manager::parseNewFn(const char *mod_name, 
			      const char *mod_description,
			      const char *fn_name,
			      const kapi_vector<kptr_t> &chunk_starts,
			      const kapi_vector<unsigned> &chunk_sizes,
			      unsigned entry_ndx, unsigned data_ndx)
{
    const pdstring modName(mod_name);
    const pdstring modDescriptionIfNew(mod_description);
    const pdstring fnName(fn_name);

    pdvector< pair<kptr_t, unsigned> > codeRanges;
    kapi_vector<kptr_t>::const_iterator starts_iter = chunk_starts.begin();
    kapi_vector<unsigned>::const_iterator sizes_iter = chunk_sizes.begin();
    for (;
	 starts_iter != chunk_starts.end() && sizes_iter != chunk_sizes.end();
	 starts_iter++, sizes_iter++) {
	codeRanges.push_back(pair<kptr_t, unsigned>(*starts_iter,*sizes_iter));
    }

    pdvector<parseNewFunction> vpnf;
    parseNewFunction *pnf = vpnf.append_with_inplace_construction();
    new((void*)pnf)parseNewFunction(modName, modDescriptionIfNew,
				    fnName, codeRanges, entry_ndx,
				    data_ndx);

   connection_to_kerninstd->parseKernelRangesAsFns(vpnf);

   return 0;
}

void kapi_manager::asyncOutlineGroup(
    bool doTheReplaceFunction,
    bool replaceFunctionPatchUpCallSitesToo,
    kapi_manager::blockPlacementPrefs kapiPlacementPrefs,
    kapi_manager::ThresholdChoices kapiThresholdPrefs,
    kptr_t rootFnAddr,
    const dictionary_hash<kptr_t, kapi_vector<unsigned> > &kBlockCounters,
    const kapi_vector<kptr_t> &kForceIncludeDescendants)
{
    // Convert all kapi_vectors to pdvectors
    dictionary_hash<kptr_t, pdvector<unsigned> > fnBlockCounters(addrHash4);
    dictionary_hash<kptr_t, kapi_vector<unsigned> >::const_iterator ifunc = 
	kBlockCounters.begin();
    for (; ifunc != kBlockCounters.end(); ifunc++) {
	pdvector<unsigned> pdCounts;
	const kapi_vector<unsigned> &kCounts = ifunc.currval();
	kapi_vector<unsigned>::const_iterator icount = kCounts.begin();
	for (; icount != kCounts.end(); icount++) {
	    pdCounts.push_back(*icount);
	}

	kptr_t fnAddr = ifunc.currkey();
	fnBlockCounters.set(fnAddr, pdCounts);
    }

    // Do the same for forceIncludeDescendants
    pdvector<kptr_t> pdForceIncludeDescendants;
    kapi_vector<kptr_t>::const_iterator iforce =
	kForceIncludeDescendants.begin();
    for (; iforce != kForceIncludeDescendants.end(); iforce++) {
	pdForceIncludeDescendants.push_back(*iforce);
    }

    // Convert a couple of preferences from KAPI
    outlinedGroup::blockPlacementPrefs thePlacementPrefs;
    switch(kapiPlacementPrefs) {
    case kapi_manager::origSeq:
	thePlacementPrefs = outlinedGroup::origSeq;
	break;
    case kapi_manager::chains:
	thePlacementPrefs = outlinedGroup::chains;
	break;
    default:
	assert(false);
    }

    hotBlockCalcOracle::ThresholdChoices theThresholdPrefs;
    switch(kapiThresholdPrefs) {
    case kapi_manager::AnyNonZeroBlock:
	theThresholdPrefs = hotBlockCalcOracle::AnyNonZeroBlock;
	break;
    case kapi_manager::FivePercent:
	theThresholdPrefs = hotBlockCalcOracle::FivePercent;
	break;
    default:
	assert(false);
    }

    assert(theGlobalGroupMgr != 0);
    theGlobalGroupMgr->asyncOutlineGroup(doTheReplaceFunction,
					 replaceFunctionPatchUpCallSitesToo,
					 thePlacementPrefs,
					 theThresholdPrefs,
					 rootFnAddr, fnBlockCounters,
					 pdForceIncludeDescendants);
}


void kapi_manager::unOutlineGroup(kptr_t rootFnEntryAddr)
{
   assert(theGlobalGroupMgr != 0);
   theGlobalGroupMgr->unOutlineGroup(rootFnEntryAddr);
}

void kapi_manager::unOutlineAll()
{
   assert(theGlobalGroupMgr != 0);
   theGlobalGroupMgr->unOutlineAll();
}
#endif // sparc_sun_solaris2_7

// Collect callee addresses for a given callsite of an indirect call
ierr kapi_manager::watchCalleesAt(kptr_t addrFrom)
{
   if (calleeWatchers->defines(addrFrom)) {
      return site_already_watched;
   }

   ierr rv;
   const moduleMgr *constMgr = global_moduleMgr; // hack around private func
   const function_t* func = constMgr->findFnOrNULL(addrFrom, false); // false
      // -- addrFrom is not necessarily the entry point
   if (func == 0) {
      return function_not_found;
   }
   kapi_point point;
   if ((rv = createInstPointAtAddr(addrFrom, &point)) < 0) {
      return rv;
   }


#ifdef sparc_sun_solaris2_7
   sparc_instr *callInsn = (sparc_instr*)func->get1OrigInsn(addrFrom);
   sparc_reg rs1 = sparc_reg::g0;
   sparc_reg rs2 = sparc_reg::g0;
   int addrOffs = 0;
   if (!callInsn->isCallRegPlusOffset(&rs1, &addrOffs) &&
       !callInsn->isCallRegPlusReg(&rs1, &rs2)) {
      return not_indirect_call;
   }
#elif defined(i386_unknown_linux2_4)
   x86_instr *callInsn = (x86_instr*)func->get1OrigInsn(addrFrom);
   if(!callInsn->isUnanalyzableCall())
      return not_indirect_call;
#elif defined(ppc64_unknown_linux2_4)
   power_instr *callInsn = (power_instr*)func->get1OrigInsn(addrFrom);
   if(!callInsn->isUnanalyzableCall())
      return not_indirect_call;
#endif

   kapi_module kmod;
   if ((rv = findModule("kerninst", &kmod)) < 0) {
      return rv;
   }

   kapi_function kfunc;
   if ((rv = kmod.findFunction("kerninst_update_callee_count", &kfunc)) < 0) {
      return rv;
   }

   // kerninst_update_callee_count has two arguments: addrFrom and addrTo
   // addrFrom is const, addrTo is extracted from the call (jmpl) instruction
   kapi_vector<kapi_snippet> args;
   args.push_back(kapi_const_expr(addrFrom));


#ifdef sparc_sun_solaris2_7
   if (addrOffs != 0) {
      args.push_back(kapi_arith_expr(kapi_plus,
                                     kapi_raw_register_expr(rs1.getIntNum()),
                                     kapi_const_expr(addrOffs)));
   }
   else if (rs2 != sparc_reg::g0) {
      args.push_back(kapi_arith_expr(kapi_plus,
				     kapi_raw_register_expr(rs1.getIntNum()),
				     kapi_raw_register_expr(rs2.getIntNum())));
   }
   else {
      args.push_back(kapi_raw_register_expr(rs1.getIntNum()));
   }
#elif defined(i386_unknown_linux2_4)
   x86_instr::x86_cfi cfi;
   callInsn->getControlFlowInfo(&cfi);
   if(cfi.destination == instr_t::controlFlowInfo::memIndirect) {
      instr_t::address *memaddr = &cfi.memaddr;
      instr_t::address::addrtype memtype = memaddr->getType();
      switch(memtype) {
       case instr_t::address::singleRegPlusOffset: {
          args.push_back(
             kapi_arith_expr(kapi_deref,
                kapi_arith_expr(kapi_plus,
                                kapi_raw_register_expr(memaddr->getReg1()->getIntNum()),
                                kapi_const_expr(memaddr->getOffset()))));
          break;
       }
       case instr_t::address::scaledSingleRegPlusOffset: {
          args.push_back(
             kapi_arith_expr(kapi_deref,
                kapi_arith_expr(kapi_plus,
                                kapi_arith_expr(kapi_shift_left, 
                                                kapi_raw_register_expr(memaddr->getReg2()->getIntNum()), 
                                                kapi_const_expr(memaddr->getScale())),
                                kapi_const_expr(memaddr->getOffset()))));
          break;
       }
       case instr_t::address::scaledDoubleRegPlusOffset: {
          args.push_back(
             kapi_arith_expr(kapi_deref,
                kapi_arith_expr(kapi_plus,
                                kapi_arith_expr(kapi_shift_left, 
                                                kapi_raw_register_expr(memaddr->getReg2()->getIntNum()), 
                                                kapi_const_expr(memaddr->getScale())),
                                kapi_raw_register_expr(memaddr->getReg1()->getIntNum()))));
          break;
       }
       default:
          assert(!"internal error: call with invalid addressing mode\n");
          break;
      }
   }
   else if(cfi.destination == instr_t::controlFlowInfo::regRelative)
      args.push_back(kapi_raw_register_expr(cfi.destreg1->getIntNum()));
   else 
      assert(!"internal error: unknown destination type for indirect call\n");
#elif defined(ppc64_unknown_linux2_4)
   if (callInsn->isBranchToCounterAndLink() )
      args.push_back(kapi_raw_register_expr(CTR_AST_DATAREG));
   if (callInsn->isBranchToLinkRegAndLink() )
      args.push_back(kapi_raw_register_expr(LR_AST_DATAREG));   
#endif

   kapi_call_expr code(kfunc.getEntryAddr(), args);

   int sh;
   if ((sh = insertSnippet(code, point)) < 0) {
      return sh;
   }

   calleeWatchers->set(addrFrom, sh);

   return 0;
}

// Stop collecting callee addresses for a given callsite
ierr kapi_manager::unWatchCalleesAt(kptr_t addrFrom)
{
   unsigned sh;
   ierr rv;

   if (!calleeWatchers->find(addrFrom, sh)) {
      return site_not_watched;
   }
   if ((rv = removeSnippet(sh)) < 0) {
      return rv;
   }
   calleeWatchers->undef(addrFrom);

   return 0;
}

// Fill-in the supplied vectors with callee addresses and execution
// counts collected so far
ierr kapi_manager::getCalleesForSite(kptr_t siteAddr,
				     kapi_vector<kptr_t> *calleeAddrs,
				     kapi_vector<unsigned> *calleeCounts)
{
   // The data interleaves addresses with execution counts
   pdvector<kptr_t> data =
      connection_to_kerninstd->getCalleesForSite(siteAddr);

   pdvector<kptr_t>::const_iterator iter = data.begin();
   while (iter != data.end()) {
      calleeAddrs->push_back(*iter);
      iter++;

      assert(iter != data.end());
      calleeCounts->push_back(*iter);
      iter++;
   }

   return 0;
}

// Replace the function starting at oldFnStartAddr with the one
// starting at newFnStartAddr. Returns a request id to be used later 
// to unreplace the function.
int kapi_manager::replaceFunction(kptr_t oldFnStartAddr, 
                                  kptr_t newFnStartAddr)
{
   return theGlobalInstrumenter->replaceAFunction(oldFnStartAddr,
                                                  newFnStartAddr,
                                                  false);
}

// Undo the effect of replaceFunction request denoted by reqid
ierr kapi_manager::unreplaceFunction(unsigned reqid)
{
   theGlobalInstrumenter->unReplaceAFunction(reqid);
   return 0;
}

// Replace the destination of the call at callSiteAddr with newDestAddr.
// Returns a request id to be used later to unreplace the call.
int kapi_manager::replaceFunctionCall(kptr_t callSiteAddr,
                                      kptr_t newFnStartAddr)
{
   return theGlobalInstrumenter->replaceAFunctionCall(callSiteAddr,
                                                      newFnStartAddr);
}

// Undo the effect of replaceFunctionCall request denoted by reqid
ierr kapi_manager::unreplaceFunctionCall(unsigned reqid)
{
   theGlobalInstrumenter->unReplaceAFunctionCall(reqid);
   return 0;
}
