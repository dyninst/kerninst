// sparc_instr.C
// Ariel Tamches

#include <assert.h>
#include <stdio.h> // sprintf()
#include "sparc_instr.h"
#include "util/h/out_streams.h"
#include "util/h/rope-utils.h" // num2string()

#include "insnVec.h"

// ----------------------------------------------------------------------

// define the static member vrbles (these are meant to be passed in as the
// first parameter to one of the sparc_instr constructors; making them each
// of a different type instead of one enumerated type really aids error
// checking.)

sparc_instr::Cas sparc_instr::cas;
sparc_instr::Load sparc_instr::load;
sparc_instr::LoadAltSpace sparc_instr::loadaltspace;
sparc_instr::LoadFP sparc_instr::loadfp;
sparc_instr::Store sparc_instr::store;
sparc_instr::StoreAltSpace sparc_instr::storealtspace;
sparc_instr::StoreFP sparc_instr::storefp;
sparc_instr::LoadStoreUB sparc_instr::loadstoreub;
sparc_instr::Swap sparc_instr::swap;
sparc_instr::SetHi sparc_instr::sethi;
sparc_instr::Nop sparc_instr::nop;
sparc_instr::PopC sparc_instr::popc;
sparc_instr::Prefetch sparc_instr::prefetch;
sparc_instr::PrefetchAltSpace sparc_instr::prefetchAltSpace;
sparc_instr::Logic sparc_instr::logic;
sparc_instr::BSet sparc_instr::bset;
sparc_instr::Cmp sparc_instr::cmp; // pseudo-instr; actually subcc
sparc_instr::Tst sparc_instr::tst; // pseudo-instr; actually orcc
sparc_instr::Mov sparc_instr::mov; // pseudo-instr, actually or
sparc_instr::Movcc sparc_instr::movcc; // new with v9
sparc_instr::MovR sparc_instr::movr; // new with v9
sparc_instr::FMovcc sparc_instr::fmovcc; // new with v9
sparc_instr::FMovR sparc_instr::fmovr; // new with v9
sparc_instr::Clear sparc_instr::clear; // pseudo-instr, actually or (if reg) or st (if mem)
sparc_instr::Shift sparc_instr::shift;
sparc_instr::SignX sparc_instr::signx;
sparc_instr::Not sparc_instr::_not; // not is now a reserved word in c++
sparc_instr::Neg sparc_instr::neg;
sparc_instr::Add sparc_instr::add;
sparc_instr::Sub sparc_instr::sub;
sparc_instr::MulX sparc_instr::mulx;
sparc_instr::SDivX sparc_instr::sdivx;
sparc_instr::UDivX sparc_instr::udivx;
sparc_instr::Save sparc_instr::save;
sparc_instr::Restore sparc_instr::restore;
sparc_instr::FlushW sparc_instr::flushw;
sparc_instr::Bicc sparc_instr::bicc;
sparc_instr::BPcc sparc_instr::bpcc;
sparc_instr::BPr sparc_instr::bpr;
sparc_instr::FBPfcc sparc_instr::fbpfcc;
sparc_instr::FBfcc sparc_instr::fbfcc;
sparc_instr::CallAndLink sparc_instr::callandlink;
sparc_instr::JumpAndLink sparc_instr::jumpandlink;
sparc_instr::Jump sparc_instr::jump;
sparc_instr::Ret sparc_instr::ret;
sparc_instr::Retl sparc_instr::retl;
sparc_instr::V9Return sparc_instr::v9return;
sparc_instr::Trap sparc_instr::trap;
sparc_instr::StoreBarrier sparc_instr::storebarrier;
sparc_instr::MemBarrier sparc_instr::membarrier;
sparc_instr::UnImp sparc_instr::unimp;
sparc_instr::Flush sparc_instr::flush;
sparc_instr::FAdd sparc_instr::fadd;
sparc_instr::FSub sparc_instr::fsub;
sparc_instr::FCmp sparc_instr::fcmp;
sparc_instr::Float2Int sparc_instr::float2int;
sparc_instr::Float2Float sparc_instr::float2float;
sparc_instr::Int2Float sparc_instr::int2float;
sparc_instr::FMov sparc_instr::fmov;
sparc_instr::FNeg sparc_instr::fneg;
sparc_instr::FAbs sparc_instr::fabs;
sparc_instr::FMul sparc_instr::fmul;
sparc_instr::FDiv sparc_instr::fdiv;
sparc_instr::FSqrt sparc_instr::fsqrt;
sparc_instr::ReadStateReg sparc_instr::readstatereg;
sparc_instr::WriteStateReg sparc_instr::writestatereg;
sparc_instr::ReadPrivReg sparc_instr::readprivreg;
sparc_instr::WritePrivReg sparc_instr::writeprivreg;

const sparc_instr::IntCondCodes sparc_instr::reversedIntCondCodes[16] = {
   // opposite of (0) iccNever is (8) iccAlways:
   iccAlways,
   // opposite of (1) iccEq is (9) iccNotEq:
   iccNotEq,
   // opposite of (2) iccLessOrEq is (10) iccGr:
   iccGr,
   // opposite of (3) iccLess is (11) iccGrOrEq:
   iccGrOrEq,
   // opposite of (4) iccLessOrEqUnsigned is (12) iccGrUnsigned:
   iccGrUnsigned,
   // opposite of (5) iccLessUnsigned is (13) iccGrOrEqUnsigned:
   iccGrOrEqUnsigned,
   // opposite of (6) iccNegative is (14) iccPositive:
   iccPositive,
   // opposite of (7) iccOverflowSet is (15) iccOverflowClear:
   iccOverflowClear,

   // opposite of (8) iccAlways is (0) iccNever:
   iccNever,
   // opposite of (9) iccNotEq is (1) iccEq:
   iccEq,
   // opposite of (10) iccGr is (2) iccLessOrEq:
   iccLessOrEq,
   // opposite of (11) iccGrOrEq is (3) iccLess:
   iccLess,
   // opposite of (12) iccGrUnsigned is (4) iccLessOrEqUnsigned:
   iccLessOrEqUnsigned,
   // opposite of (13) iccGrOrEqUnsigned is (5) iccLessUnsigned:
   iccLessUnsigned,
   // opposite of (14) iccPositive is (6) iccNegative:
   iccNegative,
   // opposite of (15) iccOverflowClear is (7) iccOverflowSet:
   iccOverflowSet
};

const sparc_instr::FloatCondCodes sparc_instr::reversedFloatCondCodes[16] = {
   // opp of (0) fccNever is (8) fccAlways:
   fccAlways,
   // opp of (1) fccNotEq [or unordered] is (9) fccEq:
   fccEq,
   // opp of (2) fccLessOrGr is (10) fccUnordOrEq:
   fccUnordOrEq,
   // opp of (3) fccUnordOrLess is (11) fccGrOrEq:
   fccGrOrEq,
   // opp of (4) fccLess is (12) fccUnordOrGrOrEq:
   fccUnordOrGrOrEq,
   // opp of (5) fccUnordOrGr is (13) fccLessOrEq:
   fccLessOrEq,
   // opp of (6) fccGr is (14) fccUnordOrLessOrEq:
   fccUnordOrLessOrEq,
   // opp of (7) fccUnord is (15) fccOrd:
   fccOrd,

   // opp of (8) fccAlways is (0) fccNever:
   fccNever,
   // opp of (9) fccEq is (1) fccNotEq [or unordered]:
   fccNotEq,
   // opp of (10) fccUnordOrEq is (2) fccLessOrGr:
   fccLessOrGr,
   // opp of (11) fccGrOrEq is (3) fccUnordOrLess:
   fccUnordOrLess,
   // opp of (12) fccUnordOrGrOrEq is (4) fccLess:
   fccLess,
   // opp of (13) fccLessOrEq is (5) fccUnordOrGr:
   fccUnordOrGr,
   // opp of (14) fccUnordOrLessOrEq is (6) fccGr:
   fccGr,
   // opp of (15) fccOrd is (7) fccUnord:
   fccUnord
};

const sparc_instr::RegCondCodes sparc_instr::reversedRegCondCodes[8] = {
   // opp of (0) regReserved1 is (4) regReserved2:
   regReserved2,
   // opp of (1) regZero is (5) regNotZero:
   regNotZero,
   // opp of (2) regLessOrEqZero is (6) regGrZero:
   regGrZero,
   // opp of (3) regLessZero is (7) regGrOrEqZero:
   regGrOrEqZero,

   // opp of (4) regReserved2 is (0) regReserved1:
   regReserved1,
   // opp of (5) regNotZero is (1) regZero:
   regZero,
   // opp of (6) regGrZero is (2) regLessOrEqZero:
   regLessOrEqZero,
   // opp of (7) regGrOrEqZero is (3) regLessZero:
   regLessZero
};

/* ------------------------------------------------------------------- */

class sparc_instr_heap {
 private:
//    unsigned num_allocs;
//    unsigned num_deallocs;
//    unsigned num_reuses;

   // The following deserve explanation: we want to pre-allocate memory
   // for sparc_instr's in large chunks (multiples of a page). Currently, 
   // sizeof(sparc_instr)==8, so 32768 insns will fit in 32 8KB pages.
   // If you change the sizeof(sparc_instr) by adding/removing data members,
   // please recalculate the number of insns
   static const unsigned chk_num_insns = 32768;
   static const unsigned chk_size = chk_num_insns*sizeof(sparc_instr);

   pdvector<void*> chunks;
   pdvector<unsigned> chk_numelems;
   pdvector<unsigned> chk_deletes;
   unsigned curr_fill_chk;
   bool delete_spot;
   std::pair<unsigned, void*> the_delete; 
      // first is chk ndx, second the newly available location
 public:
   sparc_instr_heap() : //num_allocs(0), num_deallocs(0), num_reuses(0), 
      delete_spot(false) {}

   ~sparc_instr_heap() {
      if(chunks.size()) {
         int i = 0;
         int finish = chunks.size();
         for(; i != finish; i++) {
             free(chunks[i]);
         }
      }
      //printUsage();
   }

   void* alloc() {
      void *result = NULL;
      //num_allocs++;

      // first time in alloc
      if(chunks.size() == 0) {
         result = malloc(chk_size);
         if(!result)
            assert(!"sparc_instr_heap::alloc - could not allocate memory\n");
         chunks.push_back(result);
         chk_numelems.push_back(1);
         chk_deletes.push_back(0);
         curr_fill_chk = 0;
         return result;
      }

      // reuse last deleted, if available. this is an optimization for the
      // behavior of getting a copy of an instr using getInstr() for use in
      // some sort of analysis, then deleting the copy before going onto the
      // next instr
      if(delete_spot) {
         //num_reuses++;
         unsigned chk = the_delete.first;
         result = the_delete.second;
         chk_deletes[chk]--;
         delete_spot = false;
         return result;
      }

      // find non-full chunk, if exists
      unsigned num_chunks = chunks.size();
      if(curr_fill_chk < num_chunks) {
         unsigned elems = chk_numelems[curr_fill_chk];
         if(elems < chk_num_insns) {
            result = (void*)((sparc_instr*)chunks[curr_fill_chk] 
                             + chk_numelems[curr_fill_chk]);
            chk_numelems[curr_fill_chk]++;
            if(chk_numelems[curr_fill_chk] == chk_num_insns)
               curr_fill_chk++;
            return result;
         }
      }

      // we're all full, create new chunk
      result = malloc(chk_size);
      if(!result)
         assert(!"sparc_instr_heap::alloc - could not allocate memory\n");
      chunks.push_back(result);
      chk_numelems.push_back(1);
      chk_deletes.push_back(0);
      return result;
   }

   void dealloc(void *insn) {
      //num_deallocs++;
      unsigned chks_size = chunks.size();
      int i = curr_fill_chk < chks_size ? curr_fill_chk : (chks_size-1);
      for(; i>=0; i--) {
         char *chk_start = (char*)chunks[i];
         char *chk_end = chk_start + chk_size;
         if(((char*)insn >= chk_start) && ((char*)insn < chk_end)) {
            chk_deletes[i]++;
            if(chk_deletes[i] == chk_num_insns) {
               void *last_chk_start = chunks[chks_size-1];
               unsigned last_chk_elems = chk_numelems[chks_size-1];
               unsigned last_chk_deletes = chk_deletes[chks_size-1];
               if(last_chk_start != chk_start) {
                  chunks[i] = last_chk_start;
                  chk_numelems[i] = last_chk_elems;
                  chk_deletes[i] = last_chk_deletes;
               }
               free((void*)chk_start);
               chunks.pop_back();
               chk_numelems.pop_back();
               chk_deletes.pop_back();
               if(delete_spot) {
                  if(the_delete.first == i) {
                     // delete spot was in this freed chunk, so clobber it
                     delete_spot = false;
                  }
               }
            }
            else {
               delete_spot = true;
               the_delete.first = i;
               the_delete.second = insn;
            }
            break;
         } 
      }
   }

//    void printUsage() {
//       cerr << "sparc_instr_heap Usage:\n" << "- #allocs : " << num_allocs
//            << "\n- #deallocs : " << num_deallocs << "\n- #reuses : "
//            << num_reuses << "\n- #chks : " << chunks.size() 
//            << "\n- chksize (in bytes) : " << chk_size << endl;
//    }
};

// noone outside this .C should play with the_insn_heap
static sparc_instr_heap the_insn_heap;

void* sparc_instr::operator new(size_t) 
{
   return the_insn_heap.alloc();
}

void sparc_instr::operator delete(void *insn) 
{
   if(!insn) return;
   the_insn_heap.dealloc(insn);
}

//----------------------------------------------------------------------

void sparc_instr::setImm13Common(uimmediate<2> hi2bits, sparc_reg destReg,
                                 uimmediate<6> op3, sparc_reg srcReg) {
   raw = 0;
   rollin(raw, hi2bits);
   rollinIntReg(raw, destReg);
   rollin(raw, op3);
   rollinIntReg(raw, srcReg);
   uimmediate<1> i(1); // i=1
   rollin(raw, i);
}

void sparc_instr::setImm13Common(uimmediate<2> hi2bits, uimmediate<5> rdBits,
                                 uimmediate<6> op3, sparc_reg srcReg) {
   raw = 0;
   rollin(raw, hi2bits);
   rollin(raw, rdBits);
   rollin(raw, op3);
   rollinIntReg(raw, srcReg);
   uimmediate<1> i(1); // i=1
   rollin(raw, i);
}

void sparc_instr::setSimm13Style(uimmediate<2> hi2bits, sparc_reg destReg,
                                 uimmediate<6> op3, sparc_reg srcReg,
                                 simmediate<13> constant) {
   setImm13Common(hi2bits, destReg, op3, srcReg);
   rollin(raw, constant);
}

void sparc_instr::setSimm13Style(uimmediate<2> hi2bits, uimmediate<5> rdBits,
                                 uimmediate<6> op3, sparc_reg srcReg,
                                 simmediate<13> constant) {
   setImm13Common(hi2bits, rdBits, op3, srcReg);
   rollin(raw, constant);
}

void sparc_instr::setUimm13Style(uimmediate<2> hi2bits, sparc_reg destReg,
                                 uimmediate<6> op3, sparc_reg srcReg,
                                 uimmediate<13> constant) {
   setImm13Common(hi2bits, destReg, op3, srcReg);
   rollin(raw, constant);
}

void sparc_instr::set2RegStyle(uimmediate<2> hi2bits, sparc_reg destReg,
                               uimmediate<6> op3, sparc_reg srcReg1,
                               uimmediate<8> asi, sparc_reg srcReg2) {
   raw = 0;
   rollin(raw, hi2bits);
   rollinIntReg(raw, destReg); // 5 bits
   rollin(raw, op3);     // 6 bits
   rollinIntReg(raw, srcReg1); // 5 bits
   uimmediate<1> i(0);   // i=0
   rollin(raw, i);
   rollin(raw, asi);     // 8 bits
   rollinIntReg(raw, srcReg2); // 5 bits
}

void sparc_instr::set2RegStyle(uimmediate<2> hi2bits,
                               uimmediate<5> rdBits,
                               uimmediate<6> op3, sparc_reg srcReg1,
                               uimmediate<8> asi, sparc_reg srcReg2) {
   raw = 0;
   rollin(raw, hi2bits);
   rollin(raw, rdBits); // 5 bits
   rollin(raw, op3);     // 6 bits
   rollinIntReg(raw, srcReg1); // 5 bits
   uimmediate<1> i(0);   // i=0
   rollin(raw, i);
   rollin(raw, asi);     // 8 bits
   rollinIntReg(raw, srcReg2); // 5 bits
}

uimmediate<6> sparc_instr::calcLogicalOp3(logical_types theop, bool modifycc) const {
   unsigned op3 = (unsigned)theop; // enumerated type has its op3 numbers.
   assert(op3 >= 1 && op3 <= 3 || op3 >= 5 && op3 <= 7);
   
   if (modifycc)
      op3 |= 0x10;
   
   return op3;
}

uimmediate<6> sparc_instr::calcAddOp3(bool modifyicc, bool carry) {
   if (!modifyicc && !carry)
      return 0x00;
   else if (!modifyicc && carry)
      return 0x08;
   else if (!carry)
      return 0x10; // modify icc, no carry
   else
      return 0x18; // modify icc, carry
}

uimmediate<6> sparc_instr::calcSubOp3(bool modifyicc, bool carry) {
   unsigned result = 0x04;
   if (carry)
      result += 0x08;
   if (modifyicc)
      result += 0x10;
   return result;
}

uimmediate<5> sparc_instr::calcFloatRegEncoding1(sparc_reg floatReg) {
   return floatReg.getFloatNum();
}

uimmediate<5> sparc_instr::calcFloatRegEncoding2(sparc_reg floatReg) {
   const unsigned regNum = floatReg.getFloatNum();
   assert(regNum % 2 == 0);
   
   unsigned result = (regNum & 0x1f) | ((regNum & 0x20) >> 5);
   return result;
}

uimmediate<5> sparc_instr::calcFloatRegEncoding4(sparc_reg floatReg) {
   const unsigned regNum = floatReg.getFloatNum();
   assert(regNum % 4 == 0);

   // interestingly, this is the same as for calcFloatRegEncoding2()
   unsigned result = (regNum & 0x1f) | ((regNum & 0x20) >> 5);

   assert((result & 0x2) == 0);
   return result;
}

uimmediate<5> sparc_instr::calcFloatRegEncoding(sparc_reg floatReg,
                                                unsigned precision) {
   switch (precision) {
      case 1:
         return calcFloatRegEncoding1(floatReg);
      case 2:
         return calcFloatRegEncoding2(floatReg);
      case 4:
         return calcFloatRegEncoding4(floatReg);
      default:
         assert(false);
         abort(); // placate compiler when compiling with NDEBUG
   }
}

unsigned sparc_instr::hi_of_32(uint32_t val) {
   // static
   return ::getBits(val, 10, 31);
}

unsigned sparc_instr::lo_of_32(uint32_t val) {
   // static
   return ::getBits(val, 0, 9);
}

unsigned sparc_instr::hi_of_64(uint64_t val64) {
   // static 
   uint32_t low32bits = val64 & 0xffffffff;
   return ::getBits(low32bits, 10, 31);
}

unsigned sparc_instr::lo_of_64(uint64_t val64) {
   // static 
   uint32_t low32bits = val64 & 0xffffffff;
   return ::getBits(low32bits, 0, 9);
}

unsigned sparc_instr::uhi_of_64(uint64_t val64) {
   // static 
   uint32_t hi32bits = val64 >> 32;
   return ::getBits(hi32bits, 10, 31);
}

unsigned sparc_instr::ulo_of_64(uint64_t val64) {
   // static
   uint32_t hi32bits = val64 >> 32;
   return ::getBits(hi32bits, 0, 9);
}

bool sparc_instr::instructionHasValidAlignment(kptr_t addr) {
   // used for assertions.  The x86 version would just always return true.
   return addr % 4 == 0;
}

bool sparc_instr::hasUniformInstructionLength() {
   // true for SPARC, not so for x86 of course.
   return true;
}

bool sparc_instr::condBranchesUseDelaySlot() {
   // yes, in this architecture (SPARC), conditional branches always have a
   // delay slot.  (Though not all *unconditional* branches do -- consider ba,a.)
   return true;
}

// ----------------------------------------------------------------------
// ----------------------- Making Instructions --------------------------
// ----------------------------------------------------------------------

sparc_instr::sparc_instr(Cas, bool extended, sparc_reg rs1, 
			 sparc_reg rs2, sparc_reg rd) : instr_t() {
   set2RegStyle(0x3, rd, extended ? 0x3e : 0x3c,
                rs1, 0x80, // ASI_PRIMARY
                rs2);
}

sparc_instr::sparc_instr(Load, LoadOps op,
                         sparc_reg srcAddrReg, simmediate<13> offset,
                         sparc_reg destReg) : instr_t() {
   uimmediate<6> op3 = op;
   setSimm13Style(0x3, destReg, op3, srcAddrReg, offset);
}

sparc_instr::sparc_instr(Load, LoadOps op,
                         sparc_reg srcAddrReg1, sparc_reg srcAddrReg2,
                         sparc_reg destReg) : instr_t() {
   uimmediate<6> op3 = op;
   set2RegStyle(0x3, destReg, op3, srcAddrReg1,
                0, // asi not yet supported
                srcAddrReg2);
}

sparc_instr::sparc_instr(bool, LoadAltSpace, LoadOps op,
                         sparc_reg srcAddrReg, simmediate<13> offset,
                         sparc_reg destReg) : instr_t() {
   uimmediate<6> op3 = op + 0x10;
   setSimm13Style(0x3, destReg, op3, srcAddrReg, offset);
}

sparc_instr::sparc_instr(bool, LoadFP, FPLoadOps op,
                         sparc_reg srcAddrReg, simmediate<13> offset,
                         sparc_reg destReg) : instr_t() {
   // no implicit multiplication of offset by 4, since the src addr reg
   // may not already contain a number divisible by 4.

   // Remember that sparc's floating-point registers have goofy encoding in the
   // instruction field, for backwards compatibility reasons.
   unsigned precision = (op == sparc_instr::ldSingleReg ? 1 :
                         op == sparc_instr::ldDoubleReg ? 2 :
                         op == sparc_instr::ldQuadReg ? 4 : 0);
   assert(precision);
   
   uimmediate<5> rdBits = calcFloatRegEncoding(destReg, precision);

   uimmediate<6> op3 = op;
   setSimm13Style(0x3, rdBits, op3, srcAddrReg, offset);
}

sparc_instr::sparc_instr(bool, LoadFP, FPLoadOps op,
                         sparc_reg srcAddrReg1, sparc_reg srcAddrReg2,
                         sparc_reg destReg) : instr_t() {
   // instr format is the same as above, except i=0 instead of 1,
   // and simm13 is 8 zeros followed by srcAddrReg2.
   
   // Remember that sparc's floating-point registers have goofy encoding in the
   // instruction field, for backwards compatibility reasons.
   unsigned precision = (op == sparc_instr::ldSingleReg ? 1 :
                         op == sparc_instr::ldDoubleReg ? 2 :
                         op == sparc_instr::ldQuadReg ? 4 : 0);
   assert(precision);
   
   uimmediate<5> rdBits = calcFloatRegEncoding(destReg, precision);
   
   uimmediate<6> op3 = op;

   set2RegStyle(0x3, rdBits, op3, srcAddrReg1, 0, srcAddrReg2);
}

sparc_instr::sparc_instr(Store, StoreOps op, 
			 sparc_reg srcValueReg,
                         sparc_reg addressReg, 
			 simmediate<13> offset) : instr_t() {
   uimmediate<6> op3 = op;
   setSimm13Style(0x3, srcValueReg, op3, addressReg, offset);
}

sparc_instr::sparc_instr(Store, StoreOps op, 
			 sparc_reg srcValueReg,
                         sparc_reg addressReg1, 
			 sparc_reg addressReg2) : instr_t() {
   uimmediate<6> op3 = op;
   set2RegStyle(0x3, srcValueReg, op3, addressReg1,
                0, // asi not yet implemented
                addressReg2);
}

sparc_instr::sparc_instr(StoreAltSpace, StoreOps op, 
			 sparc_reg srcValueReg,
                         sparc_reg addressReg, 
			 simmediate<13> offset) : instr_t() {
   uimmediate<6> op3 = op + 0x10;
   setSimm13Style(0x3, srcValueReg, op3, addressReg, offset);
}

sparc_instr::sparc_instr(StoreFP, FPStoreOps op, 
			 sparc_reg srcValFpReg,
                         sparc_reg addressReg, 
			 simmediate<13> offset) : instr_t() {
   const unsigned precision = (op == stSingleReg ? 1 :
                               op == stDoubleReg ? 2 : 4);

   // Can't use setSimm13Style since it chokes upon seeing 'srcValFpReg' isn't an
   // int reg
   uimmediate<6> op3 = op;

   raw = 0;
   rollin(raw, uimmediate<2>(0x3));
   rollinFpReg(raw, srcValFpReg, precision);
   rollin(raw, op3);
   rollinIntReg(raw, addressReg);
   uimmediate<1> i(1); // i=1
   rollin(raw, i);
   rollin(raw, offset);
}

sparc_instr::sparc_instr(StoreFP, FPStoreOps op, 
			 sparc_reg srcValFpReg,
                         sparc_reg addressReg1, 
			 sparc_reg addressReg2) : instr_t() {
   const unsigned precision = (op == stSingleReg ? 1 :
                               op == stDoubleReg ? 2 : 4);

   // can't use set2RegStyle since it chokes upon seeing 'srcValFpReg' isn't an int reg
   uimmediate<6> op3 = op;

   raw = 0;
   rollin(raw, uimmediate<2>(0x3));
   rollinFpReg(raw, srcValFpReg, precision); // 5 bits
   rollin(raw, op3);     // 6 bits
   rollinIntReg(raw, addressReg1); // 5 bits
   rollin(raw, uimmediate<1>(0)); // i == 0
   rollin(raw, uimmediate<8>(0)); 
   rollinIntReg(raw, addressReg2); // 5 bits
}

sparc_instr::sparc_instr(LoadStoreUB, sparc_reg srcAndDestReg,
                         sparc_reg addressReg, 
			 simmediate<13> offset) : instr_t() {
   setSimm13Style(0x03, srcAndDestReg,
                  0x0d, // op3 is 001101
                  addressReg, offset);
}

sparc_instr::sparc_instr(LoadStoreUB, sparc_reg srcAndDestReg,
                         sparc_reg addressReg1, 
			 sparc_reg addressReg2) : instr_t() {
   set2RegStyle(0x03, srcAndDestReg,
                0x0d, // alternate-space variant not yet implemented
                addressReg1,
                0, // asi not yet supported
                addressReg2);
}

sparc_instr::sparc_instr(Swap, sparc_reg destReg,
                         sparc_reg addressReg,
                         simmediate<13> offset) : instr_t() {
   setSimm13Style(0x3, destReg, 0x0f, // op3 is 001111
                  addressReg, offset);
}

sparc_instr::sparc_instr(Swap, sparc_reg destReg,
                         sparc_reg addressReg1, 
			 sparc_reg addressReg2) : instr_t() {
   set2RegStyle(0x3, destReg,
                0x0f, // 001111 -- alternate space version not yet supported
                addressReg1, 0, // asi not yet supported
                addressReg2);
}

sparc_instr::sparc_instr(SetHi, HiOf, uint32_t val, 
			 sparc_reg dest) : instr_t() {
   // HiOf in this context: bits 10 thru 31 of a 32-bit val
   sethicommon(hi_of_32(val), dest);
}

sparc_instr::sparc_instr(SetHi, HiOf64, uint64_t val, 
			 sparc_reg dest) : instr_t() {
   // HiOf in this context: bits 10 thru 31 of a 64-bit val
   sethicommon(hi_of_64(val), dest);
}

sparc_instr::sparc_instr(SetHi, UHiOf64, uint64_t val, 
			 sparc_reg dest) : instr_t() {
   // HiOf in this context: bits 42 thru 63 of a 64-bit val
   sethicommon(uhi_of_64(val), dest);
}

sparc_instr::sparc_instr(Nop) : instr_t() { // a variant of sethi
   sethicommon(0, sparc_reg::g0);
}


sparc_instr::sparc_instr(PopC, sparc_reg rs2, sparc_reg rd) : instr_t() {
   set2RegStyle(0x2, rd, 0x2e, sparc_reg::g0, 0, rs2);
}

sparc_instr::sparc_instr(Prefetch, PrefetchFcn fcn, 
			 sparc_reg rs1, sparc_reg rs2) : instr_t() {
   set2RegStyle(0x3, sparc_reg(sparc_reg::rawIntReg, fcn),
                0x2d, rs1, 0, rs2);
}

sparc_instr::sparc_instr(Prefetch, PrefetchFcn fcn,
                         sparc_reg rs1, simmediate<13> offset) : instr_t() {
   setSimm13Style(0x3, sparc_reg(sparc_reg::rawIntReg, fcn),
                  0x2d, rs1, offset);
}

sparc_instr::sparc_instr(PrefetchAltSpace, PrefetchFcn fcn,
                         sparc_reg rs1, sparc_reg rs2) : instr_t() {
   set2RegStyle(0x3, sparc_reg(sparc_reg::rawIntReg, fcn),
                0x3d, rs1, 0, rs2);
}

sparc_instr::sparc_instr(PrefetchAltSpace, PrefetchFcn fcn,
                         sparc_reg rs1, simmediate<13> offset) : instr_t() {
   setSimm13Style(0x3, sparc_reg(sparc_reg::rawIntReg, fcn),
                  0x3d, rs1, offset);
}

sparc_instr::sparc_instr(Logic, logical_types theop, bool cc,
                         sparc_reg dest, sparc_reg src1, 
			 sparc_reg src2) : instr_t() {
   setto_logic(theop, cc, dest, src1, src2);
}

sparc_instr::sparc_instr(Logic, logical_types theop, bool cc,
                         sparc_reg dest, sparc_reg src, 
			 simmediate<13> constant) : instr_t() {
   // imho, it's weird for logical operations like this one to sign extend
   // the constant operand...but sparc does just that, so we must take in
   // an simmediate<13>, not a uimmediate<13>
   setto_logic(theop, cc, dest, src, constant);
}

sparc_instr::sparc_instr(bool, BSet, simmediate<13> constant, 
			 sparc_reg reg) : instr_t() {
   // imho, it's weird for logical operations like this one to sign extend
   // the constant operand...but sparc does just that, so we must take in
   // an simmediate<13>, not a uimmediate<13>
   setto_logic(logic_or, false, reg, reg, constant);
}

sparc_instr::sparc_instr(Mov, sparc_reg rs1, sparc_reg rd) : instr_t() {
   // mov rs1, destReg   (a pseudo-instr; actually or rs1, 0, destReg)
   setto_logic(logic_or, false, rd, sparc_reg::g0, rs1);
}

void sparc_instr::sethicommon(unsigned val, sparc_reg dest) {
   raw = 0; // high 2 bits 00
   rollinIntReg(raw, dest);
   uimmediate<3> temp(4); // 100
   rollin(raw, temp);
   
   uimmediate<22> imm22 = val;
   rollin(raw, imm22);
}

sparc_instr::sparc_instr(Cmp, sparc_reg rs1, sparc_reg rs2) : instr_t() {
   uimmediate<6> op3 = calcSubOp3(true, false); // cc, no carry
   set2RegStyle(0x2, sparc_reg::g0, op3, rs1, 0, rs2);
}

sparc_instr::sparc_instr(Cmp, sparc_reg rs1, 
			 simmediate<13> val) : instr_t() {
   uimmediate<6> op3 = calcSubOp3(true, false); // cc, no carry
   setSimm13Style(0x2, sparc_reg::g0, op3, rs1, val);
}

sparc_instr::sparc_instr(Tst, sparc_reg rs1) : instr_t() {
   setto_logic(logic_or, true, // cc
               sparc_reg::g0, // dest
               rs1,
               sparc_reg::g0);
}

void sparc_instr::setto_logic(logical_types lt, bool cc,
                              sparc_reg dest, sparc_reg src1, sparc_reg src2) {
   uimmediate<6> op3 = calcLogicalOp3(lt, cc);
   set2RegStyle(0x2, dest, op3, src1, 0, src2);
}

