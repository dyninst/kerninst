// power_misc_regset.C

#include "power_misc_regset.h"
#include "util/h/popc.h"
#include "util/h/ffs.h"
#include "util/h/xdr_send_recv.h"

#include <stdlib.h> // random()

power_misc_regset::Raw power_misc_regset::raw;
power_misc_regset::Random power_misc_regset::random;
power_misc_regset::Empty power_misc_regset::empty;
power_misc_regset::Full power_misc_regset::full;

power_misc_regset::power_misc_regset(Random) {
   misc_stuff = (uint32_t)::random();
   ms.padding = 0;
}

power_misc_regset::power_misc_regset(XDR *xdr) {
   if (!P_xdr_recv(xdr, misc_stuff))
      throw xdr_recv_fail();
   assert(ms.padding == 0);
}

bool power_misc_regset::send(XDR *xdr) const {
   assert(ms.padding == 0);
   return P_xdr_send(xdr, misc_stuff);
}

power_misc_regset::power_misc_regset(const power_reg &r) {
     //this is not nearly as bad as it looks.  While we have fields for
   //keeping track of nearly all possible registers, we are not likely 
   //to be going past the TbReg() case.
  
   misc_stuff = 0; // for now...

   if (r.isLinkReg())
      ms.lr = 1;
   else if (r.isCtrReg())
      ms.ctr = 1;
   else if (r.isSO())
      ms.so = 1;
   else if (r.isOV() )
      ms.ov = 1;
   else if (r.isCA() )
      ms.ca = 1;
   else if (r.isCrField()) 
      misc_stuff = 1ULL << ( r.getCrFieldNum() + 56);
   else if (r.isTbReg())
      misc_stuff = 1ULL << ( r.getTbrNum() + 16);
   else if (r.isXerResv() )
      ms.xerresv = 1;
   else if (r.isXerStrBytes() )
      ms.xerstrbytes = 1;
   else if (r.isFpscReg() )
      ms.fpscr = 1;
   else if (r.isAsReg() )
      ms.asr = 1;
   else if (r.isDaReg() )
      ms.dar = 1;
   else if (r.isDsisReg() )
      ms.dsisr = 1;
   else if (r.isMsReg() )
      ms.msr = 1;
   else if (r.isDecReg() )
      ms.dec = 1;
   else if (r.isPvReg() )
      ms.pvr = 1;
   else if (r.isSdr1Reg() )
      ms.sdr1 = 1;
   else if (r.isSrReg() )
      misc_stuff = 1ULL << (r.getSrrNum() + 25);
   else if (r.isEaReg() )
      ms.ear = 1;
   else if (r.isNullReg() )
      ms.null = 1;
   else if (r.isSprgReg() )
      misc_stuff = 1ULL << (r.getSprgNum() + 29);
   else if (r.isPmcReg() )
      misc_stuff = 1ULL << (r.getPmcNum() + 34);
   else if (r.isMmcraReg() ) 
      ms.mmcra = 1;
   else if (r.isMmcr0Reg() ) 
      ms.mmcr0 = 1;
   else if (r.isMmcr1Reg() ) 
      ms.mmcr1 = 1;
   else if (r.isSiaReg() ) 
      ms.siar = 1;
   else if (r.isSdaReg() ) 
      ms.sdar = 1;
   else if (r.isDabReg() ) 
      ms.dabr = 1;
   else if (r.isPiReg() ) 
      ms.pir = 1;
   else if (r.isHdecReg() ) 
      ms.hdec = 1;
   else if (r.isAccReg() ) 
      ms.accr = 1;
   else if (r.isCtrlReg() ) 
      ms.ctrl = 1;
   else
      assert(false);
   ms.padding = 0;
}

bool power_misc_regset::operator==(const power_reg &r) const {
   // compare misc stuff
   if (ari_popc(misc_stuff) != 1)
      return false;

   return exists(r);
}

unsigned power_misc_regset::count() const {
   assert(ms.padding == 0);
   return ari_popc(misc_stuff);
}


