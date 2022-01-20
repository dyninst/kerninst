// main.C
// test program for sparc_reg_set

#include <iostream.h>
#include "sparc_reg_set.h"
#include "common/h/String.h"

static void test_empty() {
   sparc_reg_set set = sparc_reg_set::empty;
   sparc_reg_set::const_iterator iter=set.begin();
   assert(iter == set.end());
   assert(set.begin() == set.begin());
   assert(set.end() == set.end());
   assert(set.begin() == set.end());

   sparc_reg_set s2(set);
   assert(s2 == set);

   assert(set.isempty());
   assert(s2.isempty());

   assert(s2.begin() == s2.end());

   const sparc_reg_set negate_emptyset = ~sparc_reg_set::emptyset;
   assert(negate_emptyset == sparc_reg_set::fullset);
   
   assert(~sparc_reg_set::emptyset == sparc_reg_set::fullset);
   assert(~~sparc_reg_set::emptyset == sparc_reg_set::emptyset);

   assert(sparc_reg_set::emptyset == ~sparc_reg_set::fullset);
   assert(sparc_reg_set::fullset == ~~sparc_reg_set::fullset);
}

static void test_icc() {
   sparc_reg_set s2 = sparc_reg_set::empty;
   
   s2 += sparc_reg::reg_icc;
   assert(!s2.isempty());
   assert(s2.exists(sparc_reg::reg_icc));
   sparc_reg_set::const_iterator iter2 = s2.begin();
   assert(iter2 != s2.end());
   assert(*iter2 == sparc_reg::reg_icc);
   iter2++;
   assert(iter2 == s2.end());
   assert(s2.count() == 1);

   s2.clear();
   assert(!s2.exists(sparc_reg::reg_icc));
   assert(s2.isempty());
   assert(s2.begin() == s2.end());
   assert(s2.count() == 0);
}

static void test_icc_and_xcc() {
   sparc_reg_set s2 = sparc_reg_set::empty;
   
   s2 |= sparc_reg::reg_icc;
   s2 |= sparc_reg::reg_xcc;
   assert(s2.exists(sparc_reg::reg_icc));
   assert(s2.exists(sparc_reg::reg_xcc));
   assert(!s2.isempty());
   assert(s2.count() == 2);
   sparc_reg_set::const_iterator iter3 = s2.begin();
   assert(iter3 != s2.end());
   assert(*iter3 == sparc_reg::reg_icc);
   iter3++;
   assert(iter3 != s2.end());
   assert(*iter3 == sparc_reg::reg_xcc);
   iter3++;
   assert(iter3 == s2.end());

   s2 -= sparc_reg::reg_icc;
   assert(!s2.exists(sparc_reg::reg_icc));
   assert(s2.exists(sparc_reg::reg_xcc));
   assert(!s2.isempty());
   sparc_reg_set::const_iterator iter4 = s2.begin();
   assert(iter4 != s2.end());
   assert(*iter4 == sparc_reg::reg_xcc);
   iter4++;
   assert(iter4 == s2.end());

   s2 -= sparc_reg::reg_xcc;
   assert(!s2.exists(sparc_reg::reg_icc));
   assert(!s2.exists(sparc_reg::reg_xcc));
   assert(s2.isempty());
   sparc_reg_set::const_iterator iter5 = s2.begin();
   assert(iter5 == s2.end());
   assert(iter5 == s2.begin());
}