void sparc_instr::setto_logic(logical_types lt, bool cc,
                              sparc_reg dest, sparc_reg src, simmediate<13> constant) {
   uimmediate<6> op3 = calcLogicalOp3(lt, cc);
   setSimm13Style(0x2, dest, op3, src, constant);
}

sparc_instr::sparc_instr(Mov, simmediate<13> constant, 
			 sparc_reg destReg) : instr_t() {
   // mov const, destReg   (actually a pseudo-instr using or)
//     unsigned constantAsRaw=0; // need to treat the constant as unsigned
//     rollin(constantAsRaw, constant);

   setto_logic(logic_or, false, destReg, sparc_reg::g0, constant);
}

sparc_instr::sparc_instr(Movcc, unsigned cond, bool xcc,
                         sparc_reg srcReg, sparc_reg destReg) : instr_t() {
   raw = 0x2; // high 2 bits: 10
   rollinIntReg(raw, destReg);

   uimmediate<6> op3 = 0x2c; // 10 1100
   rollin(raw, op3);

   uimmediate<1> cc2 = 1;
   rollin(raw, cc2);

   uimmediate<4> condbits = cond;
   rollin(raw, condbits);
   
   uimmediate<1> ibit = 0;
   rollin(raw, ibit);
   
   uimmediate<1> cc1 = xcc ? 1 : 0;
   rollin(raw, cc1);
   
   uimmediate<1> cc0 = 0;
   rollin(raw, cc0);

   uimmediate<6> unused = 0;
   rollin(raw, unused);
   
   rollinIntReg(raw, srcReg);
}

sparc_instr::sparc_instr(Movcc, unsigned cond,
                         unsigned fcc_num, // 0 thru 3
                         sparc_reg srcReg, sparc_reg destReg) : instr_t() {
   assert(fcc_num < 4);
   
   raw = 0x2; // high 2 bits: 10

   rollinIntReg(raw, destReg);

   uimmediate<6> op3 = 0x2c; // 10 1100
   rollin(raw, op3);

   uimmediate<1> cc2 = 0;
   rollin(raw, cc2);
   
   uimmediate<4> condbits = cond;
   rollin(raw, condbits);
   
   uimmediate<1> ibit = 0;
   rollin(raw, ibit);

   uimmediate<2> cc1andcc0 = fcc_num;
   rollin(raw, cc1andcc0);

   uimmediate<6> unused = 0;
   rollin(raw, unused);
   
   rollinIntReg(raw, srcReg);
}

sparc_instr::sparc_instr(MovR, unsigned cond, sparc_reg rs1, sparc_reg rs2,
                         sparc_reg rd) : instr_t() {
   raw = 0x2;
   rollinIntReg(raw, rd);
   rollin(raw, uimmediate<6>(0x2f));
   rollinIntReg(raw, rs1);
   rollin(raw, uimmediate<1>(0));
   uimmediate<3> condbits = cond;
   rollin(raw, condbits);
   rollin(raw, uimmediate<5>(0));
   rollinIntReg(raw, rs2);
}
   
sparc_instr::sparc_instr(MovR, unsigned cond, sparc_reg rs1, int val,
                         sparc_reg rd) : instr_t() {
   raw = 0x2;
   rollinIntReg(raw, rd);
   rollin(raw, uimmediate<6>(0x2f));
   rollinIntReg(raw, rs1);
   rollin(raw, uimmediate<1>(1));
   uimmediate<3> condbits = cond;
   rollin(raw, condbits);
   simmediate<10> simm10(val);
   rollin(raw, simm10);
}

sparc_instr::sparc_instr(FMovcc, unsigned precision,
                         unsigned cond, bool xcc,
                         sparc_reg rs2, sparc_reg rd // float regs
                         ) : instr_t() {
   raw = 0x2;
   rollinFpReg(raw, rd, precision);
   rollin(raw, uimmediate<6>(0x35));
   rollin(raw, uimmediate<1>(0));
   rollin(raw, uimmediate<4>(cond));
   rollin(raw, uimmediate<3>(xcc ? 0x6 : 0x4));

   const uimmediate<6> opf_low = precision == 1 ? 0x1 :
                                 precision == 2 ? 0x2 : 0x3;
   rollin(raw, opf_low);

   rollinFpReg(raw, rs2, precision);
}

sparc_instr::sparc_instr(FMovcc, unsigned precision,
                         unsigned cond, unsigned fcc_num,
                         sparc_reg rs2, sparc_reg rd // float regs
                         ) : instr_t() {
   raw = 0x2;
   rollinFpReg(raw, rd, precision);
   rollin(raw, uimmediate<6>(0x35));
   rollin(raw, uimmediate<1>(0));
   rollin(raw, uimmediate<4>(cond));
   rollin(raw, uimmediate<3>(fcc_num));

   const uimmediate<6> opf_low = precision == 1 ? 0x1 :
                                 precision == 2 ? 0x2 : 0x3;
   rollin(raw, opf_low);

   rollinFpReg(raw, rs2, precision);
}

sparc_instr::sparc_instr(FMovR, unsigned precision, unsigned cond,
                         sparc_reg rs1, // int reg
                         sparc_reg rs2, // fp reg
                         sparc_reg rd // fp reg
                         ) : instr_t() {
   assert(rs1.isIntReg());
   assert(rs2.isFloatReg());
   assert(rd.isFloatReg());
   assert(precision == 1 || precision == 2 || precision == 4);

   raw = 0x2;
   rollinFpReg(raw, rd, precision);
   rollin(raw, uimmediate<6>(0x35));
   rollinIntReg(raw, rs1);
   rollin(raw, uimmediate<1>(0));
   rollin(raw, uimmediate<3>(cond));

   uimmediate<5> opf_low = (precision == 1 ? 0x05 :
                            precision == 2 ? 0x06 : 0x07);
   rollin(raw, opf_low);

   rollinFpReg(raw, rs2, precision);
}

sparc_instr::sparc_instr(Clear, sparc_reg destReg) : instr_t() {
   // clr rd is actually or %g0, %g0, rd
   setto_logic(logic_or, false, destReg, sparc_reg::g0, sparc_reg::g0);
}

sparc_instr::sparc_instr(Clear, StoreOps op, sparc_reg addrReg1, 
			 sparc_reg addrReg2) : instr_t() {
   // clr [rs1 + rs2] is actually st %g0, [rs1 + rs2] (or stb, sth or stx but
   // don't use std)
   assert(op != stDoubleWord);
   *this = sparc_instr(store, op, sparc_reg::g0, addrReg1, addrReg2);
}

sparc_instr::sparc_instr(Clear, StoreOps op, sparc_reg addrReg1, 
			 simmediate<13> offset) : instr_t() {
   // clr [rs1 + offset] is actually st %g0, [rs1 + offset] (or stb, sth or stx but
   // don't use std)
   assert(op != stDoubleWord);
   *this=sparc_instr(store, op, sparc_reg::g0, addrReg1, offset);
}

sparc_instr::sparc_instr(Shift, ShiftKind kind,
                         bool extended,
                         sparc_reg destReg, sparc_reg srcReg1,
                         sparc_reg srcReg2) : instr_t() {
   // reg[destReg] <- (srcReg1 << srcReg2)
   uimmediate<6> op3 = kind;
   set2RegStyle(0x2, destReg, op3, srcReg1, extended ? 0x80 : 0, srcReg2);
}

sparc_instr::sparc_instr(Shift,
                         ShiftKind kind,
                         bool extended,
                         sparc_reg destReg, sparc_reg srcReg,
                         uimmediate<6> constant) : instr_t() {
   // reg[destReg] <- (srcReg << constant)
   
   // can't use set2RegStyle since this is an unusual instruction...i=1 yet we
   // don't use a 13 bit signed constant, we use a 5 bit unsigned constant preceded
   // with 8 zeros.
   
   raw = 0x2; // high 2 bits 10
   rollinIntReg(raw, destReg);
   
   uimmediate<6> op3 = kind;
   rollin(raw, op3);

   rollinIntReg(raw, srcReg);

   uimmediate<1> i(1);
   rollin(raw, i);

   uimmediate<1> x(extended ? 1 : 0);
   rollin(raw, x);

   uimmediate<6> unused(0);
   rollin(raw, unused);

   rollin(raw, constant);
}

sparc_instr::sparc_instr(SignX, sparc_reg r) : instr_t() {
   // can't use set2RegStyle since this is an unusual instruction...i=1 yet we
   // don't use a 13 bit signed constant, we use a 5 bit unsigned constant preceded
   // with 8 zeros.
   
   raw = 0x2; // high 2 bits 10
   rollinIntReg(raw, r);
   
   uimmediate<6> op3 = rightArithmetic; // enum ShiftKind
   rollin(raw, op3);

   rollinIntReg(raw, r);
   uimmediate<1> i(1);
   rollin(raw, i);

   uimmediate<1> x = 0; // NOT extended
   rollin(raw, x);

   rollin(raw, uimmediate<7>(0)); // unused 7 bits
   rollin(raw, uimmediate<5>(0)); // shift amount: 0
}

sparc_instr::sparc_instr(SignX, sparc_reg src, sparc_reg dest) : instr_t() {
   // can't use set2RegStyle since this is an unusual instruction...i=1 yet we
   // don't use a 13 bit signed constant, we use a 5 bit unsigned constant preceded
   // with 8 zeros.
   
   raw = 0x2; // high 2 bits 10
   rollinIntReg(raw, dest);
   
   uimmediate<6> op3 = rightArithmetic; // enum ShiftKind
   rollin(raw, op3);

   rollinIntReg(raw, src);
   uimmediate<1> i(1);
   rollin(raw, i);

   uimmediate<1> x = 0; // NOT extended
   rollin(raw, x);

   uimmediate<7> unused(0);
   rollin(raw, unused);

   rollin(raw, uimmediate<5>(0)); // shift amount: 0
}

sparc_instr::sparc_instr(Not, sparc_reg r) : instr_t() {
   setto_logic(logic_xnor, false, // no cc
               r, // dest
               sparc_reg::g0, r);
}

sparc_instr::sparc_instr(Not, sparc_reg rs1, sparc_reg rd) : instr_t() {
   setto_logic(logic_xnor, false, // no cc
               rd, // dest
               sparc_reg::g0, rs1);
}

sparc_instr::sparc_instr(Neg, sparc_reg r) : instr_t() {
   uimmediate<6> op3 = calcSubOp3(false, false); // no cc, no carry
   set2RegStyle(0x2, r, // dest
                op3, sparc_reg::g0, 0, r);
}

sparc_instr::sparc_instr(Neg, sparc_reg rs2, sparc_reg rd) : instr_t() {
   uimmediate<6> op3 = calcSubOp3(false, false); // no cc, no carry
   set2RegStyle(0x2, rd, // dest
                op3, sparc_reg::g0, 0, rs2);
}

sparc_instr::sparc_instr(Add, bool modifyicc, bool carry,
                         sparc_reg destReg, sparc_reg srcReg1,
                         sparc_reg srcReg2) : instr_t() {
   uimmediate<6> op3 = calcAddOp3(modifyicc, carry);
   set2RegStyle(0x2, destReg, op3, srcReg1, 0, srcReg2);
}

sparc_instr::sparc_instr(Add, bool modifyicc, bool carry,
                         sparc_reg destReg, sparc_reg srcReg,
                         simmediate<13> constant) : instr_t() {
   uimmediate<6> op3 = calcAddOp3(modifyicc, carry);
   setSimm13Style(0x2, destReg, op3, srcReg, constant);
}

sparc_instr::sparc_instr(Add, sparc_reg rs1, sparc_reg rs2, 
			 sparc_reg rd) : instr_t() {
   uimmediate<6> op3 = calcAddOp3(false, false); // no cc, no carry
   set2RegStyle(0x2, rd, op3, rs1, 0, rs2);
}

sparc_instr::sparc_instr(Add, sparc_reg rs1, simmediate<13> constant,
                         sparc_reg rd) : instr_t() {
   uimmediate<6> op3 = calcAddOp3(false, false); // no cc, no carry
   setSimm13Style(0x2, rd, op3, rs1, constant);
}


sparc_instr::sparc_instr(Sub, bool modifyicc, bool carry,
                         sparc_reg destReg, sparc_reg srcReg1,
                         sparc_reg srcReg2) : instr_t() {
   uimmediate<6> op3 = calcSubOp3(modifyicc, carry);
   set2RegStyle(0x2, destReg, op3, srcReg1, 0, srcReg2);
}

sparc_instr::sparc_instr(Sub, bool modifyicc, bool carry,
                         sparc_reg destReg, sparc_reg srcReg,
                         simmediate<13> constant) : instr_t() {
   uimmediate<6> op3 = calcSubOp3(modifyicc, carry);
   setSimm13Style(0x2, destReg, op3, srcReg, constant);
}

sparc_instr::sparc_instr(Sub, sparc_reg rs1, sparc_reg rs2, 
			 sparc_reg rd) : instr_t() {
   uimmediate<6> op3 = calcSubOp3(false, false); // no cc, no carry
   set2RegStyle(0x2, rd, op3, rs1, 0, rs2);
}

sparc_instr::sparc_instr(Sub, sparc_reg rs1, simmediate<13> constant,
                         sparc_reg rd) : instr_t() {
   uimmediate<6> op3 = calcSubOp3(false, false); // no cc, no carry
   setSimm13Style(0x2, rd, op3, rs1, constant);
}

sparc_instr::sparc_instr(MulX, sparc_reg rs1, sparc_reg rs2, 
			 sparc_reg rd) : instr_t() {
   set2RegStyle(0x2, rd, 0x09, rs1, 0, rs2);
}

sparc_instr::sparc_instr(MulX, sparc_reg rs1, simmediate<13> val, 
			 sparc_reg rd) : instr_t() {
   setSimm13Style(0x2, rd, 0x09, rs1, val);
}

sparc_instr::sparc_instr(SDivX, sparc_reg rs1, sparc_reg rs2, 
			 sparc_reg rd) : instr_t() {
   set2RegStyle(0x2, rd, 0x2d, rs1, 0, rs2);
}

sparc_instr::sparc_instr(SDivX, sparc_reg rs1, simmediate<13> val, 
			 sparc_reg rd) : instr_t() {
   setSimm13Style(0x2, rd, 0x2d, rs1, val);
}

sparc_instr::sparc_instr(UDivX, sparc_reg rs1, sparc_reg rs2, 
			 sparc_reg rd) : instr_t() {
   set2RegStyle(0x2, rd, 0x2d, rs1, 0, rs2);
}

sparc_instr::sparc_instr(UDivX, sparc_reg rs1, simmediate<13> val, 
			 sparc_reg rd) : instr_t() {
   // believe it or not, despite this instruction being an explicitly *unsigned*
   // divide, the imm13 is still sign-extended (and treated as an unsigned value!)
   setSimm13Style(0x2, rd, 0x2d, rs1, val);
}

sparc_instr::sparc_instr(Save, simmediate<13> constant) : instr_t() {
   setSimm13Style(0x2,
                  sparc_reg::sp,
                  0x3c, // op3 111100
                  sparc_reg::sp,
                  constant);
}

sparc_instr::sparc_instr(Save, sparc_reg destReg, sparc_reg srcReg1,
                         sparc_reg srcReg2) : instr_t() {
   set2RegStyle(0x2, destReg,
                0x3c, // op3 111100
                srcReg1, 0, srcReg2);
}

sparc_instr::sparc_instr(Save, sparc_reg destReg, sparc_reg srcReg,
                         simmediate<13> constant) : instr_t() {
   setSimm13Style(0x2, destReg, 0x3c, // op3 111100
                  srcReg, constant);
}

sparc_instr::sparc_instr(Restore) : instr_t() {
   set2RegStyle(0x2,
                sparc_reg::g0, // dest
                0x3d, // op3 111101
                sparc_reg::g0, // src1
                0,
                sparc_reg::g0 // src2
                );
}

sparc_instr::sparc_instr(bool, Restore,
                         sparc_reg srcReg1, sparc_reg srcReg2, 
			 sparc_reg destReg) : instr_t() {
   set2RegStyle(0x2, destReg, 0x3d, // op3 111101
                srcReg1, 0, srcReg2);
}

sparc_instr::sparc_instr(Restore,
                         sparc_reg srcReg, simmediate<13> constant, 
			 sparc_reg destReg) : instr_t() {
   setSimm13Style(0x2, destReg, 0x3d, // op3 111101
                  srcReg, constant);
}

sparc_instr::sparc_instr(FlushW) : instr_t() {
   set2RegStyle(0x2,
                sparc_reg::g0, // dest
		0x2b, // op3 101011
		sparc_reg::g0, // src1
		0,
		sparc_reg::g0); // src2
}

sparc_instr::sparc_instr(Bicc, unsigned cond, bool annulled,
                         kaddrdiff_t displacement_bytes) : instr_t() {
   raw = 0x0; // high 2 bits 00
   uimmediate<1> abit(annulled ? 1 : 0);
   rollin(raw, abit);
   
   uimmediate<4> condbits = cond;
   rollin(raw, condbits);

   uimmediate<3> temp(2); // 010
   rollin(raw, temp);

   assert(displacement_bytes % 4 == 0);
   simmediate<22> displacement_ninsns = displacement_bytes / 4;
   rollin(raw, displacement_ninsns);
}

sparc_instr::sparc_instr(BPcc, unsigned cond, bool annulled,
                         bool predictTaken,
			 bool xcc, // if false, then icc is used
                         kaddrdiff_t displacement_bytes
                         ) : instr_t() {
   raw = 0x0; // high 2 bits 00
   uimmediate<1> abit(annulled ? 1 : 0);
   rollin(raw, abit);

   uimmediate<4> condBits = cond;
   rollin(raw, condBits);

   uimmediate<3> temp(1); // 001
   rollin(raw, temp);

   uimmediate<2> ccbits(xcc ? 2 : 0); // 1 and 3 are reserved
   rollin(raw, ccbits);

   abit = predictTaken ? 1 : 0;
   rollin(raw, abit);

   assert(displacement_bytes % 4 == 0);
   simmediate<19> displacement_ninsns = displacement_bytes / 4;
   rollin(raw, displacement_ninsns);
}

sparc_instr::sparc_instr(BPr, unsigned cond, bool annulled, 
			 bool predictTaken,
			 sparc_reg rs1,
                         kaddrdiff_t displacement_nbytes) : instr_t() {
   raw = 0x0; // high 2 bits 00

   uimmediate<1> bit(annulled ? 1 : 0);
   rollin(raw, bit);

   bit = 0; // bit 28 must be zero for BPr
   rollin(raw, bit);

   uimmediate<3> condbits = cond;
   rollin(raw, condbits);

   uimmediate<3> midbits = 0x3; // bits 22-24 must be 011
   rollin(raw, midbits);

   // bits 20-21 get the highest 2 bits of displacement_ninsns
   assert(displacement_nbytes % 4 == 0);
   kaddrdiff_t displacement_ninsns = displacement_nbytes / 4;

   uimmediate<2> disp16hibits = (displacement_ninsns >> 14) & 0x3;
   rollin(raw, disp16hibits);

   bit = predictTaken;
   rollin(raw, bit);

   rollinIntReg(raw, rs1);

   uimmediate<14> disp16lobits = (displacement_ninsns & 0x3fff);
   rollin(raw, disp16lobits);
}

sparc_instr::sparc_instr(FBfcc, unsigned cond, bool annulled,
                         kaddrdiff_t displacement_nbytes) : instr_t() {
   raw = 0x0; // high 2 bits 00
   uimmediate<1> abit(annulled ? 1 : 0);
   rollin(raw, abit);
   
   uimmediate<4> condBits = cond;
   rollin(raw, condBits);
   
   uimmediate<3> temp(6); // 110
   rollin(raw, temp);

   assert(displacement_nbytes % 4 == 0);
   simmediate<22> displacement_ninsns = displacement_nbytes / 4;
   rollin(raw, displacement_ninsns);
}

sparc_instr::sparc_instr(FBPfcc, unsigned cond,
                         bool annulled, bool predictTaken,
                         unsigned fcc_num, /* 0 thru 3...for fcc0 thru fcc3 */
                         kaddrdiff_t displacement_nbytes) : instr_t() {
   raw = 0x0; // high 2 bits 00
   uimmediate<1> abit(annulled ? 1 : 0);
   rollin(raw, abit);
   
   uimmediate<4> condBits = cond;
   rollin(raw, condBits);
   
   uimmediate<3> temp(5); // 101
   rollin(raw, temp);
   
   uimmediate<2> ccbits(fcc_num);
   rollin(raw, ccbits);
   
   abit = predictTaken ? 1 : 0;
   rollin(raw, abit);

   assert(displacement_nbytes % 4 == 0);
   simmediate<19> displacement_ninsns = displacement_nbytes / 4;
   rollin(raw, displacement_ninsns);
}

sparc_instr::sparc_instr(CallAndLink, kaddrdiff_t displacement) : instr_t() {
   raw = 0x1; // high 2 bits 01
   
   // make sure the call insn has enough range!
   // Unfortunately, the assert has to be fairly clever, to avoid wraparound.
   if (displacement < 0)
      assert(inRangeOfCall(-displacement, // dummy from-addr
                           0 // dummy to-addr
                           ));
   else
      assert(inRangeOfCall(0, displacement));

   assert(displacement % 4 == 0);
   simmediate<30> disp30 = displacement / 4;
   
   rollin(raw, disp30);
}

sparc_instr::sparc_instr(CallAndLink, kptr_t addr, 
			 kptr_t currPC) : instr_t() {
//   const uint32_t displacement = addr - currPC;
      // immune to unsigned arith underflow?  Probably OK.

   kaddrdiff_t displacement = addr - currPC;
      // unsigned arithm OK

   assert(inRangeOfCall(currPC, // fromaddr
                        addr // toaddr
                        ));

   assert(displacement % 4 == 0);

   kaddrdiff_t displacement_div4 = displacement / 4;

   raw = 0x1; // high 2 bits 01
   simmediate<30> disp30 = displacement_div4;
   rollin(raw, disp30);
}

sparc_instr::sparc_instr(CallAndLink, sparc_reg callee) : instr_t() {
   // actually jmpl [callee + 0], %o7
   set2RegStyle(0x2, sparc_reg::o7, 0x38, // op3 111000
                callee, 0, sparc_reg::g0);
}

sparc_instr::sparc_instr(JumpAndLink, sparc_reg linkReg,
                         sparc_reg addrReg1, sparc_reg addrReg2) : instr_t() {
   set2RegStyle(0x2, linkReg, 0x38, // op3 111000
                addrReg1, 0, addrReg2);
}

sparc_instr::sparc_instr(JumpAndLink, sparc_reg linkReg,
                         sparc_reg addrReg, int offset_numbytes) : instr_t() {
   // No, we don't assume that offset_numbytes is divisible by 4, because it's
   // only the sum [rs1 + offset_numbytes] that must be a multiple of 4; it's quite
   // possible to get that even when neither of the components by themselves is a
   // multiple of 4.

   setSimm13Style(0x2, linkReg, 0x38, // op3 111000
                  addrReg,
                  offset_numbytes // yep, no division by 4 for jmpl
                  );
}

sparc_instr::sparc_instr(Jump, sparc_reg jumpee) : instr_t() {
   set2RegStyle(0x2, sparc_reg::g0, 0x38, // op3 111000
                jumpee, 0, sparc_reg::g0);
}

sparc_instr::sparc_instr(Jump, sparc_reg addrReg1, 
			 sparc_reg addrReg2) : instr_t() {
   set2RegStyle(0x2, sparc_reg::g0, 0x38, // op3 111000
                addrReg1, 0, addrReg2);
}

sparc_instr::sparc_instr(Jump, sparc_reg addrReg, 
			 int offset_numbytes) : instr_t() {
   setSimm13Style(0x2, sparc_reg::g0, 0x38, // op3 111000
                  addrReg,
                  offset_numbytes // yep, no division by 4 for jmpl
                  );
}

sparc_instr::sparc_instr(Ret) : instr_t() {
   setSimm13Style(0x2,
                  sparc_reg::g0, // rd (link reg)
                  0x38, // jmpl op3
                  sparc_reg::i7, 8);
}

sparc_instr::sparc_instr(Retl) : instr_t() {
   setSimm13Style(0x2,
                  sparc_reg::g0, // rd (link reg)
                  0x38, // jmpl op3
                  sparc_reg::o7, 8);
}

sparc_instr::sparc_instr(V9Return, sparc_reg rs1, sparc_reg rs2) : instr_t() {
   set2RegStyle(0x2, sparc_reg::g0, // rd bits should be set to 0 for v9return (unused)
                0x39, rs1,
                0x0, // 0 for asi bits (unused)
                rs2);
}

sparc_instr::sparc_instr(V9Return, sparc_reg rs1, 
			 simmediate<13> offset) : instr_t() {
   setSimm13Style(0x2, sparc_reg::g0, // 0 for rd bits (unused)
                  0x39, rs1, offset);
}


sparc_instr::sparc_instr(Trap, unsigned cond,
                         sparc_reg srcReg1, sparc_reg srcReg2) : instr_t() {
   uimmediate<4> condbits = cond;
   // same as the bicc instructions
   
   raw = 0x2; // high 2 bits 10
   uimmediate<1> reserved1(0); // reserved bit
   rollin(raw, reserved1);
   rollin(raw, condbits);
   rollin(raw, uimmediate<6>(0x3a));
   rollinIntReg(raw, srcReg1);
   rollin(raw, uimmediate<1>(0)); // i=0
   rollin(raw, uimmediate<8>(0)); // 8 reserved bits
   rollinIntReg(raw, srcReg2);
}

sparc_instr::sparc_instr(Trap, unsigned cond, bool xcc,
                         sparc_reg srcReg1, 
			 simmediate<7> constant) : instr_t() {
   uimmediate<4> condbits = cond;
   // same as the bicc instructions
   
   raw = 0x2; // high 2 bits 10
   rollin(raw, uimmediate<1>(0)); // reserved bit
   rollin(raw, condbits);
   rollin(raw, uimmediate<6>(0x3a));
   rollinIntReg(raw, srcReg1);
   rollin(raw, uimmediate<1>(1)); // i=1
   if (xcc)
      rollin(raw, uimmediate<2>(2));
   else
      rollin(raw, uimmediate<2>(0));
   rollin(raw, uimmediate<4>(0));
   rollin(raw, constant);
}

sparc_instr::sparc_instr(Trap, unsigned cond, bool xcc,
                         simmediate<7> constant) : instr_t() {
   uimmediate<4> condbits = cond;
   // same as the bicc instructions
   
   raw = 0x2; // high 2 bits 10
   rollin(raw, uimmediate<1>(0)); // reserved bit
   rollin(raw, condbits);
   rollin(raw, uimmediate<6>(0x3a));
   rollinIntReg(raw, sparc_reg::g0);
   rollin(raw, uimmediate<1>(1)); // i=1
   if (xcc)
      rollin(raw, uimmediate<2>(2));
   else
      rollin(raw, uimmediate<2>(0));
   rollin(raw, uimmediate<4>(0));
   rollin(raw, constant);
}

sparc_instr::sparc_instr(Trap, simmediate<7> constant) : instr_t() {
   uimmediate<4> condbits = iccAlways;

   raw = 0x2;
   rollin(raw, uimmediate<1>(0));
   rollin(raw, condbits);
   rollin(raw, uimmediate<6>(0x3a));
   rollinIntReg(raw, sparc_reg::g0);
   rollin(raw, uimmediate<1>(1)); // i=1
   rollin(raw, uimmediate<2>(2)); // xcc, not that it mappers
   rollin(raw, uimmediate<4>(0));
   rollin(raw, constant);
}


sparc_instr::sparc_instr(StoreBarrier) : instr_t() {
   // NOTE: This is for sparc v8...in v9, there are different instructions
   // to use.
   raw = 0x2; // high 2 bits 10
   uimmediate<5> zero5(0); // 5 zero bits
   rollin(raw, zero5);
   rollin(raw, uimmediate<6>(0x28)); // op3 is 101000
   rollin(raw, uimmediate<5>(0x0f)); // 01111
   rollin(raw, uimmediate<1>(0x0)); // 0
   uimmediate<13> temp(0); // unused
   rollin(raw, temp);
}

sparc_instr::sparc_instr(MemBarrier,
			 uimmediate<3> cmask, 
			 uimmediate<4> mmask) : instr_t() {
   // NOTE: This is for sparc v9
   raw = 0x2; // high 2 bits 10
   uimmediate<5> zero5(0); // 5 zero bits
   rollin(raw, zero5);
   rollin(raw, uimmediate<6>(0x28)); // op3 is 101000
   rollin(raw, uimmediate<5>(0x0f)); // 01111
   rollin(raw, uimmediate<1>(0x1)); // i=1
   rollin(raw, uimmediate<6>(0x0)); // unused
   rollin(raw, cmask); // cmask
   rollin(raw, mmask); // cmask
}

sparc_instr::sparc_instr(UnImp, uimmediate<22> constant) : instr_t() {
   // execution of such an instruction causes an illegal-instruction trap.
   raw = 0x0; // high 2 bits 0
   
   uimmediate<5> reserved(0);
   rollin(raw, reserved);
   
   uimmediate<3> zero(0);
   rollin(raw, zero);
   
   rollin(raw, constant);
}

sparc_instr::sparc_instr(Flush, sparc_reg srcReg1, 
			 sparc_reg srcReg2) : instr_t() {
   // Flush: ensures subsequence *instruction* fetches to M[srcReg1 + srcReg2]
   // by the CPU executing the flush will appear after any loads, stores,
   // and load-stores issued [by that same processor] prior to the flush.
   // In a multiprocessor, also ensures that stores and load-stores to that addr
   // issued before the flush by the [same] processor become visible to the
   // *instruction* fetches of all other processors some time after the execution
   // of the flush.
   //
   // Flush operates on a double-word indicated by the address.
   //
   // Flush is needed only between a store and subsequence *instruction* access
   // to the modified location.  Not needed for data loads.
   
   set2RegStyle(0x2, sparc_reg::g0, // unused 5 bits
                0x3b, // op3 of 111011
                srcReg1, 0, srcReg2);
}

sparc_instr::sparc_instr(Flush, sparc_reg srcReg1, 
			 simmediate<13> offset) : instr_t() {
   setSimm13Style(0x2, sparc_reg(sparc_reg::rawIntReg, 0), // unused 5 bits
                  0x3b, // op3 of 111011
                  srcReg1, offset);
}

sparc_instr::sparc_instr(FAdd, unsigned precision,
                         sparc_reg rs1, sparc_reg rs2, 
			 sparc_reg rd) : instr_t() {
   raw = 0x2;
   rollinFpReg(raw, rd, precision);
   rollin(raw, uimmediate<6>(0x34));
   rollinFpReg(raw, rs1, precision);

   assert(precision == 1 || precision == 2 || precision == 4);
   uimmediate<9> opf = (precision==1 ? 0x041 :
                        precision==2 ? 0x042 : 0x043);
   rollin(raw, opf);
   
   rollinFpReg(raw, rs2, precision);
}

sparc_instr::sparc_instr(FSub, unsigned precision,
                         sparc_reg rs1, sparc_reg rs2, 
			 sparc_reg rd) : instr_t() {
   raw = 0x2;
   rollinFpReg(raw, rd, precision);
   rollin(raw, uimmediate<6>(0x34));
   rollinFpReg(raw, rs1, precision);

   assert(precision == 1 || precision == 2 || precision == 4);
   uimmediate<9> opf = (precision==1 ? 0x045 :
                        precision==2 ? 0x046 : 0x047);
   rollin(raw, opf);
   
   rollinFpReg(raw, rs2, precision);
}

sparc_instr::sparc_instr(FCmp, bool exceptionIfUnordered,
                         unsigned precision, unsigned fcc_num, 
			 sparc_reg rs1, sparc_reg rs2) : instr_t() {
   raw = 0x2;
   rollin(raw, uimmediate<3>(0));
   rollin(raw, uimmediate<2>(fcc_num));
   rollin(raw, uimmediate<6>(0x35));
   rollinFpReg(raw, rs1, precision);
   
   unsigned opf = 0x050;
   opf += precision;
   if (precision == 4)
      opf--;
   if (exceptionIfUnordered)
      opf += 0x004;
   
   rollin(raw, uimmediate<9>(opf));
   rollinFpReg(raw, rs2, precision);
}

