// power_instr.C
// Igor Grobman,  based on sparc_instr.C by Ariel Tamches and
// also based on Paradyn power parsing code.

#include <iostream> // cerr
#include <assert.h>
#include <stdio.h> // sprintf()
#include "power_instr.h"
#include "out_streams.h"
#include "util/h/rope-utils.h" // num2string()
#include <math.h>





power_instr::power_cfi::power_cfi() {
#ifdef PURE_BUILD
   power_bit_stuff = 0;
      // avoids purify UMC's, which would be present otherwise due to
      // the read-modify-write nature of bit field operations.
#endif
}

// ----------------------------------------------------------------------

// define the static member vrbles (these are meant to be passed in as the
// first parameter to one of the power_instr constructors; making them each
// of a different type instead of one enumerated type really aids error
// checking.)

  
power_instr::Trap power_instr::trap; 
power_instr::Sync power_instr::sync;
power_instr::Nop power_instr::nop;
  
power_instr::Mul power_instr::mul;
power_instr::Abs power_instr::abs;
power_instr::DifferenceOrZero power_instr::differenceorzero;
power_instr::Div power_instr::div;   
power_instr::AddSub power_instr::addsub;
power_instr::Cmp power_instr::cmp;
power_instr::Neg power_instr::neg;
power_instr::LogicAnd power_instr::logicand;
power_instr::LogicOr power_instr::logicor;
power_instr::LogicXor power_instr::logicxor;
power_instr::Nand power_instr::nand;
power_instr::Nor power_instr::nor;
power_instr::Eqv power_instr::eqv;
power_instr::MaskGen power_instr::maskgen;
power_instr::MaskInsertFromReg power_instr::maskinsertfromreg;

power_instr::Exts power_instr::exts;
power_instr::Cntlz power_instr::cntlz;
   
 
power_instr::Load power_instr::load;
power_instr::Store power_instr::store;

power_instr::Shift power_instr::shift;
power_instr::Rotate power_instr::rotate;
   
//special register moves
power_instr::MoveToSPR power_instr::movetospr;
power_instr::MoveFromSPR power_instr::movefromspr;
power_instr::MoveToCondRegFlds power_instr::movetocondregflds;
power_instr::MoveToCondRegFromXER power_instr::movetocondregfromxer;
power_instr::MoveFromCondReg power_instr::movefromcondreg;
power_instr::MoveToMachineStateReg power_instr::movetomachinestatereg;
power_instr::MoveFromMachineStateReg power_instr::movefrommachinestatereg;


//Branches
power_instr::Branch power_instr::branch;
power_instr::BranchCond power_instr::branchcond;
power_instr::BranchCondLinkReg power_instr::branchcondlinkreg;
power_instr::BranchCondCountReg power_instr::branchcondcountreg;
power_instr::Sc power_instr::sc;

power_instr::CondReg power_instr::condreg;  //condition register modifications
power_instr::CondRegField power_instr::condregfield;   

//Floating point
power_instr::FPLoad power_instr::fpload;
power_instr::FPStore power_instr::fpstore;
power_instr::FPMove power_instr::fpmove;
power_instr::FPArithmetic power_instr::fparithmetic;
power_instr::FPConversion power_instr::fpconversion;  
power_instr::FPCompareUnordered power_instr::fpcompareunordered;
power_instr::FPCompareOrdered power_instr::fpcompareordered;
power_instr::MoveFromFPSCR power_instr::movefromfpscr;
power_instr::MoveToCondRegFromFPSCR power_instr::movetocondregfromfpscr;
power_instr::MoveToFPSCRFieldImm power_instr::movetofpscrfieldimm;
power_instr::MoveToFPSCRFields power_instr::movetofpscrfields;
power_instr::MoveToFPSCRBit0 power_instr::movetofpscrbit0;
power_instr::MoveToFPSCRBit1 power_instr::movetofpscrbit1;

power_instr::Rfi power_instr::rfi;
   
//TLB ops
power_instr::TLBInvalidateEntry power_instr::tlbinvalidateentry;
power_instr::TLBInvalidateAll power_instr::tlbinvalidateall;
power_instr::TLBSync power_instr::tlbsync;

//Cache Management
power_instr::ICacheBlockInvalidate power_instr::icacheblockinvalidate;
power_instr::InstructionSync power_instr::instructionsync;
power_instr::DCacheBlockInvalidate power_instr::dcacheblockinvalidate;
power_instr::DCacheBlockTouch power_instr::dcacheblocktouch;
power_instr::DCacheBlockTouchStore power_instr::dcacheblocktouchstore;
power_instr::DCacheBlockSetZero power_instr::dcacheblocksetzero;
power_instr::DCacheBlockStore power_instr::dcacheblockstore;
power_instr::DCacheBlockFlush power_instr::dcacheblockflush;
power_instr::CacheLineComputeSize power_instr::cachelinecomputesize;
power_instr::CacheLineFlush power_instr::cachelineflush;
power_instr::CacheLineInvalidate power_instr::cachelineinvalidate;

power_instr::EnforceInOrderExecutionIO power_instr::enforceinorderexecutionio;
power_instr::MoveFromTimeBase power_instr::movefromtimebase;



//----------------------------------------------------------------------

//This should be better (std::reverse ?)
unsigned   power_instr::reverse5BitHalves (unsigned int orig) const {
   unsigned int rev = (orig>>5);
   orig <<= 5;
   orig &= 0x000003E0;
   rev |= orig;
   return rev;
}


void power_instr::set_iform(uimmediate<6> opcd, simmediate<24> li, 
               uimmediate<1> aa, uimmediate<1> lk) {
   iform.op = opcd;
   iform.li = li;
   iform.aa = aa;
   iform.lk = lk;
}

void power_instr::set_bform(uimmediate<6> opcd, uimmediate<5> bo, 
                            uimmediate<5> bi, simmediate<14> bd,
                            uimmediate<1> aa, uimmediate<1> lk) {
   bform.op = opcd;
   bform.bo = bo;
   bform.bi = bi;
   bform.bd = bd;
   bform.aa = aa;
   bform.lk = lk;
}

   
void power_instr::set_dform(uimmediate<6> opcd, power_reg rt, power_reg ra, 
                            uimmediate<16> d) {
   
   dform.op = opcd;
   dform.rt = rt.getIntOrFloatNum();
   dform.ra = ra.getIntOrFloatNum();
   dform.d_or_si = d;
}

void power_instr::set_dform(uimmediate<6> opcd, power_reg rt, power_reg ra, 
                            simmediate<16> si) {
   
   dform.op = opcd;
   dform.rt = rt.getIntOrFloatNum();
   dform.ra = ra.getIntOrFloatNum();
   dform.d_or_si = si;
}


void power_instr::set_dform(uimmediate<6> opcd, uimmediate<5> to, 
                             power_reg ra, simmediate<16> si) {
    dform.op = opcd;
    dform.rt = to;
    dform.ra = ra.getIntOrFloatNum();
    dform.d_or_si = si;
}

void power_instr::set_dform(uimmediate<6> opcd, uimmediate<3> bf, 
                            uimmediate<1> l, 
                            power_reg ra, uimmediate<16> d) {
   dform.op = opcd;
   
   //insert the bit flag stuff
   unsigned tmp = bf;
   tmp <<= 2;
   tmp |= l;
   dform.rt = tmp;
   
   dform.ra = ra.getIntOrFloatNum();
   dform.d_or_si = d;
}

void power_instr::set_dform(uimmediate<6> opcd, uimmediate<3> bf, 
                            uimmediate<1> l, 
                            power_reg ra, simmediate<16> si) {
   dform.op = opcd;
   
   //insert the bit flag stuff
   unsigned tmp = bf;
   tmp <<= 2;
   tmp |= l;
   dform.rt = tmp;
   
   dform.ra = ra.getIntOrFloatNum();
   dform.d_or_si = si;
}


void power_instr::set_dsform(uimmediate<6> opcd, power_reg rt, power_reg ra, 
                             simmediate<14> ds, uimmediate<2> xo ) {
   dsform.op = opcd;
   dsform.rt = rt.getIntNum();
   dsform.ra = ra.getIntNum();
   dsform.ds = ds;
   dsform.xo = xo;
}

void power_instr::set_xform(uimmediate<6> opcd, power_reg rt, power_reg ra, 
                        power_reg rb, uimmediate<10> xo, uimmediate<1> rc) {
    xform.op = opcd;
    xform.rt = rt.getIntOrFloatNum();
    xform.ra = ra.getIntOrFloatNum();
    xform.rb = rb.getIntOrFloatNum();
    xform.xo = xo;
    xform.rc = rc;
 }





void power_instr::set_xform(uimmediate<6> opcd, power_reg rt, power_reg ra, 
               uimmediate<5> nb_or_sh, uimmediate<10> xo, uimmediate<1> rc) {
   
   xform.op = opcd;
   xform.rt = rt.getIntOrFloatNum();
   xform.ra = ra.getIntOrFloatNum();
   xform.rb = nb_or_sh;
   xform.xo = xo;
   xform.rc = rc;
}

void power_instr::set_xform(uimmediate<6> opcd, uimmediate<3> bf, 
                            uimmediate<1> l, power_reg ra, power_reg rb, 
                            uimmediate<10> xo, uimmediate<1> rc) {
   xform.op = opcd;
   
   //insert the bit flag stuff
   unsigned tmp = bf;
   tmp <<= 2;
   tmp |= l;
   xform.rt = tmp;

   xform.ra = ra.getIntOrFloatNum();
   xform.rb = rb.getIntOrFloatNum();
   xform.xo = xo;
   xform.rc = rc;
}

void power_instr::set_xform(uimmediate<6> opcd, uimmediate<3> bf, 
                            uimmediate<3> bfa,  
                            uimmediate<10> xo, uimmediate<1> rc) {
   xform.op = opcd;
   
   //insert the bit flag stuff
   xform.rt = bf;
   xform.rt <<= 2;
   xform.ra = bfa;
   xform.ra <<=2;

   xform.rb = 0;
   xform.xo = xo;
   xform.rc = rc;
}


void power_instr::set_xform(uimmediate<6> opcd, uimmediate<3> bf, 
                            uimmediate<4> u, uimmediate<10> xo, 
                            uimmediate<1> rc) {
   xform.op = opcd;
   
   //insert the bit flag stuff
   xform.rt = bf;
   xform.rt <<= 2;
   
   xform.ra = 0;
   xform.rb = u;
   xform.rb <<= 1;
   xform.xo = xo;
   xform.rc = rc;
}


void power_instr::set_xform(uimmediate<6> opcd, uimmediate<5> to_or_bt, 
                            power_reg ra, power_reg rb, uimmediate<10> xo, 
                            uimmediate<1> rc) {
   
   xform.op = opcd;
   xform.rt = to_or_bt;
   xform.ra = ra.getIntOrFloatNum();
   xform.rb = rb.getIntOrFloatNum();
   xform.xo = xo;
   xform.rc = rc;
}

void power_instr::set_xlform(uimmediate<6> opcd, uimmediate<5> bt_or_bo, 
                             uimmediate<5> ba_or_bi, uimmediate<5> bb, 
                             uimmediate<10> xo, uimmediate<1> lk) {
   
   xlform.op = opcd;
   xlform.bt = bt_or_bo;
   xlform.ba = ba_or_bi;
   xlform.bb = bb;
   xlform.xo = xo;
   xlform.lk = lk;
}

void power_instr::set_xlform(uimmediate<6> opcd, uimmediate<3> bf, 
                uimmediate<3> bfa, uimmediate<10> xo) {
   
   xlform.op = opcd;
   xlform.bt = bf;
   xlform.bt <<=2;
   xlform.ba = bfa;
   xlform.ba <<=2;
   xlform.bb = 0;
   xlform.xo = xo;
   xlform.lk = 0;
}

void power_instr::set_xfxform(bool tbr, uimmediate<6> opcd, power_reg rt, 
                              power_reg spr, 
                              uimmediate<10> xo) {
   xfxform.op = opcd;
   xfxform.rt = rt.getIntNum();
   if (tbr)
      xfxform.spr = reverse5BitHalves(spr.getTbrEncoding());
   else
      xfxform.spr = reverse5BitHalves(spr.getSprEncoding());
   xfxform.xo = xo;
   xfxform.rc = 0;
}





void power_instr::set_xfxform(uimmediate<6> opcd, power_reg rt, 
                              uimmediate<8> fxm, uimmediate<10> xo) {
   xfxform.op = opcd;
   xfxform.rt = rt.getIntNum();
   
   unsigned tmp = 0;
   tmp = 0;
   tmp |= fxm;
   tmp <<= 1;
   xfxform.spr = tmp;
   
   xfxform.xo = xo;
   xfxform.rc = 0;
}


void power_instr::set_xflform(uimmediate<6> opcd, uimmediate<8> flm, 
                              power_reg frb, uimmediate<10> xo, 
                              uimmediate<1> rc) {
   xflform.op = opcd;
   xflform.u1=0;
   xflform.flm=flm;
   xflform.u2=0;
   xflform.frb = frb.getFloatNum();
   xflform.xo = xo;
   xflform.rc = rc;
}




void power_instr::set_xsform(uimmediate<6> opcd, power_reg rs, power_reg ra, 
                             uimmediate<6> sh, uimmediate<9> xo, 
                             uimmediate<1> rc) {
   
   xsform.op = opcd;
   xsform.rs = rs.getIntNum();
   xsform.ra = ra.getIntNum();

   unsigned tmp = 0;
   tmp = 0;
   tmp |=  sh;
   xsform.sh1 |= tmp;
   
   xsform.xo = xo;
   
   tmp = sh;
   tmp >>= 5;
   xsform.sh2=tmp;
   xsform.rc = rc;
}


void power_instr::set_xoform(uimmediate<6> opcd, power_reg rt, power_reg ra, 
                             power_reg rb, uimmediate<1> oe,  
                             uimmediate<9> xo, uimmediate<1> rc) {
   xoform.op = opcd;
   xoform.rt = rt.getIntNum();
   xoform.ra = ra.getIntNum();
   xoform.rb = rb.getIntNum();
   xoform.oe = oe;
   xoform.xo = xo;
   xoform.rc = rc;

}
void power_instr::set_aform(uimmediate<6> opcd, power_reg frt, power_reg fra, 
               power_reg frb, power_reg frc, uimmediate<5> xo, 
               uimmediate<1> rc) {
   aform.op = opcd;
   aform.frt = frt.getFloatNum();
   aform.fra = fra.getFloatNum();
   aform.frb = frb.getFloatNum();
   aform.frc = frc.getFloatNum();
   aform.xo = xo;
   aform.rc = rc;
}


void power_instr::set_mform(uimmediate<6> opcd, power_reg rs, power_reg ra, 
                            power_reg rb, uimmediate<5> mb, uimmediate<5> me,
                            uimmediate<1> rc) {
   mform.op = opcd;
   mform.rs = rs.getIntNum();
   mform.ra = ra.getIntNum();
   mform.sh = rb.getIntNum();
   mform.mb = mb;
   mform.me = me;
   mform.rc = rc;
}


void power_instr::set_mform(uimmediate<6> opcd, power_reg rs, power_reg ra, 
               uimmediate<5> sh, uimmediate<5> mb, uimmediate<5> me,
               uimmediate<1> rc) {
   
   mform.op = opcd;
   mform.rs = rs.getIntNum();
   mform.ra = ra.getIntNum();
   mform.sh = sh;
   mform.mb = mb;
   mform.me = me;
   mform.rc = rc;
}
void power_instr::set_mdform(uimmediate<6> opcd, power_reg rs, power_reg ra, 
                             uimmediate<6> sh, uimmediate<6> mb_or_me, 
                             uimmediate<3> xo,  
                             uimmediate<1> rc) {
    
   mdform.op = opcd;
   mdform.rs = rs.getIntNum();
   mdform.ra = ra.getIntNum();
   
   unsigned tmp = 0;
   tmp = 0;
   tmp |=  sh;
   mdform.sh1 = tmp;
   tmp = sh;
   tmp >>= 5;
   mdform.sh2=tmp;

   //mb_or_me is actually executed with mb being  mb(5)||mb(0:4) 
   //bizzarre compatibility requirements??
   mdform.mb_or_me = (mb_or_me<<1) | (mb_or_me>>5);
   mdform.xo = xo;
   mdform.rc = rc;
}


void power_instr::set_mdsform(uimmediate<6> opcd, power_reg rs, power_reg ra, 
                 power_reg rb, uimmediate<6> mb_or_me, uimmediate<4> xo,
                 uimmediate<1> rc) {
   mdsform.op = opcd;
   mdsform.rs = rs.getIntNum();
   mdsform.ra = ra.getIntNum();
   mdsform.rb = rb.getIntNum();
   mdsform.mb_or_me = mb_or_me;
   mdsform.xo = xo;
   mdsform.rc = rc;
}   


// ----------------------------------------------------------------------
// ----------------------- Making Instructions --------------------------
// ----------------------------------------------------------------------

power_instr::power_instr(Trap, ArgSize size, uimmediate<5> to, 
                         power_reg ra, simmediate<16> si): instr_t() {
   uimmediate<6> opcd(0);
   if (size == doubleWord )
      opcd = 2;
   else 
      opcd = 3;
   set_dform(opcd, to, ra, si);
}




power_instr::power_instr(Trap, ArgSize size, uimmediate<5> to, 
                         power_reg ra, power_reg rb) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<1> rc = 0;
   uimmediate<10> xo(0);
   if (size == doubleWord )
      xo = 68;
   else 
      xo = 4;

   set_xform(opcd, to, ra, rb, xo, rc);

}
   
power_instr::power_instr(Sync) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<1> rc = 0;
   uimmediate<10> xo=598;

   //gpr0 produces zeroes in the field, which is what we want
   power_reg r(power_reg::gpr0);

   set_xform(opcd, r, r, r, xo, rc);
}

power_instr::power_instr(Nop) : instr_t() {
 
   uimmediate<6> opcd = 24;
   power_reg r(power_reg::gpr0);
   uimmediate<16>ui = 0;

   //preferred no-op is ori 0,0,0
   set_dform(opcd, r, r, ui);
}

power_instr::power_instr(Abs, power_reg rt, power_reg ra,  uimmediate<1> oe, 
            uimmediate<1> rc) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<9> xo = 360;
   power_reg rb(power_reg::gpr0);
   
   set_xoform(opcd, rt, ra, rb, oe, xo, rc);
}

power_instr::power_instr(DifferenceOrZero, power_reg rt, power_reg ra,  
                        power_reg rb, uimmediate<1> oe, uimmediate<1> rc) :
			instr_t () {
   uimmediate<6> opcd = 31;
   uimmediate<9> xo = 264;
   
   set_xoform(opcd, rt, ra, rb, oe, xo, rc);
}
power_instr::power_instr(DifferenceOrZero, power_reg rt, 
            power_reg ra, simmediate<16> si) : instr_t() {
   
   uimmediate<6> opcd(9);
  
   set_dform(opcd, rt, ra, si);

}



power_instr::power_instr(Mul, MulHiLo hilo, power_reg rt, 
            power_reg ra, simmediate<16> si) : instr_t() {
   
   uimmediate<6> opcd(0);

   //D-form deals only with low-order bits
   assert(hilo == low);
   
   opcd=7;
  
   set_dform(opcd, rt, ra, si);

}


