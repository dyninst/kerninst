// power_reg_set.C
// Igor Grobman (based on sparc_reg_set by Ariel Tamches)

#include "bitUtils.h"
#include "power_reg_set.h"
#include "util/h/popc.h"
#include "util/h/ffs.h"
#include "util/h/xdr_send_recv.h"
#include <stdlib.h> // random()

// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------

power_reg_set::power_reg_set(Random) :
   regset_t (),
   theFloatRegSet(power_fp_regset::random),
   theIntRegSet(power_int_regset::random),
   theMiscRegSet(power_misc_regset::random)
{
}


power_reg_set::power_reg_set(XDR *xdr) :
   regset_t (),
   theFloatRegSet(xdr),
   theIntRegSet(xdr),
   theMiscRegSet(xdr) {
}

bool power_reg_set::send(XDR *xdr) const {
   return (theFloatRegSet.send(xdr) &&
           theIntRegSet.send(xdr) &&
           theMiscRegSet.send(xdr));
}


bool power_reg_set::inSet(const regset_t &set, unsigned bit)  {
   const power_reg_set& theset = (power_reg_set &)set; 
   //get rid of necessity to cast
   
   if (bit < 32) 
      return theset.existsIntReg(bit);
   else if (bit < 64) { // one of the 32  fp regs.
      unsigned floatnum = bit-32;
      return theset.existsFpReg(floatnum);
   }
   switch (bit) {
      case 64: 
         return theset.existsLr();
      case 65: 
         return theset.existsCtr();
      case 66:
         return false; //XER is not used as a full register
      case 67: 
         return theset.existsFpscr();
      case 68:
         return false; //CR is not used as a full register
      case 69: 
         return theset.existsTbr();
      case 70: 
         return theset.existsTbu();
      case 71: 
         return theset.existsAsr();
      case 72: 
         return theset.existsDar();
      case 73: 
         return theset.existsDsisr();
      case 74: case 75: case 76: case 77:
         return theset.existsSprg(bit - 74);
      case 78:
         return theset.existsMsr();
      case 79: 
         return theset.existsDec();
      case 80: 
         return theset.existsPvr();
      case 81: 
         return theset.existsSdr1();
      case 82: case 83: 
         return theset.existsSrr(bit - 82);
      case 84:
         return theset.existsEar();
      case 85: 
         return theset.existsMmcra();
      case 86: case 87: case 88: case 89: case 90: case 91: case 92:
      case 93: 
         return theset.existsPmc(bit - 85);
      case 94: 
         return theset.existsMmcr0();
      case 95: 
         return theset.existsSiar();
      case 96: 
         return theset.existsSdar();
      case 97: 
         return theset.existsMmcr1();
      case 98: 
         return theset.existsDabr();
      case 99: 
         return theset.existsPir();
      case 100:
         return theset.existsHdec();
      case 101: case 102: case 103: case 104: case 105: case 106: case 107: 
      case 108: 
         return theset.existsCrField(bit - 101);
      case 109:
         return theset.existsSo();
      case 110:
         return theset.existsOv();
      case 111: 
         return theset.existsCa();
      case 112:
         return theset.existsXerResv();
      case 113:
         return theset.existsXerStrBytes();
      case 114:
         return theset.existsAccr();
      case 115:
         return theset.existsCtrl();
      case 116:
         return theset.existsNull();
      default:
         assert(false);
   }
   return false;
}

/* ******************************************************************* */

power_reg_set::power_reg_set(SingleIntReg, unsigned rnum) :
   regset_t(),
   theIntRegSet(power_int_regset::singleIntReg, rnum)
{
   // default ctor for theFloatRegSet (empty) & theMiscRegSet (empty)
}

power_reg_set::power_reg_set(SingleFloatReg,
                             unsigned rnum) :
   regset_t(),
   theFloatRegSet(power_fp_regset::singleFloatReg, rnum),
   theIntRegSet(power_int_regset::empty),
   theMiscRegSet(power_misc_regset::empty)
{
}

power_reg_set::power_reg_set(const power_reg &r) : regset_t() {
   // theFloatRegSet: initially empty
   // theIntRegSet: initially empty
   // theMiscRegSet: initially empty
   // intializes to a set of a single element
  
   power_reg rc = power_reg(r);
   if (rc.isIntReg())
      theIntRegSet = power_int_regset(power_int_regset::singleIntReg, r);
   else if (rc.isFloatReg())
      theFloatRegSet = power_fp_regset(power_fp_regset::singleFloatReg, r);
   else
      theMiscRegSet = power_misc_regset(r);
}

