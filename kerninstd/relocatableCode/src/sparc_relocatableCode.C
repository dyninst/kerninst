#include "sparc_relocatableCode.h"
#include "sparc_instr.h"
#include "util/h/hashFns.h"
#include "xdr_send_recv_common.h"

extern bool P_xdr_recv(XDR *xdr, relocatableCode_t::insnOffset &io);
extern bool P_xdr_send(XDR *xdr, const relocatableCode_t::insnOffset &io);

// Check that instructions at the given offset correspond to the
// expected call sequence, replace dummy addresses with the valid ones
// in-place
void sparc_relocatableCode::resolve1Call(const insnOffset &offs,
				   pdvector<insnVec_t*> &chunks,
				   kptr_t startAddr,
				   kptr_t calleeAddr,
                                   kptr_t calleeAddrAtEmitTime
                                      // used for asserting only
                                   ) 
{
   insnVec_t *oldv=insnVec_t::getInsnVec(), *newv=insnVec_t::getInsnVec();
   
   // generic version, with a known toAddr but unknown fromAddr
   instr_t::genCallAndLinkGeneric_unknownFromAddr(oldv, 
						  calleeAddrAtEmitTime);

   // new version. We have to be careful to compute the address of
   // the call instruction correctly, since we may prepend a few nops
   // at the beginning. What helps is that "call/jmpl" is always the
   // last instruction in the sequence.
   instr_t::genCallAndLink(newv, startAddr + (oldv->numInsns() - 1) * 4, 
			   calleeAddr);

   insnVec_t::const_iterator old_iter = oldv->begin();
   insnVec_t::const_iterator new_iter = newv->begin();

   insnOffset offset = offs;

   // The optimized version may be shorter than the generic. To preserve the
   // offsets in the rest of the code we need to prepend a few nops to the
   // optimized code. Notice "prepend", since the call instruction has a
   // delay slot which would not be properly executed if we "appended" nops
   int numNops = oldv->numInsns() - newv->numInsns();
   assert(numNops >= 0);
   // Verify that we see the generic version emitted
   // Overwrite part of it with nops
   for (; old_iter != oldv->end() && numNops > 0;
	old_iter++, offset.byteOffset += 4, numNops--) {
      instr_t *current = offset2Insn(offset, chunks);
      assert(current->getRaw() == (*old_iter)->getRaw());
      *current = sparc_instr(sparc_instr::nop);
   }

   // Verify that we see the generic version emitted
   // Replace it with the optimized version
   for (; old_iter != oldv->end() && new_iter != newv->end();
	  old_iter++, new_iter++, offset.byteOffset += 4) {
      instr_t *current = offset2Insn(offset, chunks);
      assert(current->getRaw() == (*old_iter)->getRaw());
      *current = **new_iter;
   }
   delete oldv;
   delete newv;
}

sparc_relocatableCode::sparc_relocatableCode(const tempBufferEmitter &em) :
   relocatableCode_t(em), 
   unresolvedSetHiBSetOrAddBackpatches( ((sparc_tempBufferEmitter &)
   	em).getUnresolvedSetHiBSetOrAddBackpatches()) {
   assert(em.isCompleted());
}

sparc_relocatableCode::sparc_relocatableCode(const relocatableCode_t &src) :
   relocatableCode_t(src),
   unresolvedSetHiBSetOrAddBackpatches
   ( ((sparc_relocatableCode &)src).unresolvedSetHiBSetOrAddBackpatches)
{
}

sparc_relocatableCode::sparc_relocatableCode(relocatableCode_t::Dummy) :
   relocatableCode_t(_dummy)
{
   // everything an empty vector.  0 chunks.
}


static bool P_xdr_send(XDR *xdr,
                       const sparc_tempBufferEmitter::setHiBSetOrAddLocation &loc) {
   return (P_xdr_send(xdr, loc.setHiLabelName) &&
           P_xdr_send(xdr, loc.setHiInsnByteOffset) &&
           P_xdr_send(xdr, loc.bsetOrAddLabelName) &&
           P_xdr_send(xdr, loc.bsetOrAddInsnByteOffset));
}

static bool P_xdr_recv(XDR *xdr, sparc_tempBufferEmitter::setHiBSetOrAddLocation &loc) {
   try {
      new((void*)&loc)sparc_tempBufferEmitter::setHiBSetOrAddLocation(read1_string(xdr),
                                                                read1_uint32(xdr),
                                                                read1_string(xdr),
                                                                read1_uint32(xdr));
      return true;
   }
   catch (xdr_recv_fail) {
      return false;
   }
}