sparc_instr::sparc_instr(Float2Int, unsigned float_src_precision,
                         bool int_dest_extended,
                         sparc_reg rs2, // a float reg
                         sparc_reg rd // an fp reg (! yes, really !)
                         ) : instr_t() {
   // int_dest_extended==true: rd is a 64-bit float register (e.g. an fp register
   //    whose number is divisible by 2)
   // int_dest_extended==false: rd is a 32-bit float register
   assert(rs2.isFloatReg() && rd.isFloatReg());

   raw = 0x2;
   rollinFpReg(raw, rd, int_dest_extended ? 2 : 1);
   
   rollin(raw, uimmediate<6>(0x34));
   
   rollin(raw, uimmediate<5>(0));
   
   unsigned opf = 0x080 + float_src_precision;
   if (float_src_precision==4)
      opf--;
   if (!int_dest_extended)
      opf += 0x050;

   rollin(raw, uimmediate<9>(opf));
   
   rollinFpReg(raw, rs2, float_src_precision);
}

sparc_instr::sparc_instr(Float2Float, unsigned src_reg_precision,
                         unsigned dest_reg_precision, sparc_reg rs2, 
			 sparc_reg rd) : instr_t() {
   raw = 0x2;
   rollinFpReg(raw, rd, dest_reg_precision);
   rollin(raw, uimmediate<6>(0x34));
   rollin(raw, uimmediate<5>(0));

   unsigned opf = 0x0c0;
   switch (src_reg_precision) {
      case 1:
         opf |= 0x1;
         break;
      case 2:
         opf |= 0x2;
         break;
      case 4:
         opf |= 0x3;
         break;
      default:
         assert(false);
   }
   switch (dest_reg_precision) {
      case 1:
         opf |= 0x4;
         break;
      case 2:
         opf |= 0x8;
         break;
      case 4:
         opf |= 0xc;
         break;
      default:
         assert(false);
   }

   switch (opf) {
      case 0x0c9:
      case 0x0cd:
      case 0x0c6:
      case 0x0ce:
      case 0x0c7:
      case 0x0cb:
         break;
      default:
         assert(false);
   }

   rollin(raw, uimmediate<9>(opf));
   rollinFpReg(raw, rs2, src_reg_precision);
}

sparc_instr::sparc_instr(Int2Float, bool src_reg_extended, unsigned dest_reg_precision,
                         sparc_reg rs2, sparc_reg rd) : instr_t() {
   // src_reg_extended==true: rs2 is a 64-bit float register (double precision)
   // src_reg_extended==false: rs2 is a 32-bit float register (single precision)
   assert(rs2.isFloatReg()); // really!
   assert(rd.isFloatReg());
   
   raw = 0x2;
   rollinFpReg(raw, rd, dest_reg_precision);
   rollin(raw, uimmediate<6>(0x34));
   rollin(raw, uimmediate<5>(0));

   unsigned opf = src_reg_extended ? 0x080 : 0x0c0;
   switch (dest_reg_precision) {
      case 1:
         opf |= 0x004;
         break;
      case 2:
         opf |= 0x008;
         break;
      case 4:
         opf |= 0x00c;
         break;
   }
   
   switch (opf) {
      case 0x084:
      case 0x088:
      case 0x08c:
      case 0x0c4:
      case 0x0c8:
      case 0x0cc:
         break;
      default:
         assert(false);
   }

   rollin(raw, uimmediate<9>(opf));
   rollinFpReg(raw, rs2, src_reg_extended ? 2 : 1);
}

sparc_instr::sparc_instr(FMov, unsigned precision, sparc_reg rs2, 
			 sparc_reg rd) : instr_t() {
   assert(precision == 1 || precision == 2 || precision == 4);
   
   raw = 0x2;
   rollinFpReg(raw, rd, precision);
   rollin(raw, uimmediate<6>(0x34));
   rollin(raw, uimmediate<5>(0));

   unsigned opf = (precision == 1 ? 0x001 :
                   precision == 2 ? 0x002 : 0x003);
   rollin(raw, uimmediate<9>(opf));

   rollinFpReg(raw, rs2, precision);
}

sparc_instr::sparc_instr(FNeg, unsigned precision, 
			 sparc_reg rs2, sparc_reg rd) : instr_t() {
   assert(precision == 1 || precision == 2 || precision == 4);
   
   raw = 0x2;
   rollinFpReg(raw, rd, precision);
   rollin(raw, uimmediate<6>(0x34));
   rollin(raw, uimmediate<5>(0));

   unsigned opf = (precision == 1 ? 0x005 :
                   precision == 2 ? 0x006 : 0x007);
   rollin(raw, uimmediate<9>(opf));

   rollinFpReg(raw, rs2, precision);
}

sparc_instr::sparc_instr(FAbs, unsigned precision, 
			 sparc_reg rs2, sparc_reg rd) : instr_t() {
   assert(precision == 1 || precision == 2 || precision == 4);
   
   raw = 0x2;
   rollinFpReg(raw, rd, precision);
   rollin(raw, uimmediate<6>(0x34));
   rollin(raw, uimmediate<5>(0));

   unsigned opf = (precision == 1 ? 0x009 :
                   precision == 2 ? 0x00a : 0x00b);
   rollin(raw, uimmediate<9>(opf));

   rollinFpReg(raw, rs2, precision);
}

sparc_instr::sparc_instr(FMul, unsigned src_precision, sparc_reg rs1, sparc_reg rs2,
                         unsigned dest_precision, sparc_reg rd) : instr_t() {
   unsigned opf = 0x040;
   switch (src_precision) {
      case 1:
         if (dest_precision == 1)
            opf = 0x049;
         else if (dest_precision == 2)
            opf = 0x069;
         else
            assert(false);
         break;
      case 2:
         if (dest_precision == 2)
            opf = 0x04a;
         else if (dest_precision == 4)
            opf = 0x06e;
         else
            assert(false);
         break;
      case 4:
         if (dest_precision == 4)
            opf = 0x04b;
         else
            assert(false);
         break;
      default:
         assert(false);
   }

   raw = 0x2;
   rollinFpReg(raw, rd, dest_precision);
   rollin(raw, uimmediate<6>(0x34));
   rollinFpReg(raw, rs1, src_precision);

   rollin(raw, uimmediate<9>(opf));

   rollinFpReg(raw, rs2, src_precision);
}

sparc_instr::sparc_instr(FDiv, unsigned precision, sparc_reg rs1, sparc_reg rs2,
                         sparc_reg rd) : instr_t() {
   raw = 0x2;
   rollinFpReg(raw, rd, precision);
   rollin(raw, uimmediate<6>(0x34));
   rollinFpReg(raw, rs1, precision);

   const unsigned opf = (precision == 1 ? 0x04d :
                         precision == 2 ? 0x04e : 0x04f);
   rollin(raw, uimmediate<9>(opf));

   rollinFpReg(raw, rs2, precision);
}

sparc_instr::sparc_instr(FSqrt, unsigned precision, 
			 sparc_reg rs2, sparc_reg rd) : instr_t() {
   raw = 0x2;
   rollinFpReg(raw, rd, precision);
   rollin(raw, uimmediate<6>(0x34));
   rollin(raw, uimmediate<5>(0));

   const unsigned opf = (precision == 1 ? 0x029 :
                         precision == 2 ? 0x02a : 0x02b);
   rollin(raw, uimmediate<9>(opf));

   rollinFpReg(raw, rs2, precision);
}

sparc_instr::sparc_instr(ReadStateReg, unsigned stateRegNum, 
			 sparc_reg dest) : instr_t() {
   set2RegStyle(0x2, dest, 0x28, sparc_reg(sparc_reg::rawIntReg, stateRegNum),
                0, sparc_reg(sparc_reg::rawIntReg, 0));
}

sparc_instr::sparc_instr(WriteStateReg, sparc_reg rs1, 
			 unsigned stateRegNum) : instr_t() {
   set2RegStyle(0x2, sparc_reg(sparc_reg::rawIntReg, stateRegNum),
                0x30, rs1,
                0, sparc_reg::g0);
}

sparc_instr::sparc_instr(ReadPrivReg, unsigned privRegNum, 
			 sparc_reg dest) : instr_t() {
   set2RegStyle(0x2, dest, 0x2a, sparc_reg(sparc_reg::rawIntReg, privRegNum),
                0, sparc_reg(sparc_reg::rawIntReg, 0));
}

// We use this one to raise and drop PIL (Processor Interrupt Level)
sparc_instr::sparc_instr(WritePrivReg, sparc_reg rs1, 
			 unsigned privRegNum) : instr_t() {
   set2RegStyle(0x2, sparc_reg(sparc_reg::rawIntReg, privRegNum), 
		0x32, rs1,
                0, sparc_reg::g0);
}

int32_t sparc_instr::getBitsSignExtend(unsigned lobit, unsigned hibit) const {
   // First off, let's work only with the bits we want:
   const uint32_t temp = getRawBits(lobit, hibit);
   
   // And now let's sign-extend those bits
   const unsigned numbits = hibit - lobit + 1;
   return signExtend(temp, numbits); // bitutils.C
}

sparc_reg_set sparc_instr::bits2FloatReg(unsigned bits, unsigned reg_precision) const {
   assert(reg_precision == 1 || reg_precision == 2 || reg_precision==4);
      // single, double, or quad precision

   assert(bits < 32); // should hold no matter the precision being used
   
   if (reg_precision == 1) {
      // in single-precision float ops, you can only address f0 thru f31,
      // each of 32 bits.

      //return sparc_reg_set(sparc_reg(sparc_reg::f, bits));
      return sparc_reg_set(sparc_reg_set::singleFloatReg, 1,
                           sparc_reg(sparc_reg::f, bits));
   }
   else if (reg_precision == 2) {
      // encoding is a little weird, due to v8 compatibility issues.
      // the bits come in order: b4, b3, b2, b1, b5...which we need to
      // rearrange to b5, b4, b3, b2, b1, 0, thus giving us an even number
      // from 0 to 62, inclusive.
      unsigned rnum = (bits & 0x1) << 5;
      rnum |= (bits & 0x1e);
      assert(rnum % 2 == 0);
      assert(rnum <= 62);

      return sparc_reg_set(sparc_reg_set::singleFloatReg, 2,
                           sparc_reg(sparc_reg::f, rnum));
//        sparc_reg_set result(sparc_reg(sparc_reg::f, rnum));
//        result += sparc_reg(sparc_reg::f, rnum+1);
//
//        return result;
   }
   else {
      assert(reg_precision == 4);
      // Due to v8 compatibility issues, encoding is a little strange:
      // The bits come in order: b4, b3, b2, 0, b5...which we need to
      // rearrange into: b5, b4, b3, b2, 0, 0
      unsigned rnum = ((bits & 0x1) << 5);
      rnum |= (bits & 0x1c);
      assert(rnum <= 60);
      assert(rnum % 4 == 0);

      return sparc_reg_set(sparc_reg_set::singleFloatReg, 4,
                           sparc_reg(sparc_reg::f, rnum));
//        sparc_reg_set result(sparc_reg(sparc_reg::f, rnum));
//        result += sparc_reg(sparc_reg::f, rnum+1);
//        result += sparc_reg(sparc_reg::f, rnum+2);
//        result += sparc_reg(sparc_reg::f, rnum+3);
//        return result;
   }
}

sparc_reg_set sparc_instr::getRs1AsFloat(unsigned float_precision) const {
   // float_precision: 1 (single), 2 (double), 4 (quad)
   return bits2FloatReg(getRs1Bits(raw), float_precision);
}

sparc_reg_set sparc_instr::getRs2AsFloat(unsigned float_precision) const {
   // float_precision: 1 (single), 2 (double), 4 (quad)
   return bits2FloatReg(getRs2Bits(raw), float_precision);
}

sparc_reg_set sparc_instr::getRdAsFloat(unsigned float_precision) const {
   // float_precision: 1 (single), 2 (double), 4 (quad)
   return bits2FloatReg(getRdBits(raw), float_precision);
}


sparc_instr::IntCondCodes sparc_instr::getIntCondCode_25_28() const {
   // bits 25 thru 28
   unsigned result = getRawBits(25, 28);
   return (IntCondCodes)result;
}

sparc_instr::IntCondCodes sparc_instr::getIntCondCode_14_17() const {
   // bits 14 thru 17
   unsigned result = getRawBits(14, 17);
   return (IntCondCodes)result;
}

sparc_instr::FloatCondCodes sparc_instr::getFloatCondCode_25_28() const {
   return (FloatCondCodes)getRawBits(25, 28);
}

sparc_instr::FloatCondCodes sparc_instr::getFloatCondCode_14_17() const {
   return (FloatCondCodes)getRawBits(14, 17);
}

static const pdstring intCondCodeStrings[] = {"n", "e", "le", "l", "leu",
                                           "lu", "neg", "vs", "a", "ne",
                                           "g", "ge", "gu", "geu", "pos",
                                           "vc"};

pdstring sparc_instr::disass_int_cond_code(unsigned cond) {
   assert(cond <= 15);
   return intCondCodeStrings[cond];
}

static const pdstring regCondStrings[] = {"", "z", "lez", "lz", "",
                                       "nz", "gz", "gez"};

pdstring sparc_instr::disass_reg_cond(unsigned cond) {
   assert(cond <= 7);
   if (cond == 0 || cond == 4)
      throw unknowninsn();
   return regCondStrings[cond];
}

static const pdstring floatCondCodeStrings[] = {"n", "ne", "lg", "ul",
                                             "l", "ug", "g", "u",
                                             "a", "e", "ue", "ge",
                                             "uge", "le", "ule", "o"};

pdstring sparc_instr::disass_float_cond_code(unsigned cond) {
   assert(cond <= 15);
   return floatCondCodeStrings[cond];
}

static const pdstring floatPrecisionStrings[] = {"", "s", "d", "", "q"};

pdstring sparc_instr::disassFloatPrecision(unsigned precision) {
   assert(precision == 1 || precision == 2 || precision == 4);
   return floatPrecisionStrings[precision];
}

pdstring sparc_instr::disass_simm7() const { return disass_simm(getSimm7()); }
pdstring sparc_instr::disass_simm11() const { return disass_simm(getSimm11()); }
pdstring sparc_instr::disass_simm13() const { return disass_simm(getSimm13()); }
pdstring sparc_instr::disass_uimm11() const { return disass_uimm(getUimm11()); }
pdstring sparc_instr::disass_uimm13() const { return disass_uimm(getUimm13()); }

pdstring sparc_instr::disass_simm(int simm) const {
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

pdstring sparc_instr::disass_uimm(unsigned uimm) const {
//     if (disAsDecimal)
//        return pdstring(uimm);
   
   // hex disassembly:
   return tohex(uimm);
}

pdstring sparc_instr::disass_reg_or_imm13(bool disConstAsSigned) const {
   const bool i = geti();
   if (i)
      // use 13 bit signed immediate
      if (disConstAsSigned)
         return disass_simm13();
      else
         return disass_uimm13();
   else
      return getRs2AsInt().disassx();
}

pdstring sparc_instr::disass_reg_or_imm11(bool disConstAsSigned) const {
   const bool i = geti();
   if (i)
      // use 11 bit signed immediate
      if (disConstAsSigned)
         return disass_simm11();
      else
         return disass_uimm11();
   else
      return getRs2AsInt().disassx();
}

pdstring sparc_instr::disass_reg_or_shcnt() const {
   // see A.48 (shift instructions) of sparc v9 manual
   const bool x = getRawBit(12);
   const bool i = getRawBit(13);
   
   if (!i && !x)
      // shift count is least significant 5 bits of rs2
      return getRs2AsInt().disassx();
   else if (!i && x)
      // shift count is least significant 6 bits of rs2
      return getRs2AsInt().disassx();
   else if (i && !x) {
      // shift count is immediate specified in bits 0 thru 4
      const unsigned shiftcount = getRawBits0(4);
      return num2string(shiftcount);
      //return pdstring(shiftcount).string_of();
   }
   else {
      assert(i && x);
      // shift count is immediate specified in bits 0 thru 5
      const unsigned shiftcount = getRawBits0(5);
      return num2string(shiftcount);
      //return pdstring(shiftcount).string_of();
   }
}

pdstring sparc_instr::disass_software_trap_number() const {
   // see p84 of sparc arch manual v8.
   // implementation is much like disass_addr except no [ ]
   // also see p237 of sparc v9 arch manual, which shrinks the sw trap # down
   // to 7 bits.
   
   const bool i = geti();
   if (i)
      // i=1 --> use simm7
      return disass_addr_simm7();
   else
      return disass_addr_2reg();
}

pdstring sparc_instr::disass_addr(bool includeBraces) const {
   // if includeBraces is true, we disassemble as: [addr]
   // else we disassemble as: addr
   // Example of the former: load/store instructions: st %l0, [%o0 + 70]
   // Example of the latter: jmp %o5+60

   pdstring result;

   if (includeBraces)
      result += "[";
   
   const bool i = geti();
   if (i)
      // i=1 --> use simm13
      result += disass_addr_simm13();
   else
      result += disass_addr_2reg();

   if (includeBraces)
      result += "]";

   return result;
}

pdstring sparc_instr::disass_addr_simm13() const {
   // see p. 84 of sparc arch manual v8
   pdstring result;
   
   // skip rs1 if it's %g0
   sparc_reg_set rs1 = getRs1AsInt();
   if (rs1.expand1() != sparc_reg::g0)
      result += rs1.disassx() + " + ";
   
   result += disass_simm13();
   
   return result;
}

pdstring sparc_instr::disass_addr_simm7() const {
   pdstring result;
   
   // skip rs1 if it's %g0
   sparc_reg_set rs1 = getRs1AsInt();
   if (rs1.expand1() != sparc_reg::g0)
      result += rs1.disassx() + " + ";
   
   result += disass_simm7();
   
   return result;
}

pdstring sparc_instr::disass_addr_2reg() const {
   // see p. 84 of sparc arch manual v8

   sparc_reg_set rs1 = getRs1AsInt();
   sparc_reg_set rs2 = getRs2AsInt();

   if (rs1 == sparc_reg::g0)
      return rs2.disassx();
   else if (rs2 == sparc_reg::g0)
      return rs1.disassx();
   else
      return rs1.disassx() + " + " + rs2.disassx();
}

pdstring sparc_instr::disass_asi(unsigned asi) {
   // Some asi numbers are defined in v9.  Many, many others are UltraSparc
   // extensions (UltraSparc User's Manual, sec 8.3.2).

   static const pdstring asi_strings[] = {
      // Restricted asi's:
      "0x00", "0x01", "0x02", "0x03",

      "ASI_NUCLEUS", "0x05", "0x06", "0x07",
      "0x08", "0x09", "0x0a", "0x0b", "ASI_NUCLEUS_LITTLE", "0x0d", "0x0e", "0x0f",

      "ASI_AS_IF_USER_PRIMARY", "ASI_AS_IF_USER_SECONDARY", "0x12", "0x13",

      // 0x14 and 0x15: bypass asi's; not translated by MMU; they pass thru their
      // virtual addresses as physical addresses.
      "ASI_PHYS_USE_EC", "ASI_PHYS_BYPASS_EC_WITH_EBIT",

      "0x16", "0x17",
      "ASI_AS_IF_USER_PRIMARY_LITTLE", "ASI_AS_IF_USER_SECONDARY_LITTLE",
      "0x1a", "0x1b",

      // 0x1c and 0x1d: bypass asi's; not translated by MMU; they pass thru their
      // virtual addresses as physical addresses.
      "ASI_PHYS_USE_EC_LITTLE", "ASI_PHYS_BYPASS_EC_WITH_EBIT_LITTLE",

      "0x1e", "0x1f",

      "0x20", "0x21", "0x22", "0x23", "ASI_NUCLEUS_QUAD_LDD", "0x25", "0x26", "0x27",
      "0x28", "0x29", "0x2a", "0x2b", "ASI_NUCLEUS_QUAD_LDD_LITTLE", "0x2d",
      "0x2e", "0x2f",

      "0x30", "0x31", "0x32", "0x33", "0x34", "0x35", "0x36", "0x37",
      "0x38", "0x39", "0x3a", "0x3b", "0x3c", "0x3d", "0x3e", "0x3f",

      "0x40", "0x41", "0x42", "0x43", "0x44",

      // 0x45 thru 0x6f: UltraSparc internal asi's (non-translating asis): not
      // translated by MMU; instead, they pass their virtual addresses thru
      // physical addresses.
      "ASI_LSU_CONTROL_REG",
      "ASI_DCACHE_DATA", "ASI_DCACHE_TAG",
      "ASI_INTR_DISPATCH_STATUS", "ASI_INTR_RECEIVE",
      "ASI_UPA_CONFIG_REG", "ASI_ESTATE_ERROR_EN_REG", "ASI_AFSR",
      "ASI_AFAR", "ASI_ECACHE_TAG_DATA", "0x4f",

      "ASI_IMMU", "ASI_IMMU_TSB_8KB_PTR_REG", "ASI_IMMU_TSB_64KB_PTR_REG",
      "0x53", "ASI_ITLB_DATA_IN_REG", "ASI_ITLB_DATA_ACCESS_REG",
      "ASI_ITLB_TAG_READ_REG", "ASI_IMMU_DEMAP",
      "ASI_DMMU", "ASI_DMMU_TSB_8KB_PTR_REG", "ASI_DMMU_TSB_64KB_PTR_REG",
      "ASI_DMMU_TSB_DIRECT_PTR_REG", "ASI_DTLB_DATA_IN_REG",
      "ASI_DTLB_DATA_ACCESS_REG", "ASI_DTLB_TAG_READ_REG", "ASI_DMMU_DEMAP",

      "0x60", "0x61", "0x62", "0x63", "0x64", "0x65",
      "ASI_ICACHE_INSTR", "ASI_ICACHE_TAG",
      "0x68", "0x69", "0x6a", "0x6b", "0x6c", "0x6d",
      "ASI_ICACHE_PRE_DECODE", "ASI_ICACHE_NEXT_FIELD",


      "ASI_BLOCK_AS_IF_USER_PRIMARY", "ASI_BLOCK_AS_IF_USER_SECONDARY",
      "0x72", "0x73", "0x74", "0x75",

      // 0x76 and 0x77: UltraSparc non-translating asi's
      "ASI_ECACHE_W", "ASI_UDBH_ERROR_REG_WRITE", // warning: 0x77 has several names!

      "ASI_BLOCK_AS_IF_USER_PRIMARY_LITTLE", "ASI_BLOCK_AS_IF_USER_SECONDARY_LITTLE",
      "0x7a", "0x7b", "0x7c", "0x7d",

      // 0x7e and 0x7f: UltraSparc non-translating asi's
      "ASI_ECACHE_R", "ASI_UDBH_ERROR_REG_READ",
      // warning: 0x7F has several names!



      // Non-restricted asi's:
      "ASI_PRIMARY", "ASI_SECONDARY",
      "ASI_PRIMARY_NO_FAULT",
      "ASI_SECONDARY_NO_FAULT",
      "0x84", "0x85", "0x86", "0x87",
      "ASI_PRIMARY_LITTLE", "ASI_SECONDARY_LITTLE", "ASI_PRIMARY_NO_FAULT_LITTLE",
      "ASI_SECONDARY_NO_FAULT_LITTLE", "0x8c", "0x8d", "0x8e", "0x8f",

      "0x90", "0x91", "0x92", "0x93", "0x94", "0x95", "0x96", "0x97",
      "0x98", "0x99", "0x9a", "0x9b", "0x9c", "0x9d", "0x9e", "0x9f",

      "0xa0", "0xa1", "0xa2", "0xa3", "0xa4", "0xa5", "0xa6", "0xa7",
      "0xa8", "0xa9", "0xaa", "0xab", "0xac", "0xad", "0xae", "0xaf",

      "0xb0", "0xb1", "0xb2", "0xb3", "0xb4", "0xb5", "0xb6", "0xb7",
      "0xb8", "0xb9", "0xba", "0xbb", "0xbc", "0xbd", "0xbe", "0xbf",

      "ASI_PST8_PRIMARY", "ASI_PST8_SECONDARY", "ASI_PST16_PRIMARY",
      "ASI_PST16_SECONDARY", "ASI_PST32_PRIMARY", "ASI_PST32_SECONDARY",
      "0xc6", "0xc7",
      "ASI_PST8_PRIMARY_LITTLE", "ASI_PST8_SECONDARY_LITTLE",
      "AST_PST16_PRIMARY_LITTLE", "ASI_PST16_SECONDARY_LITTLE",
      "ASI_PST32_PRIMARY_LITTLE", "ASI_PST32_SECONDARY_LITTLE",
      "0xce", "0xcf",

      "ASI_FL8_PRIMARY", "ASI_FL8_SECONDARY", "ASI_FL16_PRIMARY",
      "ASI_FL16_SECONDARY", "0xd4", "0xd5", "0xd6", "0xd7",
      "ASI_FL8_PRIMARY_LITTLE", "ASI_FL8_SECONDARY_LITTLE",
      "ASI_FL16_PRIMARY_LITTLE", "ASI_FL16_SECONDARY_LITTLE",
      "0xdc", "0xdd", "0xde", "0xdf",

      "ASI_BLK_COMMIT_PRIMARY", "ASI_BLK_COMMIT_SECONDARY",
      "0xe2", "0xe3", "0xe4", "0xe5", "0xe6", "0xe7",
      "0xe8", "0xe9", "0xea", "0xeb", "0xec", "0xed", "0xee", "0xef",

      "ASI_BLOCK_PRIMARY", "ASI_BLOCK_SECONDARY", "0xf2", "0xf3", "0xf4",
      "0xf5", "0xf6", "0xf7",
      "ASI_BLOCK_PRIMARY_LITTLE", "ASI_BLOCK_SECONDARY_LITTLE",
      "0xfa", "0xfb", "0xfc", "0xfd", "0xfe", "0xff",
   };

   assert(asi_strings[0xff] == pdstring("0xff"));
   
   return asi_strings[asi];
}

void sparc_instr::disass_11_intloads(disassemblyFlags *disassFlags,
                                     unsigned op3, const sparc_reg_set &rd,
                                     bool i,
                                     bool alternateSpace) const {
   const char* opstrings[] =
   {"ld ", "ldub ", "lduh ", "ldd ", 0, 0, 0, 0, 
    "ldsw ", "ldsb ", "ldsh ", "ldx ", 0, "ldstub ", 0, "swap ",
    "lda ", "lduba ", "lduha ", "ldda ", 0, 0, 0, 0, 
    "ldswa ", "ldsba ", "ldsha ", "ldxa ", 0, "ldstuba ", 0, "swapa "};

   assert(op3 <= 0x1f);

   disassFlags->result = opstrings[op3];

   disassFlags->result += disass_addr(true); // true --> display braces around addr
   if (alternateSpace) {
      disassFlags->result += " ";
      
      if (!i)
         // imm_asi
         disassFlags->result += disass_asi(getRawBits(5, 12));
      else
         disassFlags->result += sparc_reg::reg_asi.disass();
   }
   disassFlags->result += ", ";
   disassFlags->result += rd.disassx();

   // TO DO: alternate-space versions need a different disassembly!
}

void sparc_instr::disass_11_floatloads(disassemblyFlags *disassFlags,
                                       unsigned op3, const sparc_reg_set &rd) const {
   const char *opstrings[] = {"ldf ", 0, "ldqf ", "lddf "}; // 0x20, 0x22, 0x23
   disassFlags->result = opstrings[op3 - 0x20];

   disassFlags->result += disass_addr(true // display braces around addr
                                      ) + ", " + rd.expand1().disass();
      // rd is a float reg.  The call to expand1() is necessary when disassembling
      // float regs, lest a dbl-precision float reg (for example) appear as 2 regs
      // in the disassembly.
}

void sparc_instr::disass_11_loadfpstatereg(disassemblyFlags *disassFlags,
                                           bool extended) const {
   disassFlags->result = "ld";
   if (extended)
      disassFlags->result += "x";
   disassFlags->result += " ";
   disassFlags->result += disass_addr(//disassFlags->constsAsDecimal,
                                      true) + // braces
                                      ", %fsr";
}

void sparc_instr::
disass_11_load_fp_from_alternate_space(disassemblyFlags *disassFlags,
                                       unsigned op3, bool i,
                                       const sparc_reg_set &rd) const {
   const char *ops[] = {"lda ", 0, "ldqa ", "ldda "};
   disassFlags->result = ops[op3 - 0x30];
   disassFlags->result += disass_addr(//disassFlags->constsAsDecimal,
                                      true // braces around address
                                      );

   disassFlags->result += " ";
   if (!i)
      disassFlags->result += disass_asi(getRawBits(5, 12));
   else
      disassFlags->result += sparc_reg::reg_asi.disass();
         
   disassFlags->result += ", ";
   disassFlags->result += rd.disassx();
      // rd is a float reg.  The call to expand1() is necessary when disassembling
      // float regs, lest a dbl-precision float reg (for example) appear as 2 regs
      // in the disassembly.
}

void sparc_instr::disass_11_intstores(disassemblyFlags *disassFlags,
                                      unsigned op3, bool i,
                                      const sparc_reg_set &rd,
                                      bool alternateSpace) const {
   const char *opstrings[] =
         {"st ", "stb ", "sth ", "std ", 0, 0, 0, 0, 0, 0, "stx ",
          0, 0, 0, 0, 0, // 0x0f thru 0x13 -- filler
          "sta ", "stba ", "stha ", "stda ", 0, 0, 0, 0, 0, 0, "stxa "};
   disassFlags->result = opstrings[op3 - 0x04];

   bool clrPseudoInstr = false;
      
   if (!alternateSpace && op3 != 0x07 && rd == sparc_reg::g0) {
      clrPseudoInstr = true;
      disassFlags->result = "clr";
      if (op3 == 0x06)
         disassFlags->result += "h";
      else if (op3 == 0x05)
         disassFlags->result += "b";
      else if (op3 == 0x0e)
         disassFlags->result += "x";
      disassFlags->result += " ";
   }

   if (!clrPseudoInstr)
      disassFlags->result += rd.disassx() + ", ";

   disassFlags->result += disass_addr(true); // true --> display braces around addr

   if (alternateSpace) {
      disassFlags->result += " ";
      if (!i)
         disassFlags->result += disass_asi(getRawBits(5, 12));
      else
         disassFlags->result += sparc_reg::reg_asi.disass();
   }
}

void sparc_instr::disass_11_floatstores(disassemblyFlags *disassFlags,
                                        unsigned op3,
                                        const sparc_reg_set &rd, // a float reg
                                        bool i, bool alternate_space) const {
   if (alternate_space) {
      const char *opstrings[] = {"sta ", 0, "stqa ", "stda "};
      disassFlags->result = opstrings[op3 - 0x34];
   }
   else {
      const char *opstrings[] = {"st ", 0, "stq ", "std "};
      disassFlags->result = opstrings[op3 - 0x24];
   }

   disassFlags->result += rd.expand1().disass(); // a float reg
      // rd is a float reg.  The call to expand1() is necessary when disassembling
      // float regs, lest a dbl-precision float reg (for example) appear as 2 regs
      // in the disassembly.
   disassFlags->result += ", ";
   disassFlags->result += disass_addr(true); // display braces

   if (alternate_space) {
      disassFlags->result += " ";
      if (!i)
         disassFlags->result += disass_asi(getRawBits(5, 12));
      else
         disassFlags->result += sparc_reg::reg_asi.disass();
   }
}

void sparc_instr::disass_11_store_fp_statereg(disassemblyFlags *disassFlags,
                                              bool expanded) const {
   disassFlags->result = "st";
   if (expanded)
      disassFlags->result += "x";
   disassFlags->result += " %fsr, ";
   disassFlags->result += disass_addr(true);
}

void sparc_instr::disass_11_cas(disassemblyFlags *disassFlags,
                                bool i, bool extended,
                                const sparc_reg_set &rs1,
                                const sparc_reg_set &rs2,
                                const sparc_reg_set &rd) const {
   unsigned asi = getRawBits(5, 12); // 0x80 is ASI_PRIMARY
   bool casPseudoInstr = (asi == 0x80 && !extended);
   bool casxPseudoInstr = (asi == 0x80 && extended);

   if (casPseudoInstr)
      disassFlags->result = "cas ";
   else if (casxPseudoInstr)
      disassFlags->result = "casx ";
   else if (!extended)
      disassFlags->result = "casa ";
   else
      disassFlags->result = "casxa ";

   disassFlags->result += "[";
   disassFlags->result += rs1.disassx();
   disassFlags->result += "]";

   if (!casPseudoInstr && !casxPseudoInstr) {
      // not a pseudo instruction: print out "%asi" or the asi number
      disassFlags->result += " ";
      if (!i)
         disassFlags->result += disass_asi(asi);
      else
         disassFlags->result += sparc_reg::reg_asi.disass();
   }
         
   disassFlags->result += ", ";
   disassFlags->result += rs2.disassx();
   disassFlags->result += ", ";
   disassFlags->result += rd.disassx();
}

void sparc_instr::disass_11_prefetch(disassemblyFlags *disassFlags,
                                     unsigned fcn, bool i,
                                     bool alternateSpace) const {
   disassFlags->result = "prefetch";
   if (alternateSpace)
      disassFlags->result += "a";
   disassFlags->result += " ";
   disassFlags->result += disass_addr(true); // true --> display braces around addr

   if (alternateSpace) {
      disassFlags->result += " ";

      if (!i)
         disassFlags->result += disass_asi(getRawBits(5, 12));
      else
         disassFlags->result += sparc_reg::reg_asi.disass();
   }

   disassFlags->result += ", ";
   const char *strings[] = {"several-reads", "one-read", "several-writes",
                            "one-write", "page",
                            NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                            NULL, NULL, NULL, // 5-15 reserved in v9
                            "16", "17", "18", "19", "20", "21", "22", "23",
                            "24", "25", "26", "27", "28", "29", "30", "31"};
   // 16-31 are impl-dependent (legal) in v9

   disassFlags->result += strings[fcn];
}

void sparc_instr::getInformation11(registerUsage *ru,
                                   memUsage *mu,
                                   disassemblyFlags *disassFlags,
                                   sparc_cfi *,
                                   relocateInfo *
                                   ) const {
   // high 2 bits of raw instruction are 11
   // could be: integer load, fp load, coproc load, integer store, fp store,
   // coproc store, cas, prefetch-data, ldstub, or swap.
   
   const unsigned op3 = getOp3();
   const sparc_reg_set rd  = getRdAsInt();
   const sparc_reg_set rs1 = getRs1AsInt();
   const sparc_reg_set rs2 = getRs2AsInt();
   const bool i = geti();

   switch (op3) {
   case 0x00: case 0x01: case 0x02: case 0x03: case 0x08: case 0x09: case 0x0a: case 0x0b:
   case 0x10: case 0x11: case 0x12: case 0x13: case 0x18: case 0x19: case 0x1a: case 0x1b: // alternate-space versions of the above line
   case 0x0d: case 0x0f:
   case 0x1d: case 0x1f: {
      // first row: load unsigned word, load unsigned byte, load unsigned halfword,
      //    load double-word (deprecated in v9), load signed word, load signed byte, 
      //    load signed halfword, and load extended, respectively.
      // second row: alternate-space versions of the first row
      // third row: load-store-unsigned-byte and swap, respectively.
      // fourth row: alternate-space versions of the third row
      
      bool alternateSpace = ((op3 & 0x10) != 0);

      if (ru) {
         *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1;
         if (!i)
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2;
         else if (alternateSpace)
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += sparc_reg::reg_asi;
         *(sparc_reg_set *)(ru->definitelyWritten) += rd;
      }

      if (mu) {
         mu->memRead = true;
         if (!i)
            mu->addrRead = address(address::doubleReg, 
				   (reg_t*)new sparc_reg(rs1.expand1()), 
				   (reg_t*)new sparc_reg(rs2.expand1()), 
				   0, 0);
         else
            mu->addrRead = address(address::singleRegPlusOffset, 
				   (reg_t*)new sparc_reg(rs1.expand1()),
				   NULL, getSimm13(), 0);

	 // TO DO: the asi should probably be added to the address structure
      }

      if (disassFlags)
         disass_11_intloads(disassFlags, op3, rd, i, alternateSpace);

      if (ru && (op3 == 0x03 || op3 == 0x13)) {
         // load double-word (regular or alternate-space).  Deprecated in v9.
	 sparc_reg rdAsReg = rd.expand1();
	 assert(rdAsReg.getIntNum() % 2 == 0);
         *(sparc_reg_set *)(ru->definitelyWritten) += 
	   sparc_reg(sparc_reg::rawIntReg, rdAsReg.getIntNum()+1);
      }
      else if (ru && (op3 == 0x0f || op3 == 0x1f))
         // swap.  Deprecated in v9
         *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rd;

      break;
   }
   case 0x20: case 0x23: case 0x22: {
      // load fp, load double-fp, load quad-fp, load fp-state

      unsigned reg_precision = (op3 == 0x20) ? 1 : (op3 == 0x23) ? 2 : 4;
      sparc_reg_set destReg = getRdAsFloat(reg_precision);

      if (ru) {
         *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1; 
	 // *integer register*
         if (!i)
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2; 
	    // *integer register*
         *(sparc_reg_set *)(ru->definitelyWritten) += destReg; // *fp register*
      }

      if (mu) {
         mu->memRead = true;
         if (!i)
           mu->addrRead = address(address::doubleReg, 
				  (reg_t*)new sparc_reg(rs1.expand1()), 
				  (reg_t*)new sparc_reg(rs2.expand1()), 
				  0, 0);
         else
           mu->addrRead = address(address::singleRegPlusOffset, 
				  (reg_t*)new sparc_reg(rs1.expand1()), 
				  NULL, getSimm13(), 0);
      }

      if (disassFlags)
         disass_11_floatloads(disassFlags, op3, destReg);

      break;
   }
   case 0x21: {
      // load fp state reg
      assert(rd.expand1().getIntNum() <= 1);
      bool extended = (rd.expand1().getIntNum() == 1);

      if (ru) {
	 *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1;
	 if (!i)
	    *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2;

	 // ru->definitelyWritten += %fsr; (not yet implemented)
      }

      if (mu) {
	 mu->memRead = true;
	 if (!i)
	    mu->addrRead = address(address::doubleReg, 
				   (reg_t*)new sparc_reg(rs1.expand1()), 
				   (reg_t*)new sparc_reg(rs2.expand1()), 
				   0, 0);
	 else
	    mu->addrRead = address(address::singleRegPlusOffset, 
				   (reg_t*)new sparc_reg(rs1.expand1()),
				   NULL, getSimm13(), 0);
      }

      if (disassFlags)
         disass_11_loadfpstatereg(disassFlags, extended);
      
      break;
   }
   case 0x30: case 0x33: case 0x32: {
      // in v8: load coproc
      // in v9: load fp from alternate space (single, double, quad, respectively)
      // Note: this could share a lot of code with 0x20, 0x23, 0x22.

      unsigned reg_precision = (op3 == 0x30) ? 1 : (op3 == 0x33) ? 2 : 4;
      sparc_reg_set destReg = getRdAsFloat(reg_precision);

      if (ru) {
	 *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1; // an *int* reg
	 if (!i)
	    *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2; // an *int* reg
         else
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += sparc_reg::reg_asi;
	 *(sparc_reg_set *)(ru->definitelyWritten) += destReg; // an *fp* reg
      }

      if (mu) {
	 mu->memRead = true;
	 if (!i)
	    mu->addrRead = address(address::doubleReg, 
				   (reg_t*)new sparc_reg(rs1.expand1()), 
				   (reg_t*)new sparc_reg(rs2.expand1()), 
				   0, 0);
	 else
	    mu->addrRead = address(address::singleRegPlusOffset, 
				   (reg_t*)new sparc_reg(rs1.expand1()),
				   NULL, getSimm13(), 0);
      }

      if (disassFlags)
         disass_11_load_fp_from_alternate_space(disassFlags, op3, i, rd);

      break;
   }
   case 0x04: case 0x05: case 0x06: case 0x07: case 0x0e:
   case 0x14: case 0x15: case 0x16: case 0x17: case 0x1e: {
      // 1st row: store word, byte, halfword, doubleword, extended, respectively.
      // 2d row: alternate-space versions.

      bool alternateSpace = (op3 & 0x10);

      if (ru) {
         *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1;
         if (!i)
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2;
         else if (alternateSpace)
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += sparc_reg::reg_asi;
         *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rd;
      }

      if (mu) {
         mu->memWritten = true;
         if (!i)
            mu->addrWritten = address(address::doubleReg, 
				      (reg_t*)new sparc_reg(rs1.expand1()), 
				      (reg_t*)new sparc_reg(rs2.expand1()),
				      0, 0);
         else
            mu->addrWritten = address(address::singleRegPlusOffset, 
				      (reg_t*)new sparc_reg(rs1.expand1()), 
				      NULL, getSimm13(), 0);
      }
      
      if (disassFlags)
         disass_11_intstores(disassFlags, op3, i, rd, alternateSpace);
      
      if ((op3 == 0x07 || op3 == 0x17) && ru) {
         // store double-word (std).  Deprecated in v9.  Note that std gets its 64 bits
         // from the least significant 32 bits of 2 consecutive registers; stx, in
         // contrast, gets its 64 bits from one register.
	 sparc_reg rdAsReg = rd.expand1();
         assert(rdAsReg.getIntNum() % 2 == 0);
         *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) +=
            sparc_reg(sparc_reg::rawIntReg, rdAsReg.getIntNum()+1);
      }

      break;
   }
   case 0x24: case 0x27: case 0x26:
   case 0x34: case 0x37: case 0x36: {
      // store fp, double-fp, quad-fp (and alternate-space versions), respectively
      const bool alternate_space = (op3 >= 0x34);

      unsigned reg_precision = ((op3 & 0xf) == 0x4) ? 1 :
                         ((op3 & 0xf) == 0x7) ? 2: 4;
      sparc_reg_set rd = getRdAsFloat(reg_precision);

      if (ru) {
         *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1; // an *integer* register
         if (!i)
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2; // an *integer* register
         else if (alternate_space)
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += sparc_reg::reg_asi;
         *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rd; // a *float* register
      } // if ru

      if (mu) {
         mu->memWritten = true;
         if (!i)
            mu->addrWritten = address(address::doubleReg, 
				      (reg_t*)new sparc_reg(rs1.expand1()), 
				      (reg_t*)new sparc_reg(rs2.expand1()), 
				      0, 0);
         else
            mu->addrWritten = address(address::singleRegPlusOffset, 
				      (reg_t*)new sparc_reg(rs1.expand1()), 
				      NULL, getSimm13(), 0);
      }

      if (disassFlags)
         disass_11_floatstores(disassFlags, op3, rd, i, alternate_space);
      
      break;
   }
   case 0x25: {
      // store fp state reg to memory.
      // new for v9: if rd==1 then extended; if 0 then uses just low bits of fsr
      
      assert(rd.expand1().getIntNum() <= 1);
      bool expanded = (rd.expand1().getIntNum() == 1);
      
      if (ru) {
	 *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1;
	 if (!i)
	    *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2;

	 // want to add "fsr" to definitelyUsedBeforeWritten, but that reg isn't
	 // yet implemented.
      }

      if (mu) {
	 mu->memWritten = true;
	 if (!i)
	    mu->addrWritten = address(address::doubleReg, 
				      (reg_t*)new sparc_reg(rs1.expand1()), 
				      (reg_t*)new sparc_reg(rs2.expand1()), 
				      0, 0);
	 else
	    mu->addrWritten = address(address::singleRegPlusOffset, 
				      (reg_t*)new sparc_reg(rs1.expand1()), 
				      NULL, getSimm13(), 0);
      }

      if (disassFlags)
         disass_11_store_fp_statereg(disassFlags, expanded);
      
      break;
   }
   case 0x3c: case 0x3e: {
      // compare-and-swap (casa).  New with v9.
      // atomic {
      //    if (M[rs1] == rs2)
      //       swap rd, M[rs1]
      //    else
      //       rd <- M[rs1]  (i.e. just assign, don't do a full-blown swap)
      // }
      // Note: in both cases, rd <- old value of M[rs1].  What's conditional is
      // whether M[rs1] gets written with the old value of rd or left alone.
      //
      // There's a 32-bit variant (casa) and a 64-bit variant (casxa)
      // 
      // Note that both instructions are alternate-space instructions (ick).
      // If i=0 then the addr space is specified in bits 5 thru 12; else, it's
      // specified in the %asi register.  Happily, there are pseudo-instructions which
      // make the user unaware of such complexities (cas, casx).
      // 
      // Note that compare-and-swap doesn't imply any memory barrier, so give the same
      // consideration to memory barrier as if a load, store, or swap were used.
      
      const bool extended = (op3 == 0x3e); // true --> 64 bits; false --> 32 bits
      
      if (ru) {
         // reads rs1 and rs2.  writes to rd.  reads from rd _if_ the swap occurs.
         
         *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1;
         *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2;
         if (i)
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += sparc_reg::reg_asi;
         *(sparc_reg_set *)(ru->maybeUsedBeforeWritten) += rd;
         *(sparc_reg_set *)(ru->definitelyWritten) += rd;
      }
      
      if (disassFlags)
         disass_11_cas(disassFlags, i, extended, rs1, rs2, rd);
      
      break;
   }
   case 0x2d: case 0x3d: {
      // prefetch data.  From alternate-space if 0x3d. New in sparc v9.
      // prefetch takes an address (rs1 + simm13 if i=1, or rs1+rs2 if i=0), which is 
      // all we're interested in here. 

      const bool alternateSpace = (op3 == 0x3d);

      unsigned fcn = getRawBits(25, 29); 
      // 0: prefetch for several reads; 1: prefetch for one read 
      // 2: prefetch for several writes; 3: prefetch for one write 
      // 4: prefetch page; 5-15: reserved; 16-31: impl-dependent (not illegal) 
      
      if (ru) { 
         *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1;
         if (!i)
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2;
         else if (alternateSpace)
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += sparc_reg::reg_asi;
      } 
      
      if (disassFlags)
         disass_11_prefetch(disassFlags, fcn, i, alternateSpace);
      
      break;
   }
   default:
      //cerr << "unknown getInformation11 op3: " << (void*)op3 << endl;
      dothrow_unknowninsn();
      break;
   } // switch
}