power_reg_set::power_reg_set(const power_reg_set &src) :
   regset_t(src),
   theFloatRegSet(src.theFloatRegSet),
   theIntRegSet(src.theIntRegSet),
   theMiscRegSet(src.theMiscRegSet)
{
}

power_reg_set::power_reg_set(const regset_t &src) :
   regset_t(src),
   theFloatRegSet(((power_reg_set &)src).theFloatRegSet),
   theIntRegSet(((power_reg_set &)src).theIntRegSet),
   theMiscRegSet(((power_reg_set &)src).theMiscRegSet)
{
}

void power_reg_set::operator=(const power_reg_set &src) {
   theFloatRegSet = src.theFloatRegSet;
   theIntRegSet = src.theIntRegSet;
   theMiscRegSet = src.theMiscRegSet;
}


void power_reg_set::operator=(const regset_t &src) {
   theFloatRegSet = ((power_reg_set &)src).theFloatRegSet;
   theIntRegSet = ((power_reg_set &)src).theIntRegSet;
   theMiscRegSet = ((power_reg_set &)src).theMiscRegSet;
}
power_reg_set::power_reg_set(Empty) : regset_t(),
   theFloatRegSet(power_fp_regset::empty),
   theIntRegSet(power_int_regset::empty),
   theMiscRegSet(power_misc_regset::empty)
{
}

power_reg_set::power_reg_set(Full) :
   regset_t(),
   theFloatRegSet(power_fp_regset::full),
   theIntRegSet(power_int_regset::full),
   theMiscRegSet(power_misc_regset::full)
{
}



bool power_reg_set::operator==(const regset_t &src) const {
   return (theFloatRegSet == ((power_reg_set &)src).theFloatRegSet &&
           theIntRegSet == ((power_reg_set &)src).theIntRegSet &&
           theMiscRegSet == ((power_reg_set &)src).theMiscRegSet);
}



bool power_reg_set::operator==(const reg_t &r) const {
   // the set is equal to r if it contains a single element that is r.
   if (!theIntRegSet.isempty())
      // only matches if r is an integer register
      return (theFloatRegSet.isempty() && theMiscRegSet.isempty() &&
              theIntRegSet == (power_reg&)r);
   else if (!theMiscRegSet.isempty())
      return theFloatRegSet.isempty() && theMiscRegSet == (power_reg&)r;
   else if (!theFloatRegSet.isempty())
      return theFloatRegSet == (power_reg &)r; 
   else
      return false;
}

bool power_reg_set::operator!=(const regset_t &src) const {
   return (theFloatRegSet != ((power_reg_set &)src).theFloatRegSet ||
           theIntRegSet != ((power_reg_set &)src).theIntRegSet ||
           theMiscRegSet != ((power_reg_set &)src).theMiscRegSet);
}



bool power_reg_set::exists(const reg_t &r) const {
   if (  r.isIntReg())
      return existsIntReg(r.getIntNum());
   else if (r.isFloatReg())
      return existsFpReg(r.getFloatNum());
   else
      return theMiscRegSet.exists((power_reg &)r);
}

bool power_reg_set::exists(const regset_t &subset) const {
   // return true iff 'subset' is a subset of 'this'
   power_reg_set ss = (power_reg_set &) subset;

   return ( (subset != (regset_t &)emptyset) &&
           theFloatRegSet.exists(ss.theFloatRegSet) &&
           theIntRegSet.exists(ss.theIntRegSet) &&
           theMiscRegSet.exists(ss.theMiscRegSet));
}

bool power_reg_set::existsIntReg(unsigned num) const {
   return theIntRegSet.existsIntRegNum(num);
}

bool power_reg_set::existsFpReg(unsigned num) const {
   assert(num < 32);
   return theFloatRegSet.exists(num);
}



bool power_reg_set::isempty() const {
   return (theFloatRegSet.isempty() &&
           theIntRegSet.isempty() &&
           theMiscRegSet.isempty());
}

void power_reg_set::clear() {
   theFloatRegSet.clear();
   theIntRegSet.clear();
   theMiscRegSet.clear();
}

regset_t& power_reg_set::operator|=(const regset_t &src) {
   // performs an in-place set union operation.  It's fast.
   theFloatRegSet.operator|=(((power_reg_set &)src).theFloatRegSet);
   theIntRegSet.operator|=(((power_reg_set &)src).theIntRegSet);
   theMiscRegSet.operator|=(((power_reg_set &)src).theMiscRegSet);

   return *this;
}

