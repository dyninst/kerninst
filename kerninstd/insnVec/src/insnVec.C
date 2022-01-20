// insnVec.C

#include "insnVec.h"
#include "util/h/xdr_send_recv.h"
#include "xdr_send_recv_common.h"

#ifdef sparc_sun_solaris2_7
#include "sparc_insnVec.h"
#elif defined(i386_unknown_linux2_4)
#include "x86_insnVec.h"
#elif defined(rs6000_ibm_aix5_1)
#include "power_insnVec.h"
#endif


insnVec_t::insnVec_t(const pdvector<instr_t *> &ithe_insns) : num_bytes(0)
{
   pdvector<instr_t *>::const_iterator finish = ithe_insns.end();
   pdvector<instr_t *>::const_iterator i = ithe_insns.begin();
   for(; i != finish; ++i) {
      the_insns.push_back(instr_t::getInstr(**i));
      num_bytes += (*i)->getNumBytes();
   }
}

insnVec_t::~insnVec_t() {
   iterator finish = end();
   iterator i = begin();
   for(; i != finish; ++i) {
      // delete the individual instructions in the insnVec
     delete(*i);
   }
   num_bytes = 0;
}

insnVec_t* insnVec_t::getInsnVec() {
   insnVec_t *result;
#ifdef sparc_sun_solaris2_7
   result = new sparc_insnVec();
#elif defined(i386_unknown_linux2_4)
   result = new x86_insnVec();
#elif defined(rs6000_ibm_aix5_1)
   result = new power_insnVec();
#endif
   return result;
}

insnVec_t* insnVec_t::getInsnVec(XDR *xdr) {
   insnVec_t *result;
#ifdef sparc_sun_solaris2_7
   result = new sparc_insnVec(xdr);
#elif defined(i386_unknown_linux2_4)
   result = new x86_insnVec(xdr);
#elif defined(rs6000_ibm_aix5_1)
   result = new power_insnVec(xdr);
#endif
   return result;
}

insnVec_t* insnVec_t::getInsnVec(const insnVec_t &src) {
   insnVec_t *result;
#ifdef sparc_sun_solaris2_7
   result = new sparc_insnVec(src);
#elif defined(i386_unknown_linux2_4)
   result = new x86_insnVec(src);
#elif defined(rs6000_ibm_aix5_1)
   result = new power_insnVec(src);
#endif
   return result;
}

insnVec_t* insnVec_t::getInsnVec(const void *raw_insn_bytes, 
				 unsigned total_num_bytes) {
   insnVec_t *result;
#ifdef sparc_sun_solaris2_7
   result = new sparc_insnVec(raw_insn_bytes, total_num_bytes);
#elif defined(i386_unknown_linux2_4)
   result = new x86_insnVec(raw_insn_bytes, total_num_bytes);
#elif defined(rs6000_ibm_aix5_1)
   result = new power_insnVec(raw_insn_bytes, total_num_bytes);
#endif
   return result;
}

void insnVec_t::putInsnVec(XDR *xdr, void *where) {
#ifdef sparc_sun_solaris2_7
   (void)new(where) sparc_insnVec(xdr);
#elif defined(i386_unknown_linux2_4)
   (void)new(where) x86_insnVec(xdr);
#elif defined(rs6000_ibm_aix5_1)
   (void)new(where) power_insnVec(xdr);
#endif
}

insnVec_t& insnVec_t::operator=(const insnVec_t &src) {
   pdvector<instr_t *>::const_iterator finish = src.the_insns.end();
   pdvector<instr_t *>::const_iterator i = src.the_insns.begin();
   for(; i != finish; ++i) {
      the_insns.push_back(instr_t::getInstr(**i));
   }
   num_bytes = src.num_bytes;
   return *this;
}

void insnVec_t::clear() {
   the_insns.clear();
   num_bytes = 0;
}

void insnVec_t::overwriteEnMasse(const void *raw_insn_bytes,
			       unsigned total_num_bytes) {
   // erases old contents (if any) and initializes with above stuff.
   // Rather similar to the ctor with the same arguments...no coincidence that
   // they share much code.

   the_insns.zap(); // clear() is insufficient (see comment in Vector.h itself)
   raw_initialize(raw_insn_bytes, total_num_bytes);
}


void insnVec_t::operator+=(const insnVec_t &other) {
   the_insns += other.the_insns;
}

bool insnVec_t::send(XDR *xdr) const {
 // Send number of instructions
   if (!P_xdr_send(xdr, the_insns.size()))
      return false;

   // Send each individual instruction.
   pdvector<instr_t *>::const_iterator iter = the_insns.begin();
   pdvector<instr_t *>::const_iterator finish = the_insns.end();
   for (; iter != finish; ++iter) {
      if (!P_xdr_send(xdr, **iter))
         throw xdr_send_fail();
   }
   
   return true;
}
