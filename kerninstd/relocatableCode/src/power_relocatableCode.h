// power_relocatableCode.h

#ifndef _POWER_RELOCATABLE_CODE_H_
#define _POWER_RELOCATABLE_CODE_H_


#include "power_tempBufferEmitter.h"
#include "tempBufferEmitter.h"
#include "relocatableCode.h"

class XDR;
class power_instr;
class power_reg;

class power_relocatableCode : public relocatableCode_t {
 public:
   
   explicit power_relocatableCode(const tempBufferEmitter &em);
   power_relocatableCode(Dummy);
   power_relocatableCode(XDR *);
   power_relocatableCode(const relocatableCode_t &src); // yes, we really do need this (nuts)
   ~power_relocatableCode();

   bool send(XDR *) const;

   typedef tempBufferEmitter::insnOffset insnOffset;

 // NOTE: it's nice that the resolver abilities are in a template.
   // Perhaps we should ALSO template how we "emit"; that is: returning a copy
   // of the emitted code isn't always as efficient as it could be; why not
   // have the ability for this method to emit directly to a directToKernelEmitter,
   // when desired?  Perhaps an "emitterOracle?"
   template <class resolverOracle>
   const pdvector<insnVec_t*> &
   obtainFullyResolvedCode(const pdvector<kptr_t> &chunkStartAddrs,
			   const resolverOracle &theResolverOracle) const ;
      // theResolverOracle must have certain methods, which this method will invoke:
      // (1) std::pair<bool, kptr_t> obtainFunctionAddr(const pdstring &fnName)
      // (2) std::pair<bool, kptr_t> obtainImmAddr(const pdstring &objectName)
      // presumably, these methods will search some kind of runtime symbol table for the
      // given symbol names, and return their address.
      // Internally: updates code cache & returns a reference to it.

public:
   // Check that instructions at the given offset correspond to the
   // expected call sequence, replace dummy addresses with the valid ones
   // in-place
   static void resolve1Call(const insnOffset &offs,
			    pdvector<insnVec_t*> &chunks,
			    kptr_t insnAddr,
			    kptr_t calleeAddr,
                            kptr_t calleeAddrAtEmitTime
                               // only used for asserting
                            );
   // Check that instructions at the given offset correspond to the
   // expected immediate address generation, replace dummy addresses with 
   // the valid ones in-place
   static void resolve1SetImmAddr(const insnOffset &offs, 
				  pdvector<insnVec_t*> &chunks,
				  kptr_t symAddr);

};