power_reg_set& power_reg_set::operator|=(const power_reg_set &src) {
   // performs an in-place set union operation.  It's fast.
   theFloatRegSet.operator|=(src.theFloatRegSet);
   theIntRegSet.operator|=(src.theIntRegSet);
   theMiscRegSet.operator|=(src.theMiscRegSet);

   return *this;
}

power_reg_set& power_reg_set::operator|=(const power_reg &r) {
   const power_reg_set src(r);
   return operator|=(src);
}

power_reg_set& power_reg_set::operator&=(const power_reg_set &src) {
   // performs an in-place set intersection operation.  It's fast.
   theFloatRegSet.operator&=(src.theFloatRegSet);
   theIntRegSet.operator&=(src.theIntRegSet);
   theMiscRegSet.operator&=(src.theMiscRegSet);

   return *this;
}


regset_t * power_reg_set::setIntersection ( const regset_t &set) const {
   power_reg_set &pset = (power_reg_set &)set;
   return new power_reg_set(*this & pset);
}

power_reg_set& power_reg_set::operator&=(const power_reg &r) {
   // performs an in-place set intersection operation.  It's fast.
   const power_reg_set src(r);
   return operator&=(src);
}

regset_t& power_reg_set::operator|=(const reg_t &r) {
   const power_reg_set src((power_reg &)r);
   return operator|=(src);
}

regset_t& power_reg_set::operator+=(const regset_t &src) {
   return operator|=(src);
}

regset_t& power_reg_set::operator+=(const reg_t &r) {
   return operator|=(r);
}


power_reg_set& power_reg_set::operator+=(const power_reg_set &src) {
   return operator|=(src);
}

power_reg_set& power_reg_set::operator+=(const power_reg &r) {
   return operator|=(r);
}

regset_t& power_reg_set::operator&=(const regset_t &src) {
   // performs an in-place set intersection operation.  It's fast.
   theFloatRegSet.operator&=(((power_reg_set &)src).theFloatRegSet);
   theIntRegSet.operator&=(((power_reg_set &)src).theIntRegSet);
   theMiscRegSet.operator&=(((power_reg_set &)src).theMiscRegSet);

   return *this;
}

regset_t& power_reg_set::operator&=(const reg_t &r) {
   // performs an in-place set intersection operation.  It's fast.
   const power_reg_set src((power_reg &)r);
   return operator&=(src);
}

regset_t& power_reg_set::operator-=(const regset_t &src) {
   // performs an in-place set difference operation.  It's fast.
   // Set difference is the intersection of 'this' and the set-negation of 'src'
   // Thus, it's the bitwise-and of 'bits' and '~src.bits'

   theFloatRegSet.operator-=(((power_reg_set &)src).theFloatRegSet);
   theIntRegSet.operator-=(((power_reg_set &)src).theIntRegSet);
   theMiscRegSet.operator-=(((power_reg_set &)src).theMiscRegSet);
   
   return *this;
}

regset_t& power_reg_set::operator-=(const reg_t &r) {
   const power_reg_set src((power_reg &)r);
   return operator-=(src);
}

power_reg_set power_reg_set::operator|(const power_reg_set &src) const {
   power_reg_set result = *this;
   return (result |= src);
}

power_reg_set power_reg_set::operator|(const power_reg &r) const {
   power_reg_set result = *this;
   return (result |= r);
}

power_reg_set power_reg_set::operator+(const power_reg_set &src) const {
   return operator|(src);
}

power_reg_set power_reg_set::operator+(const power_reg &r) const {
   return operator|(r);
}



power_reg_set power_reg_set::operator&(const power_reg_set &src) const {
   power_reg_set result = *this;
   return (result &= src);
}

power_reg_set power_reg_set::operator&(const power_reg &r) const {
   power_reg_set result = *this;
   return (result &= r);
}

power_reg_set power_reg_set::operator-(const power_reg_set &src) const {
   power_reg_set result = *this;
   result -= src;
   return result;
}

power_reg_set power_reg_set::operator-(const power_reg &r) const {
   power_reg_set result = *this;
   result -= r;
   return result;
}

power_reg_set& power_reg_set::operator-=(const power_reg_set &src) {
   // performs an in-place set difference operation.  It's fast.
   // Set difference is the intersection of 'this' and the set-negation of 'src'
   // Thus, it's the bitwise-and of 'bits' and '~src.bits'

   theFloatRegSet.operator-=(src.theFloatRegSet);
   theIntRegSet.operator-=(src.theIntRegSet);
   theMiscRegSet.operator-=(src.theMiscRegSet);
   
   return *this;
}