power_instr::power_instr(Mul, ArgSize size, MulHiLo hilo, bool isSigned, 
                         power_reg rt, 
                         power_reg ra, power_reg rb, 
                         uimmediate<1> oe, uimmediate<1> rc) : instr_t() {
   uimmediate<6> opcd = 31;
   
   uimmediate<9> xo(0);

   if (hilo == high) {
      assert(oe==0); //oe is reserved in this case
      if (size == word) {
         if (isSigned) 
            xo = 75;
         else
            xo = 11;
      } else { //size == double
         if (isSigned)
            xo = 73;
         else
            xo = 11;
      }
   } else { //hilo == low
      if (size == word)
         xo = 235;
      else
         xo = 233;
   }

   set_xoform(opcd, rt, ra, rb, oe, xo, rc);
}


  
power_instr::power_instr(Div, ArgSize size, bool isSigned,  power_reg rt, 
                         power_reg ra, power_reg rb, uimmediate<1> oe, 
                         uimmediate<1> rc) : instr_t (){
     
   uimmediate<6> opcd = 31;
   
   uimmediate<9> xo(0);

   if (size == word) {
      if (isSigned) 
         xo = 491;
      else
         xo = 459;
   } else { //size == doubleWord
      if (isSigned)
         xo = 489;
      else
         xo = 457;
   }


   set_xoform(opcd, rt, ra, rb, oe, xo, rc);
}


power_instr::power_instr(AddSub, AddSubOpsD op, power_reg rt, 
            power_reg ra, simmediate<16> si) : instr_t () {
   
   uimmediate<6> opcd = op;
   set_dform(opcd, rt, ra, si);
}

power_instr::power_instr(AddSub, AddSubOpsXO op, power_reg rt,
            power_reg ra, power_reg rb, uimmediate<1> oe, uimmediate<1> rc):
	    instr_t () {
   
   uimmediate<6> opcd = 31;
   uimmediate<9> xo = op;

   set_xoform(opcd, rt, ra, rb, oe, xo, rc);
}



power_instr::power_instr(Neg, power_reg rt, power_reg ra,  uimmediate<1> oe, 
            uimmediate<1> rc) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<9> xo = 104;
   power_reg rb(power_reg::gpr0);
   
   set_xoform(opcd, rt, ra, rb, oe, xo, rc);
}
   
power_instr::power_instr(Cmp, bool logical, uimmediate<3> bf, uimmediate<1> l, 
            power_reg ra, power_reg rb) : instr_t() {
   uimmediate<6> opcd=31;
   uimmediate<10> xo(0);
   uimmediate<1> rc =0;

   if (logical) 
      xo = 32;
   else
      xo = 0;
   
   set_xform(opcd, bf, l, ra, rb, xo, rc);
}
   
power_instr::power_instr(Cmp, bool logical, uimmediate<3> bf, uimmediate<1> l, 
           power_reg ra, simmediate<16> si ) : instr_t() {
   uimmediate<6> opcd(0);

   if (logical)
      opcd = 10;
   else
      opcd = 11;
   
   set_dform(opcd, bf, l, ra, si);
}




power_instr::power_instr(LogicAnd, bool shifted, power_reg ra, power_reg rs, 
                         uimmediate<16> ui) : instr_t()
{ 
   uimmediate<6> opcd(0);

   if (shifted)
      opcd = 29;
   else
      opcd = 28;
   
   set_dform(opcd, rs, ra, ui);   
}



power_instr::power_instr(LogicOr, bool shifted, power_reg ra, power_reg rs, 
                         uimmediate<16> ui) : instr_t() {
   
   uimmediate<6> opcd(0);

   if (shifted)
      opcd = 25;
   else
      opcd = 24;
   
   set_dform(opcd, rs, ra, ui);   
}



power_instr::power_instr(LogicXor, bool shifted, power_reg ra, power_reg rs,
                         uimmediate<16> ui) : instr_t() {
   uimmediate<6> opcd(0);

   if (shifted)
      opcd = 27;
   else
      opcd = 26;
   
   set_dform(opcd, rs, ra, ui);   
}



power_instr::power_instr(LogicAnd, bool complement,  power_reg ra, 
                         power_reg rs, power_reg rb, uimmediate<1> rc) : instr_t(){
   
   uimmediate<6> opcd=31;
   uimmediate<10> xo(0);

   if (complement) 
      xo = 60;
   else
      xo = 28;
   
   set_xform(opcd, rs, ra, rb, xo, rc);

}


power_instr::power_instr(LogicOr, bool complement,  power_reg ra, 
                         power_reg rs, power_reg rb, uimmediate<1> rc) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo(0);

   if (complement) 
      xo = 412;
   else
      xo = 444;
   
   set_xform(opcd, rs, ra, rb, xo, rc);
}
   

power_instr::power_instr(LogicXor,  power_reg ra, power_reg rs, power_reg rb,
            uimmediate<1> rc) : instr_t () {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = 316;
   set_xform(opcd, rs, ra, rb, xo, rc);
}

power_instr::power_instr(Nand,  power_reg ra, power_reg rs, power_reg rb,
            uimmediate<1> rc) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = 476;
   set_xform(opcd, rs, ra, rb, xo, rc);
}

power_instr::power_instr(Nor,  power_reg ra, power_reg rs, power_reg rb,
            uimmediate<1> rc) : instr_t () {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = 124;
   set_xform(opcd, rs, ra, rb, xo, rc);
}
   
power_instr::power_instr(Eqv,  power_reg ra, power_reg rs, power_reg rb,
            uimmediate<1> rc) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = 284;
   set_xform(opcd, rs, ra, rb, xo, rc);
}

power_instr::power_instr(MaskGen,  power_reg ra, power_reg rs, power_reg rb,
            uimmediate<1> rc) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = 29;
   set_xform(opcd, rs, ra, rb, xo, rc);
}

power_instr::power_instr(MaskInsertFromReg,  power_reg ra, power_reg rs, 
                         power_reg rb, uimmediate<1> rc) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = 541;
   set_xform(opcd, rs, ra, rb, xo, rc);
}



power_instr::power_instr(Exts, ExtsOps op, power_reg ra, 
                         power_reg rs, uimmediate<1> rc) : instr_t (){
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = op;
   power_reg r(power_reg::gpr0);

   set_xform(opcd, rs, ra, r, xo, rc);
}

power_instr::power_instr(Cntlz, ArgSize size, power_reg ra, 
                         power_reg rs, uimmediate<1> rc) : instr_t() {
   
  uimmediate<6> opcd = 31;
  uimmediate<10> xo(0);
  power_reg r(power_reg::gpr0);

  if (size == doubleWord )
     xo = 58;
  else //word
     xo = 26;
  

   set_xform(opcd, rs, ra, r, xo, rc);
}

power_instr::power_instr(Load, LoadOpsD op, power_reg rt, 
                         power_reg ra, simmediate<16> d) : instr_t() {

   uimmediate<6> opcd = op;
   set_dform(opcd, rt, ra, d);
}
power_instr::power_instr(Load, LoadOpsDS op, power_reg rt, 
                         power_reg ra, signed disp) : instr_t() {
   uimmediate<6> opcd = 58;
   uimmediate<2> xo = op;
   simmediate<14> d = disp/4;
   //note disp/4.  This is because DS is multiplied by 4 in order to achieve
   //doubleword alignment.
   set_dsform(opcd, rt, ra, d, xo);
}

power_instr::power_instr(Load, LoadOpsX op, power_reg rt, power_reg ra, 
                         power_reg rb) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = op;
   uimmediate<1> rc = 0;

   set_xform(opcd, rt, ra, rb, xo, rc);
}

power_instr::power_instr(Store, StoreOpsD op, power_reg rt, 
                         power_reg ra, simmediate<16> d) : instr_t() {
   uimmediate<6> opcd = op;
   set_dform(opcd, rt, ra, d);   
}
power_instr::power_instr(Store, StoreOpsDS op, power_reg rt, 
            power_reg ra, signed disp) : instr_t() {
   uimmediate<6> opcd = 62;
   uimmediate<2> xo = op;
   simmediate<14> d = disp/4;
   //note d/4.  This is because DS is multiplied by 4 in order to achieve
   //doubleword alignment.
   set_dsform(opcd, rt, ra, d, xo);

}


   
power_instr::power_instr(Store, StoreOpsX op, power_reg rt, 
                         power_reg ra, power_reg rb) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = op;
   uimmediate<1> rc(0);
   
   switch (op) {
   case stWordCondX:
   case stDoubleCondX:
      rc=1;
      break;
   default:
      rc=0;
      break;
   }

   set_xform(opcd, rt, ra, rb, xo, rc);
}


power_instr::power_instr(Shift, ShiftOps op, power_reg ra, power_reg rs, 
                         power_reg rb, uimmediate<1> rc) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = op;

   set_xform(opcd, rs, ra, rb, xo, rc);
   
}

power_instr::power_instr(Shift, power_reg ra, power_reg rs, uimmediate<5> sh, 
            uimmediate<1> rc) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = 824;
   
   set_xform(opcd, rs, ra, sh, xo, rc);
}

power_instr::power_instr(Shift, power_reg ra, power_reg rs, uimmediate<6> sh, 
            uimmediate<1> rc) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<9> xo = 413;
   
   set_xsform(opcd, rs, ra, sh, xo, rc);
}



power_instr::power_instr(Rotate, RotateOpsMD op, power_reg ra, power_reg rs, 
            uimmediate<6> sh, uimmediate<6> mb_or_me, uimmediate<1> rc) : instr_t() {
   uimmediate<6> opcd = 30;
   uimmediate<3> xo = op;

   set_mdform (opcd, rs, ra, sh, mb_or_me, xo, rc);

}


power_instr::power_instr(Rotate, RotateOpsM op, power_reg ra, power_reg rs, 
            uimmediate<5> sh, uimmediate<5> mb, uimmediate<5> me, 
            uimmediate<1> rc) : instr_t() {
   assert (op != rotateLeftWordANDWithMaskM);
   uimmediate<6> opcd = op;

   set_mform(opcd, rs, ra, sh, mb, me, rc);
}


power_instr::power_instr(Rotate, RotateOpsM op, power_reg ra, power_reg rs, 
            power_reg rb, uimmediate<5> mb, uimmediate<5> me, 
            uimmediate<1> rc) : instr_t() {
   assert (op == rotateLeftWordANDWithMaskM);
   uimmediate<6> opcd = op;

   set_mform(opcd, rs, ra, rb, mb, me, rc);
}

power_instr::power_instr(Rotate, RotateOpsMDS op, power_reg ra, power_reg rs, 
                         power_reg rb, uimmediate<6> mb_or_me, 
                         uimmediate<1> rc) : instr_t() {
   uimmediate<6> opcd = 30;
   uimmediate<4> xo = op;

   set_mdsform (opcd, rs, ra, rb, mb_or_me, xo, rc);
}



power_instr::power_instr(MoveToSPR, power_reg spr, power_reg rs) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate <10> xo = 467;

   assert (spr.isSprReg() );
   
   set_xfxform(false, opcd, rs, spr, xo);
}


power_instr::power_instr(MoveFromSPR, power_reg spr, power_reg rt) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate <10> xo = 339;

   assert (spr.isSprReg() );
   
   set_xfxform(false, opcd, rt, spr, xo);

}

power_instr::power_instr(MoveToCondRegFlds, uimmediate<8> fxm, power_reg rs) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate <10> xo = 144;

   set_xfxform(opcd, rs, fxm, xo);
}

power_instr::power_instr(MoveToCondRegFromXER, uimmediate<3> bf) : instr_t() {
   uimmediate <6> opcd = 31;
   uimmediate <10> xo = 512;
   uimmediate <4> zeroedU = 0;
   uimmediate<1> zeroedRc = 0;
   
   set_xform(opcd, bf, zeroedU, xo, zeroedRc);
}

power_instr::power_instr(MoveFromCondReg, power_reg rt) : instr_t() {
   uimmediate <6> opcd = 31;
   uimmediate <10> xo = 19;
   //gpr0 produces zeroes in the field, which is what we want
   power_reg r(power_reg::gpr0);
   uimmediate<1> rc = 0;
   
   set_xform(opcd, rt, r, r, xo, rc);
}

power_instr::power_instr(MoveToMachineStateReg, power_reg rs) : instr_t() {
   uimmediate <6> opcd = 31;
   uimmediate <10> xo = 146;
   //gpr0 produces zeroes in the field, which is what we want
   power_reg r(power_reg::gpr0);
   uimmediate<1> rc = 0;

   set_xform(opcd, rs, r, r, xo, rc);

}

power_instr::power_instr(MoveFromMachineStateReg, power_reg rt) : instr_t() {
   uimmediate <6> opcd = 31;
   uimmediate <10> xo = 83;
   //gpr0 produces zeroes in the field, which is what we want
   power_reg r (power_reg::gpr0);
   uimmediate<1> rc = 0;

   set_xform(opcd, rt, r, r, xo, rc);
}

   
void power_instr::convertCondCode(CondCode cond, unsigned crfield, 
				  uimmediate<5> &bo_imm, 
				  uimmediate<5> &bi_imm) {
   //must recast bo and bi as ints, so that we can use the usual bitwise 
   //operators
   unsigned bo = bo_imm;
   unsigned bi = bi_imm;    

    switch(cond) {
	case lessThan:
	    bo |= 12; //branch on true
	    bi = 0; //LT
	    break;
	case lessThanEqual:
	    bo |= 4; //branch on false
	    bi = 1; //GT
	    break;
	case equal:
	    bo |= 12;  //branch on true
	    bi = 2; //EQ
	    break;
	case notEqual:
	    bo |= 4; //branch on false
	    bi = 2; //EQ
	    break;
	case greaterThanEqual:
	    bo |= 4; //branch on false
	    bi = 0; //LT
	    break;
	case greaterThan:
	    bo |= 12; //branch on true
	    bi = 1; //GT
	    break;
	case summaryOverflow:
	    bo |= 12; //branch on true
	    bi = 3; //SO
	    break;
	case notSummaryOverflow:
	    bo |= 4; //branch on false
	    bi = 3; //SO
	    break;   
        case always:
            bo |= 20; //branch always
            break; 
	default:
	    assert(false && "Invalid condition code");
	    break;
    }
    bi += crfield*4;
 
   bo_imm = bo;
   bi_imm = bi;
}

power_instr::power_instr(Branch, signed disp, 
                         uimmediate<1> aa, uimmediate<1> lk) : instr_t() {
   uimmediate<6> opcd = 18;
   simmediate<24> li = disp/4;
   //divide disp by 4, since 0b00 gets appended to it by the processor
   set_iform(opcd, li, aa, lk);
  
}

power_instr::power_instr(BranchCond, uimmediate<5> bo, uimmediate<5> bi, 
            signed disp, uimmediate<1> aa, uimmediate<1> lk) : instr_t() {
   uimmediate<6> opcd = 16;
   simmediate<14> bd = disp/4;
   //divide disp by 4, since 0b00 gets appended to it by the processor
   set_bform(opcd, bo, bi, bd, aa, lk);
}

power_instr::power_instr(BranchCond, CondCode cond, unsigned crfield, 
            signed disp, uimmediate<1> aa, uimmediate<1> lk) : instr_t() {
   uimmediate<6> opcd = 16;
   uimmediate<5> bo = 0, bi = 0;
   simmediate<14> bd = disp/4; 
   //divide bd by 4, since 0b00 gets appended to it by the processor
   convertCondCode(cond, crfield, bo, bi);
   set_bform(opcd, bo, bi, bd, aa, lk);
}

power_instr::power_instr(BranchCondLinkReg, uimmediate<5> bo, 
                         uimmediate<5> bi,  uimmediate<1> lk) : instr_t() {
   uimmediate<6> opcd = 19;
   uimmediate<10> xo = 16;
   uimmediate<5> bbzero = 0;

   set_xlform(opcd, bo, bi, bbzero, xo, lk);

}

power_instr::power_instr(BranchCondLinkReg,  CondCode cond, unsigned crfield, 
			 uimmediate<1> lk) : instr_t() {
   uimmediate<6> opcd = 19;
   uimmediate<10> xo = 16;
   uimmediate<5> bbzero = 0;
   uimmediate<5> bo = 0, bi = 0;
   convertCondCode(cond, crfield, bo, bi);
   set_xlform(opcd, bo, bi, bbzero, xo, lk);

}


power_instr::power_instr(BranchCondCountReg, uimmediate<5> bo, 
                         uimmediate<5> bi,  
            uimmediate<1> lk) : instr_t() {
   uimmediate<6> opcd = 19;
   uimmediate<10> xo = 528;
   uimmediate<5> bbzero = 0;

   set_xlform(opcd, bo, bi, bbzero, xo, lk);
}

power_instr::power_instr(BranchCondCountReg, CondCode cond, unsigned crfield,
            uimmediate<1> lk) : instr_t() {
   uimmediate<6> opcd = 19;
   uimmediate<10> xo = 528;
   uimmediate<5> bbzero = 0;
   uimmediate<5> bo = 0, bi = 0;
   convertCondCode(cond, crfield, bo, bi);

   set_xlform(opcd, bo, bi, bbzero, xo, lk);
}

   
power_instr::power_instr(Sc) : instr_t() {
   uimmediate<6> opcd = 17;
   uimmediate<5> bo = 0;
   uimmediate<5> bi = 0;
   simmediate<14> bd = 0;
   uimmediate<1> aa = 1;
   uimmediate<1> lk = 0;

   set_bform(opcd, bo, bi, bd, aa, lk);
}

   
power_instr::power_instr(CondReg, CondRegOps op, uimmediate<5> bt, 
            uimmediate<5> ba, uimmediate<5> bb) : instr_t() {
   uimmediate<6> opcd = 19;
   uimmediate <10> xo = op;
   uimmediate<1> rc = 0;

   set_xlform(opcd, bt, ba, bb, xo, rc);
}
   
power_instr::power_instr(CondRegField, uimmediate<3> bf, uimmediate<3> bfa) : instr_t() {
   uimmediate<6> opcd = 19;
   uimmediate<10> xo = 0;

   set_xlform(opcd, bf, bfa, xo);
}
   
   
power_instr::power_instr(FPLoad, FPLoadOpsD op, power_reg frt, 
            power_reg ra, simmediate<16> d) : instr_t() {
   uimmediate<6> opcd = op;
   assert(frt.isFloatReg());

   set_dform(opcd, frt, ra, d);
}


power_instr::power_instr(FPLoad, FPLoadOpsX op, power_reg frt, 
                         power_reg ra, power_reg rb) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = op;
   uimmediate<1> rc = 0;
   assert(frt.isFloatReg());

   set_xform(opcd, frt, ra, rb, xo, rc);
}



power_instr::power_instr(FPStore, FPStoreOpsD op, power_reg frt, 
            power_reg ra, simmediate<16> d) : instr_t() {
   uimmediate<6> opcd = op;
   assert(frt.isFloatReg());

   set_dform(opcd, frt, ra, d);
}



power_instr::power_instr(FPStore, FPStoreOpsX op , power_reg frs, 
            power_reg ra, power_reg rb) : instr_t() {
    uimmediate<6> opcd = 31;
   uimmediate<10> xo = op;
   uimmediate<1> rc = 0;
   assert(frs.isFloatReg());

   set_xform(opcd, frs, ra, rb, xo, rc);
}
   
power_instr::power_instr(FPArithmetic, FPArithmeticOps op, FPPrecision p, 
                         power_reg frt, power_reg fra, 
                         power_reg frb, power_reg frc, uimmediate<1> rc) : instr_t() {
   uimmediate<6> opcd(0);
   uimmediate<5> xo = op;

   assert(frt.isFloatReg() && fra.isFloatReg() 
          && frb.isFloatReg() && frc.isFloatReg() );
   if (p == singlePrecision)
      opcd = 59;
   else //double precision
      opcd = 63;

   set_aform(opcd, frt, fra, frb, frc, xo, rc);
   
}
power_instr::power_instr(FPConversion, FPConversionOps op, 
                         power_reg frt, power_reg frb,
               uimmediate<1> rc) : instr_t() {
   uimmediate <6> opcd = 63;
   uimmediate<10> xo = op;
   power_reg fr(power_reg::fpr0);

   assert(frt.isFloatReg() && frb.isFloatReg() );
   
   set_xform(opcd, frt, fr, frb, xo, rc);

}
   