void sparc_instr::disass_10_bitlogical(disassemblyFlags *disassFlags,
                                       const unsigned op3, bool i, bool cc,
                                       const sparc_reg_set &rs1,
                                       const sparc_reg_set &rs2,
                                       const sparc_reg_set &rd) const {
   const int simm13 = getSimm13();
      
   bool movPseudoInstr = false;
   bool clrPseudoInstr = false;
   bool btstPseudoInstr = false;
   bool bsetPseudoInstr = false;

   const unsigned op3_lo4bits = op3 & 0xf;
         
   const char *opstrings[] = {"and", "or", "xor", 0, "andn", "orn", "xnor"};
   disassFlags->result = opstrings[op3_lo4bits - 0x1];
      
   if (cc)
      disassFlags->result += "cc";
         
   // Treat "or %g0, %g0, rd" as "clr rd"
   if (op3_lo4bits == 0x2 && rs1 == sparc_reg::g0 && !i && rs2 == sparc_reg::g0) {
      clrPseudoInstr = true;
      disassFlags->result = "clr";
   }
         
   // Treat "or %g0, 0, rd" as "clr rd" also
   else if (op3_lo4bits == 0x2 && rs1 == sparc_reg::g0 && i && simm13==0) {
      clrPseudoInstr = true;
      disassFlags->result = "clr";
   }
         
   // Treat "or %g0, reg_or_imm, rd" as "mov reg_or_imm, rd"
   else if (op3_lo4bits == 0x2 && rs1 == sparc_reg::g0) {
      movPseudoInstr = true;
      disassFlags->result = "mov";
      if (cc)
         disassFlags->result += "cc";
   }
         
   // Treat "andcc rs1, reg_or_imm, %g0" as "btst reg_or_imm, rs1"
   else if (op3_lo4bits == 0x1 && cc && rd == sparc_reg::g0) {
      btstPseudoInstr = true;
      disassFlags->result = "btst";
   }

   // Treat "or rd, reg_or_imm, rd" as "bset reg_or_imm, rd"
   else if (op3_lo4bits == 0x2 && !cc && rs1 == rd) {
      bsetPseudoInstr = true;
      disassFlags->result = "bset";
   }

   disassFlags->result += " ";
      
   if (!movPseudoInstr && !clrPseudoInstr && !btstPseudoInstr && !bsetPseudoInstr)
      disassFlags->result += rs1.disassx() + ", ";
         
   if (!clrPseudoInstr) {
      // Like it or not, all logical instructions sign-extend the simm13
      // argument, so in that sense at least it makes sense to disassemble
      // the constant as a signed value.
      bool asSigned = true;
//        if (movPseudoInstr)
//           asSigned = true;
            
      disassFlags->result += disass_reg_or_imm13(asSigned);
      disassFlags->result += ", ";
   }
         
   if (btstPseudoInstr)
      disassFlags->result += rs1.disassx();
   else
      disassFlags->result += rd.disassx();
}

void sparc_instr::disass_10_shiftlogical(disassemblyFlags *disassFlags,
                                         unsigned op3,
                                         const sparc_reg_set &rs1,
                                         const sparc_reg_set &rd) const {
   const bool x = getRawBit(12);
   const char *opstrings[] = {"sll", "srl", "sra"}; // 0x25-0x27
   disassFlags->result = opstrings[op3 - 0x25];
   if (x)
      disassFlags->result += "x";
      
   disassFlags->result += " ";
   disassFlags->result += rs1.disassx();
   disassFlags->result += ", ";
   disassFlags->result += disass_reg_or_shcnt();
   disassFlags->result += ", ";
   disassFlags->result += rd.disassx();
}

void sparc_instr::disass_10_addsubtract(disassemblyFlags *disassFlags,
                                        const unsigned op3, bool i, bool cc,
                                        const sparc_reg_set &rs1,
                                        const sparc_reg_set &rd) const {
   const bool carry = ((op3 & 0x8) != 0);
   const unsigned op3_lo3bits = op3 & 0x7;
      
   const int simm13 = getSimm13();

   const bool cmpPseudoInstr = op3_lo3bits == 0x4 && cc && !carry;
   // subcc rs1, reg_or_imm, %g0 --> cmp rs1, reg_or_imm
   // note: in the future, we need a way to override this...curr flags won't do

   const bool incPseudoInstr = op3_lo3bits == 0x0 && i && !carry &&
      rs1 == rd && simm13 == 1;
   // add rs1, 1, rd  where rd==rs1  -->  'inc rd'

   if (incPseudoInstr)
      disassFlags->result = "inc";
   else if (cmpPseudoInstr)
      disassFlags->result = "cmp";
   else if (op3_lo3bits == 0x0)
      disassFlags->result = "add";
   else if (op3_lo3bits == 0x4)
      disassFlags->result = "sub";
   else
      assert(false);
         
   if (carry)
      disassFlags->result += "c";
         
   if (cc && !cmpPseudoInstr) // don't print "cc" if cmp; it's implicit!
      disassFlags->result += "cc";
         
   disassFlags->result += " ";

   if (!incPseudoInstr) {
      disassFlags->result += rs1.disassx();
      disassFlags->result += ", ";
      disassFlags->result += disass_reg_or_imm13(true // signed
                                                 );
   }

   if (!incPseudoInstr && !cmpPseudoInstr)
      disassFlags->result += ", ";

   if (!cmpPseudoInstr)
      disassFlags->result += rd.disassx();
}

void sparc_instr::disass_10_mulscc(disassemblyFlags *disassFlags,
                                   const sparc_reg_set &rs1,
                                   const sparc_reg_set &rd) const {
   disassFlags->result = pdstring("mulscc ") + rs1.disassx() + ", ";
   disassFlags->result += disass_reg_or_imm13(true // signed
                                              ) + ", ";
   disassFlags->result += rd.disassx();
}

void sparc_instr::disass_10_muldiv32(disassemblyFlags *disassFlags,
                                     unsigned op3,
                                     bool cc,
                                     const sparc_reg_set &rs1,
                                     const sparc_reg_set &rd) const {
   const bool is_signed = ((op3 & 0x1) != 0);
   const bool is_divide = ((op3 & 0x4) != 0); // bit 2 is 0 for mul, 1 for divide

   disassFlags->result = is_signed ? "s" : "u";
   disassFlags->result += is_divide ? "div" : "mul";
   if (cc)
      disassFlags->result += "cc";
         
   disassFlags->result += pdstring(" ") + rs1.disassx();
   disassFlags->result += pdstring(", ") + disass_reg_or_imm13(is_signed);
   disassFlags->result += pdstring(", ") + rd.disassx();
}

void sparc_instr::disass_10_muldiv64(disassemblyFlags *disassFlags,
                                     unsigned op3,
                                     const sparc_reg_set &rs1,
                                     const sparc_reg_set &rd) const {
   if (op3 == 0x09)
      disassFlags->result = "mulx ";
   else if (op3 == 0x2d)
      disassFlags->result = "sdivx ";
   else if (op3 == 0x0d)
      disassFlags->result = "udivx ";
         
   disassFlags->result += rs1.disassx() + ", ";
   disassFlags->result +=
      disass_reg_or_imm13(true // true --> signed (seems strange for
                          // udivx, but sparc v9 arch manual says
                          // that sign-extension is always performed)
                          );
         
   disassFlags->result += ", ";
   disassFlags->result += rd.disassx();
}

void sparc_instr::disass_10_saverestore(disassemblyFlags *disassFlags,
                                        bool isSave,
                                        const sparc_reg_set &rs1,
                                        const sparc_reg_set &rs2,
                                        const sparc_reg_set &rd) const {
   disassFlags->result = (isSave ? "save" : "restore");
         
   bool trivialRestore = !isSave && rs1 == sparc_reg::g0 &&
      rs2 == sparc_reg::g0 && rd == sparc_reg::g0;
         
   if (!trivialRestore) {
      disassFlags->result += " ";
      disassFlags->result += rs1.disassx() + ", ";
      disassFlags->result += disass_reg_or_imm13(true // true --> signed
                              ) + ", ";
      disassFlags->result += rd.disassx();
   }
}

void sparc_instr::disass_10_doneretry(disassemblyFlags *disassFlags,
                                      unsigned fcn) const {
   if (fcn == 0)
      disassFlags->result = "done";
   else
      disassFlags->result = "retry";
}
   
void sparc_instr::disass_10_v9return(disassemblyFlags *disassFlags) const {
   disassFlags->result = pdstring("return ") + disass_addr(false);
      // false --> no braces around addr
}

void sparc_instr::disass_10_ticc(disassemblyFlags *disassFlags,
                                 bool i,
                                 bool cc0, bool cc1,
                                 const sparc_reg_set &rs2) const {
   const IntCondCodes code = getIntCondCode_25_28();
   disassFlags->result = "t";
   disassFlags->result += disass_int_cond_code(code) + " ";
         
   // now we need to append the software trap number.  See p84 of sparc v8
   // architecture manual.  It's pretty much the same as [address] except no
   // surrounding brackets.  But also see p237 (A.60) of sparc v9 manual, which
   // puts in a few wrinkles.

   if (!cc0 && !cc1)
      disassFlags->result += "%icc, ";
   else if (cc1 && !cc0)
      disassFlags->result += "%xcc, ";
   else
      disassFlags->result += "%???, ";

   if (!i)
      disassFlags->result += rs2.disassx();
   else
      disassFlags->result += disass_software_trap_number();
}

void sparc_instr::disass_10_wrasr(disassemblyFlags *disassFlags,
                                  const sparc_reg_set &rs1,
                                  unsigned rdBits) const {
   disassFlags->result = "wr ";
   disassFlags->result += rs1.disassx() + ", ";
   disassFlags->result += disass_reg_or_imm13(false // unsigned
                                              );
   
   disassFlags->result += ", %";

   if (rdBits == 0) {
      // WRY -- write Y register.  Deprecated in v9.
      // ru->definitelyWritten += %y
      disassFlags->result += "y";
   }
   else if (rdBits == 1)
      dothrow_unknowninsn(); // rd=1 undefined in 9
   else if (rdBits == 2)
      // write condition codes register
      disassFlags->result += "ccr";
   else if (rdBits == 3)
      // write %asi
      disassFlags->result += "asi";
   else if (rdBits == 4 || rdBits == 5)
      dothrow_unknowninsn(); // rd==4 reserved in v9
   else if (rdBits == 6)
      // wrfprs
      disassFlags->result += "fprs";
   else if (rdBits < 14)
      dothrow_unknowninsn(); // 7-14 reserved in v9
   else if (rdBits == 15) {
      dothrow_unknowninsn();
      //assert(false && "wrasr 15 nyi (could be sir)");
   }
   else if (rdBits == 19)
      // 19: gsr (impdep grafx status register of ultrasparc)
      disassFlags->result += "gsr";
   else
      // WRASR 16-31 are impl dependent (allowed in v9)
      // privileged instr if dest reg is privileged
      //disassFlags->result += pdstring("asr") + pdstring(rdBits).string_of();
      disassFlags->result += pdstring("asr") + num2string(rdBits);
}

void sparc_instr::disass_10_saved_restored(disassemblyFlags *disassFlags,
                                           bool saved) const {
   if (saved)
      disassFlags->result = "saved";
   else
      disassFlags->result = "restored";
}

void sparc_instr::disass_10_wrpr(disassemblyFlags *disassFlags,
                                 const sparc_reg_set &rs1) const {
   disassFlags->result = "wrpr ";
   disassFlags->result += rs1.disassx();
   disassFlags->result += ", ";
   disassFlags->result += disass_reg_or_imm13(true);
   disassFlags->result += ", %";
	 
   const char *regstrings[] = {"tpc", "tnpc", "tstate", "tt", "tick", "tba",
                               "pstate", "tl", "pil", "cwp", "cansave",
                               "canrestore", "cleanwin", "otherwin", "wstate",
                               0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
                               0, 0, 0, 0, 0, 0,
                               "ver"};

   const unsigned whichreg = getRawBits(25, 29);
   disassFlags->result += regstrings[whichreg];
}

void sparc_instr::disass_10_flush(disassemblyFlags *disassFlags) const {
   disassFlags->result = pdstring("flush ") + disass_addr(true);
      // true --> braces around addr
}

void sparc_instr::disass_10_movcc(disassemblyFlags *disassFlags,
                                  sparc_reg condition_reg, // fcc0-3 or icc or xcc
                                  const sparc_reg_set &rd) const {
   assert(condition_reg.isFloatCondCode() || condition_reg.isIntCondCode());
   const bool is_integer = (condition_reg.isIntCondCode());
   disassFlags->result = "mov";
   if (is_integer)
      disassFlags->result += disass_int_cond_code(getIntCondCode_14_17());
   else
      disassFlags->result += disass_float_cond_code(getFloatCondCode_14_17());

   disassFlags->result += " ";

   disassFlags->result += condition_reg.disass();

   disassFlags->result += ", ";
   disassFlags->result += disass_reg_or_imm11(true // signed
                                              );
   disassFlags->result += ", ";
   disassFlags->result += rd.disassx();
}

void sparc_instr::disass_10_movr(disassemblyFlags *disassFlags,
                                 unsigned cond,
                                 const sparc_reg_set &rs1,
                                 const sparc_reg_set &rs2,
                                 const sparc_reg_set &rd) const {
   disassFlags->result = "movr";
   disassFlags->result += disass_reg_cond(cond);
//     const char *condstrings[] = {"?", "z", "lez", "lz", "?",
//                                  "nz", "gz", "gez"};
//     disassFlags->result += condstrings[cond];
   disassFlags->result += " ";
   disassFlags->result += rs1.disassx();
   disassFlags->result += ", ";
   if (geti()) {
      int simm10 = getSimm10();
      disassFlags->result += disass_simm(simm10);
   }
   else
      disassFlags->result += rs2.disassx();
   disassFlags->result += ", ";
   disassFlags->result += rd.disassx();
}

void sparc_instr::disass_10_popc(disassemblyFlags *disassFlags,
                                 const sparc_reg_set &rd) const {
   disassFlags->result = pdstring("popc ") +
      disass_reg_or_imm13(false // false --> unsigned 
                          ) + ", "; 
   disassFlags->result += rd.disassx();
}

void sparc_instr::disass_10_rdpr(disassemblyFlags *disassFlags,
                                 unsigned whichreg,
                                 const sparc_reg_set &rd) const {
   disassFlags->result = "rdpr %";
	 
   const char *regstrings[] = {"tpc", "tnpc", "tstate", "tt", "tick", "tba",
                               "pstate", "tl", "pil", "cwp", "cansave",
                               "canrestore", "cleanwin", "otherwin", "wstate",
                               "fq",
                               0, 0, 0, 0, 0, 0, 0, 0, 0, 
                               0, 0, 0, 0, 0, 0,
                               "ver"};

   disassFlags->result += regstrings[whichreg];
   disassFlags->result += ", ";
   disassFlags->result += rd.disassx();
}

void sparc_instr::disass_10_jmpl(disassemblyFlags *disassFlags,
                                 const sparc_reg_set &rs1,
                                 const sparc_reg_set &rd) const {
   const int simm13 = getSimm13();

   if (rs1 == sparc_reg::i7 && rd == sparc_reg::g0 && simm13 == 8) {
      // jmpl %i7+8, %g0 should be disass'd as "ret"
      disassFlags->result = "ret";
   }
   else if (rs1 == sparc_reg::o7 && rd == sparc_reg::g0 && simm13 == 8) {
      // jmpl %o7+8, %g0 should be disass'd as "retl"
      disassFlags->result = "retl";
   }
   else if (rd == sparc_reg::g0) {
      // jmpl addr, %g0 should be disassembled as "jmp addr"
      // (no [] braces around addr)
      disassFlags->result = pdstring("jmp ") +
         disass_addr(false); // false --> no braces around addr
   }
   else if (rd == sparc_reg::o7) {
      // jmpl [addr], %o7 should be disassembled as "call addr"
      disassFlags->result = pdstring("call ") +
         disass_addr(false); // false --> don't display braces around addr
   }
   else {
      disassFlags->result = "jmpl ";
      disassFlags->result += disass_addr(false) +
                                      // false --> don't display braces around addr
                             ", " + rd.disassx();
   }
}

