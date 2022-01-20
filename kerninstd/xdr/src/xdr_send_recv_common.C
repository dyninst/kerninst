// xdr_send_recv_common.C

#include "util/h/xdr_send_recv.h"
#include "xdr_send_recv_common.h"
#include "basicblock.h"
#include "bitwise_dataflow_fn.h"
#include "mbdf_window.h"
#include "funkshun.h"
#include "instr.h"
#include "insnVec.h"
#include "loadedModule.h"
#include "reg_set.h"
#include "relocatableCode.h"
#include "fnCodeObjects.h"
#include "basicBlockCodeObjects.h"

// Reminder: as always with (the new version of) igen, the second parameter 
// to a P_xdr_recv() will be a reference to allocated but uninitialized
// raw memory.  (Perhaps the allocation will be via new, via malloc,
// or even on the stack -- don't assume.)  Not even a default constructor
// will have been called on this variable.  So you must call the 
// constructor manually, using placement-new syntax.

//----------------------------------------------------------------------

bool P_xdr_send(XDR *xdr, const basicblock_t &bb) {
   return bb.send(xdr);
}

//----------------------------------------------------------------------

bool P_xdr_send(XDR *xdr, const trick_fn &tf) {
   const function_t *fn = tf.get();
   return fn->send(xdr);
}

bool P_xdr_recv(XDR *xdr, trick_fn &trick) {
   assert(xdr->x_op == XDR_DECODE);

   const function_t *fn = function_t::getFunction(xdr);
   assert(fn);
   
   (void)new((void*)&trick)trick_fn(fn);
   
   return true;
}

//----------------------------------------------------------------------

bool P_xdr_send(XDR *xdr, const instr_t &i) {
   return i.send(xdr);
}

bool P_xdr_recv(XDR *xdr, instr_t &i) {
   instr_t::putInstr(xdr, (void*)&i);
   return true;
}

// ---------------------------------------------------------------------------

bool P_xdr_send(XDR *xdr, const insnVec_t &iv) {
   return iv.send(xdr);
}

bool P_xdr_recv(XDR *xdr, insnVec_t &iv) {
   // as usual with P_xdr_recv(), iv is allocated but uninitialized raw memory
   insnVec_t::putInsnVec(xdr, &iv);
   return true;
}

// ---------------------------------------------------------------------------

bool P_xdr_send(XDR *xdr, const mbdf_window &w) {
   return w.send(xdr);
}

bool P_xdr_recv(XDR *xdr, mbdf_window &w) {
   mbdf_window *temp = mbdf_window::getWindow(xdr);
   w = *temp;
   delete temp;
   return true;
}

bool P_xdr_send(XDR *xdr, const monotone_bitwise_dataflow_fn &fn) {
   return fn.send(xdr);
}

bool P_xdr_recv(XDR *xdr, monotone_bitwise_dataflow_fn &fn) {
   try {
      monotone_bitwise_dataflow_fn *temp = monotone_bitwise_dataflow_fn::getDataflowFn(xdr);
      fn = *temp;
      delete temp;
   }
   catch (xdr_recv_fail) {
      return false;
   }
   return true;
}

bool P_xdr_send(XDR *xdr, const trick_mbdf &tm) {
   const monotone_bitwise_dataflow_fn *fn = tm.get();
   return fn->send(xdr);
}

bool P_xdr_recv(XDR *xdr, trick_mbdf &trick) {
   assert(xdr->x_op == XDR_DECODE);

   const monotone_bitwise_dataflow_fn *fn = 
      monotone_bitwise_dataflow_fn::getDataflowFn(xdr);
   assert(fn);
   
   (void)new((void*)&trick)trick_mbdf(fn);
   
   return true;
}

// ---------------------------------------------------------------------------

