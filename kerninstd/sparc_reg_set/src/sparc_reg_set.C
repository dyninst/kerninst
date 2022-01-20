// sparc_reg_set.C
// Ariel Tamches

#include "bitUtils.h"
#include "sparc_reg_set.h"
#include "util/h/popc.h"
#include "util/h/ffs.h"
#include "util/h/xdr_send_recv.h"
#include <stdlib.h> // random()

// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------

sparc_reg_set::sparc_reg_set(XDR *xdr) :
   theFloatRegSet(xdr),
   theIntRegSet(xdr),
   theMiscRegSet(xdr) {
}

bool sparc_reg_set::send(XDR *xdr) const {
   return (theFloatRegSet.send(xdr) &&
           theIntRegSet.send(xdr) &&
           theMiscRegSet.send(xdr));
}

bool sparc_reg_set::inSet(const regset_t &theset, unsigned bit) {
  const sparc_reg_set &set = (const sparc_reg_set &)theset;
   if (bit < 32) 
      return set.existsIntReg(bit);
   else if (bit < 96) { // one of the 64 32-bit fp regs.
      unsigned floatnum = bit-32;
      return set.existsFpRegSinglePrecision(floatnum);
   }
   switch (bit) {
      case 96:
         return set.existsIcc();
      case 97:
         return set.existsXcc();
      case 98:
         return set.existsFcc0();
      case 99:
         return set.existsFcc1();
      case 100:
         return set.existsFcc2();
      case 101:
         return set.existsFcc3();
      case 102:
         return set.existsPC();
      case 103:
         return set.existsGSR();
      case 104:
         return set.existsASI();
      case 105:
         return set.existsPIL();
      default:
         assert(false);
   }
}

reg_t& sparc_reg_set::getRegByBit(unsigned bit) {
   if (bit < 32) // int reg
      return *(new sparc_reg(sparc_reg::rawIntReg, bit));
   else if (bit < 96) // fp reg
      return *(new sparc_reg(sparc_reg::f, bit-32));

   switch (bit) {
    case 96:
      return sparc_reg::reg_icc;
    case 97:
      return sparc_reg::reg_xcc;
    case 98:
      return sparc_reg::reg_fcc0;
    case 99:
      return sparc_reg::reg_fcc1;
    case 100:
      return sparc_reg::reg_fcc2;
    case 101:
      return sparc_reg::reg_fcc3;
    case 102:
      return sparc_reg::reg_pc;
    case 103:
      return sparc_reg::reg_gsr;
    case 104:
      return sparc_reg::reg_asi;
    case 105:
      return sparc_reg::reg_pil;
    default:
      assert(false);
      abort(); // placate compiler when compiling with NDEBUG
   }
}

/* ******************************************************************* */

sparc_reg_set::sparc_reg_set(SingleIntReg, unsigned rnum) :
   theIntRegSet(sparc_int_regset::singleIntReg, rnum)
{
   // default ctor for theFloatRegSet (empty) & theMiscRegSet (empty)
}

sparc_reg_set::sparc_reg_set(SingleFloatReg,
                             unsigned precision,
                             sparc_reg r) :
   theFloatRegSet(sparc_fp_regset::SingleFloatReg(), precision, r),
   theIntRegSet(sparc_int_regset::empty),
   theMiscRegSet(sparc_misc_regset::empty)
{
}

sparc_reg_set::sparc_reg_set(const reg_t &r) {
   // theFloatRegSet: initially empty
   // theIntRegSet: initially empty
   // theMiscRegSet: initially empty
   // intializes to a set of a single element
  
   //We need this ugly workaround, because pure virtual methods
   //cannot be called from a constructor.  An extra copy, oh well.
   sparc_reg rc = sparc_reg((sparc_reg&)r);
   if (rc.isIntReg())
      theIntRegSet = sparc_int_regset(sparc_int_regset::singleIntReg, 
				      rc);
   else if (rc.isFloatReg())
      theFloatRegSet = sparc_fp_regset(sparc_fp_regset::singleFloatReg, 
				       rc);
      // single-precision
   else
      theMiscRegSet = sparc_misc_regset(rc);
}

sparc_reg_set::sparc_reg_set(const regset_t &src) :
   theFloatRegSet(((const sparc_reg_set&)src).theFloatRegSet),
   theIntRegSet(((const sparc_reg_set&)src).theIntRegSet),
   theMiscRegSet(((const sparc_reg_set&)src).theMiscRegSet)
{}

void sparc_reg_set::operator=(const regset_t &src) {
   theFloatRegSet = ((const sparc_reg_set&)src).theFloatRegSet;
   theIntRegSet = ((const sparc_reg_set&)src).theIntRegSet;
   theMiscRegSet = ((const sparc_reg_set&)src).theMiscRegSet;
}

