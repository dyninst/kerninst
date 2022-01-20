// fnCodeObjects.C

#include "fnCodeObjects.h"
#include "xdr_send_recv_common.h"
#include "util/h/xdr_send_recv.h" // the fancier P_xdr_send/recv routines
#include "util/h/rope-utils.h" // addr2hex()
#include "simplePath.h"

#include "controlFlow.h"

// Q: is it really necessary to still have this ifdef?
#ifdef _KERNINSTD_
#include "kmem.h"
#include "codeObjectCreationOracle.h"
#endif

fnCodeObjects::fnCodeObjects() {
}

fnCodeObjects::fnCodeObjects(const fnCodeObjects &src) :
      theBlocks(src.theBlocks) {
   // we have made a shallow pointer-only copy of each element; make it a deep copy now

   pdvector<basicBlockCodeObjects*>::iterator iter = theBlocks.begin();
   pdvector<basicBlockCodeObjects*>::iterator finish = theBlocks.end();
   for (; iter != finish; ++iter) {
      const basicBlockCodeObjects *bbco = *iter;

      *iter = new basicBlockCodeObjects(*bbco);
   }
}


// Needed for egcs 1.1.2:
template <class T>
static void destruct1(const T &vrble) {
   vrble.~T();
}

fnCodeObjects::fnCodeObjects(XDR *xdr) {
   // P_xdr_recv() for pdvector<> wants to operate on uninitialized memory, so we should
   // call the destructor first:
   destruct1(theBlocks); // needed on egcs1.1.2

   if (!P_xdr_recv_pointers(xdr, theBlocks))
      throw xdr_recv_fail();
}

bool fnCodeObjects::send(XDR *xdr) const {
   return P_xdr_send_pointers(xdr, theBlocks);
}


fnCodeObjects::~fnCodeObjects() {
   pdvector<basicBlockCodeObjects*>::iterator iter = theBlocks.begin();
   pdvector<basicBlockCodeObjects*>::iterator finish = theBlocks.end();
   for (; iter != finish; ++iter) {
      basicBlockCodeObjects *bbco = *iter;
      delete bbco;
   }
}

// Q: is it really necessary to still have this ifdef?
#ifdef _KERNINSTD_
void fnCodeObjects::parse(const moduleMgr &theModuleMgr,
                          function_t *fn,
                          const pdvector<fnAddrBeingRelocated> &fnAddrsBeingRelocated,
                             // see the .I file for an explanation of this param
                          bool verbose_fnParse) {
   theBlocks.resize(fn->numBBs());
   // theBlocks[] is just pointers; so we need to construct manually.

   pdvector<basicBlockCodeObjects*>::iterator biter = theBlocks.begin();
   pdvector<basicBlockCodeObjects*>::iterator bfinish = theBlocks.end();
   for (; biter != bfinish; ++biter)
      *biter = new basicBlockCodeObjects(); // default ctor works nicely

   const bbid_t entry_bb_id = fn->xgetEntryBB();

   try {
      codeObjectCreationOracle *theCodeObjectCreationOracle = 
	codeObjectCreationOracle::
	getCodeObjectCreationOracle(theModuleMgr,
				    *fn,
				    *this,
				    fnAddrsBeingRelocated,
				    verbose_fnParse);

      parseBlockIfNeeded(entry_bb_id,
                         simplePath(),
                         theCodeObjectCreationOracle);
      delete(theCodeObjectCreationOracle);
   }
   catch (RelocationForThisCodeObjNotYetImpl) {
      assert(isUnparsed());
      // makeUnparsed();
   }
   catch (function_t::parsefailed) {
      throw function_t::parsefailed();
   }
   catch (instr_t::unimplinsn) {
      throw function_t::parsefailed();
   }
   catch (instr_t::unknowninsn) {
      throw function_t::parsefailed();
   }
   catch (...) {
      cout << "codeObject parse caught an unknown exception" << endl;
      throw function_t::parsefailed();
   }

   // assert that all blocks are completed:
   if (!isUnparsed()) {
      for (bbid_t bbid=0; bbid < theBlocks.size(); ++bbid) {
         const basicBlockCodeObjects *bbco = theBlocks[bbid];
         if (!bbco->isCompleted()) {
            cout << "basic block id " << bbid
                 << " has not been complete()'d after parsing" << endl;
            const basicblock_t *bb = fn->getBasicBlockFromId(bbid);
            cout << "Block starts at " << addr2hex(bb->getStartAddr()) << endl;
            cout << "and lasts " << bb->getNumInsnBytes() << " bytes" << endl;
            assert(false);
         }
         assert(!bbco->isEmpty() && "basic block contains no code objects?");
      }
   }
}