void sparc_instr::getInformation10_unimpl_0x36(registerUsage *ru,
                                               disassemblyFlags *disassFlags,
                                               const sparc_reg_set rdAsIntReg,
                                               const sparc_reg_set rs1AsIntReg,
                                               const sparc_reg_set rs2AsIntReg) const {
   const sparc_reg_set rs1AsDblFloat = getRs1AsFloat(2);
   const sparc_reg_set rs2AsDblFloat = getRs2AsFloat(2);
   const sparc_reg_set rdAsDblFloat = getRdAsFloat(2);

   const sparc_reg_set rs1AsSnglFloat = getRs1AsFloat(1);
   const sparc_reg_set rs2AsSnglFloat = getRs2AsFloat(1);
   const sparc_reg_set rdAsSnglFloat = getRdAsFloat(1);
         
   const unsigned opf = getRawBits(5, 13);
   switch (opf) {
      case 0x080: {
         // shutdown.  Wait for all xactions to complete, leaving system &
         // external $ interface in clean state...
         // We treat as a nop
         if (ru)
            ;
         if (disassFlags) {
            disassFlags->result = "shutdown";
         }
         break;
      }
      // 0x050 through 0x057: Partitioned Add/Subtract Instructions
      // See UltraSPARC User's Manual, sec 13.5.2, p 199-200.
      case 0x050: // impdep fpadd16: four 16-bit adds
      case 0x052: // impdep fpadd32: two 32-bit adds
      case 0x054: // impdep fpsub16: four 16-bit subtracts
      case 0x056: // impdep fpsub32: two 32-bit subtracts
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1AsDblFloat;
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2AsDblFloat;
            *(sparc_reg_set *)(ru->definitelyWritten) += rdAsDblFloat;
         }
         if (disassFlags) {
            const char *opstrings[] = {"fpadd16 ", NULL, "fpadd32 ", NULL,
                                       "fpsub16 ", NULL, "fpsub32 "};
            disassFlags->result = pdstring(opstrings[opf - 0x050]) +
               rs1AsDblFloat.expand1().disass() + ", " +
               rs2AsDblFloat.expand1().disass() + ", " +
               rdAsDblFloat.expand1().disass();
               // The call to expand1() is necessary when disassembling float regs,
               // lest a dbl-precision float reg (for example) appear as 2 regs
               // in the disassembly.
         }
         break;
      case 0x051: // impdep fpadd16s: Two 16-bit adds
      case 0x053: // impdep fpadd32s: One 32-bit adds
      case 0x055: // impdep fpsub16s: Two 16-bit subtracts
      case 0x057: // impdep fpsub32s: One 32-bit subtract
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1AsSnglFloat;
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2AsSnglFloat;
            *(sparc_reg_set *)(ru->definitelyWritten) += rdAsSnglFloat;
         }
         if (disassFlags) {
            const char *opstrings[] = {"fpadd16s ", NULL, "fpadd32s ", NULL,
                                       "fpsub16s "};
                  
            disassFlags->result = pdstring(opstrings[opf - 0x051]) +
               rs1AsSnglFloat.expand1().disass() + ", " +
               rs2AsSnglFloat.expand1().disass() + ", " +
               rdAsSnglFloat.expand1().disass();
               // The call to expand1() is necessary when disassembling float regs,
               // lest a dbl-precision float reg (for example) appear as 2 regs
               // in the disassembly.
         }
         break;

      case 0x03b: {
         // impdep FPACK16: Packs four 16-bit values in rs2 into four 8-bit
         // values into rd.
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2AsDblFloat;
            *(sparc_reg_set *)(ru->definitelyWritten) += rdAsSnglFloat;
         }
         if (disassFlags) {
            disassFlags->result = pdstring("fpack16 ") +
               rs2AsDblFloat.expand1().disass() + ", " +
               rdAsSnglFloat.expand1().disass();
               // The call to expand1() is necessary when disassembling float regs,
               // lest a dbl-precision float reg (for example) appear as 2 regs
               // in the disassembly.
         }
         break;
      }
      case 0x03a: {
         // impdep FPACK32: Takes two 32-bit values in rs2, scales, truncates
         // and clips them into two 8-bit integers.  These are then merged
         // at the least significant positions of each 32-bit word in rs1
         // left shifted into 8 bits.  The result is 64 bits and is stored in
         // rd.  Huh?
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1AsDblFloat;
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2AsDblFloat;
            *(sparc_reg_set *)(ru->definitelyWritten) += rdAsDblFloat;
         }
         if (disassFlags) {
            disassFlags->result = pdstring("fpack32 ") +
               rs1AsDblFloat.expand1().disass() + ", " +
               rs1AsDblFloat.expand1().disass() + ", " +
               rdAsDblFloat.expand1().disass();
               // The call to expand1() is necessary when disassembling float regs,
               // lest a dbl-precision float reg (for example) appear as 2 regs
               // in the disassembly.
         }
         break;
      }
      case 0x03d: {
         // impdep FPACKFIX: Takes two 32-bit values in rs2, scales, truncates
         // and clips them into two 16-bit signed integers, and puts
         // them into the 32-bit rd register.
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2AsDblFloat;
            *(sparc_reg_set *)(ru->definitelyWritten) += rdAsSnglFloat;
         }
         if (disassFlags) {
            disassFlags->result = pdstring("fpackfix ") +
               rs2AsDblFloat.expand1().disass() + ", " +
               rdAsSnglFloat.expand1().disass();
               // The call to expand1() is necessary when disassembling float regs,
               // lest a dbl-precision float reg (for example) appear as 2 regs
               // in the disassembly.
         }
         break;
      }
      case 0x04d: {
         // impdep FEXPAND: Takes four 8-bit unsigned integers in rs2,
         // converts each to a 16-bit fixed value, and stores the four
         // results into the 64-bit rd registers.
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2AsSnglFloat;
            *(sparc_reg_set *)(ru->definitelyWritten) += rdAsDblFloat;
         }
         if (disassFlags) {
            disassFlags->result = pdstring("fpackfix ") +
               rs2AsSnglFloat.expand1().disass() + ", " +
               rdAsDblFloat.expand1().disass();
         }
         break;
      }
      case 0x04b: {
         // impdep FPMERGE: Interleaves four 8-bit unsigned values in rs1 and
         // rs2, to produce a 64-bit value in rd.
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1AsSnglFloat;
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2AsSnglFloat;
            *(sparc_reg_set *)(ru->definitelyWritten) += rdAsDblFloat;
         }
         if (disassFlags) {
            disassFlags->result = pdstring("fpmerge ") +
               rs1AsSnglFloat.expand1().disass() + ", " +
               rs2AsSnglFloat.expand1().disass() + ", " +
               rdAsDblFloat.expand1().disass();
               // The call to expand1() is necessary when disassembling float regs,
               // lest a dbl-precision float reg (for example) appear as 2 regs
               // in the disassembly.
         }
         break;
      }
      case 0x031: {
         // impdep fmul8x16: multiplies upper 8 bits of each 16-bit signed
         // value in rs1 by the signed 16-bit fixed-point signed integer
         // it corresponds to in rs1. The 24-bit product is rounded and stored
         // in the upper 16 bits of the corresponding 16-bit field of rd.
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1AsSnglFloat;
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2AsDblFloat;
            *(sparc_reg_set *)(ru->definitelyWritten) += rdAsDblFloat;
         }
         if (disassFlags) {
            disassFlags->result = pdstring("fmul8x16 ") +
               rs1AsSnglFloat.expand1().disass() + ", " +
               rs2AsDblFloat.expand1().disass() + ", " +
               rdAsDblFloat.expand1().disass();
               // The call to expand1() is necessary when disassembling float regs,
               // lest a dbl-precision float reg (for example) appear as 2 regs
               // in the disassembly.
         }
         break;
      }
      case 0x033: {
         // impdep fmul8x16au: same as above except that one 16-bit value,
         // the most significant 16 bits of the 32-bit rs2 reg.
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1AsSnglFloat;
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2AsSnglFloat;
            *(sparc_reg_set *)(ru->definitelyWritten) += rdAsDblFloat;
         }
         if (disassFlags) {
            disassFlags->result = pdstring("fmul8x16au ") +
               rs1AsSnglFloat.expand1().disass() + ", " +
               rs2AsSnglFloat.expand1().disass() + ", " +
               rdAsDblFloat.expand1().disass();
               // The call to expand1() is necessary when disassembling float regs,
               // lest a dbl-precision float reg (for example) appear as 2 regs
               // in the disassembly.
         }
         break;
      }
      case 0x035: {
         // impdep fmul8x16al: same as fmul8x16au except that the least
         // significant 16 bits of the 32-bit rs2 reg are used.
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1AsSnglFloat;
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2AsSnglFloat;
            *(sparc_reg_set *)(ru->definitelyWritten) += rdAsDblFloat;
         }
         if (disassFlags) {
            disassFlags->result = pdstring("fmul8x16al ") +
               rs1AsSnglFloat.expand1().disass() + ", " +
               rs2AsSnglFloat.expand1().disass() + ", " +
               rdAsDblFloat.expand1().disass();
         }
         break;
      }
      case 0x036: {
         // impdep fmul8sux16: multiplies upper 8 bits of each 16-bit signed
         // value in rs1 by the corresponding signed 16-bit fixed-point
         // signed int in rs2.  Result rounded and stored in upper 16 bits of
         // result into corresponding 16-bit field of rd.
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1AsDblFloat;
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2AsDblFloat;
            *(sparc_reg_set *)(ru->definitelyWritten) += rdAsDblFloat;
         }
         if (disassFlags) {
            disassFlags->result = pdstring("fmul8sux16 ") +
               rs1AsDblFloat.expand1().disass() + ", " +
               rs2AsDblFloat.expand1().disass() + ", " +
               rdAsDblFloat.expand1().disass();
               // The call to expand1() is necessary when disassembling float regs,
               // lest a dbl-precision float reg (for example) appear as 2 regs
               // in the disassembly.
         }
         break;
      }
      case 0x037: {
         // impdep fmul8ulx16: multiplies unsigned low 8 bits of each 16-bit
         // val in rs1 by the corresponding signed int in rs2.  Each
         // 24-bit product is signed-extended to 32 bits.  Upper 16 bits are
         // then rounded and stored in corresponding 16 bits of rd.
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1AsDblFloat;
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2AsDblFloat;
            *(sparc_reg_set *)(ru->definitelyWritten) += rdAsDblFloat;
         }
         if (disassFlags) {
            disassFlags->result = pdstring("fmul8ulx16 ") +
               rs1AsDblFloat.expand1().disass() + ", " +
               rs2AsDblFloat.expand1().disass() + ", " +
               rdAsDblFloat.expand1().disass();
               // The call to expand1() is necessary when disassembling float regs,
               // lest a dbl-precision float reg (for example) appear as 2 regs
               // in the disassembly.
         }
         break;
      }
      case 0x038: {
         // impdep fmuld8sux16: mult upper 8 bits of each 16-bit signed value
         // in rs1 by corresponding signed 16-bit signed int in rs2.
         // Result left-shifted 8 bits to make a 32-bit result, which is stored
         // in the corresponding 32-bit of rd.
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1AsSnglFloat;
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2AsSnglFloat;
            *(sparc_reg_set *)(ru->definitelyWritten) += rdAsDblFloat;
         }
         if (disassFlags) {
            disassFlags->result = pdstring("fmuld8sux16 ") +
               rs1AsSnglFloat.expand1().disass() + ", " +
               rs2AsSnglFloat.expand1().disass() + ", " +
               rdAsDblFloat.expand1().disass();
               // The call to expand1() is necessary when disassembling float regs,
               // lest a dbl-precision float reg (for example) appear as 2 regs
               // in the disassembly.
         }
         break;
      }
      case 0x039: {
         // impdep fmuld8ulx16: multiplies unsigned low 8 bits of each
         // 16-bit value in rs1 by the corresponding fixed point signed
         // i nt in rs2.  Product sign-extended to 32 bits and stored in rd.
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1AsSnglFloat;
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2AsSnglFloat;
            *(sparc_reg_set *)(ru->definitelyWritten) += rdAsDblFloat;
         }
         if (disassFlags) {
            disassFlags->result = pdstring("fmuld8ulx16 ") +
               rs1AsSnglFloat.expand1().disass() + ", " +
               rs2AsSnglFloat.expand1().disass() + ", " +
               rdAsDblFloat.expand1().disass();
               // The call to expand1() is necessary when disassembling float regs,
               // lest a dbl-precision float reg (for example) appear as 2 regs
               // in the disassembly.
         }
         break;
      }

      case 0x018: // impdep alignaddress
      case 0x01a: // impdep alignaddrl (little-endian)
         // adds rs1 and rs2 (int registers), storing result into rd (int reg too)
         // with the low 3 bits forced to zero.  The 3 bits are put into
         // a field of %gsr.  For alignaddrl, it's actually the 2's comp of those
         // 3 bits that get put into %gsr.
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1AsIntReg;
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2AsIntReg;
            *(sparc_reg_set *)(ru->definitelyWritten) += rdAsIntReg;
         }
         if (disassFlags) {
            const char *opstrings[] = {"alignaddr ", NULL, "alignaddrl "};
            disassFlags->result = pdstring(opstrings[opf - 0x018]) +
               rs1AsIntReg.disassx() + ", " + rs2AsIntReg.disassx() + ", " +
               rdAsIntReg.disassx();
         }
         break;
      case 0x048: // impdep faligndata
         // concatenates rs1 and rs2 (each 64-bit fp regs) to form a 128-bit
         // value, which is stored in rd (also a 64-bit fp reg).
         // most significant byte of extracted value is specified by
         // %gsr.alignaddr_offset.
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1AsDblFloat;
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2AsDblFloat;
            *(sparc_reg_set *)(ru->definitelyWritten) += rdAsDblFloat;
         }
         if (disassFlags) {
            disassFlags->result = "faligndata ";
            disassFlags->result += rs1AsDblFloat.expand1().disass() + ", " +
               rs2AsDblFloat.expand1().disass() + ", " +
               rdAsDblFloat.expand1().disass();
               // The call to expand1() is necessary when disassembling float regs,
               // lest a dbl-precision float reg (for example) appear as 2 regs
               // in the disassembly.
         }
         break;

      case 0x060: // fzero (zero fill)
      case 0x061: // fzeros 
      case 0x062: // fnor
      case 0x063: // fnors
      case 0x064: // fandnot2
      case 0x065: // fandnot2s
      case 0x066: // fnot2
      case 0x067: // fnot2s
      case 0x068: // fandnot1
      case 0x069: // fandnot1s
      case 0x06a: // fnot1
      case 0x06b: // fnot1s
      case 0x06c: // fxor
      case 0x06d: // fxors
      case 0x06e: // fnand
      case 0x06f: // fnands
      case 0x070: // fand
      case 0x071: // fands
      case 0x072: // fxnor
      case 0x073: // fxnors
      case 0x074: // fsrc1 (copy src1)
      case 0x075: // fsrc1s
      case 0x076: // fornot2 (src1 or src2')
      case 0x077: // fornot2s
      case 0x078: // fsrc2
      case 0x079: // fsrc2s
      case 0x07a: // fornot1 (src1' or src2)
      case 0x07b: // fornot1s
      case 0x07c: // for
      case 0x07d: // fors
      case 0x07e: // fone (fill with 1's)
      case 0x07f: // fones (fill with 1's, single-precision)
      {
         // impdep logical operate instructions
         const bool single_precision = (opf & 0x1);
         if (ru) {
            if (single_precision) {
               *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1AsSnglFloat;
               *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2AsSnglFloat;
               *(sparc_reg_set *)(ru->definitelyWritten) += rdAsSnglFloat;
            }
            else {
               *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1AsDblFloat;
               *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2AsDblFloat;
               *(sparc_reg_set *)(ru->definitelyWritten) += rdAsDblFloat;
            }
         }
         if (disassFlags) {
            const char *opstrings[] = {"fzero ", "fzeros ", "fnor ", "fnors ",
                                       "fandnot2 ", "fandnot2s ",
                                       "fnot2 ", "fnot2s",
                                       "fandnot1 ", "fandnot1s ",
                                       "fnot1 ", "fnot1s ",
                                       "fxor ", "fxors ",
                                       "fnand ", "fnands ",
                                       "fand ", "fands ",
                                       "fxnor ", "fxnors ",
                                       "fsrc1 ", "fsrc1s ",
                                       "fornot2 ", "fornot2s ",
                                       "fsrc2 ", "fsrc2s ",
                                       "fornot1 ", "fornot1s ",
                                       "for ", "fors ",
                                       "fone ", "fones "};
            disassFlags->result += opstrings[opf - 0x060];
            // rs1 unless fzero(s), fone(s), fsrc2(s),
            // fnot2(s)
            switch (opf & 0xffe) { // ignore low bit for this comparison
               case 0x060:
               case 0x07e:
               case 0x078:
               case 0x066:
                  break;
               default:
                  if (single_precision)
                     disassFlags->result += rs1AsSnglFloat.expand1().disass() + ", ";
                  else
                     disassFlags->result += rs1AsDblFloat.expand1().disass() + ", ";
            }
            // rs2 unless fzero(s), fone(s), fsrc1(s), fnot1(s)
            switch (opf & 0x0ffe) { // ignore low bit for this comparison
               case 0x060:
               case 0x07e:
               case 0x074:
               case 0x06a:
                  break;
               default:
                  if (single_precision)
                     disassFlags->result += rs1AsSnglFloat.expand1().disass() + ", ";
                  else
                     disassFlags->result += rs1AsDblFloat.expand1().disass() + ", ";
            }
            if (single_precision)
               disassFlags->result += rdAsSnglFloat.expand1().disass();
            else
               disassFlags->result += rdAsDblFloat.expand1().disass();
         } // disassFlags
         break;
      } // logical operations

      // Pixel Compare Instructions: compare either four 16-bit values or two 32-bit
      // values.  rs1 and rs2 are dbl float regs; rd is an integer reg
      case 0x020: // fcmple16
      case 0x022: // fcmpne16
      case 0x024: // fcmple32
      case 0x026: // fcmpne32
      case 0x028: // fcmpgt16
      case 0x02a: // fcmpeq16
      case 0x02c: // fcmpgt32
      case 0x02e: // fcmpeq32
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1AsDblFloat;
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2AsDblFloat;
            *(sparc_reg_set *)(ru->definitelyWritten) += rdAsIntReg; // int reg
         }
         if (disassFlags) {
            const char *opstrings[] = {"fcmple16 ", NULL, "fcmpne16 ", NULL,
                                       "fcmple32 ", NULL, "fcmpne32 ", NULL,
                                       "fcmpgt16 ", NULL, "fcmpeq16 ", NULL,
                                       "fcmpgt32 ", NULL, "fcmpeq32"};
            disassFlags->result = pdstring(opstrings[opf - 0x020]) +
                                  rs1AsDblFloat.expand1().disass() +
                                  ", " + rs2AsDblFloat.expand1().disass() + ", " +
                                  rdAsIntReg.disassx();
         }
         break;
  
      // Edge handling instructions
      // 
      case 0x000: // edge8
      case 0x002: // edge8l
      case 0x004: // edge16
      case 0x006: // edge16l
      case 0x008: // edge32
      case 0x00a: // edge32l
      {
         dothrow_unimplinsn();
         //assert(false && "nyi");
         break;
      }

      case 0x03e: // pdist
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1AsDblFloat;
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2AsDblFloat;
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rdAsDblFloat;
            *(sparc_reg_set *)(ru->definitelyWritten) += rdAsDblFloat;
         }
         if (disassFlags) {
            disassFlags->result = pdstring("pdist ") +
               rs1AsDblFloat.expand1().disass() + ", " +
               rs2AsDblFloat.expand1().disass() + ", " +
               rdAsDblFloat.expand1().disass();
         }
         break;

      case 0x010: // array8
      case 0x012: // array16
      case 0x014: // array32
      {
         dothrow_unimplinsn();
         //assert(false && "nyi");
         break;
      }
      default:
         // unknown opf of a 0x36 impdep insn
         dothrow_unimplinsn();
   } // switch opf for 0x36 impdep insn
}

void sparc_instr::disass_10_rd_state_reg(disassemblyFlags *disassFlags,
                                         const sparc_reg_set &rd,
                                         unsigned rs1bits, bool i) const {
   disassFlags->result = "rd ";
   
   switch (rs1bits) {
      case 0: // rd %y, rd
         disassFlags->result += pdstring("%y, ") + rd.disassx();
         break;
      case 1: // reserved
         dothrow_unknowninsn();
         break;
      case 2: // rd %ccr, rd
         disassFlags->result += pdstring("%ccr, ") + rd.disassx();
         break;
      case 3: // rd %asi, rd
         disassFlags->result += sparc_reg::reg_asi.disass();
         disassFlags->result += pdstring(", ") + rd.disassx();
         break;
      case 4: // rd %tick, rd
         disassFlags->result += pdstring("%tick, ") + rd.disassx();
         break;
      case 5: // rd %pc, rd
         disassFlags->result += pdstring("%pc, ") + rd.disassx();
         break;
      case 6:
         disassFlags->result += pdstring("%fprs, ") + rd.disassx();
         break;
      case 7:
      case 8:
      case 9:
      case 10:
      case 11:
      case 12:
      case 13:
      case 14: // read ancillary state register (reserved)
         disassFlags->result += pdstring("%asr") +
                                num2string(rs1bits) +
                                ", " +
                                rd.disassx();
         break;
      case 15: {
         // note: when rs1 is 15, things get complex:
         //       -- if rd=0 and i=0, then it's a stbar
         //       -- if rd=0 and i=1, then it's a membar
         //       -- if rd!=0 then reserved
         if (getRawBits(25, 29) == 0 && !i) {
            // stbar (v8)...obsoleted by membar in v9.
            // no regs read or written
            disassFlags->result = "stbar"; // not +=; we discard result up to now
         }
         else if (getRawBits(25, 29) == 0 && i) {
            // membar (new in v9).  No regs read or written
            // Bits 0 thru 3 are mmask; bits 4 thru 6 are cmask.
            // note: membar applies to memory ops by issuing cpu; no effect on
            // other cpus.
            // mmask: classes of memory references subject to ordering
            // cmask: when non-zero, completion (not just ordering) constraints
            //        imposed.
            //
            // Some definitions: Load performed when the value loaded xmitted
            // from memory and can't be modified by another cpu.
            // A store has been performed when the value stored visible (when
            // the prev value can no longer be read by _any_ processor).
            //
            // For info on membar use, see sparc v9 manual section 8.4.3 and
            // appendix J and chapter 8 and appendix f.
            //
            // note: membar with mmask=0x8 and cmask=0x0 is identical to v8's
            // stbar.
            
            disassFlags->result = "membar"; // not +=; we discard result up to now
            
            // mmask:
            bool storeStoreBit = getRawBit(3);
            bool loadStoreBit = getRawBit(2);
            bool storeLoadBit = getRawBit(1);
            bool loadLoadBit = getRawBit(0);
            
            if (storeStoreBit)
               // effects of all stores prior to the membar must be visible to
               // all cpus before the effect of any stores after the membar.
               // Equiv to v8's stbar instr.
               disassFlags->result += " #StoreStore";
            if (loadStoreBit)
               // loads before membar must be performed before the effect of any
               // stores after the membar is visible to any other processor.
               disassFlags->result += " #LoadStore";
            if (storeLoadBit)
               // stores prior to membar must be visible to all cpus before
               // loads after the membar may be performed.
               disassFlags->result += " #StoreLoad";
            if (loadLoadBit)
               // loads prior to membar must be performed before loads after it.
               disassFlags->result += " #LoadLoad";
            
            // cmask:
            bool syncBarrierBit = getRawBit(6);
            bool memIssueBarrierBit = getRawBit(5);
            bool lookAsideBarrierBit = getRawBit(4);
            if (syncBarrierBit)
               // all ops (incl non-memory reference ops) prior to membar must
               // be performed and the effects of any exceptions visible before
               // any instr after the membar may be initiated.
               disassFlags->result += " #Sync";
            if (memIssueBarrierBit)
               // all mem ref ops prior to membar must have been performed before
               // any memory op after the membar may be initiated.
               disassFlags->result += " #MemIssue";
            if (lookAsideBarrierBit)
               // a store before the membar must complete before any load
               // following the membar which references the same addr may be
               // initiated.
               disassFlags->result += " #Lookaside";
         } // membar
         break;
      } // case 15
      case 16: // %pcr
         disassFlags->result += "%pcr, " + rd.disassx();
         break;
      case 17: // %pic
         disassFlags->result += "%pic, " + rd.disassx();
         break;
      case 19: // gsr (grafx status register of ultrasparc)
         disassFlags->result += pdstring("%gsr, ") + rd.disassx();
         break;
      default:
         // 16-32: read state register of impl-dependent reg.
         disassFlags->result += pdstring("%asr") +
                                num2string(rs1bits) +
                                ", " +
                                rd.disassx();
         break;
   } // switch on rs1 bits
}

void sparc_instr::getInformation_10_0x34_floats(registerUsage *ru,
                                                disassemblyFlags *disassFlags) const {
   const unsigned opf = getRawBits(5, 13);
   switch(opf) {
      case 0x041: case 0x042: case 0x043:
      case 0x045: case 0x046: case 0x047: {
         // floating point add & subtract
         unsigned reg_precision = (opf & 0x03) == 0x1 ? 1 :
            (opf & 0x03) == 0x2 ? 2 : 4;
   
         sparc_reg_set rs1 = getRs1AsFloat(reg_precision);
         sparc_reg_set rs2 = getRs2AsFloat(reg_precision);
         sparc_reg_set rd  = getRdAsFloat(reg_precision);
   
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1;
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2;
            *(sparc_reg_set *)(ru->definitelyWritten) += rd;
         }
   
         if (disassFlags) {
            const char *opstrings[] = {"fadds ", "faddd ", "faddq ", 0, "fsubs ",
                                       "fsubd ", "fsubq "}; // 0x41-0x47
            disassFlags->result = opstrings[opf - 0x41];
   
            disassFlags->result += rs1.expand1().disass() + ", " +
               rs2.expand1().disass() + ", " +
               rd.expand1().disass();
         } // if (disassFlags)
         break;
      }
      case 0x081: case 0x082: case 0x083:
      case 0x0d1: case 0x0d2: case 0x0d3: {
         // Convert fp to integer
         unsigned src_reg_precision = (opf & 0x3) == 0x1 ? 1 :
            (opf & 0x3) == 0x2 ? 2 : 4;
         bool dest_reg_extended = ((opf & 0x0f0) == 0x080);
         if (!dest_reg_extended)
            assert((opf & 0x0f0) == 0x0d0);
         sparc_reg_set rs2 = getRs2AsFloat(src_reg_precision);
         sparc_reg_set rd = getRdAsFloat(dest_reg_extended ? 2 : 1);
         
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2; // a float reg
            *(sparc_reg_set *)(ru->maybeWritten) += rd; // a float reg
               // sorry, can't be added to 'definitelyWritten', since an NV
               // exception can be thrown if rs2 doesn't fit into an int reg
         }
   
         if (disassFlags) {
            disassFlags->result = "f";
            disassFlags->result += disassFloatPrecision(src_reg_precision);
            disassFlags->result += "to";
            disassFlags->result += dest_reg_extended ? "x" : "i";
            disassFlags->result += pdstring(" ") + rs2.expand1().disass() + ", " +
               rd.expand1().disass();
         }
         break;
      }
      case 0x0c9: case 0x0cd: case 0x0c6: case 0x0ce: case 0x0c7: case 0x0cb: {
         // float2float
         unsigned srcReg_Precision = (opf & 0x3);
         if (srcReg_Precision == 3)
            srcReg_Precision++;
         assert(srcReg_Precision==1 || srcReg_Precision==2 || srcReg_Precision==4);
   
         const sparc_reg_set rs2 = getRs2AsFloat(srcReg_Precision);

         unsigned destReg_Precision = ((opf & 0xc) >> 2);
         if (destReg_Precision == 3)
            destReg_Precision++;
         assert(destReg_Precision==1 || destReg_Precision==2 || destReg_Precision==4);
   
         sparc_reg_set rd = getRdAsFloat(destReg_Precision);
   
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2;
            *(sparc_reg_set *)(ru->maybeWritten) += rd;
               // sorry, can't be definitelyWritten because an exception can
               // be raised depending on the contents of rs2
         }
   
         if (disassFlags) {
            disassFlags->result = "f";
            disassFlags->result += disassFloatPrecision(srcReg_Precision);
            disassFlags->result += "to";
            disassFlags->result += disassFloatPrecision(destReg_Precision);
            disassFlags->result += " ";
            disassFlags->result += rs2.expand1().disass() + ", " + rd.expand1().disass();
         }

         break;
      }
      case 0x084: case 0x088: case 0x08c: case 0x0c4: case 0x0c8: case 0x0cc: {
         // convert integer to floating point
         // rs2 is the integer source operand, but it's contained in a *fp* register.
         bool extended = ((opf & 0x040) == 0);
         unsigned destReg_Precision = (opf & 0xf) == 4 ? 1 :
            (opf & 0xf) == 8 ? 2 : 4;
         sparc_reg_set rd = getRdAsFloat(destReg_Precision);
         sparc_reg_set rs2 = getRs2AsFloat(extended ? 2 : 1);
        
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2; // a *float* register
            *(sparc_reg_set *)(ru->definitelyWritten) += rd; // a *float* register
         }
   
         if (disassFlags) {
            disassFlags->result = "f";
            if (extended)
               disassFlags->result += "x";
            else
               disassFlags->result += "i";
            disassFlags->result += "to";
            disassFlags->result += disassFloatPrecision(destReg_Precision);

            disassFlags->result += " ";
            disassFlags->result += rs2.expand1().disass() + ", " +
               rd.expand1().disass();
         }

         break;
      }
      case 0x001: case 0x002: case 0x003: case 0x005: case 0x006: case 0x007:
      case 0x009: case 0x00a: case 0x00b: {
         // floating-point move, with a possible twist: negate or take absolute value.
         unsigned regPrecision = (opf & 0x3) == 0x1 ? 1 :
            (opf & 0x3) == 0x2 ? 2 : 4;
         sparc_reg_set rs2 = getRs2AsFloat(regPrecision);
         sparc_reg_set rd  = getRdAsFloat(regPrecision);
         unsigned operation = opf >> 2; // 0 --> move; 1 --> neg; 2 --> abs
         assert(operation <= 2);
   
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2;
            *(sparc_reg_set *)(ru->definitelyWritten) += rd;
         }
   
         if (disassFlags) {
            disassFlags->result = "f";
            const char *opstrings[] = {"mov", "neg", "abs"};
            disassFlags->result += opstrings[operation];
            disassFlags->result += disassFloatPrecision(regPrecision);

            disassFlags->result += " ";
            disassFlags->result += rs2.expand1().disass() + ", " + rd.expand1().disass();
         }

         break;
      }
      case 0x049: case 0x04a: case 0x04b: case 0x069: case 0x06e:
      case 0x04d: case 0x04e: case 0x04f: {
         // floating point multiply or divide
         const unsigned srcregPrecision = (opf & 0x03) == 0x1 ? 1 :
            (opf & 0x03) == 0x2 ? 2 : 4;
         unsigned destregPrecision = srcregPrecision;
         if (opf == 0x069) destregPrecision = 2;
         else if (opf == 0x06e) destregPrecision = 4;

         sparc_reg_set rs1 = getRs1AsFloat(srcregPrecision);
         sparc_reg_set rs2 = getRs2AsFloat(srcregPrecision);
         sparc_reg_set rd  = getRdAsFloat(destregPrecision);
   
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1;
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2;
            *(sparc_reg_set *)(ru->definitelyWritten) += rd;
         }
   
         if (disassFlags) {
            const char *opstrings[] = {"fmuls ", "fmuld ", "fmulq ", 0,
                                       "fdivs ", "fdivd ", "fdivq "};
            if ((opf & 0x020) != 0) {
               if (opf == 0x069)
                  disassFlags->result = "fsmuld";
               else if (opf == 0x06e)
                  disassFlags->result = "fdmulq";
               else
                  dothrow_unknowninsn();
            }
            else
               disassFlags->result = opstrings[opf - 0x049];

            disassFlags->result += rs1.expand1().disass() + ", ";
            disassFlags->result += rs2.expand1().disass() + ", ";
            disassFlags->result += rd.expand1().disass();
         }

         break;
      }
      case 0x029: case 0x02a: case 0x02b: {
         // floating-point square root
         unsigned regPrecision = (opf == 0x029) ? 1 : (opf == 0x02a) ? 2 : 4;
         // there's no rs1 in a square root operation
         sparc_reg_set rs2 = getRs2AsFloat(regPrecision);
         sparc_reg_set rd  = getRdAsFloat(regPrecision);
   
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2;
            *(sparc_reg_set *)(ru->definitelyWritten) += rd;
         }
   
         if (disassFlags) {
            disassFlags->result += "fsqrt";
            disassFlags->result += disassFloatPrecision(regPrecision);
            disassFlags->result += " ";
            disassFlags->result += rs2.expand1().disass() + ", ";
            disassFlags->result += rd.expand1().disass();
         }

         break;
      }
      default:
         dothrow_unknowninsn(); // other op3=0x34 unknown or not yet implemented
   }
}

