// power_misc_regset.h
// Special-purpose regs like cond code

#ifndef _POWER_MISC_REGSET_H_
#define _POWER_MISC_REGSET_H_

#include <rpc/xdr.h>
#include "power_reg.h"
#include <inttypes.h>

struct XDR;

class power_misc_regset {

 private:
   union {
      uint64_t misc_stuff;
      struct {
         unsigned lr:1;
         unsigned ctr:1;
         
         //XER fields
         unsigned so:1;
         unsigned ov:1;
         unsigned xerresv:1;
         unsigned xerstrbytes:1;
         unsigned ca:1;        
         unsigned fpscr:1;
         //We treat CR fields as registers
         unsigned cr0:1;
         unsigned cr1:1;
         unsigned cr2:1;
         unsigned cr3:1;
         unsigned cr4:1;
         unsigned cr5:1;
         unsigned cr6:1;
         unsigned cr7:1;

         unsigned tb:1;
         unsigned tbu:1;
         unsigned asr:1;
         unsigned dar:1;
         unsigned dsisr:1;
         unsigned msr:1;
         unsigned dec:1;
         unsigned pvr:1;
         unsigned sdr1:1;
         unsigned srr0:1;
         unsigned srr1:1;
         unsigned ear:1;
         unsigned null:1; //Non-existent in the actual architecture, but
         //useful as default value and error-check.
         
         unsigned sprg0:1;
         unsigned sprg1:1;
         unsigned sprg2:1;
         unsigned sprg3:1;
         
         unsigned mmcra:1;
         unsigned pmc1:1;
         unsigned pmc2:1;
         unsigned pmc3:1;
         unsigned pmc4:1;
         unsigned pmc5:1;
         unsigned pmc6:1;
         unsigned pmc7:1;
         unsigned pmc8:1;
         unsigned mmcr0:1;
         unsigned siar:1;
         unsigned sdar:1;
         unsigned mmcr1:1;
         unsigned dabr:1;
         unsigned pir:1;
         unsigned hdec:1;
         unsigned accr:1;
         unsigned ctrl:1;
         unsigned padding:13;
      } ms;
   };
   
   class Raw {};
   class Random {};
   class Empty {};
   class Full {};
   
 public:
   power_misc_regset() : misc_stuff(0) {}
   
   power_misc_regset(XDR *);
   bool send(XDR *) const;

   static Raw raw;
   power_misc_regset(Raw, uint32_t imisc_stuff) : misc_stuff(imisc_stuff) {
      ms.padding = 0;
   }

   power_misc_regset(const power_reg &r);

   static Random random;
   power_misc_regset(Random);
      // Obviously, a (seemingly crazy) routine like this is only used in
      // regression torture test programs.

   static Empty empty;
   static Full full;
   
   power_misc_regset(Empty) : misc_stuff(0ULL) {}
   power_misc_regset(Full) {
      misc_stuff = 0xffffffffffffffffULL;
      ms.padding = 0;
   }
   
   power_misc_regset(const power_misc_regset &src) : misc_stuff(src.misc_stuff) {}

   void operator=(const power_misc_regset &src) { misc_stuff = src.misc_stuff; }

   uint32_t getRawBits() const { return misc_stuff; }

   bool operator==(const power_misc_regset &other) const {
      return misc_stuff == other.misc_stuff;
         // we don't mask misc_stuff by 0x3 since it should already
         // have been done by setting 'padding' to 0 for both "this" and "other"
   }
   bool operator==(const power_reg &r) const;
   bool operator!=(const power_misc_regset &other) const {
      return misc_stuff != other.misc_stuff;
         // we don't mask misc_stuff by 0x3 since it should have
         // been done already
   }

   bool exists(const power_misc_regset &subset) const {
      return 0x0ULL == (subset.misc_stuff & (~misc_stuff));
   }

   bool exists(const power_reg &r) const;
   
   bool existsLr() const { return ms.lr; }
   bool existsCtr() const { return ms.ctr;}
   bool existsSo() const { return ms.so; }
   bool existsOv() const { return ms.ov; }
   bool existsXerResv() const { return ms.xerresv;}
   bool existsXerStrBytes() const { return ms.xerstrbytes; }
   bool existsCa() const { return ms.ca; }
   bool existsFpscr() const { return ms.fpscr;}
   
   bool existsCrField(unsigned n) const { 
      return 0 != ((1ULL << (n+ 56) ) & misc_stuff); 
   }
 
   bool existsTb() const { return ms.tb; }
   bool existsTbu() const { return ms.tbu; }
   bool existsAsr() const { return ms.asr;}
   bool existsDar() const { return ms.dar; }
   bool existsDsisr() const { return ms.dsisr; }
   bool existsMsr() const { return ms.msr;}
   bool existsDec() const { return ms.dec; }
   bool existsPvr() const { return ms.pvr; }
   bool existsSdr1() const {return ms.sdr1; }
   bool existsSrr(unsigned n) const { 
      return 0 != ((1U << (n+ 25) ) & misc_stuff); 
   }
  
   bool existsEar() const { return ms.ear; }
   bool existsMmcra() const {return ms.mmcra; }
   bool existsMmcr0() const {return ms.mmcr0;}
   bool existsSiar() const {return ms.siar;}
   bool existsSdar() const {return ms.sdar;}
   bool existsMmcr1() const {return ms.mmcr1;}
   bool existsDabr() const {return ms.dabr;}
   bool existsPir() const {return ms.pir;}
   bool existsHdec() const {return ms.hdec;}
   bool existsAccr() const {return ms.accr;}
   bool existsCtrl() const {return ms.ctrl;}
   bool existsNull() const {return ms.null; }
   bool existsPmc( unsigned n) const {
       return 0 != ((1U << (n+34) ) & misc_stuff); 
   }
   bool existsBat( unsigned n) const {
       return false;
   }
   bool existsSprg( unsigned n) const {
      return 0 != ((1U << (n+29) ) & misc_stuff); 
   }

   bool isempty() const {
      return misc_stuff == 0;
   }
   void clear() {
      misc_stuff = 0;
   }

   void operator|=(const power_misc_regset &other) {
      misc_stuff |= other.misc_stuff; // OK for bit-fields?
   }
   void operator&=(const power_misc_regset &other) {
      misc_stuff &= other.misc_stuff; // okay on bit fields?
   }
   void operator-=(const power_misc_regset &other) {
      misc_stuff &= (~other.misc_stuff);
      ms.padding = 0; // necessary
   }

   unsigned count() const;

   power_reg expand1() const;
};

#endif
