#include "x86_relocatableCode.h"
#include "x86_instr.h"
#include "util/h/hashFns.h"
#include "xdr_send_recv_common.h"

extern bool P_xdr_recv(XDR *xdr, relocatableCode_t::insnOffset &io);
extern bool P_xdr_send(XDR *xdr, const relocatableCode_t::insnOffset &io);

// Check that instructions at the given offset correspond to the
// expected call sequence, replace dummy addresses with the valid ones
// in-place
void x86_relocatableCode::resolve1Call(const insnOffset &offs,
				       pdvector<insnVec_t*> &chunks,
				       kptr_t startAddr,
				       kptr_t calleeAddr,
				       kptr_t calleeAddrAtEmitTime
				          // used for asserting only
				       ) 
{
   // change relative offset of call using new absolute addr of call insn 
   // and callee
   insnOffset offset = offs;
   x86_instr *current = (x86_instr*)offset2Insn(offset, chunks);
   int calleeOffset = calleeAddr - (startAddr + current->getNumBytes());
   current->changeBranchOffset(calleeOffset);
}

x86_relocatableCode::x86_relocatableCode(const tempBufferEmitter &em) :
   relocatableCode_t(em)
{
   assert(em.isCompleted());
}

x86_relocatableCode::x86_relocatableCode(const relocatableCode_t &src) :
   relocatableCode_t(src)
{}

x86_relocatableCode::x86_relocatableCode(relocatableCode_t::Dummy) :
   relocatableCode_t(_dummy)
{
   // everything an empty vector.  0 chunks.
}

// The following is needed when compiling with egcs 1.1.2:
template <class T>
static void destruct1(T &vrble) {
   vrble.~T();
}

x86_relocatableCode::x86_relocatableCode(XDR *xdr) :
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

x86_relocatableCode::~x86_relocatableCode() {}

bool x86_relocatableCode::send(XDR *xdr) const {
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

   if (!P_xdr_send_pointers(xdr, fullyResolvedCodeCache))
      return false;

   return true;
}

// Replace dummy address in instruction at the given offset with 
// the valid address in-place
void x86_relocatableCode::resolve1SetImmAddr(const insnOffset &offs, 
					     pdvector<insnVec_t *> &chunks,
					     kptr_t addr)
{
   x86_instr *i = (x86_instr*)offset2Insn(offs, chunks);
   if(i->getNumBytes() != 5)
      assert(!"x86_relocatableCode::resolve1SetImmAddr() - expected 5-byte insn\n");
   const char *insn_bytes = i->getRawBytes();
   uint32_t *imm32 = const_cast<uint32_t*>((const uint32_t*)(insn_bytes+1));
   if(*imm32 != (uint32_t)0)
      assert(!"x86_relocatableCode::resolve1SetImmAddr() - expected zero imm val\n");
   *imm32 = (uint32_t)addr;
}
