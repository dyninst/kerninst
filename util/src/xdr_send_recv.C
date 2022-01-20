// xdr_send_recv.C

#include "util/h/xdr_send_recv.h"
#include <assert.h>

bool P_xdr_send(XDR *xdr, const StringPool &p) {
   unsigned num_bytes = p.getNumBytes();
   if (!P_xdr_send(xdr, num_bytes))
      return false;
   
   const char *raw_data = p.get_raw();
   char *raw_data_noconst = const_cast<char*>(raw_data);

   if (!xdr_bytes(xdr, &raw_data_noconst, &num_bytes, num_bytes))
      return false;
   
   return true;
}

bool P_xdr_recv(XDR *xdr, StringPool &p) {
   // format: num byte [unsigned], raw data [xdr_bytes]

   try {
      (void)new((void*)&p)StringPool(xdr);
   }
   catch(xdr_recv_fail) {
      return false;
   }

   return true;
}

bool read1_bool(XDR *xdr) {
   bool result;
   if (!P_xdr_recv(xdr, result))
      throw xdr_recv_fail();
   else
      return result;
}

#if !defined(i386_unknown_nt4_0)
uint16_t read1_uint16(XDR *xdr) {
   uint16_t result;
   if (!P_xdr_recv(xdr, result))
      throw xdr_recv_fail();
   else
      return result;
}
#endif

uint32_t read1_uint32(XDR *xdr) {
   uint32_t result;
   if (!P_xdr_recv(xdr, result))
      throw xdr_recv_fail();
   else
      return result;
}

unsigned read1_unsigned(XDR *xdr) {
   unsigned result;
   if (!P_xdr_recv(xdr, result))
      throw xdr_recv_fail();
   else
      return result;
}

pdstring read1_string(XDR *xdr) {
   pdstring result;
   result.~pdstring();

   if (!P_xdr_recv(xdr, result))
      throw xdr_recv_fail();

   return result;
}