template <class resolverOracle>
const pdvector<insnVec_t*> &
power_relocatableCode::
obtainFullyResolvedCode(const pdvector<kptr_t> &chunkStartAddrs, // an entry per chunk
                        const resolverOracle &theResolverOracle) const  {
   // theResolverOracle must have certain methods, which this method will invoke:
   // (1) kptr_t obtainFunctionAddr(const pdstring &fnName)
   // (2) kptr_t obtainImmAddr(const pdstring &objectName)
   // presumably, these methods will search some kind of runtime symbol table for the
   // given symbol names, and return their address.

   //fullyResolvedCodeCache = theChunks; // fixes a subtle bug reported by Ari

   assert(chunkStartAddrs.size() == theChunks.size());
   for (pdvector<kptr_t>::const_iterator iter = chunkStartAddrs.begin();
        iter != chunkStartAddrs.end(); ++iter) {
      const kptr_t chunkAddr = *iter;
      assert(instr_t::instructionHasValidAlignment(chunkAddr));
   }

   // assert that "fullyResolvedCodeCache" is the same size as "theChunks"
   assert(fullyResolvedCodeCache.size() == theChunks.size());
   for (unsigned ndx = 0; ndx < fullyResolvedCodeCache.size(); ++ndx) {
      assert(fullyResolvedCodeCache[ndx]->numInsnBytes() ==
             theChunks[ndx]->numInsnBytes());
   }
   
   // STEP 1: resolved branches to labels
   {
      pdvector< std::pair<insnOffset, pdstring> >::const_iterator iter =
         unresolvedBranchesToLabels.begin();
      pdvector< std::pair<insnOffset, pdstring> >::const_iterator finish =
         unresolvedBranchesToLabels.end();
      for (; iter != finish; ++iter) {
         const insnOffset &branchinsn_off = iter->first;
         const pdstring &labelName = iter->second;
         
         const insnOffset &label_off = labels.get(labelName);

         // calculate the delta, a sort of "label_off minus branchinsn_off"
         const kaddrdiff_t delta = distance_calc(label_off, // to
						 branchinsn_off, // from
						 chunkStartAddrs);

         instr_t *i = offset2Insn(branchinsn_off, fullyResolvedCodeCache);
         i->changeBranchOffset(delta);
      }
   }
   // We don't clear unresolvedBranchesToLabels[] because this method may get called
   // again, with a different emitAddr!

   // STEP 1.5: fix up unresolved *interprocedural* branches by name
   {
      pdvector< std::pair<insnOffset, pdstring> >::const_iterator iter =
         unresolvedInterprocBranchesToName.begin();
      pdvector< std::pair<insnOffset, pdstring> >::const_iterator finish =
         unresolvedInterprocBranchesToName.end();
      for (; iter != finish; ++iter) {
         const insnOffset &branchinsn_off = iter->first;
         const pdstring &symName = iter->second; // NOT a label name

         std::pair<bool, kptr_t> result = 
            theResolverOracle.obtainFunctionEntryAddr(symName);

         kptr_t destAbsoluteAddr = 0;
         if (result.first)
            destAbsoluteAddr = result.second;
         else if (knownSymAddrs.find(symName, destAbsoluteAddr))
            ;
         else
            assert(false && "could not resolve interprocedural branch by name");
            
         const kptr_t thisInsnAbsoluteAddr =
            absolute_addr(branchinsn_off, chunkStartAddrs);
         const kaddrdiff_t offset = destAbsoluteAddr - thisInsnAbsoluteAddr;
            // answer will be correct even if negative, right?  (No unsigned underflow
            // worries, right?)

         instr_t *i = offset2Insn(branchinsn_off, fullyResolvedCodeCache);
         i->changeBranchOffset(offset); // will assert that the offset fits!
      }
   }
   // We don't clear unresolvedInterprocBranchesToName[] because this method may get
   // called again, with a different emitAddr!

   // STEP 1.75: fix up unresolved *interprocedural* branches to addr
   {
      pdvector< std::pair<insnOffset, kptr_t> >::const_iterator iter =
         unresolvedInterprocBranchesToAddr.begin();
      pdvector< std::pair<insnOffset, kptr_t> >::const_iterator finish =
         unresolvedInterprocBranchesToAddr.end();
      for (; iter != finish; ++iter) {
         const insnOffset &branchinsn_off = iter->first;
         const kptr_t thisInsnAbsoluteAddr =
            absolute_addr(branchinsn_off, chunkStartAddrs);
         const kptr_t destAbsoluteAddr = iter->second;
         const kaddrdiff_t offset = destAbsoluteAddr - thisInsnAbsoluteAddr;
            // answer will be correct even if negative, right?  (No unsigned underflow
            // worries, right?)

         instr_t *i = offset2Insn(branchinsn_off, fullyResolvedCodeCache);
         i->changeBranchOffset(offset); // will assert that the offset fits!
      }
   }
   // We don't clear unresolvedInterprocBranchesToAddr[] because this method may get
   // called again, with a different emitAddr!

   // STEP 2: fix up unresolved calls-by-name.
   {
      pdvector< std::pair<insnOffset, pdstring> >::const_iterator iter =
         unresolvedCallsByName.begin();
      pdvector< std::pair<insnOffset, pdstring> >::const_iterator finish =
         unresolvedCallsByName.end();

      for (; iter != finish; ++iter) {
         // First check theResolverOracle
         const insnOffset &theInsnOffset = iter->first;
         const pdstring &theSymName = iter->second;

         const kptr_t insnAbsoluteAddr =
            absolute_addr(theInsnOffset, chunkStartAddrs);

         // We check for calls in knownSymAddrs[], in labels[], and in
         // theResolverOracle.  The oracle should be checked last, since
         // it's slow if it doesn't find anything.
         // XXX TODO: does theResolverOracle already check for it within
         // knownSymAddrs[]?  If so, then we may have some unnecessary code
         // duplication.

         if (knownSymAddrs.defines(theSymName)) {
            resolve1Call(theInsnOffset, fullyResolvedCodeCache,
                         insnAbsoluteAddr, // from
                         knownSymAddrs.get(theSymName), // callee addr
                         0 // old value of "to" (at emit time); used for asserting only
                         );
            continue;
         }

         // Next check for it as a labelAddr (rare to make a call to a label but
         // not unheard of.  Happens for recursion, though.)
         if (labels.defines(theSymName)) {
            const insnOffset &calleeOffset = labels.get(theSymName);
            const kptr_t calleeAbsoluteAddr =
               absolute_addr(calleeOffset, chunkStartAddrs);
            
            resolve1Call(theInsnOffset, fullyResolvedCodeCache, 
			 insnAbsoluteAddr, calleeAbsoluteAddr,
                         0 // old value of "to" (at emit time); used for asserting only
                         );

            continue;
         }

         // And finally, try the oracle (try this last since it's really
         // slow if it can't find anything).
         std::pair<bool, kptr_t> addr =
            theResolverOracle.obtainFunctionEntryAddr(theSymName);

         if (!addr.first) {
            cout << "Could not resolve call symbol named \""
                 << theSymName << "\"" << endl;
            assert(false); // throw an exception instead
         }
         else {
            resolve1Call(theInsnOffset, fullyResolvedCodeCache,
                         insnAbsoluteAddr, // from
                         addr.second, // to
                         0 // old to-addr (at emit time); just used for asserting
                         );
            continue;
         }

         assert(false);
      }
   }
   // We don't clear unresolvedCallsByName[] because this method may get called
   // again, with a different emitAddr!

   // STEP 3: fix up unresolved calls-by-addr
   {
      pdvector< std::pair<insnOffset,kptr_t> >::const_iterator iter =
         unresolvedCallsByAddr.begin();
      pdvector< std::pair<insnOffset,kptr_t> >::const_iterator finish =
         unresolvedCallsByAddr.end();

      for (; iter != finish; ++iter) {
         const kptr_t calleeAddr = iter->second;

         const insnOffset &theInsnOffset = iter->first;
         const kptr_t insnAbsoluteAddr = absolute_addr(theInsnOffset, chunkStartAddrs);

         const instr_t *callInsn = offset2Insn(theInsnOffset, fullyResolvedCodeCache);

	 // Now that we allocate all patches in the 32-bit space, 
	 // this assert becomes valid again
         kaddrdiff_t orig_offset = -1;
         assert(callInsn->isCallInstr(orig_offset));

         //assert(orig_offset == (int)calleeAddr);
            // sorry, can't assert this due to re-use of fullyResolvedCodeCache

         resolve1Call(theInsnOffset, fullyResolvedCodeCache,
                      insnAbsoluteAddr, // from
                      calleeAddr, // to
                      calleeAddr // to-addr at emit time (same); used for asserting
                      );
      }
   }
   // We don't clear unresolvedCallsByAddr[] because this method may get called
   // again, with a different emitAddr!

   // STEP 4 -- fix up unresolvedSymAddrs:
   {
      pdvector< std::pair<insnOffset,pdstring> >::const_iterator iter =
         unresolvedSymAddrs.begin();
      pdvector< std::pair<insnOffset,pdstring> >::const_iterator finish =
         unresolvedSymAddrs.end();
      for (; iter != finish; ++iter) {
         kptr_t addr;

         const insnOffset &theInsnOffset = iter->first;
         const pdstring &theSymName = iter->second;
         
         // If in knownSymAddrs[], then assert fail (tempBufferEmitter was being lazy?
         // Should have been resolved when tempBufferEmitter called its complete()!)
         assert(!knownSymAddrs.defines(theSymName));
      
         if (iter->second == tempBufferEmitter::this_chunk_addr)
            addr = chunkStartAddrs[theInsnOffset.chunkNdx];
         else {
            std::pair<bool, kptr_t> result = theResolverOracle.obtainImmAddr(theSymName);
      
            if (!result.first) {
               cout << "Could not resolve symAddr symbol named \""
                    << theSymName << "\"" << endl;
               assert(false); // throw an exception instead
               abort(); // placate compiler viz. "addr" possibly being undefined
            }
            else
               addr = result.second;
         }
         resolve1SetImmAddr(theInsnOffset, fullyResolvedCodeCache, addr);
      }
   }
   // We *don't* clear unresolvedSymAddrs[] because this method may get called
   // again, with a different emitAddr!

   // STEP 4.5: unresolvedLabelAddrsAsData
   {
      pdvector< std::pair<insnOffset, pdstring> >::const_iterator iter =
         unresolvedLabelAddrsAsData.begin();
      pdvector< std::pair<insnOffset, pdstring> >::const_iterator finish =
         unresolvedLabelAddrsAsData.end();
      for (; iter != finish; ++iter) {
         const std::pair<insnOffset, pdstring> &info = *iter;
         
         const insnOffset &theInsnOffset = info.first;
         const insnOffset &labelOffset = labels.get(info.second);
         const kptr_t labelAbsoluteAddr = absolute_addr(labelOffset, chunkStartAddrs);

         kptr_t valueToUse = (kptr_t)-1; // for now...
         
         // assert that labelAbsoluteAddr is 32 bits
         if (sizeof(labelAbsoluteAddr) == 4)
            valueToUse = labelAbsoluteAddr;
         else if (sizeof(labelAbsoluteAddr) == 8) {
            valueToUse = labelAbsoluteAddr & (kptr_t)0xffffffff;
            assert(labelAbsoluteAddr == (kptr_t)valueToUse);
         }
         else
            assert(false);
         
         power_instr *i = (power_instr *) offset2Insn(theInsnOffset, fullyResolvedCodeCache);
         *i = power_instr(valueToUse);
      }
   }
   // We *don't* clear unresolvedSymAddrs[] because this method may get called
   // again, with a different emitAddr!

   // STEP 5: unresolvedLabelAddrMinusLabelAddrThenDivides
   {
      pdvector< std::pair<insnOffset, triple<pdstring, pdstring, unsigned> > >::const_iterator iter = unresolvedLabelAddrMinusLabelAddrThenDivides.begin();
      pdvector< std::pair<insnOffset, triple<pdstring, pdstring, unsigned> > >::const_iterator finish = unresolvedLabelAddrMinusLabelAddrThenDivides.end();
      for (; iter != finish; ++iter) {
         const std::pair<insnOffset, triple<pdstring,pdstring,unsigned> > &info = *iter;
         
         const insnOffset &theInsnOffset = info.first;
         const insnOffset &label1Offset = labels.get(info.second.first);
         const insnOffset &label2Offset = labels.get(info.second.second);
         
         const kaddrdiff_t delta = distance_calc(label1Offset, // to start with
						 label2Offset, // subtract this
						 chunkStartAddrs);

         assert(delta > 0 && "expected positive");
         const unsigned result = delta / info.second.third;

         power_instr *i = (power_instr *)offset2Insn(theInsnOffset, fullyResolvedCodeCache);
         *i = power_instr(result);
      }
   }
   // We *don't* clear unresolvedSymAddrs[] because this method may get called
   // again, with a different emitAddr!
   
   // STEP 6: Loop thru unresolvedLabelAddrs:
   {
      pdvector< std::pair<insnOffset, pdstring> >::const_iterator iter =
         unresolvedLabelAddrs.begin();
      pdvector< std::pair<insnOffset, pdstring> >::const_iterator finish =
         unresolvedLabelAddrs.end();
      for (; iter != finish; ++iter) {
         const std::pair<insnOffset, pdstring> &info = *iter;
      
         const insnOffset &theInsnOffset = info.first;
         const insnOffset &labelOffset = labels.get(info.second);
         const kptr_t labelAbsoluteAddr = absolute_addr(labelOffset, 
							chunkStartAddrs);
      
         resolve1SetImmAddr(theInsnOffset, fullyResolvedCodeCache, 
			    labelAbsoluteAddr);
      }
   }
   // We don't clear unresolvedLabelAddrs[] because this method may get called
   // again, with a different emitAddr!

   
   return fullyResolvedCodeCache;
}

#endif
