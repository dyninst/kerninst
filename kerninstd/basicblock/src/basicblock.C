#include <limits.h>
#include <assert.h>
#include "basicblock.h"
#include "util/h/minmax.h"
#include "util/h/xdr_send_recv.h" // for xdr routines

#ifdef sparc_sun_solaris2_7
#include "sparc_basicblock.h"
#elif defined(rs6000_ibm_aix5_1) 
#include "power_basicblock.h"
#elif defined(i386_unknown_linux2_4)
#include "x86_basicblock.h"
#endif

basicblock_t::bbid_t
basicblock_t::SuccIter::end = (bbid_t)-1;

basicblock_t::bbid_t
basicblock_t::PredIter::end = (bbid_t)-1;

const basicblock_t::bbid_t
basicblock_t::ConstSuccIter::end = (bbid_t)-1;

basicblock_t::bbid_t
basicblock_t::ConstPredIter::end = (bbid_t)-1;

basicblock_t::bbid_t
basicblock_t::UnanalyzableBBId = (bbid_t)-3;

basicblock_t::bbid_t
basicblock_t::interProcBranchExitBBId = (bbid_t)-4;

basicblock_t::bbid_t
basicblock_t::xExitBBId = (bbid_t)-5;

basicblock_t::basicblock_t(const basicblock_t &src) {
   // needed by x86_fn_ll, which uses vector<basicblock_t>
   if (this != &src)
      assign(src);
}

void basicblock_t::assign(const basicblock_t &src) {
   startAddr = src.startAddr;
   lastInsnAddr = src.lastInsnAddr;
   numInsnBytesInBB = src.numInsnBytesInBB;

   moreThan2Succs = src.moreThan2Succs;
   if (moreThan2Succs) {
      // using actualSuccs instead
      actualSuccs = new bbid_t[src.numSuccessors()+1]; // +1 for (bbid_t)-1
      bbid_t *ptr1 = actualSuccs;
      bbid_t *ptr2 = src.actualSuccs;
      while ((*ptr1++ = *ptr2++) != (bbid_t)-1)
         ;
   }
   else {
      s.succ0 = src.s.succ0;
      if (s.succ0 == (bbid_t)-1)
         s.succ1 = (bbid_t)-1; // not really needed
      else
         s.succ1 = src.s.succ1;
   }

   moreThan2Preds = src.moreThan2Preds;
   if (moreThan2Preds) {
      // using actualPreds instead
      actualPreds = new bbid_t[src.numPredecessors()+1]; // +1 for (bbid_t)-1
      bbid_t *ptr1 = actualPreds;
      bbid_t *ptr2 = src.actualPreds;
      while ((*ptr1++ = *ptr2++) != (bbid_t)-1)
         ;
   }
   else {
      p.pred0 = p.pred1 = (bbid_t)-1;
      
      if ((p.pred0 = src.p.pred0) != (bbid_t)-1)
         p.pred1 = src.p.pred1;
   }
}

basicblock_t & basicblock_t::operator=(const basicblock_t &src) {
   // needed by x86_fn_ll, which uses vector<basicblock_t>
   if (this != &src)
      assign(src);

   return *this;
}

basicblock_t::basicblock_t(XDR *xdr)
{
   if (!P_xdr_recv(xdr, startAddr))
      throw xdr_recv_fail();

   if (!P_xdr_recv(xdr, lastInsnAddr))
      throw xdr_recv_fail();
   
   uint16_t temp_numInsnBytes;
   if (!P_xdr_recv(xdr, temp_numInsnBytes))
      throw xdr_recv_fail();
   else
      numInsnBytesInBB = temp_numInsnBytes;

   unsigned num_succs;
   if (!P_xdr_recv(xdr, num_succs))
      throw xdr_recv_fail();

   unsigned num_preds;
   if (!P_xdr_recv(xdr, num_preds))
      throw xdr_recv_fail();

   moreThan2Succs = (num_succs > 2);
   if (moreThan2Succs) {
      // num_succs > 2
      actualSuccs = new bbid_t[num_succs + 1];
         // +1 for (bbid_t)-1 delimiter at end
      assert(actualSuccs);
      
      bbid_t *succiter = actualSuccs;
      bbid_t *succfinish = succiter + num_succs;
      while (succiter != succfinish) {
         if (!P_xdr_recv(xdr, *succiter))
            throw xdr_recv_fail();
         ++succiter;
      }

      *succfinish = (bbid_t)-1;
   }
   else if (num_succs == 0) {
      s.succ0 = s.succ1 = (bbid_t)-1;
   }
   else if (num_succs <= 2) {
      if (!P_xdr_recv(xdr, s.succ0))
         throw xdr_recv_fail();
      
      if (num_succs == 1)
         s.succ1 = (bbid_t)-1;
      else if (!P_xdr_recv(xdr, s.succ1))
         throw xdr_recv_fail();
   }
   else
      assert(false);

   // ----------

   moreThan2Preds = (num_preds > 2);
   p.pred0 = p.pred1 = (bbid_t)-1;

   if (moreThan2Preds) {
      actualPreds = new bbid_t[num_preds+1];
      bbid_t *iter = actualPreds;
      bbid_t *finish = actualPreds + num_preds;

      while (iter != finish) {
         if (!P_xdr_recv(xdr, *iter))
            throw xdr_recv_fail();
         ++iter;
      }
      *finish = (bbid_t)-1;
   }
   else if (num_preds > 0) {
      // at least 1
      if (!P_xdr_recv(xdr, p.pred0))
         throw xdr_recv_fail();
      
      if (num_preds > 1) {
         // two.
         if (!P_xdr_recv(xdr, p.pred1))
            throw xdr_recv_fail();
      }
   }

   assert(num_preds == numPredecessors());
}

