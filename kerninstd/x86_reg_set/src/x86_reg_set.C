// x86_reg_set.C

#include "bitUtils.h"
#include "x86_reg_set.h"
#include "util/h/popc.h"
#include "util/h/ffs.h"
#include "pdutil/h/rpcUtil.h"
#include <stdlib.h> // random()

static unsigned correctIntRegMask(unsigned in);

x86_reg_set::x86_reg_set(XDR *xdr) {
   if (!P_xdr_recv(xdr, int_bits))
      throw xdr_recv_fail();
   if (!P_xdr_recv(xdr, float_bits))
      throw xdr_recv_fail();
   if (!P_xdr_recv(xdr, misc_stuff))
      throw xdr_recv_fail();
}

bool x86_reg_set::send(XDR *xdr) const {
   return P_xdr_send(xdr, int_bits) &&
          P_xdr_send(xdr, float_bits) &&
          P_xdr_send(xdr, misc_stuff);
}

x86_reg_set::x86_reg_set(Random) : regset_t() {
   unsigned val = (unsigned)::random();
   float_bits = val;
   float_bits <<= 32;
     
   val = (unsigned)::random();
   float_bits |= val;
   int_bits = correctIntRegMask((unsigned)::random());
   misc_stuff = (unsigned)::random();
   ms.padding = 0;
}

bool x86_reg_set::inSet(const regset_t &theset, unsigned bit) {
   if (bit < 30) 
      return theset.existsIntReg(bit);
   else if (bit < 33) // misc regs
      switch (bit) {
      case 30:
	 return ((const x86_reg_set&)theset).existsEFLAGS();
      case 31:
	 return ((const x86_reg_set&)theset).existsPC();
      case 32:
         // floating pt regs not yet supported
	 return false;
      default:
	 assert(false);
      }
   else { 
      // control, debug, nullreg not yet supported
      return false;
   }
}

reg_t& x86_reg_set::getRegByBit(unsigned bit) {
   if (bit < 30) 
      return (reg_t&) *(new x86_reg(x86_reg::rawIntReg, bit));
   else if (bit < 33) // misc regs
      switch (bit) {
      case 30:
	 return x86_reg::EFLAGS;
      case 31:
	 return x86_reg::PC;
      case 32:
	 return x86_reg::FP;
      default:
	 assert(false);
      }
   else // control, debug, nullreg
      assert(false);
   return x86_reg::nullreg;
}

/* ******************************************************************* */

/*
       2            1       0       0
       9            6       8       0
             EEEEEEEE                
       GFDSCEDSBSBDCADSBSBDCABDCABDCA
       SSSSSSIIPPXXXXIIPPXXXXHHHHLLLL
            x       x       x       x

       i = regnum     regname       
       -----------------------------
        0 -  7   	 xH or xL      
        8 - 11   	    xX         
       12 - 15   	 xI or xP      
       16 - 19   	   ExX         
       20 - 23   	exI or exP     
       24 - 29   	    xS         

 */

static unsigned fullIntRegMask() {
   return 0x3fffffff;
}

static unsigned saveInt32RegMask() {
   return 0x00CF0000; // ESI,EDI,EAX,EBX,ECX,EDX
}

static unsigned correctIntRegMask(unsigned in) {
   // Clear bits that are not in int masks.
   // If an E*X bit is on, also set corresponding *X, *H and *L bits.
   // If an E*I bit is on, also set corresponding *I bit.
   // If an E*P bit is on, also set corresponding *P bit.

   unsigned puff[] = {0x000, 0x111, 0x222, 0x333, 0x444, 0x555, 0x666, 0x777,
                      0x888, 0x999, 0xaaa, 0xbbb, 0xccc, 0xddd, 0xeee, 0xfff};
   unsigned char exs;
   unsigned eips;

   in &= fullIntRegMask();    // mask out unused bits
   exs = (in >> 16) & 0xf;    // E*X regs.
   in |= puff[exs];           // Set *X/*H/*L bits.
   eips = (in >> 8) & 0xf000; // E*I and E*P regs.
   in |= eips;                // Set *I and *P bits.
   return in;
}