void sparc_reg_set::operator=(const sparc_reg_set &src) {
   theFloatRegSet = src.theFloatRegSet;
   theIntRegSet = src.theIntRegSet;
   theMiscRegSet = src.theMiscRegSet;
}

sparc_reg_set::sparc_reg_set(Empty) :
   theFloatRegSet(sparc_fp_regset::empty),
   theIntRegSet(sparc_int_regset::empty),
   theMiscRegSet(sparc_misc_regset::empty)
{
}

sparc_reg_set::sparc_reg_set(Full) :
   theFloatRegSet(sparc_fp_regset::full),
   theIntRegSet(sparc_int_regset::full),
   theMiscRegSet(sparc_misc_regset::full)
{
}

sparc_reg_set::sparc_reg_set(Save, const sparc_reg_set &src, bool iValForLsAndOs) :
   theFloatRegSet(src.theFloatRegSet), // no change for fp regs
   theIntRegSet(sparc_int_regset::save, src.theIntRegSet, iValForLsAndOs),
   theMiscRegSet(src.theMiscRegSet) // no change for misc regs
{
}

bool sparc_reg_set::operator==(const regset_t &src) const {
   return (theFloatRegSet == ((const sparc_reg_set&)src).theFloatRegSet &&
           theIntRegSet == ((const sparc_reg_set&)src).theIntRegSet &&
           theMiscRegSet == ((const sparc_reg_set&)src).theMiscRegSet);
}

bool sparc_reg_set::operator==(const reg_t &r) const {
   // the set is equal to r if it contains a single element that is r.
   if (!theIntRegSet.isempty())
      // only matches if r is an integer register
      return (theFloatRegSet.isempty() && theMiscRegSet.isempty() &&
              theIntRegSet == (const sparc_reg&)r);
   else if (!theMiscRegSet.isempty())
      return theFloatRegSet.isempty() && theMiscRegSet == (const sparc_reg&)r;
   else if (!theFloatRegSet.isempty())
      return theFloatRegSet == (const sparc_reg&)r; // assumes r is single-precision
   else
      return false;
}

bool sparc_reg_set::operator!=(const regset_t &src) const {
   return (theFloatRegSet != ((const sparc_reg_set&)src).theFloatRegSet ||
           theIntRegSet != ((const sparc_reg_set&)src).theIntRegSet ||
           theMiscRegSet != ((const sparc_reg_set&)src).theMiscRegSet);
}

bool sparc_reg_set::equalIsAndLs(const sparc_reg_set &src) const {
   return theIntRegSet.equalIsAndLs(src.theIntRegSet);
}

bool sparc_reg_set::exists(const reg_t &r) const {
   if (r.isIntReg())
      return existsIntReg(r.getIntNum());
   else if (r.isFloatReg())
      return existsFpRegSinglePrecision(r.getFloatNum());
      // As usual in this class, an fp sparc_reg is assumed to be single-precision.
   else
      return theMiscRegSet.exists((const sparc_reg&)r);
}

bool sparc_reg_set::exists(const regset_t &subset) const {
   // return true iff 'subset' is a subset of 'this'
   return (theFloatRegSet.exists(((const sparc_reg_set&)subset).theFloatRegSet) &&
           theIntRegSet.exists(((const sparc_reg_set&)subset).theIntRegSet) &&
           theMiscRegSet.exists(((const sparc_reg_set&)subset).theMiscRegSet));
}

bool sparc_reg_set::existsIntReg(unsigned num) const {
   return theIntRegSet.existsIntRegNum(num);
}

bool sparc_reg_set::existsFpReg(unsigned num, unsigned precision) const {
   assert(num < 64);
   assert(precision == 1 || 
          precision == 2 && num % 2 == 0 ||
          precision == 4 && num % 4 == 0);
   return theFloatRegSet.exists(num, precision);
}

bool sparc_reg_set::existsFpRegSinglePrecision(unsigned num) const {
   const uint64_t raw = theFloatRegSet.getRawBits();
   return 0 != (raw & (1ULL << num));
}

bool sparc_reg_set::isempty() const {
   return (theFloatRegSet.isempty() &&
           theIntRegSet.isempty() &&
           theMiscRegSet.isempty());
}

void sparc_reg_set::clear() {
   theFloatRegSet.clear();
   theIntRegSet.clear();
   theMiscRegSet.clear();
}

regset_t& sparc_reg_set::operator|=(const regset_t &src) {
   // performs an in-place set union operation.  It's fast.
   theFloatRegSet.operator|=(((sparc_reg_set&)src).theFloatRegSet);
   theIntRegSet.operator|=(((sparc_reg_set&)src).theIntRegSet);
   theMiscRegSet.operator|=(((sparc_reg_set&)src).theMiscRegSet);

   return *this;
}

