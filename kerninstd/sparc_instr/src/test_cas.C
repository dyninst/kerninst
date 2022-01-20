// test_cas.C

#include "test_cas.h"
#include "sparc_instr.h"

static void test_building_cas(bool extended, sparc_reg rs1, sparc_reg rs2, sparc_reg rd,
                              const string &disass_cmp) {
   sparc_instr i(sparc_instr::cas, extended, rs1, rs2, rd);
     // pseudo-instr: assuming ASI_PRIMARY

   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   sparc_instr::disassemblyFlags fl(0x40000000);
   i.getInformation(&ru, NULL, &fl, &cfi, NULL);

   if (fl.result != disass_cmp) {
      cout << "expected \"" << disass_cmp << "\"" << endl;
      cout << "but got \"" << fl.result << "\"" << endl;
      assert(false);
   }
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   // definitely written: rd
   // definitely used before written: rs1, rs2
   // maybe used before written: rd
   // maybe written: M[rs1]

   if (rd == sparc_reg::g0)
      assert(ru.definitelyWritten.count() == 0);
   else {
      assert(ru.definitelyWritten.count() == 1);
      assert(ru.definitelyWritten.exists(rd));
   }

   assert(ru.definitelyUsedBeforeWritten.count() == (rs1 == rs2 ? 1U : 2U));
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   assert(ru.definitelyUsedBeforeWritten.exists(rs2));

   assert(ru.maybeUsedBeforeWritten.count() == 1);
   assert(ru.maybeUsedBeforeWritten.exists(rd));
   
   assert(ru.maybeWritten.count() == 0);
}

static void test_building_cas(bool extended, const string &idisass_cmp) {
   string disass_cmp = idisass_cmp;
   if (extended)
      disass_cmp += 'x';
   disass_cmp += ' ';

   for (unsigned rs1lcv=0; rs1lcv < 32; ++rs1lcv) {
      const sparc_reg rs1(sparc_reg::rawIntReg, rs1lcv);
      
      const string disass_cmp_rs1 = disass_cmp + '[' + rs1.disass() + "], ";

      for (unsigned rs2lcv=0; rs2lcv < 32; ++rs2lcv) {
         const sparc_reg rs2(sparc_reg::rawIntReg, rs2lcv);

         const string disass_cmp_rs2 = disass_cmp_rs1 + rs2.disass() + ", ";

         for (unsigned rdlcv=0; rdlcv < 32; ++rdlcv) {
            const sparc_reg rd(sparc_reg::rawIntReg, rdlcv);

            const string disass_cmp_rd = disass_cmp_rs2 + rd.disass();

            test_building_cas(extended, rs1, rs2, rd, disass_cmp_rd);
         }
      }
   }
}

void test_building_cas() {
   const string disass_cmp = "cas";
   test_building_cas(true, disass_cmp);
   test_building_cas(false, disass_cmp);
}