power_instr::power_instr(FPCompareUnordered, uimmediate<3> bf, 
            power_reg fra, power_reg frb) : instr_t() {
   uimmediate<6> opcd = 63;
   uimmediate<1> l = 0;
   uimmediate<10> xo = 0;
   uimmediate<1> rc = 0;
   assert(fra.isFloatReg() && frb.isFloatReg());

   set_xform(opcd, bf, l, fra, frb, xo, rc);
}

power_instr::power_instr(FPCompareOrdered, uimmediate<3> bf, 
                         power_reg fra, power_reg frb) : instr_t() {
   uimmediate<6> opcd = 63;
   uimmediate<1> l = 0;
   uimmediate<10> xo = 32;
   uimmediate<1> rc = 0;
   assert(fra.isFloatReg() && frb.isFloatReg());

   set_xform(opcd, bf, l, fra, frb, xo, rc);
}
   
power_instr::power_instr(MoveFromFPSCR, power_reg frt, uimmediate<1> rc) : instr_t() {
   uimmediate<6> opcd = 63;
   uimmediate<10> xo = 583;
   power_reg fr(power_reg::fpr0);
   assert(frt.isFloatReg());
   
   set_xform(opcd, frt, fr, fr, xo, rc);
}

power_instr::power_instr(MoveToCondRegFromFPSCR, uimmediate<3> bf, 
                         uimmediate<3> bfa) : instr_t() {
   uimmediate<6> opcd = 63;
   uimmediate<10> xo = 64;
   uimmediate<1> rc = 0;

   set_xform(opcd, bf, bfa, xo, rc);

}
power_instr::power_instr(MoveToFPSCRFieldImm, uimmediate<3> bf, 
            uimmediate<4> u, uimmediate<1> rc) : instr_t() {
   uimmediate<6> opcd = 63;
   uimmediate<10> xo = 134;
   
   set_xform(opcd, bf, u, xo, rc);
}

power_instr::power_instr(MoveToFPSCRFields, uimmediate<8> flm, 
            power_reg frb, uimmediate<1> rc) : instr_t() {
   uimmediate<6> opcd = 63;
   uimmediate<10> xo = 711;
   
   assert(frb.isFloatReg());
   
   set_xflform(opcd, flm, frb, xo, rc);
}

power_instr::power_instr(MoveToFPSCRBit0, uimmediate<5> bt, uimmediate<1> rc) : instr_t() {
   uimmediate<6> opcd = 63;
   uimmediate<10> xo = 70;
   uimmediate<5> ba = 0;
   uimmediate<5> bb = 0;

   set_xlform(opcd, bt, ba, bb, xo, rc);
}



power_instr::power_instr(MoveToFPSCRBit1, uimmediate<5> bt, uimmediate<1> rc) : instr_t() {
   uimmediate<6> opcd = 63;
   uimmediate<10> xo = 38;
   uimmediate<5> ba = 0;
   uimmediate<5> bb = 0;

   set_xlform(opcd, bt, ba, bb, xo, rc);
}

power_instr::power_instr(Rfi) : instr_t() {
   uimmediate<6> opcd = 19;
   uimmediate<10> xo = 50;
   uimmediate<5> bt = 0;
   uimmediate<5> ba = 0;
   uimmediate<5> bb = 0;
   uimmediate<1> rc = 0;
   
   set_xlform(opcd, bt, ba, bb, xo, rc);
}
   

power_instr::power_instr(TLBInvalidateEntry, power_reg rb) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = 306;
   power_reg r(power_reg::gpr0);
   uimmediate<1> rc = 0;

   set_xform(opcd, r, r, rb, xo, rc);
}


power_instr::power_instr(TLBInvalidateAll) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = 370;
   power_reg r(power_reg::gpr0);
   uimmediate<1> rc = 0;
   
   set_xform(opcd, r, r, r, xo, rc);
}

power_instr::power_instr(TLBSync) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = 566;
   power_reg r(power_reg::gpr0);
   uimmediate<1> rc = 0;
   
   set_xform(opcd, r, r, r, xo, rc);
}

power_instr::power_instr(ICacheBlockInvalidate, power_reg ra, power_reg rb) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = 982;
   power_reg r(power_reg::gpr0);
   uimmediate<1> rc = 0;
   
   set_xform(opcd, r, ra, rb, xo, rc);
}

power_instr::power_instr(InstructionSync) : instr_t() {
   uimmediate<6> opcd = 19;
   uimmediate<10> xo = 150;
   power_reg r(power_reg::gpr0);
   uimmediate<1> rc = 0;
   
   set_xform(opcd, r, r, r, xo, rc);

}

power_instr::power_instr(DCacheBlockInvalidate, power_reg ra, power_reg rb) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = 470;
   power_reg r(power_reg::gpr0);
   uimmediate<1> rc = 0;

   set_xform(opcd, r, ra, rb, xo, rc);
}



power_instr::power_instr(DCacheBlockTouch, power_reg ra, power_reg rb) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = 278;
   power_reg r(power_reg::gpr0);
   uimmediate<1> rc = 0;

   set_xform(opcd, r, ra, rb, xo, rc);
}

power_instr::power_instr(DCacheBlockTouchStore, power_reg ra, power_reg rb) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = 246;
   power_reg r(power_reg::gpr0);
   uimmediate<1> rc = 0;

   set_xform(opcd, r, ra, rb, xo, rc);
}

power_instr::power_instr(DCacheBlockSetZero, power_reg ra, power_reg rb) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = 1014;
   power_reg r (power_reg::gpr0);
   uimmediate<1> rc = 0;
   
   set_xform(opcd, r, ra, rb, xo, rc);
}

power_instr::power_instr(DCacheBlockStore, power_reg ra, power_reg rb) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = 54;
   power_reg r (power_reg::gpr0);
   uimmediate<1> rc = 0;

   set_xform(opcd, r, ra, rb, xo, rc);
}

power_instr::power_instr(DCacheBlockFlush, power_reg ra, power_reg rb) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = 86;
   power_reg r (power_reg::gpr0);
   uimmediate<1> rc = 0;

   set_xform(opcd, r, ra, rb, xo, rc);
}

power_instr::power_instr(CacheLineComputeSize, power_reg rt, power_reg ra) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = 531;
   power_reg r (power_reg::gpr0);
   uimmediate<1> rc = 0;

   set_xform(opcd, rt, ra, r, xo, rc);
}

power_instr::power_instr(CacheLineFlush, power_reg ra, power_reg rb) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = 118;
   power_reg r (power_reg::gpr0);
   uimmediate<1> rc = 0;

   set_xform(opcd, r, ra, rb, xo, rc);
}


power_instr::power_instr(CacheLineInvalidate, power_reg ra, power_reg rb) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = 502;
   power_reg r (power_reg::gpr0);
   uimmediate<1> rc = 0;

   set_xform(opcd, r, ra, rb, xo, rc);
}

power_instr::power_instr(EnforceInOrderExecutionIO) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = 854;
   power_reg r (power_reg::gpr0);
   uimmediate<1> rc = 0;

   set_xform(opcd, r, r, r, xo, rc);

}


power_instr::power_instr(MoveFromTimeBase, power_reg rt, power_reg tbr) : instr_t() {
   uimmediate<6> opcd = 31;
   uimmediate<10> xo = 371;
   assert(tbr.isTbReg() );
   
   set_xfxform(true, opcd, rt, tbr, xo);
}



int32_t power_instr::getBitsSignExtend(unsigned lobit, unsigned hibit) const {
   // First off, let's work only with the bits we want:
   const uint32_t temp = getBits(lobit, hibit);
   
   // And now let's sign-extend those bits
   const unsigned numbits = hibit - lobit + 1;
   return signExtend(temp, numbits); // bitutils.C
}

pdstring power_instr::disass_prediction_bit(bool bit, bool reg_or_nonneg)
   const {
   if ( bit) {
      if (reg_or_nonneg)
         return "+";
      else
         return "-";
   }
   
   //default behavior if prediction bit is unset
   return "";
}


pdstring power_instr::disass_crfield (unsigned bi) const {
   if (bi > 3) 
      return ("cr" + pdstring(bi / 4) + "," );
   else 
      return "";
}


void power_instr::getRegisterUsage_CRLogical ( registerUsage *ru,
                                               unsigned bt, unsigned ba, 
                                               unsigned bb) const {
   *(power_reg_set *)ru->definitelyWritten += (const reg_t &)power_reg(power_reg::crfield_type, 
                                                                       bt/4 /* want the number of condition register field, not bit */ 
                                                                       );
   *(power_reg_set *)ru->definitelyUsedBeforeWritten += (const reg_t &) power_reg(power_reg::crfield_type, ba/4);
   *(power_reg_set *)ru->definitelyUsedBeforeWritten += (const reg_t &) power_reg(power_reg::crfield_type, bb/4);
   if (xlform.lk)
      *(power_reg_set *)ru->definitelyWritten += (const reg_t &)power_reg::lr; 
   //actually, lr is either undefined, or the instruction is invalid,
   //depending on which architecture definition (power vs. powerpc) 
   //you believe

}

static const pdstring CondStringsTrue[] = {"lt", "gt", "eq", "so" };
static const pdstring CondStringsFalse[] = {"ge", "le", "ne", "ns" };

bool power_instr::getInformation_condbranch (registerUsage *ru,
                                           disassemblyFlags *disassFlags, 
                                           power_cfi *cfi, 
                                           pdstring suffix) const {
   unsigned bo = getBits(6,9);  //let's ignore the predict bit for now
   unsigned bi = bform.bi;
   
   //This is what we can find out from looking at BO field
   bool decCtr = false;   
   bool condTrue = false; //branch if condition is true or false?
   bool ctrZero = false; //branch if ctr is zero or nonzero?
   bool ctrOnly = false; //condition is not taken into account
   bool branchAlways = false;

   switch(bo) {
     case 0: //bdn on false
        decCtr = true;
        condTrue = false;
        ctrZero = false;
        break;
     case 1:  //bdz on false
        decCtr = true;
        condTrue = false;
        ctrZero = true;
        break;
     case 2: //branch on false
     case 3:
        decCtr = false;
        condTrue = false;
        ctrZero = false;
        break;
     case 4: //bdn on true
        decCtr = true;
        condTrue = true;
        ctrZero = false;
        break;
     case 5: //bdz on true
        decCtr = true;
        condTrue = true;
        ctrZero = true;
        break;
     case 6: //branch on true
     case 7:
        decCtr = false;
        condTrue = true;
        ctrZero = false;
        break;
     case 8: //plain bdn
     case 12:
        ctrOnly = true;
        decCtr = true;
        ctrZero = false;
        break;
     case 9: //plain bdz
     case 13:
        ctrOnly = true;
        decCtr = true;
        ctrZero = false;
        break;
     case 10: //branch always
     case 11:
     case 14:
     case 15:
        branchAlways = true;
        break;
     default: {
        //unsuccessful decoding bo field
        //Now that we do not assume z bits are zero, this case should never happen
        assert("illegal bo field" && false);
 
        //Reconstruct simple disassembly
        pdstring result;
        result = "bc";
        if (bform.lk)
           result += "l";
        if (bform.aa)
           result += "a";
        
        result += suffix;
        result += " ";
        result += disass_uimm(bform.bo);
        result += ",";
        result += disass_uimm(bform.bi);
        result += ",";
        if (suffix == "")
           result += disass_bd14();  //target_addr
        
        dout<<"WARNING: unrecognized BO field for instruction "<<result
            <<endl;
        
        if (disassFlags) 
           disassFlags->result = result;
   
        return false;
     }
   }

   
   if ( decCtr &&  (suffix == "ctr") ) {
      //can't use the counter as a test, and branch to it (invalid instruction)
      return false;
   }
      

   if (ru) {
          
      if (bform.lk)
         *(power_reg_set *)ru->maybeWritten += (const reg_t &)power_reg::lr;
      
      if (decCtr) {
         *(power_reg_set *)ru->definitelyUsedBeforeWritten += 
            (const reg_t &)power_reg::ctr;
         *(power_reg_set *)ru->definitelyWritten += (const reg_t &)power_reg::ctr;
      }
      if (!branchAlways && !ctrOnly) 
         *(power_reg_set *)ru->definitelyUsedBeforeWritten += 
          (const reg_t &)  power_reg(power_reg::crfield_type, bi/4);

      if (branchAlways) {
         if (suffix == "lr") {
            *(power_reg_set *)ru->maybeUsedBeforeWritten -= 
               (const reg_t &)power_reg::lr;
            *(power_reg_set *)ru->definitelyUsedBeforeWritten +=
               (const reg_t &)power_reg::lr;
         }
         if (suffix == "ctr") {
            *(power_reg_set *)ru->maybeUsedBeforeWritten -= 
               (const reg_t &)power_reg::ctr;
            *(power_reg_set *)ru->definitelyUsedBeforeWritten += 
               (const reg_t &)power_reg::ctr;
         }
             
         if (bform.lk) {
            *(power_reg_set *)ru->maybeWritten -= (const reg_t &)power_reg::lr;
            *(power_reg_set *)ru->definitelyWritten += (const reg_t &)power_reg::lr;
         }
      }


     
   }
         
   
   if (disassFlags) {
      disassFlags->result = "b"; //we are branching, after all
      
      if (branchAlways) {
         disassFlags->result += "all";  
         //nothing like this in assembler mnemonic listing, 
         //but should be clear enough
      } else {
      
         if (decCtr) {
            if (ctrZero)
               disassFlags->result += "dz";
            else 
               disassFlags->result += "dn";
         }
         if (!ctrOnly) {
            if (condTrue) 
               disassFlags->result += CondStringsTrue[bi%4];
            else
               disassFlags->result += CondStringsFalse[bi%4];
         }
      }
     
      if (bform.lk) {
         disassFlags->result += "l";
      }
      if (suffix == "") { //b-form {
         if (bform.aa)
            disassFlags->result += "a";
      }

      bool reg_or_nonneg;
      if (suffix != "")
         reg_or_nonneg = true; //register-based branch
      else  //bform with immediate
         reg_or_nonneg = (getBd14() < 0);

      disassFlags->result += disass_prediction_bit(getPredictBit(),
                                                   reg_or_nonneg);
      disassFlags->result += " ";
      disassFlags->result += disass_crfield(bform.bi);
      if (suffix == "") //b-form immediate
         disassFlags->result += disass_bd14();  //target_addr
      else
         disassFlags->result += suffix;
   }
   

   if (cfi) {
      cfi->fields.controlFlowInstruction = true;
      if (branchAlways) {
         cfi->fields.isAlways = true;
      } else {
         cfi->power_fields.decrementCTR = decCtr;
         cfi->power_fields.CTRzero = ctrZero;
         cfi->fields.isConditional = true;
         if (!ctrOnly) {
            cfi->power_fields.condTrue = condTrue;
         }
         cfi->power_fields.predict = getPredictBit();
      }
      if (bform.lk)
          cfi->power_fields.link = true;

      cfi->condition = bform.bi;
   }
   return true;

}
                                      
                                                                           

void power_instr::disass_dform_loads_and_stores (disassemblyFlags *disassFlags,
                                                 const unsigned op,
                                                 power_reg_set rt,
                                                 power_reg_set ra) const {
   const char *opstrings [] = 
      { "lwz ", "lwzu ", "lbz ", "lbzu ", "stw ", "stwu ", "stb ", "stbu ", 
        "lhz ", "lhzu ", "lha ", "lhau ", "sth ", "sthu ", "lmw ", "stmw ", 
        "lfs ", "lfsu ", "lfd ", "lfdu ", "stfs ", "stfsu ", "stfd ", "stfdu "
      };

   assert( (op > 31) && (op < 56) );
   
   const unsigned opindx = op - 32;

   disassFlags->result = opstrings[opindx] + rt.disassx() +
      + "," + pdstring(dform.d_or_si) + "(" + ra.disassx() + ")";
   //note that since registers are just numbers in power assembly,
   //the above code works equally well for both floating point and integer regs

}


void power_instr::disass_dform_arithmetic (disassemblyFlags *disassFlags,
                                           const unsigned op, 
                                           power_reg_set rt,
                                           power_reg_set ra) const {
   assert ( (op == 7) || (op == 8)|| (op == 9) || ( (op > 11) && (op < 16)) );
   const char *opstrings [] = 
      { "mulli ", "subfic ", "dozi ", NULL, NULL, "addic ", "addic. ", 
        "addi ", "addis " 
      };
   const unsigned opindx = op - 7;

   if ( (op == 14) && !dform.ra)  //addi with ra=0 is li  
      disassFlags->result = "li " + rt.disassx() + "," +  
         "," + pdstring(dform.d_or_si);
   else if  ( (op == 15) && !dform.ra)  //addis with ra=0 is lis 
      disassFlags->result = "lis " + rt.disassx() + "," +  
           pdstring(dform.d_or_si);
   else
      disassFlags->result = opstrings[opindx] + rt.disassx() + "," +  
         ra.disassx() + "," + pdstring(dform.d_or_si);
}
                                      
 
void power_instr::disass_dform_logical (disassemblyFlags *disassFlags,
                                        const unsigned op, 
                                        const unsigned ui,
                                        power_reg_set ra,
                                        power_reg_set rt) const {
   assert ( (op >23) && (op < 30) );
   
   const char *opstrings [] = 
      { "ori ", "oris ", "xori ", "xoris ", "andi. ", "andis. " };

   const unsigned opindx = op - 24;
   disassFlags->result = opstrings[opindx] + ra.disassx() + "," +  
      rt.disassx() + "," + pdstring(ui);
   
}   
                                      
pdstring power_instr::disass_to_field (const unsigned to) const {
   
   switch (to) {
      case 0x10:
         return "lt";
      case 0x14: 
         return "le";
      case 0x04:
         return "eq";
      case 0x0C:
         return "ge";
      case 0x08:
         return "gt";
      case 0x18:
         return "ne";
      case 0x02:
         return "llt";//logically less than
      case 0x06:
         return "lle"; //etc. as above
      case 0x05:
         return "lge";
      case 0x01:
         return "lgt";
      case 0x03:
         return "lne";
      case 0x1F:
         return "trap"; //unconditional
      default:
         return "";
   }
}


