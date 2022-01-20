// relocatableCode.C
// Ariel Tamches
// see the .h file for comments

#include "relocatableCode.h"
#include "util/h/hashFns.h"
#include "util/h/xdr_send_recv.h"

#ifdef sparc_sun_solaris2_7
#include "sparc_relocatableCode.h"
#elif defined(i386_unknown_linux2_4)
#include "x86_relocatableCode.h"
#elif defined(rs6000_ibm_aix5_1)
//#include "power_relocatableCode.h"
#endif


// static member definition:
relocatableCode_t::Dummy relocatableCode_t::_dummy;

relocatableCode_t* relocatableCode_t::getRelocatableCode(XDR *xdr)
{
#ifdef sparc_sun_solaris2_7
   return (relocatableCode_t*)new sparc_relocatableCode(xdr);
#elif defined(i386_unknown_linux2_4)
   return (relocatableCode_t*)new x86_relocatableCode(xdr);
#elif defined(rs6000_ibm_aix5_1)
   return (relocatableCode_t*)new power_relocatableCode(xdr);
#endif
}

void relocatableCode_t::putRelocatableCode(XDR *xdr, relocatableCode_t &rc)
{
#ifdef sparc_sun_solaris2_7
   (void)new(&rc) sparc_relocatableCode(xdr);
#elif defined(i386_unknown_linux2_4)
   (void)new(&rc) x86_relocatableCode(xdr);
#elif defined(rs6000_ibm_aix5_1)
   (void)new(&rc) power_relocatableCode(xdr);
#endif
}

relocatableCode_t::relocatableCode_t(Dummy) :
   labels(stringHash),
   knownSymAddrs(stringHash)
{
   // everything an empty vector.  0 chunks.
}

void relocatableCode_t::
fillKnownSymAddrs(const pdvector< std::pair<pdstring, kptr_t> > &iknownSymAddrs) {
   pdvector< std::pair<pdstring, kptr_t> >::const_iterator iter = iknownSymAddrs.begin();
   pdvector< std::pair<pdstring, kptr_t> >::const_iterator finish = iknownSymAddrs.end();
   for (; iter != finish; ++iter) {
      const pdstring &symName = iter->first;
      const kptr_t symAddr = iter->second;
      
      knownSymAddrs.set(symName, symAddr); // no duplicates allowed
   }
   assert(knownSymAddrs.size() == iknownSymAddrs.size());
}

void relocatableCode_t::
fillLabels(const pdvector< std::pair<pdstring, insnOffset> > &ilabels) {
   pdvector< std::pair<pdstring, insnOffset> >::const_iterator iter = ilabels.begin();
   pdvector< std::pair<pdstring, insnOffset> >::const_iterator finish = ilabels.end();
   for (; iter != finish; ++iter) {
      const pdstring &symName = iter->first;
      const insnOffset &offset = iter->second;
      
      labels.set(symName, offset); // no duplicates allowed
   }

   assert(labels.size() == ilabels.size());
}

relocatableCode_t::relocatableCode_t(const tempBufferEmitter &em) :
   theChunks(em.getEmitted(false)), // false --> needn't be fully resolved yet
   labels(em.getLabels()),
   knownSymAddrs(em.getKnownSymAddrs()), // copying entire dictionary
   unresolvedBranchesToLabels(em.getUnresolvedBranchesToLabels()),
   unresolvedInterprocBranchesToName(em.getUnresolvedInterprocBranchesToName()),
   unresolvedInterprocBranchesToAddr(em.getUnresolvedInterprocBranchesToAddr()),
   unresolvedCallsByName(em.getUnresolvedCallsByName()),
   unresolvedCallsByAddr(em.getUnresolvedCallsByAddr()),
   unresolvedSymAddrs(em.getUnresolvedSymAddrs()),
   unresolvedLabelAddrsAsData(em.getUnresolvedLabelAddrsAsData()),
   unresolvedLabelAddrMinusLabelAddrThenDivides(em.getUnresolvedLabelAddrMinusLabelAddrThenDivides()),
   unresolvedLabelAddrs(em.getUnresolvedLabelAddrs()),
 
   fullyResolvedCodeCache(theChunks) // initialized to the proper size
{
   assert(em.isCompleted());
   // we have made a shallow pointer-only copy of each element; make it a deep copy now
   pdvector<insnVec_t*>::iterator chunks_iter = theChunks.begin();
   pdvector<insnVec_t*>::iterator chunks_finish = theChunks.end();
   pdvector<insnVec_t*>::iterator code_iter = fullyResolvedCodeCache.begin();
   for (; chunks_iter != chunks_finish; ++chunks_iter, ++code_iter) {
      const insnVec_t *iv = *chunks_iter;
      *chunks_iter = insnVec_t::getInsnVec(*iv);
      *code_iter = insnVec_t::getInsnVec(*iv);
   }	          
}