static void test_empty_full() {
   sparc_reg_set s1 = sparc_reg_set::emptyset;
   sparc_reg_set s2 = sparc_reg_set(sparc_reg_set::raw, 0, 0, 0);
   assert(s1 == s2);

   assert(s1 != sparc_reg_set::fullset);
   assert(s1 != sparc_reg_set::allGs);
   assert(s1 != sparc_reg_set::allOs);
   assert(s1 != sparc_reg_set::allLs);
   assert(s1 != sparc_reg_set::allIs);
   assert(s1 != sparc_reg_set::allIntRegs);
   assert(s1 != sparc_reg_set::allFPs);
   assert(s1 != sparc_reg_set(sparc_reg_set::singleIntReg, 0));
   assert(s1 == sparc_reg_set::Empty());
   
   sparc_reg_set s3 = s1;
   assert(s1 == s3);
   
   s3 = s2;
   assert(s3 == s1);
   
   assert(s1.isempty());

   sparc_reg_set full(sparc_reg_set::fullset);
   assert(s1 != full);
   
   full.clear();
   assert(s1 == full);


   assert(sparc_reg_set::emptyset == sparc_reg_set(sparc_reg_set::empty));
   assert(sparc_reg_set::fullset == sparc_reg_set(sparc_reg_set::full));

   assert(sparc_reg_set::emptyset == ~sparc_reg_set::fullset);

   cout << "full set is " << sparc_reg_set::fullset.disassx() << endl;
}

static void test_single_int_reg() {
   for (unsigned num=0; num < 32; ++num) {
      sparc_reg r(sparc_reg::rawIntReg, num);
      sparc_reg_set rset(r);
      
      assert(rset == r);

      assert(rset.exists(r));
      assert(rset.existsIntReg(num));

      sparc_reg_set cmpset(sparc_reg_set::empty);
      cmpset += r;
      assert(cmpset == rset);
      cmpset.clear();
      assert(cmpset != rset);
      
      cmpset |= r;
      assert(cmpset == rset);
      cmpset.clear();
      assert(cmpset != rset);

      cmpset += r;
      cmpset &= r;
      assert(cmpset == rset);
      cmpset.clear();
      assert(cmpset != rset);

      cmpset |= r;
      cmpset &= r;
      assert(cmpset == rset);
      cmpset.clear();
      assert(cmpset != rset);

      cmpset += r;
      cmpset -= r;
      assert(cmpset == sparc_reg_set::empty);

      cmpset |= r;
      cmpset -= r;
      assert(cmpset == sparc_reg_set::empty);

      assert(rset == (sparc_reg_set::emptyset | r));
      assert(rset == (sparc_reg_set::emptyset | rset));
      assert(rset == (sparc_reg_set::emptyset + r));
      assert(rset == (sparc_reg_set::emptyset + rset));
      assert(rset == (rset & rset));
      assert(rset == (rset & r));
      assert(sparc_reg_set::emptyset == rset - rset);
      assert(sparc_reg_set::emptyset == rset - r);

      assert(rset.count() == 1);
      assert(rset.countIntRegs() == 1);
      assert(rset.countFPRegsAsSinglePrecision() == 0);
      assert(rset.countMiscRegs() == 0);
   }
}

static void test_random_int_regs() {
   for (unsigned lcv=0; lcv < 1000; ++lcv) {
      unsigned int_bits = (unsigned)random(); // initially returns a long
      sparc_reg_set set(sparc_reg_set::raw, int_bits, 0, 0);

      assert(set.getRawIntBits() == int_bits);
      assert(set.getRawFloatBits() == 0);
      assert(set.getRawMiscBits() == 0);
      
      for (unsigned floatlcv=0; floatlcv < 64; ++floatlcv)
         assert(!set.existsFpRegSinglePrecision(floatlcv));

      for (unsigned floatlcv=0; floatlcv < 32; ++floatlcv)
         assert(!set.existsFpReg(floatlcv, 1)); // 1 --> precision

      for (unsigned floatlcv=0; floatlcv < 64; floatlcv+=2)
         assert(!set.existsFpReg(floatlcv, 2)); // 2 --> precision

      for (unsigned floatlcv=0; floatlcv < 64; floatlcv+=4)
         assert(!set.existsFpReg(floatlcv, 4)); // 4 --> precision

      assert(!set.existsIcc());
      assert(!set.existsXcc());
      assert(!set.existsFcc(0));
      assert(!set.existsFcc(1));
      assert(!set.existsFcc(2));
      assert(!set.existsFcc(3));
      assert(!set.existsPC());
      assert(!set.existsGSR());
      assert(!set.existsASI());
      assert(!set.existsPIL());

      sparc_reg_set negation_set = ~set; // should contain all float & misc regs
      assert((negation_set | set) == sparc_reg_set::fullset);
      assert(negation_set + set == sparc_reg_set::fullset);
      assert((negation_set & set) == sparc_reg_set::emptyset);
      assert(negation_set - negation_set == sparc_reg_set::emptyset);
      assert(set - set == sparc_reg_set::emptyset);
      assert(set - negation_set == set);
      assert(negation_set - set == negation_set);
      assert(negation_set != set);
      assert(set.count() + negation_set.count() == sparc_reg::total_num_regs);
      
      negation_set += sparc_reg_set::allIntRegs; // now it should be the full set
      assert(negation_set == sparc_reg_set::fullset);
      assert(~negation_set == sparc_reg_set::emptyset);
   }
}