//mnemonic decoding for all instructions with opcode = 31
pdstring power_instr::getMnemonic31(const unsigned xop) const {
   //Unfortunately, xops are not at all contiguous, there are 
   //"holes" everywhere, some larger than others.  There are
   // ~115 instructions scattered over 10-bit field.
   switch (xop) { 
   case 0: return "cmp"; case 4: return "tw"; case 8: return "subfc";
   case 9: return "mulhdu"; case 10: return "addc"; 
   case 11: return "mulhwu"; case 19: return "mfcr"; case 20: return "lwarx"; 
   case 21: return "ldx"; case 23:  return "lwzx"; case 24:  return "slw";
   case 26: return "cntlzw"; case 27:  return "sld"; case 28:  return "and";
   case 29: return "maskg";
   case 32: return "cmpl"; case 40:  return "subf"; case 53:  return "ldux";
   case 54: return "dcbst"; case 55: return "lwzux"; 
   case 58: return "cntlzd"; case 60:  return "andc"; case 68: return "td";
   case 73: return "mulhd"; case 75:  return "mulhw"; case 82: return "mtsrd";
   case 83: return "mfmsr"; case 84:  return "ldarx"; case 86:  return "dcbf";
   case 87: return "lbzx"; case 104:  return "neg"; case 118: return "cli"; 
   case 114: return "mtsrdin"; case 119: return "lbzux"; 
   case 124: return "nor"; 
   case 136:  return "subfe"; case 138: return "adde"; 
   case 144:  return "mtcrf"; 
   case 146: return "mtmsr"; case 149: return "stdx"; 
   case 150: return "stwcx."; case 151: return "stwx"; 
   case 178: return "mtmsrd"; case 181: return "stdux"; 
   case 183: return "stwux"; 
   case 200: return "subfze"; case 202: return "addze"; 
   case 210: return "mtsr"; case 214: return "stdcx."; case 215: return "stbx";
   case 232: return "subfme"; case 233: return "mulld"; 
   case 234: return "addme"; case 235: return "mullw"; 
   case 242: return "mtsrin"; case 246: return "dcbtst"; 
   case 247: return "stbux"; case 264: return "doz"; case 266: return "add"; 
   case 277: return "lscbx "; case 278: return "dcbt";
   case 279: return "lhzx"; case 284: return "111"; case 306: return "tlbie";
   case 310: return "eciwx"; case 311: return "lhzux"; 
   case 316: return "xor"; case 339: return "mfspr"; case 341: return "lwax";
   case 343: return "lhax"; case 360: return "abs"; case 370: return "tlbia"; 
   case 371: return "mftb";
   case 373: return "lwaux"; case 375: return "lhaux"; 
   case 402: return "slbmte"; case 407: return "sthx";
   case 412: return "orc"; case 413: return "sradi"; case 434: return "slbie";
   case 438: return "ecowx"; case 439: return "sthux"; case 444: return "or";
   case 457: return "divdu"; case 459: return "divwu"; case 467: return "mtspr";
   case 470: return "dcbi"; case 476: return "nand"; case 488: return "nabs";
   case 489: return "divd";
   case 491: return "divw"; case 498: return "slbia"; case 512: return "mcrxr";
   case 502: return "cli";
   case 533: return "lswx"; case 534: return "lwrbx"; case 535: return "lfsx";
   case 536: return "srw"; case 537: return "rrib"; case 539: return "srd";  
   case 541: return "maskir";
   case 566: return "tlbsync";
   case 595: return "mfsr"; case 597: return "lswi"; case 598: return "sync";
   case 599: return "lfdx"; case 627: return "mfsri"; 
   case 630: return "dclst"; 
   case 631: return "lfdux"; 
   case 659: return "mfsrin"; case 661: return "stswx"; 
   case 662: return "stwbrx"; case 663: return "stfsx"; 
   case 695: return "stfsux"; case 725: return "stswi"; 
   case 727: return "stfdx"; case 759: return "stfdux"; 
   case 790: return "lhbrx"; case 791: return "lfqux"; 
   case 792: return "sraw"; case 794: return "srad";
   case 818: return "rac"; case 823: return "lfqux";
   case 824: return "srawi"; case 851: return "slbmfev"; 
   case 854: return "eieio"; case 915: return "slbmfee";
   case 918: return "sthbrx"; case 919: return "stfqx"; 
   case 922: return "extsh"; case 951: return "stfqux"; 
   case 954: return "extsb"; case 982: return "icbi"; 
   case 983: return "stfiwx"; case 986: return "extsw"; 
   case 1014: return "dcbz"; 
   default: assert(false);
   }
   return "";
}

//mnemonic decoding for all instructions with opcode = 63
pdstring power_instr::getMnemonic63(const unsigned xop) const {
   switch(xop) {
     case 0: 
        return "fcmpu";
     case 12: case 14: case 15: case 18: case 20: case 21: case 22:
     case 23: case 25: case 26: case 28: case 29: case 30: case 31:
     case 32: case 38: case 40: {
        //decently dense cluster of xops here
        const char *opstrings [] =
           {"frsp", NULL, "fctiw", "fctiwz", NULL, NULL, "fdiv", NULL, 
            "fsub", "fadd", "fsqrt", "fsel", NULL, "fmul", "fsqrte",
            NULL, "fmsub", "fmadd", "fmnsub", "fmnadd", "fcmpo", 
            NULL, NULL, NULL, NULL, NULL, NULL, "mtfsb1", "fneg"};
        
        const unsigned opindx = xop - 12;

        return opstrings[opindx];
     }
      case 64: return "mcrfs"; case 70: return "mtfsb0"; case 72: return "fmr";
      case 134: return "mtfsfi"; case 136: return "fnabs"; 
      case 264: return "fabs"; case 583: return "mffs"; 
      case 711: return "mtfsf"; case 814: return "fctid";
      case 815: return "fctidz"; case 846: return "fcfid";
      default: assert(false);
   }
   
   return "";
}


power_reg_set power_instr::getSPRUsage(unsigned sprnum) const {
   switch (sprnum) {
      case 1: 
         return power_reg_set::allXERFields;
      case 8: 
         return power_reg_set(power_reg::lr);
      case 9: 
         return power_reg_set(power_reg::ctr);
      case 18: 
          return power_reg_set(power_reg::dsisr);
      case 19: 
         return power_reg_set(power_reg::dar);
      case 22: 
         return power_reg_set(power_reg::dec);
      case 25: 
         return power_reg_set(power_reg::sdr1);
      case 26: case 27: 
         return power_reg_set(power_reg::power_reg(power_reg::srr_type, 
                                                   sprnum - 26));
      case 29:
         return power_reg_set(power_reg::accr);
      case 136:
         return power_reg_set(power_reg::ctrl);
      case 259:
         return power_reg_set(power_reg::power_reg(power_reg::sprg_type,
                                                   3));
         //sprg3 can be accessed as non-privilleged using this number
      case 272: case 273: case 274: case 275:
         return power_reg_set(power_reg::power_reg(power_reg::sprg_type, 
                                                   sprnum - 272));
      case 280: 
          return power_reg_set(power_reg::asr);
      case 282:
         return power_reg_set(power_reg::ear);
      case 284: 
         return power_reg_set(power_reg::tb);
      case 285: 
         return power_reg_set(power_reg::tbu);
      case 287: 
         return power_reg_set(power_reg::pvr);
      case 528: case 529: case 530: case 531: case 532: case 533: case 534: 
      case 535: case 536: case 537: case 538: case 539: case 540: case 541:
      case 542: case 543:
      //   return power_reg_set(power_reg::power_reg(power_reg::bat_type, 
      //                                             sprnum - 528));
           return power_reg::nullreg; //for now..
      //performance counters can be accessed using privilleged and
      //unprivilleged numbers
      case 771: case 772: case 773: case 774: case 775: 
      case 776: case 777: case 778: 
         return power_reg_set(power_reg::power_reg(power_reg::pmc_type,
                                                   sprnum - 770));
      case 787: case 788: case 789: case 790: case 791:
      case 792: case 793: case 794: 
         return power_reg_set(power_reg::power_reg(power_reg::pmc_type, 
                                                   sprnum -786)); 
      case 770: case 786:
         return power_reg_set(power_reg::mmcra);
      case 779: case 795:
         return power_reg_set(power_reg::mmcr0);
      case 780: case 796:
         return power_reg_set(power_reg::siar);
      case 781: case 797:
         return power_reg_set(power_reg::sdar);
      case 782: case 798:
         return power_reg_set(power_reg::mmcr1);
      case 1013:
         return power_reg_set(power_reg::dabr);
      case 1023:
         return power_reg_set(power_reg::pir);
      default:
         dout<<"WARNING: unknown special purpose register # "<<sprnum
             <<"in instruction "<<(int *) raw<<endl;
         return power_reg_set(power_reg::nullreg);
         //   assert(false);
    }
   //We'll never reach this, but let's get rid of gcc warning anyway
   return power_reg_set::empty;
}

void power_instr::getInformation_iform(registerUsage *ru, memUsage */* mu */ , 
                          disassemblyFlags *disassFlags,  
                          power_cfi *cfi, 
                          relocateInfo *rel) const {
   //this is unconditional branch to fixed address, i.e. a jump
   if (ru) {
      if (iform.lk)
         *(power_reg_set *)ru->definitelyWritten += (const reg_t &)power_reg::lr;
   }
   if (disassFlags) {
      disassFlags->result = "b";
      if (iform.lk)
         disassFlags->result += "l";
      if (iform.aa)
         disassFlags->result += "a";

      disassFlags->result += " ";
      disassFlags->result += disass_li24();  //target_addr
   }
   if (cfi) {
      cfi->fields.controlFlowInstruction = true;
      cfi->fields.isAlways = true;
      
      if (iform.aa)
         cfi->destination = controlFlowInfo::absolute;
      else
         cfi->destination = controlFlowInfo::pcRelative;
      if (iform.lk)
         cfi->power_fields.link = true;
      
      cfi->offset_nbytes = getLi24();
   }
   if (rel) {
      if (!iform.aa) {
         rel->success = inRangeOfBranch(rel->old_insnaddr, rel->new_insnaddr);
         
         
         if (rel->success) {
            const int offset = getLi24(); //offset in bytes
            kaddrdiff_t new_displacement = rel->old_insnaddr + offset - 
               rel->new_insnaddr;
            signed li = new_displacement; 
            rel->result = power_instr(branch, li, iform.aa,
                                          iform.lk).getRaw();
         }
      }
   }
}
   

void power_instr::getInformation_bform(registerUsage *ru, memUsage * /* mu */, 
                          disassemblyFlags *disassFlags, 
                          power_cfi *cfi, relocateInfo *rel) const {
   if (bform.op ==  17) {
      if (ru) {
         if (bform.lk)
            *(power_reg_set *)ru->definitelyWritten += (const reg_t &)power_reg::lr; 
         //actually, lr is either undefined, or the instruction is invalid,
         //depending on which architecture definition (power vs. powerpc) 
         //you believe
      }
         
      if (disassFlags) {
         disassFlags->result = "sc";
      }
   } else {//conditional branch immediate
      assert (bform.op == 16); 
      if ( !(getInformation_condbranch(ru, disassFlags, cfi, "")) ) {   
         //some illegal combination of bits in the BO field
         dout<<"illegal bit combination while parsing bo field\n";
      }

      if (cfi) {
         //fields and CR bit are already set by getInformation_condbranch() 
         if (bform.aa)
            cfi->destination = controlFlowInfo::absolute;
         else
            cfi->destination = controlFlowInfo::pcRelative;
         
         cfi->offset_nbytes = getBd14();
      }
      if (rel) {
         if (!bform.aa) {
            rel->success = inRangeOfBranchCond(rel->old_insnaddr, 
                                           rel->new_insnaddr);
            if (rel->success) {
               const int offset = getBd14();
               kaddrdiff_t new_displacement = rel->old_insnaddr + offset - 
                  rel->new_insnaddr;
               signed bd = new_displacement;
               rel->result = power_instr(branchcond, 
                                             bform.bo,
                                             bform.bi,
                                             bd, iform.aa,
                                             iform.lk).getRaw();
            }
         }
      }
   }
}

void power_instr::getInformation_xlform(registerUsage *ru, memUsage * /* mu */ , 
                                       disassemblyFlags *disassFlags, 
                                       power_cfi *cfi, 
                                        relocateInfo * /* rel */) const {
   unsigned xop = xlform.xo;

   switch(xop) {
      case 0: {  //mcrf -- move condition register field
         unsigned bf = getBFField();
         unsigned bfa = getBFAField();
         if (ru) {
            *(power_reg_set *)ru->definitelyWritten += 
               (const reg_t &) power_reg(power_reg::crfield_type, bf);
            *(power_reg_set *)ru->definitelyUsedBeforeWritten += 
               (const reg_t &)power_reg(power_reg::crfield_type, bfa);
            if (xlform.lk)
               *(power_reg_set *)ru->definitelyWritten += 
                  (const reg_t &)power_reg::lr; 
            //actually, lr is either undefined, or the instruction is invalid,
            //depending on which architecture definition (power vs. powerpc) 
            //you believe
         }
         if (disassFlags) {
            disassFlags->result = "mrcf ";
            disassFlags->result += pdstring(bf) + "," + pdstring(bfa);
         }
         break;
      }
      case 33:  //crnor 
         if (disassFlags) {
            if (xlform.ba == xlform.bb) //crnot extended op
               disassFlags->result = "crnot " + pdstring(xlform.bt)
                  + "," +  pdstring(xlform.ba);
            else //plain old crnor
               disassFlags->result = "crnor " + pdstring(xlform.bt) + ","
                  + pdstring(xlform.ba) + "," + pdstring (xlform.bb);
         } 
         if (ru)
            getRegisterUsage_CRLogical (ru, xlform.bt,
                                        xlform.ba,
                                        xlform.bb);
         break;
      case 129: //crandc
         if (disassFlags) {
            disassFlags->result = "crandc " + pdstring(xlform.bt) + ","
               + pdstring(xlform.ba) + "," + pdstring (xlform.bb);
         }
         if (ru)
            getRegisterUsage_CRLogical (ru, xlform.bt,
                                        xlform.ba,     
                                   xlform.bb);
         break;
       case 193: //crxor
         if (disassFlags) {
              if ( (xlform.bt == xlform.ba) &&
                   (xlform.ba == xlform.bb) ) //crclr
                 disassFlags->result = "crclr " + pdstring(xlform.bt);
              else
                 disassFlags->result = "crxor " + pdstring(xlform.bt) + 
                    "," + pdstring(xlform.ba) + "," +
                    pdstring (xlform.bb);
         }
         if (ru)
            getRegisterUsage_CRLogical (ru, xlform.bt,
                                        xlform.ba,
                                        xlform.bb);
         break;
      case 225: //crnand
         if (disassFlags) {
            disassFlags->result = "crnand " + pdstring(xlform.bt) + ","
               + pdstring(xlform.ba) + "," + pdstring (xlform.bb);
         }
         if (ru)
            getRegisterUsage_CRLogical (ru, xlform.bt,
                                        xlform.ba,
                                        xlform.bb);
         break;
      case 257: //crand
          if (disassFlags) {
            disassFlags->result = "crand " + pdstring(xlform.bt) + ","
               + pdstring(xlform.ba) + "," + pdstring (xlform.bb);
         }
         if (ru)
            getRegisterUsage_CRLogical (ru, xlform.bt,
                                        xlform.ba,
                                        xlform.bb);
         break;
      case 289: //creqv
         if (disassFlags) {
            if ( (xlform.bt == xlform.ba) &&
                 (xlform.ba == xlform.bb) ) //crset
               disassFlags->result = "crset " + pdstring(xlform.bt);
            else //plain old creqv
               disassFlags->result = "creqv " + pdstring(xlform.bt) + ","
               + pdstring(xlform.ba) + "," + pdstring (xlform.bb);
         }
         if (ru)
            getRegisterUsage_CRLogical (ru, xlform.bt,
                                        xlform.ba,
                                        xlform.bb);
         break;
      case 417: //crorc
         if (disassFlags) {
            disassFlags->result = "crorc " + pdstring(xlform.bt) + ","
               + pdstring(xlform.ba) + "," + pdstring (xlform.bb);
         }
         if (ru)
            getRegisterUsage_CRLogical (ru, xlform.bt,
                                        xlform.ba,
                                        xlform.bb);
         break;
      case 449: //cror
         if (disassFlags) {
            if (xlform.ba == xlform.bb)
               disassFlags->result = "crmove "  + pdstring(xlform.bt) +
                  "," +  pdstring(xlform.bb);
            else
               disassFlags->result = "cror " + pdstring(xlform.bt) + ","
                  + pdstring(xlform.ba) + "," + 
                  pdstring (xlform.bb);
         }
         if (ru)
            getRegisterUsage_CRLogical (ru, xlform.bt,
                                        xlform.ba,
                                        xlform.bb);
         break;
      case 16: //bclr -- conditional branch to link register
         if (ru) 
            *(power_reg_set *)ru->maybeUsedBeforeWritten += 
               (const reg_t &)power_reg::lr;
         if ( !(getInformation_condbranch(ru, disassFlags, cfi, "lr")) ) {  
            //some illegal combination of bits in the BO field
            dothrow_unknowninsn();
         } 
         if (cfi) {
            cfi->destination = controlFlowInfo::regRelative;
            cfi->offset_nbytes = 0;
            cfi->destreg1 = (reg_t *) new power_reg(power_reg::lr); 
         }
         break;
      case 528: //bcctr -- conditional branch to counter register
         if (ru)
            *(power_reg_set *)ru->maybeUsedBeforeWritten +=
               (const reg_t &)power_reg::ctr;
         if ( !(getInformation_condbranch(ru, disassFlags, cfi, "ctr")) ) {  
            //some illegal combination of bits in the BO field
            dothrow_unknowninsn();
         } 
         if (cfi) {
            cfi->destination = controlFlowInfo::regRelative;
            cfi->offset_nbytes = 0;
            cfi->destreg1 = (reg_t *) new power_reg(power_reg::ctr); 
         }
         break;
      case 50: case 18: //rfi/rfid -- return from interrupt
         if (ru) {
            *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::msr;
            *(power_reg_set *)ru->definitelyUsedBeforeWritten += (const reg_t &) power_reg::srr0;
         }
         if (disassFlags) {
            if (xop == 50)
               disassFlags->result = "rfi";
            else
               disassFlags->result = "rfid";
         }
         break;
       case 82: { //rfsvc -- return from SVC
          if (disassFlags)
             disassFlags->result = "rfsvc";
          break;
       }
      case 150: //isync -- instruction synchronize
         if (ru) {
            if (xlform.lk)
               *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::lr; 
            //actually, lr is either undefined, or the instruction is invalid,
            //depending on which architecture definition (power vs. powerpc) 
            //you believe
         }
         if (disassFlags) 
            disassFlags->result = "isync";
         break;
      default:
         dothrow_unknowninsn();
   }
     
}


