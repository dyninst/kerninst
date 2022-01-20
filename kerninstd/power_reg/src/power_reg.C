// power_reg.C
// Igor Grobman, based on sparc_reg.C by Ariel Tamches

#include <assert.h>
#include "power_reg.h"
#include "common/h/String.h"

// Define static members:
power_reg::RAWIntReg power_reg::rawIntReg;
power_reg::FP     power_reg::f;

power_reg::LINKREG power_reg::lr_type;
power_reg::COUNTERREG power_reg::ctr_type;
power_reg::XEREG power_reg::xer_type;
power_reg::SO power_reg::so_type;
power_reg::OV power_reg::ov_type;
power_reg::CA power_reg::ca_type;
power_reg::XERRESV power_reg::xerresv_type;
power_reg::XERSTRBYTES power_reg::xerstrbytes_type;
power_reg::FPSCREG power_reg::fpscr_type;
power_reg::CONDREG power_reg::cr_type;
power_reg::CRFIELD power_reg::crfield_type;
power_reg::TBREG power_reg::tbr_type;
power_reg::ASREG power_reg::asr_type;
power_reg::DAREG power_reg::dar_type;
power_reg::DSISREG power_reg::dsisr_type;
power_reg::SPRGREG power_reg::sprg_type;
power_reg::MSREG power_reg::msr_type;
power_reg::DECREG power_reg::dec_type;
power_reg::PVREG power_reg::pvr_type;
power_reg::SDR1REG power_reg::sdr1_type;
power_reg::SRREG power_reg::srr_type;
power_reg::EAREG power_reg::ear_type;
power_reg::BATREG power_reg::bat_type;
power_reg::SPREG power_reg::spr_type;
power_reg::ACCREG power_reg::accr_type;
power_reg::CTRLREG power_reg::ctrl_type;
power_reg::HDECREG power_reg::hdec_type;
power_reg::MMCRAREG power_reg::mmcra_type;
power_reg::PMCREG power_reg::pmc_type;
power_reg::MMCR0REG power_reg::mmcr0_type;
power_reg::SIAREG power_reg::siar_type;
power_reg::SDAREG power_reg::sdar_type;
power_reg::MMCR1REG power_reg::mmcr1_type;
power_reg::DABREG power_reg::dabr_type;
power_reg::PIREG power_reg::pir_type;

power_reg::Null power_reg::null_type;


  //general purpose register declarations
power_reg power_reg::gpr0(rawIntReg, 0); 
power_reg power_reg::gpr1(rawIntReg, 1);
power_reg power_reg::gpr2(rawIntReg, 2); 
power_reg power_reg::gpr3(rawIntReg, 3);
power_reg power_reg::gpr4(rawIntReg, 4); 
power_reg power_reg::gpr5(rawIntReg, 5);
power_reg power_reg::gpr6(rawIntReg, 6); 
power_reg power_reg::gpr7(rawIntReg, 7); 
power_reg power_reg::gpr8(rawIntReg, 8); 
power_reg power_reg::gpr9(rawIntReg, 9);
power_reg power_reg::gpr10(rawIntReg, 10); 
power_reg power_reg::gpr11(rawIntReg, 11);
power_reg power_reg::gpr12(rawIntReg, 12);  
power_reg power_reg::gpr13(rawIntReg, 13);   
power_reg power_reg::gpr14(rawIntReg, 14);
power_reg power_reg::gpr15(rawIntReg, 15);  
power_reg power_reg::gpr16(rawIntReg, 16);   
power_reg power_reg::gpr17(rawIntReg, 17);
power_reg power_reg::gpr18(rawIntReg, 18);  
power_reg power_reg::gpr19(rawIntReg, 19);   
power_reg power_reg::gpr20(rawIntReg, 20);
power_reg power_reg::gpr21(rawIntReg, 21);  
power_reg power_reg::gpr22(rawIntReg, 22);   
power_reg power_reg::gpr23(rawIntReg, 23);
power_reg power_reg::gpr24(rawIntReg, 24);  
power_reg power_reg::gpr25(rawIntReg, 25);   
power_reg power_reg::gpr26(rawIntReg, 26);
power_reg power_reg::gpr27(rawIntReg, 27);  
power_reg power_reg::gpr28(rawIntReg, 28);   
power_reg power_reg::gpr29(rawIntReg, 29);
power_reg power_reg::gpr30(rawIntReg, 30);  
power_reg power_reg::gpr31(rawIntReg, 31); 

