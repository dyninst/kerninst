// codeSnippet.C

#include "codeSnippet.h"
#include "relocatableCode.h"
#include "kernelResolver.h"
#include "util/h/hashFns.h"

codeSnippet::codeSnippet(client_connection &itheClient,
                         unsigned ispliceReqId,
                         unsigned imaxNumInt32RegsNeeded,
                         unsigned imaxNumInt64RegsNeeded,
                         // note: no pre-code data allowed in the following params
                         const relocatableCode_t *icode_ifpreinsn_ifnosave,
                         const relocatableCode_t *icode_ifpreinsn_ifsave,
                         const relocatableCode_t *icode_ifprereturn_ifnosave,
                         const relocatableCode_t *icode_ifprereturn_ifsave) :
   theClient(itheClient),
   code_ifpreinsn_ifnosave(icode_ifpreinsn_ifnosave),
   code_ifpreinsn_ifsave(icode_ifpreinsn_ifsave),
   code_ifprereturn_ifnosave(icode_ifprereturn_ifnosave),
   code_ifprereturn_ifsave(icode_ifprereturn_ifsave)
{
   spliceReqId = ispliceReqId;
   maxNumInt32RegsNeeded = imaxNumInt32RegsNeeded;
   maxNumInt64RegsNeeded = imaxNumInt64RegsNeeded;

   // For now, the instrumentation code must be single-chunk relocatableCode_t's
   // (or a "Dummy" relocatableCode_t)

   // pre-insn: both noSave and ifSave versions can be dummy:
   assert(code_ifpreinsn_ifnosave->isDummy() ||
          code_ifpreinsn_ifnosave->numChunks() == 1);
   assert(code_ifpreinsn_ifsave->isDummy() ||
          code_ifpreinsn_ifsave->numChunks() == 1);

   // pre-return: both noSave and ifSave versions can be dummy:
   assert(code_ifprereturn_ifnosave->isDummy() ||
          code_ifprereturn_ifnosave->numChunks() == 1);
   assert(code_ifprereturn_ifsave->isDummy() ||
          code_ifprereturn_ifsave->numChunks() == 1);
}

codeSnippet::~codeSnippet() {
   delete code_ifpreinsn_ifnosave;
   delete code_ifpreinsn_ifsave;
   delete code_ifprereturn_ifnosave;
   delete code_ifprereturn_ifsave;
}

void codeSnippet::relocate_and_emit(directToMemoryEmitter_t &em,
                                    bool preInsn,
                                    bool useSaveRestoreVersion) const {
   const kptr_t emit_addr_in_kernel = em.getCurrentEmitAddrInKernel();

   const relocatableCode_t *theRelocatableCode = preInsn ?
      (useSaveRestoreVersion ? code_ifpreinsn_ifsave : code_ifpreinsn_ifnosave) :
      (useSaveRestoreVersion ? code_ifprereturn_ifsave : code_ifprereturn_ifnosave);

   // Here comes a nice strong assert:
   assert(!theRelocatableCode->isDummy());
   
   pdvector<kptr_t> chunkEmitAddrs;
   chunkEmitAddrs += emit_addr_in_kernel;

   dictionary_hash<pdstring, kptr_t> knownDownloadedCodeAddrs(stringHash);
   kernelResolver theKernelResolver(knownDownloadedCodeAddrs);
   const pdvector<insnVec_t*> &code =
      theRelocatableCode->obtainFullyResolvedCode(chunkEmitAddrs, theKernelResolver);
   assert(code.size() == 1 && "expected exactly one chunk");
   
   // assert theRelocatableCode != relocatableCode_t::dummy via the following indirect
   // (but fast) assert:  NOTE that this disallows zero-sized instrumentation, which
   // is probably OK in practice.
   assert(code[0]->numInsnBytes() > 0);

   insnVec_t::const_iterator iter = code[0]->begin();
   insnVec_t::const_iterator finish = code[0]->end();
   for (; iter != finish; ++iter) {
      instr_t *instr = instr_t::getInstr(**iter);
      em.emit(instr);
   }
}