void power_instr::getInformation_dform(registerUsage *ru , memUsage *mu, 
                                       disassemblyFlags *disassFlags, 
                                       power_cfi * /* cfi */,            
                                       relocateInfo * /* rel */) const {
   
   const unsigned op = dform.op;
 
   switch(op) {
      case 2: case 3: { //traps
         const power_reg_set ra = getReg2AsInt();
       
         if (ru) 
            *(power_reg_set *)ru->definitelyUsedBeforeWritten +=ra;
         if (disassFlags) {
             const pdstring condition = disass_to_field (dform.rt);
             const pdstring prefix = (op == 2) ? "td" : "tw" ;

             if (condition != "") {
                if (condition == "trap") //unconditional trap
                   disassFlags->result = condition;
                else //there is actually a parsable condition
                   disassFlags->result = prefix + condition + "i" + " " +
                      ra.disassx() + "," + 
                      pdstring(dform.d_or_si);
             }
             else //unparsable condition
                disassFlags->result = prefix + "i" + " " + pdstring(dform.rt) +
                    ra.disassx() + "," + 
                   pdstring(dform.d_or_si);
         }
         //Control Flow? Ari's comment about SPARC (below) applies here -Igor
         // What to do about control flow information???  Perhaps a trap will 
         //return; perhaps it won't!  For now, we'll work on the (flimsy) 
         //assumption that the trap traps into the kernel, which modifies no 
         // registers, and returns to the insn after the trap.
         break;
      }
      case 7: case 8: case 9:
      case 12: case 13: case 14: case 15: { //integer arithmetic with immediates
         const power_reg_set rt = getReg1AsInt();
         const power_reg_set ra = getReg2AsInt();
         
         if (ru) {
            *(power_reg_set *)ru->definitelyWritten += rt;
            *(power_reg_set *)ru->definitelyUsedBeforeWritten += ra;
            
            if ( (op == 8) || (op == 12) || (op == 13) ) { //carrying
               *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::ca;
               if (op == 13)  //add immediate carrying and record
                  *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr0;
            }
         }
         if (disassFlags)
            disass_dform_arithmetic(disassFlags, op, rt, ra);
         break;
      }
      case 10: case 11: { //compare
         const power_reg_set ra = getReg2AsInt();
         const unsigned bf = getBFField();
         const unsigned l = getLField();
         
         if (ru) {
            *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg(power_reg::crfield_type, bf);
            *(power_reg_set *)ru->definitelyUsedBeforeWritten += ra;
            //XER SO field might be used in comparison in case of overflow
            *(power_reg_set *)ru->maybeUsedBeforeWritten += (const reg_t &) power_reg::so;
         }
         
         if (disassFlags) {   
            if (op == 11) //cmpi
               disassFlags->result = "cmpi " + pdstring(bf) + "," + pdstring(l) 
                  + "," + ra.disassx() + "," 
                  + pdstring(dform.d_or_si);
            else //cmpli
               disassFlags->result = "cmpli " + pdstring(bf) + "," + pdstring(l) 
                  + "," + ra.disassx() + "," 
                  + pdstring((unsigned)dform.d_or_si);
         }
         break;
      }
      case 24: case 25: case 26: case 27: case 28: case 29: { 
         //logical immediate instructions
         const power_reg_set rs = getReg1AsInt();
         const power_reg_set ra = getReg2AsInt();
         const unsigned ui = dform.d_or_si;

         if (ru) {
            *(power_reg_set *)ru->definitelyWritten += ra;
            *(power_reg_set *)ru->definitelyUsedBeforeWritten += rs;
            if ( (op == 28) || (op == 29))  //andi., andis.
               *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr0;
         }
         
         if (disassFlags) 
            disass_dform_logical(disassFlags, op, ui, ra, rs);
         break;
      }
     
      case 32: case 33: case 34: case 35: case 36: case 37: case 38: case 39:
      case 40: case 41: case 42: case 43: case 44: case 45:  { 
         //integer loads and stores
         bool update=false; //do we "update", i.e. put calculated EA into RA ?
         const power_reg_set rs_or_rt = getReg1AsInt();
         const power_reg_set ra = getReg2AsInt();

         if ( (op < 46) && (op % 2) ) { //update ops are odd-numbered! 
            update = true; //we'll need this later for register usage
            //            if (!dform.ra || (dform.ra == dform.rt) ) {
               //RA can't be 0, nor can RA=RT for update instructions
               //oops... not illegal on power
               // dothrow_unknowninsn();
            //}
         }
         if (ru) {
            if ( (op < 36) || (op > 39 && op < 44 ))  //loads
               *(power_reg_set *)ru->definitelyWritten += rs_or_rt;
            else //stores
               *(power_reg_set *)ru->definitelyUsedBeforeWritten += rs_or_rt;
            
            if (dform.ra)
                *(power_reg_set *)ru->definitelyUsedBeforeWritten += ra;
          
         }
         if (mu) {
            if ( (op < 36) || (op > 39 && op < 44 )) {  //loads
               mu->memRead = true;
               reg_t *reg1 =(reg_t *) new power_reg(ra.expand1());
               mu->addrRead = address(address::singleRegPlusOffset, 
                                      reg1, dform.d_or_si);
            }
            else { // stores
               mu->memWritten = true;
               reg_t *reg1 =(reg_t *) new power_reg(ra.expand1());
               mu->addrWritten = address(address::singleRegPlusOffset, 
                                         reg1, dform.d_or_si);
            }
         }
         if (disassFlags)
            disass_dform_loads_and_stores (disassFlags, op, rs_or_rt, ra);
        
         break;
      }
      case  46: case 47: { //load/store multiple words
         const power_reg_set rs_or_rt = getReg1AsInt();
         const power_reg_set ra = getReg2AsInt();
         
         if (ru) {
            for (int i = rs_or_rt.expand1().getIntNum();i< 32;i++) {
               if (op == 46) //load
                  *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg(power_reg::rawIntReg, i);
               else //store
                  *(power_reg_set *)ru->definitelyUsedBeforeWritten += (const reg_t &) power_reg(power_reg::rawIntReg, i);
            }
            if (dform.ra)
               *(power_reg_set *)ru->definitelyUsedBeforeWritten += ra;
           
            
         }
         if (mu) {
            if  (op == 46) {  //load
               mu->memRead = true;
               reg_t *reg1 =(reg_t *) new power_reg(ra.expand1());
               mu->addrRead = address(address::singleRegPlusOffset, 
                                      reg1, dform.d_or_si);
            }
            else { // store
               mu->memWritten = true;
               reg_t *reg1 =(reg_t *) new power_reg(ra.expand1());
               mu->addrWritten = address(address::singleRegPlusOffset, 
                                      reg1, dform.d_or_si);
            }
         }
         if (disassFlags)
            disass_dform_loads_and_stores (disassFlags, op, rs_or_rt, ra);
         
         break;
      }
      case 48: case 49: case 50: case 51: case 52: case 53: case 54: case 55: { 
         //floating point loads and stores
         
         bool update=false; //do we "update", i.e. put calculated EA into RA ?
         const power_reg_set frs_or_frt = getReg1AsFloat();
         const power_reg_set ra = getReg2AsInt();

         if  (op % 2) { //update ops are odd-numbered!
            update = true; //we'll need this later for register usage
            //if (!dform.ra || (dform.ra == dform.rt) ) {
               //RA can't be 0, nor can RA=RT for update instructions
               //dothrow_unknowninsn();
            //}
         }
         if (ru) {
            if  (op < 52) //loads
               *(power_reg_set *)ru->definitelyWritten += frs_or_frt;
            else //stores
               *(power_reg_set *)ru->definitelyUsedBeforeWritten += frs_or_frt;
            
           
            if (dform.ra)
               *(power_reg_set *)ru->definitelyUsedBeforeWritten += ra;
         }
         if (mu) {
            if  (op < 52)  {  //loads
               mu->memRead = true;
               reg_t *reg1 =(reg_t *) new power_reg(ra.expand1());
               mu->addrRead = address(address::singleRegPlusOffset, 
                                      reg1, dform.d_or_si);
            }
            else { // stores
               mu->memWritten = true;
               reg_t *reg1 =(reg_t *) new power_reg(ra.expand1());
               mu->addrWritten = address(address::singleRegPlusOffset, 
                                      reg1, dform.d_or_si);
            }
            
         }
         if (disassFlags)
            disass_dform_loads_and_stores (disassFlags, op, frs_or_frt, ra);
         break;
      }
      default:
         dothrow_unknowninsn();
   }
      
}

void power_instr::getInformation_dsform(registerUsage *ru, memUsage *mu, 
                           disassemblyFlags *disassFlags,
                                        power_cfi */* cfi */, relocateInfo * /* rel */) const {
   const unsigned xop = dsform.xo;
   if ( (dsform.op == 58) || (dsform.op == 62) ) {
      const power_reg_set rs_or_rt = getReg1AsInt();
      const power_reg_set ra = getReg2AsInt();
      bool update = false;
      
      
      
      if (xop == 1) { //update
         update = true; //we'll need this later for register usage
         //if (!dsform.ra || (dsform.ra == dsform.rt) ) {
         //  //RA can't be 0, nor can RA=RT for update instructions
         //  dothrow_unknowninsn();
         //}
      }
      
      if (dsform.op == 58 ) {
         if (xop > 2) {
            dout<<"\nunknown DS form instruction "<<(int *) raw<<endl;
            printOpcodeFields();
            // dothrow_unknowninsn();
         }
         if (ru) {
            *(power_reg_set *)ru->definitelyWritten += rs_or_rt;
            if (dsform.ra)
               *(power_reg_set *)ru->definitelyUsedBeforeWritten += ra;
         }
         if (mu) {    
            mu->memRead = true;
            reg_t *reg1 =(reg_t *) new power_reg(ra.expand1());
            mu->addrRead = address(address::singleRegPlusOffset, 
                                   reg1, dsform.ds<<2);
         }
         
         if (disassFlags) {
            if (xop == 0)
               disassFlags->result = "ld ";
            else if (xop == 1)
               disassFlags->result = "ldu ";
            else if (xop == 2)
               disassFlags->result = "lwa ";
            else
               disassFlags->result = "unknown_ds";
         }
      } else if (dsform.op == 62) {
         if (xop > 1) {
            dout<<"\nunknown DS form instruction "<<(int *) raw<<endl;
              printOpcodeFields();
            // dothrow_unknowninsn();
         }
     
         
         if (ru) {
            *(power_reg_set *)ru->definitelyUsedBeforeWritten += rs_or_rt;
            *(power_reg_set *)ru->definitelyUsedBeforeWritten += ra;
         }
         if (mu) {    
            mu->memWritten = true;
            reg_t *reg1 =(reg_t *) new power_reg(ra.expand1());
            mu->addrWritten = address(address::singleRegPlusOffset, 
                                      reg1, dsform.ds<<2);
         }
         
         if (disassFlags) {
            if (xop == 0)
               disassFlags->result = "std ";
            else if (xop == 1)
               disassFlags->result = "stdu ";
            else 
               disassFlags->result = "unknown_ds";
         }
      }
      //finish off the common deconding.
      if (disassFlags)
         disassFlags->result += rs_or_rt.disassx() + "," +
            pdstring(dsform.ds*4) + "(" + ra.disassx() + ")";
      //multiplying DS by 4 takes care of the 0xb00 that is appended in execution 

   } else if ( (dsform.op == 56) || (dsform.op == 57)  ||
               (dsform.op == 60) || (dsform.op == 61) ) {
      //load/store floating quad 
      
      const power_reg_set frs_or_frt = getReg1AsFloat();
      const power_reg_set ra = getReg2AsInt();
     
      
      //      if (xop != 0) 
      // dothrow_unknowninsn();
         
      if (ru) {
         for (unsigned i=0, r=dsform.rt; i< 2; i++, r=(r+1)%32) {
            if ( (dsform.op == 56) || (dsform.op == 57)) //load
               *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg(power_reg::f, r);
            else //store
               *(power_reg_set *)ru->definitelyUsedBeforeWritten += 
                  (const reg_t &) power_reg(power_reg::f, r); 
         }

         if (dsform.ra)
            *(power_reg_set *)ru->definitelyUsedBeforeWritten += ra;
      }
      
      if (mu) {    
         if ( (dsform.op == 56) || (dsform.op == 57)) { //load
            mu->memRead = true;
            reg_t *reg1 =(reg_t *) new power_reg(ra.expand1());
            mu->addrRead = address(address::singleRegPlusOffset, 
                                   reg1, dsform.ds<<2);
         } else { //store
            mu->memWritten = true;
            reg_t *reg1 =(reg_t *) new power_reg(ra.expand1());
            mu->addrWritten = address(address::singleRegPlusOffset, 
                                   reg1, dsform.ds<<2);
         }
      }
      if (disassFlags) {
         switch (dsform.op) {
            case 56:
               disassFlags->result = "lfq ";
            case 57:
               disassFlags->result = "lfqu ";
            case 60:
               disassFlags->result = "stfq ";
            case 61:
               disassFlags->result = "stfqu ";
         }
      }
      
      //finish off the common deconding.
      if (disassFlags)
         disassFlags->result += frs_or_frt.disassx() + "," +
            pdstring(dsform.ds*4) + "(" + ra.disassx() + ")";
      //multiplying DS by 4 takes care of the 0xb00 that is appended in execution 
      
   }
   else
      assert(false);
}

void power_instr::processIntegerLoads(registerUsage *ru, memUsage *mu, 
                                          disassemblyFlags *disassFlags, unsigned xop )  const { 
   const power_reg_set rt = getReg1AsInt();
   const power_reg_set ra = getReg2AsInt();
   const power_reg_set rb = getReg3AsInt();
   
   bool update = false;
   if ( (xop == 119) ||  (xop == 311) || (xop == 375) ||
        (xop == 55) || (xop == 373) || (xop == 53) ) {
      update = true; //we'll need this later for register usage
      //if (!xform.ra || 
      //  (dform.ra == xform.rt) ) {
      //RA can't be 0, nor can RA=RT for update instructions
      //   dothrow_unknowninsn();
      //}
   }
   
   if (ru) {
      *(power_reg_set *)ru->definitelyUsedBeforeWritten += rb;
      *(power_reg_set *)ru->definitelyWritten += rt;
      if (xform.ra)
         *(power_reg_set *)ru->definitelyUsedBeforeWritten += ra;
      if (xform.rc)
         *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr0; 
      //actually, cr0 is either undefined, or the instruction is 
      //invalid, depending on which architecture definition 
      //(power vs. powerpc) you believe
      
   }
   
   if (mu) {
      mu->memRead = true;
      reg_t *reg1 =(reg_t *) new power_reg(ra.expand1());
      reg_t *reg2 =(reg_t *) new power_reg(rb.expand1());
      mu->addrRead = address(address::doubleReg, 
                             reg1, reg2 );
   }
   if (disassFlags) {
      disassFlags->result = getMnemonic31(xop) + " " + 
         rt.disassx() + "," +  
         ra.disassx() + 
         "," + rb.disassx();
   }
}
void power_instr::processIntegerStores(registerUsage *ru, memUsage *mu, 
                          disassemblyFlags *disassFlags, unsigned xop ) const {
      //integer stores
   const power_reg_set rs = getReg1AsInt();
   const power_reg_set ra = getReg2AsInt();
   const power_reg_set rb = getReg3AsInt();
   
   bool update = false;
   
   if ( (xop == 183) || (xop == 439) || 
        (xop == 247) || (xop == 181) ) {
      update = true; //we'll need this later for register usage
      //if (!xform.ra || 
      //   (dform.ra == xform.rt) ) {
      //RA can't be 0, nor can RA=RT for update instructions
      //  dothrow_unknowninsn();
      //}
   }
   if (ru) {
      *(power_reg_set *)ru->definitelyUsedBeforeWritten += rs;
      *(power_reg_set *)ru->definitelyUsedBeforeWritten += rb;
      if (xform.ra)
         *(power_reg_set *)ru->definitelyUsedBeforeWritten += ra;
      if ( (xop == 150) || (xop == 214) ) //conditional stores
         *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr0;
      
      if (xform.rc)
         *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr0; 
      //actually, cr0 is either undefined, or the instruction is 
      //invalid, depending on which architecture definition 
      //(power vs. powerpc) you believe
      
   }
   
   if (mu) {
      mu->memWritten = true;
      reg_t *reg1 =(reg_t *) new power_reg(ra.expand1());
      reg_t *reg2 =(reg_t *) new power_reg(rb.expand1());
      mu->addrWritten = address(address::doubleReg, 
                                reg1, reg2);
   }
   if (disassFlags) {
      disassFlags->result = getMnemonic31(xop) + " " + 
         rs.disassx() + "," +  
         ra.disassx() + 
         "," + rb.disassx();
   }
}