relocatableCode_t::relocatableCode_t(const relocatableCode_t &src) :
   theChunks(src.theChunks),
   labels(src.labels), // ugh, copying an entire dictionary
   knownSymAddrs(src.knownSymAddrs), // ugh, copying an entire dictionary
   unresolvedBranchesToLabels(src.unresolvedBranchesToLabels),
   unresolvedInterprocBranchesToName(src.unresolvedInterprocBranchesToName),
   unresolvedInterprocBranchesToAddr(src.unresolvedInterprocBranchesToAddr),
   unresolvedCallsByName(src.unresolvedCallsByName),
   unresolvedCallsByAddr(src.unresolvedCallsByAddr),

   unresolvedSymAddrs(src.unresolvedSymAddrs),
   unresolvedLabelAddrsAsData(src.unresolvedLabelAddrsAsData),

   unresolvedLabelAddrMinusLabelAddrThenDivides(src.unresolvedLabelAddrMinusLabelAddrThenDivides),
   unresolvedLabelAddrs(src.unresolvedLabelAddrs),
   fullyResolvedCodeCache(src.fullyResolvedCodeCache)
{
   // we have made a shallow pointer-only copy of each element; make it a deep copy now
   pdvector<insnVec_t*>::iterator chunks_iter = theChunks.begin();
   pdvector<insnVec_t*>::iterator chunks_finish = theChunks.end();
   for (; chunks_iter != chunks_finish; ++chunks_iter) {
      const insnVec_t *iv = *chunks_iter;
      *chunks_iter = insnVec_t::getInsnVec(*iv);
   }	          
   
   pdvector<insnVec_t*>::iterator code_iter = fullyResolvedCodeCache.begin();
   pdvector<insnVec_t*>::iterator code_finish = fullyResolvedCodeCache.end();
   for (; code_iter != code_finish; ++code_iter) {
      const insnVec_t *iv = *code_iter;
      *code_iter = insnVec_t::getInsnVec(*iv);
   }
}

relocatableCode_t*
relocatableCode_t::getRelocatableCode(const relocatableCode_t &src) {
#ifdef sparc_sun_solaris2_7
   return (relocatableCode_t*) 
      new sparc_relocatableCode((sparc_relocatableCode&)src);
#elif defined(i386_unknown_linux2_4)
   return (relocatableCode_t*)new x86_relocatableCode(src);
#elif defined(rs6000_ibm_aix5_1)
   return (relocatableCode_t*)new power_relocatableCode(src);
#endif
}

relocatableCode_t* relocatableCode_t::getRelocatableCode(Dummy d) {
#ifdef sparc_sun_solaris2_7
   return (relocatableCode_t*) new sparc_relocatableCode(d);
#elif defined(i386_unknown_linux2_4)
   return (relocatableCode_t*)new x86_relocatableCode(d);
#elif defined(rs6000_ibm_aix5_1)
   return (relocatableCode_t*)new power_relocatableCode(d);
#endif
}

void relocatableCode_t::resolve1Call(const insnOffset &offs,
                                     pdvector<insnVec_t*> &chunks,
                                     kptr_t insnAddr,
                                     kptr_t calleeAddr,
                                     kptr_t calleeAddrAtEmitTime
                                        // only used for asserting
                                     ) {
#ifdef sparc_sun_solaris2_7
   sparc_relocatableCode::resolve1Call(offs, chunks, insnAddr, calleeAddr, calleeAddrAtEmitTime);
#elif defined(i386_unknown_linux2_4)
   x86_relocatableCode::resolve1Call(offs, chunks, insnAddr, calleeAddr, calleeAddrAtEmitTime);
#elif defined(ppc64_unknown_linux2_4)
   power_relocatableCode::resolve1Call(offs, chunks, insnAddr, calleeAddr, calleeAddrAtEmitTime);
#endif
}

 void relocatableCode_t::resolve1SetImmAddr(const insnOffset &offs, 
                                            pdvector<insnVec_t*> &chunks,
                                            kptr_t symAddr) {
#ifdef sparc_sun_solaris2_7
   sparc_relocatableCode::resolve1SetImmAddr(offs, chunks, symAddr);
#elif defined(i386_unknown_linux2_4)
   x86_relocatableCode::resolve1SetImmAddr(offs, chunks, symAddr);
#elif defined(ppc64_unknown_linux2_4)
   power_relocatableCode::resolve1SetImmAddr(offs, chunks, symAddr);
#endif
 }