sparc_reg_set sparc_reg_set::operator|=(const sparc_reg_set &src) {
   // performs an in-place set union operation.  It's fast.
   theFloatRegSet.operator|=(src.theFloatRegSet);
   theIntRegSet.operator|=(src.theIntRegSet);
   theMiscRegSet.operator|=(src.theMiscRegSet);

   return *this;
}

regset_t& sparc_reg_set::operator|=(const reg_t &r) {
   const sparc_reg_set src((sparc_reg&)r);
   return operator|=((const regset_t &)src);
}

sparc_reg_set sparc_reg_set::operator|=(const sparc_reg &r) {
   const sparc_reg_set src(r);
   return operator|=(src);
}

sparc_reg_set sparc_reg_set::operator+=(const sparc_reg_set &src) {
   return operator|=(src);
}

sparc_reg_set sparc_reg_set::operator+=(const sparc_reg &r) {
   return operator|=(r);
}

regset_t& sparc_reg_set::operator+=(const regset_t &src) {
   return operator|=(src);
}

regset_t& sparc_reg_set::operator+=(const reg_t &r) {
   return operator|=(r);
}

regset_t& sparc_reg_set::operator&=(const regset_t &src) {
   // performs an in-place set intersection operation.  It's fast.
   theFloatRegSet.operator&=(((const sparc_reg_set&)src).theFloatRegSet);
   theIntRegSet.operator&=(((const sparc_reg_set&)src).theIntRegSet);
   theMiscRegSet.operator&=(((const sparc_reg_set&)src).theMiscRegSet);
   return *this;
}

regset_t& sparc_reg_set::operator&=(const reg_t &r) {
   // performs an in-place set intersection operation.  It's fast.
   const sparc_reg_set src((sparc_reg&)r);
   return operator&=((const regset_t &)src);
}

sparc_reg_set sparc_reg_set::operator&=(const sparc_reg_set &src) {
   // performs an in-place set intersection operation.  It's fast.
   theFloatRegSet.operator&=(src.theFloatRegSet);
   theIntRegSet.operator&=(src.theIntRegSet);
   theMiscRegSet.operator&=(src.theMiscRegSet);

   return *this;
}

sparc_reg_set sparc_reg_set::operator&=(const sparc_reg &r) {
   // performs an in-place set intersection operation.  It's fast.
   const sparc_reg_set src(r);
   return operator&=(src);
}

regset_t* sparc_reg_set::setIntersection (const regset_t &set) const {
   sparc_reg_set &set2 = (sparc_reg_set &)set;
   return new sparc_reg_set(*this & set2);
}

regset_t& sparc_reg_set::operator-=(const regset_t &src) {
   // performs an in-place set difference operation.  It's fast.
   // Set difference is the intersection of 'this' and the set-negation of 'src'
   // Thus, it's the bitwise-and of 'bits' and '~src.bits'
   theFloatRegSet.operator-=(((const sparc_reg_set &)src).theFloatRegSet);
   theIntRegSet.operator-=(((const sparc_reg_set &)src).theIntRegSet);
   theMiscRegSet.operator-=(((const sparc_reg_set &)src).theMiscRegSet);

   return *this;
}

regset_t& sparc_reg_set::operator-=(const reg_t &r) {
   const sparc_reg_set src((sparc_reg&)r);
   return operator-=((const regset_t &)src);
}

sparc_reg_set sparc_reg_set::operator-=(const sparc_reg_set &src) {
   // performs an in-place set difference operation.  It's fast.
   // Set difference is the intersection of 'this' and the set-negation of 'src'
   // Thus, it's the bitwise-and of 'bits' and '~src.bits'

   theFloatRegSet.operator-=(src.theFloatRegSet);
   theIntRegSet.operator-=(src.theIntRegSet);
   theMiscRegSet.operator-=(src.theMiscRegSet);
   
   return *this;
}

sparc_reg_set sparc_reg_set::operator-=(const sparc_reg &r) {
   const sparc_reg_set src(r);
   return operator-=(src);
}

sparc_reg_set sparc_reg_set::operator|(const sparc_reg_set &src) const {
   sparc_reg_set result = *this;
   return (result |= src);
}

sparc_reg_set sparc_reg_set::operator|(const sparc_reg &r) const {
   sparc_reg_set result = *this;
   return (result |= r);
}

sparc_reg_set sparc_reg_set::operator+(const sparc_reg_set &src) const {
   return operator|(src);
}

sparc_reg_set sparc_reg_set::operator+(const sparc_reg &r) const {
   return operator|(r);
}

sparc_reg_set sparc_reg_set::operator&(const sparc_reg_set &src) const {
   sparc_reg_set result = *this;
   return (result &= src);
}

sparc_reg_set sparc_reg_set::operator&(const sparc_reg &r) const {
   sparc_reg_set result = *this;
   return (result &= r);
}

