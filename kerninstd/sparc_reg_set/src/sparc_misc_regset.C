// sparc_misc_regset.C

#include "sparc_misc_regset.h"
#include "util/h/popc.h"
#include "util/h/ffs.h"
#include "util/h/xdr_send_recv.h"
#include <stdlib.h> // random()

sparc_misc_regset::Raw sparc_misc_regset::raw;
sparc_misc_regset::Random sparc_misc_regset::random;
sparc_misc_regset::Empty sparc_misc_regset::empty;
sparc_misc_regset::Full sparc_misc_regset::full;

sparc_misc_regset::sparc_misc_regset(Random) {
   misc_stuff = (uint32_t)::random();
   ms.padding = 0;
}

sparc_misc_regset::sparc_misc_regset(XDR *xdr) {
   if (!P_xdr_recv(xdr, misc_stuff))
      throw xdr_recv_fail();
   assert(ms.padding == 0);
}

bool sparc_misc_regset::send(XDR *xdr) const {
   assert(ms.padding == 0);
   return P_xdr_send(xdr, misc_stuff);
}

sparc_misc_regset::sparc_misc_regset(const sparc_reg &r) {
   misc_stuff = 0; // for now...
       
   if (r.isIntCondCode()) {
      if (r.isIcc())
         ms.icc = 1;
      else {
         assert(r.isXcc());
         ms.xcc = 1;
      }
   }
   else if (r.isFloatCondCode()) {
      if (r.isFcc0())
         ms.fcc0 = 1;
      else if (r.isFcc1())
         ms.fcc1 = 1;
      else if (r.isFcc2())
         ms.fcc2 = 1;
      else if (r.isFcc3())
         ms.fcc3 = 1;
      else
         assert(false);
   }
   else if (r.isPC()) {
      ms.pc = 1;
   }
   else if (r.isGSR()) {
      ms.gsr = 1;
   }
   else if (r.isASI()) {
      ms.asi = 1;
   }
   else if (r.isPIL()) {
      ms.pil = 1;
   }
   else
      assert(false);
}

bool sparc_misc_regset::operator==(const sparc_reg &r) const {
   // compare misc stuff
   if (ari_popc(misc_stuff) != 1)
      return false;

   return exists(r);
}

unsigned sparc_misc_regset::count() const {
   assert(ms.padding == 0);
   return ari_popc(misc_stuff);
}

bool sparc_misc_regset::existsFcc(unsigned fccnum) const {
   switch (fccnum) {
      case 0:
         return existsFcc0();
      case 1:
         return existsFcc1();
      case 2:
         return existsFcc2();
      case 3:
         return existsFcc3();
      default:
         assert(false);
   }
}

bool sparc_misc_regset::exists(const sparc_reg &r) const {
   if (r.isIcc())
      return existsIcc();
   else if (r.isXcc())
      return existsXcc();
   else if (r.isFcc0())
      return existsFcc0();
   else if (r.isFcc1())
      return existsFcc1();
   else if (r.isFcc2())
      return existsFcc2();
   else if (r.isFcc3())
      return existsFcc3();
   else if (r.isPC())
      return existsPC();
   else if (r.isGSR())
      return existsGSR();
   else if (r.isASI())
      return existsASI();
   else if (r.isPIL())
      return existsPIL();
   else
      return false;
}

sparc_reg sparc_misc_regset::expand1() const {
   assert(ari_popc(misc_stuff) == 0);
   
   if (ms.icc)
      return sparc_reg::reg_icc;
   else if (ms.xcc)
      return sparc_reg::reg_xcc;
   else if (ms.fcc0)
      return sparc_reg::reg_fcc0;
   else if (ms.fcc1)
      return sparc_reg::reg_fcc1;
   else if (ms.fcc2)
      return sparc_reg::reg_fcc2;
   else if (ms.fcc3)
      return sparc_reg::reg_fcc3;
   else if (ms.pc)
      return sparc_reg::reg_pc;
   else if (ms.gsr)
      return sparc_reg::reg_gsr;
   else if (ms.asi)
      return sparc_reg::reg_asi;
   else if (ms.pil)
      return sparc_reg::reg_pil;
   else
      assert(false);
}
