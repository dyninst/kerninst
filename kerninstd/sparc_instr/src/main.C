// main.C
// sparc_instr test program

#include <iostream.h>
#include <fstream.h>
#include "sparc_reg.h"
#include "sparc_instr.h"
#include <stdio.h>

extern void do_all_tests();
extern bool do_1_interactive_test(const char *name);

// This one is only for sparc:
#ifdef sparc_sun_solaris2_4
extern void test_that_ulls_are_still_in_2_regs();
#endif

static void do_interactive_tests() {
   char buffer[1000];

   while (true) {
      if (NULL == fgets(buffer, 1000, stdin)) {
         // end of file
         return;
      }

      if (strlen(buffer) == 0)
         continue;
      
      buffer[strlen(buffer)-1] = '\0';

      if (strlen(buffer) == 0)
         continue;

      if (!do_1_interactive_test(buffer))
         cout << "unknown test: \"" << buffer << "\"" << endl;
   }
}

int main(int argc, char **argv) {
   // no args: test every instruction
   // 1 arg: "interactive": wait for input that tells us which insn to test
   // 1 arg: a hex number: disassemble that one instruction

   // This test is only for sparc:
#ifdef sparc_sun_solaris2_4
   test_that_ulls_are_still_in_2_regs(); // do_test.C
#endif
   
   if (argc == 1) {
      // no args: do all tests

      do_all_tests();
      return 0;
   }

   if (argc == 2 && 0==strcmp("interactive", argv[1])) {
      do_interactive_tests();
      return 0;
   }

   else {
      for (char **ptr = &argv[1]; *ptr; ++ptr) {
         unsigned long value = strtoul(*ptr, NULL, 16);
         //cout << "disassembly of raw value " << (void*)value << endl;
      
         sparc_instr::disassemblyFlags fl;
//      fl.constsAsDecimal = false;
         fl.pc = 0x1000;
//      fl.addressesAsPCrelative = false;
//      fl.v9 = true;
      
         sparc_instr i(value);
         try {
            i.getDisassembly(fl);
            cout << fl.result << endl;
         }
         catch(...) {
            cout << (void*)value << endl;
         }
         
      }
   }
}