sparc_reg_set sparc_reg_set::operator-(const sparc_reg_set &src) const {
   sparc_reg_set result = *this;
   result -= src;
   return result;
}

sparc_reg_set sparc_reg_set::operator-(const sparc_reg &r) const {
   sparc_reg_set result = *this;
   result -= r;
   return result;
}

sparc_reg sparc_reg_set::expand1IntReg() const {
   assert(!theIntRegSet.isempty());
   assert(theFloatRegSet.isempty());
   assert(theMiscRegSet.isempty());

   return theIntRegSet.expand1();
}

sparc_reg sparc_reg_set::expand1FloatReg() const {
   assert(!theFloatRegSet.isempty());
   assert(theIntRegSet.isempty());
   assert(theMiscRegSet.isempty());

   return theFloatRegSet.expand1Reg();
}

sparc_reg sparc_reg_set::expand1() const {
   if (!theIntRegSet.isempty())
      return theIntRegSet.expand1();
   else if (!theFloatRegSet.isempty())
      return expand1FloatReg();
   else
      return theMiscRegSet.expand1();
}

unsigned sparc_reg_set::count() const {
   // return the # of elems in the set.  Assumes single-precision for fp (arbitrary!)
   return countIntRegs() + countFPRegsAsSinglePrecision() + countMiscRegs();
}

unsigned sparc_reg_set::countIntRegs() const {
   return theIntRegSet.count();
}

unsigned sparc_reg_set::countFPRegsAsSinglePrecision() const {
   // reminder: there are 64 single-precision fp regs, even though acessing
   // those numbered 32 to 63 is limited to dbl and quad precision instructions.
   return theFloatRegSet.countAsSinglePrecision();
}

unsigned sparc_reg_set::countMiscRegs() const {
   return theMiscRegSet.count();
}

void sparc_reg_set::processRestore(const sparc_reg_set &prevWindow) {
   theFloatRegSet = prevWindow.theFloatRegSet;
   theMiscRegSet = prevWindow.theMiscRegSet;

   theIntRegSet.processRestore(prevWindow.theIntRegSet);
}

pdstring sparc_reg_set::disassx() const {
   pdstring result;

   const_iterator start = begin();
   const_iterator finish = end();
   
   for (const_iterator iter=start; iter != finish; ++iter) {
      const sparc_reg &reg = (const sparc_reg&)*iter;

      // add a space, except for the first time
      if (iter != start)
         result += " ";
      
      result += reg.disass();
   }

   return result;
}

/* ********************************************************************** */

sparc_reg_set sparc_reg_set::operator~() const {

   sparc_reg_set result(sparc_reg_set::raw,
                        ~(theIntRegSet.getRawBits()),
                        ~(theFloatRegSet.getRawBits()),
			~(theMiscRegSet.getRawBits()));
      // don't worry, theMiscRegSet raw ctor clears the padding bits for us

   return result;
}

/* ********************************************************************** */

// declare some static members.  Note: we mustn't use any static members
// when defining static members, since the order of construction is undefined!
const sparc_reg_set sparc_reg_set::emptyset(sparc_reg_set::raw, 0x0, 0x0, 0);
const sparc_reg_set sparc_reg_set::fullset(sparc_reg_set::raw, 0xffffffff,
                                           0xffffffffffffffffULL,
                                           0xffffffff);
const sparc_reg_set sparc_reg_set::always_live_set(sparc_reg_set::raw, 
						   0x40004001, 0, 0);

sparc_reg_set::Save sparc_reg_set::save;
const sparc_reg_set sparc_reg_set::allGs(sparc_reg_set::raw, 0x000000ff, 0x0, 0x0);
const sparc_reg_set sparc_reg_set::allOs(sparc_reg_set::raw, 0x0000ff00, 0x0, 0x0);
const sparc_reg_set sparc_reg_set::allLs(sparc_reg_set::raw, 0x00ff0000, 0x0, 0x0);
const sparc_reg_set sparc_reg_set::allIs(sparc_reg_set::raw, 0xff000000, 0x0, 0x0);
const sparc_reg_set sparc_reg_set::allFPs(sparc_reg_set::raw, 0x00000000,
                                          0xffffffffffffffffULL,
                                          0x0);
const sparc_reg_set sparc_reg_set::allIntRegs(sparc_reg_set::raw, 0xffffffff, 0x0, 0x0);
//sparc_reg_set sparc_reg_set::allMiscRegs(sparc_reg_set::raw, 0, 0, 0x1ff);
const sparc_reg_set sparc_reg_set::allMiscRegs(sparc_reg_set::raw, 0, 0, 0xffffffff);

const sparc_reg_set
sparc_reg_set::regsThatIgnoreWrites(sparc_reg_set::raw, 0x1, 0x0, 0x0);
   // just %g0