static unsigned promoteIntRegMask(unsigned in) {
   // If an 8-bit reg is set, set its 16- and 32-bit containing reg.
   in |= (in & 0xf0) << 4;    // promote *H to *X
   in |= (in & 0xf) << 8;     // promote *L to *X
   in |= (in & 0xff00) << 8;  // promote *P/*S/*X to E*P/E*S/E*X
   return in;
}


// Converts int reg I to the mask, according to the above table.
static unsigned singleIntRegMask(unsigned i) {
   assert(i < 30);
   return 1U << i;
}

x86_reg_set::x86_reg_set(SingleIntReg, unsigned i) : regset_t() {
   assert(i < 30);
   int_bits = singleIntRegMask(i);
   float_bits = 0;
   misc_stuff = 0;
}

x86_reg_set::x86_reg_set(SingleFloatReg, unsigned precision,
			 x86_reg r) : regset_t() {
   assert(0);
   // We're not sure whether this is right for X86.
   assert(r.isFloatReg());
   unsigned floatNum = r.getFloatNum();
      // from 0 to 63
   assert(floatNum < 64);

   int_bits = 0;
   misc_stuff = 0;
   float_bits = 0; // for now...

   assert(precision == 1 || precision == 2 || precision == 4);
   
   for (unsigned lcv=0; lcv < precision; ++lcv) {
      float_bits |= (1ULL << (floatNum + lcv));
   }

   if (precision == 1)
      assert(floatNum < 32);
   else if (precision == 2)
      assert(floatNum % 2 == 0);
   else if (precision == 4)
      assert(floatNum % 4 == 0);
   else
      assert(false);
}

x86_reg_set::x86_reg_set(const x86_reg &r) : regset_t() {
   // intializes to a set of a single element

   if(r.isNullReg()) {
      /* Don't represent null reg in regsets */
      int_bits = float_bits = misc_stuff = 0;
      return;
   }
   else if(r.isIntReg()) {
      // for int_bits, since this isn't the singleIntReg constructor, we
      // go ahead and do promotion and correction
      int_bits = singleIntRegMask(r.getIntNum());
      int_bits = correctIntRegMask(promoteIntRegMask(int_bits));
      float_bits = 0;
      misc_stuff = 0;
      return;
   }
   else if(r.isFloatReg()) { // one of the 64 32-bit fp regs.
      assert (0);
      int_bits = 0;
      float_bits = 1ULL << r.getFloatNum();
      misc_stuff = 0;
      return;
   }

   int_bits = 0;
   float_bits = 0;
   misc_stuff = 0;

   if(r.isPC())
      ms.pc = 1;
   else if(r.isEFLAGS()) {
      ms.eflags = 1;
   } 
   else if(r.isFP()) {
      ms.fp = 1;
   } 
   //   else {
   //assert(!"x86_reg_set::x86_reg_set(x86_reg) - unsupported reg\n");
   //}
}

x86_reg_set::x86_reg_set(const x86_reg_set &src) : regset_t() {
   int_bits = src.int_bits;
   float_bits = src.float_bits;
   misc_stuff = src.misc_stuff;
}

void x86_reg_set::operator=(const regset_t &src) {
   int_bits = ((x86_reg_set&)src).int_bits;
   float_bits = ((x86_reg_set&)src).float_bits;
   misc_stuff = ((x86_reg_set&)src).misc_stuff;
}

void x86_reg_set::operator=(const x86_reg_set &src) {
   int_bits = src.int_bits;
   float_bits = src.float_bits;
   misc_stuff = src.misc_stuff;
}

x86_reg_set::x86_reg_set(Empty) : regset_t() {
   int_bits = 0;
   float_bits = 0;
   misc_stuff = 0;
}

x86_reg_set::x86_reg_set(Full) : regset_t() {
   int_bits = fullIntRegMask();
   float_bits = 0xffffffffffffffffULL;
   misc_stuff = 0xffffffff;
   ms.padding = 0;
}

x86_reg_set::x86_reg_set(Save) : regset_t() {
   float_bits = 0;
   int_bits = saveInt32RegMask();
   misc_stuff = 0;
}

bool x86_reg_set::operator==(const regset_t &src) const {
   return (int_bits == ((x86_reg_set&)src).int_bits &&
	   float_bits == ((x86_reg_set&)src).float_bits &&
	   misc_stuff == ((x86_reg_set&)src).misc_stuff);
}