basicblock_t *basicblock_t::getBasicBlock(XDR *xdr)
{
   basicblock_t *bb;
#ifdef sparc_sun_solaris2_7
   bb = new sparc_basicblock(xdr);
#elif defined(rs6000_ibm_aix5_1) 
   bb = new power_basicblock(xdr);
#elif defined(i386_unknown_linux2_4) 
   bb = new x86_basicblock(xdr);
#endif
   return bb;
}

bool basicblock_t::send(XDR *xdr) const {
   if (!P_xdr_send(xdr, startAddr))
      return false;
   
   if (!P_xdr_send(xdr, lastInsnAddr))
      return false;
   
   const uint16_t temp_numInsnBytesInBB = numInsnBytesInBB;
   if (!P_xdr_send(xdr, temp_numInsnBytesInBB))
      return false;

   const unsigned num_succs = numSuccessors();
   const unsigned num_preds = numPredecessors();
   if (!P_xdr_send(xdr, num_succs) ||
       !P_xdr_send(xdr, num_preds))
      return false;
   
   ConstSuccIter succiter = beginSuccIter();
   ConstSuccIter succfinish = endSuccIter();
   while (succiter != succfinish) {
      bbid_t succid = *succiter;
      if (!P_xdr_send(xdr, succid))
         return false;
      ++succiter;
   }
   
   ConstPredIter prediter = beginPredIter();
   ConstPredIter predfinish = endPredIter();
   while (prediter != predfinish) {
      bbid_t predid = *prediter;
      if (!P_xdr_send(xdr, predid))
         return false;
      ++prediter;
   }

   return true;
}

basicblock_t::basicblock_t(kptr_t bbStartAddr, bbid_t parent_id) :
   moreThan2Succs(false),
   moreThan2Preds(false)
{
   s.succ0 = s.succ1 = (bbid_t)-1; // couldn't initialize above
   p.pred0 = p.pred1 = (bbid_t)-1; // couldn't initialize above
   
   startAddr = bbStartAddr;
   lastInsnAddr = startAddr;
   numInsnBytesInBB = 0;

   if (parent_id != (bbid_t)-1)
      addPredecessorNoDup(parent_id);
}

basicblock_t *basicblock_t::getBasicBlock(kptr_t bbStartAddr, 
					  bbid_t parent_bb_id)
{
   basicblock_t *bb;
#ifdef sparc_sun_solaris2_7
   bb = new sparc_basicblock(bbStartAddr, parent_bb_id);
#elif defined(rs6000_ibm_aix5_1) 
   bb = new power_basicblock(bbStartAddr, parent_bb_id);
#elif defined(i386_unknown_linux2_4) 
   bb = new x86_basicblock(bbStartAddr, parent_bb_id);
#endif
   return bb;
}

basicblock_t::basicblock_t(kptr_t bbStartAddr, kptr_t bbLastInsnAddr,
			   uint16_t bbNumInsnBytes,
                           const fnCode_t &/*fnCode*/) :
          startAddr(bbStartAddr),
	  lastInsnAddr(bbLastInsnAddr),
	  numInsnBytesInBB(bbNumInsnBytes),
          moreThan2Succs(false),
          moreThan2Preds(false)
{
   s.succ0 = s.succ1 = (bbid_t)-1; // can't initialize it above (struct members)
   p.pred0 = p.pred1 = (bbid_t)-1; // couldn't initialize above
}