void sparc_instr::getInformation_10_0x35_floats(registerUsage *ru,
                                                disassemblyFlags *disassFlags) const {
   const unsigned opf = getRawBits(5, 13);
//   const unsigned opf_low = getRawBits(5, 9); // unused so we comment it out
   
   if (getRawBits(7, 10)==0) {
      // FMOVcc -- move fp reg to another fp reg...you choose the condition code
      // (any int cond code; any fp cond code)
      unsigned size = (opf & 0x3) == 0x1 ? 1 : (opf & 0x3) == 0x2 ? 2 : 4;
	 
      sparc_reg_set rs2 = getRs2AsFloat(size);
      sparc_reg_set rd  = getRdAsFloat(size);

      const unsigned opf_cc = getRawBits(11, 13);
      sparc_reg condition_code_reg(sparc_reg::g0); // for now...
      switch (opf_cc) {
         case 0:
         case 1:
         case 2:
         case 3:
            condition_code_reg = sparc_reg(sparc_reg::fcc_type, getRawBits(11, 12));
            break;
         case 4:
            condition_code_reg = sparc_reg::reg_icc;
            break;
         case 6:
            condition_code_reg = sparc_reg::reg_xcc;
            break;
         default:
            dothrow_unknowninsn();
      }
         
      if (ru) {
         *(sparc_reg_set *)(ru->maybeUsedBeforeWritten) += rs2; // read if condition is true
         *(sparc_reg_set *)(ru->maybeWritten) += rd; // written iff the condition is true

         *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += condition_code_reg;
            // fcc0-3 or icc or xcc
      }
   
      if (disassFlags) {
         dothrow_unimplinsn();
      }
   }
   else if (getRawBits(7, 9)==0x1) {
      // FMOVr -- move fp reg based on integer condition.  No cond codes are used.
      // Note: op3 == 0x35 is also used by floating-point compare and FMOVcc.
      // What this instr does: if rs1 (integer register) [eq, <=, <, neq, >=, >] 0,
      // then rd (fp reg) <- rs2 (fp reg)
      unsigned size = (opf & 0x3) == 0x1 ? 1 : (opf & 0x3) == 0x2 ? 2 : 4;
         
      sparc_reg_set rs1 = getRs1AsInt();
      sparc_reg_set rs2 = getRs2AsFloat(size);
      sparc_reg_set rd  = getRdAsFloat(size);
   
      if (ru) {
         *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1; // an *integer* reg
         *(sparc_reg_set *)(ru->maybeUsedBeforeWritten) += rs2;
         *(sparc_reg_set *)(ru->maybeWritten) += rd;
      }
   
      if (disassFlags) {
         dothrow_unimplinsn(); // not yet implemented
      }
   }
   else if (opf >= 0x051 && opf <= 0x057 && opf != 0x054) {
      // Floating-point compare.
      // unsigned fpCondCode = getRawBits(25, 26); // fcc0 thru fcc3; one gets written to.
      unsigned regPrecision = (opf & 0x3) == 0x1 ? 1 :
         (opf & 0x3) == 0x2 ? 2 : 4;
      sparc_reg_set rs1 = getRs1AsFloat(regPrecision);
      sparc_reg_set rs2 = getRs2AsFloat(regPrecision);
      sparc_reg fcc_reg = sparc_reg(sparc_reg::fcc_type, getRawBits(25, 26));
   
      const bool exceptionIfUnordered = ((opf & 0x4) != 0);
   
      if (ru) {
         *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1;
         *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2;
         if (exceptionIfUnordered)
            *(sparc_reg_set *)(ru->maybeWritten) += fcc_reg;
         else
            *(sparc_reg_set *)(ru->definitelyWritten) += fcc_reg;
      }
   
      if (disassFlags) {
         disassFlags->result = "fcmp";
         if (exceptionIfUnordered)
            disassFlags->result += "e";
         disassFlags->result += disassFloatPrecision(regPrecision);
         disassFlags->result += " ";
         disassFlags->result += fcc_reg.disass();
         disassFlags->result += pdstring(", ") + rs1.expand1().disass() + ", ";
         disassFlags->result += rs2.expand1().disass();
      }
   }
   else
      dothrow_unknowninsn();
}

void sparc_instr::getInformation10(registerUsage *ru, memUsage *mu,
                                   disassemblyFlags *disassFlags,
                                   sparc_cfi *cfi,
                                   relocateInfo *rel) const {
   // logical instructions, shifts, add, tagged add, subtract, tagged subtract,
   // mulscc, integer multiply, integer divide, save, restore, and a ton of fp
   // operations (but no fp branch, so nothing sensitive to the PC, so nothing
   // the needs changing on relocation)
   
   const unsigned op3 = getOp3();
   const sparc_reg_set rd  = getRdAsInt();
   const sparc_reg_set rs1 = getRs1AsInt();
   const sparc_reg_set rs2 = getRs2AsInt();
   const bool i = geti();

   switch (op3) {
      case 0x01: case 0x11: case 0x05: case 0x15: case 0x02: case 0x12:
      case 0x06: case 0x16: case 0x03: case 0x13: case 0x07: case 0x17: {
         // AND, ANDcc, ANDN, ANDNcc, OR, ORcc, ORN, ORNcc, XOR, XORcc, XNOR, XNORcc,
         // respectively.
      
         bool cc = op3 & 0x10;

         if (ru) {
            if (cc) {
               *(sparc_reg_set *)(ru->definitelyWritten) += sparc_reg::reg_icc;
               *(sparc_reg_set *)(ru->definitelyWritten) += sparc_reg::reg_xcc;
            }

            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1;
            if (!i)
               *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2;
            *(sparc_reg_set *)(ru->definitelyWritten) += rd;
         }

         if (disassFlags)
            disass_10_bitlogical(disassFlags, op3, i, cc, rs1, rs2, rd);

         break;
      }
      case 0x25: case 0x26: case 0x27: {
         // shift left logical, shift right logical, shift right arithmetic.
         // If a certain bit (x=bit 12) is 1, then they are 64-bit variants.
      
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1;
            if (!i)
               *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2;
            *(sparc_reg_set *)(ru->definitelyWritten) += rd;
         }

         if (disassFlags)
            disass_10_shiftlogical(disassFlags, op3, rs1, rd);

         break;
      }
      
      case 0x00: case 0x10: case 0x08: case 0x18:
      case 0x04: case 0x14: case 0x0c: case 0x1c: {
         // add, addcc, add w/ carry, addcc w/ carry,
         // sub, subcc, sub w/ carry, subcc w/ carry,
         // respectively.
         const bool cc = ((op3 & 0x10) != 0);
         const bool carry = ((op3 & 0x08) != 0);

         if (ru) {
            if (cc) {
               *(sparc_reg_set *)(ru->definitelyWritten) += sparc_reg::reg_icc;
               *(sparc_reg_set *)(ru->definitelyWritten) += sparc_reg::reg_xcc;
            }

            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1;
            if (!i)
               *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2;

            if (carry)
               *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += sparc_reg::reg_icc; // but not xcc too
               
            *(sparc_reg_set *)(ru->definitelyWritten) += rd;
         }

         if (disassFlags)
            disass_10_addsubtract(disassFlags, op3, i, cc, rs1, rd);

         break;
      }
      
      case 0x20: case 0x22: case 0x21: case 0x23:
         // tagged add and modify icc, tagged add and modify icc and trap on overflow,
         // tagged sub and modify icc, tagged sub and modify icc and trap on overflow,
         // respectively.
         // Not yet implemented.
         dothrow_unimplinsn();
         break;

      case 0x24: {
         // MULScc
         // multiply step and modify icc.  Deprecated in sparc v9.
         // Uses the y register, which we don't yet support.
         
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1;
            // ru->definitelyUsedBeforeWritten += %y;
            if (!i)
               *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2;

            *(sparc_reg_set *)(ru->definitelyWritten) += rd;
            //*(sparc_reg_set *)(ru->definitelyWritten) += %y;
         }

         if (disassFlags)
            disass_10_mulscc(disassFlags, rs1, rd);
         
         break;
      }
      case 0x0a: case 0x0b: case 0x1a: case 0x1b: case 0x0e: case 0x0f:
      case 0x1e: case 0x1f: {
         // UMUL, SMUL, UMULcc, SMULcc,
         // UDIV, SDIV, UDIVcc, SDIVcc, respectively
         // All are deprecated in v9.
      
         const bool cc = ((op3 & 0x10) != 0); // same as with add, sub
      
         if (ru) {
            if (cc) {
               *(sparc_reg_set *)(ru->definitelyWritten) += sparc_reg::reg_icc;
               *(sparc_reg_set *)(ru->definitelyWritten) += sparc_reg::reg_xcc; // is this right here?
            }

            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1;
            if (!i)
               *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2;
            *(sparc_reg_set *)(ru->definitelyWritten) += rd;
            // also definitely writes to %y, though we have no way of showing that.
         }
      
         if (disassFlags)
            disass_10_muldiv32(disassFlags, op3, cc, rs1, rd);
         
         break;
      }
      case 0x09: case 0x2d: case 0x0d: {
         // mulx (signed or unsigned), sdivx, udivx, respectively.  New with sparc v9.
         // Each read from rs1 and (rs2 if i=0, else an immediate from the instr), and
         // write the result to rd.
         
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1;
            if (!i)
               *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2;
            *(sparc_reg_set *)(ru->definitelyWritten) += rd;
         }
         
         if (disassFlags)
            disass_10_muldiv64(disassFlags, op3, rs1, rd);
         
         break;
      }

      case 0x3c: case 0x3d: {
         // save, restore
      
         const bool isSave = (op3 == 0x3c);
      
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1; // in the old frame
            if (!i)
               *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2; // the old frame
            *(sparc_reg_set *)(ru->definitelyWritten) += rd; // in the new frame
            ((sparc_ru *)ru)->sr.isSave = isSave;
            ((sparc_ru *)ru)->sr.isRestore = !isSave;
         }
      
         if (disassFlags)
            disass_10_saverestore(disassFlags, isSave, rs1, rs2, rd);
         
         break;
      }
      case 0x3e: {
         // DONE and RETRY...return from trap (skipping and retrying the trapped 
         // instruction, respectively).  New with sparc v9.  Do not execute a done or
         // retry in a delay slot.

         // Obviously we need to start handling these insns.  Both return from a trap
         // handler.  Thus both (it would seem) end not only the current basic block
         // but also the function...the only real difference is where they return to,
         // which we don't care too much about, right?

         const unsigned fcn = getRawBits(25, 29);

         if (fcn == 0 || fcn == 1) {
            // 0 --> done (return from trap);
            // 1 --> retry (return from trap; reexecute trap insn)

            if (ru) {
               // TSTATE is used to restore CWP, ASI, CCR, and PSTATE.
               // PC and nPC are set.  TL is decremented.
               // None of these regs are implemented in ru yet.
            }

            if (cfi) {
               cfi->fields.controlFlowInstruction = true;
               cfi->delaySlot = instr_t::dsNone;
               cfi->sparc_fields.isDoneOrRetry = true;
               // many not applicable fields left undefined (e.g. cfi->destination)
            }

            if (disassFlags)
               disass_10_doneretry(disassFlags, fcn);
            
         }
         else
            // fcn from 2 thru 31: reserved
            dothrow_unknowninsn();

         break;
      }
      case 0x38: {
         // jmpl
         if (ru != NULL) {
            *(sparc_reg_set *)(ru->definitelyWritten) += sparc_reg::reg_pc;

            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1;
            if (!i)
               *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2;
            *(sparc_reg_set *)(ru->definitelyWritten) += rd; // link register
         }
      
         if (cfi != NULL) {
            cfi->fields.controlFlowInstruction = true;
            cfi->delaySlot = instr_t::dsAlways;
            cfi->fields.isJmpLink = true;

            const int offset_bytes = i ? getSimm13() : 0;
               // no *4 of the simm13, unlike cond branch
            
            if (i) {
               if (rs1 == sparc_reg::i7 && rd == sparc_reg::g0 &&
                   (offset_bytes == 8 || offset_bytes == 12))
                  // the check for 8 is obvious; I've seen 12 used in order to return
                  // past an illtrap instruction generated after the call & its delay
                  // slot, used when a structure is returned.  The idea (a stupid one
                  // imho) is to add a little type-safety to C...if the callee returns
                  // normally, then the illtrap will be generated.  On the other hand,
                  // if it returns 4 bytes past the usual spot (indicating that it was
                  // aware that it was supposed to return a structure), then things
                  // work out.
                  cfi->fields.isRet = true;
               else if (rs1 == sparc_reg::o7 && rd == sparc_reg::g0 &&
                        (offset_bytes == 8 || offset_bytes == 12))
                  // see above for comments on the simm13==12 case...
                  cfi->fields.isRet = true;
            }
            
            if (rd == sparc_reg::o7)
               cfi->fields.isCall = true;
         
            cfi->destination = (i ? controlFlowInfo::regRelative :
                                controlFlowInfo::doubleReg);
                                
            cfi->destreg1 = (reg_t*)new sparc_reg(rs1.expand1());
            if (!i)
               cfi->destreg2 = (reg_t*)new sparc_reg(rs2.expand1());
            else
               cfi->offset_nbytes = offset_bytes;
         }

         if (disassFlags)
            disass_10_jmpl(disassFlags, rs1, rd);

         break;
      }
      
      case 0x39: {
         // v8: RETT -- return from trap.  Privileged instruction.
         // v9: RETURN -- not privileged.  Combines the effect of a trivial restore
         //     instruction (adjusts the register window) and a jump instruction (though
         //     the PC isn't  written to any destination register).  Note that return has
         //     big register ramifications (shifts the register window back, like
         //     restore), plus big control flow ramifications (does a jump [no link])
         // In the following, we assume that it's a v9 RETURN instruction.
         // Note that there's no implicit mulitplication by 4 of the simm13 (like
         // jmpl and unlike conditional branches, to give examples)
      
         if (ru) { 
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1; // in the old frame
            if (!i) 
               *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2; // in the old frame

            *(sparc_reg_set *)(ru->definitelyWritten) += sparc_reg::reg_pc;

            ((sparc_ru *)ru)->sr.isV9Return = true;
         }
      
         if (cfi) {
            cfi->fields.controlFlowInstruction = true;
            cfi->sparc_fields.isV9Return = true;
            cfi->delaySlot = instr_t::dsAlways;

            cfi->destination = (!i ? controlFlowInfo::doubleReg :
                                controlFlowInfo::regRelative);
            cfi->destreg1 = (reg_t*)new sparc_reg(rs1.expand1());
            if (!i)
               cfi->destreg2 = (reg_t*)new sparc_reg(rs2.expand1());
            else
               cfi->offset_nbytes = getSimm13(); // no *4, on purpose
         }

      
         if (disassFlags)
            disass_10_v9return(disassFlags);
         
         break;
      }
      case 0x3a: {
         // ticc.

         const bool cc1 = getRawBit(12);
         const bool cc0 = getRawBit(11);
         
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1;
            if (!i)
               *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2;
         }
      
         if (disassFlags)
            disass_10_ticc(disassFlags, i, cc0, cc1, rs2);
         
         // What to do about control flow information???  Perhaps a trap will return;
         // perhaps it won't!  For now, we'll work on the (flimsy) assumption that
         // the trap traps into the kernel, which modifies no registers, and returns
         // to the insn after the trap.

         break;
      }
      case 0x28: {
         // read state register (rd %special, rd)
         // we'd like to pull out "*(sparc_reg_set *)(ru->definitelyWritten) += rd" but can't since
         // it doesn't apply when rs1 is 15 (membar).
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyWritten) += rd;
            
            switch (getRawBits(14, 18)) { // rs1 bits
               case 0: // rd %y, rd
                  // ru->definitelyUsedBeforeWritten += %y
                  break;
               case 1: // reserved
                  dothrow_unknowninsn();
                  break;
               case 2: // rd %ccr, rd
                  *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += sparc_reg::reg_xcc;
                  *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += sparc_reg::reg_icc;
                  break;
               case 3: // rd %asi, rd
                  *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += sparc_reg::reg_asi;
                  break;
               case 4: // rd %tick, rd
                  // ru->definitelyUsedBeforeWritten += %tick
                  break;
               case 5: // rd %pc, rd
                  *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += sparc_reg::reg_pc;
                  break;
               case 6:
                  // %fprs, which includes %fcc0 thru 3
                  *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += sparc_reg::reg_fcc0;
                  *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += sparc_reg::reg_fcc1;
                  *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += sparc_reg::reg_fcc2;
                  *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += sparc_reg::reg_fcc3;
                  break;
               case 7:
               case 8:
               case 9:
               case 10:
               case 11:
               case 12:
               case 13:
               case 14: // read ancillary state register (reserved)
                  dothrow_unknowninsn();
                  break;
               case 15: {
                  // note: when rs1 is 15, things get complex:
                  //       -- if rd=0 and i=0, then it's a stbar
                  //       -- if rd=0 and i=1, then it's a membar
                  //       -- if rd!=0 then reserved
                  if (getRawBits(25, 29) == 0 && !i) {
                     // stbar (v8)...obsoleted by membar in v9.
                     // no regs read or written
                  }
                  else if (getRawBits(25, 29) == 0 && i) {
                     // membar (new in v9).  No regs read or written
                     ;
                  }
                  else
                     // reserved in v9
                     dothrow_unknowninsn();

                  break;
               }
               case 19: // gsr (grafx status register of ultrasparc)
                  // 19: %gsr
                  *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += sparc_reg::reg_gsr;
                  break;
                  
               default:
                  // 16-32: read state register of impl-dependent reg.
                  // ru->definitelyUsedBeforeWritten += <%statereg>
                  break;
            } // switch on rs1 bits
         } // if (ru)

         if (disassFlags)
            disass_10_rd_state_reg(disassFlags, rd, getRawBits(14, 18), i);

         break;
      }
      
      case 0x29:
         // RDPSR -- read processor state register.  privileged.  v8 only because this
         // register no longer exists in v9!
         dothrow_unknowninsn();
         break;

      case 0x2a: {
         // v8: RDWIM -- read window invalid mask register (privileged)
         //              this register no longer exists in v9!
         // v9: RDPR -- read privileged register (which register is specified in rs1)
         //     values of rs1: 0=tpc, 1=tnpc, 2=tstate, 3=tt, 4=tick, 5=tba, 6=pstate,
         //     7=tl, 8=pil, 9=cwp, 10=cansave, 11=canrestore, 12=cleanwin, 13=wstate,
         //     14=wstate, 15=fq, 16-30 reserved, 31=ver 
      
         const unsigned whichreg = getRawBits(14, 18); // where rs1 usually resides. 
         if (whichreg >= 16 && whichreg <= 30) 
            dothrow_unknowninsn(); // values 16 to 30 are reserved in v9
      
         if (ru) { 
            *(sparc_reg_set *)(ru->definitelyWritten) += rd;

            // should add 'whichreg' to definitelyUsedBeforeWritten, but we have no way
            // of representing those registers yet.  (Well, one or two are represented:)
            if (whichreg == 8)
               *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += sparc_reg::reg_pil;
         } 

         if (disassFlags)
            disass_10_rdpr(disassFlags, whichreg, rd);
         
         break;
      }
      case 0x2b: {
         // v8: RDTBR -- read trap base register (privileged)
         //              this register no longer exists in v9!
         // v9: FLUSHW -- flush register windows (not privileged)
         // Here, we'll assume flushw.

         // It's hard to put a finger on the effects of a FLUSHW.  Since it flushes
         // all reg windows (except the curr one), it certainly reads some registers.
         // But since they're not in the current window, how do we specify them?
         if (ru) 
            ; // no regs 

         // FLUSHW defintely writes to memory (where it dumps the reg window contents),
         // but I'm not sure how to specify these locations.
         if (mu) {
            mu->memWritten = true;
            ; // mu->addrWritten = xxx;
         }
      
         if (disassFlags)
            disassFlags->result = "flushw";

         break;
      }

      case 0x30: {
         // wrasr
         unsigned rdBits = getRawBits(25, 29);
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1;
            if (!i)
               *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2;

            if (rdBits == 0)
               // WRY -- write Y register.  Deprecated in v9.
               // *(sparc_reg_set *)(ru->definitelyWritten) += %y
               ;
            else if (rdBits == 1)
               dothrow_unknowninsn(); // rd=1 undefined in 9
            else if (rdBits == 2) {
               // %ccr -- the *integer* condition codes register
               // (for %fcc0-3, look in the floating point status register)
               *(sparc_reg_set *)(ru->definitelyWritten) += sparc_reg::reg_icc;
               *(sparc_reg_set *)(ru->definitelyWritten) += sparc_reg::reg_xcc;
            }
            else if (rdBits == 3)
               // write %asi
               *(sparc_reg_set *)(ru->definitelyWritten) += sparc_reg::reg_asi;
            else if (rdBits == 4 || rdBits == 5)
               dothrow_unknowninsn(); // rd==4 reserved in v9
            else if (rdBits == 6) {
               // wr %fprs.  This writes (among other things) %fcc0 thru 3
               *(sparc_reg_set *)(ru->definitelyWritten) += sparc_reg::reg_fcc0;
               *(sparc_reg_set *)(ru->definitelyWritten) += sparc_reg::reg_fcc1;
               *(sparc_reg_set *)(ru->definitelyWritten) += sparc_reg::reg_fcc2;
               *(sparc_reg_set *)(ru->definitelyWritten) += sparc_reg::reg_fcc3;
            }
            else if (rdBits < 14)
               dothrow_unknowninsn(); // 7-14 reserved in v9
            else if (rdBits == 15)
               // could be sir
               dothrow_unknowninsn();
            else if (rdBits == 19)
               // 19: gsr (impdep grafx status register of ultrasparc)
               *(sparc_reg_set *)(ru->definitelyWritten) += sparc_reg::reg_gsr;
            else
               // WRASR 16-31 are impl dependent (allowed in v9)
               // privileged instr if dest reg is privileged

               if (ru)
               ; // *(sparc_reg_set *)(ru->definitelyWritten) += %<this asr reg>
         }

         if (disassFlags)
            disass_10_wrasr(disassFlags, rs1, rdBits);

         break;
      }

      case 0x31: {
         // v8: wrpsr.  (This reg no longer exists in v9)
         // v9: saved and restored -- adjust the state of regwin control regs.
         // privileged.
         // Spill handlers use SAVED to indicate a window has been spilled OK.
         // Fill handlers use RESTORED to indicate a window has been filled OK.
         // Judging by this, we'll say that there are no mu or cfi effects.
         // ru effects: reading/writing reg win ctl regs (cansave, otherwin, canrestore,
         // otherwin, cleanwin, nwindows)

         const unsigned fcn = getRawBits(25, 29);
         assert(fcn == 0 || fcn == 1);
         const bool saved = (fcn == 0);

         if (ru) {
            if (saved)
               ; // write to CANSAVE.  Read from OTHERWIN.  Maybe write to CANRESTORE.
            // maybe write to OTHERWIN.
            else
               ; // write to CANRESTORE.  Read from OTHERWIN.  Maybe write to CANSAVE.
            // maybe write to OTHERWIN.  Maybe write to CLEANWIN.
         }

         if (disassFlags)
            disass_10_saved_restored(disassFlags, saved);

         break;
      }
      case 0x32: {
         // v8: WRWIM -- write window invalid mask register
         // v9: WRPR  -- write privileged register (always a privileged instr)
         //              wrpr rs1, reg_or_simm13, rd  (xor operation)

         // The following assumes v9 wrpr:
         // rs1 is the src register.  rd encodes the privileged register.
         const unsigned privRegNum = getRawBits(25, 29); // where rd usually resides
         assert(privRegNum < 15); // only values 0-14 are supported in v9
      
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1;
            if (!i)
               *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2;
	 
            // can't add 'privRegNum' to *(sparc_reg_set *)(ru->definitelyWritten) because esoteric regs
            // are not yet implemented. (well, one or two are:)
            if (privRegNum == 8)
               *(sparc_reg_set *)(ru->definitelyWritten) += sparc_reg::reg_pil;
         }

         if (disassFlags)
            disass_10_wrpr(disassFlags, rs1);
         
         break;
      }
      case 0x33:
         // WRTBR -- write trap base register -- does this exist in v9???
         // privileged
         dothrow_unimplinsn();
         break;

      case 0x3b:
         // FLUSH -- flush instruction memory.  See H.1.6, self-modifying code, in sparc
         // v9 arch manual.
         // Ensure that a double-word is consistent across local caches, and, in a
         // multiprocessor, will eventually become consistent elsewhere.
         //
         // More specifically: flush ensures that ifetches of this addr by the proc
         // issuing the flush will appear to execute after any loads, stores, and
         // atomic load-stores to that addr issued by that proc for that addr prior to
         // the flush.  So much for in a uniprocessor.  In a multiprocessor system,
         // flush also ensures that these values will eventually become visible to
         // ifetches of all other processors.
         // flush behaves as if it were a store w.r.t. to membar-orderings
         // (see A.31, memory barrier, in sparc v9 manual)
         //
         // notes: Flush is needed only between a store and subsequent ifetch from the
         // modified location.  When multiple processes may concurrently modify
         // potentially-executing code, care must be taken to ensure that the order of
         // update maintains the program in a semantically correct form at all times.
         //
         // In a uniprocessor, the memory model guarantees that _data_ (not instr) loads
         // observe the results of the most recent store, even if there's no flush.
      
         if (ru) {
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1;
            if (!i)
               *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2;
         }
      
         if (disassFlags)
            disass_10_flush(disassFlags);
         
         break;

      case 0x2c: {
         // move integer register on condition -- either int code code or fp cond code.
         // MOVcc.  New with sparc v9.
      
         const bool cc2 = getRawBit(18);
         const bool cc1 = getRawBit(12);
         const bool cc0 = getRawBit(11);

         sparc_reg condition_reg(sparc_reg::g0); // for now
         if (!cc2) {
            unsigned fccnum = ((unsigned)cc1 << 1) | (unsigned)cc0;
            condition_reg = sparc_reg(sparc_reg::fcc_type, fccnum);
         }
         else if (!cc1 && !cc0)
            condition_reg = sparc_reg::reg_icc;
         else if (cc1 && !cc0)
            condition_reg = sparc_reg::reg_xcc;
         else
            dothrow_unknowninsn();
      
         if (ru) { 
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += condition_reg;

            // rd is written to _if_ the condition is true.
            const unsigned condBits = getRawBits(14, 17);
            const bool always = (cc2 ? sparc_instr::iccAlways == (sparc_instr::IntCondCodes)condBits : sparc_instr::fccAlways == (sparc_instr::FloatCondCodes)condBits);
            const bool never = (cc2 ? sparc_instr::iccNever == (sparc_instr::IntCondCodes)condBits : sparc_instr::fccNever == (sparc_instr::FloatCondCodes)condBits);

            if (always)
               *(sparc_reg_set *)(ru->definitelyWritten) += rd;
            else if (never)
               ;
            else
               *(sparc_reg_set *)(ru->maybeWritten) += rd;

            if (!i)
               *(sparc_reg_set *)(ru->maybeUsedBeforeWritten) += rs2; 
         } 
      
         if (disassFlags)
            disass_10_movcc(disassFlags, condition_reg, rd);
         
         break;
      }
      case 0x2f: {
         // move integer register on register condition (MOVR).  New with sparc v9.
         // don't confuse with MOVcc (also new in v9), which uses condition code bits. 
         // MOVR doesn't use any condition codes. 
         // How it works: rs1 is read; if it satisfies a condition specified in the
         // instr  (=, !=, <=, <, >, >= to zero), then rd is written with rs2 (if i=0)
         // or with an immediate from the instruction (if i=1) 
      
         const unsigned cond = getRawBits(10, 12);

         if (ru) { 
            *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1; 
            if (!i)
               *(sparc_reg_set *)(ru->maybeUsedBeforeWritten) += rs2;
         
            *(sparc_reg_set *)(ru->maybeWritten) += rd;
         } 
      
         if (disassFlags)
            disass_10_movr(disassFlags, cond, rs1, rs2, rd);
         
         break;
      }
      case 0x2e: {
         // population count (popc).  New with sparc v9.  Count the number of 1 bits
         // in rs2 (if i=0) or in a constant (if i=1), and stores the result in rd.
         // registers used: rs2 (if i=0) else none.  regs written: rd.  rs1 isn't used. 
         // (is it just me, or is popc on a fixed constant a silly instruction?)
      
         unsigned encoding = getRawBits(14, 18); 
      
         // bits 14-18 must be 0 for popc; other values are reserved in v9.
         if (encoding != 0) 
            dothrow_unknowninsn();
      
         if (ru) { 
            if (!i)
               *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs2; 
            *(sparc_reg_set *)(ru->definitelyWritten) += rd;
         } 
      
         if (disassFlags)
            disass_10_popc(disassFlags, rd);
         
         break;
      }
      case 0x34:
         getInformation_10_0x34_floats(ru, disassFlags);
         break;

      case 0x35:
         getInformation_10_0x35_floats(ru, disassFlags);
         break;
         
      case 0x36:
         // in v8: CPop1, CPop2.
         // in v9: impl-dependent instructions.
         // Refer to Chap 13, "UltraSPARC Extended Instructions" of
         // "UltraSPARC User's Manual", available online at
         // www.sun.com/microelectronics/manuals/
         getInformation10_unimpl_0x36(ru, disassFlags,
                                      rd, rs1, rs2);
         break;
   
      case 0x37:
         // Unknown impdep instruction.  This sucks.
         // We'll disassemble them, but we can't provide any
         // information about them.

         if (ru || mu || cfi || rel)
            dothrow_unknowninsn(); // not yet implemented; never will be.

         if (disassFlags) {
            const char *ops[] = {"impdep1", "impdep2"};
            disassFlags->result = ops[op3 - 0x36];
            disassFlags->result += pdstring(" ") +
                                   num2string(getRawBits(25, 29)) +
                                   " " +
                                   num2string(getRawBits(0, 18));
         }
         
         break;

      default:
         dothrow_unknowninsn();
         break;
   }
}

void sparc_instr::disass_01_call(disassemblyFlags *disassFlags,
                                 kaddrdiff_t displacement) const {
   disassFlags->result = "call ";
      
   const kptr_t newpc = disassFlags->pc + displacement;

   disassFlags->result += addr2hex(newpc);
   
   if (disassFlags->disassemble_destfuncaddr)
      disassFlags->destfunc_result += disassFlags->disassemble_destfuncaddr(disassFlags->pc + displacement);
}

void sparc_instr::getInformation01(registerUsage *ru,
                                   memUsage *,
                                   disassemblyFlags *disassFlags,
                                   sparc_cfi *cfi,
                                   relocateInfo *rel) const {
   // call and link into %o7
   // NOTE: the address contained in the call is pc-relative, so in order to
   // make a meaningful disassembly, we need the pc
   
   const kaddrdiff_t displacement = getBitsSignExtend(0, 30) * 4;
   
   if (ru) {
      *(sparc_reg_set *)(ru->definitelyWritten) += sparc_reg::reg_pc;
      *(sparc_reg_set *)(ru->definitelyWritten) += sparc_reg::o7;
   }
   
   if (cfi) {
      cfi->fields.controlFlowInstruction = true;
      cfi->fields.isCall = true;
      cfi->delaySlot = instr_t::dsAlways;
      
      cfi->destination = controlFlowInfo::pcRelative;
      cfi->offset_nbytes = displacement; // yep, we already multiplied by 4
   }
   
   if (disassFlags)
      disass_01_call(disassFlags, displacement);
   
   if (rel) {
      // old_pc + old_displacement = destination_addr
      // new_pc + new_displacement = destination_addr
      // combine the two equations and solve for new_displacement to get:
      // new_displacement = old_pc + old_displacement - new_pc
      
      // note: new_displacement is kaddrdiff_t, not kptr_t.  Beware of
      // doing unsigned arithmetic here which can lead to negative numbers.
      kaddrdiff_t new_displacement = rel->old_insnaddr + displacement - 
	 rel->new_insnaddr;

      rel->result = sparc_instr(callandlink, new_displacement).getRaw();
   }
}

void sparc_instr::disass_00_sethi(disassemblyFlags *disassFlags,
                                  const sparc_reg_set &rd) const {
   // sethi or nop
   unsigned imm22 = getRawBits0(21);
   if (rd == sparc_reg::g0 && imm22 == 0) {
      // nop
      disassFlags->result = "nop";
   }
   else {
      disassFlags->result = "sethi %hi(";
      imm22 <<= 10;
            
      disassFlags->result += disass_uimm(imm22);
      // false --> display as hex
            
      disassFlags->result += "), ";
      disassFlags->result += rd.disassx();
   }
}

void sparc_instr::disass_00_bpr(disassemblyFlags *disassFlags,
                                unsigned cond, bool annulled, bool predict,
                                const sparc_reg_set &rs1,
                                int offset) const {
   disassFlags->result = "br";
   disassFlags->result += disass_reg_cond(cond);
         
   if (annulled)
      disassFlags->result += ",a";
         
   if (predict)
      disassFlags->result += ",pt";
   else
      disassFlags->result += ",pn";
         
   disassFlags->result += pdstring(" ") + rs1.disassx();
   disassFlags->result += ", ";
         
   // absolute address
   const kptr_t newpc = disassFlags->pc + offset;

   disassFlags->result += addr2hex(newpc);

   if (disassFlags->disassemble_destfuncaddr)
      disassFlags->destfunc_result += disassFlags->disassemble_destfuncaddr(newpc);
}