bool P_xdr_send(XDR *xdr, const loadedModule &mod) {
   // format:
   // module name, description, string pool (function names will use this)
   // each function

   if (!P_xdr_send(xdr, mod.getName()) || !P_xdr_send(xdr, mod.getDescription()))
      return false;

   const StringPool &theStringPool = mod.getStringPool();
   if (!P_xdr_send(xdr, theStringPool))
      return false;
   
   kptr_t TOCPtr = mod.getTOCPtr();
   if (!P_xdr_send(xdr, TOCPtr))
      return false;

   uint32_t num_fns = mod.numFns();
   if (!P_xdr_send(xdr, num_fns))
      return false;

   loadedModule::const_iterator iter = mod.begin();
   loadedModule::const_iterator finish = mod.end();
   for (; iter != finish; ++iter) {
      const function_t *fn = *iter;
      if (!fn->send(xdr))
         return false;
   }

   uint32_t checksum = 0x11111111;
   if (!P_xdr_send(xdr, checksum))
      return false;

   return true;
}

bool P_xdr_send(XDR *xdr, const trick_module &m) {
   const loadedModule *mod = m.get();
   return P_xdr_send(xdr, *mod);
}

bool P_xdr_recv(XDR *xdr, trick_module &trick) {
   assert(xdr->x_op == XDR_DECODE);

   const loadedModule *mod = new loadedModule(xdr);
   assert(mod);

   (void)new((void*)&trick)trick_module(mod);

   return true;
}

// ---------------------------------------------------------------------------

bool P_xdr_send(XDR *xdr, const trick_regset &tr) {
   const regset_t *rs = tr.get();
   assert(rs);
   return rs->send(xdr);
}

bool P_xdr_recv(XDR *xdr, trick_regset &trick) {
   assert(xdr->x_op == XDR_DECODE);

   const regset_t *rs = regset_t::getRegSet(xdr);
   assert(rs);
   
   (void)new((void*)&trick)trick_regset(rs);
   
   return true;
}


// ---------------------------------------------------------------------------
bool P_xdr_send(XDR *xdr, const relocatableCode_t &rc) {
   return rc.send(xdr);
}

bool P_xdr_recv(XDR *xdr, relocatableCode_t &rc) {
   relocatableCode_t::putRelocatableCode(xdr, rc);
   return true;
}

bool P_xdr_send(XDR *xdr, const trick_relocatableCode &tr) {
   const relocatableCode_t *rc = tr.get();
   return rc->send(xdr);
}

bool P_xdr_recv(XDR *xdr, trick_relocatableCode &trick) {
   assert(xdr->x_op == XDR_DECODE);

   const relocatableCode_t *rc = relocatableCode_t::getRelocatableCode(xdr);
   assert(rc);
   
   (void)new((void*)&trick)trick_relocatableCode(rc);
   
   return true;
}

// ---------------------------------------------------------------------------

bool P_xdr_send(XDR *xdr, const tempBufferEmitter::insnOffset &io) {
   return P_xdr_send(xdr, io.chunkNdx) && P_xdr_send(xdr, io.byteOffset);
}

bool P_xdr_recv(XDR *xdr, tempBufferEmitter::insnOffset &io) {
   unsigned chunk_ndx;
   unsigned byte_offset;
   if (!P_xdr_recv(xdr, chunk_ndx) || !P_xdr_recv(xdr, byte_offset))
      return false;
   
   (void)new((void*)&io)tempBufferEmitter::insnOffset(chunk_ndx, byte_offset);
   return true;
}

// ---------------------------------------------------------------------------

bool P_xdr_send(XDR *xdr, const fnCodeObjects &cobj) {
   return cobj.send(xdr);
}

bool P_xdr_recv(XDR *xdr, fnCodeObjects &cobj) {
   try {
      (void)new((void*)&cobj)fnCodeObjects(xdr);
   }
   catch (xdr_recv_fail) {
      return false;
   }
   return true;
}

// ---------------------------------------------------------------------------

bool P_xdr_send(XDR *xdr, const basicBlockCodeObjects &cobj) {
   return cobj.send(xdr);
}

bool P_xdr_recv(XDR *xdr, basicBlockCodeObjects &cobj) {
   try {
      (void)new((void*)&cobj)basicBlockCodeObjects(xdr);
   }
   catch (xdr_recv_fail) {
      return false;
   }
   
   return true;
}