void power_instr::processLoadStoreStringWordImmediate(registerUsage *ru, 
                                                      memUsage *mu, 
                                                      disassemblyFlags *disassFlags, unsigned xop ) const {
       const power_reg_set rt = getReg1AsInt();
       const power_reg_set ra = getReg2AsInt();
       unsigned nr; //num of registers
       
       if (!xform.rb) //actually rb == NB (number of bytes)
          nr = 8; //32/4
       else
          nr = (unsigned)ceil(xform.rb/4.0);
       
       if (ru) { 
          if (xform.ra)
             *(power_reg_set *)ru->definitelyUsedBeforeWritten += ra;
          for (unsigned i=0, r=xform.rt; i< nr; i++, r=(r+1)%32) {
             if (xop == 597) //load
                *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg(power_reg::rawIntReg, r);
             else //store
                *(power_reg_set *)ru->definitelyUsedBeforeWritten += 
                   (const reg_t &) power_reg(power_reg::rawIntReg, r); 
          }
          if (xform.rc)
             *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr0; 
          //actually, cr0 is either undefined, or the instruction is 
          //invalid, depending on which architecture definition 
          //(power vs. powerpc) you believe
       }    
       if (mu) {
          if (xop == 597) { //load
             mu->memRead = true;
             reg_t *reg1 =(reg_t *) new power_reg(ra.expand1());
             mu->addrRead = address(address::singleRegPlusOffset, 
                                    reg1, 0 );
          } else { //store
             mu->memWritten = true;
             reg_t *reg1 =(reg_t *) new power_reg(ra.expand1());
             mu->addrWritten = address(address::singleRegPlusOffset, 
                                       reg1 ,0 );
          }
       }
       if (disassFlags) {
          disassFlags->result = getMnemonic31(xop) + " " + 
             rt.disassx() + "," +  
             ra.disassx() + 
             "," + pdstring(xform.rb);
       }
}
void power_instr::processLoadStoreStringWordIndexed(registerUsage *ru, memUsage *mu, 
                                          disassemblyFlags *disassFlags, unsigned xop ) const{
      const power_reg_set ra = getReg2AsInt();
      const power_reg_set rb = getReg3AsInt();
      const power_reg_set rt = getReg1AsInt();
      
      if (ru) {
         if (xop == 277){ //load string compare byte
            *(power_reg_set *)ru->definitelyUsedBeforeWritten += (const reg_t &) power_reg::xerresv;
            *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::xerstrbytes;
            if (xform.rc)
               *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr0;
         } else
            *(power_reg_set *)ru->definitelyUsedBeforeWritten += (const reg_t &) power_reg::xerstrbytes;
         
         //the number of bytes to be loaded is in XER 25:31.
         //no way for us to know.
         if (xop == 533) //load
            *(power_reg_set *)ru->maybeWritten += power_reg_set::allIntRegs;
         else //store
            *(power_reg_set *)ru->maybeUsedBeforeWritten += power_reg_set::allIntRegs;
         
         if (xform.rc)
            *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr0; 
         //actually, cr0 is either undefined, or the instruction is 
         //invalid, depending on which architecture definition 
         //(power vs. powerpc) you believe
      }
      if (mu) {
         if (xop == 533) { //load
            mu->memWritten = true;
            reg_t *reg1 =(reg_t *) new power_reg(ra.expand1());
            reg_t *reg2 =(reg_t *) new power_reg(rb.expand1());
            mu->addrWritten = address(address::doubleReg, 
                                      reg1, reg2 );
         } else { //store
            mu->memWritten = true;
            reg_t *reg1 =(reg_t *) new power_reg(ra.expand1());
            reg_t *reg2 =(reg_t *) new power_reg(rb.expand1());
            mu->addrWritten = address(address::doubleReg, 
                                      reg1, reg2 );
         }
      }
      if (disassFlags) {
         disassFlags->result = getMnemonic31(xop) + " " + 
            rt.disassx() + "," +  
            ra.disassx() + 
            "," + rb.disassx();
      }
}
void power_instr::processLoadStoreFloatingQuad(registerUsage *ru, memUsage *mu, 
                                          disassemblyFlags *disassFlags, unsigned xop ) const {

     const power_reg_set frt = getReg1AsFloat();
     const power_reg_set ra = getReg2AsInt();
     const power_reg_set rb = getReg3AsInt();
     bool update = false;
     
     if ( (xop == 823) || (xop == 951) )
        update = true;
     
     if (ru) { 
        if (xform.ra)
           *(power_reg_set *)ru->definitelyUsedBeforeWritten += ra;
        
        //writes/uses two consecutive registers starting at frt
        for (unsigned i=0, r=xform.rt; i< 2; i++, r=(r+1)%32) {
           if ( (xop == 791) || (xop == 823) )  //load
              *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg(power_reg::f, r);
           else //store
              *(power_reg_set *)ru->definitelyUsedBeforeWritten +=
                 (const reg_t &) power_reg(power_reg::f, r); 
        }
        if (xform.rc)
           *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr1; 
        //actually, cr1 is either undefined, or the instruction is 
        //invalid, depending on which architecture definition 
        //(power vs. powerpc) you believe
     }    
     if (mu) {
        if  ( (xop == 791) || (xop == 823) ) { //load
           mu->memRead = true;
           reg_t *reg1 =(reg_t *) new power_reg(ra.expand1());
           reg_t *reg2 =(reg_t *) new power_reg(rb.expand1());
           mu->addrRead = address(address::doubleReg, 
                                  reg1, reg2 );
        } else { //store
           mu->memWritten = true;
           reg_t *reg1 =(reg_t *) new power_reg(ra.expand1());
           reg_t *reg2 =(reg_t *) new power_reg(rb.expand1());
           mu->addrWritten = address(address::doubleReg, 
                                     reg1,reg2 );
        }
     }
     if (disassFlags) {
        disassFlags->result = getMnemonic31(xop) + " " + 
           frt.disassx() + "," +  
           ra.disassx() + 
           "," + rb.disassx();
     }
}
void power_instr::processCompare (registerUsage *ru, 
                     disassemblyFlags *disassFlags, unsigned xop ) const {
       const power_reg_set ra = getReg2AsInt();
       const power_reg_set rb = getReg3AsInt();
       const unsigned bf = getBFField();
       const unsigned l = getLField();
       
       if (ru) {
          *(power_reg_set *)ru->definitelyWritten += 
             (const reg_t &) power_reg(power_reg::crfield_type, bf);
          *(power_reg_set *)ru->definitelyUsedBeforeWritten += ra;
          *(power_reg_set *)ru->definitelyUsedBeforeWritten += rb;
          
          //XER SO field might be used in comparison 
          //in case of overflow
          *(power_reg_set *)ru->maybeUsedBeforeWritten += (const reg_t &) power_reg::so;
       }
       
       if (disassFlags) 
          disassFlags->result =  getMnemonic31(xop) + " " +
             pdstring(bf) + "," + pdstring(l) 
             + "," + ra.disassx() + "," +
             rb.disassx();
}
void power_instr::processTrap (registerUsage *ru, 
                               disassemblyFlags *disassFlags, unsigned xop )  const {
   const power_reg_set ra = getReg2AsInt();
   const power_reg_set rb = getReg3AsInt();
   
   
   if (ru) {
      *(power_reg_set *)ru->definitelyUsedBeforeWritten +=ra;
      *(power_reg_set *)ru->definitelyUsedBeforeWritten +=rb;
      if (xform.rc)
         *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr0; 
      //actually, cr0 is either undefined, or the instruction is 
      //invalid, depending on which architecture definition 
      //(power vs. powerpc) you believe
   }
   
   if (disassFlags) {
      const pdstring condition = disass_to_field (dform.rt);
      const pdstring prefix = getMnemonic31(xop);
      
      if (condition != "") {
         if (condition == "trap") //unconditional trap
            disassFlags->result = condition;
         else //there is actually a parsable condition
            disassFlags->result = prefix + condition  + " " +
               ra.disassx() + "," + 
               rb.disassx();
      }
      else //unparsable condition
         disassFlags->result = prefix  + " " + pdstring(dform.rt) +
            ra.disassx() + "," + 
            rb.disassx();
   }
   //Control Flow? Ari's comment about SPARC (below) applies here -Igor
   // What to do about control flow information???  Perhaps a trap will 
   //return; perhaps it won't!  For now, we'll work on the (flimsy) 
   //assumption that the trap traps into the kernel, which modifies no 
   // registers, and returns to the insn after the trap.
   
}
void power_instr::processLogicAndMiscArithmetic(registerUsage *ru, 
                                                disassemblyFlags *disassFlags, unsigned xop )  const {
   //logic, cntlz, exts and shift insns
   const power_reg_set ra = getReg2AsInt();
   const power_reg_set rs = getReg1AsInt();
   const power_reg_set rb = getReg3AsInt();
   const unsigned rc = xform.rc;
   bool tworeg = false; //two regs or 3 regs used?
   
   if ( (xop == 954) || (xop == 922) || (xop == 986) ||
        (xop == 58) || (xop == 26) || (xop == 824) )
      tworeg = true;
   
   if (ru) {
      *(power_reg_set *)ru->definitelyUsedBeforeWritten += rs;
      if (!tworeg)
         *(power_reg_set *)ru->definitelyUsedBeforeWritten += rb;
      *(power_reg_set *)ru->definitelyWritten += ra;
      if (rc)
         *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr0;
      if ( (xform.xo == 794) || (xform.xo == 792) ) { //algebraic shifts
         *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr0;
      }
   }
   
   if (disassFlags) { 
      disassFlags->result = getMnemonic31(xop);
      if (rc)
         disassFlags->result += ".";
      disassFlags->result += " " +
         ra.disassx() + "," +  
         rs.disassx();
      if (!tworeg)
         disassFlags->result += "," + rb.disassx();
   }
}
void power_instr::processSPRMove(registerUsage *ru, 
                    disassemblyFlags *disassFlags, unsigned xop )  const {
   const power_reg_set rs = getReg1AsInt();
   unsigned int spr = reverse5BitHalves(xfxform.spr);
   
   if (ru) {   
      if (xop == 467) { //move to spr
         //Note: this could be a set, since e.g. a move to XER
         //affects all XER fields, same goes for CR
         *(power_reg_set *)ru->definitelyWritten += getSPRUsage( spr); 
         *(power_reg_set *)ru->definitelyUsedBeforeWritten += rs;
      } else { //move from spr
         //Note: this could be a set, since e.g. a move from XER
         //affects all XER fields, same goes for CR
         *(power_reg_set *)ru->definitelyUsedBeforeWritten += getSPRUsage(spr);
         *(power_reg_set *)ru->definitelyWritten += rs;
                     
      }
      if (xform.rc)
         *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr0; 
      //actually, cr0 is either undefined, or the instruction is 
      //invalid, depending on which architecture definition 
      //(power vs. powerpc) you believe
   }
   
   if (disassFlags) {
      if (xop == 467)
         disassFlags->result = "mtspr " + pdstring(spr) + "," +
            rs.disassx();
      else 
         disassFlags->result = "mfspr " + rs.disassx()
            + "," + pdstring(spr);
   }
}
void power_instr::processCRMove (registerUsage *ru, 
                    disassemblyFlags *disassFlags, unsigned xop )  const {
   const power_reg_set rs = getReg1AsInt();
   unsigned int fxm =  xfxform.spr >> 1;
   if (ru) {
      if (xop == 144) { //mtcrf
         //this modifies CR fields specified by mask
         for (int i=0, j=1; i<=7; i++, j*=2) {
            if (j & fxm)
               *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg(power_reg::crfield_type, i);
            *(power_reg_set *)ru->definitelyUsedBeforeWritten += rs;
         }
      } else { //mfcr
         *(power_reg_set *)ru->definitelyWritten += rs;
         *(power_reg_set *)ru->definitelyUsedBeforeWritten += power_reg_set::allCRFields;
      }
      if (xform.rc)
         *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr0; 
      //actually, cr0 is either undefined, or the instruction is 
      //invalid, depending on which architecture definition 
      //(power vs. powerpc) you believe
   }
   
   if (disassFlags) {
      disassFlags->result = getMnemonic31(xop);
      if (xop == 144) //mtcrf
         disassFlags->result +=  pdstring(fxm)  + rs.disassx();  
      else  //mfcr
         disassFlags->result += rs.disassx();
   }
}
void power_instr::processXERToCRMove (registerUsage *ru, 
                         disassemblyFlags *disassFlags )   const { 
   const unsigned bf = getBFField();
   if (ru) {
      *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg(power_reg::crfield_type, bf);
      *(power_reg_set *)ru->definitelyWritten += power_reg_set::allXERFields;
      if (xform.rc)
         *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr0; 
      //actually, cr0 is either undefined, or the instruction is 
      //invalid, depending on which architecture definition 
      //(power vs. powerpc) you believe
   }
   if (disassFlags)
      disassFlags->result += "mcrxr " + pdstring(bf);
}
void power_instr::processFloatingLoads(registerUsage *ru, memUsage *mu, 
                          disassemblyFlags *disassFlags, unsigned xop )  const {      
   const power_reg_set frt = getReg1AsFloat();
   const power_reg_set ra = getReg2AsInt();
   const power_reg_set rb = getReg3AsInt();
   
   bool update = false;
   if ( (xop == 567) ||  (xop == 631) ) {
      update = true; //we'll need this later for register usage
      // if (!xform.ra) {
      //RA can't be 0 for update instructions
      //  dothrow_unknowninsn();
      //}
   }
   
   if (ru) {
      *(power_reg_set *)ru->definitelyUsedBeforeWritten += rb;
      *(power_reg_set *)ru->definitelyWritten += frt;
      if (xform.ra)
         *(power_reg_set *)ru->definitelyUsedBeforeWritten += ra;
      
      if (xform.rc)
         *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr1; 
      //actually, cr1 is either undefined, or the instruction is 
      //invalid depending on which architecture definition 
      //(power vs. powerpc) you believe

   }
   
   if (mu) {
      mu->memRead = true;
      reg_t *reg1 =(reg_t *) new power_reg(ra.expand1());
      reg_t *reg2 =(reg_t *) new power_reg(rb.expand1());
      mu->addrRead = address(address::doubleReg, 
                             reg1, reg2 );
   }
   if (disassFlags) {
      disassFlags->result = getMnemonic31(xop) + " " + 
         frt.disassx() + "," +  
         ra.disassx() + 
         "," + rb.disassx();
   }
}
void power_instr::processFloatingStores (registerUsage *ru, memUsage *mu, 
                            disassemblyFlags *disassFlags, unsigned xop )  const {
    //floating point stores
   const power_reg_set frs = getReg1AsFloat();
   const power_reg_set ra = getReg2AsInt();
   const power_reg_set rb = getReg3AsInt();
   
   bool update = false;
   if ( (xop == 759) ||  (xop == 695) ) {
      update = true; //we'll need this later for register usage
      //if (!xform.ra) {
      //   //RA can't be 0 for update instructions
      //   dothrow_unknowninsn();
      //}
   }
   
   if (ru) {
      *(power_reg_set *)ru->definitelyUsedBeforeWritten += rb;
      *(power_reg_set *)ru->definitelyUsedBeforeWritten += frs;
      if (xform.ra)
         *(power_reg_set *)ru->definitelyUsedBeforeWritten += ra;
      
      if (xform.rc)
         *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr1; 
      //actually, cr1 is either undefined, or the instruction is 
      //invalid depending on which architecture definition 
      //(power vs. powerpc) you believe
      
   }
   
   if (mu) {
      mu->memWritten = true;
      reg_t *reg1 =(reg_t *) new power_reg(ra.expand1());
      reg_t *reg2 =(reg_t *) new power_reg(rb.expand1());
      mu->addrWritten = address(address::doubleReg, 
                                reg1, reg2 );
   }
   if (disassFlags) {
      disassFlags->result = getMnemonic31(xop) + " " + 
         frs.disassx() + "," +  
         ra.disassx() + 
         "," + rb.disassx();
   }
}
void power_instr::processCacheInstr(registerUsage *ru, 
                                    disassemblyFlags *disassFlags, unsigned xop )   const {   
   //data/instruction cache instructions
   const power_reg_set ra = getReg2AsInt();
   const power_reg_set rb = getReg3AsInt();
   
   if (ru) {
      if (xform.ra)
         *(power_reg_set *)ru->definitelyUsedBeforeWritten += ra;
      *(power_reg_set *)ru->definitelyUsedBeforeWritten += rb;
      if (xform.rc)
         *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr0; 
      //actually, cr0 is either undefined, or the instruction is 
      //invalid, depending on which architecture definition 
      //(power vs. powerpc) you believe
   }   
   if (disassFlags) 
      disassFlags->result = getMnemonic31(xop) + " " +
         ra.disassx() + "," + rb.disassx();
 
}
void power_instr::processMSRMove(registerUsage *ru, 
                    disassemblyFlags *disassFlags, unsigned xop )  const {
   const power_reg_set rs_or_rt = getReg1AsInt();
   
   if (ru) {
      if (xop == 83) { //mfmsr 
         *(power_reg_set *)ru->definitelyUsedBeforeWritten += (const reg_t &) power_reg::msr;
         *(power_reg_set *)ru->definitelyWritten += rs_or_rt;
      } else { //mtmsr, mtsrd 
         *(power_reg_set *)ru->definitelyUsedBeforeWritten += rs_or_rt;
         *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::msr;
      }
   }  
   if (disassFlags) 
      disassFlags->result = getMnemonic31(xop) + rs_or_rt.disassx();
}
void power_instr::processLBInstr (registerUsage *ru, 
                     disassemblyFlags *disassFlags, unsigned xop )  const {
   const power_reg_set rb = getReg3AsInt();
   
   if (ru) {
      if (xop == 306 || xop == 434) //TLB/SLB invalidate entry 
      *(power_reg_set *)ru->definitelyUsedBeforeWritten += rb;
   }
   if (disassFlags) {  
      disassFlags->result = getMnemonic31(xform.xo);
      if (xop == 306 || xop == 434) //TLB/SLB invalidate entry 
         disassFlags->result += rb.disassx();
   }
}

void power_instr::processSLBMove (registerUsage *ru, 
                     disassemblyFlags *disassFlags, unsigned xop )  const {
    const power_reg_set rs_or_rt = getReg1AsInt();
    const power_reg_set rb = getReg3AsInt();
  
   if (ru) { 
      *(power_reg_set *)ru->definitelyUsedBeforeWritten += rb;
      if (xop == 402) { //slbmte
	  *(power_reg_set *) ru->definitelyWritten += rs_or_rt;
      } else { //slbmfee, slbmfev
	  *(power_reg_set *)ru->definitelyUsedBeforeWritten += rs_or_rt;
      }
   }
   if (disassFlags) {  
      disassFlags->result = getMnemonic31(xform.xo);
      disassFlags->result += rs_or_rt.disassx() + ", " + rb.disassx();
   }
}



void power_instr::processTBMove(registerUsage *ru, 
                   disassemblyFlags *disassFlags )  const {
   const power_reg_set rt = getReg1AsInt();
   unsigned int tbr = reverse5BitHalves(xfxform.spr);
   assert ( (tbr == 268) || (tbr == 269) );
   
   if (ru) {
      if (tbr == 268)
         *(power_reg_set *)ru->definitelyUsedBeforeWritten += (const reg_t &) power_reg::tb;
      else
         *(power_reg_set *)ru->definitelyUsedBeforeWritten += (const reg_t &) power_reg::tbu;
      *(power_reg_set *)ru->definitelyWritten += rt;
   }
   
   if (disassFlags)
      disassFlags->result += "mftb " + rt.disassx() +
         "," + pdstring(xfxform.spr);
}
void  power_instr::processCacheLineInstr (registerUsage *ru, 
                             disassemblyFlags *disassFlags, unsigned xop ) const  {
    const power_reg_set rt = getReg1AsInt();
    const power_reg_set ra = getReg2AsInt();
    const power_reg_set rb = getReg3AsInt();
    
    if (ru) {
       if (xop == 531) { //clcs -- cache line compute size
          *(power_reg_set *)ru->definitelyWritten += rt;
          *(power_reg_set *)ru->definitelyUsedBeforeWritten += ra;
       } else { //clf/cli/dclst -- cache line instructions
               if (xform.ra) //if not zero
                  *(power_reg_set *)ru->definitelyWritten += ra;
               *(power_reg_set *)ru->definitelyUsedBeforeWritten += rb;
       }
    }
    
    if (disassFlags)
       if (xop == 531) //clcs
          disassFlags->result = "clcs " + rt.disassx() +
             + "," +  ra.disassx();
       else 
          disassFlags->result = getMnemonic31(xop)  +
             ra.disassx()   + "," +  rb.disassx();
}
void power_instr::processSegRegMove(registerUsage *ru, 
                       disassemblyFlags *disassFlags, unsigned xop )  const {
       const power_reg_set rs_or_rt = getReg1AsInt();
       const power_reg_set ra = getReg2AsInt();
       const power_reg_set rb = getReg3AsInt();
       const unsigned sr = getBits(12,15);
       if (ru) {
          if (xop == 595 || xop == 659|| xop == 627 || xop == 818 ) //mfsr, mfsrin, mfsri, rac
             *(power_reg_set *)ru->definitelyWritten += rs_or_rt;
          else //
             *(power_reg_set *)ru->definitelyUsedBeforeWritten += rs_or_rt;
          if (xop == 242 || xop == 114 || xop == 659)  //mtsrin, mtsrdin, mfsrin
             *(power_reg_set *)ru->definitelyUsedBeforeWritten += rb;
          if (xop == 627 || xop == 818) { //mfsri, rac
             
             if (xform.ra)
                *(power_reg_set *)ru->definitelyWritten += ra; //sum is written here
             *(power_reg_set *)ru->definitelyUsedBeforeWritten += rb;
             if (xform.rc)
                *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr0;
          }
       }
       //We'll ignore the issue of segment register usage for now.
       
       if (disassFlags) {
          disassFlags->result = getMnemonic31(xop); 
          if (xop == 210 || xop == 82 || xop == 595)
             disassFlags->result  += pdstring(sr) + ",";
          if ( (xop == 818) && (xform.rc) )
             disassFlags->result += ".";
          
          disassFlags->result += rs_or_rt.disassx();
          
          if  (xop == 818|| xop == 627) 
             disassFlags->result += "," + ra.disassx();
          
          if (xop == 242 || xop == 114 || xop == 659|| xop == 818 || xop == 627)
             disassFlags->result += ", " + rb.disassx();
       }
}

