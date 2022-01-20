// insnVec.h

// A class that provides a vector of instructions, indexable by both
// (1) instruction byte offset and
// (2) instruction number
// in constant time.

#ifndef _INSN_VEC_H_
#define _INSN_VEC_H_

#include "common/h/Vector.h"
#include "instr.h"

// fwd decls avoid the need for #include's, which lead to code bloat:
struct XDR;

class insnVec_t {
 protected:
   pdvector<instr_t *> the_insns;
   unsigned num_bytes;

   virtual void raw_initialize(const void *raw_insn_bytes, 
			       unsigned total_num_bytes) = 0;
   
 public:
   insnVec_t() : num_bytes(0) {}
   insnVec_t(XDR *) : num_bytes(0) {}
   insnVec_t(const pdvector<instr_t *> &ithe_insns);

   // perhaps we should make the following ctor private, to ensure that it's
   // not used in practice.  After all, it's expensive, and would we ever 
   // really need to copy an insnVec?  The same applies to operator=().
   insnVec_t(const insnVec_t &src) : num_bytes(src.num_bytes) {
       pdvector<instr_t *>::const_iterator finish = src.the_insns.end();
       pdvector<instr_t *>::const_iterator i = src.the_insns.begin();
       for(; i != finish; ++i) {
          this->the_insns.push_back(instr_t::getInstr(**i));
       }
   }

   virtual ~insnVec_t();

   static insnVec_t* getInsnVec();
   static insnVec_t* getInsnVec(XDR *);
   static insnVec_t* getInsnVec(const insnVec_t &src);
   static insnVec_t* getInsnVec(const void *raw_insn_bytes, 
				unsigned total_num_bytes);

   static void putInsnVec(XDR *xdr, void *where);

   insnVec_t& operator=(const insnVec_t &src);

   virtual unsigned getMemUsage_exceptObjItself() const = 0;

   virtual void reserveBytesHint(unsigned numbytes) = 0;

   bool send(XDR *) const;

   void clear();
   
   void overwriteEnMasse(const void *raw_insn_bytes, unsigned total_num_bytes);
   virtual void *reserveForManualOverwriteEnMasse(unsigned total_num_bytes) = 0;

   void operator+=(const insnVec_t &other);

   void appendAnInsn(instr_t *i) {
      the_insns += i;
      num_bytes += i->getNumBytes();
   }

   virtual void shrinkToBytes(unsigned newNumBytes, 
			      bool aggressivelyFreeUpMemory) = 0;
      // aggressivelyFreeUpMemory: if true, we fry, reallocate, and copy
      // if false, we just shrink (calling vector's shrink()), which
      // doesn't actually free any memory!
   
   unsigned numInsns() const { return the_insns.size(); }
   unsigned numInsnBytes() const { return num_bytes; }

   virtual unsigned insnNdx2ByteOffset(unsigned ndx, 
				       bool enforce_range=true) const = 0;
   virtual unsigned byteOffset2InsnNdx(unsigned byteoffset) const = 0;
 
   // NOTE: we do not define the convenient operator[] on purpose, because 
   // it would have to be either by instruction index or by byte offset, 
   // and before long someone would confuse the two and use operator[] 
   // incorrectly.

   instr_t *get_by_ndx(unsigned ndx) const { return the_insns[ndx]; }

   virtual instr_t *get_by_byteoffset(unsigned byteoffset) const = 0;

   virtual unsigned offsetOfInsnContaining(unsigned byteoffset) const = 0;


   // Iteration, STL style: define subclasses const_iterator and iterator.
   typedef pdvector<instr_t *>::const_iterator const_iterator;
   typedef pdvector<instr_t *>::iterator iterator;

   const_iterator begin() const { return the_insns.begin(); }
   iterator begin() { return the_insns.begin(); }
   const_iterator end() const { return the_insns.end(); }
   iterator end() { return the_insns.end(); }
};

#endif
