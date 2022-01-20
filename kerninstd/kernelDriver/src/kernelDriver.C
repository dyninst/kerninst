// kernelDriver.C

// TO DO -- /dev/kerninst needs, in its undo state, a flag stating whether 
//          a given address is code.  Because if so, it needs to iflush after 
//          writing to, or undoing, that address!!!

#include "kernelDriver.h"

#include <errno.h>
#include <sched.h>
#include <sys/mman.h> // PROT_READ etc. for mmap
#include <sys/utsname.h>

#include "util/h/hashFns.h" // addrHash4()
#include "util/h/rope-utils.h" // addr2hex()
#include "util/h/mrtime.h"
#include "util/h/out_streams.h"

#ifdef sparc_sun_solaris2_7
#include <sys/processor.h> // processor_info(), processor_bind()
#include <sys/sysmacros.h> // roundup()
#include "sparcv8plus_abi.h"
#include "sparcv9_abi.h"
#elif defined(i386_unknown_linux2_4)
#include <sys/ioctl.h> // ioctl()
#include <sys/param.h> // roundup()
#include <sys/utsname.h> // uname()
#include "x86_instr.h"
#elif defined(ppc64_unknown_linux2_4)
#include <sys/ioctl.h> // ioctl()
#include <sys/param.h> // roundup()
#include "power_instr.h"
#endif

#include "out_streams.h"
#include "kerninstdClient.xdr.h"

#define tenSecondsAsNanoSecs 10000000000ULL

#ifdef sparc_sun_solaris2_7

void undoablePokeInfo1Loc::push(uint32_t previous_value, uint32_t new_value) {
   // Make sure that the caller isn't ignorant of the current state of affairs...
   // that it knows the currently poked value
   if (replacementStack.size())
      assert(replacementStack.back().second == previous_value);
   else
      assert(origval == previous_value);

   replacementStack += make_pair(previous_value, new_value);
}

void undoablePokeInfo1Loc::
changeTopOfStack(uint32_t new_value_was_this, // we'll assert this
                 uint32_t new_value_is_now_this // we'll change it to this
                 ) {
   assert(replacementStack.size());
   pair<uint32_t, uint32_t> &info = replacementStack.back();
   assert(info.second == new_value_was_this);
   info.second = new_value_is_now_this;
}

bool undoablePokeInfo1Loc::
pop(uint32_t previous_current_value, uint32_t new_current_value) {
   // returns true iff now empty stack
   assert(replacementStack.size());

   assert(replacementStack.back().second == previous_current_value);
   assert(replacementStack.back().first == new_current_value);
   replacementStack.pop_back();

   if (replacementStack.size() == 0) {
      // empty stack
      assert(new_current_value == origval);
      return true;
   }
   else {
      // not yet empty stack
      assert(replacementStack.back().second == new_current_value);
      return false;
   }
}

#elif defined(i386_unknown_linux2_4)  || defined(ppc64_unknown_linux2_4)

void undoablePokeInfo1Loc::push(const char *previous_value, const char *new_value) {
   // Make sure that the caller isn't ignorant of the current state of 
   // affairs ... that it knows the currently poked value
   if (replacementStack.size()) {
      if(memcmp(replacementStack.back().second, previous_value, length) != 0)
         assert(!"undoablePokeInfo1Loc::push() prev_val mismatch\n");
   }
   else {
      if(memcmp(origval, previous_value, length) != 0)
         assert(!"undoablePokeInfo1Loc::push() prev_val mismatch\n");
   }

   char *oldv = (char *) malloc(length);
   char *newv = (char *) malloc(length);
   assert((oldv != NULL) & (newv != NULL));
   memcpy(oldv, previous_value, length);
   memcpy(newv, new_value, length);

   replacementStack += make_pair(oldv, newv);
}

void undoablePokeInfo1Loc::
changeTopOfStack(const char *new_value_was_this, // we'll assert this
                 const char *new_value_is_now_this // we'll change it to this
                 ) {
   assert(replacementStack.size());
   pair<const char *, const char *> &info = replacementStack.back();
   if(memcmp(info.second, new_value_was_this, length) != 0)
      assert(!"undoablePokeInfo1Loc::changeTopOfStack() val mismatch\n");
   free(const_cast<char*>(info.second));
   char *newv = (char *) malloc(length);
   assert(newv != NULL);
   memcpy(newv, new_value_is_now_this, length);
   info = make_pair(info.first, newv);
}

bool undoablePokeInfo1Loc::
pop(const char *previous_current_value, const char *new_current_value) {
   // returns true iff now empty stack
   assert(replacementStack.size());

   const pair<const char*, const char*> &top = replacementStack.back();

   if(memcmp(top.second, previous_current_value, length) != 0) {
      assert(!"undoablePokeInfo1Loc::pop() prev_val mismatch\n");
   }
   if(memcmp(top.first, new_current_value, length) != 0) {
      assert(!"undoablePokeInfo1Loc::pop() new_val mismatch\n");
   }
   
   // the following call to pop_back will delete the pair at the top 
   // of the stack, but doesn't actually delete the char*'s it holds,
   // so we do so manually
   free(const_cast<char*>(top.first));
   free(const_cast<char*>(top.second));
   replacementStack.pop_back();

   if (replacementStack.size() == 0) {
      // empty stack
      if(memcmp(new_current_value, origval, length) != 0)
         assert(!"undoablePokeInfo1Loc::pop() val mismatch\n");
      return true;
   }
   else {
      // not yet empty stack
      if(memcmp(replacementStack.back().second, new_current_value, length) != 0)
         assert(!"undoablePokeInfo1Loc::pop() val mismatch\n");
      
      return false;
   }
}

#endif


// ----------------------------------------------------------------------

#ifdef i386_unknown_linux2_4
extern unsigned ud2_bug_size; // defined in x86_instr.C
#endif