void power_instr::getInformation_xform31(registerUsage *ru, memUsage *mu, 
                                         disassemblyFlags *disassFlags,
                                         power_cfi * /* cfi */, 
                                         relocateInfo * /* rel */) const {
      if ( xsform.xo == 413) { 
         //catch the lonely XS form instruction--sradi
         
         const power_reg_set rs = getReg1AsInt();
         const power_reg_set ra = getReg2AsInt();
         
         if (ru) {
            *(power_reg_set *)ru->definitelyWritten +=ra;
            *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::ca;
            *(power_reg_set *)ru->definitelyUsedBeforeWritten += rs;
            if (xsform.rc)
               *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr0;
         }

         if (disassFlags) {
            //reconstruct sh field
            unsigned sh = xsform.sh2;
            sh <<= 5;
            sh |= xsform.sh1;
            
            disassFlags->result = "sradi";
            if (xsform.rc)
               disassFlags->result += ".";
            disassFlags->result += " " + ra.disassx() + "," +
               rs.disassx() + "," + pdstring(sh);
         }
      } else if ( (xoform.xo == 266) || (xoform.xo == 40) || 
                  (xoform.xo == 10) || (xoform.xo == 8) || 
                  (xoform.xo == 138) || (xoform.xo == 136) ||
                  (xoform.xo == 234) || (xoform.xo == 232) || 
                  (xoform.xo == 202) || (xoform.xo == 200) ||
                  (xoform.xo == 104) || (xoform.xo == 233) ||
                  (xoform.xo == 235) || (xoform.xo == 73) || 
                  (xoform.xo == 75) || (xoform.xo == 9) || 
                  (xoform.xo == 11) || (xoform.xo == 489) ||
                  (xoform.xo == 491) || (xoform.xo == 457) || 
                  (xoform.xo == 459)  || (xoform.xo == 264) ||
                  (xoform.xo == 360) || (xoform.xo == 488) ) {
         // xo-form arithmetic.       
         const power_reg_set rt = getReg1AsInt();
         const power_reg_set ra = getReg2AsInt();
         const power_reg_set rb = getReg3AsInt();
         const unsigned oe = xoform.oe;
         const unsigned rc = xoform.rc;
         unsigned xop = xoform.xo;
         
         bool carry = false;
         bool tworeg = false; //two regs or 3 regs used?
         
         if ( (xop == 10) || (xop == 8) || (xop == 138) ||
              (xop == 136) || (xop == 234) || (xop == 232) ||
              (xop == 202) || (xop == 200)) 
            carry = true;
         
         if ( (xop == 234) || (xop == 232) || (xop == 202) ||
              (xop == 200) || (xop == 104) || (xop == 360) ||
              xop == 488)
            tworeg = true;
         
         if (ru) {
            *(power_reg_set *)ru->definitelyUsedBeforeWritten += ra;
            if (!tworeg)
               *(power_reg_set *)ru->definitelyUsedBeforeWritten += rb;
            *(power_reg_set *)ru->definitelyWritten +=rt;
            
            if (oe) {//overflow
               *(power_reg_set *)ru->maybeWritten += (const reg_t &) power_reg::so;
               *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::ov;
            }
            if (rc)
               *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr0;
            if (carry)
               *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::ca;
         }
         
         if (disassFlags) {
            disassFlags->result = getMnemonic31(xop);
            if (oe)
               disassFlags->result += "o";
            if (rc)
               disassFlags->result += ".";
            
            disassFlags->result += " " +  rt.disassx() +
               "," +  ra.disassx();
            if (!tworeg)
               disassFlags->result += "," + rb.disassx();
         }
         
      }  else { //not XS or XO with opcdode = 31
           const unsigned xop = xform.xo;
          switch (xop) {
            case 87: case 119: case 279: case 311: case 343: case 375:
            case 23: case 55: case 341: case 373: case 21: case 53:
            case 790: case 534: case 20: case 84:  {
               //integer loads
               processIntegerLoads(ru, mu, disassFlags, xop);   
               break;
            }
            case 215: case 247: case 407: case 439: case 151: case 183:
            case 149: case 181: case 918: case 662:  case 150: case 214:  {
               //integer stores
               processIntegerStores(ru, mu, disassFlags, xop);
               break;
            }
            case 597: case 725: { // load/store string word immediate      
               processLoadStoreStringWordImmediate(ru, mu, disassFlags, xop);
               break; 
            }  
            case 791: case 823: case 919: case 951: { 
               // load/store floating quad 
               processLoadStoreFloatingQuad(ru, mu, disassFlags, xop);
               break; 
            }               
            case 533: case 661: case 277: { //load/store string word indexed
               processLoadStoreStringWordIndexed(ru, mu, disassFlags, xop);
               break;
            }
            
            case 598: //sync
               if (disassFlags)
                  disassFlags->result="sync";
               break;
               
            case 0: case 32: { //compare
               processCompare(ru, disassFlags, xop);
               break;
            }
            case 4: case 68: { //trap
               processTrap(ru, disassFlags, xop);
               break;
            }
            case 28: case 29: case 444: case 316: case 476: case 124:
            case 284: case 60: case 412: case 954: case 922:
            case 986: case 58: case 26: case 27: case 24:
            case 539: case 541: case 536:  case 794: 
            case 792: case 824: case 537:  { 
               //logic, cntlz, exts and shift insns
               processLogicAndMiscArithmetic(ru, disassFlags, xop);
               break;
            }
            case 467: case 339: { //move to/from spr
               processSPRMove(ru, disassFlags, xop);
               break;
            }
            case 144: { //mtcrf
               processCRMove(ru, disassFlags, xop);
               break;
            }
            case 512: { //move  XER -> CR field
               processXERToCRMove(ru, disassFlags);
              
               break;
            }
            case 19: { //move from CR;
               processCRMove(ru, disassFlags, xop);
               break;
            }
            case 535: case 567: case 599: case 631: {  
               //floating loads
               processFloatingLoads(ru, mu, disassFlags, xop);
               break;
            }
            case 663: case 695: case 727: case 759: {
               //floating point stores
               processFloatingStores(ru, mu, disassFlags, xop);
               break;
            }
            case 278: case 246: case 1014: case 54: case 86: case 470: 
            case 982: {
               processCacheInstr(ru, disassFlags, xop);
               //data/instruction cache instructions
               break;
            }
            case 854: //eieio
               if (disassFlags)
                  disassFlags->result = "eieio"; 
               break;
            case 146: case 178: { //mtsr, mtsrd -- move to MSR
               processMSRMove(ru, disassFlags, xop);
               break;
            }
            case 83: { //move from MSR
               processMSRMove(ru, disassFlags, xop);
               break;
            }
            case 306: case 434: { //TLB/SLB invalidate entry
               processLBInstr(ru, disassFlags, xop);
               break;
            }
            case 370: case 566: case 498:  //TLB/SLB instructions
               processLBInstr(ru, disassFlags, xop);
               break;
	    case 402: case 851: case 915:
	       processSLBMove(ru, disassFlags, xop);
	       break;
            case 371: { //move from TB
               processTBMove(ru, disassFlags);
               break;
            }
            case 531: { //clcs -- cache line compute size
               processCacheLineInstr(ru, disassFlags, xop);
               break;
            }
            case 118: case 502: case 630: {
               processCacheLineInstr(ru, disassFlags, xop);
                break;
            }
            case 210: case 82: { //mtsr, mtsrd -- move to segment register
               processSegRegMove(ru, disassFlags, xop);
               break;
            }
            case 242: case 114: { 
               //mtsrin, mtsrdin -- move to segment register indirect
               processSegRegMove(ru, disassFlags, xop);
               break;
            }               
            case 595: { //mfsr -- move from segment register
               processSegRegMove(ru, disassFlags, xop);
               break;
            }
            case 659: { //mfsrin -- move from segment register indirect
               processSegRegMove(ru, disassFlags, xop);
               break;
            }              
            case 627: case 818: { //mfsri, rac
               processSegRegMove(ru, disassFlags, xop);
               break;
            }              

            default:
               //dout<<"unknown xform instruction "<<(int *)raw<<endl;
               //printOpcodeFields();
               dothrow_unknowninsn();
           }  
      }
}



void power_instr::getInformation_xform63(registerUsage *ru, memUsage * /* mu */, 
                                         disassemblyFlags *disassFlags,
                                          power_cfi */* cfi */, 
                                          relocateInfo */* rel */) const {

      const unsigned xop = xform.xo;
      
      switch (xop) {
         case 12: case 814: case 815: case 14: case 15: case 846: {
            //floating rounding and conversion ops.  Note fall-through
            //to float moves (the only diff. is conversions modify FPSCR)
            const power_reg_set frt = getReg1AsFloat();
            const power_reg_set frb = getReg3AsFloat();
            if (ru)
               *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::fpscr;
            //Fall through...
         }
         case 72: case 40: case 264: case 136: { //floating moves
            const power_reg_set frt = getReg1AsFloat();
            const power_reg_set frb = getReg3AsFloat();

            if (ru) {
               *(power_reg_set *)ru->definitelyUsedBeforeWritten += frb;
               *(power_reg_set *)ru->definitelyWritten += frt;
               if (xform.rc)
                  *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr1;
            }
            if (disassFlags) {
               disassFlags->result = getMnemonic63(xop);
               if (xform.rc)
                  disassFlags->result += ".";
               disassFlags->result += " " + frt.disassx();
               + "," + frb.disassx();
            }
            break;
         }
         case 0: case 32: { //floating point compare
            
            const power_reg_set fra = getReg2AsFloat();
            const power_reg_set frb = getReg3AsFloat();
            const unsigned bf = getBFField();
            
            if (ru) {
               *(power_reg_set *)ru->definitelyWritten += 
                  (const reg_t &) power_reg(power_reg::crfield_type, bf);
               *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::fpscr;
               *(power_reg_set *)ru->definitelyUsedBeforeWritten += fra;
               *(power_reg_set *)ru->definitelyUsedBeforeWritten += frb;
               if (xform.rc)
                  *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr0; 
               //actually, cr0 is either undefined, or the instruction is 
               //invalid, depending on which architecture definition 
               //(power vs. powerpc) you believe
            }
            
            if (disassFlags) 
               disassFlags->result =  getMnemonic63(xop) + 
                  pdstring(bf) + "," +
                  fra.disassx() + "," +
                  frb.disassx();
            break;
         
         }
         case 583: { //move from FPSCR
            const power_reg_set frt = getReg1AsFloat();
            if (ru) {
               *(power_reg_set *)ru->definitelyUsedBeforeWritten += (const reg_t &) power_reg::fpscr;
               *(power_reg_set *)ru->definitelyWritten += frt;
               if (xform.rc)
                  *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr1;
            }
            
            if (disassFlags) {
               disassFlags->result = "mffs";
               if (xform.rc)
                  disassFlags->result += ".";
               disassFlags->result += " " + frt.disassx();
               
            }
            break;
         }
         case 64: { //move to CR from FPSCR
            unsigned bf = getBFField();
            unsigned bfa = getBFAField();
            if (ru) {
               *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg(power_reg::crfield_type, bf);
               *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::fpscr; //both used
                                                          //and written
               if (xform.rc)
                  *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr0; 
               //actually, cr0 is either undefined, or the instruction is 
               //invalid, depending on which architecture definition 
               //(power vs. powerpc) you believe
            }
            if (disassFlags) {
               disassFlags->result = "mcrfs ";
               disassFlags->result += pdstring(bf) + "," + pdstring(bfa);
            }
            break;
         }
         case 134: { //move to FPSCR field immediate
            const unsigned u =  getUField();
            const unsigned bf = getBFField();
            
            if (ru) {
               *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::fpscr;
               if (xform.rc)
                  *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr1;
            }

            if (disassFlags) {
               disassFlags->result = "mtfsfi";
               if (xform.rc)
                  disassFlags->result += ".";
               disassFlags->result += " " + pdstring(bf) + "," + pdstring(u);
            }
            break;
         }
         case 711: { //move to FPSCR fields
            const power_reg_set frb = getReg3AsFloat();
            unsigned int flm =  xflform.flm;
            
             if (ru) {
               *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::fpscr;
               *(power_reg_set *)ru->definitelyUsedBeforeWritten += frb;
               if (xform.rc)
                  *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr1;
            }

            if (disassFlags) {
               disassFlags->result = "mtfsf";
               if (xform.rc)
                  disassFlags->result += ".";
               disassFlags->result += " " + pdstring(flm) + 
                  "," + frb.disassx();
            }
            break;
         }
         case 70: case 38: { //move to FPSCR bit 0/bit 1
            unsigned bt = xform.rt;
         
            if (ru) {
               *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::fpscr;
               if (xform.rc)
                  *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr1;
            }
            if (disassFlags) {
               disassFlags->result = getMnemonic63(xop);
               if (xform.rc)
                  disassFlags->result += ".";
               disassFlags->result += " " + pdstring(bt);
            }
            break;
         }
        default:
           dothrow_unknowninsn();

      } 
   
 }

void power_instr::getInformation_aform(registerUsage *ru, memUsage * /* mu */, 
                                       disassemblyFlags *disassFlags,
                                       power_cfi */* cfi */, 
                                       relocateInfo */* rel */) const {
   assert( ( aform.op == 59) || (aform.op == 63) );

   const power_reg_set frt = getReg1AsFloat();
   const power_reg_set fra = getReg2AsFloat();
   const power_reg_set frb = getReg3AsFloat();
   const power_reg_set frc = getReg4AsFloat();
   const unsigned op = aform.op;
   const unsigned xop = aform.xo;
   const unsigned opindx = xop - 18;

   const char *opstrings [] = 
      { "fdiv", NULL, "fsub", "fadd", NULL, NULL, NULL, "fmul", NULL, NULL,
        "fmsub", "fmadd", "fnmsub", "fnmadd" };
   
   //set up beginning of decoding.  At this point we know everything,
   //but which registers are being used.
   if (disassFlags) {
      disassFlags->result = opstrings[opindx];
      if (op == 59) //single
         disassFlags->result += "s";
      if (aform.rc)
         disassFlags->result += ".";
      disassFlags->result += " ";
   }
   //Set up what we know about register usage.
   if (ru) {
      if (aform.rc)
         *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr0;
      *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::fpscr;
   }
           
   switch (xop) {
      case 21: case 20: case 18: { //these use frt, fra, frb
         if (ru) {
            *(power_reg_set *)ru->definitelyWritten += frt;
            *(power_reg_set *)ru->definitelyUsedBeforeWritten += fra;
            *(power_reg_set *)ru->definitelyUsedBeforeWritten += frb;
         }
         if (disassFlags) 
            disassFlags->result += frt.disassx() + "," +
               fra.disassx() + "," + frb.disassx();
         break;
      }
      case 25: { //fmul uses frt, fra, frc
         if (ru) {
            *(power_reg_set *)ru->definitelyWritten += frt;
            *(power_reg_set *)ru->definitelyUsedBeforeWritten += fra;
            *(power_reg_set *)ru->definitelyUsedBeforeWritten += frc;
         }
         if (disassFlags) 
            disassFlags->result += frt.disassx() + "," +
               fra.disassx() + "," + frc.disassx();
         break;
      }
      case 29: case 28: case 31: case 30: { //multiply-add use all four regs
          if (ru) {
            *(power_reg_set *)ru->definitelyWritten += frt;
            *(power_reg_set *)ru->definitelyUsedBeforeWritten += fra;
            *(power_reg_set *)ru->definitelyUsedBeforeWritten += frb;
            *(power_reg_set *)ru->definitelyUsedBeforeWritten += frc;
         }
         if (disassFlags) 
            disassFlags->result += frt.disassx() + "," +
               fra.disassx() + "," + frb.disassx()
               + "," +  frc.disassx();
         break;
      }
      default:
         dothrow_unknowninsn();
   }
}

void power_instr::getInformation_mform(registerUsage *ru, memUsage */* mu */, 
                                        disassemblyFlags *disassFlags,
                                       power_cfi * /* cfi */, 
                                       relocateInfo */* rel */) const {
   if ( ! ((mform.op == 20) ||  (mform.op == 21) || 
           (mform.op == 22) ||  (mform.op == 23) ))
      dothrow_unknowninsn();

   const power_reg_set rs = getReg1AsInt();
   const power_reg_set ra = getReg2AsInt();
   const power_reg_set rb = getReg3AsInt();
   
   const unsigned op = mform.op;
   const unsigned opindx = op - 20;
   const char *opstrings [] = 
      { "rlwimi", "rlwinm", "rlmi", "rlwnm"};

   if (disassFlags) {
      disassFlags->result = opstrings[opindx];
      if (mform.rc)
         disassFlags->result += ".";
      disassFlags->result += " " + ra.disassx() + ","
         + rs.disassx() + ","  + pdstring(mform.sh) + ","
         + pdstring(mform.mb) + ","  + pdstring(mform.me);
   }  
   
   if (ru) {
      *(power_reg_set *)ru->definitelyWritten += ra;
      *(power_reg_set *)ru->definitelyUsedBeforeWritten += rs;
      if (mform.rc)
         *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr0;
   }
   
   if  (op == 23) {
      if (ru) 
         *(power_reg_set *)ru->definitelyUsedBeforeWritten += rb;
   }
   //otherwise, we already have all the information
}


void power_instr::getInformation_mdform(registerUsage *ru, memUsage */* mu */, 
                                        disassemblyFlags *disassFlags,
                                        power_cfi */* cfi */, 
                                        relocateInfo */* rel */) const {
   assert (mdform.op == 30);
   const unsigned mdxop = mdform.xo;
   const unsigned mdsxop = mdsform.xo;
   const char *opstrings [] = 
      { "rldicl", "rldicr", "rldic", "rldimi", NULL, NULL, NULL, NULL,
        "rldcl", "rldcr", NULL, NULL, NULL, NULL, NULL };
   const power_reg_set rs = getReg1AsInt();
   const power_reg_set ra = getReg2AsInt();
   const power_reg_set rb = getReg3AsInt();
   
   //make sure we are dealing with a known instruction
   if (mdxop > 3) {
      if (opstrings[mdxop] == NULL)
         dothrow_unknowninsn();
   }
   if (ru) {
       *(power_reg_set *)ru->definitelyWritten += ra;
      *(power_reg_set *)ru->definitelyUsedBeforeWritten += rs;
      if (mdform.rc)
         *(power_reg_set *)ru->definitelyWritten += (const reg_t &) power_reg::cr0;
      if (  (mdxop > 4) &&  (mdsxop == 8) || (mdsxop == 9) )
         *(power_reg_set *)ru->definitelyUsedBeforeWritten += rb;
   }

   if (disassFlags) {      
      if (mdxop < 4) { //MD-form
         //reconstruct sh field
         unsigned sh = mdform.sh2 << 5;
         sh |= mdform.sh1;
         
         disassFlags->result = opstrings[mdxop];
         if (mform.rc)
            disassFlags->result += ".";
         disassFlags->result += " " + ra.disassx() + ","
            + rs.disassx() + "," +
            pdstring(sh) + "," + pdstring(mdform.mb_or_me);
      } else { //MDS-form
         disassFlags->result = opstrings[mdsxop];
         if (mform.rc)
            disassFlags->result += ".";
         disassFlags->result += " " + pdstring(mdform.ra) + ","
            + rs.disassx() + "," +
            rb.disassx() + "," + 
            pdstring(mdform.mb_or_me);
      }
   }
}

pdstring power_instr::disass_simm(int simm) const {
//     if (disAsDecimal)
//        return pdstring(simm);

   // hex disassembly:
   const bool negative = (simm < 0);
   pdstring result;
   if (negative) {
      simm = -simm;
      result = "-";
   }
   result += tohex(simm);
   return result;   
}

pdstring power_instr::disass_uimm(unsigned uimm) const {
//     if (disAsDecimal)
//        return pdstring(uimm);
   
   // hex disassembly:
   return tohex(uimm);
}