bool power_misc_regset::exists(const power_reg &r) const {
   //this is not nearly as bad as it looks.  While we have fields for
   //keeping track of nearly all possible registers, we are not likely 
   //to be going past the TbReg() case.

   if (r.isLinkReg())
      return existsLr();
   else if (r.isCtrReg())
      return existsCtr();
   else if (r.isSO())
      return existsSo();
   else if (r.isOV() )
      return existsOv();
   else if (r.isCA() )
      return existsCa();
   else if (r.isCrField()) 
      return existsCrField(r.getCrFieldNum() );
   else if (r.isTb())
      return existsTb();
   else if (r.isTbu() )
      return existsTbu();
   else if (r.isXerResv() )
      return existsXerResv();
   else if (r.isXerStrBytes() )
      return existsXerStrBytes();
   else if (r.isFpscReg() )
      return existsFpscr();
   else if (r.isAsReg() )
      return existsAsr();
   else if (r.isDaReg() )
      return existsDar();
   else if (r.isDsisReg() )
      return existsDsisr();
   else if (r.isMsReg() )
      return existsMsr();
   else if (r.isDecReg() )
      return existsDec();
   else if (r.isPvReg() )
      return existsPvr();
   else if (r.isSdr1Reg() )
      return existsSdr1();
   else if (r.isSrReg() )
      return existsSrr(r.getSrrNum() );
   else if (r.isEaReg() )
      return existsEar();
   else if (r.isNullReg() )
      return existsNull();
   else if (r.isSprgReg() )
      return existsSprg(r.getSprgNum());
   else if (r.isPmcReg() )
      return existsPmc(r.getPmcNum() );
   else if (r.isMmcraReg() )
      return existsMmcra();
   else if (r.isMmcr0Reg() )
      return existsMmcr0();
   else if (r.isMmcr1Reg() )
      return existsMmcr1();
   else if (r.isSiaReg() )
      return existsSiar();
   else if (r.isSdaReg() )
      return existsSdar();
   else if (r.isDabReg() )
      return existsDabr();
   else if (r.isPiReg() )
      return existsPir();
   else if (r.isHdecReg() )
      return existsHdec();
   else if (r.isAccReg() )
      return existsAccr();
   else if (r.isCtrlReg() )
      return existsCtrl();
   else
      assert(false);

   //gets rid of gcc warnings
   return false;
}

 

power_reg power_misc_regset::expand1() const {
   assert(ari_popc(misc_stuff) == 0);
   
   if (ms.lr)
      return power_reg::lr;
   else if (ms.ctr)
      return power_reg::ctr; 
   else if (ms.so)
      return power_reg::so;
   else if (ms.ov )
      return power_reg::ov;
   else if (ms.ca)
      return power_reg::ca;
   else if (ms.cr0) 
      return power_reg::cr0;
   else if (ms.cr1) 
      return power_reg::cr1;
    else if (ms.cr2) 
      return power_reg::cr2;
   else if (ms.cr3) 
      return power_reg::cr3;
   else if (ms.cr4) 
      return power_reg::cr4;
   else if (ms.cr5) 
      return power_reg::cr5;
    else if (ms.cr6) 
       return power_reg::cr6;
   else if (ms.cr7) 
      return power_reg::cr7;
   else if (ms.tb)
      return power_reg::tb;
   else if (ms.tbu)
      return power_reg::tbu;
   else if (ms.xerresv )
      return power_reg::xerresv;
   else if (ms.xerstrbytes )
      return power_reg::xerstrbytes;
   else if (ms.fpscr)
      return power_reg::fpscr;
   else if (ms.asr )
      return power_reg::asr;
   else if (ms.dar)
      return power_reg::dar;
   else if (ms.dsisr )
      return power_reg::dsisr;
   else if (ms.msr)
      return power_reg::msr;
   else if (ms.dec )
      return power_reg::dec;
   else if (ms.pvr )
      return power_reg::pvr;
   else if (ms.sdr1 )
      return power_reg::sdr1;
   else if (ms.srr0 )
      return power_reg::srr0;
   else if (ms.srr1)
      return power_reg::srr1;
   else if (ms.ear)
      return power_reg::ear;
   else if (ms.null)
      return power_reg::nullreg;
   //let's ignore the BAT and SPRG registers for now...
   else
      assert(false);
   
   //get rid of gcc warnings
   return power_reg::nullreg;
}