void basicblock_t::appendToBBContents(const instr_t *instr,
                                      kptr_t chunkStartAddr,
                                      const insnVec_t &chunkContents) {
   const unsigned insnByteOffsetWithinBB = numInsnBytesInBB; // before bumping it up
   const unsigned insnByteOffsetWithinChunk =
      startAddr - chunkStartAddr + insnByteOffsetWithinBB;
   
   lastInsnAddr = startAddr + numInsnBytesInBB;
   numInsnBytesInBB += instr->getNumBytes();

   assert(chunkContents.get_by_byteoffset(insnByteOffsetWithinChunk) == instr);
}

basicblock_t::basicblock_t(Split, bbid_t parent_bb_id, basicblock_t *parent,
			   kptr_t splitAddr, const fnCode_t &/*fnCode*/) :
   moreThan2Succs(false),
   moreThan2Preds(false)
{
   s.succ0 = s.succ1 = (bbid_t)-1;
   p.pred0 = p.pred1 = (bbid_t)-1; // couldn't initialize above
   moreThan2Preds = false;
   
   // split-ctor.
   // splitAddr is an addr within parent; make it the start of 'this'; 
   // make parent fall through to 'this'.
   // We make no changes to 'parent', since this is a ctor (ctors don't 
   // traditionally modify their arguments).  All changes to parent are 
   // made by the caller, which for us is basicblock_t::splitAt(). (We 
   // can't make parent const for other reasons)

   assert(splitAddr > parent->getStartAddr()); // don't split on 1st insn
   assert(splitAddr <= parent->getEndAddr());
   
   // we need to do: successors = parent.successors (& update "moreThan2Succs")
   if (parent->moreThan2Succs) {
      moreThan2Succs = true;
      unsigned num_succ = parent->numSuccessors();
      actualSuccs = new bbid_t[num_succ+1]; // +1 for (bbid_t)-1 entry at end
      memcpy(actualSuccs, parent->actualSuccs,
             sizeof(bbid_t) * (num_succ + 1));
      assert(actualSuccs[num_succ] == (bbid_t)-1);
   }
   else {
      s.succ0 = parent->s.succ0;
      if (s.succ0 != (bbid_t)-1)
         s.succ1 = parent->s.succ1;
   }
   assert(numSuccessors() == parent->numSuccessors());

   if (!addPredecessorUnlessDup(parent_bb_id))
      assert(false);

   const kptr_t endAddr = parent->getEndAddr();
   startAddr = splitAddr;
   lastInsnAddr = parent->getLastInsnAddr();
   numInsnBytesInBB = endAddr - startAddr + 1;
   if (instr_t::hasUniformInstructionLength())
      assert(numInsnBytesInBB % 4 == 0);
}

basicblock_t* basicblock_t::getBasicBlock(Split s, bbid_t parent_bb_id, 
					  basicblock_t *parent,
					  kptr_t splitAddr, 
					  const fnCode_t &fnCode) {
   basicblock_t *bb;
#ifdef sparc_sun_solaris2_7
   bb = new sparc_basicblock(s, parent_bb_id, parent, splitAddr, fnCode);
#elif defined(i386_unknown_linux2_4)
   bb = new x86_basicblock(s, parent_bb_id, parent, splitAddr, fnCode);
#elif defined(rs6000_ibm_aix5_1) 
   bb = new power_basicblock(s, parent_bb_id, parent, splitAddr, fnCode);
#endif
   return bb;
}

void basicblock_t::replace1Predecessor(bbid_t old_bb_id, bbid_t new_bb_id) {
   // We use iterators to make things simpler.  Assert fails if not found.

   PredIter iter = beginPredIter();
   PredIter finish = endPredIter();

   bool successful = false;
   
   for (; iter != finish; ++iter) {
      bbid_t &bbid = *iter;
      
      if (bbid == old_bb_id) {
         // success
         assert(!successful);
         bbid = new_bb_id;
         successful = true;
      }
      else
         assert(bbid != new_bb_id); // avoid duplicates
   }

   assert(successful && "replace1Predecessor: not found");
   assert(!hasAsAPredecessor(old_bb_id));
}

void basicblock_t::erase1Predecessor(bbid_t old_bb_id) {
   // Gather a list of new preds
   bool found = false;
   pdvector<bbid_t> newPreds;
   PredIter piter = beginPredIter();
   PredIter pfinish = endPredIter();
   for (; piter != pfinish; ++piter) {
      const bbid_t pred_bb_id = *piter;
      if (pred_bb_id == old_bb_id) {
         assert(!found);
         found = true;
      }
      else
         newPreds += pred_bb_id;
   }
   
   assert(found);
   
   clearAllPredecessors();
   pdvector<bbid_t>::const_iterator iter = newPreds.begin();
   pdvector<bbid_t>::const_iterator finish = newPreds.end();
   for (; iter != finish; ++iter) {
      const bbid_t new_pred_id = *iter;
      addPredecessorNoDup(new_pred_id);
   }
}