void sparc_instr::disass_00_bpcc_fbpcc(disassemblyFlags *disassFlags,
                                       bool floatOp, bool predict, bool annulled,
                                       bool cc0, bool cc1, int displacement) const {
   if (floatOp)
      disassFlags->result = pdstring("fb") +
                            disass_float_cond_code(getFloatCondCode_25_28());
   else
      disassFlags->result = pdstring("b") + disass_int_cond_code(getIntCondCode_25_28());

   if (annulled)
      disassFlags->result += ",a";

   if (predict)
      disassFlags->result += ",pt";
   else
      disassFlags->result += ",pn";

   disassFlags->result += " ";

   if (floatOp) {
      unsigned num = 0;
      num = (cc1 << 1) | cc0;
      assert(num < 4);
      disassFlags->result += sparc_reg(sparc_reg::fcc_type, num).disass() + ", ";
   }
   else {
      if (cc1 && !cc0)
         disassFlags->result += sparc_reg::reg_xcc.disass() + ", ";
      else if (!cc1 && !cc0)
         disassFlags->result += sparc_reg::reg_icc.disass() + ", ";
      else
         dothrow_unknowninsn();
   }
   	 
   const kptr_t newPC = disassFlags->pc + displacement;

   disassFlags->result += addr2hex(newPC);

   if (disassFlags->disassemble_destfuncaddr)
      disassFlags->destfunc_result += disassFlags->disassemble_destfuncaddr(newPC);
}

void sparc_instr::disass_00_bicc_fbfcc(disassemblyFlags *disassFlags,
                                       bool floatOp, bool annulled,
                                       int displacement) const {
   if (floatOp)
      disassFlags->result = pdstring("fb") +
                            disass_float_cond_code(getFloatCondCode_25_28());
   else
      disassFlags->result = pdstring("b") + disass_int_cond_code(getIntCondCode_25_28());
         
   if (annulled)
      disassFlags->result += ",a";
   disassFlags->result += " ";
         
   kptr_t newPC = disassFlags->pc + displacement;

   disassFlags->result += addr2hex(newPC);

   if (disassFlags->disassemble_destfuncaddr)
      disassFlags->destfunc_result += disassFlags->disassemble_destfuncaddr(newPC);
}

void sparc_instr::getInformation00(registerUsage *ru,
                                   memUsage *,
                                   disassemblyFlags *disassFlags,
                                   sparc_cfi *cfi,
                                   relocateInfo *rel) const {
   // sethi, nop (actually a version of sethi), branch on icc, branch on fp cc's,
   // branch on coproc cc's, unimp.
   
   // bits 22 thru 24 are the op3 here, in effect.
   unsigned temp = getRawBits(22, 24);
   
   const sparc_reg_set rd = getRdAsInt();
   const sparc_reg_set rs1 = getRs1AsInt();

   switch (temp) {   
   case 0x4: {
      // sethi or nop.
      if (ru)
         // rd is written to, no register is read from.
         *(sparc_reg_set *)(ru->definitelyWritten) += rd;
      
      if (disassFlags)
         disass_00_sethi(disassFlags, rd);
      
      break;
   }
   case 0x3: {
      // branch on integer register w/ prediction (BPr).  New with v9.  Don't confuse
      // with BPcc, which uses condition codes.  Here, we don't use any condition codes.
      //
      // How it works.  Treat rs1 as signed; if condition (specified in instr) is true
      // then do a pc-relative branch...bits 21-20 and 13-0 give, when multiplied by
      // 4, the offset.
      // Bit 29 is the annulled bit.  Bit 28 is 0.  Bits 25-27 give the condition.
      // Bit 19 is the predict bit (1 --> expect taken).
      // rs1 is bits 14 to 18, as usual.
      //
      // If branch is taken, the delay instr is always executed (regardless of annul!)
      // If branch is not taken, then the annul bit is respected.
      
      const unsigned offset_hi = getRawBits(20, 21);
      const unsigned offset_lo = getRawBits0(13);
      const unsigned offsetbits = (offset_hi << 14) | offset_lo;
      const int offset = signExtend(offsetbits, 16) * 4;
      
      const bool predict = getPredict();
      const unsigned cond = getRawBits(25, 27);
      const bool annulled = getRawBit(29);
      if (getRawBit(28) != 0)
         dothrow_unknowninsn();
      
      if (ru) {
         *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += rs1;
	 *(sparc_reg_set *)(ru->maybeWritten) += sparc_reg::reg_pc;
      }
      
      if (cfi) {
         cfi->fields.controlFlowInstruction = true;
         cfi->sparc_fields.annul = annulled;
         cfi->condition = cond;
         cfi->sparc_fields.isBPr = true;
         cfi->sparc_fields.predict = predict;
         cfi->destination = controlFlowInfo::pcRelative;
         cfi->offset_nbytes = offset; // we multiplied the simm16 by 4 already
         cfi->conditionReg = (reg_t*)new sparc_reg(rs1.expand1());
         
         // Sorting out delay slots for BPr: A delay slot always exists.  There are
         // no unconditional BPr's, so it all depends on the annul bit.  If no annul
         // bit, then the delay slot is always executed; else, it's executed only
         // when the branch is taken.
         if (!annulled)
            cfi->delaySlot = instr_t::dsAlways;
         else
            cfi->delaySlot = instr_t::dsWhenTaken;
      }

      if (disassFlags)
         disass_00_bpr(disassFlags, cond, annulled, predict, rs1, offset);
      
      if (rel) {
         // Using the formula we derived in callandlink:
         // new_displacement = old_pc + old_displacement - new_pc
         rel->success = inRangeOfBPr(rel->old_insnaddr, rel->new_insnaddr);
         if (rel->success) {
            kaddrdiff_t new_displacement = rel->old_insnaddr + offset - 
	       rel->new_insnaddr;

            rel->result = sparc_instr(bpr, cond, annulled, predict,
				      rs1.expand1(), 
				      new_displacement).getRaw();
         }
      }
      
      break;
   }
   case 0x1: case 0x5: {
      // branch on integer cond codes with prediction (BPcc) or
      // branch on fp cond codes with prediction (FBPcc), respectively.
      // Both are new with v9.
      const IntCondCodes icond = getIntCondCode_25_28();
      const FloatCondCodes fcond = getFloatCondCode_25_28();
      const bool cc1 = getcc1();
      const bool cc0 = getcc0();
      const bool predict = getPredict();
      const bool annulled = getRawBit(29);
      const int displacement = getBitsSignExtend(0, 18) * 4;
      const bool floatOp = (temp == 0x5);
      
      if (ru) {
	 *(sparc_reg_set *)(ru->maybeWritten) += sparc_reg::reg_pc;
	 // the maybe can be changed to definitely if the branch is always
	 // taken.

	 if (!floatOp) {
	    if (icond == iccAlways) {
	       *(sparc_reg_set *)(ru->maybeWritten) -= sparc_reg::reg_pc;
	       *(sparc_reg_set *)(ru->definitelyWritten) += sparc_reg::reg_pc;
	    }
	    else if (icond == iccNever) {
	       *(sparc_reg_set *)(ru->maybeWritten) -= sparc_reg::reg_pc;
	    }
            else
               *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) = 
		 cc1 ? sparc_reg_set(sparc_reg::reg_xcc) : 
	               sparc_reg_set(sparc_reg::reg_icc);
	 }
         else {
	    if (fcond == fccAlways) {
	       *(sparc_reg_set *)(ru->maybeWritten) -= sparc_reg::reg_pc;
	       *(sparc_reg_set *)(ru->definitelyWritten) += sparc_reg::reg_pc;
	    }
	    else if (fcond == fccNever)
	       *(sparc_reg_set *)(ru->maybeWritten) -= sparc_reg::reg_pc;
            else {
               const unsigned fccnum = ((unsigned)cc1 << 1) | cc0;
               *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) 
		 += sparc_reg(sparc_reg::fcc_type, fccnum);
            }
	 }
      }
      
      if (cfi) {
         cfi->fields.controlFlowInstruction = true; // even for the inert bn
         cfi->sparc_fields.annul = annulled;
         cfi->sparc_fields.predict = predict;

         if (floatOp) {
            cfi->sparc_fields.isFBPfcc = true;
            cfi->cctype = controlFlowInfo::floatcc;
	    cfi->condition = fcond;
            cfi->fcc_num = ((unsigned)cc1 << 1) | (unsigned)cc0;
            assert(cfi->fcc_num < 4);
         }
         else {
            cfi->sparc_fields.isBPcc = true;
            cfi->cctype = controlFlowInfo::intcc;
	    cfi->condition = icond;
            cfi->xcc = cc1 ? true : false;
         }
         
         cfi->destination = controlFlowInfo::pcRelative;
         cfi->offset_nbytes = displacement; // we multiplied by 4 alread

         // Sorting out the mess of delay slots:
         // See sparc_instr.h for the breakdown

         if (!annulled)
            // b<any> when not annulled, including ba and bn
            cfi->delaySlot = dsAlways;
         else if (!floatOp && cfi->condition == iccAlways ||
                  floatOp && cfi->condition == fccAlways)
            // ba,a
            cfi->delaySlot = dsNone;
         else if (!floatOp && cfi->condition == iccNever ||
                  floatOp && cfi->condition == fccNever)
            // the weirdest case: bn,a   Delay slot position exists
            // but is never executed, so it could even contain data.
            cfi->delaySlot = dsNever;
         else
            // b<cond>, a, where <cond> is anything except always or never.
            // Delay slot exists and is executed iff the branch is taken.
            cfi->delaySlot = dsWhenTaken;
      }

      if (disassFlags)
         disass_00_bpcc_fbpcc(disassFlags, floatOp, predict, annulled, cc0, cc1,
                              displacement);
      
      if (rel) {
         // as we derived in callandlink,
         // new_displacement = old_pc + old_displacement - new_pc
         rel->success = inRangeOfBPcc(rel->old_insnaddr, rel->new_insnaddr);
         if (rel->success) {
            kaddrdiff_t new_displacement = rel->old_insnaddr + displacement -
	       rel->new_insnaddr;
            if (temp == 0x1) // BPcc
               rel->result = sparc_instr(bpcc, icond, annulled, predict,
                                             cc1 && !cc0, // xcc?
                                             new_displacement).getRaw();
            else {
               assert(temp == 0x5); // FBPfcc
               rel->result = sparc_instr(fbpfcc, fcond, annulled, predict,
                                             cc1 ? (cc0 ? 3 : 2) : (cc0 ? 1 : 0),
                                             new_displacement).getRaw();
            }
         }
      }
      
      break;
   }
   case 0x2: case 0x6: {
      // branch on integer condition code (bicc) or
      // branch on fp cond code (FBfcc).  Both are deprecated in v9.
      
      const bool annulled = getRawBit(29);
      const IntCondCodes icond = getIntCondCode_25_28(); // bits 25-28
      const FloatCondCodes fcond = getFloatCondCode_25_28(); // bits 25-28
      const int displacement = getBitsSignExtend(0, 21) * 4;
      const bool floatOp = (temp == 0x6);

      const bool always = (floatOp && fcond == fccAlways ||
                           !floatOp && icond == iccAlways);
      const bool never = (floatOp && fcond == fccNever ||
                           !floatOp && icond == iccNever);
      
      if (ru) {
	 *(sparc_reg_set *)(ru->maybeWritten) += sparc_reg::reg_pc;

         if (always) {
            *(sparc_reg_set *)(ru->maybeWritten) -= sparc_reg::reg_pc;
            *(sparc_reg_set *)(ru->definitelyWritten) += sparc_reg::reg_pc;
         }
         else if (never)
            *(sparc_reg_set *)(ru->maybeWritten) -= sparc_reg::reg_pc;
         else {
            // conditional
            if (floatOp)
               *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += sparc_reg::reg_fcc0;
            else
               *(sparc_reg_set *)(ru->definitelyUsedBeforeWritten) += sparc_reg::reg_icc;
         }
      }
      
      if (cfi) {
         cfi->fields.controlFlowInstruction = true;
         cfi->sparc_fields.annul = annulled;
         if (floatOp) {
	    cfi->cctype = controlFlowInfo::floatcc;
            cfi->condition = fcond;
            cfi->sparc_fields.isFBfcc = true;
            cfi->fcc_num = 0;
         }
         else {
	    cfi->cctype = controlFlowInfo::intcc;
            cfi->condition = icond;
            cfi->sparc_fields.isBicc = true;
            cfi->xcc = false; // not really needed, implicit
         }
         
         cfi->destination = controlFlowInfo::pcRelative;
         cfi->offset_nbytes = displacement; // we multiplied by 4 already

         // Sorting out the mess of delay slots:
         // See sparc_instr.h for the breakdown

         if (!annulled)
            // b<any> when not annulled, including ba and bn --> ds is always executed
            cfi->delaySlot = dsAlways;
         else if (always)
            // ba,a (int or float version) --> ds doesn't exist
            cfi->delaySlot = dsNone;
         else if (never)
            // the weirdest case: bn,a   Delay slot position exists
            // but is never executed, so it could even contain data.
            cfi->delaySlot = dsNever;
         else
            // b<cond>, a, where <cond> is anything except always or never.
            // The delay slot exists and is executed iff the branch is taken.
            cfi->delaySlot = dsWhenTaken;
      }

      if (disassFlags)
         disass_00_bicc_fbfcc(disassFlags, floatOp, annulled, displacement);
      
      if (rel) {
         // new_displacement = old_pc + old_displacement - new_pc
         rel->success = inRangeOfBicc(rel->old_insnaddr, rel->new_insnaddr);
         if (rel->success) {
            kaddrdiff_t new_displacement = rel->old_insnaddr + displacement -
	       rel->new_insnaddr;
            if (temp == 0x2) // bicc
               rel->result = sparc_instr(bicc, icond, annulled,
					 new_displacement).getRaw();
            else { // fbfcc
               assert(temp == 0x6);
               rel->result = sparc_instr(fbfcc, fcond, annulled,
					 new_displacement).getRaw();
            }
         }
      }
      
      break;
   }
   case 0x7:
      // branch on coproc cond codes.  v8 only?
      dothrow_unknowninsn();
      break;

   case 0x0: {
      // UNIMP instruction (causes an illegal_instr trap).  The const22 is
      // ignored by the hardware.  For our purposes, no registers are used or
      // written to.
      // note: in v9, it's called illtrap, not unimp

      if (disassFlags) {
	 const unsigned const22 = getRawBits0(21);
         disassFlags->result = "illtrap ";
         disassFlags->result += disass_uimm(const22);
      }
      break;
   }
   default:
      dothrow_unknowninsn(); // unknown bits 22-24
   } // switch
}

void sparc_instr::getInformation(registerUsage *ru,
                                 memUsage *mu,
                                 disassemblyFlags *df,
                                 controlFlowInfo *icfi,
                                 relocateInfo *rel) const {
   // Why do we do so much (disass, get register usage, get control flow
   // information) in one routine?  So there is only one place where code 
   // exists to parse the raw instructions's bits (opcode, etc.), thus 
   // reducing the chance of tricky bit parsing bugs spreading.  However, 
   // we must be careful not to cram too much code "inlined" into a single 
   // function, lest we fry the icache.
   
   // initialize the results:

   sparc_cfi *cfi = (sparc_cfi *)icfi;

   if (rel) {
      rel->success = true; // assume success (for now)
      rel->result = getRaw(); // assume relocated insn is same as old insn (for now)
   }

   register const unsigned hi2bits = getHi2Bits();
   switch (hi2bits) {
      case 0x3:
         getInformation11(ru, mu, df, cfi, rel);
         break;
      case 0x2:
         getInformation10(ru, mu, df, cfi, rel);
         break;
      case 0x1:
         getInformation01(ru, mu, df, cfi, rel);
         break;
      case 0x0:
         getInformation00(ru, mu, df, cfi, rel);
         break;
      default:
         assert(false);
   }
   
   if (ru) {
      // Let's clean up the register usage:
      // -- g0 is never written to.

      static sparc_reg_set g0set(sparc_reg::g0);
      *(sparc_reg_set *)(ru->definitelyWritten) -= g0set;
      *(sparc_reg_set *)(ru->maybeWritten)      -= g0set;
   }
   
   if (cfi) {
     //calculate arch-independent CFI designations based on low-level analysis
     //On sparc, we already fill in all the correct fields, except isBranch, which we take care of
     //here.
     if (cfi->fields.controlFlowInstruction) {
       if( cfi->sparc_fields.isBicc || cfi->sparc_fields.isBPcc || cfi->sparc_fields.isBPr ||
	   cfi->sparc_fields.isFBfcc || cfi->sparc_fields.isFBPfcc) {
	 cfi->fields.isBranch = true;
       }
     }
   }
}
bool sparc_instr::isCall() const {
   // There are two kinds of calls: call and jmpl <address>, %o7

   const unsigned hi2bits = getHi2Bits();
   if (hi2bits == 0x1)
      return true;
   else if (hi2bits == 0x2) {
      const unsigned op3 = getOp3();
      if (op3 == 0x38) { // jmpl
        const sparc_reg rd = getRdAs1IntReg();
        if (rd == sparc_reg::o7)
           return true;
      }
   }

   return false;
}

bool sparc_instr::isUnanalyzableCall() const {
   const unsigned hi2bits = getHi2Bits();

   if (hi2bits == 0x2) {
      const unsigned op3 = getOp3();
      if (op3 == 0x38) { // jmpl
        const sparc_reg rd = getRdAs1IntReg();
        if (rd == sparc_reg::o7)
           return true;
      }
   }

   return false;
}

// True iff jmpl %reg + offs, %o7. Fills in addrReg and addrOffs
bool sparc_instr::isCallRegPlusOffset(sparc_reg *addrReg, int *addrOffs) const
{
    const unsigned hi2bits = getHi2Bits();

    if (hi2bits == 0x2) {
	const unsigned op3 = getOp3();
	if (op3 == 0x38 && geti()) { // jmpl reg + offs
	    const sparc_reg rd = getRdAs1IntReg();
	    if (rd == sparc_reg::o7) {
		*addrReg = getRs1As1IntReg();
		*addrOffs = getSimm13();
		return true;
	    }
	}
    }

    return false;
}

// True iff jmpl %reg1 + %reg2, %o7. Fills in rs1 and rs2
bool sparc_instr::isCallRegPlusReg(sparc_reg *rs1, sparc_reg *rs2) const
{
    const unsigned hi2bits = getHi2Bits();

    if (hi2bits == 0x2) {
	const unsigned op3 = getOp3();
	if (op3 == 0x38 && !geti()) { // jmpl rs1+rs2
	    const sparc_reg rd = getRdAs1IntReg();
	    if (rd == sparc_reg::o7) {
		*rs1 = getRs1As1IntReg();
		*rs2 = getRs2As1IntReg();
		return true;
	    }
	}
    }

    return false;
}

bool sparc_instr::isCallInstr(kaddrdiff_t &displacement) const {
   const unsigned hi2bits = getHi2Bits();
   if (hi2bits == 0x1) {
      displacement = getBitsSignExtend(0, 29) * 4;
      return true;
   }
   else
      return false;
}

bool sparc_instr::
isCallToFixedAddress(kptr_t insnAddr, kptr_t &calleeAddr) const {
   kaddrdiff_t offset = 0;
   if (isCallInstr(offset)) {
      if (offset == 0) {
         dout << "WARNING: isCallToFixedAddress: offset = 0, insnAddr = "
	      << addr2hex(insnAddr) << endl;
      }
      calleeAddr = insnAddr + offset;
      return true;
   }
   else
      return false;
}

bool sparc_instr::
isBranchToFixedAddress(kptr_t insnAddr, kptr_t &brancheeAddr) const {
   sparc_cfi cfi;
   getControlFlowInfo((controlFlowInfo *)&cfi);

   if (cfi.fields.controlFlowInstruction &&
       (cfi.sparc_fields.isBicc || cfi.sparc_fields.isBPcc || cfi.sparc_fields.isBPr ||
        cfi.sparc_fields.isFBfcc || cfi.sparc_fields.isFBPfcc)) {
      // it's a conditional branch
      assert(cfi.destination == controlFlowInfo::pcRelative);
      brancheeAddr = insnAddr + cfi.offset_nbytes;
      return true;
   }
   else
      return false;
}

bool sparc_instr::isRestore() const {
   unsigned hi2bits = getHi2Bits();
   if (hi2bits != 0x2)
      return false;

   unsigned op3 = getOp3();
   if (op3 != 0x3d)
      return false;

   return true;
}

bool sparc_instr::isRestoreInstr(sparc_reg &rd) const {
   if (!isRestore())
      return false;

   rd = getRdAs1IntReg();
   return true;
}

bool sparc_instr::isRestoreUsingSimm13Instr(sparc_reg &rs1,
                                            int &simm13,
                                            sparc_reg &rd) const {
   if (!isRestore())
      return false;
   
   if (!geti())
      return false;
   
   rs1 = getRs1As1IntReg();
   simm13 = getSimm13();
   rd = getRdAs1IntReg();

   return true;
}

bool sparc_instr::isRestoreUsing2RegInstr(sparc_reg &rs1,
                                          sparc_reg &rs2,
                                          sparc_reg &rd) const {
   if (!isRestore())
      return false;
   
   if (geti())
      return false;
   
   rs1 = getRs1As1IntReg();
   rs2 = getRs2As1IntReg();
   rd = getRdAs1IntReg();

   return true;
}



bool sparc_instr::isV9ReturnInstr() const {
   if (getHi2Bits() != 0x2) return false;
   if (getOp3() != 0x39) return false;
   return true;
}

bool sparc_instr::isSPSaveWithFixedOffset(int &offset) const {
   if (getHi2Bits() != 0x2) return false;
   if (getOp3() != 0x3c) return false;
   if (getRdAs1IntReg() != sparc_reg::sp) return false;
   if (getRs1As1IntReg() != sparc_reg::sp) return false;
   if (!geti()) return false;

   offset = getSimm13();
   return true;
}

bool sparc_instr::isSPSave() const {
   // maybe, maybe not a fixed offset
   return (getHi2Bits() == 0x2 &&
           getOp3() == 0x3c &&
           getRdAs1IntReg() == sparc_reg::sp &&
           getRs1As1IntReg() == sparc_reg::sp);
}

bool sparc_instr::isSave() const {
   // maybe a fixed offset, maybe not.
   // maybe uses %sp as usual, maybe not.
   return (getHi2Bits() == 0x2 &&
           getOp3() == 0x3c);
}

bool sparc_instr::isNop() const {
   return (raw == 0x01000000);
}

bool sparc_instr::isLoad() const {
   unsigned hi2bits = getHi2Bits();
   if (hi2bits != 0x3)
      return false;

   unsigned op3 = getOp3();
   switch (op3) {
   case 0x9: case 0xa: case 0x8: case 0x1: case 0x2: case 0x0: case 0xb: case 0x3:
      return true;
   }

   return false;
}

bool sparc_instr::isLoadRegOffsetToDestReg(const sparc_reg &rd,
					   sparc_reg &rs1, // filled in
					   int &simm13 // filled in
					   ) 
{
   if (!isLoad())
     return false;
   if (getRdAs1IntReg() != rd)
     return false;
   if (geti() != 1)
      return false;
   rs1 = getRs1As1IntReg();
   simm13 = getSimm13();
   return true;
}

// Test if the instruction is ld [rs+offs], rd
bool sparc_instr::isLoadRegOffset(sparc_reg &rd, // filled in
				  sparc_reg &rs1, // filled in
				  int &simm13 // filled in
                                  ) 
{
   if (!isLoad())
     return false;
   if (geti() != 1)
      return false;
   rd = getRdAs1IntReg();
   rs1 = getRs1As1IntReg();
   simm13 = getSimm13();
   return true;
}

bool sparc_instr::isLoadDualRegToDestReg(const sparc_reg &rd,
                                         sparc_reg &rs1, // filled in
                                         sparc_reg &rs2 // filled in
                                         ) const {
   if (!isLoad())
     return false;

   if (getRdAs1IntReg() != rd)
     return false;
   if (geti() != 0)
      return false;
   
   rs1 = getRs1As1IntReg();
   rs2 = getRs2As1IntReg();

   return true;
}


bool sparc_instr::isSetHi(sparc_reg &rd) const {
   if (getHi2Bits() != 0x0)
      return false;
   if (getRawBits(22, 24) != 0x4)
      return false;

   rd = getRdAs1IntReg();

   return true;
}

bool sparc_instr::isSetHiToDestReg(const sparc_reg &r, uint32_t &val) const {
   // "r" must match; "val" is filled in for you
   // If true, sets the high 22 bits of 'val' while leaving the low
   // 10 bits alone.

   if (getHi2Bits() != 0x0)
      return false;
   if (getRawBits(22, 24) != 0x4)
      return false;

   const sparc_reg rd = getRdAs1IntReg();
   if (r != rd)
      return false;

   uint32_t const22 = getRawBits0(21);
   val &= 0x3ff; // keep only the low 10 bits
   val |= (const22 << 10); // fill in the high bits

   return true;
}

bool sparc_instr::isOR2RegToDestReg(const sparc_reg &rd, // must match
				    sparc_reg &rs1, // returned for you
				    sparc_reg &rs2  // returned for you
                                   ) const
{
   if (getHi2Bits() != 0x2)
      return false;
   if (getOp3() != 0x02) // or (no cc)
      return false;
   if (geti())
      return false;

   if (getRdAs1IntReg() != rd)
      return false;
   rs1 = getRs1As1IntReg();
   rs2 = getRs2As1IntReg();

   return true;
}

bool sparc_instr::isORImmToDestReg(const sparc_reg &rd, sparc_reg &rs1,
				   uint32_t &imm) const {
   if (getHi2Bits() != 0x2)
      return false;
   if (getOp3() != 0x02) // or (no cc)
      return false;
   if (!geti())
      return false;

   if (getRdAs1IntReg() != rd)
      return false;
   rs1 = getRs1As1IntReg();

   imm |= getUimm13();
   return true;
}

bool sparc_instr::isORImm(sparc_reg &rs1,
                          uint32_t &imm,
                          sparc_reg &rd) const {
   if (getHi2Bits() != 0x2)
      return false;
   if (getOp3() != 0x02) // or (no cc)
      return false;
   if (!geti())
      return false;

   rd = getRdAs1IntReg();
   rs1 = getRs1As1IntReg();

   imm |= getUimm13();
   return true;
}

bool sparc_instr::isBSetLo(const sparc_reg &r, uint32_t &val) const {
   // or %R1, const, %R1, where <const> is no more than 10 bits.
   if (getHi2Bits() != 0x2)
      return false;
   if (getOp3() != 0x02)  // or (no cc)
      return false;
   if (!geti())
      return false;

   const sparc_reg rs1 = getRs1As1IntReg();
   const sparc_reg rd = getRdAs1IntReg();
   if (rs1 != r)
      return false;
   if (rd != r)
      return false;

   // Now all that's left to check is whether the immediate uses no more than 10 bits.
   unsigned uimm13 = getUimm13();
   if (uimm13 != (uimm13 & 0x3ff))
      return false;

   val |= uimm13;

   return true;
}

bool sparc_instr::isCmp(sparc_reg rs1, int &val) const {
   if (getHi2Bits() != 0x2) return false;
   if (getRdAs1IntReg() != sparc_reg::g0) return false;
   if (getOp3() != 0x14) return false;
   if (getRs1As1IntReg() != rs1) return false;
   if (geti() != 1) return false;

   val = getSimm13();
   return true;
}

bool sparc_instr::isCmpWithValue(sparc_reg rs1, int val) const {
   if (getHi2Bits() != 0x2) return false;
   if (getRdAs1IntReg() != sparc_reg::g0) return false;
   if (getOp3() != 0x14) return false;
   if (getRs1As1IntReg() != rs1) return false;
   if (geti() != 1) return false;
   if (getSimm13() != val) return false;
   
   return true;
}

bool sparc_instr::isAndnImmToDestReg(sparc_reg rd, sparc_reg &rs1,
                                     int &amt) const {
   if (getHi2Bits() != 0x2) return false;
   if (getRdAs1IntReg() != rd) return false;
   if (getOp3() != 0x05) return false;
   if (geti() != 1) return false;
   
   rs1 = getRs1As1IntReg();
   amt = getSimm13();
   return true;
}

bool sparc_instr::isBiccCondition(unsigned cond, bool annul) const {
   if (getHi2Bits() != 0x0) return false;
   if (getRawBit(29) != annul) return false;
   if (getIntCondCode_25_28() != cond) return false;
   if (getRawBits(22, 24) != 0x2) return false;
   
   return true;
}

bool sparc_instr::isBicc(unsigned &cond) const {
   if (getHi2Bits() != 0x0) return false;
   // we don't care about the annul bit
   if (getRawBits(22, 24) != 0x2) return false;

   cond = getIntCondCode_25_28();
   return true;
}

bool sparc_instr::isBPcc(unsigned &cond, bool xcc) const {
   if (getHi2Bits() != 0x0) return false;
   // don't care about annul bit
   if (getRawBits(22, 24) != 0x1) return false;

   const bool cc0 = getRawBit(20);
   const bool cc1 = getRawBit(21);
   bool actual_xcc;
   if (!cc1 && !cc0)
      actual_xcc = false;
   else if (cc1 && !cc0)
      actual_xcc = true;
   else
      assert(false);
   if (xcc != actual_xcc) return false;

   // we don't care about the predict bit
   cond = getIntCondCode_25_28();
   return true;
}



bool sparc_instr::isMove(const sparc_reg &rd, // must match
                         sparc_reg &rs // returned for you
                         ) const {
   // "or %g0, rs, rd"
   // or
   // "or rs, %g0, rd"
   // or
   // "or rs, 0, rd"

   if (getHi2Bits() != 0x2) return false;
   if (getRdAs1IntReg() != rd) return false;
   if (getOp3() != 0x02) return false;

   if (geti() == 0) {
      // or rs1, rs2, rd
      if (getRs1As1IntReg() == sparc_reg::g0) {
         // or g0, rs2, rd --> match
         rs = getRs2As1IntReg();
         return true;
      }
      else if (getRs2As1IntReg() == sparc_reg::g0) {
         // or rs1, g0, rd --> match
         rs = getRs1As1IntReg();
         return true;
      }
   }
   else {
      // or rs1, simm13, rd
      if (getSimm13() == 0) {
         // or rs1, 0, rd --> match
         rs = getRs1As1IntReg();
         return true;
      }
   }

   return false;
}

bool sparc_instr::isShiftLeftLogicalToDestReg(const sparc_reg &rd, // must match
					      sparc_reg &rs1, // returned
					      unsigned &shiftamt // returned
					      ) const {
   if (getHi2Bits() != 0x2) return false;
   if (getRdAs1IntReg() != rd) return false;
   if (getOp3() != 0x25) return false;
   if (geti() == 0) return false;

   if (getRawBit(12))
      // x==1 --> shift count in bits 0 thru 5
      shiftamt = getRawBits(0, 5);
   else
      // x==0 --> shift count in bits 0 thru 4
      shiftamt = getRawBits(0, 4);

   rs1 = getRs1As1IntReg();
   return true;
}

bool sparc_instr::isShiftRightLogicalToDestReg(sparc_reg rd, // must match
                                               sparc_reg &rs1,
                                               unsigned &shiftamt) const {
   if (getHi2Bits() != 0x2) return false;
   if (getRdAs1IntReg() != rd) return false;
   if (getOp3() != 0x26) return false;
   if (geti() != 1) return false;
   
   if (getRawBit(12))
      shiftamt = getRawBits(0, 5);
   else
      shiftamt = getRawBits(0, 4);
   
   rs1 = getRs1As1IntReg();
   return true;
}

bool sparc_instr::isAdd2RegToDestReg(const sparc_reg &rd, // must match
				     sparc_reg &rs1, // filled in
				     sparc_reg &rs2 // filled in
				     ) const
{
  if (getHi2Bits() != 0x2)
     return false;
  
  if (getOp3() != 0x0)
     return false;

  if (geti() != 0)
     return false;

  if (getRdAs1IntReg() != rd)
    return false;
  

  rs1 = getRs1As1IntReg();
  rs2 = getRs2As1IntReg();
  return true;
}