bool x86_reg_set::operator==(const x86_reg_set &src) const {
   return (int_bits == src.int_bits &&
	   float_bits == src.float_bits &&
	   misc_stuff == src.misc_stuff);
}

bool x86_reg_set::operator==(const reg_t &r) const {
   // the set is equal to r if it contains a single element that is r.
   if (int_bits != 0) {
      // only matches if r is an integer register
      return (float_bits == 0 && misc_stuff == 0
	      && singleIntRegMask(r.getIntNum()) == int_bits);
   } else if (float_bits != 0) {
      assert(0);
      // only matches if r is a float register.
      // As usual, this class treats the fp register file as 64 32-bit regs.
      if (int_bits != 0 || misc_stuff != 0) {
	 return false;
      }
      return (1ULL << r.getFloatNum()) == float_bits;
   }
   
   // compare misc stuff
   if (ari_popc(misc_stuff) != 1) {
      return false;
   }
   if (r.isPC()) {
      return ms.pc;
   } else if (((x86_reg&)r).isEFLAGS()) {
      return ms.eflags;
   } else if (((x86_reg&)r).isFP()) {
      return ms.fp;
   } else {
      assert(false);
   }
}

bool x86_reg_set::operator!=(const regset_t &src) const {
   return (int_bits != ((x86_reg_set&)src).int_bits || 
	   float_bits != ((x86_reg_set&)src).float_bits ||
	   misc_stuff != ((x86_reg_set&)src).misc_stuff);
}

bool x86_reg_set::exists(const reg_t &r) const {
   if (r.isIntReg())
      return existsIntReg(r.getIntNum());
   else if (r.isFloatReg())
      // as usual, this class treats the fp reg file as 64 regs of 32 bits each.
      // That's the comparison you'll get here; "r" will be one of 64 32-bit fp regs.
      { assert(0); return existsFpReg(r.getFloatNum(), 0); }
   else if (r.isPC())
	return ms.pc;
   else if (((x86_reg&)r).isEFLAGS())
	return ms.eflags;
   else if (((x86_reg&)r).isFP())
	return ms.fp;
   else
	assert(false);
}

bool x86_reg_set::exists(const regset_t &subset) const {
   // return true iff 'subset' is a subset of 'this'
   if (0ULL != (((x86_reg_set&)subset).float_bits & (~float_bits)))
      return false;
   if (0U != (((x86_reg_set&)subset).int_bits & (~promoteIntRegMask(int_bits))))
      return false;
   if (0U != (((x86_reg_set&)subset).misc_stuff & (~misc_stuff)))
      return false;

   return true;
}

bool x86_reg_set::existsIntReg(unsigned num) const {
   const unsigned mask = singleIntRegMask(num);
   return ((int_bits & mask) != 0);
}

bool x86_reg_set::existsFpReg(unsigned num, unsigned /*precision*/) const {
   assert(0); // FP not defined for x86.
   // As usual, this class treats the fp reg file as containing 64 32-bit fp regs.
   const unsigned long long mask = 1ULL << num;
   return ((float_bits & mask) != 0);
}

bool x86_reg_set::existsFpRegSinglePrecision(unsigned num) const {
   assert(0); // FP not defined for x86.
   // As usual, this class treats the fp reg file as containing 64 32-bit fp regs.
   const unsigned long long mask = 1ULL << num;
   return ((float_bits & mask) != 0);
}

bool x86_reg_set::isempty() const {
   return (int_bits == 0 && float_bits == 0 && misc_stuff == 0);
}

void x86_reg_set::clear() {
   int_bits = 0;
   float_bits = 0;
   misc_stuff = 0;
}

static unsigned diffIntRegMasks(unsigned a, unsigned b)
{
   // Return set difference A-B.  Ordinary set difference (A&~B) can
   // yield an invalid integer reg set.

   return correctIntRegMask(promoteIntRegMask(a)) & 
          ~(correctIntRegMask(promoteIntRegMask(b)));
}

regset_t& x86_reg_set::operator|=(const regset_t &src) {
   // performs an in-place set union operation.  It's fast.
   int_bits |= ((x86_reg_set&)src).int_bits;
   float_bits |= ((x86_reg_set&)src).float_bits;
   misc_stuff |= ((x86_reg_set&)src).misc_stuff; // correct for bit fields?  Nah.
   return *this;
}