basicblock_t::~basicblock_t() {
   if (moreThan2Succs)
      delete [] actualSuccs;
   
   if (moreThan2Preds)
      delete [] actualPreds;
}

bool basicblock_t::addPredecessorUnlessDup(bbid_t pred) {
   // If it already exists, do nothing.  Looping through the entire set may seem
   // inefficient, but realistically, I don't see how the set can get very big
   // (hmmm...except perhaps for big switch statements)

   const PredIter start = beginPredIter();
   const PredIter finish = endPredIter();
   for (PredIter iter=start; iter != finish; ++iter) {
      if (*iter == pred)
         return false;
   }

   addPredecessorNoDup(pred);
   return true;
}

void basicblock_t::addPredecessorNoDup(bbid_t pred) {
   const unsigned origNumPreds = numPredecessors();
   
   assert(moreThan2Preds == (origNumPreds > 2));
   
   if (moreThan2Preds) {
      // already using actualPreds[]
      unsigned oldnum = numPredecessors();
      assert(actualPreds[origNumPreds] == (bbid_t)-1);

      bbid_t *new_vec = new bbid_t[origNumPreds+1+1];
         // +1 for new entry, +1 for (bbid_t)-1
      for (unsigned lcv=0; lcv < oldnum; lcv++) {
         bbid_t bb_id = actualPreds[lcv];
         assert(bb_id != pred); // assert noDup
         assert(bb_id != (bbid_t)-1);
         new_vec[lcv]= bb_id;
      }
      new_vec[origNumPreds] = pred;
      new_vec[origNumPreds+1] = (bbid_t)-1;
      delete [] actualPreds;
      actualPreds = new_vec;
   }
   else if (origNumPreds == 0) {
      // there were no preds.  Now there's 1.
      assert(p.pred0 == (bbid_t)-1);
      p.pred0 = pred;
      p.pred1 = (bbid_t)-1;
   }
   else if (origNumPreds == 1) {
      // was 1, now 2
      assert(p.pred0 != (bbid_t)-1 && p.pred0 != pred);
      assert(p.pred1 == (bbid_t)-1);
      p.pred1 = pred;
      assert(numPredecessors() == 1+origNumPreds);
   }
   else if (origNumPreds == 2) {
      // was 2, now 3
      // Note that we mustn't allocate actualPreds[] yet, due to the union
      // (we don't want to invalidate pred0 before we copy it, e.g.!)
      bbid_t *vec = new bbid_t[4];
      vec[0] = p.pred0;
      vec[1] = p.pred1;
      vec[2] = pred;
      vec[3] = (bbid_t)-1;

      actualPreds = vec;
      moreThan2Preds = true;
   }

   assert(numPredecessors() == origNumPreds + 1);
   assert(moreThan2Preds == (numPredecessors() > 2));
}

void basicblock_t::clearAllPredecessors() {
   if (moreThan2Preds)
      delete [] actualPreds;

   p.pred0 = p.pred1 = (bbid_t)-1;
   moreThan2Preds = false;
}

bool basicblock_t::addSuccessorUnlessDup(bbid_t succ) {
   // If it's already there, do nothing.
   // returns true iff it added
   const SuccIter start = beginSuccIter();
   const SuccIter finish = endSuccIter();
   for (SuccIter succiter=start; succiter != finish; ++succiter) {
      bbid_t bb_id = *succiter;
      if (bb_id == succ)
         return false; // dup
   }
   
   addSuccessorNoDup(succ);

   return true;
}