kernelDriver::kernelDriver(const char *drivername) :
   allocatedKernelMemBlocks(addrHash4),
   theUndoablePokes(addrHash4)
{
   driverfd = open(drivername, O_RDWR);
   if (driverfd == -1) {
      cout << "Failed to open kerninst driver using name \"" << drivername
           << "\"." << endl << flush;
      perror("");
      cout << "Check path name; check that the driver has been installed;"
           << endl;
      cout << "make sure you're running as su; make sure the driver is not"
           << endl
           << "already open by another process" << endl;
      throw OpenFailed();
   }

#ifdef sparc_sun_solaris2_7
   // set nucleus bounds
   struct kernInstAddressMap result;
#ifdef PURE_BUILD
   // avoid a purify UMR/UMC:
   memset(&result, '\0', sizeof(result));
#endif
   if (-1 == ioctl(driverfd, KERNINST_GET_ADDRESS_MAP, &result))
      assert(false && "kerninst_get_address_map failed");

   theNucleusDetector.setBounds(result.startBelow, result.endBelow,
				result.startNucleus, result.endNucleus);
   dout << "kerninstd: nucleus text bounds are ";
   const pair<kptr_t, kptr_t> bounds = theNucleusDetector.queryNucleusBounds();
   dout << addr2hex(bounds.first) << "-" << addr2hex(bounds.second) << endl;
#elif defined(i386_unknown_linux2_4)
   ud2_bug_size = 2;
   if (-1 == ioctl(driverfd, KERNINST_GET_BUG_UD2_SIZE, &ud2_bug_size))
      assert(!"ioctl KERNINST_GET_BUG_UD2_SIZE failed");
#endif

   kmemfd = open("/dev/kmem", O_RDWR);
   if (kmemfd == -1) {
      perror("open of kmem");
      assert(false && "could not open /dev/kmem for writing");
   }

   memfd = open("/dev/mem", O_RDWR);
   if (memfd == -1)
      assert(false && "could not open /dev/mem for writing");

   long num_found = 0, num_reported;
   
   if ((num_reported = sysconf(_SC_NPROCESSORS_CONF)) <= 0) {
       perror("sysconf error");
       assert(false);
   }

#ifdef sparc_sun_solaris2_7
   for (long i=0; num_found < num_reported && i < 65536; i++) {
       // i < 65536 is used to avoid an infinite loop if the
       // reported number of processors is greater than we detect
       processor_info_t pi;
       if (processor_info(i, &pi) < 0) {
	   if (errno == EINVAL) {
	       dout << "cpu" << i << ": " << "not present\n";
	   }
	   else {
	       perror("processor_info error");
	       assert(false);
	   }
       }
       else {
	   num_found++;
	   cpuinfo += pair<unsigned,unsigned>(i, pi.pi_clock);
	   dout << "cpu" << i << ": " << pi.pi_clock << " MHz\n";
       }
   }
#elif defined(i386_unknown_linux2_4)  || defined(ppc64_unknown_linux2_4)
   FILE *cpuinfo_f = fopen( "/proc/cpuinfo", "r" );
   if(cpuinfo_f == NULL) 
       assert(!"kernelDriver: unable to open /proc/cpuinfo\n");
#ifdef i386_unknown_linux2_4
   bool got_family = false;
   bool got_model = false;
   unsigned cpu_family = 0;
   unsigned cpu_model = 0;
#endif
   while(!feof(cpuinfo_f)) {
       char strbuf[150];
       char *res = fgets(strbuf, 148, cpuinfo_f);
       double cpumhz = 0.0;
#ifdef i386_unknown_linux2_4
       unsigned cpufamily;
       unsigned cpumodel;
       if(res != NULL) {
           int totassigned = sscanf(res, "cpu MHz : %lf",&cpumhz);
#else //ppc64
       if (res != NULL) {
           int totassigned = sscanf(res, "clock		 : %lf",&cpumhz);
#endif
	   if(totassigned == 1) {
	       cpuinfo += pair<unsigned,unsigned>(num_found,(long)cpumhz);
	       dout << "cpu" << num_found++ << ": " << cpumhz << " MHz\n";
	   }
#ifdef i386_unknown_linux2_4
           else if(! got_family) {
              totassigned = sscanf(res, "cpu family : %d",&cpufamily);
              if(totassigned == 1) {
                 got_family = true;
                 cpu_family = cpufamily;
              }
           }
           else if(! got_model) {
              totassigned = sscanf(res, "model : %d",&cpumodel);
              if(totassigned == 1) {
                 got_model = true;
                 cpu_model = cpumodel;
              }
           }
           if(got_family && got_model) {
              cpumodelinfo += pair<unsigned, unsigned>(cpu_family, cpu_model);
              got_family = false;
              got_model = false;
           }
#endif
       }
   }
   fclose(cpuinfo_f);
   if(num_found < num_reported) {
       while(num_found++ != num_reported)
	   cpuinfo += pair<unsigned, unsigned>(num_found-1, 0);
   }
#endif

#ifdef sparc_sun_solaris2_7
   // disableTickProtectionAllCPUs();
#endif
}

kernelDriver::~kernelDriver() {
   unsigned sz = theUndoablePokes.size();
   if (sz > 0) {
      cout << "kernelDriver dtor note: there are " << sz << " undoable pokes still registered.  /dev/kerninst will have to clean them up." << endl;
   }

   sz = allocatedKernelMemBlocks.size();
   if (sz > 0) {
      cout << "kernelDriver dtor note: there are " << sz << " kernel blocks still allocated.  /dev/kerninst will have to clean them up." << endl;
   }
   
   (void)close(driverfd);
   (void)close(kmemfd);
   (void)close(memfd);
}

#ifdef sparc_sun_solaris2_7
pair<kptr_t, kptr_t> kernelDriver::queryNucleusTextBounds() const {
   return theNucleusDetector.queryNucleusBounds();
}
#endif

// Perform a given ioctl on every CPU in the system. Do this by
// cycling through the CPUs, binding ourselves to each one and
// performing the ioctl there.
void kernelDriver::ioctl_all_cpus(int fildes, int request, void *argp)
{
#ifdef sparc_sun_solaris2_7
    unsigned num_cpus = cpuinfo.size();
    for (unsigned i=0; i<num_cpus; i++) {
	unsigned cpuid = cpuinfo[i].first;

	// Bind kerninstd to each CPU
	if (processor_bind(P_PID, P_MYID, cpuid, NULL) < 0) {
	    perror("WARNING: processor_bind failed, continuing");
	    // assert(false); // How about off-line processors?
	}
	else {
	    // Hope that processor_bind will take effect right away and
	    // we get scheduled on the new CPU (kernel code suggests so)
	    // Otherwise we would have to do
	    // sched_yield();
	    if (ioctl(fildes, request, argp) < 0) {
		perror("ioctl_all_cpus() failed");
		assert(false);
	    }
	}
    }
    // Reset the binding. Should we restore the original one?
    if (processor_bind(P_PID, P_MYID, PBIND_NONE, NULL) < 0) {
	perror("processor_bind error, aborting");
	assert(false);
    }
#elif defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4)
    /* MJB FIXME - is there a way to set cpu affinity in 2.4 kernels? */
    assert(!"kernelDriver::ioctl_all_cpus nyi");
#endif
}

#ifdef sparc_sun_solaris2_7
// Enable user-level access to the %tick register on every CPU in the system
void kernelDriver::disableTickProtectionAllCPUs() 
{
    // The way we disable %tick protection is unsafe: it freezes some
    // 32-bit Solaris 9 boxes. Not sure why, but here is a guess:
    // Suppose somebody reads %tick, then we come in and disable tick
    // protection (flip the high bit of %tick), then that guy reads
    // %tick again and computes delta, which turns out negative, which
    // is not something he is prepared to handle.
    assert(false); // Make sure it is not called from anywhere
    ioctl_all_cpus(driverfd, KERNINST_DISABLE_TICK_PROTECTION, NULL);
}

bool kernelDriver::waitForPokeToTakeEffect(kptr_t /*addr*/, 
					   uint32_t /*newVal*/,
                                           mrtime_t /*maxNanoSecWaitTime*/) const {
   return true;

#ifdef NOTYET
   const mrtime_t time1 = getmrtime();

   while (peek1Word(addr) != newVal) {
      if (getmrtime()-time1 > maxNanoSecWaitTime)
         return false;
   }
   
   const mrtime_t time2 = getmrtime();
   const mrtime_t totalTime = time2-time1;

   cout << "Took " << totalTime << " nsecs for write to take effect" << endl;

   return true; // success
#endif //NOTYET
}

void kernelDriver::poke1PermanentNucleusWord(kptr_t address,
                                             uint32_t value) const {
   kernInstWrite1Word s = {address, value};
   
   if (-1 == ioctl(driverfd, KERNINST_WRITE1ALIGNEDWORDTOPHYSMEM, &s))
      assert(false && "KERNINST_WRITE1ALIGNEDWORDTOPHYSMEM failed");
}

void kernelDriver::poke1PermanentNonNucleusWord(kptr_t address,
                                                uint32_t value) const {
   if (sizeof(value) != pwrite(kmemfd, &value, sizeof(value), address))
      assert(false);
}

void kernelDriver::poke1WordPermanent(kptr_t address, uint32_t value) const {
   // If not in nucleus, then a pwrite to /dev/kmem works.
   // Otherwise, try driver ioctl, which'll map, write, then unmap

   assert(sizeof(value) == 4);

   if (theNucleusDetector.isInNucleus(address))
      poke1PermanentNucleusWord(address, value);
   else
      poke1PermanentNonNucleusWord(address, value);

   if (!waitForPokeToTakeEffect(address, value, tenSecondsAsNanoSecs))
      assert(false);
}

void kernelDriver::poke1WordUndoablePush(kptr_t addr,
                                         uint32_t oldValue,
                                         uint32_t newValue,
                                         unsigned howSoon) {
   // Undoable -- when kerninstd dies w/o cleaning up, /dev/kerninst will
   // undo the effects of the writes (having kept a meticulous log of all
   // of these calls, remembering the previous value).

   // May not be true on multiprocessors, let's comment it out
   // assert(peek1Word(addr) == oldValue);

   if (theUndoablePokes.defines(addr)) {
      // assert value presently in memory is "oldValue", then change it to "newValue"
      // Don't inform the driver, who only wants to be informed if this was the first
      // change.
      poke1WordPermanent(addr, newValue);

      // Verify that the changes were written as expected.
      // May not be true on multiprocessors, let's comment it out
      // assert(peek1Word(addr) == newValue &&
      //     "kernelDriver::poke1WordUndoablePush() poke1WordPermanent didn't seem to take effect");

      // Undo info is already being kept at "addr".  Push an entry onto the log.
      undoablePokeInfo1Loc &theInfo = theUndoablePokes.get(addr);
      theInfo.push(oldValue, newValue); // does some asserts
   }
   else {
      // Undoable info is NOT being kept for this address.  Let the driver know.
      
      kernInstWrite1UndoableWord s = {addr, newValue, howSoon, (uint32_t)-1};
         // The last field, "origval", will be filled in by the driver

      const bool verbose = false;

      if (theNucleusDetector.isInNucleus(addr)) {
         const mrtime_t startTime = verbose ? getmrtime() : 0;
      
         if (-1 == ioctl(driverfd, KERNINST_WRUNDOABLE_NUCLEUS, &s))
            assert(false && "KERNINST_WRUNDOABLE_NUCLEUS failed");

         if (!waitForPokeToTakeEffect(addr, newValue, tenSecondsAsNanoSecs))
            assert(false);

         if (verbose) {
            const mrtime_t totalTime = getmrtime() - startTime;
            cout << "Did an undoable nucleus poke in "
                 << totalTime/1000 << " usecs." << endl;
         }
      }
      else {
         const mrtime_t startTime = verbose ? getmrtime() : 0;

         if (-1 == ioctl(driverfd, KERNINST_WRUNDOABLE_REG, &s))
            assert(false && "KERNINST_WRUNDOABLE_REG failed");

         if (!waitForPokeToTakeEffect(addr, newValue, tenSecondsAsNanoSecs))
            assert(false);

         if (verbose) {
            const mrtime_t totalTime = getmrtime() - startTime;
            cout << "Did an undoable non-nucleus poke in "
                 << totalTime/1000 << " usecs." << endl;
         }
      }

      // Gets tripped on a dual-processor US-III, looks mp-unsafe
      // Comment it out for now.
      // const uint32_t previousValueAccordingToDriver = s.origval;
      // assert(previousValueAccordingToDriver == oldValue);

      // And finally, update our structures
      undoablePokeInfo1Loc theInfoInitializer(oldValue, howSoon);
      theUndoablePokes.set(addr, theInfoInitializer);

      undoablePokeInfo1Loc &theInfo = theUndoablePokes.get(addr);
      theInfo.push(oldValue, newValue);
   }

   // Verify that the changes were written as expected.
   // May not be true on multiprocessors, let's comment it out
   //assert(peek1Word(addr) == newValue);
}

void kernelDriver::
poke1WordUndoableChangeTopOfStack(kptr_t addr,
                                  uint32_t oldValue, // we assert that mem equalled this
                                  uint32_t newValue, // ...and we change it to this
                                  unsigned howSoon) {
   // May not be true on multiprocessors, let's comment it out
   //assert(peek1Word(addr) == oldValue);

   undoablePokeInfo1Loc &info = theUndoablePokes.get(addr); // must exist

   if (info.getHowSoon() == howSoon) {
      // the easy case
      poke1WordPermanent(addr, newValue);
      info.changeTopOfStack(oldValue, newValue);
   }
   else {
      // howSoon changed, too.  Let kernel know.

      kernInstChangeUndoHowSoon s={addr, howSoon};
      if (-1 == ioctl(driverfd, KERNINST_CHG_UNDO_HOWSOON, &s))
         assert(false && "KERNINST_CHG_UNDO_HOWSOON failed");

      poke1WordPermanent(addr, newValue);
         // TO DO -- there may be a race condition bug here; we may need to
         // package these up as a single driver call!

      info.changeHowSoon(howSoon);
      info.changeTopOfStack(oldValue, newValue);
   }

   // Verify that the changes were written as expected.
   // May not be true on multiprocessors, let's comment it out
   //assert(peek1Word(addr) == newValue);
}

void kernelDriver::
undoPoke1WordPop(kptr_t addr,
                 uint32_t currentValueInMem,
                 uint32_t popBackToThisValue) {
   // May not be true on multiprocessors, let's comment it out
   //assert(peek1Word(addr) == currentValueInMem);
   
   const bool verbose = false;

   const mrtime_t startTime = verbose ? getmrtime() : 0;

   undoablePokeInfo1Loc &info = theUndoablePokes.get(addr); // must exist

   if (info.pop(currentValueInMem, popBackToThisValue)) {
      // stack is now empty; we really have unwound all of the pokes to this
      // address, so inform the driver.

      assert(info.getOrigValue() == popBackToThisValue);
      (void)theUndoablePokes.get_and_remove(addr);
      
      kernInstUndo1Write s = {addr, popBackToThisValue};

      const bool mightBeInNucleus = theNucleusDetector.isInNucleus(addr);

      if (mightBeInNucleus) {
         if (-1 == ioctl(driverfd, KERNINST_UNDO_WR_NUCLEUS, &s))
            assert(false && "KERNINST_UNDO_WR_NUCLEUS failed");
      }
      else {
         if (-1 == ioctl(driverfd, KERNINST_UNDO_WR_REG, &s))
            assert(false && "KERNINST_UNDO_WR_REG failed");
      }

      if (!waitForPokeToTakeEffect(addr, popBackToThisValue, tenSecondsAsNanoSecs))
         assert(false);

      if (verbose) {
         const mrtime_t totalTime = getmrtime() - startTime;
         cout << "undoPoke1Word ";
         if (mightBeInNucleus)
            cout << "[nucleus]";
         else
            cout << "[non-nucleus]";
         cout << " took " << totalTime/1000 << " usecs" << endl;
      }
   }
   else {
      // stack isn't empty, so just do a normal poke
      poke1WordPermanent(addr, popBackToThisValue);
   }
   
   // Verify that the changes were written as expected.
   // May not be true on multiprocessors, let's comment it out
   //assert(peek1Word(addr) == popBackToThisValue);
}

#elif defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4)
bool kernelDriver::waitForPokeToTakeEffect(kptr_t /*addr*/, 
					   const char */*newVal*/,
                                           mrtime_t /*maxNanoSecWaitTime*/) const {
   return true;

#ifdef NOTYET
   const mrtime_t time1 = getmrtime();

   while (peek1Word(addr) != newVal) {
      if (getmrtime()-time1 > maxNanoSecWaitTime)
         return false;
   }
   
   const mrtime_t time2 = getmrtime();
   const mrtime_t totalTime = time2-time1;

   cout << "Took " << totalTime << " nsecs for write to take effect" << endl;

   return true; // success
#endif //NOTYET
}

void kernelDriver::poke1LocUndoablePush(kptr_t addr,
                                        const char *oldValue, 
                                           // we assert this...
                                        const char *newValue, 
                                           // and change to this
                                        unsigned length,
                                        unsigned howSoon) {
   // Undoable -- when kerninstd dies w/o cleaning up, /dev/kerninst will
   // undo the effects of the writes (having kept a meticulous log of all
   // of these calls, remembering the previous value).
   
   if (theUndoablePokes.defines(addr)) {
      // assert value presently in memory is "oldValue", then change it 
      // to "newValue". No need to make driver do undoable poke, since it 
      // only wants to be informed of the first change to this addr.
#ifdef ppc64_unknown_linux2_4    
//need to have extra padding to take care of kernel/user ptr size
//difference  
      struct kernInstWrite1Insn s = {addr,0, newValue, length};
#else
      struct kernInstWrite1Insn s = {addr, newValue, length};
#endif
      if (-1 == ioctl(driverfd, KERNINST_WRITE1INSN, &s))
         assert(false && "KERNINST_WRITE1INSN failed during undoable push");

      // Undo info is already being kept at "addr". Push an entry onto the log.
      undoablePokeInfo1Loc &theInfo = theUndoablePokes.get(addr);
      theInfo.push(oldValue, newValue); // does some asserts
   }
   else {
      // Undoable info is NOT being kept for this address. Let the driver know.
      char origVal[length];
      kernInstWrite1UndoableInsn s = {addr, 
#ifdef ppc64_unknown_linux2_4
                                      0, //padding
#endif
                                      newValue, 
                                      length,
                                      howSoon, 
#ifdef ppc64_unknown_linux2_4
                                      0, //padding
#endif
                                      &origVal[0]};
         // The last field, "origval", will be filled in by the driver

      const bool verbose = false;
      const mrtime_t startTime = verbose ? getmrtime() : 0;

      if (-1 == ioctl(driverfd, KERNINST_WRUNDOABLE_REG, &s))
         assert(false && "KERNINST_WRUNDOABLE_REG failed");

      if (!waitForPokeToTakeEffect(addr, 
                                   newValue,
                                   tenSecondsAsNanoSecs))
         assert(false);

      if (verbose) {
         const mrtime_t totalTime = getmrtime() - startTime;
         cout << "Did an undoable poke in " << totalTime/1000 << " usecs.\n";
      }

      if(memcmp(s.origval, oldValue, length) != 0)
         assert(!"kernelDriver::poke1LocUndoablePush() - origval mismatch\n");

      // And finally, update our structures
      undoablePokeInfo1Loc theInfoInitializer(oldValue, length, howSoon);
      theUndoablePokes.set(addr, theInfoInitializer);

      undoablePokeInfo1Loc &theInfo = theUndoablePokes.get(addr);
      theInfo.push(oldValue, newValue);
   }
}

void kernelDriver::poke1LocUndoableChangeTopOfStack(kptr_t addr,
                                                    const char *oldValue, 
                                                       // we assert this...
                                                    const char *newValue, 
                                                       // and change to this
                                                    unsigned howSoon) {

   undoablePokeInfo1Loc &info = theUndoablePokes.get(addr); // must exist

   if (info.getHowSoon() != howSoon) {
      // howSoon changed, too.  Let kernel know.
      struct kernInstChangeUndoHowSoon s = {addr, howSoon};
      if (-1 == ioctl(driverfd, KERNINST_CHG_UNDO_HOWSOON, &s))
         assert(false && "KERNINST_CHG_UNDO_HOWSOON failed");

      info.changeHowSoon(howSoon);
   }

#ifdef ppc64_unknown_linux2_4    
//need to have extra padding to take care of kernel/user ptr size
//difference  
   struct kernInstWrite1Insn s = {addr, 0, newValue, info.getLength()};
#else
   struct kernInstWrite1Insn s = {addr, newValue, info.getLength()};
#endif
   if (-1 == ioctl(driverfd, KERNINST_WRITE1INSN, &s))
      assert(false && "KERNINST_WRITE1INSN failed during undoable change top of stack");
   info.changeTopOfStack(oldValue, newValue);
}

void kernelDriver::undoPoke1LocPop(kptr_t addr,
                                   const char *currentValue,
                                   const char *popBackToThisValue) {
   const bool verbose = false;
   const mrtime_t startTime = verbose ? getmrtime() : 0;

   undoablePokeInfo1Loc &info = theUndoablePokes.get(addr); // must exist
   unsigned length = info.getLength();

   if (info.pop(currentValue, popBackToThisValue)) {
      // stack is now empty; we really have unwound all of the pokes to this
      // address, so inform the driver.

      if(memcmp(info.getOrigValue(), popBackToThisValue, length) != 0)
         assert(!"kernelDriver::undoPoke1LocPop() origval mismatch\n");
      (void)theUndoablePokes.get_and_remove(addr);
#ifdef ppc64_unknown_linux2_4    
//need to have extra padding to take care of kernel/user ptr size
//difference  
      kernInstUndo1Write s = {addr, 0, popBackToThisValue, length};
#else
      kernInstUndo1Write s = {addr, popBackToThisValue, length};
#endif
      if (-1 == ioctl(driverfd, KERNINST_UNDO_WR_REG, &s))
         assert(!"KERNINST_UNDO_WR_REG failed");

      if (!waitForPokeToTakeEffect(addr, popBackToThisValue, 
                                   tenSecondsAsNanoSecs))
         assert(false);

      if (verbose) {
         const mrtime_t totalTime = getmrtime() - startTime;
         cout << "undoPoke1Loc took " << totalTime/1000 << " usecs\n";
      }
   }
   else {
      // stack isn't empty, so just do a normal poke
#ifdef ppc64_unknown_linux2_4    
//need to have extra padding to take care of kernel/user ptr size
//difference  
      struct kernInstWrite1Insn s = {addr,0, popBackToThisValue, length};
#else
      struct kernInstWrite1Insn s = {addr, popBackToThisValue, length};
#endif
      if (-1 == ioctl(driverfd, KERNINST_WRITE1INSN, &s))
         assert(false && "KERNINST_WRITE1INSN failed during undoable pop");
   }
}

#ifdef i386_unknown_linux2_4
void kernelDriver::insertTrapToPatchAddrMapping(kptr_t trapAddr, 
                                                kptr_t patchAddr) {
   kernInstTrapMapping m = {trapAddr, patchAddr};
   if (-1 == ioctl(driverfd, KERNINST_REGISTER_TRAP_PATCH_MAPPING, &m))
      assert(!"KERNINST_REGISTER_TRAP_PATCH_MAPPING failed");
}

void kernelDriver::updateTrapToPatchAddrMapping(kptr_t trapAddr, 
                                                kptr_t patchAddr) {
   kernInstTrapMapping m = {trapAddr, patchAddr};
   if (-1 == ioctl(driverfd, KERNINST_UPDATE_TRAP_PATCH_MAPPING, &m))
      assert(!"KERNINST_UPDATE_TRAP_PATCH_MAPPING failed");
}

void kernelDriver::removeTrapToPatchAddrMapping(kptr_t trapAddr) {
   if (-1 == ioctl(driverfd, KERNINST_UNREGISTER_TRAP_PATCH_MAPPING, &trapAddr))
      assert(!"KERNINST_UNREGISTER_TRAP_PATCH_MAPPING failed");
}
#endif //ifdef i386
#endif //ifdef solaris2_7

uint32_t kernelDriver::peek1Word(kptr_t address) const {
   uint32_t result;
   
   if (4 != peek_block(&result, address, 4, false)) // not ok to read less
      assert(false);
   
   return result;
}

uint32_t kernelDriver::peek_block(void *dest, kptr_t startAddr,
                                  uint32_t len, bool reread_on_fault) const {
    // a pread() from /dev/kmem
    // returns actual # of bytes read (in particular, 0 on error)
    // If it can't read "len" bytes, it'll read as much as it can, and
    // return that number of bytes.  Useful, e.g. when there are "holes"
    // in kernel virtual memory (we read from beginning of one symbol
    // to the beginning of the next symbol, which can lead to such
    // memory holes when used naively).
   
#ifdef PURE_BUILD
   // The following avoids purify UMRs (purify can't know when a kernel routine
   // writes to memory since it doesn't instrument the kernel)
   memset(dest, '\0', len); // sorry for the expense
#endif
#if 0
#ifdef ppc64_unknown_linux2_4
#define MAX_POS_NUM 0x7FFFFFFFFFFFFFFFULL
   //for pread, we must make sure offset never looks to be negative, hence if startAddr is too
   //large, we must seek ahead in /dev/kmem, and adjust startAddr accordingly
   bool reset_seek =  false;

   if (startAddr > MAX_POS_NUM) {

       reset_seek = true;
       kptr_t rv;
       if ( (rv = lseek64(kmemfd, MAX_POS_NUM , SEEK_CUR)) == MAX_POS_NUM  ) { 
	   startAddr -=  MAX_POS_NUM;
       } else {
	   perror("kernelDriver::peek_block: error seeking");
	   return 0;
       }
   }
#endif
#endif   
   if (!reread_on_fault) {
       int rv = kerninst_pread(kmemfd, dest, len, startAddr);
       if (rv < 0) {
	   perror("kernelDriver::peek_block: pread error");
	   return 0;
       }
       return rv;
   }

   char *startDest = (char *)dest;
   uint32_t total = 0;
   uint32_t bsize = len;

   while (total < len && bsize != 0) {
       int rv;
       if ((rv = kerninst_pread(kmemfd, startDest, bsize, startAddr)) > 0) {
	   total += rv;
	   startDest += rv;
	   startAddr += rv;
       } else if (rv == 0) {
           //end of file
           break;
       }
       else if (errno != EFAULT && errno != EIO ){ // EFAULT or EIO -> we've 
                                                   // encountered a memory hole
	   perror("kernelDriver::peek_block: pread error");
	   return 0;
       }
#ifdef sparc_sun_solaris2_7
       bsize = (bsize >> 1) & ~3; // Truncate to the 4 byte boundary
#endif
       if (total + bsize > len) {
	   bsize = len - total;
       }
   }

   if (total < len) {
       dout << "Warning: potential memory hole at or close to address "
	    << addr2hex(startAddr) << endl;
   }
#if 0 
#ifdef ppc64_unknown_linux2_4
   //reset the file offset pointer back, if we moved it above.
   if (reset_seek) {
       lseek64(kmemfd, -0x7FFFFFFFFFFFFFFFULL , SEEK_CUR);
   }
      
#endif
#endif
   return total;
}

int kernelDriver::poke_block(kptr_t startAddr, uint32_t len, const void *src)
{
#ifdef sparc_sun_solaris2_7
    if (theNucleusDetector.isInNucleus(startAddr) ||
	theNucleusDetector.isInNucleus(startAddr + len)) {
	assert(false && "Not supported yet");
    }
#endif
    if (kerninst_pwrite(kmemfd, src, len, startAddr) != (int)len) {
	perror("kernelDriver::poke_block: wrote less than expected");
	assert(false);
    }
#ifdef ppc64_unknown_linux2_4
    flushIcacheRange(startAddr, len);
#endif
    return 0;
}

uint32_t kernelDriver::getSymtabStuffSize() const {
   uint32_t result = 0; // initialize to avoid purify UMR
   if (-1 == ioctl(driverfd, KERNINST_SYMTAB_STUFF_SIZE, &result)) {
      cerr << "ioctl symtab_stuff_size to kernel driver failed" << endl;
      result = 0;
   }
   return result;
}

void kernelDriver::getSymtabStuff(void *buffer, uint32_t size) const {
#ifdef PURE_BUILD
   // initialize buffer to avoid purify UMR:
   memset(buffer, '\0', size); // sorry for the expense
#endif
   struct kernInstSymtabGetStruct s = {(kptr_t)buffer, size, 0};
   if (-1 == ioctl(driverfd, KERNINST_SYMTAB_STUFF_GET, &s)) {
      cerr << "ioctl symtab_stuff_get to kernel driver failed" << endl;
      assert(0);
   }
   assert(s.actual_amt_copied == size); // better to throw an exception?
}

// Allocate nbytes in the kernel address space and return two pointers to
// the region. The second pointer may differ from the first one and is
// used when we mmap the region into the daemon's address space
// allocType allows us to specify where in the kernel the region should be
// placed (nucleus, below_nucleus, kmem32, data)
pair<kptr_t, kptr_t>
kernelDriver::allocate_kernel_block(uint32_t nbytes,
                                    uint32_t requiredAlignment,
                                       // e.g. 32 for i-cache-block alignment
                                    bool /*dummy*/,
				    unsigned allocType) {
   const bool verbose = false;
   
   const mrtime_t time1 = verbose ? getmrtime() : 0;
   
   kernInstAllocStruct abs = {nbytes, requiredAlignment, allocType, 0, 0};
      // 4th & 5th fields get filled in
   if (-1 == ioctl(driverfd, KERNINST_ALLOC_BLOCK, &abs)) {
      perror("ioctl KERNINST_ALLOC_BLOCK");
      return make_pair(0, 0);
   }

   if (verbose) {
      const mrtime_t totalTime = getmrtime() - time1;
      cout << "kernelDriver allocated [allocType=" << allocType
           << "] in " << totalTime/1000 << " usecs." 
           <<" addr " <<abs.result << endl;
   }
   
   const kptr_t kernelAddr = abs.result;
   
   if (kernelAddr != 0) {
       assert(kernelAddr % requiredAlignment == 0 && 
	      "result not aligned properly!");
       assert(abs.result_mappable % requiredAlignment == 0 &&
	      "result aligned OK, but result_mappable not!");
   
       allocatedKernelMemBlocks.set(kernelAddr, nbytes);
   }

   return make_pair(abs.result, abs.result_mappable);
}

// Compatibility interface to the allocator
pair<kptr_t, kptr_t>
kernelDriver::allocate_kernel_block(uint32_t nbytes,
                                    uint32_t requiredAlignment,
                                    bool tryForNucleusText) {
    // Note: tryForNucleusText true --> we try, but don't insist on, 
    // getting memory from the nucleus.
    uint32_t allocType = tryForNucleusText ? 
	ALLOC_TYPE_TRY_NUCLEUS : ALLOC_TYPE_DATA;
    return allocate_kernel_block(nbytes, requiredAlignment, false, allocType);
}

void kernelDriver::free_kernel_block(kptr_t kernelAddr,
                                     kptr_t mappedKernelAddr,
                                        // redundant; driver already knows this value
                                     uint32_t nbytes) {
   kernInstFreeStruct s = {kernelAddr, mappedKernelAddr, nbytes};

   if (-1 == ioctl(driverfd, KERNINST_FREE_BLOCK, &s)) {
      perror("ioctl KERNINST_FREE_BLOCK");
   }

   if (allocatedKernelMemBlocks.get_and_remove(kernelAddr) != nbytes)
      assert(false);
}

#ifdef sparc_sun_solaris2_7
// Allocate kernel memory from the 32-bit kernel heap. 
// We use this region as springboard space to instrument 
// the non-nucleus parts of the kernel
kptr_t kernelDriver::allocate_kmem32(uint32_t nbytes)
{
    kernInstAllocStruct abs = {nbytes, 4, ALLOC_TYPE_KMEM32, 0, 0};
    // 4th & 5th fields get filled in
    if (ioctl(driverfd, KERNINST_ALLOC_BLOCK, &abs) < 0) {
	perror("ioctl KERNINST_ALLOC_BLOCK");
	return 0;
    }

    const kptr_t kernelAddr = abs.result;
    assert(kernelAddr != 0);
    dout << "Allocated a " << nbytes << "-byte block at " 
	 << addr2hex(kernelAddr) << " in the 32-bit region\n";

    allocatedKernelMemBlocks.set(kernelAddr, nbytes);

    return kernelAddr;
}

void kernelDriver::free_kmem32(kptr_t kernelAddr, uint32_t nbytes) 
{
    kernInstFreeStruct s = {kernelAddr, kernelAddr, nbytes};

    if (ioctl(driverfd, KERNINST_FREE_BLOCK, &s) < 0) {
	perror("ioctl KERNINST_FREE_BLOCK");
	assert(false);
    }
    if (allocatedKernelMemBlocks.get_and_remove(kernelAddr) != nbytes) {
	assert(false);
    }
}
#endif //solaris2_7

uint32_t kernelDriver::pageNumBytes = kernelDriver::getPageNumBytes_FirstTime();

uint32_t kernelDriver::roundUpToMultipleOfPageSize(uint32_t len) { // kptr_t ?
   // a static method
   return roundup(len, getPageNumBytes());
}

kptr_t kernelDriver::addr2Page(kptr_t addr) {
   // a static method
   const uint32_t pagesize = getPageNumBytes();

   kptr_t extra = addr % pagesize;

   kptr_t result = addr - extra;
   assert(result % pagesize == 0);
   return result;
}

uint32_t kernelDriver::getPageNumBytes() {
   assert(pageNumBytes != 0);
   return pageNumBytes;
}

uint32_t kernelDriver::getPageNumBytes_FirstTime() {
   // a static method
   long result = sysconf(_SC_PAGESIZE);
   return result;
}

void *kernelDriver::map_for_readonly(kptr_t addrInKernel, uint32_t len,
                                     bool assertPageSizedAndPageAlignedAlready) {
   return map_common(addrInKernel, len, PROT_READ,
                     assertPageSizedAndPageAlignedAlready);
}

void *kernelDriver::map_for_rw(kptr_t addrInKernel, uint32_t len,
                               bool assertPageSizedAndPageAlignedAlready) {
   return map_common(addrInKernel, len, PROT_READ | PROT_WRITE,
                     assertPageSizedAndPageAlignedAlready);
}

void* kernelDriver::kerninst_mmap(void *addr, size_t len, int prot, 
				  int flags, int fildes, uint32_t off)
{
   return mmap((caddr_t)addr, len, prot, flags, fildes, off);
}

ssize_t kernelDriver::kerninst_pread(int fildes, void *buf, 
				     size_t nbyte, uint32_t off)
{
   if( lseek(fildes, (off_t)off, SEEK_SET) != (off_t)-1 ) {
      return read(fildes, buf, nbyte);
   }
   else {
      perror("kernelDriver::kerninst_pread - unsuccessful lseek");
      return 0;
   }
}

ssize_t kernelDriver::kerninst_pwrite(int fildes, const void *buf, 
                                      size_t nbyte, uint32_t off)
{
   if( lseek(fildes, (off_t)off, SEEK_SET) != (off_t)-1 ) {
      return write(fildes, buf, nbyte);
   }
   else {
      perror("kernelDriver::kerninst_pwrite - unsuccessful lseek");
      return 0;
   }
}


#if defined(sparc_sun_solaris2_7) || defined(ppc64_unknown_linux2_4)
void* kernelDriver::kerninst_mmap(void *addr, size_t len, int prot, 
				  int flags, int fildes, uint64_t off)
{
   return mmap64((caddr_t)addr, len, prot, flags, fildes, off);
}

ssize_t kernelDriver::kerninst_pread(int fildes, void *buf, 
				     size_t nbyte, uint64_t off)
{
#ifdef ppc64_unknown_linux2_4
    //for pread, we must make sure offset never looks to be negative, hence if startAddr is too
    //large, we must seek ahead in /dev/kmem, and adjust startAddr accordingly
#define MAX_POS_NUM 0x7FFFFFFFFFFFFFFFULL

    bool reset_seek =  false;
    kptr_t rv;
    if (off > MAX_POS_NUM) {
	
	reset_seek = true;

	if ( (rv = lseek64(fildes, MAX_POS_NUM , SEEK_CUR)) == MAX_POS_NUM ) { 
	    off -=  MAX_POS_NUM;
	} else {
	    perror("kernelDriver::kerninst_pread: error seeking");
	    return 0;
	}
    }
    if ( (rv = lseek64(fildes, off, SEEK_CUR)) != (MAX_POS_NUM  + off) )  {
//	perror("kernelDriver::kerninst_pread: error seeking");
//	return 0;
    }
	
    
    int ret =  read(fildes, buf, nbyte);

    //reset the file offset pointer back, if we moved it above.
    if ( (rv =  lseek64(fildes, 0, SEEK_SET) ) != 0) {
	perror("kernelDriver::kerninst_pread: error resetting seek");
	return 0;
    }
    return ret;
#else
    return pread64(fildes, buf, nbyte, off);
#endif
}

ssize_t kernelDriver::kerninst_pwrite(int fildes, const void *buf, 
				      size_t nbyte, uint64_t off)
{
#ifdef ppc64_unknown_linux2_4
#define MAX_POS_NUM 0x7FFFFFFFFFFFFFFFULL
    //for pwrite, we must make sure offset never looks to be negative, hence if startAddr is too
    //large, we must seek ahead in /dev/kmem, and adjust startAddr accordingly
    bool reset_seek =  false;
    kptr_t rv;
    if (off > MAX_POS_NUM) {
	
	reset_seek = true;

        if(  (rv = lseek64(fildes, MAX_POS_NUM , SEEK_CUR)) == MAX_POS_NUM  ) { 
	    off -= MAX_POS_NUM;
	} else {
	    perror("kernelDriver::kerninst_pwrite: error seeking");
	    return 0;
	}
    } 
    if ( (rv = lseek64(fildes, off, SEEK_CUR)) !=  off )  {
//	perror("kernelDriver::kerninst_pwrite: error seeking");
//	return 0;
    }
	
    int ret =  write(fildes, buf, nbyte);

    //reset the file offset pointer back, if we moved it above.
    if ( (rv =  lseek64(fildes, 0, SEEK_SET) ) != 0) {
	perror("kernelDriver::kerninst_pread: error resetting seek");
	return 0;
    }
    return ret;
#else
   return pwrite64(fildes, buf, nbyte, off);
#endif
}
#endif

void *kernelDriver::map_common(kptr_t addrInKernel, uint32_t len,
                               uint32_t prot,
                               bool assertPageSizedAndPageAligned) {
   // mmap only works on page-sized quantities that are also page-aligned.
   // If the two bools are true, then we'll assert this for you.
   // Else, if needed, we mmap more space (to make it page-sized and/or to make
   // it page-aligned) than you asked for, behind the scenes.  You don't have
   // to worry about this, though.

//     cout << "Welcome to map_common(); addrInKernel is " << addr2hex(addrInKernel)
//          << " len is " << len << endl;
   
   if (assertPageSizedAndPageAligned) {
      assert(addrInKernel % getPageNumBytes() == 0 &&
             "insufficient alignment for mmap!");
      assert(len % getPageNumBytes() == 0 &&
             "needs to be page-sized for mmap!");
   }

   const uint32_t MMU_PAGESIZE = getPageNumBytes();
   const kptr_t MMU_PAGEMASK = ~((kptr_t)MMU_PAGESIZE-1);
   const kptr_t startMappingAddr = addrInKernel & MMU_PAGEMASK;
   const kptr_t offset = addrInKernel & ~MMU_PAGEMASK;
   const uint32_t bytesToMap = roundup(len + offset, MMU_PAGESIZE);
   assert(bytesToMap % MMU_PAGESIZE == 0);
   //const uint32_t pagesToMap = bytesToMap / MMU_PAGESIZE;
   
   void *result = kerninst_mmap(NULL, bytesToMap, prot,
				MAP_SHARED, kmemfd, startMappingAddr);
   if (result == MAP_FAILED) {
      perror("kernelDriver could not mmap");
      assert(false && "could not mmap!");
   }

//   if (offset != 0) {
//        cout << "mmap " << len << " bytes at " << addr2hex(addrInKernel) << endl;
//        cout << "handled with mmap of " << bytesToMap << " bytes at "
//             << addr2hex(startMappingAddr) << endl;
//   }
   
   return (void*)((kptr_t)result + offset);
}

void kernelDriver::unmap_common(void *inHere, uint32_t len) {
   // We no longer assert that "len" is a multiple of page size, since map is clever
   // enough to handle non-page sizes, and non-page-aligned addresses.
   // See map_common() for details...
   
   const dptr_t MMU_PAGESIZE = getPageNumBytes();
   const dptr_t MMU_PAGEMASK = ~(MMU_PAGESIZE-1);
   const kptr_t mapActuallyStartedHere = (kptr_t)inHere & MMU_PAGEMASK;
   const kptr_t offset = (kptr_t)inHere & ~MMU_PAGEMASK;
   const dptr_t bytesMapped = roundup(len + offset, MMU_PAGESIZE);
   assert(bytesMapped % MMU_PAGESIZE == 0);
   //const uint32_t pagesMapped = bytesMapped / MMU_PAGESIZE;

   assert(mapActuallyStartedHere <= (kptr_t)inHere);
   assert(bytesMapped >= len);

//     if (offset != 0) {
//        cout << "munmap " << inHere << " of " << len << " bytes handled as" << endl;
//        cout << "munmap from " << addr2hex(mapActuallyStartedHere) << " of "
//             << bytesMapped << " bytes" << endl;
//     }
   
   if (-1 == munmap((caddr_t)mapActuallyStartedHere, bytesMapped)) {
      assert(false && "could not munmap!");
   }
}

void kernelDriver::unmap_from_rw(void *inHere, uint32_t len) {
   unmap_common(inHere, len);
}

void kernelDriver::unmap_from_readonly(void *inHere, uint32_t len) {
   unmap_common(inHere, len);
}

#ifdef ppc64_unknown_linux2_4
void kernelDriver::putInternalKernelSymbols(kptr_t kernelptrs[]) 
{

    if (ioctl(driverfd, KERNINST_PUT_INTERNAL_KERNEL_SYMBOLS, kernelptrs) < 0) {
	perror("ioctl KERNINST_PUT_INTERNAL_KERNEL_SYMBOLS");
	assert(false);
    }
}
void kernelDriver::flushIcacheRange(kptr_t addr, unsigned numBytes) 
{
    struct kernInstFlushRange range = {addr, numBytes};
    if (ioctl(driverfd, KERNINST_FLUSH_ICACHE_RANGE, range) < 0) {
	perror("ioctl KERNINST_FLUSH_ICACHE_RANGE");
	assert(false);
    } 
}
#endif

#ifdef sparc_sun_solaris2_7
extern "C" uint64_t ari_get_tick_raw(); // in /dev/kerninst directory

uint64_t kernelDriver::getTickRaw() const {
    static bool tick_protected = true;
    uint64_t val;

    if (tick_protected) {
	if (ioctl(driverfd, KERNINST_GET_TICK_RAW, &val) < 0) {
	    perror("ioctl(KERNINST_GET_TICK_RAW) failed");
	    assert(false);
	}
	if ((val >> 63) == 0) {
	    tick_protected = false; // next time we'll read it directly
	}
    }
    else {
	val = ari_get_tick_raw();
    }
    return val;
}

extern "C" uint64_t ari_get_stick_raw(); // in /dev/kerninst directory

// Read the %stick register (Ultra-3 only!)
uint64_t kernelDriver::getStickRaw() const {
    static bool stick_protected = true;
    uint64_t val;

    if (stick_protected) {
	if (ioctl(driverfd, KERNINST_GET_STICK_RAW, &val) < 0) {
	    perror("ioctl(KERNINST_GET_STICK_RAW) failed");
	    assert(false);
	}
	if ((val >> 63) == 0) {
	    stick_protected = false; // next time we'll read it directly
	}
    }
    else {
	val = ari_get_stick_raw();
    }
    return val;
}

uint64_t kernelDriver::getPcrRawCurrentCPU() const {
   // needed since we can't unprotect %pcr

   uint64_t val = 0; // initialize to avoid purify UMR
   if (-1 == ioctl(driverfd, KERNINST_GET_PCR_RAW, &val))
      assert(false && "getPcrRaw() failed");
      
   return val;
}

// Program the PCR register on a current CPU in the system
// Unsafe to use on SunFire machines. Use cpcInterface instead!
void kernelDriver::setPcrRawCurrentCPU(uint64_t raw) 
{
    if (ioctl(driverfd, KERNINST_SET_PCR_RAW, &raw) < 0) {
	assert(false && "setPcrRawCurrentCPU() failed");
    }
}

uint64_t kernelDriver::getPicRaw() const {
   // needed since we can't unprotect %pic

   uint64_t val = 0; // initialize to avoid purify UMR
   if (-1 == ioctl(driverfd, KERNINST_GET_PIC_RAW, &val))
      assert(false && "getPicRaw() failed");
      
   return val;
}

void kernelDriver::setPicRaw(uint64_t raw) { // const?
   // needed since we can't unprotect %pic

   if (-1 == ioctl(driverfd, KERNINST_SET_PIC_RAW, &raw))
      assert(false && "setPicRaw() failed");
}

// Convert the given ABI into a numerical value (0 == sparcv8+, 1 == sparcv9)
static unsigned abi2ABICode(const abi &theABI) {
   if (dynamic_cast<const sparcv8plus_abi*>(&theABI) != NULL)
      return 0;
   else if (dynamic_cast<const sparcv9_abi*>(&theABI) != NULL)
      return 1;
   else
      assert(false && "unknown abi");
}

// Obtain miscellaneous machine information and fill *pmi
void kernelDriver::getPresentedMachineInfo(const abi& kernelABI, 
					   const abi& kerninstdABI,
					   presentedMachineInfo *pmi) const 
{
    pmi->kernelABICode = abi2ABICode(kernelABI);
    pmi->kerninstdABICode = abi2ABICode(kerninstdABI);

    // Determine the OS version
    struct utsname un;
    if (uname(&un) < 0) {
	perror("uname() error");
	assert(false);
    }
    if (!strcmp(un.release, "5.6")) {
	pmi->os_version = 6;
    }
    else if (!strcmp(un.release, "5.7")) {
	pmi->os_version = 7;
    }
    else if (!strcmp(un.release, "5.8")) {
	pmi->os_version = 8;
    }
    else {
	cout << "Can't recognize the OS version, assuming Solaris 7\n";
	pmi->os_version = 7;
    }

    // Determine the processor type by asking the driver
    uint32_t impl_num;
    if (ioctl(driverfd, KERNINST_GET_CPU_IMPL_NUM, &impl_num) < 0) {
	perror("KERNINST_GET_CPU_IMPL_NUM ioctl error");
	assert(false);
    }
    pmi->has_stick = (impl_num == 0x14); // has %stick iff ultra-3

    // Determine system frequency:
    //  (%tick frequency on ultra-2, %stick on ultra-3)
    if (ioctl(driverfd, KERNINST_GET_CLOCK_FREQ, &pmi->system_freq) < 0) {
	perror("KERNINST_GET_CLOCK_FREQ ioctl error");
	assert(false);
    }

    if (ioctl(driverfd, KERNINST_GET_DISP_LEVEL, &pmi->disp_level) < 0) {
	perror("KERNINST_GET_DISP_LEVEL ioctl error");
	assert(false);
    }
    dout << "This system has a DISP_LEVEL of " << pmi->disp_level << endl;

    kernInstOffsetInfo koi;
    if (ioctl(driverfd, KERNINST_GET_OFFSET_INFO, &koi) < 0) {
	perror("KERNINST_GET_OFFSET_INFO ioctl error");
	assert(false);
    }
    pmi->t_cpu_offset = koi.t_cpu_offset;
    pmi->t_procp_offset = koi.t_procp_offset;
    pmi->p_pidp_offset = koi.p_pidp_offset;
    pmi->pid_id_offset = koi.pid_id_offset;
#ifdef ppc64_unknown_linux2_4   
    pmi->t_pacacurrent_offset = koi.t_pacacurrent_offset;
    pmi->paca_access = koi.paca_access;
#endif

    cpu_info ci;
    unsigned num_cpus = cpuinfo.size();

    for (unsigned i=0; i < num_cpus; i++) {
	ci.cpuid = cpuinfo[i].first;
#ifdef ppc64_unknown_linux2_4
        //power is being funny as usual.  The time base counter counts in
        //some fixed frequency, which is some multiple of the cycle speed.
        //The kernel tells us tb frequency per usec, so we can use this
        //value in scaling calculations instead of mhz 
        if (ioctl(driverfd, KERNINST_GET_TICKS_PER_USEC, &ci.megahertz) < 0) {
	   perror("KERNINST_GET_TICKS_PER_USEC ioctl error");
	   assert(false);
        }
#else
	ci.megahertz = cpuinfo[i].second;
#endif
	pmi->theCpusInfo += ci;
    }

    // nucleus bounds:
    const pair<kptr_t, kptr_t> nucleusBounds = queryNucleusTextBounds();
    pmi->nucleusTextBoundsLow = nucleusBounds.first;
    pmi->nucleusTextBoundsHi = nucleusBounds.second;
}

#elif defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4)

uint64_t kernelDriver::getTickRaw() const {
   uint32_t val[2];
   uint64_t retval = 0;
#ifdef i386_unknown_linux2_4
   if (ioctl(driverfd, KERNINST_GET_TICK_RAW, val) < 0) {
      perror("ioctl(KERNINST_GET_TICK_RAW) failed");
      assert(false);
   }

   retval = (uint64_t)val[1]; // high 32-bits
   retval <<= 32;
   retval += (uint64_t)val[0]; // low 32-bits
   return retval;
#else //must be ppc64
     if (ioctl(driverfd, KERNINST_GET_TICK_RAW, &retval) < 0) {
      perror("ioctl(KERNINST_GET_TICK_RAW) failed");
      assert(false);
   }
   return retval;
#endif //i386_unknown_linux2_4
}

uint64_t kernelDriver::getStickRaw() const {
   assert(!"kernelDriver::getStickRaw() not supported on x86\n");
   return 0;
}

// Obtain miscellaneous machine information and fill *pmi
void kernelDriver::getPresentedMachineInfo(const abi& /*kernelABI*/, 
					   const abi& /*kerninstdABI*/,
					   presentedMachineInfo *pmi) const 
{

   struct utsname uname_buf;
   if(! uname(&uname_buf) ) {
      if(!strncmp("2.4", &uname_buf.release[0], 3)) {
         pmi->os_version = 24; // for 2.4
      }
      else if(!strncmp("2.6", &uname_buf.release[0], 3)) {
         pmi->os_version = 26; // for 2.6
      }
      else {
         cerr << "WARNING: Linux kernel versions other than 2.4.x and 2.6.x are not supported. Defaulting to Linux 2.4.x series behavior.\n";
         pmi->os_version = 24;  // for 2.4
      }
   }
   else
      assert(!"INTERNAL ERROR: failed attempt to obtain kernel release number using uname()\n");
 
   pmi->system_freq = 0; // for now

   cpu_info ci;
   unsigned num_cpus = cpuinfo.size();
#ifdef i386_unknown_linux2_4
   assert(num_cpus == cpumodelinfo.size());
#endif
   unsigned family = 0;
   unsigned model = 0;

   for (unsigned i=0; i < num_cpus; i++) {
      ci.cpuid = cpuinfo[i].first;
#ifdef ppc64_unknown_linux2_4
        //power is being funny as usual.  The time base counter counts in
        //some fixed frequency, which is some multiple of the cycle speed.
        //The kernel tells us tb frequency per usec, so we can use this
        //value in scaling calculations instead of mhz 
        kptr_t tbperusec;
        if (ioctl(driverfd, KERNINST_GET_TICKS_PER_USEC, &tbperusec) < 0) {
	   perror("KERNINST_GET_TICKS_PER_USEC ioctl error");
	   assert(false);
        }
        ci.megahertz = (unsigned)tbperusec; 
        //even though the ioctl returns a 64-bit number, tbperusec can't be that big
#else
      ci.megahertz = cpuinfo[i].second;
#endif
      if(i == 0) pmi->system_freq = ci.megahertz * 1000000;
      pmi->theCpusInfo += ci;
#ifdef i386_unknown_linux2_4
      // get the highest <family, model> pair
      if(cpumodelinfo[i].first > family) {
         family = cpumodelinfo[i].first;
         model = cpumodelinfo[i].second;
      }
      else if(cpumodelinfo[i].first == family) {
         if(cpumodelinfo[i].second > model)
            model = cpumodelinfo[i].second;
      }
#endif
   }

   // For lack of a better place to store cpu family and model info, we'll
   // hijack the ABI members since they're not currently used on x86   
   pmi->kerninstdABICode = model;
   pmi->kernelABICode = family;

   // these are not applicable on x86/linux, so set equal to 0
   pmi->has_stick = false;
   pmi->disp_level = 0;

   // lastly, get the offset info
   kernInstOffsetInfo koi;
   if (ioctl(driverfd, KERNINST_GET_OFFSET_INFO, &koi) < 0) {
      perror("KERNINST_GET_OFFSET_INFO ioctl error");
      assert(false);
   }
#ifdef ppc64_unknown_linux2_4
   pmi->t_pacacurrent_offset = koi.t_pacacurrent_offset;
   pmi->paca_access = koi.paca_access;
#endif
   pmi->t_cpu_offset = koi.thr_cpu_offset;
   pmi->t_procp_offset = koi.thr_task_offset;
   pmi->t_size = koi.thr_size;
   pmi->p_pidp_offset = koi.task_pid_offset;
   pmi->pid_id_offset = 0;
}

#ifdef i386_unknown_linux2_4
P4PerfCtrState kernelDriver::getP4PerfCtrState()
{
   perfctr_state_t the_state;
   if (ioctl(driverfd, KERNINST_GET_PERF_CTR_STATE, &the_state) < 0) {
      perror("KERNINST_GET_PERF_CTR_STATE ioctl error");
      assert(false);
   }

   P4PerfCtrState result;
   for(int i=0; i<NUM_COUNTERS; i++) {
      result.push_back(
                 std::make_pair<unsigned,unsigned>(the_state.cccr_val[i],
                                                   the_state.escr_val[i]));
   }
   result.push_back(std::make_pair<unsigned,unsigned>(the_state.pebs_msr[0],
                                                      the_state.pebs_msr[1]));
   return result;
}

void kernelDriver::setP4PerfCtrState(const P4PerfCtrState &newState)
{
   perfctr_state_t the_state;
   assert(newState.size() == NUM_COUNTERS+1);
   for(int i=0; i<NUM_COUNTERS; i++) {
      the_state.cccr_val[i] = newState[i].first;
      the_state.escr_val[i] = newState[i].second;
   }
   the_state.pebs_msr[0] = newState[NUM_COUNTERS].first;
   the_state.pebs_msr[1] = newState[NUM_COUNTERS].second;

   if (ioctl(driverfd, KERNINST_SET_PERF_CTR_STATE, &the_state) < 0) {
      perror("KERNINST_SET_PERF_CTR_STATE ioctl error");
      assert(false);
   }
}
#elif defined(ppc64_unknown_linux2_4)
void kernelDriver::getPowerPerfCtrState(pdvector <uint64_t> & curr_state) 
{
   struct kernInstPowerPerfCtrState the_state; 
   if (ioctl(driverfd, KERNINST_GET_POWER_PERF_CTR_STATE, &the_state) < 0) {
      perror("KERNINST_GET_POWER_PERF_CTR_STATE ioctl error");
      assert(false);
   }
   curr_state[0] = the_state.mmcr0;
   curr_state[1] = the_state.mmcr1;
   curr_state[2] = the_state.mmcra;
}
void kernelDriver::setPowerPerfCtrState(const pdvector<uint64_t> &the_state) 
{
   struct kernInstPowerPerfCtrState new_state = {the_state[0], the_state[1], the_state[2]} ;
   if (ioctl(driverfd, KERNINST_SET_POWER_PERF_CTR_STATE, &new_state) < 0) {
      perror("KERNINST_SET_PERF_CTR_STATE ioctl error");
      assert(false);
   }
}
void kernelDriver::cSwitchTest()
{
   if (ioctl(driverfd, KERNINST_CSWITCH_TEST, NULL) < 0)
      assert(!"kernelDriver::cswitchtest() - ioctl failed");
}
#endif //i386
#endif // arch_os

kptr_t kernelDriver::getAllVTimersAddr()
{
   kptr_t rv = 0;
   if (ioctl(driverfd, KERNINST_GET_ALLVTIMERS_ADDR, &rv) < 0)
      assert(!"kernelDriver::getAllVTimersAddr() - ioctl failed");
   return rv;
}

void kernelDriver::addTimerToAllVTimers(kptr_t timer_addr)
{
   if (ioctl(driverfd, KERNINST_ADD_TO_ALLVTIMERS, &timer_addr) < 0)
      assert(!"kernelDriver::addTimerToAllVTimers() - ioctl failed");
}

void kernelDriver::removeTimerFromAllVTimers(kptr_t timer_addr)
{
   if (ioctl(driverfd, KERNINST_REMOVE_FROM_ALLVTIMERS, &timer_addr) < 0)
      assert(!"kernelDriver::removeTimerFromAllVTimers() - ioctl failed");
}

void kernelDriver::clearClientState()
{
   if (ioctl(driverfd, KERNINST_CLEAR_CLIENT_STATE) < 0)
      assert(!"kernelDriver::clearClientState() - ioctl failed");
}

void kernelDriver::callOnce()
{
   if (ioctl(driverfd, KERNINST_CALLONCE, NULL) < 0)
      assert(!"kernelDriver::callOnce() - ioctl failed");
}

#if (defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4))
void kernelDriver::toggleDebugBits(uint32_t bitmask)
{
   if (ioctl(driverfd, KERNINST_TOGGLE_DEBUG_BITS, &bitmask) < 0)
      assert(!"kernelDriver::toggleDebugBits() - ioctl failed");
}

void kernelDriver::toggleEnableBits(uint32_t bitmask)
{
   if (ioctl(driverfd, KERNINST_TOGGLE_ENABLE_BITS, &bitmask) < 0)
      assert(!"kernelDriver::toggleEnableBits() - ioctl failed");
}
#endif
    
// Return callees recorded for a given indirect call site.
// For each callee we return its address followed by the number of times it
// was called.
pdvector<kptr_t> kernelDriver::getCalleesForSite(kptr_t siteAddr) const
{
    struct kernInstCallSiteInfo csi;

    csi.addrFrom = siteAddr;
    if (ioctl(driverfd, KERNINST_KNOWN_CALLEES_SIZE, &csi) < 0) {
	assert(false && "KERNINST_KNOWN_CALLEES_SIZE ioctl failed");
    }

    unsigned num_entries = 2 * csi.num_callees; // (addr,count) pairs
    kptr_t *buffer = new kptr_t[num_entries];
    csi.buffer = (kptr_t)buffer;
    if (ioctl(driverfd, KERNINST_KNOWN_CALLEES_GET, &csi) < 0) {
	assert(false && "KERNINST_KNOWN_CALLEES_GET ioctl failed");
    }

    pdvector<kptr_t> callees;
    for (unsigned i=0; i<num_entries; i++) {
	callees.push_back(buffer[i]);
    }

    delete[] buffer;

    return callees;
}

// Read the value of a hardware counter across all cpus
pdvector<uint64_t> kernelDriver::readHwcAllCPUs(unsigned type) const
{
    pdvector<uint64_t> samples;
#if (defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4))
    struct kernInstReadHwcAllCPUs s;
    s.type = type;
    s.num_cpus = cpuinfo.size();
#ifdef ppc64_unknown_linux2_4
    s.pad1 = 0;
#endif
    s.cpu_vals = new uint64_t[s.num_cpus];
    if (ioctl(driverfd, KERNINST_READ_HWC_ALL_CPUS, &s) < 0) {
        assert(false && "KERNINST_READ_HWC_ALL_CPUS ioctl failed");
    }
    for (unsigned i=0; i<s.num_cpus; i++) {
	samples.push_back(s.cpu_vals[i]);
    }

    delete[] s.cpu_vals;
#elif defined(sparc_sun_solaris2_7)
    cerr << "WARNING: kernelDriver::readHwcAllCPUs() not implemented on sparc/solaris\n";
#endif
    return samples;
}