bool sparc_instr::isAddImmToDestReg(const sparc_reg &rd, // must match
                                    sparc_reg &rs1, // filled in
                                    unsigned &val) const {
   // won't return true if cc or carry bits are set; only op3 of 0x0
   if (getHi2Bits() != 0x2)
      return false;
  
   if (getOp3() != 0x0)
      return false;

   if (geti() != 1)
      return false;

   if (getRdAs1IntReg() != rd)
      return false;
  
   rs1 = getRs1As1IntReg();

   int delta = getSimm13();
   val += delta;

   return true;
}

bool sparc_instr::isAddImm(sparc_reg &rs1, // filled in
                           unsigned &val, // we ADD to this!!!!!!!!!!!
                           sparc_reg &rd // filled in
                           ) const {
   // won't return true if cc or carry bits are set; only op3 of 0x0
   if (getHi2Bits() != 0x2)
      return false;
  
   if (getOp3() != 0x0)
      return false;

   if (geti() != 1)
      return false;

   rd = getRdAs1IntReg();
   rs1 = getRs1As1IntReg();

   int delta = getSimm13();
   val += delta;

   return true;
}

bool sparc_instr::isAddImm(int &val) const {
   // won't return true if cc or carry bits are set; only op3 of 0x0
   if (getHi2Bits() != 0x2)
      return false;
  
   if (getOp3() != 0x0)
      return false;

   if (geti() != 1)
      return false;

   val = getSimm13();

   return true;
}


bool sparc_instr::isSub2RegToDestReg(sparc_reg rd, // must match
                                     sparc_reg &rs1, sparc_reg &rs2) {
   if (getHi2Bits() != 0x2) return false;
   if (getRdAs1IntReg() != rd) return false;
   if (getOp3() != 0x04) return false;
   if (geti() != 0) return false;
   
   rs1 = getRs1As1IntReg();
   rs2 = getRs2As1IntReg();
   return true;
}

bool sparc_instr::isRd() const {
   return getRawBits(30, 31) == 0x2 && getOp3() == 0x28;
}

bool sparc_instr::isRdPr() const {
   return getRawBits(30, 31) == 0x2 && getOp3() == 0x2a;
}
bool sparc_instr::isWrPr() const {
   return getRawBits(30, 31) == 0x2 && getOp3() == 0x32;
}
bool sparc_instr::isDone() const {
   return getRawBits(30, 31) == 0x2 && getOp3() == 0x3e && getRawBits(25, 29) == 0;
}
bool sparc_instr::isRetry() const {
   return getRawBits(30, 31) == 0x2 && getOp3() == 0x3e && getRawBits(25, 29) == 1;
}

void sparc_instr::getDisassembly(disassemblyFlags &df) const {
   getInformation(NULL, NULL, &df, NULL, NULL);
}

pdstring sparc_instr::disassemble(kptr_t insnaddr) const {
   disassemblyFlags fl;
   fl.pc = insnaddr;
   
   getInformation(NULL, NULL, &fl, NULL, NULL);

   return fl.result;
}

void sparc_instr::getRelocationInfo(relocateInfo &ri) const {
   getInformation(NULL, NULL, NULL, NULL, &ri);
}

/* ******************************************************************** */

bool sparc_instr::existsTrueDependence(const registerUsage &ru,
                                       const registerUsage &nextRu,
                                       const memUsage &mu,
                                       const memUsage &nextMu) {
   if (mu.memWritten && nextMu.memRead)
      return true; // for now, we don't bother with addresses

   const sparc_reg_set instr1Writes = *(sparc_reg_set*)(ru.definitelyWritten) + 
                                      *(sparc_reg_set*)(ru.maybeWritten);
   const sparc_reg_set instr2Reads = *(sparc_reg_set*)(nextRu.definitelyUsedBeforeWritten) +
                                     *(sparc_reg_set*)(nextRu.maybeUsedBeforeWritten);
   const sparc_reg_set intersection = instr1Writes & instr2Reads;
 
   if (intersection != sparc_reg_set::emptyset)
      // instr1 (this) writes to a reg which is used by instr2 (nextInstr)
      return true;

   return false;
}

bool sparc_instr::existsAntiDependence(const registerUsage &ru,
                                       const registerUsage &nextRu,
                                       const memUsage &mu,
                                       const memUsage &nextMu) {
   if (mu.memRead && nextMu.memWritten)
      return true; // for now, we don't bother discriminating the addrs

   const sparc_reg_set instr1Reads = *(sparc_reg_set*)(ru.definitelyUsedBeforeWritten) +
                                     *(sparc_reg_set*)(ru.maybeUsedBeforeWritten);
   const sparc_reg_set instr2Writes = *(sparc_reg_set*)(nextRu.definitelyWritten) +
                                      *(sparc_reg_set*)(nextRu.maybeWritten);
   const sparc_reg_set intersection = instr1Reads & instr2Writes;

   if (intersection != sparc_reg_set::emptyset)
      return true;

   return false;
}

bool sparc_instr::existsOutputDependence(const registerUsage &ru,
                                         const registerUsage &nextRu,
                                         const memUsage &mu,
                                         const memUsage &nextMu) {
   if (mu.memWritten && nextMu.memWritten)
      return true; // for now, we don't bother discriminating the addrs

   const sparc_reg_set instr1Writes = *(sparc_reg_set*)(ru.definitelyWritten) +
                                      *(sparc_reg_set*)(ru.maybeWritten);
   const sparc_reg_set instr2Writes = *(sparc_reg_set*)(nextRu.definitelyWritten) +
                                      *(sparc_reg_set*)(nextRu.maybeWritten);
   const sparc_reg_set intersection = instr1Writes & instr2Writes;
   if (intersection != sparc_reg_set::emptyset)
      return true;

   return false;
}

sparc_instr::delaySlotInfo
sparc_instr::getDelaySlotInfo() const {
   sparc_cfi cfi;
   getControlFlowInfo((controlFlowInfo *)&cfi);

   return cfi.delaySlot;
}


sparc_instr::dependence
sparc_instr::calcDependence(const instr_t *nextInstr) const {
   // assuming 'this' is executed, followed by 'nextInstr', calculate the
   // dependency.  If there is more than one dependence, the stronger one
   // is returned.  I.e, if a true dependence exists, then return true;
   // else if an anti-dependence exists then return anti;
   // else if an output-dependence exists then return output;
   // else return none;

   sparc_ru ru, nextRu;
   memUsage mu, nextMu;

   getInformation((registerUsage *)&ru, &mu, NULL, NULL, NULL);
   nextInstr->getInformation((registerUsage *)&nextRu, &nextMu, 
			     NULL, NULL, NULL);

   if (existsTrueDependence(ru, nextRu, mu, nextMu))
      return deptrue;
   else if (existsAntiDependence(ru, nextRu, mu, nextMu))
      return depanti;
   else if (existsOutputDependence(ru, nextRu, mu, nextMu))
      return depoutput;
   else
      return depnone;
}

instr_t *sparc_instr::relocate(kptr_t old_insnaddr,
			       kptr_t new_insnaddr) const {
   relocateInfo ri;
   ri.old_insnaddr = old_insnaddr;
   ri.new_insnaddr = new_insnaddr;
   getRelocationInfo(ri);
   return new sparc_instr(ri.result);
}

// We need to transfer src in the previous register window to ours
// A couple of cases are obvious, but the others require some code to be
// generated
sparc_reg sparc_instr::recoverFromPreviousFrame(sparc_reg src, 
						sparc_reg scratch, 
						unsigned stackBias,
						pdvector<instr_t*> *seq)
{
    sparc_reg rv(sparc_reg::g0);
    unsigned num;
    if (src.is_oReg(num)) {
	rv = sparc_reg(sparc_reg::in, num);
    }
    else if (src.is_gReg(num)) {
	rv = src;
    }
    else if (src.is_iReg(num) || src.is_lReg(num)) {
	// Make sure that the previous frame is flushed to memory
	seq->push_back(new sparc_instr(sparc_instr::flushw));

	// Load the register from the previous stack frame
	int regOffs = stackBias +
	    sizeof(kptr_t) * (src.getIntNum() - sparc_reg::l0.getIntNum());
	if (sizeof(kptr_t) == sizeof(uint32_t)) {
	    seq->push_back(new sparc_instr(sparc_instr::load, 
				           sparc_instr::ldUnsignedWord,
				           sparc_reg::fp, regOffs,
				           scratch));
	}
	else if (sizeof(kptr_t) == sizeof(uint64_t)) {
	    seq->push_back(new sparc_instr(sparc_instr::load, 
				           sparc_instr::ldExtendedWord,
				           sparc_reg::fp, regOffs,
				           scratch));
	}
	else {
	    assert(false);
	}
	rv = scratch;
    }
    return rv;
}

// Generate a sequence of instructions equivalent to this one, but
// that writes to rv_reg instead of this instruction's dest. If
// hasExtraSave is true -- the source registers need to be recovered
// from the previous stack frame
void sparc_instr::changeDestRegister(bool hasExtraSave, sparc_reg rv_reg, 
				     sparc_reg scratch_reg, unsigned stackBias,
				     pdvector<instr_t*> *equiv) const
{
    // Register layout of a SPARC instruction:
    // bits 0-4: rs2 (if has rs2)
    // bits 14-18: rs1 (if has rs1)
    // bits 25-29: rd (if has rd)
    uint32_t rv_raw = raw;
    const uint32_t fivebits = 31; // five bits on

    if (hasExtraSave) {
	// We need to recover source registers of dsInsn from
	// the previous stack frame

	// rs1 and rs2 may be bogus if this instruction does not use them,
	// but we later check with getRegisterUsage to see if they exist
	sparc_reg rs1 = sparc_reg(sparc_reg::rawIntReg, getRs1Bits(raw));
	sparc_reg rs2 = sparc_reg(sparc_reg::rawIntReg, getRs2Bits(raw));

	sparc_ru ru;
	getRegisterUsage(&ru);
	if (ru.definitelyUsedBeforeWritten->exists(rs1)) {
	    rs1 = recoverFromPreviousFrame(rs1, rv_reg, stackBias, equiv);
	    // Replace old rs1 with the new one
	    rv_raw = (rv_raw & ~(fivebits << 14)) | (rs1.getIntNum() << 14);
	}
	if (ru.definitelyUsedBeforeWritten->exists(rs2)) {
	    rs2 = recoverFromPreviousFrame(rs2, scratch_reg, stackBias, equiv);
	    // Replace old rs2 with the new one
	    rv_raw = (rv_raw & ~fivebits) | rs2.getIntNum();
	}
    }
    rv_raw = (rv_raw & ~(fivebits << 25)) | (rv_reg.getIntNum() << 25);
    equiv->push_back(new sparc_instr(rv_raw));
}

bool sparc_instr::inRangeOfBicc(kptr_t from, kptr_t to) {
   // static
   return inRangeOf(from, to, 24);
}

bool sparc_instr::inRangeOfFBfcc(kptr_t from, kptr_t to) {
   // static 
   return inRangeOf(from, to, 24);
}

bool sparc_instr::inRangeOfBPr(kptr_t from, kptr_t to) {
   // static
   return inRangeOf(from, to, 18);
}

bool sparc_instr::inRangeOfBPcc(kptr_t from, kptr_t to) {
   // static
   return inRangeOf(from, to, 21);
}

bool sparc_instr::inRangeOfFBPfcc(kptr_t from, kptr_t to) {
   // static
   return inRangeOf(from, to, 21);
}

bool sparc_instr::inRangeOfCall(kptr_t from, kptr_t to) {
   // static
   return inRangeOf(from, to, 32);
}

void sparc_instr::getRangeOfBicc(kptr_t from, kptr_t &min_reachable_result,
                                 kptr_t &max_reachable_result) {
   // static
   getRangeOf(from, 24, min_reachable_result, max_reachable_result);
}

void sparc_instr::getRangeOfFBfcc(kptr_t from, kptr_t &min_reachable_result,
                                  kptr_t &max_reachable_result) {
   // static
   getRangeOf(from, 24, min_reachable_result, max_reachable_result);
}

void sparc_instr::getRangeOfBPr(kptr_t from, kptr_t &min_reachable_result,
                                kptr_t &max_reachable_result) {
   // static
   getRangeOf(from, 18, min_reachable_result, max_reachable_result);
}

void sparc_instr::getRangeOfBPcc(kptr_t from, kptr_t &min_reachable_result,
                                 kptr_t &max_reachable_result) {
   // static
   getRangeOf(from, 21, min_reachable_result, max_reachable_result);
}

void sparc_instr::getRangeOfFBPfcc(kptr_t from, kptr_t &min_reachable_result,
                                   kptr_t &max_reachable_result) {
   // static 
   getRangeOf(from, 21, min_reachable_result, max_reachable_result);
}

void sparc_instr::getRangeOfCall(kptr_t from, kptr_t &min_reachable_result,
                                 kptr_t &max_reachable_result) {
   // static
   getRangeOf(from, 32, min_reachable_result, max_reachable_result);
}

// ----------------------------------------------------------------------

void sparc_instr::changeBranchOffsetAndAnnulBicc(const sparc_cfi &old_cfi,
                                                 int new_insnbytes_offset,
                                                 bool new_annul) {
   assert(old_cfi.sparc_fields.isBicc);
      // since the FBfcc() routine, below, calls us.
   
   assert(new_insnbytes_offset % 4 == 0);
   const int new_numinsns_offset = new_insnbytes_offset / 4;

   replaceBits(0, simmediate<22>(new_numinsns_offset));
   replaceBits(29, uimmediate<1>(new_annul));

   sparc_cfi cfi;
   getControlFlowInfo((controlFlowInfo *)&cfi);
   assert(cfi.fields.controlFlowInstruction);
   assert(cfi.sparc_fields.isBicc);
   assert(cfi.offset_nbytes == new_insnbytes_offset);
   assert(cfi.sparc_fields.annul == new_annul);
}

void sparc_instr::changeBranchOffsetAndAnnulBPcc(const sparc_cfi &old_cfi,
                                                 int new_insnbytes_offset,
                                                 bool new_annul) {
   assert(old_cfi.sparc_fields.isBPcc || old_cfi.sparc_fields.isFBPfcc);
      // since called by the FBPfcc routine, below
   
   assert(new_insnbytes_offset % 4 == 0);
   const int new_numinsns_offset = new_insnbytes_offset / 4;

   replaceBits(0, simmediate<19>(new_numinsns_offset));
   replaceBits(29, uimmediate<1>(new_annul));
   
   sparc_cfi cfi;
   getControlFlowInfo((controlFlowInfo *)&cfi);
   assert(cfi.fields.controlFlowInstruction);
   assert(cfi.sparc_fields.isBPcc);
   assert(cfi.sparc_fields.annul == new_annul);
   assert(cfi.offset_nbytes == new_insnbytes_offset);
}

void sparc_instr::changeBranchOffsetAndAnnulBPr(const sparc_cfi &old_cfi,
                                                int new_insnbytes_offset,
                                                bool new_annul) {
   // A little trickier since the offset (16 bits) is in ([21,20], [13,0])
   assert(old_cfi.sparc_fields.isBPr);

   assert(new_insnbytes_offset % 4 == 0);
   const int new_numinsns_offset = new_insnbytes_offset / 4;

   const uimmediate<2> new_disp16hi2bits((new_numinsns_offset >> 14) & 0x3);
   const uimmediate<14> new_disp16lobits(new_numinsns_offset & 0x3fff);

   replaceBits(29, uimmediate<1>(new_annul));
   replaceBits(20, new_disp16hi2bits);
   replaceBits(0, new_disp16lobits);
   
   sparc_cfi cfi;
   getControlFlowInfo((controlFlowInfo *)&cfi);
   assert(cfi.fields.controlFlowInstruction);
   assert(cfi.sparc_fields.isBPr);
   assert(cfi.sparc_fields.annul == new_annul);
   assert(cfi.offset_nbytes == new_insnbytes_offset);
}

void sparc_instr::changeBranchOffsetAndAnnulFBfcc(const sparc_cfi &old_cfi,
                                                  int new_insnbytes_offset,
                                                  bool new_annul) {
   assert(old_cfi.sparc_fields.isFBfcc);
   
   assert(new_insnbytes_offset % 4 == 0);
   const int new_numinsns_offset = new_insnbytes_offset / 4;

   replaceBits(29, uimmediate<1>(new_annul));
   replaceBits(0, simmediate<22>(new_numinsns_offset));

   sparc_cfi cfi;
   getControlFlowInfo((controlFlowInfo *)&cfi);
   assert(cfi.fields.controlFlowInstruction);
   assert(cfi.sparc_fields.isFBfcc);
   assert(cfi.sparc_fields.annul = new_annul);
   assert(cfi.offset_nbytes == new_insnbytes_offset);
}

void sparc_instr::changeBranchOffsetAndAnnulFBPfcc(const sparc_cfi &old_cfi,
                                                   int new_insnbytes_offset,
                                                   bool new_annul) {
   assert(old_cfi.sparc_fields.isFBPfcc);

   assert(new_insnbytes_offset % 4 == 0);
   const int new_numinsns_offset = new_insnbytes_offset / 4;

   replaceBits(29, uimmediate<1>(new_annul));
   replaceBits(0, simmediate<19>(new_numinsns_offset));

   sparc_cfi cfi;
   getControlFlowInfo((controlFlowInfo *)&cfi);
   assert(cfi.fields.controlFlowInstruction);
   assert(cfi.sparc_fields.isFBPfcc);
   assert(cfi.sparc_fields.annul = new_annul);
   assert(cfi.offset_nbytes == new_insnbytes_offset);
}

void sparc_instr::changeBranchOffsetAndAnnulBit(const sparc_cfi &cfi,
                                                int new_insnbytes_offset,
                                                bool new_annul) {
   assert(cfi.fields.controlFlowInstruction);
   
   // It was a branch (perhaps conditional)
   assert(cfi.fields.isBranch);

   if (cfi.sparc_fields.isBicc)
      changeBranchOffsetAndAnnulBicc(cfi, new_insnbytes_offset, new_annul);
   else if (cfi.sparc_fields.isBPcc)
      changeBranchOffsetAndAnnulBPcc(cfi, new_insnbytes_offset, new_annul);
   else if (cfi.sparc_fields.isBPr)
      changeBranchOffsetAndAnnulBPr(cfi, new_insnbytes_offset, new_annul);
   else if (cfi.sparc_fields.isFBfcc)
      changeBranchOffsetAndAnnulFBfcc(cfi, new_insnbytes_offset, new_annul);
   else if (cfi.sparc_fields.isFBPfcc)
      changeBranchOffsetAndAnnulFBPfcc(cfi, new_insnbytes_offset, new_annul);
   else
      assert(false);
}

void sparc_instr::changeBranchOffset(int new_insnbytes_offset) {
   // must be a conditional branch instruction.  Changes the offset.
   sparc_cfi cfi;
   getControlFlowInfo((controlFlowInfo *)&cfi);

   changeBranchOffsetAndAnnulBit(cfi, new_insnbytes_offset, cfi.sparc_fields.annul);
}

void sparc_instr::changeBranchOffsetAndAnnulBit(int new_insnbytes_offset,
                                                bool new_annul) {
   sparc_cfi cfi;
   getControlFlowInfo((controlFlowInfo *)&cfi);
   
   changeBranchOffsetAndAnnulBit(cfi,
                                 new_insnbytes_offset,
                                 new_annul);
}

void sparc_instr::reverseBranchPolarity(const sparc_cfi &cfi) {
   assert(cfi.fields.controlFlowInstruction);
   
   // It was a branch (perhaps conditional)
   assert(cfi.sparc_fields.isBicc || cfi.sparc_fields.isBPcc || cfi.sparc_fields.isBPr ||
          cfi.sparc_fields.isFBfcc || cfi.sparc_fields.isFBPfcc);

   if (cfi.sparc_fields.isBicc || cfi.sparc_fields.isBPcc)
       replaceBits(25, uimmediate<4>(reversedIntCondCodes[getRawBits(25, 28)]));
   else if (cfi.sparc_fields.isBPr)
      replaceBits(25, uimmediate<3>(reversedRegCondCodes[getRawBits(25, 27)]));
   else if (cfi.sparc_fields.isFBfcc || cfi.sparc_fields.isFBPfcc)
      replaceBits(25, uimmediate<4>(reversedFloatCondCodes[getRawBits(25, 28)]));
   else
      assert(false);
}

void sparc_instr::reverseBranchPolarity() {
   sparc_cfi cfi;
   getControlFlowInfo((controlFlowInfo *)&cfi);
   reverseBranchPolarity(cfi);
}

void sparc_instr::changeBranchPredictBitIfApplicable(const sparc_cfi &cfi,
                                                     bool newPredictValue) {
   // It was a branch (perhaps conditional)
   assert(cfi.sparc_fields.isBicc || cfi.sparc_fields.isBPcc || cfi.sparc_fields.isBPr ||
          cfi.sparc_fields.isFBfcc || cfi.sparc_fields.isFBPfcc);

   if (cfi.sparc_fields.isBicc || cfi.sparc_fields.isFBfcc)
      // not applicable -- no predict bits in these insns
      return;

   if (cfi.sparc_fields.isBPcc || cfi.sparc_fields.isBPr || cfi.sparc_fields.isFBPfcc)
      // All of these guys have the predict bit in bit 19
      replaceBits(19, uimmediate<1>(newPredictValue ? 1 : 0));
   else
      assert(false);
}

void sparc_instr::changeBranchPredictBitIfApplicable(bool newPredictValue) {
   sparc_cfi cfi;
   getControlFlowInfo((controlFlowInfo *)&cfi);
   changeBranchPredictBitIfApplicable(cfi, newPredictValue);
}



sparc_instr sparc_instr::
cfi2ConditionalMove(const sparc_cfi &cfi, // describes orig insn
                    sparc_reg srcReg, // if true, move this...
                    sparc_reg destReg // ...onto this
                    ) {
   // a static method
   const bool intCC = (cfi.sparc_fields.isBicc || cfi.sparc_fields.isBPcc);
   const bool floatCC = (cfi.sparc_fields.isFBfcc || cfi.sparc_fields.isFBPfcc);
   const bool intRegC = (cfi.sparc_fields.isBPr);
   assert(intCC || floatCC || intRegC);
                  
   if (intCC)
      return sparc_instr(sparc_instr::movcc,
                         cfi.condition,
                         cfi.xcc,
                         srcReg, // if true, move this...
                         destReg // ...onto this
                         );
   else if (floatCC)
      return sparc_instr(sparc_instr::movcc,
                         cfi.condition,
                         cfi.fcc_num,
                         srcReg, // if true, move this...
                         destReg // ...onto this
                         );
   else if (intRegC)
      return sparc_instr(sparc_instr::movr,
                         cfi.condition,
                         *(sparc_reg *)(cfi.conditionReg), // test this
                         srcReg, // if true, move this...
                         destReg // ...onto this
                         );
   else
      assert(false);
}

void sparc_instr::changeSimm13(int new_val) {
   assert(new_val >= simmediate<13>::getMinAllowableValue());
   assert(new_val <= simmediate<13>::getMaxAllowableValue());

   simmediate<13> val(new_val);
   
   raw >>= 13;
   rollin(raw, val);
}

void sparc_instr::gen32imm(insnVec_t *piv, uint32_t val, 
			   const sparc_reg &rd)
{
// This routine is fraught with potential bugs!
// Be aware of the following:
//    1) We used to think that if "val" fits in 13 bits then we could
//       use a single "or" instruction.  But beware: logical instructions
//       implicitly sign-extend an immediate argument, much like add or
//       subtract.  So we can only be sure that a single instruction works if
//       "val" fits in *12* bits.  Tricky bug!
//    2) So now...continuing on the same idea...if "val" fits in 12
//       bits, we can use a single instruction.  True enough.  But I once used
//       "bset" as the instruc.  That's buggy, because it leaves the high bits
//       alone, instead of setting them to zero, as we want.  So use "or %g0,
//       imm12, %destreg" instead.  Tricky bug!


   const unsigned twelveBitMask = 0xfff;
   if (val == (val & twelveBitMask)) {
      // "or %g0, imm12, %destreg". Don't use "bset" -- 
      // that'd be a bug (see above).
      instr_t *i = new sparc_instr(logic, logic_or, false, // no cc
				   rd, // dest
				   sparc_reg::g0,
				   val); // don't use %lo(val)
      piv->appendAnInsn(i);
   }
   // (The value 4096 can be handled via sub %g0, -4096, rd, but sethi
   // works just as well, so we don't bother with that special case.)
   else {
      // Doesn't all fit in 12 bits; needs a sethi, at least.
      instr_t *i = new sparc_instr(sparc_instr::sethi, sparc_instr::HiOf(), 
				   val, rd);
      piv->appendAnInsn(i);
   
      // bset only needed if the low 10 bits are nonzero
      const unsigned tenBitMask = 0x3ff;
      if ((val & tenBitMask) != 0) {
	 i = new sparc_instr(false, sparc_instr::bset,
			     sparc_instr::lo_of_32(val), rd);
         piv->appendAnInsn(i);
      }
   }
}

void sparc_instr::gen64imm(insnVec_t *piv, uint64_t val, 
			   const sparc_reg &rd, const sparc_reg &rtemp)
{
   // Optimizations to possibly implement:
   // 1) If all of the 1 bits are spread throughout at most 22
   //    consecutive bits, then a sethi followed by a shift left or right as
   //    appropriate will do the trick in just 2 instructions.

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
      return gen32imm(piv, low32bits_unsigned, rd);
   }

   assert(val != low32bits);

   // We're definitely going to need a distinct scratch reg:
   assert(rtemp != rd);
   
   // generate high bits
   gen32imm(piv, high32bits_unsigned, rtemp);
   instr_t *i = new sparc_instr(sparc_instr::shift,
				sparc_instr::leftLogical,
				true, // extended
				rtemp, // dest
				rtemp, 32);
   piv->appendAnInsn(i);
   
   // generate low bits
   gen32imm(piv, low32bits_unsigned, rd);
   i = new sparc_instr(sparc_instr::logic, sparc_instr::logic_or,
		       false, // no cc
		       rd, // dest
		       rd, rtemp);
   piv->appendAnInsn(i);
}

// Set the register to the given 32-bit address value
void sparc_instr::genImmAddr(insnVec_t *piv, uint32_t addr, 
			     const sparc_reg &rd)
{
   gen32imm(piv, addr, rd);
}

// Set the register to the given 64-bit address value
// Try to produce the shortest code, depending on the address value
void sparc_instr::genImmAddr(insnVec_t *piv, uint64_t addr, 
			     const sparc_reg &rd)
{
   const uint32_t high20bits = (addr >> 44);
   const uint32_t med32bits = ((addr >> 12) & 0xFFFFFFFFUL);
   const uint32_t lo12bits = (addr & 0x0FFF);

   const uint32_t hiword = (addr >> 32);
   const uint32_t loword = (addr & 0xFFFFFFFFUL);
	
   assert(high20bits == 0); // Addresses do not exceed 44 bits

   if (hiword == 0) {
      return gen32imm(piv, loword, rd);
   }

   gen32imm(piv, med32bits, rd);
   instr_t *i = new sparc_instr(sparc_instr::shift, sparc_instr::leftLogical,
				true, // extended
				rd, // dest
				rd, 12);
   piv->appendAnInsn(i);

   if (lo12bits != 0) {
      i = new sparc_instr(false, sparc_instr::bset, lo12bits, rd);
      piv->appendAnInsn(i);
   }
}

// Set the register to the given 32-bit address value
// Do not attempt to produce smaller code, since we may
// patch it with a different address later
void sparc_instr::genImmAddrGeneric(insnVec_t *piv, uint32_t addr, 
				    const sparc_reg &rd, bool setLow)
{
   uint32_t lo12bits = (addr & 0xFFFU); // low 12 bits, not 10 !
   uint32_t hi20bits = (addr & ~0xFFFU); // high 20 bits

   instr_t *i = new sparc_instr(sparc_instr::sethi, sparc_instr::HiOf(), 
				hi20bits, rd);
   piv->appendAnInsn(i);
   if (setLow) {
      i = new sparc_instr(false, sparc_instr::bset, 
			  lo12bits /* Not lo_of_32 ! */, rd);
      piv->appendAnInsn(i);
   }
}

// Set the register to the given 64-bit address value
// Do not attempt to produce smaller code, since we may
// patch it with a different address later
void sparc_instr::genImmAddrGeneric(insnVec_t *piv, uint64_t addr, 
				    const sparc_reg &rd, bool setLow)
{
    assert(addr < 0xFFFFFFFFULL);
    uint32_t addr32 = addr;
    genImmAddrGeneric(piv, addr32, rd, setLow);

// We do not need to call the 64-bit space anymore, so
// the following is commented out
// 
//     const uint32_t high20bits = (addr >> 44);
//     const uint32_t med32bits = ((addr >> 12) & 0xFFFFFFFFUL);
//     const uint32_t lo12bits = (addr & 0x0FFF);

//     assert(high20bits == 0); // Addresses do not exceed 44 bits

//     piv->appendAnInsn(sparc_instr(sparc_instr::sethi, sparc_instr::HiOf(), 
//  				 med32bits, rd));
//     piv->appendAnInsn(sparc_instr(false, sparc_instr::bset,
//  				 sparc_instr::lo_of_32(med32bits), rd));
//     piv->appendAnInsn(sparc_instr(sparc_instr::shift, sparc_instr::leftLogical,
//  				 true, // extended
//  				 rd, // dest
//  				 rd, 12));
//     if (setLow) {
//        piv->appendAnInsn(sparc_instr(false, sparc_instr::bset,
//  				    lo12bits, rd));
//     }
}

// Generate "genImmAddr(hi(to)) -> link; jmpl [link + lo(to)], link"
// Note: we require link != sparc_reg::g0, since we need a scratch 
// register for sure
void sparc_instr::genJumpAndLink(insnVec_t *piv, kptr_t to, 
				 sparc_reg &linkReg)
{
   kptr_t hi = (to & ~0xFFFULL);
   kptr_t lo12bits = (to & 0xFFFULL);
   
   assert(linkReg != sparc_reg::g0);
   genImmAddr(piv, hi, linkReg);
   instr_t *i = new sparc_instr(jumpandlink, linkReg, linkReg, lo12bits);
   piv->appendAnInsn(i);
}

// Generate either "call disp" or
// "genImmAddr(hi(to)) -> o7, jmpl o7+lo(to),o7" if the displacement
// is not in the range for the call instruction
void sparc_instr::genCallAndLink(insnVec_t *piv, kptr_t from, 
				 kptr_t to)
{
    assert(inRangeOfCall(from, to)); // all code is in the 32-bit space
    instr_t *i = new sparc_instr(callandlink, to, from);
    piv->appendAnInsn(i);
    
    // We do not need to call the 64-bit space anymore, so
    // the following is commented out
    // else {
    //     genJumpAndLink(piv, to, sparc_reg::o7);
    // }
}

// Same goal, but do not optimize produced code for size.
void sparc_instr::genCallAndLinkGeneric_unknownFromAddr(insnVec_t *piv, 
                                                        uint32_t to)
{
   instr_t *i = new sparc_instr(callandlink, to, 0); 
    // 0 --> dummy/unknown fromAddr
   piv->appendAnInsn(i);
}

// Same goal, but do not optimize produced code for size.
void sparc_instr::genCallAndLinkGeneric_unknownFromAddr(insnVec_t *piv, 
							uint64_t to) 
{
    assert(to < 0xFFFFFFFFULL); // code is now in the 32-bit space
    uint32_t to32 = to;
    genCallAndLinkGeneric_unknownFromAddr(piv, to32);

    // We do not need to call the 64-bit space anymore, so
    // the following is commented out
    // uint32_t hi = (to & ~0xFFFULL);
    // uint32_t lo12bits = (to & 0xFFFULL);
    // 
    // genImmAddrGeneric(piv, hi, sparc_reg::o7, false); 
    // piv->appendAnInsn(sparc_instr(jumpandlink, sparc_reg::o7, 
    //				     sparc_reg::o7, lo12bits));
}

void sparc_instr::
genCallAndLinkGeneric_unknownFromAndToAddr(insnVec_t *piv) {
   const kptr_t to = 0;
   genCallAndLinkGeneric_unknownFromAddr(piv, to);
}