//floating point registers
power_reg power_reg::fpr0(f, 0);   
power_reg power_reg::fpr1(f, 1);    
power_reg power_reg::fpr2(f, 2);
power_reg power_reg::fpr3(f, 3);   
power_reg power_reg::fpr4(f, 4);    
power_reg power_reg::fpr5(f, 5);
power_reg power_reg::fpr6(f, 6);   
power_reg power_reg::fpr7(f, 7);    
power_reg power_reg::fpr8(f, 8);
power_reg power_reg::fpr9(f, 9);   
power_reg power_reg::fpr10(f, 10);   
power_reg power_reg::fpr11(f, 11);
power_reg power_reg::fpr12(f, 12);  
power_reg power_reg::fpr13(f, 13);   
power_reg power_reg::fpr14(f, 14);
power_reg power_reg::fpr15(f, 15);  
power_reg power_reg::fpr16(f, 16);   
power_reg power_reg::fpr17(f, 17);
power_reg power_reg::fpr18(f, 18);  
power_reg power_reg::fpr19(f, 19);   
power_reg power_reg::fpr20(f, 20);
power_reg power_reg::fpr21(f, 21);  
power_reg power_reg::fpr22(f, 22);   
power_reg power_reg::fpr23(f, 23);
power_reg power_reg::fpr24(f, 24);  
power_reg power_reg::fpr25(f, 25);   
power_reg power_reg::fpr26(f, 26);
power_reg power_reg::fpr27(f, 27);  
power_reg power_reg::fpr28(f, 28);   
power_reg power_reg::fpr29(f, 29);
power_reg power_reg::fpr30(f, 30);  
power_reg power_reg::fpr31(f, 31); 


//special registers

power_reg power_reg::lr(lr_type);    power_reg power_reg::ctr(ctr_type);     
power_reg power_reg::xer(xer_type);  power_reg power_reg::so(so_type);   
power_reg power_reg::ov(ov_type);    power_reg power_reg::ca(ca_type);
power_reg power_reg::xerresv(xerresv_type);  
power_reg power_reg::xerstrbytes(xerstrbytes_type);
power_reg power_reg::fpscr(fpscr_type);  
power_reg power_reg::cr(cr_type);     power_reg power_reg::asr(asr_type);
power_reg power_reg::dar(dar_type);   power_reg power_reg::dsisr(dsisr_type);   
power_reg power_reg::msr(msr_type);
power_reg power_reg::dec(dec_type);   power_reg power_reg::pvr(pvr_type);     
power_reg power_reg::sdr1(sdr1_type); power_reg power_reg::srr0(srr_type, 0);  
power_reg power_reg::srr1(srr_type, 1);  power_reg power_reg::ear(ear_type);
power_reg power_reg::accr(accr_type);  power_reg power_reg::ctrl(ctrl_type);
power_reg power_reg::hdec(hdec_type);  power_reg power_reg::mmcra(mmcra_type);
power_reg power_reg::mmcr0(mmcr0_type); power_reg power_reg::siar(siar_type);
power_reg power_reg::sdar(sdar_type);  power_reg power_reg::mmcr1(mmcr1_type);
power_reg power_reg::dabr(dabr_type); power_reg power_reg::pir(pir_type);

//Condition register fields
power_reg power_reg::cr0(crfield_type, 0);
power_reg power_reg::cr1(crfield_type, 1);    
power_reg power_reg::cr2(crfield_type, 2);
power_reg power_reg::cr3(crfield_type, 3);    
power_reg power_reg::cr4(crfield_type, 4);
power_reg power_reg::cr5(crfield_type, 5);    
power_reg power_reg::cr6(crfield_type, 6);
power_reg power_reg::cr7(crfield_type, 7);    

power_reg power_reg::tb(tbr_type, 0);    
power_reg power_reg::tbu(tbr_type, 1);

power_reg power_reg::sprg0(sprg_type,0);  
power_reg power_reg::sprg1(sprg_type,1); 
power_reg power_reg::sprg2(sprg_type, 2);  
power_reg power_reg::sprg3(sprg_type, 3);

power_reg power_reg::ibat0u(bat_type, 0);  
power_reg power_reg::ibat0l(bat_type, 1);
power_reg power_reg::ibat1u(bat_type, 2);  
power_reg power_reg::ibat1l(bat_type, 3);
power_reg power_reg::ibat2u(bat_type, 4);  
power_reg power_reg::ibat2l(bat_type, 5);
power_reg power_reg::ibat3u(bat_type, 6);  
power_reg power_reg::ibat3l(bat_type, 7);
power_reg power_reg::dbat0u(bat_type, 8);  
power_reg power_reg::dbat0l(bat_type, 9);
power_reg power_reg::dbat1u(bat_type, 10);  
power_reg power_reg::dbat1l(bat_type, 11);
power_reg power_reg::dbat2u(bat_type, 12);  
power_reg power_reg::dbat2l(bat_type, 13);
power_reg power_reg::dbat3u(bat_type, 14);  
power_reg power_reg::dbat3l(bat_type, 15);

power_reg power_reg::pmc1(pmc_type,1);
power_reg power_reg::pmc2(pmc_type,2);
power_reg power_reg::pmc3(pmc_type,3);
power_reg power_reg::pmc4(pmc_type,4);
power_reg power_reg::pmc5(pmc_type,5);
power_reg power_reg::pmc6(pmc_type,6);
power_reg power_reg::pmc7(pmc_type,7);
power_reg power_reg::pmc8(pmc_type,8);

power_reg power_reg::nullreg(null_type);


power_reg::power_reg(FP, unsigned n): reg_t(32+n) {
   assert(n < 32);
}