static bool P_xdr_recv(XDR *xdr, sparc_tempBufferEmitter::setHiBSetBackpatchInfo &info) {
   if (!P_xdr_recv(xdr, info.insnLocs))
      return false;
   if (!P_xdr_recv(xdr, info.labelName))
      return false;
   if (!P_xdr_recv(xdr, info.byteOffset))
      return false;
   return true;
}
static bool P_xdr_send(XDR *xdr,
                       const sparc_tempBufferEmitter::setHiBSetBackpatchInfo &info) {
   return (P_xdr_send(xdr, info.insnLocs) &&
           P_xdr_send(xdr, info.labelName) &&
           P_xdr_send(xdr, info.byteOffset));
}


// The following is needed when compiling with egcs 1.1.2:
template <class T>
static void destruct1(T &vrble) {
   vrble.~T();
}

sparc_relocatableCode::sparc_relocatableCode(XDR *xdr) :
   relocatableCode_t(_dummy)
{
   // get theChunks
   uint32_t nelems;
   if (!P_xdr_recv(xdr, nelems))
      throw xdr_recv_fail();

   // assert that the default ctor did exactly what we expected it to do:
   assert(theChunks.size() == 0);
   assert(theChunks.capacity() == 0);
   assert(theChunks.begin() == NULL);

   if (nelems != 0) {
      // don't call reserve_for_inplace_construction(zero)

      // No need to be too clever with reserve_for_inplace_construction() here;
      // since the vector is of pointers, element copying will be cheap, etc.
      pdvector<insnVec_t*>::iterator iter = 
	 theChunks.reserve_for_inplace_construction(nelems);
      pdvector<insnVec_t*>::iterator finish = theChunks.end();

      // OK for "do" instead of "while"; the check for nelems==0 above ensures 
      // that nelems > 0 here.
      do {
	 insnVec_t *newitem = insnVec_t::getInsnVec(xdr);
	 assert(newitem);
      
	 *iter = newitem;
      } while (++iter != finish);
   }

   // explicitly destruct unresolvedCallsByName, unresolvedCallsByAddr,
   // unresolvedSymAddrs since
   // P_xdr_recv() wants to operate on raw memory.

   // Note that we receive a vector, and turn it into a dictionary, because xdr
   // isn't supported for dictionaries.
   pdvector< std::pair<pdstring, insnOffset> > ilabels;
   destruct1(ilabels);
   if (!P_xdr_recv(xdr, ilabels))
      throw xdr_recv_fail();
   fillLabels(ilabels);
   
   // Note that we receive a vector, and turn it into a dictionary, because xdr
   // isn't supported for dictionaries.
   pdvector< std::pair<pdstring, kptr_t> > iknownSymAddrs;
   destruct1(iknownSymAddrs); // make P_xdr_recv() happy by giving it raw bits
   if (!P_xdr_recv(xdr, iknownSymAddrs))
      throw xdr_recv_fail();
   fillKnownSymAddrs(iknownSymAddrs);

   destruct1(unresolvedBranchesToLabels);
   if (!P_xdr_recv(xdr, unresolvedBranchesToLabels))
      throw xdr_recv_fail();
   
   destruct1(unresolvedInterprocBranchesToName);
   if (!P_xdr_recv(xdr, unresolvedInterprocBranchesToName))
      throw xdr_recv_fail();
   
   destruct1(unresolvedInterprocBranchesToAddr);
   if (!P_xdr_recv(xdr, unresolvedInterprocBranchesToAddr))
      throw xdr_recv_fail();
   
   destruct1(unresolvedCallsByName);
   if (!P_xdr_recv(xdr, unresolvedCallsByName))
      throw xdr_recv_fail();

   destruct1(unresolvedCallsByAddr);
   if (!P_xdr_recv(xdr, unresolvedCallsByAddr))
      throw xdr_recv_fail();

   destruct1(unresolvedSymAddrs);
   if (!P_xdr_recv(xdr, unresolvedSymAddrs))
      throw xdr_recv_fail();

   destruct1(unresolvedLabelAddrsAsData);
   if (!P_xdr_recv(xdr, unresolvedLabelAddrsAsData))
      throw xdr_recv_fail();
   
   destruct1(unresolvedLabelAddrMinusLabelAddrThenDivides);
   if (!P_xdr_recv(xdr, unresolvedLabelAddrMinusLabelAddrThenDivides))
      throw xdr_recv_fail();

   destruct1(unresolvedLabelAddrs);
   if (!P_xdr_recv(xdr, unresolvedLabelAddrs))
      throw xdr_recv_fail();

   destruct1(unresolvedSetHiBSetOrAddBackpatches);
   if (!P_xdr_recv(xdr, unresolvedSetHiBSetOrAddBackpatches))
      throw xdr_recv_fail();

   // get fullyResolvedCodeCache
   if (!P_xdr_recv(xdr, nelems))
      throw xdr_recv_fail();

   // assert that the default ctor did exactly what we expected it to do:
   assert(fullyResolvedCodeCache.size() == 0);
   assert(fullyResolvedCodeCache.capacity() == 0);
   assert(fullyResolvedCodeCache.begin() == NULL);

   if (nelems != 0) {
      // don't call reserve_for_inplace_construction(zero)

      // No need to be too clever with reserve_for_inplace_construction() here;
      // since the vector is of pointers, element copying will be cheap, etc.
      pdvector<insnVec_t*>::iterator iter = 
	 fullyResolvedCodeCache.reserve_for_inplace_construction(nelems);
      pdvector<insnVec_t*>::iterator finish = 
	 fullyResolvedCodeCache.end();

      // OK for "do" instead of "while"; the check for nelems==0 above ensures 
      // that nelems > 0 here.
      do {
	 insnVec_t *newitem = insnVec_t::getInsnVec(xdr);
	 assert(newitem);
      
	 *iter = newitem;
      } while (++iter != finish);
   }
}