regset_t& x86_reg_set::operator|=(const reg_t &r) {
   const x86_reg_set src((x86_reg&)r);
   return operator|=(src);
}

x86_reg_set& x86_reg_set::operator|=(const x86_reg_set &src) {
   // performs an in-place set union operation.  It's fast.
   int_bits |= src.int_bits;
   float_bits |= src.float_bits;
   misc_stuff |= src.misc_stuff; // correct for bit fields?  Nah.
   return *this;
}

x86_reg_set& x86_reg_set::operator|=(const x86_reg &r) {
   const x86_reg_set src(r);
   return operator|=(src);
}

regset_t& x86_reg_set::operator+=(const regset_t &src) {
   return operator|=(src);
}

regset_t& x86_reg_set::operator+=(const reg_t &r) {
   return operator|=(r);
}

x86_reg_set& x86_reg_set::operator+=(const x86_reg_set &src) {
   return operator|=(src);
}

x86_reg_set& x86_reg_set::operator+=(const x86_reg &r) {
   return operator|=(r);
}

regset_t& x86_reg_set::operator&=(const regset_t &src) {
   // performs an in-place set intersection operation.  It's fast.
   int_bits &= ((x86_reg_set&)src).int_bits;
   float_bits &= ((x86_reg_set&)src).float_bits;
   misc_stuff &= ((x86_reg_set&)src).misc_stuff; // okay on bit fields?  Yea.
   return *this;
}

regset_t& x86_reg_set::operator&=(const reg_t &r) {
   // performs an in-place set intersection operation.  It's fast.
   const x86_reg_set src((x86_reg&)r);
   return operator&=(src);
}

x86_reg_set& x86_reg_set::operator&=(const x86_reg_set &src) {
   // performs an in-place set intersection operation.  It's fast.
   int_bits &= src.int_bits;
   float_bits &= src.float_bits;
   misc_stuff &= src.misc_stuff; // okay on bit fields?  Yea.
   return *this;
}

x86_reg_set& x86_reg_set::operator&=(const x86_reg &r) {
   // performs an in-place set intersection operation.  It's fast.
   const x86_reg_set src(r);
   return operator&=(src);
}

regset_t* x86_reg_set::setIntersection (const regset_t &set) const {
   x86_reg_set &set2 = (x86_reg_set &)set;
   return new x86_reg_set(*this & set2);
}

regset_t& x86_reg_set::operator-=(const regset_t &src) {
   // performs an in-place set difference operation.  It's fast.
   // Set difference is the intersection of 'this' and the set-negation of 'src'
   // Thus, it's the bitwise-and of 'bits' and '~src.bits'
   int_bits = diffIntRegMasks(int_bits, ((x86_reg_set&)src).int_bits);
   float_bits &= ~((x86_reg_set&)src).float_bits;
   misc_stuff &= (~((x86_reg_set&)src).misc_stuff);
   ms.padding = 0; // necessary

   return *this;
}

regset_t& x86_reg_set::operator-=(const reg_t &r) {
   const x86_reg_set src((x86_reg&)r);
   return operator-=(src);
}

x86_reg_set& x86_reg_set::operator-=(const x86_reg_set &src) {
   // performs an in-place set difference operation.  It's fast.
   // Set difference is the intersection of 'this' and the set-negation of 'src'
   // Thus, it's the bitwise-and of 'bits' and '~src.bits'
   int_bits = diffIntRegMasks(int_bits, src.int_bits);
   float_bits &= ~src.float_bits;
   misc_stuff &= ~src.misc_stuff;
   ms.padding = 0; // necessary

   return *this;
}

x86_reg_set& x86_reg_set::operator-=(const x86_reg &r) {
   const x86_reg_set src(r);
   return operator-=(src);
}

x86_reg_set x86_reg_set::operator|(const x86_reg_set &src) const {
   x86_reg_set result = *this;
   return (result |= src);
}

x86_reg_set x86_reg_set::operator|(const x86_reg &r) const {
   x86_reg_set result = *this;
   return (result |= r);
}

x86_reg_set x86_reg_set::operator+(const x86_reg_set &src) const {
   return operator|(src);
}