power_reg::power_reg(LINKREG): reg_t(64) {}
power_reg::power_reg(COUNTERREG): reg_t(65) {}
power_reg::power_reg(XEREG): reg_t(66) {}
power_reg::power_reg(SO): reg_t(109) {}
power_reg::power_reg(OV): reg_t(110) {}
power_reg::power_reg(CA): reg_t(111) {}
power_reg::power_reg(XERRESV): reg_t(112) {}
power_reg::power_reg(XERSTRBYTES): reg_t(113) {}
power_reg::power_reg(FPSCREG): reg_t(67) {}
power_reg::power_reg(CONDREG): reg_t(68) {}
power_reg::power_reg(CRFIELD, unsigned n): reg_t(101+n) {
   assert (n < 8);
}
power_reg::power_reg(TBREG, unsigned n): reg_t(69+n) {
   assert (n < 2);
}

power_reg::power_reg(ASREG): reg_t(71) {}
power_reg::power_reg(DAREG): reg_t(72) {}
power_reg::power_reg(DSISREG): reg_t(73) {}
power_reg::power_reg(SPRGREG, unsigned n): reg_t (74+n) {
   assert(n < 4);
}
power_reg::power_reg(MSREG): reg_t(78) {}
power_reg::power_reg(DECREG): reg_t(79) {}
power_reg::power_reg(PVREG): reg_t(80) {}
power_reg::power_reg(SDR1REG): reg_t(81) {}
power_reg::power_reg(SRREG, unsigned n): reg_t(82+n) {
   assert(n< 2);
}
power_reg::power_reg(EAREG): reg_t(84) {}
power_reg::power_reg(BATREG, unsigned n): reg_t(85+n) { 
   //IBT0U=0 ... DBAT3L=15
   assert(n < 16);
}
power_reg::power_reg(ACCREG): reg_t(114) {}
power_reg::power_reg(CTRLREG): reg_t(115) {}
power_reg::power_reg(MMCRAREG): reg_t(85) {}
power_reg::power_reg(PMCREG, unsigned n): reg_t(85+n) {
   assert (n > 0 && n < 9);
}
power_reg::power_reg(MMCR0REG): reg_t(94) {}
power_reg::power_reg(SIAREG): reg_t(95) {}
power_reg::power_reg(SDAREG): reg_t(96) {}
power_reg::power_reg(MMCR1REG): reg_t(97) {}
power_reg::power_reg(DABREG): reg_t(98) {}
power_reg::power_reg(PIREG): reg_t(99) {}
power_reg::power_reg(HDECREG): reg_t(100) {}

power_reg::power_reg(Null): reg_t(116) {}

pdstring power_reg::disass() const {
   if (regnum < 32)
      return "r" + pdstring(regnum);
   else if (regnum < 64) 
      return "f" + pdstring(regnum - 32); 
   else {
      switch(regnum) {
         case 64:
            return "lr";
         case 65: 
            return "ctr";
         case 66: 
            return "xer";
         case 67: 
            return "fpscr";
         case 68: 
            return "cr";
         case 69: 
            return "tb";
         case 70: 
            return "tbu";
         case 71: 
            return "asr";
         case 72: 
            return "dar";
         case 73: 
            return "dsisr";
         case 74: 
            return "sprg0";
         case 75: 
            return "sprg1";
         case 76: 
            return "sprg2";
         case 77: 
            return "sprg3";
         case 78: 
            return "msr";
         case 79: 
            return "dec";
         case 80: 
            return "pvr";
         case 81: 
            return "sdr1";
         case 82: 
            return "srr0";
         case 83: 
            return "srr1";
         case 84: 
            return "ear";
         case 85: 
            return "mmcra";
         case 86: 
            return "pmc1";
         case 87: 
            return "pmc2";
         case 88: 
            return "pmc3";
         case 89: 
            return "pmc4";
         case 90: 
            return "pmc5";
         case 91: 
            return "pmc6";
         case 92: 
            return "pmc7";
         case 93: 
            return "pmc8";
         case 94: 
            return "mmcr0";
         case 95: 
            return "siar";
         case 96: 
            return "sdar";
         case 97: 
            return "mmcr1";
         case 98: 
            return "dabr";
         case 99: 
            return "pir";
         case 100: 
            return "hdec";
         case 101: 
            return "cr0";
         case 102: 
            return "cr1";
         case 103: 
            return "cr2";
         case 104: 
            return "cr3";
         case 105: 
            return "cr4";
         case 106: 
            return "cr5";
         case 107: 
            return "cr6";
         case 108: 
            return "cr7";
         case 109: 
            return "so";
         case 110: 
            return "ov";
         case 111: 
            return "ca";
         case 112: 
            return "xerresv";
         case 113: 
            return "xerstr";
         case 114: 
            return "accr";
         case 115: 
            return "ctrl";
      }

   }

   return "";
}

bool power_reg::isParamReg(unsigned &num) const {
   if (regnum >= 3 && regnum <= 10) {
      num = regnum - 3;
      return true;
   }
   return false;
}
