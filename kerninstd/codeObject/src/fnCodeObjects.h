// fnCodeObjects.h
// The ifdef's are needed so that kperfmon won't choke when compiling types that
// it knows nothing about (e.g. function).

#ifndef _FN_CODE_OBJECTS_H_
#define _FN_CODE_OBJECTS_H_

#include "common/h/Vector.h"
#include "basicBlockCodeObjects.h"
#include "funkshun.h"
#include "moduleMgr.h"
#include "util/h/kdrvtypes.h" // kptr_t
#include "common/h/String.h"
#include <utility> // STL's pair
using std::pair;

// A #include of "kerninstdClient.xdr.h" leads to a recursive #include 
// of this .h file, which is no good since kerninstdClient.xdr.h needs 
// to know the complete definition of fnCodeObjects.  So we bypass the 
// #include and go with the typedef.
typedef pair<kptr_t, pdstring> fnAddrBeingRelocated; // from .I file

class codeObjectCreationOracle;

class fnCodeObjects {
 public:
   class RelocationForThisCodeObjNotYetImpl {};
   
 private:
   typedef function_t::bbid_t bbid_t;

   // NOTE: An empty theBlocks[] indicates failure to parse the code object
   // (We really do need this feature.  Some code objects are going to be
   // ridiculously hard to implement because they're inherently sensitive to
   // relocation.  Consider nonLoadingBackwardsJumpTableCodeObject, needed by blkclr(),
   // for example.)
   pdvector<basicBlockCodeObjects*> theBlocks;
      // indexed by basic block id.  As long as there are no gaps in bb id's (and I
      // think that there aren't), then this is OK.  If not, we can switch to a
      // dictionary.
      // Elements are pointers to avoid the expensive copy-ctor for class
      // basicBlockCodeObjects.

   fnCodeObjects &operator=(const fnCodeObjects &);
   
 public:
   fnCodeObjects();
   fnCodeObjects(const fnCodeObjects &); // yes, this is needed
   fnCodeObjects(XDR *);
  ~fnCodeObjects();

   void makeUnparsed() {
      theBlocks.zap();
      assert(isUnparsed());
   }
   bool isUnparsed() const {
      // Check the sentinel to see if this fnCodeObjects is a dummy (failure to parse
      // into code objects?)
      return theBlocks.size() == 0;
   }
   
   bool send(XDR *) const;

// Q: is it really necessary to still have this ifdef?
#ifdef _KERNINSTD_
   void parse(const moduleMgr &,
              function_t *,
              const pdvector<fnAddrBeingRelocated> &fnAddrsBeingRelocated,
                 // see the .I file for an explanation of this param
              bool verbose_fnParse);
      // creates an oracle (local vrble) and starts the parsing process

   void parseBlockIfNeeded(bbid_t bb_id,
                           const simplePath &,
                           codeObjectCreationOracle *);

   void appendToBasicBlock(bbid_t bb_id, unsigned byteOffsetWithinBB, codeObject *co);
   void endBasicBlock(bbid_t bb_id);
#endif

   const pdvector<basicBlockCodeObjects*> &getAllBlockCodeObjects() const {
      return theBlocks;
   }
   const basicBlockCodeObjects &getCodeObjectsForBlockById(bbid_t bbid) const {
      return *theBlocks[bbid];
   }

   pair<unsigned, unsigned>
   getCodeObjNdxWithinBBAndInsnByteOffsetWithinCodeObj(const function_t *,
                                                       bbid_t bbid,
                                                       unsigned insnByteOffsetWithinBB) const;
};

#endif
