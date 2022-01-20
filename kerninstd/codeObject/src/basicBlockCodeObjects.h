// basicBlockCodeObjects.h

#ifndef _BASIC_BLOCK_CODE_OBJECTS_H_
#define _BASIC_BLOCK_CODE_OBJECTS_H_

#include "common/h/Vector.h"
#include <utility>
using std::pair;

// fwd decls, instead of #include's, lead to much smaller .o files
// whenever templates are the least bit involved.
class codeObject;

class XDR;

class basicBlockCodeObjects {
 private:
   bool completed;
   pdvector< pair<unsigned, codeObject*> > theCodeObjects;
      // .first is the insn byte offset, within the basic block, where the
      // code object begins.  Of course, when we say insn byte offset, we mean
      // the offset in the *original* version of the code, ignoring whether the newly
      // emitted code objects will add extra insns, as they often must.

   class comparitor {
    public:
      bool operator()(unsigned byteOffsetWithinBB,
                      const pair<unsigned, codeObject*> &other) const {
         return byteOffsetWithinBB < other.first;
      }
   };
   
   // prevent copying:
   basicBlockCodeObjects &operator=(const basicBlockCodeObjects &);
   
 public:
   basicBlockCodeObjects();
   basicBlockCodeObjects(const basicBlockCodeObjects &); // yes this is needed
   basicBlockCodeObjects(XDR *);
  ~basicBlockCodeObjects();

   bool send(XDR *) const;

   void append(unsigned byteOffsetWithinBB, codeObject *co);
   void complete();
   bool isCompleted() const { return completed; }
   bool isEmpty() const {
      return theCodeObjects.size() == 0;
   }

   const pdvector< pair<unsigned, codeObject*> > &getCodeObjects() const {
      return theCodeObjects;
   }

   typedef pdvector< pair<unsigned, codeObject*> >::const_iterator const_iterator;
   const_iterator begin() const { return theCodeObjects.begin(); }
   const_iterator end()   const { return theCodeObjects.end(); }
   const pair<unsigned, codeObject*> &back() const {
      assert(theCodeObjects.size() > 0);
      return *(theCodeObjects.end()-1);
   }

   const_iterator findCodeObjAtInsnByteOffset(unsigned byteOffsetWithinBB,
                                              bool startOfCodeObjectOnly) const;
      // returns end() if not found
};

#endif