void basicblock_t::addSuccessorNoDup(bbid_t succ) {
   // This routine's allowed to assert that the successor being added
   // isn't already a successor.

   unsigned orig_num_successors = numSuccessors();
   assert(moreThan2Succs == (orig_num_successors > 2));

   if (moreThan2Succs) {
      // using actualSuccs
      unsigned oldnum = numSuccessors();
      assert(actualSuccs[oldnum] == (bbid_t)-1);

      bbid_t *new_vec = new bbid_t[oldnum+1+1]; // +1 for new entry, +1 for (bbid_t)-1
      for (unsigned lcv=0; lcv < oldnum; lcv++) {
         bbid_t bb_id = actualSuccs[lcv];
         assert(bb_id != succ); // assert noDup
         assert(bb_id != (bbid_t)-1);
         new_vec[lcv]= bb_id;
      }
      new_vec[oldnum] = succ;
      new_vec[oldnum+1] = (bbid_t)-1;
      delete [] actualSuccs;
      actualSuccs = new_vec;
      assert(numSuccessors() == 1+orig_num_successors);
   }
   else if (s.succ0 == (bbid_t)-1) {
      // there were no successors.  Now there's 1.
      s.succ0 = succ;
      s.succ1 = (bbid_t)-1;
      assert(numSuccessors() == 1+orig_num_successors);
   }
   else {
      // s.succ0 a legit entry.
      assert(s.succ0 != succ); // assert noDup
      
      if (s.succ1 == (bbid_t)-1) {
         // was 1, now 2.
         s.succ1 = succ;
         assert(orig_num_successors == 1);
         assert(numSuccessors() == 1+orig_num_successors);
      }
      else {
         // both succ0 and succ1 were legit entries.  Must now switch over
         // to using actualSuccs[]
         bbid_t *vec = new bbid_t[4]; // 3 entries plus (bbid_t)-1
         // can't assign it to actualSuccs yet or it'll overwrite succ1!

         vec[0] = s.succ0;
         vec[1] = s.succ1;
         vec[2] = succ;
         vec[3] = (bbid_t)-1;

         moreThan2Succs = true;
         actualSuccs = vec;

         assert(orig_num_successors == 2);
         assert(numSuccessors() == 1+orig_num_successors);
      }
   }

   assert(numSuccessors() == orig_num_successors + 1);
}

void basicblock_t::clearAllSuccessors() {
   if (moreThan2Succs) {
      delete [] actualSuccs;
      actualSuccs = NULL;
   }

   s.succ0 = s.succ1 = (bbid_t)-1;
   moreThan2Succs = false;
}

bool basicblock_t::hasAsAPredecessor(bbid_t testme) const {
   ConstPredIter start = beginPredIter();
   ConstPredIter finish = endPredIter();
   for (ConstPredIter iter=start; iter != finish; ++iter)
      if (*iter == testme)
         return true;
   return false;
}

bool basicblock_t::hasAsASuccessor(bbid_t testme) const {
   ConstSuccIter start = beginSuccIter();
   ConstSuccIter finish = endSuccIter();
   for (ConstSuccIter succiter = start; succiter != finish; ++succiter) {
      if (*succiter == testme)
         return true;
   }

   return false;
}

unsigned basicblock_t::numSuccessors() const {
   // try to use iterators instead
   if (moreThan2Succs) {
      // using actualSuccs
      assert(actualSuccs != (bbid_t*)-1);
      unsigned result = 0;
      bbid_t *ptr = actualSuccs;
      while (*ptr++ != (bbid_t)-1)
         ++result;

      assert(result > 2);
      return result;
   }
   else if (s.succ0 == (bbid_t)-1)
      return 0;
   else {
      if (s.succ1 == (bbid_t)-1)
         return 1;
      else
         return 2;
   }
}

basicblock_t::bbid_t basicblock_t::getsucc(unsigned ndx) const {
   // Try to use iterators instead.
   // We assert fail if ndx is not within range.  (We used to return -1 on such an
   // error; that's still an option.)
   assert(ndx < numSuccessors());

   if (moreThan2Succs)
      return actualSuccs[ndx];
   else if (s.succ0 == (bbid_t)-1)
      //return (bbid_t)-1;
      assert(false);
   else {
      if (ndx == 0)
         return s.succ0;
      else if (ndx == 1)
         return s.succ1;
      else
         assert(false);
   }
   return false; //placate compiler
}

unsigned basicblock_t::numPredecessors() const {
   // Try to use iterators instead
   if (moreThan2Preds) {
      unsigned result = 0;
      bbid_t *ptr = actualPreds;
      while (*ptr++ != (bbid_t)-1)
         ++result;
      return result;
   }
   else if (p.pred0 == (bbid_t)-1)
      return 0;
   else if (p.pred1 == (bbid_t)-1)
      return 1;
   else
      return 2;
}

basicblock_t::bbid_t basicblock_t::getpred(unsigned ndx) const {
   // Try to use iterators instead
   assert(ndx < numPredecessors());

   if (ndx == 0) {
      assert(!moreThan2Preds);
      return p.pred0;
   }
   else if (ndx == 1) {
      assert(!moreThan2Preds);
      return p.pred1;
   }
   else {
      assert(moreThan2Preds);
      return actualPreds[ndx];
   }
}
