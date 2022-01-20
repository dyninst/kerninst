// main.C
// immediate test program

#include <stdlib.h>
#include <iostream.h>
#include <stdio.h>
#include <inttypes.h>
#include "uimm.h"
#include "simm.h"
#include "bitUtils.h"

bool done = false;

char buffer[2048]; // overflow is certainly possible, but we don't care too much
// about that in this test program (we're not going to run fuzz on it!)

void read_and_process_1_line(FILE *infile) {
   gets(buffer); // reads until \n, which does NOT get included in the array

   // parse input line

//   cout << buffer << endl << flush;
   printf("%s\n", buffer);
}

static void test_bits(unsigned num) {
   while (num--) {
      uint32_t value = (uint32_t)random();

      const unsigned val1 = random() % 32;
      const unsigned val2 = random() % 32;
      const uint32_t loBit = (val1 < val2 ? val1 : val2);
      const uint32_t hiBit = (val1 > val2 ? val1 : val2);
      assert(hiBit >= loBit);
      
      const uint32_t savedBits = getBits(value, loBit, hiBit);
      assert(replaceBits(value, loBit, hiBit, savedBits) == value);
      
      const unsigned numBits = hiBit - loBit + 1;
      const uint32_t replacementBits = random() & ((1U << numBits)-1);

      const uint32_t newValue = replaceBits(value, loBit, hiBit, replacementBits);
      assert(getBits(newValue, loBit, hiBit) == replacementBits);
      if (loBit > 0)
         assert(getBits(newValue, 0, loBit-1) == getBits(value, 0, loBit-1));
      if (hiBit < 31)
         assert(getBits(newValue, hiBit+1, 31) == getBits(value, hiBit+1, 31));
   }
}

void test1() {
   // add %g1, 0x20, %o0:
   unsigned insn_bits = 0;
    
   uimmediate<2> hi2bits = 2;
   rollin(insn_bits, hi2bits);
   
   uimmediate<5> rdbits = 8; // rd: %o0
   rollin(insn_bits, rdbits);
   
   uimmediate<6> op3bits = 0x0; // add
   rollin(insn_bits, op3bits);
   
   uimmediate<5> rs1bits = 1; // rs1: %g1
   rollin(insn_bits, rs1bits);
   
   uimmediate<1> i(1);
   rollin(insn_bits, i);
   
   simmediate<13> simm13(32); // simm13: 0x20
   rollin(insn_bits, simm13);
   
   assert(insn_bits == 0x90006020);
}

void test_uimms() {
   // exhaustively try out the range of uimm13's simm13's (there aren't many, so
   // an exhaustive test will work just fine)

   unsigned max_allowable_uimm13 = (1U << 13) - 1;

   for (unsigned num=0; num <= max_allowable_uimm13; ++num) {
      uimmediate<13> uimm13(num);

      uimmediate<13> uimm13_copy = uimm13;

      unsigned val = 0;
      val |= uimm13;
      
      unsigned val_copy = 0;
      val_copy |= uimm13_copy;
      
      assert(val == val_copy);
      assert(num == val);

      assert((0ul | uimm13) == (0ul | uimm13_copy));
      assert((0ul | uimm13) == (0ul | num));
      assert((0ul | uimm13) == num);
   }
}

void test_simms() {
   // exhaustively try out the range of simm13's (there aren't many, so
   // an exhaustive test will work just fine)

   // simm13: -4096 to +4095
   long min_allowable_simm13 = -4096;
   long max_allowable_simm13 = 4095;

   for (long num=min_allowable_simm13; num <= max_allowable_simm13; ++num) {
      simmediate<13> simm13(num);
      simmediate<13> simm13_copy = simm13;

      unsigned raw = 0;
      raw |= simm13;
      
      unsigned raw_copy = 0;
      raw_copy |= simm13_copy;
      
      assert(raw == raw_copy);
      
      // can't assert that raw == num, since raw won't be "properly" sign-extended
      // But if we only examine the low 13 bits of num, then we should get an equal
      const unsigned low13mask_for_ul = (1UL << 13) - 1;
      const long low13mask_for_l = (1UL << 13) - 1;

      assert((raw & low13mask_for_ul) == (raw_copy & low13mask_for_l));
      assert((raw & low13mask_for_ul) == (num & low13mask_for_l));
      
      assert(((0ul | simm13) & low13mask_for_ul) ==
             ((0ul | simm13_copy) & low13mask_for_ul));
      assert(((0ul | simm13) & low13mask_for_ul) == ((0ul | num) & low13mask_for_l));
   }
}

static void test_randoms() {
   for (unsigned lcv=0; lcv < 10000000; ++lcv) {
      int val = simmediate<13>::getRandomValue();
      assert(val >= simmediate<13>::getMinAllowableValue());
      assert(val <= simmediate<13>::getMaxAllowableValue());
      
      unsigned val2 = uimmediate<13>::getRandomValue();
      assert(val2 >= uimmediate<13>::getMinAllowableValue());
      assert(val2 <= uimmediate<13>::getMaxAllowableValue());
   }
}

int main(int argc, char **argv) {
//       while (!done)
//          read_and_process_1_line(stdin);

//       return 0;
     
   // Pretend that we're building up a sparc instruction.
   // Tests rollin for both uimmediate's and simmediate's
   // (Use sparcinstrtest to verify the instructions)

   test_bits(100000);
   
   test1();
   
   test_uimms();
   test_simms();

   test_randoms();
   
   cout << "success" << endl;

   return 0;
   
//   sleep(5);
   

   
//     simmediate<13> i13pos(40);
//     simmediate<13> i13neg(-40);

//     unsigned long myvalue = 0;
//     myvalue = myvalue | i13pos;
//     cout << "myvalue positive is " << (void*)myvalue << endl;
   
//     myvalue = 0;
//     myvalue = myvalue | i13neg;
//     cout << "myvalue after OR with negative imm13 is " << (void*)myvalue << endl;
//     cout << "were the high bits left at 0?" << endl;

//     i13pos = 41;
//     myvalue = 0xaaaaaaaa; // 10101010...
//     myvalue |= i13pos;
//     cout << "myvalue positive is " << (void*)myvalue << endl;
//     cout << "were the high bits left alone?" << endl;

//     myvalue = 0xaaaaaaaa; // 10101010...
//     myvalue |= i13neg;
//     cout << "myvalue negative is " << (void*)myvalue << endl;
//     cout << "were the high bits left alone?" << endl;

//     myvalue = 0;
//     uimmediate<13> ui(200);
//     myvalue |= ui;
//     cout << "myvalue is " << (void*)myvalue << endl;

//     cout << "bye" << endl;
}
