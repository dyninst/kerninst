// basicBlockCodeObjects.C

#include "basicBlockCodeObjects.h"
#include "xdr_send_recv_common.h"
#include "codeObject.h"
#include "util/h/xdr_send_recv.h"
#include <algorithm> // stl's lower_bound()

basicBlockCodeObjects::basicBlockCodeObjects() {
   completed = false;
}

basicBlockCodeObjects::basicBlockCodeObjects(const basicBlockCodeObjects &src) :
      completed(src.completed),
      theCodeObjects(src.theCodeObjects) {
   // we have made a shallow, pointer-only, copy of theCodeObjects.  Make it a true
   // copy now

   pdvector< pair<unsigned, codeObject*> >::iterator iter = theCodeObjects.begin();
   pdvector< pair<unsigned, codeObject*> >::iterator finish = theCodeObjects.end();
   for (; iter != finish; ++iter) {
      codeObject *co = iter->second;
      iter->second = co->dup();
   }

   assert(theCodeObjects.size() == src.theCodeObjects.size());
}

basicBlockCodeObjects::~basicBlockCodeObjects() {
   pdvector< pair<unsigned, codeObject*> >::iterator iter = theCodeObjects.begin();
   pdvector< pair<unsigned, codeObject*> >::iterator finish = theCodeObjects.end();
   for (; iter != finish; ++iter) {
      codeObject *co = iter->second;
      delete co;
   }
}

basicBlockCodeObjects::basicBlockCodeObjects(XDR *xdr) {
   // unsigned: nelems
   // each item, in turn
   uint32_t nelems;
   if (!P_xdr_recv(xdr, nelems))
      throw xdr_recv_fail();
   
   // sanity check.  Rather arbitrary.
   assert(nelems < 10000);

   theCodeObjects.grow(nelems); // cheap; vector is of unsigned/ptr pair

   for (unsigned ndx=0; ndx < nelems; ++ndx) {
      unsigned insnByteOffset;
      if (!P_xdr_recv(xdr, insnByteOffset))
         throw xdr_recv_fail();

      codeObject *co = codeObject::make(xdr);
      theCodeObjects[ndx] = std::make_pair(insnByteOffset, co);
   }
   
   completed = true; // um, I guess
}

bool basicBlockCodeObjects::send(XDR *xdr) const {
   assert(completed); // since the xdr ctor assumes it
   // can't simply call P_xdr_send() on theCodeObjects, since the elements are
   // pointers (P_xdr_send() boolean version will actually get called(!)).
   // So we send individual elements.

   const uint32_t nelems = theCodeObjects.size();
   if (!P_xdr_send(xdr, nelems))
      throw xdr_send_fail();
   
   pdvector< pair<unsigned, codeObject*> >::const_iterator iter = theCodeObjects.begin();
   pdvector< pair<unsigned, codeObject*> >::const_iterator finish = theCodeObjects.end();
   for (; iter != finish; ++iter) {
      const unsigned insnByteOffset = iter->first;
      codeObject *co = iter->second;
      if (!P_xdr_send(xdr, insnByteOffset) || !co->send(xdr))
         return false;
   }

   return true;
}

void basicBlockCodeObjects::append(unsigned insnByteOffset, codeObject *co) {
   assert(!completed &&
          "cannot append() to a basicBlockCodeObjects() that has been completed");
   theCodeObjects += std::make_pair(insnByteOffset, co);
}

void basicBlockCodeObjects::complete() {
   assert(!completed && "basicBlockCodeObjects already completed for this block");

   // assert that insnByteOffsets are strictly increasing
   // (should we relax this restriction to monotonically increasing?  i.e., is it OK
   // for a code object to be 0 bytes in the original code.  For now we say no, and
   // this check happens to also assert that this doesn't happen.)
   if (theCodeObjects.size() > 0) {
      pdvector< pair<unsigned, codeObject*> >::const_iterator iter =
         theCodeObjects.begin();
      pdvector< pair<unsigned, codeObject*> >::const_iterator finish =
         theCodeObjects.end()-1;
      for (; iter != finish; ++iter) {
         const unsigned byteOffsetUs = iter->first;
         const unsigned byteOffsetNext = (iter+1)->first;
         assert(byteOffsetUs < byteOffsetNext);
      }
   }
   
   completed = true;
}

// ----------------------------------------------------------------------

basicBlockCodeObjects::const_iterator
basicBlockCodeObjects::findCodeObjAtInsnByteOffset(unsigned byteOffsetWithinBB,
                                                   bool startOfCodeObjectOnly) const {
   // returns end() if not found

   // can't use lower_bound() so easily because a comparitor function is surprisingly
   // hard to write.  Given pair<unsigned, codeObject*>, we know where a code object
   // begins but not where it ends.  There's no easy way to access the next item,
   // because there's no easy way to turn a value back into an iterator, and even if
   // there were, how would we check for end()?  Similarly, the slower find_if()
   // isn't easy to write either.
   // We use upper_bound() instead (how ugly!) and then back up 1 space:

   pdvector< pair<unsigned, codeObject*> >::const_iterator iter =
      upper_bound(theCodeObjects.begin(), theCodeObjects.end(),
                  byteOffsetWithinBB, comparitor());

   assert(iter != theCodeObjects.begin());
   --iter;
   
   assert(byteOffsetWithinBB >= iter->first);
   if (iter + 1 != theCodeObjects.end())
      assert(byteOffsetWithinBB < (iter+1)->first);

   if (startOfCodeObjectOnly && iter->first != byteOffsetWithinBB)
      return theCodeObjects.end();
   else
      return iter;
}