static void test_asi() {
   sparc_reg_set set(sparc_reg::reg_asi);
   assert(set.count() == 1);
   assert(set.exists(sparc_reg::reg_asi));
   assert(set.existsASI());
   assert(set.disassx() == pdstring("%asi"));

   assert(sparc_reg_set::fullset.exists(sparc_reg::reg_asi));
   assert(!sparc_reg_set::emptyset.exists(sparc_reg::reg_asi));
}

static void test_all_misc_regs() {
   assert(sparc_reg_set::allMiscRegs.exists(sparc_reg::reg_icc));
   assert(sparc_reg_set::allMiscRegs.exists(sparc_reg::reg_xcc));
   assert(sparc_reg_set::allMiscRegs.exists(sparc_reg::reg_fcc0));
   assert(sparc_reg_set::allMiscRegs.exists(sparc_reg::reg_fcc1));
   assert(sparc_reg_set::allMiscRegs.exists(sparc_reg::reg_fcc2));
   assert(sparc_reg_set::allMiscRegs.exists(sparc_reg::reg_fcc3));
   assert(sparc_reg_set::allMiscRegs.exists(sparc_reg::reg_pc));
   assert(sparc_reg_set::allMiscRegs.exists(sparc_reg::reg_gsr));
   assert(sparc_reg_set::allMiscRegs.exists(sparc_reg::reg_asi));
   assert(sparc_reg_set::allMiscRegs.exists(sparc_reg::reg_pil));
}

static void test_minus() {
   sparc_reg_set s = sparc_reg_set::allOs;
   s -= sparc_reg::o0;
   s -= sparc_reg::o1;
   s -= sparc_reg::o2;
   s -= sparc_reg::o3;
   s -= sparc_reg::o4;
   s -= sparc_reg::o5;
   s -= sparc_reg::o6;
   s -= sparc_reg::o7;
   assert(s == sparc_reg_set::emptyset);

   s = sparc_reg_set::allOs;
   assert(sparc_reg_set::emptyset == s - sparc_reg::o0
          - sparc_reg::o1
          - sparc_reg::o2
          - sparc_reg::o3
          - sparc_reg::o4
          - sparc_reg::o5
          - sparc_reg::o6
          - sparc_reg::o7
          );
}

static void test_regsThatIgnoreWrites() {
   const sparc_reg_set &s = sparc_reg_set::regsThatIgnoreWrites;
   assert(s.count() == 1);
   assert(s.countIntRegs() == 1);
   assert(s.existsIntReg(0));
   assert(s.exists(sparc_reg::g0));
   assert(s.exists(sparc_reg_set(sparc_reg::g0)));
}


int main(int, char **) {
   // TO DO -- test the new exists() method!!!!!!!!!!!!!!! (that takes in another
   // sparc_reg_set)

   assert(sizeof(sparc_reg_set) == 8 + 4 + 4);

   test_empty();

   test_all_misc_regs();
   
   test_icc();

   test_icc_and_xcc();

   test_empty_full();

   test_single_int_reg();

   test_random_int_regs();

   test_asi();

   test_minus();

   test_regsThatIgnoreWrites();
   
   cout << "success" << endl;
}