power_reg_set& power_reg_set::operator-=(const power_reg &r) {
   const power_reg_set src(r);
   return operator-=(src);
}

power_reg power_reg_set::expand1IntReg() const {
   assert(!theIntRegSet.isempty());
   assert(theFloatRegSet.isempty());
   assert(theMiscRegSet.isempty());

   return theIntRegSet.expand1();
}

power_reg power_reg_set::expand1FloatReg() const {
   assert(!theFloatRegSet.isempty());
   assert(theIntRegSet.isempty());
   assert(theMiscRegSet.isempty());

   return theFloatRegSet.expand1();
}

power_reg power_reg_set::expand1() const {
   if (!theIntRegSet.isempty())
      return theIntRegSet.expand1();
   else if (!theFloatRegSet.isempty())
      return expand1FloatReg();
   else
      return theMiscRegSet.expand1();
}

unsigned power_reg_set::count() const {
   // return the # of elems in the set.
   return countIntRegs() + countFPRegs() + countMiscRegs();
}

unsigned power_reg_set::countIntRegs() const {
   return theIntRegSet.count();
}

unsigned power_reg_set::countFPRegs() const {
   return theFloatRegSet.count();
}

unsigned power_reg_set::countMiscRegs() const {
   return theMiscRegSet.count();
}

pdstring power_reg_set::disassx() const {
   pdstring result;

   const_iterator start = begin();
   const_iterator finish = end();
   
   for (const_iterator iter=start; iter != finish; ++iter) {
      const power_reg &reg =(const power_reg &) *iter;

      // add a space, except for the first time
      if (iter != start)
         result += " ";
      
      result += reg.disass();
   }

   return result;
}


/* ********************************************************************** */

power_reg_set operator~(const power_reg_set &src) {
   // a friend function of power_reg_set

   power_reg_set result(power_reg_set::raw,
                        ~(src.theIntRegSet.getRawBits()),
                        ~(src.theFloatRegSet.getRawBits()),
			~(src.theMiscRegSet.getRawBits()));
      // don't worry, theMiscRegSet raw ctor clears the padding bits for us

   return result;
}



/* ********************************************************************** */

// declare some static members.  Note: we mustn't use any static members
// when defining static members, since the order of construction is undefined!
// (Thus, perpetually-undefined static members, like raw, random, empty, etc. may be
// used at any time)
power_reg_set::Raw power_reg_set::raw;
power_reg_set::Random power_reg_set::random; // only for regression testing, of course
power_reg_set::SPR power_reg_set::spr_type;

power_reg_set::Empty power_reg_set::empty;
power_reg_set::Full power_reg_set::full;
power_reg_set::SingleIntReg power_reg_set::singleIntReg;
power_reg_set::SingleFloatReg power_reg_set::singleFloatReg;



const power_reg_set power_reg_set::emptyset(power_reg_set::raw, 0x0, 0x0, 0ULL);
const power_reg_set power_reg_set::fullset(power_reg_set::raw, 0xffffffff,
                                           0xffffffff,
                                           0xffffffffffffffffULL);
                                          


const power_reg_set power_reg_set::allFPs(power_reg_set::raw, 0x00000000,
                                          0xffffffff,
                                          0x0ULL);
const power_reg_set power_reg_set::allIntRegs(power_reg_set::raw, 0xffffffff, 0x0, 0x0ULL);

const power_reg_set power_reg_set::allMiscRegs(power_reg_set::raw, 0, 0,  0xffffffffffffffffULL);

const power_reg_set power_reg_set::allCRFields(power_reg_set::raw, 0, 0, 0x00ff000000000000ULL);
const power_reg_set power_reg_set::allXERArithFields(power_reg_set::raw, 0, 0, 0x3800000000000000ULL);
const power_reg_set power_reg_set::allXERFields(power_reg_set::raw, 0, 0, 0x3E00000000000000ULL);

const power_reg_set power_reg_set::regsThatIgnoreWrites(power_reg_set::raw, 0x0, 0x0, 0x0ULL);

//This set contains three registers on power:
//r0--too difficult to handle its different meaning in AST code
//r1--stack pointer
//r2--TOC pointer
const power_reg_set power_reg_set::always_live_set(power_reg_set::raw, 0x00000007, 0x0, 0x0ULL);

