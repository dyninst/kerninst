// codeSnippet.h

#ifndef _CODE_SNIPPET_H_
#define _CODE_SNIPPET_H_

#include "clientConn.h"
#include "directToMemoryEmitter.h"
#include "relocatableCode.h"

class codeSnippet {
 private:
   // Formerly, we didn't store the snippet code here (we'd always make a callback
   // to the client when we wanted code).  But now we do store it.  In fact, we store
   // two copies of the snippet code (both are created by the client, of course): one
   // if we (kerninstd) decide that no save/restore needs to be added around the code,
   // and another in anticipation of save/restore around the code.
   // In each case, the code created by the client is *relocatable*, as it must be,
   // since the client has no idea where the code patch will reside.

   client_connection &theClient;
   unsigned spliceReqId; // unique, at least among this client's code snippets

   unsigned maxNumInt32RegsNeeded, maxNumInt64RegsNeeded;

   const relocatableCode_t *code_ifpreinsn_ifnosave; // no pre-code data allowed
   const relocatableCode_t *code_ifpreinsn_ifsave; // no pre-code data allowed
   const relocatableCode_t *code_ifprereturn_ifnosave; // no pre-code data allowed
   const relocatableCode_t *code_ifprereturn_ifsave; // no pre-code data allowed
   
   codeSnippet(const codeSnippet &);
   codeSnippet& operator=(const codeSnippet &);
   
 public:
   codeSnippet(client_connection &, unsigned spliceReqId,
               unsigned maxNumInt32RegsNeeded,
               unsigned maxNumInt64RegsNeeded,
               // note: no pre-code data allowed in the following params.
               // In particular, must be a single-chunk relocatableCode_t; we'll
               // assert as such.  Perhaps this restriction can be loosened some day.
               const relocatableCode_t *icode_ifpreinsn_ifnosave,
               const relocatableCode_t *icode_ifpreinsn_ifsave,
               const relocatableCode_t *icode_ifprereturn_ifnosave,
               const relocatableCode_t *icode_ifprereturn_ifsave);
  ~codeSnippet();

   unsigned getClientUniqueId() const { return theClient.getUniqueId(); }
   unsigned getSpliceReqId() const { return spliceReqId; }
   
   unsigned getMaxNumInt32RegsNeeded() const { return maxNumInt32RegsNeeded; }
   unsigned getMaxNumInt64RegsNeeded() const { return maxNumInt64RegsNeeded; }

   unsigned getMaxNumInsnBytes(bool preInsn,
                               bool assumingSaveRestore) const {
      // the answer may or may not vary depending on the parameters (probably not
      // for now), but what the heck...it may be needed down the road)
      const relocatableCode_t *theRelocatableCode =
         preInsn ? (assumingSaveRestore ? code_ifpreinsn_ifsave : code_ifpreinsn_ifnosave) : (assumingSaveRestore ? code_ifprereturn_ifsave : code_ifprereturn_ifnosave);

      if (theRelocatableCode->isDummy())
         return UINT_MAX; // or would an assertion failure be better?
         // (right now, we can't assert fail since this *does* indeed get
         // called for "dummy" relocatableCode_t's)
#ifdef ppc64_unknown_linux2_4
      else 
         return theRelocatableCode->chunkNumBytes(0) + 8*4;
      //Two 4 instruction sequences, 3 to save/restore CR, plus
      // the instruction to save/restore the stack 
#else
      else
         return theRelocatableCode->chunkNumBytes(0);
#endif
   }

   void relocate_and_emit(directToMemoryEmitter_t &em,
                          bool usePreInsnVersion,
                          bool useSaveRestoreVersion) const;
      // em.getCurrentEmitAddr() lets us know where to relocate to.
};

#endif
