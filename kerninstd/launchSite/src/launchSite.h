// launchSite.h
// Launch Site: an addr in kernel code where we insert dynamically generated code,
// typically before that instruction (preInsn), but sometimes (when we need to unwind
// a tail call optimization) before a return (preReturn).

// So this class contains structures for (1) the launch site itself,
// and (2) the snippets of code that are executed, (3) theRAndR

#ifndef _LAUNCH_SITE_H_
#define _LAUNCH_SITE_H_

#include "funkshun.h"
#include "codeSnippet.h"
#include "patchHeap.h" // for allocating patch
#include "moduleMgr.h" // for register analysis
#include "kernelDriver.h"
#include "LRAndR.h" // launcher, relocator, and returner
#include "SpringBoardHeap.h"
#include "util/h/Dictionary.h"

class LaunchSite {
 public:
   // Of all of these exceptions thrown by install()
   class NeedMorePatchSpace {};
   class FailedToInstallLauncher {};
   class CouldntReachPatch {};
   class NotEnoughFreeRegsBeforeInsn {};
   
 private:
   // Global preferences:
   static bool verboseTimingsSummary;
   static bool verboseTimingsDetailed;

   // Some vrbles which, in theory, we don't have to store -- we could pass them as
   // parameters as needed.  Unfortunately, that makes for some ugly parameters.
   // Better to pass them to the ctor.
   kernelDriver &theKernelDriver;
   patchHeap &thePatchHeap; // where we allocate patches from
   SpringBoardHeap &theSpringBoardHeap;
   const moduleMgr &theModuleMgr;
   const function_t &fn;      // the fn that we (launchAddr) reside in

   // Constant information about this site:
   kptr_t launchAddr;
   instr_t *originsn;
   instr_t::controlFlowInfo *cfi;
   regset_t *intRegsAvailBeforeInsn, *intRegsAvailAfterInsn;
      // Doesn't take into account save/restore, if any.
   
   // The Code Patch.
   bool sterilizedByCompetingReplaceFn;
      // Should the replaceFn go away, we're free to re-instrument
   
   kptr_t installedPatchAddr;
   unsigned installedPatchAllocatedNumBytes;
      // we allocated this many bytes.  We may not need to use this many, though.
      // (The last few bytes of the code patch could, for example, be undefined!)
   unsigned installedPatchActuallyUsedNumBytes;
      // This number, <= installedPatchAllocatedNumBytes, is the number of
      // instruction bytes that are actually used (as opposed to undefined)
      // in the code patch.
   
   LRAndR *installedLRAndR; // NULL if uninstalled, perhaps due to sterilization

   // The code snippets:
   pdvector<codeSnippet*> snippets;
      // vec of **ptrs** so a codeSnippet's address isn't volatile as the vector gets
      // resized.
   dictionary_hash<pair<unsigned,unsigned>, codeSnippet*> preInsnSnippets;
   dictionary_hash<pair<unsigned,unsigned>, codeSnippet*> preReturnSnippets;
      // dictionaries: map from [client-unique-id, spliceReqId-within-client] to
      // codeSnippet*
   
//     sparc_instr getRelocatedInsn() const {
//        // get1OrigInsn() is a trap, because it doesn't know how to take into
//        // effect things like code replacement that happen after parsing.
//        //return fn.get1OrigInsn(launchAddr);
//        return originsn;
//     }

   static void makeJustIntRegs(regset_t *);
      // may not be needed, now that we have integer register iterators:

   void continueWithInstrument();
      // using current code snippet iterator, fire off async igen call to client,
      // using emitAddr from the emitter and availRegs stored here.  Don't
      // advance the iterator until we get a response
   
   void instrument();
      // Instruments the kernel by creating a code patch (& springboard if needed),
      // filling it with the contents of snippets[], and finally overwriting a single
      // instruction with a jump to the springboard or code snippet.
      // Replaces an existing code patch at this site, if any.
      // But we don't do jack if sterilizedByCompetingReplaceFn is true

   void unInstrument();
      // aborts an instrumentation in progress, if any.
      // Then, uninstalls launcher...frees code patch
      // BUG WARNING: patchHeap item is freed right away; there should be a safety
      // delay!

   void addCodeSnippetCommon(client_connection &, unsigned spliceReqId,
                             unsigned maxNumInt32RegsNeeded,
                             unsigned maxNumInt64RegsNeeded,
                             bool preInsn,
                             const relocatableCode_t *code_preinsn_ifnosave,
                             const relocatableCode_t *code_preinsn_ifnosave,
                             const relocatableCode_t *code_prereturn_ifnosave,
                             const relocatableCode_t *code_prereturn_ifnosave,
                             bool instrumentNow);
   
   // explicitly private to disallow use:
   LaunchSite(const LaunchSite &);
   LaunchSite& operator=(const LaunchSite &);

 public:
   static void setVerboseTimingsSummary();
   static void setVerboseTimingsDetailed();
   
   LaunchSite(kptr_t launchAddr,
              bool isterilizedByCompetingReplaceFn,
              const moduleMgr &theModuleMgr,
              kernelDriver &iKernelDriver, patchHeap &ipatchHeap,
              SpringBoardHeap &iSpringBoardHeap);
      // initially no snippets, no patch, no launcher, no springboard.

  ~LaunchSite();
      // does not call unInstall(), so you must remember to.

   void sterilizeDueToCompetingReplaceFn(bool replaceFnAlreadyInstalled);
      // may uninstrument, though won't replace the orig insn, assume that has
      // already been done by the replaceFn.  We'll assert that the param is true.
   void unSterilize(bool replaceFnAlreadyUninstalled);
      // may re-instrument.  We pretty much ignore the param, since we do the
      // same stuff no matter what.

   kptr_t getInstalledPatchAddr() const { return installedPatchAddr; }
   unsigned getInstalledPatchAllocatedNumBytes() const {
      return installedPatchAllocatedNumBytes;
   }
   unsigned getInstalledPatchActuallyUsedNumBytes() const {
      return installedPatchActuallyUsedNumBytes;
   }

   void addCodeSnippetPreInsn(client_connection &, unsigned spliceReqId,
                              unsigned maxNumInt32RegsNeeded,
                              unsigned maxNumInt64RegsNeeded,
                              const relocatableCode_t *code_ifnosave,
                              const relocatableCode_t *code_ifsave,
                              bool instrumentNow);
   void addCodeSnippetPreReturn(client_connection &, unsigned spliceReqId,
                                unsigned maxNumInt32RegsNeeded,
                                unsigned maxNumInt64RegsNeeded,
                                const relocatableCode_t *code_prereturn_ifnosave,
                                const relocatableCode_t *code_prereturn_ifsave,
                                const relocatableCode_t *code_preinsn_ifnosave,
                                const relocatableCode_t *code_preinsn_ifsave,
                                bool instrumentNow);

   bool removeCodeSnippet(client_connection &, unsigned spliceReqId,
                          bool reinstrumentNow);

   void kerninstdIsGoingDown(kptr_t addr);
      // kerninstd is dying; put back the original instruction NOW!
};

#endif