sparc_relocatableCode::~sparc_relocatableCode() {}

bool sparc_relocatableCode::send(XDR *xdr) const {
   if (!P_xdr_send_pointers(xdr, theChunks))
      return false;

   // Note that we send a vector, since xdr isn't supported for dictionaries.
   // Of course, receiving knows this too.
   const pdvector< std::pair<pdstring, insnOffset> > xlabels = labels.keysAndValues();
   if (!P_xdr_send(xdr, xlabels))
      return false;

   // Note that we send a vector, since xdr isn't supported for dictionaries.
   // Of course, receiving knows this too.
   const pdvector< std::pair<pdstring, kptr_t> > xknownSymAddrs = knownSymAddrs.keysAndValues();
   if (!P_xdr_send(xdr, xknownSymAddrs))
      return false;
   
   if (!P_xdr_send(xdr, unresolvedBranchesToLabels))
      return false;
   
   if (!P_xdr_send(xdr, unresolvedInterprocBranchesToName))
      return false;
   
   if (!P_xdr_send(xdr, unresolvedInterprocBranchesToAddr))
      return false;
   
   if (!P_xdr_send(xdr, unresolvedCallsByName))
      return false;
   
   if (!P_xdr_send(xdr, unresolvedCallsByAddr))
      return false;
   
   if (!P_xdr_send(xdr, unresolvedSymAddrs))
      return false;

   if (!P_xdr_send(xdr, unresolvedLabelAddrsAsData))
      return false;

   if (!P_xdr_send(xdr, unresolvedLabelAddrMinusLabelAddrThenDivides))
      return false;

   if (!P_xdr_send(xdr, unresolvedLabelAddrs))
      return false;

   if (!P_xdr_send(xdr, unresolvedSetHiBSetOrAddBackpatches))
      return false;

   if (!P_xdr_send_pointers(xdr, fullyResolvedCodeCache))
      return false;

   return true;
}

// Check that instructions at the given offset correspond to the
// expected immediate address generation, replace dummy addresses with 
// the valid ones in-place
void sparc_relocatableCode::resolve1SetImmAddr(const insnOffset &offs, 
					 pdvector<insnVec_t *> &chunks,
					 kptr_t addr)
{
   const kptr_t zero = 0;
   instr_t *first = offset2Insn(offs, chunks);
   const sparc_reg rd = sparc_reg(sparc_reg::rawIntReg,
				  getRdBits(first->getRaw()));
   insnVec_t *oldv=insnVec_t::getInsnVec(), *newv=insnVec_t::getInsnVec();

   sparc_instr::genImmAddrGeneric(oldv, zero, rd);
   sparc_instr::genImmAddr(newv, addr, rd);
   assert(newv->numInsns() <= oldv->numInsns()); 

   insnVec_t::const_iterator old_iter = oldv->begin();
   insnVec_t::const_iterator new_iter = newv->begin();

   insnOffset offset = offs;

   // Verify that we see the generic version emitted
   // Replace it with the resolved, optimized version
   for (; old_iter != oldv->end() && new_iter != newv->end();
	  old_iter++, new_iter++, offset.byteOffset += 4) {
      instr_t *current = offset2Insn(offset, chunks);
      assert(current->getRaw() == (*old_iter)->getRaw());
      *current = **new_iter;
   }
   // Pad with nop's, if necessary
   for (; old_iter != oldv->end(); 
	  old_iter++, offset.byteOffset += 4) {
      instr_t *current = offset2Insn(offset, chunks);
      assert(current->getRaw() == (*old_iter)->getRaw());
      *current = sparc_instr(sparc_instr::nop);
   }
   delete oldv;
   delete newv;
}