void fnCodeObjects::parseBlockIfNeeded(bbid_t bb_id,
                                       const simplePath &thePath,
                                       codeObjectCreationOracle *theOracle) {
   const function_t *fn = theOracle->getFn();
   
   basicBlockCodeObjects *theBlockCodeObjects = theBlocks[bb_id];
   if (theBlockCodeObjects->isCompleted())
      return;

   try {
      fn->parse(bb_id,
               //kmem(fn), // fast, thankfully
               thePath,
               theOracle);
   }
   catch (RelocationForThisCodeObjNotYetImpl) {
      // important to have this, lest the "catch (...)" convert the exception
      // to a parsefailed() one
      throw;
   }
   catch (function_t::parsefailed) {
      throw function_t::parsefailed();
   }
   catch (instr_t::unimplinsn) {
      throw function_t::parsefailed();
   }
   catch (instr_t::unknowninsn) {
      throw function_t::parsefailed();
   }
   catch (...) {
      cout << "fnCodeObjects::parseBlockIfNeeded() caught an unknown exception" << endl;
      throw function_t::parsefailed();
   }
}

void fnCodeObjects::appendToBasicBlock(bbid_t bb_id,
                                       unsigned byteOffsetWithinBB,
                                       codeObject *co) {
   // I'd love to assert that "byteOffsetWithinBB" is within range of the bb's
   // size, but we don't seem to have access to the basicblock_t within this class.
   // Oh well.

   theBlocks[bb_id]->append(byteOffsetWithinBB, co);
}

void fnCodeObjects::endBasicBlock(bbid_t bb_id) {
   basicBlockCodeObjects *theBlockCodeObjects = theBlocks[bb_id];
   theBlockCodeObjects->complete();
}

pair<unsigned, unsigned>
fnCodeObjects::
getCodeObjNdxWithinBBAndInsnByteOffsetWithinCodeObj(const function_t *fn,
                                                    bbid_t bbid,
                                                    unsigned insnByteOffsetWithinBB) const {
   // return value:
   // .first: code object ndx within bb
   // .second: insn byte offset, within the code object, of the instruction

   const kptr_t bbStartAddr = fn->getBasicBlockFromId(bbid)->getStartAddr();
   assert(insnByteOffsetWithinBB < fn->getBasicBlockFromId(bbid)->getNumInsnBytes());
   const kptr_t insnAddr = bbStartAddr + insnByteOffsetWithinBB;
   assert(instr_t::instructionHasValidAlignment(insnAddr));
      // useful assert, in case someone dares to pass an instruction ndx
      // instead of an instruction byte offset as the 3d param

   const basicBlockCodeObjects *bbco = theBlocks[bbid];
   basicBlockCodeObjects::const_iterator iter = 
      bbco->findCodeObjAtInsnByteOffset(insnByteOffsetWithinBB, false);
      // false --> doesn't have to be the start of a code object
   assert(iter != bbco->end());
   
   const pair<unsigned, codeObject*> &theCodeObjectInfo = *iter;
      // .first: byte offset within bb where the code object begins
      // .second: the code object itself

   assert(insnByteOffsetWithinBB >= theCodeObjectInfo.first);
   const unsigned insnByteOffsetWithinCodeObject =
      insnByteOffsetWithinBB - theCodeObjectInfo.first;
   assert(instr_t::instructionHasValidAlignment(bbStartAddr + theCodeObjectInfo.first + insnByteOffsetWithinCodeObject));
   assert(bbStartAddr + theCodeObjectInfo.first + insnByteOffsetWithinCodeObject ==
          insnAddr);
   
   const unsigned codeObjNdxWithinBB = iter - bbco->begin();

   return make_pair(codeObjNdxWithinBB, insnByteOffsetWithinCodeObject);
}

#endif