void power_instr::getInformation(registerUsage *ru,
                                 memUsage *mu,
                                 disassemblyFlags *df,
                                 controlFlowInfo *icfi,
                                 relocateInfo *rel) const {
   // Why do we do so much (disass, get register usage, get control flow
   // information) in one routine?  So there is only one place where code exists to
   // parse the raw instructions's bits (opcode, etc.), thus reducing the chance of
   // tricky bit parsing bugs spreading.  However, we must be careful not to cram
   // too much code "inlined" into a single function, lest we fry the icache.
   
   // initialize the results:
   power_cfi *cfi = (power_cfi *) icfi;

   if (cfi) {
      cfi->bit_stuff = 0;
      cfi->power_bit_stuff = 0;
         // faster than setting bit fields to zero,
         // and avoids purify UMCs to boot!
   }

   if (rel) {
      rel->success = true; // assume success (for now)
      rel->result = getRaw(); // assume relocated insn is same as old insn (for now)
   }

   register const unsigned opcd = generic.op;
   if ( ( (opcd >= 2) && (opcd <= 15) ) ||( (opcd >= 24) && (opcd <=29 ) ) ||
        ( (opcd >=32) && (opcd <= 55)) )
      getInformation_dform(ru, mu, df, cfi, rel);
   else if ( (opcd == 16) || (opcd == 17) )
      getInformation_bform(ru, mu, df, cfi, rel);
   else if (opcd == 18)
      getInformation_iform(ru, mu, df, cfi, rel);
   else if (opcd == 19)
      getInformation_xlform(ru, mu, df, cfi, rel);
   else if ( (opcd >= 20) && (opcd <= 23) )
      getInformation_mform(ru, mu, df, cfi, rel);
   else if (opcd == 30)
      getInformation_mdform(ru, mu, df, cfi, rel); //MD and MDS form
   else if (opcd == 31)
         getInformation_xform31(ru, mu, df, cfi, rel); //X, XO, XFX, XS
   else if (opcd == 63) {
      unsigned extop = aform.xo;
      if ( (extop > 17)  && (extop < 32) ) 
         getInformation_aform(ru,mu,df,cfi, rel); 
      else //X, XFL
         getInformation_xform63(ru, mu, df, cfi, rel); 
   }
   else if (opcd == 59)
      getInformation_aform(ru, mu, df, cfi, rel);
   else if ( (opcd >= 56) && (opcd <= 61) ) //note this can't be 59
      getInformation_dsform(ru, mu, df, cfi, rel);
   else if (opcd == 62)   
      getInformation_dsform(ru, mu, df, cfi, rel);
   else if ( (raw == 0x00b00b00)|| (raw == 0x0) ) {
      //linux BUG() macro puts a fake invalid instruction.  We need to treat
      //it as an exit in CFG analysis.
      if (cfi) {
         cfi->fields.controlFlowInstruction = true;
         cfi->power_fields.isBug = true;

         //pretend it's register relative.  For relocation purposes,
         //we have to do the same thing (nothing).
         cfi->destination = controlFlowInfo::regRelative;
      }
   }
   else
      dothrow_unknowninsn();
   
   //calculate arch-independent CFI designations based on low-level analysis
   if ( (cfi) && (!cfi->power_fields.isBug) ) {
      if (cfi->fields.controlFlowInstruction) {
         if ( (cfi->destination != controlFlowInfo::regRelative)  
              && cfi->power_fields.link) {
            //branch and link = call
            cfi->fields.isCall = true;
         } else if ( (cfi->destination == controlFlowInfo::regRelative) 
                     &&  *(cfi->destreg1) == (const reg_t &) power_reg::lr) {
            //branch to link register == return, assuming jump tables etc.
            // are handled using bctr, which they are in optimized code.
            cfi->fields.isRet = true;
         } else {
            //some sort of jump
            cfi->fields.isBranch = true; 
         }
      }
   }
}

//for debugging
void power_instr::printOpcodeFields() const {
   cout<<"opcode is "<<xform.op<<" xform xo "<<xform.xo
       <<" xoform xo "<<xoform.xo<<endl;
}


//---------------------------------------------------------------

bool power_instr::isCall() const {
   power_cfi cfi;
   getControlFlowInfo((controlFlowInfo *)&cfi);
   
   return  (cfi.fields.controlFlowInstruction && cfi.fields.isCall); 
}

bool power_instr::isBranch() const {
   power_cfi cfi;
   getControlFlowInfo((controlFlowInfo *)&cfi);
   
   return (cfi.fields.controlFlowInstruction &&  cfi.fields.isBranch); 
}
bool power_instr::isBranchToFixedAddress(kptr_t insnAddr, kptr_t &brancheeAddr)
   const {
   power_cfi cfi;
   getControlFlowInfo((controlFlowInfo *)&cfi);
   
   if  (cfi.fields.controlFlowInstruction &&  
        cfi.destination != controlFlowInfo::regRelative) {
      if (cfi.destination == controlFlowInfo::pcRelative)
         brancheeAddr = insnAddr + cfi.offset_nbytes;
      else //absolute
         brancheeAddr = cfi.offset_nbytes;
      return true;
   }
   return false;
}

bool power_instr::isUnconditionalJumpToFixedAddress() const {
   power_cfi cfi;
   getControlFlowInfo((controlFlowInfo *)&cfi);
   
   if  (cfi.fields.controlFlowInstruction && ! cfi.fields.isAlways &&
        cfi.destination != controlFlowInfo::regRelative &&
        ! cfi.power_fields.link)
      return true;

   return false;
}


bool power_instr::isCallToFixedAddress(kptr_t insnAddr, kptr_t &brancheeAddr)
   const {
   power_cfi cfi;
   getControlFlowInfo((controlFlowInfo *)&cfi);

   if  (cfi.fields.controlFlowInstruction &&  cfi.fields.isCall &&
        cfi.destination != controlFlowInfo::regRelative) {
      if (cfi.destination == controlFlowInfo::pcRelative)
         brancheeAddr = insnAddr + cfi.offset_nbytes;
      else //absolute
         brancheeAddr = cfi.offset_nbytes;
      return true;
   }
   return false;
}

bool power_instr::isCallInstr(kaddrdiff_t &offset) const {
   power_cfi cfi;
   getControlFlowInfo((controlFlowInfo *)&cfi);
   
   if  (cfi.fields.controlFlowInstruction &&  cfi.fields.isCall &&
        cfi.destination != controlFlowInfo::regRelative) {
      offset = cfi.offset_nbytes;
      return true;
   }
   return false;
}

bool power_instr::isUnanalyzableCall() const {
   power_cfi cfi;
   getControlFlowInfo((controlFlowInfo *)&cfi);
   
   return ( cfi.fields.controlFlowInstruction &&
            (cfi.destination == controlFlowInfo::regRelative) && 
            (*(cfi.destreg1) == (const reg_t &) power_reg::ctr) );
}




bool power_instr::isNop() const {
   return (raw == 0x60000000); //ori 0,0,0
}

bool power_instr::isLoad() const {
   if (xform.op == 31) {
      switch (xform.xo) {
         case 87: case 119: case 279: case 311: case 343: case 375: 
         case 23: case 55: case 341: case 373: case 21: case 53: case 790:
         case 534: case 597: case 533: case 20: case 84: 
            return true;
      }
   }
   
   switch (dform.op) {
      case 32: case 33: case 34: case 35: case 40: case 41: case 42:
      case 43: case 46:  case 58:
         return true;
   }
   return false;
}

bool power_instr::isStore() const {
   if (xform.op == 31) {
      switch (xform.xo) {
        case 215: case 247: case 407: case 439: case 151: case 183: 
        case 149: case 181: case 918: case 662: case 725: case 661:
        case 150: case 214:
           return true;
      }
   }
   switch (dform.op) {
      case 38: case 39: case 44: case 45: case 36: case 37: case 62: case 47:
         return true;
   }
   return false;
}


bool power_instr::isCmp() const {
   return ( (dform.op == 10 || dform.op == 11) ||
            ( xform.op == 31 && (xform.xo == 0 || xform.xo == 32)) );
}

bool power_instr::isIntegerLoadWithOffset(long *offset) const {
   if ( ( dform.op > 31 &&  dform.op < 36 ) || 
        (dform.op > 39 && dform.op < 44 ) ) {
      *offset = dform.d_or_si;
      return true;
   }
   return false;
}

bool power_instr::isRotateLeftWordImmediate() const {
   if ( ( mform.op == 20) || ( mform.op == 21 ) ) {
      return true;
   }
   return false;
    
}

void power_instr::getDisassembly(disassemblyFlags &df) const {
   getInformation(NULL, NULL, &df, NULL, NULL);
}

pdstring power_instr::disassemble(kptr_t insnaddr) const {
   disassemblyFlags fl;
   fl.pc = insnaddr;
   
   getInformation(NULL, NULL, &fl, NULL, NULL);

   return fl.result;
}

void power_instr::getRelocationInfo(relocateInfo &ri) const {
   getInformation(NULL, NULL, NULL, NULL, &ri);
}

/* ******************************************************************** */

bool power_instr::existsTrueDependence(const registerUsage &ru,
                                       const registerUsage &nextRu,
                                       const memUsage &mu,
                                       const memUsage &nextMu) {
   if (mu.memWritten && nextMu.memRead)
      return true; // for now, we don't bother with addresses

   const power_reg_set instr1Writes = *(power_reg_set *)ru.definitelyWritten + 
      *(power_reg_set *)ru.maybeWritten;
   const power_reg_set instr2Reads = 
      *(power_reg_set *)nextRu.definitelyUsedBeforeWritten +
      *(power_reg_set *)nextRu.maybeUsedBeforeWritten;
   const power_reg_set intersection = instr1Writes & instr2Reads;
 
   if (intersection != power_reg_set::emptyset)
      // instr1 (this) writes to a reg which is used by instr2 (nextInstr)
      return true;

   return false;
}

bool power_instr::existsAntiDependence(const registerUsage &ru,
                                       const registerUsage &nextRu,
                                       const memUsage &mu,
                                       const memUsage &nextMu) {
   if (mu.memRead && nextMu.memWritten)
      return true; // for now, we don't bother discriminating the addrs

   const power_reg_set instr1Reads = 
      *(power_reg_set *)ru.definitelyUsedBeforeWritten +
      *(power_reg_set *)ru.maybeUsedBeforeWritten;
   const power_reg_set instr2Writes = 
      *(power_reg_set *)nextRu.definitelyWritten +
      *(power_reg_set *)nextRu.maybeWritten;
   const power_reg_set intersection = instr1Reads & instr2Writes;

   if (intersection != power_reg_set::emptyset)
      return true;

   return false;
}

bool power_instr::existsOutputDependence(const registerUsage &ru,
                                         const registerUsage &nextRu,
                                         const memUsage &mu,
                                         const memUsage &nextMu) {
   if (mu.memWritten && nextMu.memWritten)
      return true; // for now, we don't bother discriminating the addrs

   const power_reg_set instr1Writes = 
      *(power_reg_set *)ru.definitelyWritten +
      *(power_reg_set *)ru.maybeWritten;
   const power_reg_set instr2Writes = 
      *(power_reg_set *)nextRu.definitelyWritten +
      *(power_reg_set *)nextRu.maybeWritten;
   const power_reg_set intersection = instr1Writes & instr2Writes;
   if (intersection != power_reg_set::emptyset)
      return true;

   return false;
}

instr_t::dependence
power_instr::calcDependence(const instr_t *nextInstr) const {
   // assuming 'this' is executed, followed by 'nextInstr', calculate the
   // dependency.  If there is more than one dependence, the stronger one
   // is returned.  I.e, if a true dependence exists, then return true;
   // else if an anti-dependence exists then return anti;
   // else if an output-dependence exists then return output;
   // else return none;

   registerUsage ru, nextRu;
   memUsage mu, nextMu;

   getInformation(&ru, &mu, NULL, NULL, NULL);
   nextInstr->getInformation(&nextRu, &nextMu, NULL, NULL, NULL);

   if (existsTrueDependence(ru, nextRu, mu, nextMu))
      return deptrue;
   else if (existsAntiDependence(ru, nextRu, mu, nextMu))
      return depanti;
   else if (existsOutputDependence(ru, nextRu, mu, nextMu))
      return depoutput;
   else
      return depnone;
}

instr_t *power_instr::relocate(kptr_t old_insnaddr,
                                  kptr_t new_insnaddr) const {
   relocateInfo ri;
   ri.old_insnaddr = old_insnaddr;
   ri.new_insnaddr = new_insnaddr;
   getRelocationInfo(ri);
   return (instr_t *) new power_instr(ri.result);
}


instr_t *power_instr::tryRelocation(kptr_t old_insnaddr,
                                  kptr_t new_insnaddr, bool *relSuccess) const {
   relocateInfo ri;
   ri.old_insnaddr = old_insnaddr;
   ri.new_insnaddr = new_insnaddr;
   getRelocationInfo(ri);
   *relSuccess = ri.success;
   return (instr_t *) new power_instr(ri.result);
}


bool power_instr::inRangeOf(kptr_t fromaddr, kptr_t toaddr,
                            unsigned numBitsOfSignedDisplacement) {
   // a static method

   kptr_t min_reachable, max_reachable;
   getRangeOf(fromaddr, numBitsOfSignedDisplacement, min_reachable, max_reachable);
   
   return (toaddr >= min_reachable && toaddr <= max_reachable);
}


bool power_instr::inRangeOfBranch(kptr_t from, kptr_t to) {
   // static
   return inRangeOf(from, to, 26);
}

bool power_instr::inRangeOfBranchCond(kptr_t from, kptr_t to) {
   // static 
   return inRangeOf(from, to, 18);
}



void power_instr::getRangeOf(kptr_t fromaddr,
                             unsigned numBitsOfSignedDisplacement,
                             kptr_t &min_reachable_result,
                             kptr_t &max_reachable_result) {
   // a static method

   kptr_t negative_displacement = (kptr_t)1U << (numBitsOfSignedDisplacement-1);
   kptr_t positive_displacement = negative_displacement - 1;

   if (negative_displacement > fromaddr)
      min_reachable_result = 0;
   else
      min_reachable_result = fromaddr - negative_displacement;

   if (positive_displacement > (kptr_t)-1U - fromaddr)
      max_reachable_result = (kptr_t)-1U;
   else
      max_reachable_result = fromaddr + positive_displacement;
   
   assert(min_reachable_result <= fromaddr);
   assert(fromaddr <= max_reachable_result);
}

void power_instr::getRangeOfBranch(kptr_t from, kptr_t &min_reachable_result,
                                 kptr_t &max_reachable_result) {
   // static
   getRangeOf(from, 26, min_reachable_result, max_reachable_result);
}

void power_instr::getRangeOfBranchCond(kptr_t from, 
                                        kptr_t &min_reachable_result,
                                        kptr_t &max_reachable_result) {
   // static
   getRangeOf(from, 16, min_reachable_result, max_reachable_result);
}

void power_instr::changeBranchOffset(int new_insnbytes_offset) {
   if (bform.op == 16) // bform conditional branch
      replaceBits(2,simmediate<14>(new_insnbytes_offset/4));
      //dividing offset by 4, since 0b00 gets appended to the number anyway
   else if (iform.op == 18) 
      replaceBits(2,simmediate<24>(new_insnbytes_offset/4)); 
      //dividing offset by 4, since 0b00 gets appended to the number anyway
   else 
      assert(false && "Unknown opcode for conditional branch immediate");
}

void power_instr::genCallAndLink(insnVec_t *piv, kptr_t from, 
                                 kptr_t to)
{
    assert(inRangeOfBranch(from, to)); // this better be the case, or we'll need to make sure ctr is not live here
    instr_t *i = new power_instr(branch, from-to, 0 /*not absolute */,  1 /* link */ );
    piv->appendAnInsn(i);
}


void power_instr::genCallAndLinkGeneric_unknownFromAddr(insnVec_t *piv,
                                                        kptr_t to)
{
   //This assumes all call targets will fit within 26 bits.  Is this always true? We'll see...
   //Otherwise, one would have to make sure the CTR register is not live before calling this.
   assert(to <= 0x3FFFFFF);
   instr_t *i = new power_instr(branch, 0-to, 0 /*not absolute */, 1 /* link */);
   piv->appendAnInsn(i);

}

void power_instr::
genCallAndLinkGeneric_unknownFromAndToAddr(insnVec_t *piv) {
   const kptr_t to = 0;
   genCallAndLinkGeneric_unknownFromAddr(piv, to);
}


void power_instr::gen32imm(insnVec_t  *piv, uint32_t val, 
                           power_reg rd) {
   unsigned high16 = (val & 0xffff0000) >> 16;
   unsigned low16 = (val & 0x0000ffff);
   assert((high16+low16)==val);
   
   instr_t *i;
   if (!high16) {
      //save an instruction by just using addi, which sign-extends
      i = new power_instr(addsub, addD, rd, power_reg::gpr0, low16);
      piv->appendAnInsn(i); 
      return;
   }
   
   //set the upper bits
   i = new power_instr(addsub, addShiftedD, rd, power_reg::gpr0, high16);
   piv->appendAnInsn(i); 
   
   //see if we can save an instruction, since addShifted sets lowerbits to zero anyway
   if ( low16) {
      i = new power_instr(logicor, false /* not shifted */ , rd, rd, low16);
      piv->appendAnInsn(i);
   }
}
void power_instr::gen64imm(insnVec_t *piv, uint64_t val, 
                           power_reg rd, power_reg rtemp) {
   const uint64_t high32bits = (val >> 32);
   const uint64_t low32bits = (val & 0xffffffffULL);
   assert(((high32bits << 32) | low32bits) == val);

   const unsigned high32bits_unsigned = high32bits;
   const unsigned low32bits_unsigned = low32bits;
   assert(high32bits_unsigned == high32bits);
   assert(low32bits_unsigned == low32bits);
   
   if (high32bits == 0) {
      assert(val == low32bits);
      const unsigned low32bits_unsigned = low32bits;
      gen32imm(piv, low32bits_unsigned, rd);
      return;
   }

   assert(val != low32bits);

   // We're definitely going to need a distinct scratch reg:
   assert(rtemp != rd);
   
   // generate high bits
   gen32imm(piv, high32bits_unsigned, rtemp);
   uimmediate<6> numToShift = 32;
   instr_t *i = new power_instr(power_instr::shift,
                                rtemp, rtemp, numToShift, 0 /*no CR mod */);
   piv->appendAnInsn(i);
   
   // generate low bits
   gen32imm(piv, low32bits_unsigned, rd);
   i = new power_instr(logicor, false /* do not complement */, rtemp, rd, rd, 0 /* no CR setting */);
   piv->appendAnInsn(i);
}
// Set the register to the given 32-bit address value
// Do not attempt to produce smaller code, since we may
// patch it with a different address later
void power_instr::genImmAddrGeneric(insnVec_t *piv, uint32_t addr,
                                    const power_reg &rd, bool setLow)
{
   unsigned high16 = (addr & 0xffff0000) >> 16;
   unsigned low16 = (addr & 0x0000ffff);
   assert((high16+low16)==addr);
   
   instr_t *i;
   
   //set the upper bits
   i = new power_instr(addsub, addShiftedD, rd, power_reg::gpr0, high16);
   piv->appendAnInsn(i); 
   if (setLow) { 
      //set lower bits
      i = new power_instr(logicor, false /* not shifted */ , rd, rd, low16);
      piv->appendAnInsn(i);
   }
}

// Set the register to the given 64-bit address value
// Do not attempt to produce smaller code, since we may
// patch it with a different address later
void power_instr::genImmAddrGeneric(insnVec_t *piv, uint64_t addr,
                                    const power_reg &rt, bool setLow)
{
   //Here is the code we are generating 
   // addis     rt,0, (((addr)>>48)&0xFFFF);             
   // ori     rt,rt,(((addr)>>32)&0xFFFF);         
   // rldicr  rt,rt,32,31;   (shift left 32 bits) 
   //  oris    rt,rt,(((addr)>>16)&0xFFFF);         
   //  ori     rt,rt,((addr)&0xFFFF);
   instr_t *instr = new power_instr(addsub, addShiftedD, rt,
				    power_reg::gpr0,
				    (((addr)>>48)&0xFFFF));
   piv->appendAnInsn(instr);
   
   instr = new power_instr(logicor, false, //not shifted, 
			   rt,
			   rt,
			   (((addr)>>32)&0xFFFF));
   piv->appendAnInsn(instr);
   
   instr = new power_instr(rotate, rotateLeftDoubleImmClearRightMD, rt, rt,
			   32,31, 
			   0); //no CR effects
   piv->appendAnInsn(instr);
   
   instr = new power_instr(logicor, true, //shifted,
			   rt,
			   rt,
			   (((addr)>>16)&0xFFFF));
   piv->appendAnInsn(instr);
   if (setLow) {
       instr = new power_instr(logicor, false, //not shifted,
			       rt,
			       rt,
			       ((addr)&0xFFFF));
       piv->appendAnInsn(instr);
   }
}