x86_reg_set x86_reg_set::operator+(const x86_reg &r) const {
   return operator|(r);
}

x86_reg_set x86_reg_set::operator&(const x86_reg_set &src) const {
   x86_reg_set result = *this;
   return (result &= src);
}

x86_reg_set x86_reg_set::operator&(const x86_reg &r) const {
   x86_reg_set result = *this;
   return (result &= r);
}

x86_reg_set x86_reg_set::operator-(const x86_reg_set &src) const {
   x86_reg_set result = *this;
   return (result -= src);
}

x86_reg_set x86_reg_set::operator-(const x86_reg &r) const {
   x86_reg_set result = *this;
   return (result -= r);
}

x86_reg x86_reg_set::expand1IntReg() const {
   assert(int_bits != 0);
   assert(float_bits == 0);
   assert(misc_stuff == 0);

   // FIXME: If things like the e*X regs are set, or AX, this will
   // only find the smallest unit inside.  See the FP stuff for some
   // smarts for dealing with this.
   unsigned first = ari_ffs(int_bits);
   return x86_reg(x86_reg::rawIntReg, first);
}

x86_reg x86_reg_set::expand1FloatReg() const {
   assert(0);
   return x86_reg::PC;
}

x86_reg x86_reg_set::expand1() const {
   if (int_bits != 0)
      return expand1IntReg();
   else if (float_bits != 0)
      return expand1FloatReg();
   else if (ms.pc)
      return x86_reg::PC;
   else if (ms.eflags)
      return x86_reg::EFLAGS;
   else if (ms.fp)
      return x86_reg::FP;
   else
      assert(false);
   return x86_reg::PC;
}

unsigned x86_reg_set::count() const {
   // return the # of elems in the set
   return countIntRegs() + countFPRegsAsSinglePrecision() + countMiscRegs();
}

unsigned x86_reg_set::countIntRegs() const {
   return ari_popc(int_bits);
}

unsigned x86_reg_set::countFPRegsAsSinglePrecision() const {
   return ari_popc(float_bits);
}

unsigned x86_reg_set::countMiscRegs() const {
   return ari_popc(misc_stuff);
}

// FIXME: This might be an example where we want a strict existence test:  If
// AH, only, is set, this routine also prints AX and eAX, since they
// are, at least in part, contained in the set.
pdstring x86_reg_set::disassx() const {
   pdstring result;

   const_iterator start = begin();
   const_iterator finish = end();
   
   for (const_iterator iter=start; iter != finish; ++iter) {
      const x86_reg reg = (x86_reg&)*iter;

      // add a space, except for the first time
      if (iter != start)
         result += " ";
      
      result += reg.disass();
   }

   return result;
}

/* ********************************************************************** */

x86_reg_set operator~(const x86_reg_set &src) {
   x86_reg_set result(x86_reg_set::raw, 
		      ~(src.getRawIntBits()), 
		      ~(src.getRawFloatBits()),
		      ~(src.getRawMiscBits()));
   result.ms.padding = 0;
   return result;
}

/* ********************************************************************** */

// declare some static members.  Note: we mustn't use any static members
// when defining static members, since the order of construction is undefined!
// (Thus, perpetually-undefined static members, like raw, range, empty, etc. may be
// used at any time)

x86_reg_set::Save x86_reg_set::save;
x86_reg_set x86_reg_set::emptyset(regset_t::raw, 0, 0, 0);
x86_reg_set x86_reg_set::fullset(regset_t::raw, 0x3fffffff,
				 0xffffffffffffffffULL,
				 0xffffffff);

/* always_live_set: int(CS,DS,SS,ESP), float(none), misc(EIP,EFLAGS) */
x86_reg_set x86_reg_set::always_live_set(regset_t::raw, 0x0e101000, 0, 0xc0000000);

x86_reg_set x86_reg_set::allFPs(regset_t::raw, 0, 0xffffffffffffffffULL, 0);
x86_reg_set x86_reg_set::allIntRegs(regset_t::raw, 0xffffffff, 0, 0);
x86_reg_set x86_reg_set::allMiscRegs(regset_t::raw, 0, 0, 0xffffffff);

/* regs that ignore writes: just EIP */
x86_reg_set x86_reg_set::regsThatIgnoreWrites(regset_t::raw, 0, 0, 0x20000000);


