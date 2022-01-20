// main.C

#include <iostream.h>
#include "mbdf_window.h"
#include "sparcTraits.h"
#include "sparc_reg_set.h"

typedef mbdf_window<sparcTraits> window;

static void test_pass() {
   window win(sparc_reg_set::fullset, sparc_reg_set::empty);
      // pass everything

   window win2(sparc_reg_set::empty, sparc_reg_set::empty);
   win2.passSet(sparc_reg_set::fullset);
   assert(win2 == win);
   
   win2.start(sparc_reg::g0);
   assert(win2 != win);
   
   win2.pass(sparc_reg::g0);
   assert(win2 == win);
   
   assert(win.getStarts() == sparc_reg_set::empty);
   assert(win.getStops() == sparc_reg_set::empty);
   
   window win3(true, false); // another way to create an all-pass set
   assert(win3 == win);
   assert(win3 == win2);
   
   for (unsigned lcv=0; lcv < 10000; ++lcv) {
      // create a random sparc_reg_set
      sparc_reg_set set(sparc_reg_set::random);

      // ... it should be unchanged when applied to an all-pass set
      assert(set == win.apply(set));
   }
}

static void test_stop() {
   window win(sparc_reg_set::empty, sparc_reg_set::empty);
      // stop everything

   window win2(sparc_reg_set::empty, sparc_reg_set::fullset); // start everything
   win2.stopSet(sparc_reg_set::fullset);
   assert(win2 == win);
   
   win2.start(sparc_reg::g0);
   assert(win2 != win);
   
   win2.stop(sparc_reg::g0);
   assert(win2 == win);
   
   assert(win.getStarts() == sparc_reg_set::empty);
   assert(win.getStops() == sparc_reg_set::fullset);
   
   window win3(false, false); // another way to create an all-stop set
   assert(win3 == win);
   assert(win3 == win2);
   
   for (unsigned lcv=0; lcv < 10000; ++lcv) {
      // create a random sparc_reg_set
      sparc_reg_set set(sparc_reg_set::random);

      // ... it should all get squashed when applied to 'win'
      assert(win.apply(set) == sparc_reg_set::empty);
   }
}

static void test_start() {
   window win(sparc_reg_set::empty, sparc_reg_set::fullset);
      // stop everything

   window win2(sparc_reg_set::empty, sparc_reg_set::empty); // stop everything
   win2.startSet(sparc_reg_set::fullset);
   assert(win2 == win);
   
   win2.stop(sparc_reg::g0);
   assert(win2 != win);
   
   win2.start(sparc_reg::g0);
   assert(win2 == win);
   
   assert(win.getStarts() == sparc_reg_set::fullset);
   assert(win.getStops() == sparc_reg_set::empty);
   
   window win3(false, true); // another way to create an all-start set
   assert(win3 == win);
   assert(win3 == win2);
   
   for (unsigned lcv=0; lcv < 10000; ++lcv) {
      // create a random sparc_reg_set
      sparc_reg_set set(sparc_reg_set::random);

      // ... when applied to 'win', it should be all true
      assert(win.apply(set) == sparc_reg_set::fullset);
   }
}

static void test_compose() {
   window win(true, false); // all-pass
   window win2(win);
   
   assert(win.compose(win2) == win);
   assert(win2.compose(win) == win);

   win = window(false, true); // all-start
   win2 = window(false, false); // all-stop

   assert(win == win.compose(win2));
   assert(win2 == win2.compose(win));
}

int main(int, char **) {
   cout << "pass..." << flush;
   test_pass();
   cout << "done" << endl;

   cout << "stop..." << flush;
   test_stop();
   cout << "done" << endl;

   cout << "start..." << flush;
   test_start();
   cout << "done" << endl;

   cout << "compose..." << flush;
   test_compose();
   cout << "done" << endl;

   cout << "success" << endl;
}


