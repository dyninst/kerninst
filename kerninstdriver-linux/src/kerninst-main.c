/*
 * kerninst-main.c - implementation of kerninst driver for Linux 2.6
 */

#include "kernInstIoctls.h"
#include "calleeCounter.h"
#include "physMem.h"
#include "symbol_table.h"
#include "kerninst.h"

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/smp.h> // smp_num_cpus
#include <asm/uaccess.h>
#include <asm/processor.h>

#ifdef i386_unknown_linux2_4
#include <asm/msr.h> // rdtsc()
#include <asm/desc.h>
extern struct cpuinfo_x86 boot_cpu_data;
#endif

/* init/exit functions */

int init_kerninst(void)
{
   kerninst_major = register_chrdev(0, /* for dynamic major assignment */
                                    "kerninst", &kerninst_file_ops);

   if( kerninst_major < 0 ) {
      printk(KERN_WARNING "kerninst: error - unable to obtain device major number\n");
      return kerninst_major;
   }

   kerninst_debug_bits = 0; /* no debug output by default */
   kerninst_enable_bits = KERNINST_ENABLE_ALL; /* enable everything */

#ifdef i386_unknown_linux2_4
   get_idt_base_addr();
   get_int3_handler(1); /* 1 == first call */

   if(boot_cpu_data.x86_vendor == X86_VENDOR_INTEL) {
      if(boot_cpu_data.x86 == 15) // family = P4
         kerninst_enable_bits |= KERNINST_ENABLE_P4COUNTERS;
   }
#endif

   kerninst_init_callee_counter();

   return 0;
}

void cleanup_kerninst(void)
{
#ifdef i386_unknown_linux2_4
   if( int3_handler_installed )
      set_int3_handler(do_int3_addr);
#endif

   kerninst_delete_callee_counter();

   unregister_chrdev(kerninst_major, "kerninst");
}

module_init(init_kerninst);
module_exit(cleanup_kerninst);

/* file operations */

int kerninst_open(struct inode *node, struct file *filp)
{
   int i;
   spin_lock(&kerninst_use_lock);
   if( in_use ) {
      spin_unlock(&kerninst_use_lock);
      return -EBUSY; /* device already opened, only allow one user */
   }
   in_use++;
   spin_unlock(&kerninst_use_lock);

   printk(KERN_INFO "kerninst: kerninst_open\n");

   kerninst_reset_callee_counter();

   /* initialize structures */
   initialize_all_vtimers(&av_vtimers);
   stacklist_initialize(&sl_heap);
   hash_table_initialize(&ht_vtimers);
#ifdef i386_unknown_linux2_4
   hash_table_initialize(&ht_traps);
#endif

   /* initialize action logging */
   global_loggedAllocRequests = initialize_logged_allocRequests();
   global_loggedWrites = initialize_logged_writes();

#ifdef i386_unknown_linux2_4
   /* clear machine perfctr state */
   if(kerninst_enable_bits & KERNINST_ENABLE_P4COUNTERS) {
      int i;
      perfctr_val_t zero_val = {0,0,0};
      int rv = on_each_cpu(set_machine_state, &reset_perfctr_state, 0, 1);
      if(rv == 0) {
         for(i = 0; i < NUM_COUNTERS; i++) {
	    zero_val.ctr = COUNTER_ADDR(i);
            rv = on_each_cpu(set_perfctr_value, &zero_val, 0, 1);
	    if(rv != 0)
	       break;
         }
      }
      if(rv != 0) {
	 printk(KERN_WARNING "kerninst: unable to reset performance counter state, disabling support for counters\n");
         kerninst_enable_bits ^= KERNINST_ENABLE_P4COUNTERS;
      }
   }
#endif

   return 0;
}

int kerninst_release(struct inode *node, struct file *filp)
{
   int i;
   spin_lock(&kerninst_use_lock);
   in_use--;
   spin_unlock(&kerninst_use_lock);

   printk(KERN_INFO "kerninst: kerninst_release\n");

   /* destroy structures */
   destroy_all_vtimers(&av_vtimers);
   stacklist_destroy(&sl_heap);
   hash_table_destroy(&ht_vtimers);
#ifdef i386_unknown_linux2_4
   hash_table_destroy(&ht_traps);
#endif

   /* destroy logged actions */
   destroy_logged_allocRequests(global_loggedAllocRequests);
   destroy_logged_writes(global_loggedWrites);

#ifdef i386_unknown_linux2_4
   /* clear machine perfctr state */
   if(kerninst_enable_bits & KERNINST_ENABLE_P4COUNTERS) {
      int i;
      perfctr_val_t zero_val = {0,0,0};
      int rv = on_each_cpu(set_machine_state, &reset_perfctr_state, 0, 1);
      if(rv == 0) {
         for(i = 0; i < NUM_COUNTERS; i++) {
	    zero_val.ctr = COUNTER_ADDR(i);
            rv = on_each_cpu(set_perfctr_value, &zero_val, 0, 1);
	    if(rv != 0)
	       break;
         }
      }
      if(rv != 0) {
	 printk(KERN_WARNING "kerninst: unable to reset performance counter state, disabling support for counters\n");
         kerninst_enable_bits ^= KERNINST_ENABLE_P4COUNTERS;
      }
   }

   if( int3_handler_installed ) {
      int3_handler_installed = 0;
      set_int3_handler(do_int3_addr);
   }
#endif

   return 0;
}

int kerninst_ioctl(struct inode *node, struct file *filp,
		   unsigned cmd, u_long arg)
{
   /* the inode and file structs correspond to the file descriptor
      used in the call to ioctl, cmd is the same as in the call, and
      arg is either an integer or an address corresponding to the
      optional arg parameter to ioctl
   */
   u_long addr;
   int retval = 0;

   switch(cmd) {
   case KERNINST_GET_OFFSET_INFO: {
      struct kernInstOffsetInfo output_args;
      output_args.thr_task_offset = &((struct thread_info *)0)->task;
      output_args.thr_cpu_offset = &((struct thread_info *)0)->cpu;
      output_args.task_pid_offset = &((struct task_struct *)0)->pid;
      output_args.thr_size = THREAD_SIZE;
      copy_to_user((void *)arg, (void *)&output_args, sizeof(output_args));
      break;
   }
   case KERNINST_ALLOC_BLOCK: {
      struct kernInstAllocStruct input_output_args;
      copy_from_user((void *)&input_output_args, (void *)arg, 
                     sizeof(input_output_args));
      retval = kerninst_alloc_block(global_loggedAllocRequests, 
                                    &input_output_args);
      if(retval == 0) {
         if(kerninst_debug_bits & KERNINST_DEBUG_MEMORY)
            printk(KERN_INFO "kerninst: KERNINST_ALLOC_BLOCK nbytes=%d, allocType=%d, result=0x%08lx\n",
                   input_output_args.nbytes,
                   input_output_args.allocType,
                   (unsigned long)input_output_args.result);
      
         copy_to_user((void *)arg, (void *)&input_output_args,
                      sizeof(input_output_args));
      }
      break;
   }
   case KERNINST_FREE_BLOCK: {
      struct kernInstFreeStruct input_args;
      copy_from_user((void *)&input_args, (void *)arg, sizeof(input_args));
      retval = kerninst_free_block(global_loggedAllocRequests, &input_args);
      if(retval == 0) {
         if(kerninst_debug_bits & KERNINST_DEBUG_MEMORY)
            printk(KERN_INFO "kerninst: KERNINST_FREE_BLOCK nbytes=%d, kernelAddr=0x%08lx\n",
                   input_args.nbytes,
                   (unsigned long)input_args.kernelAddr);
      }
      break;
   }
   case KERNINST_SYMTAB_STUFF_SIZE: {
      unsigned total_nbytes;
      total_nbytes = kerninst_symtab_size();
      if(kerninst_debug_bits & KERNINST_DEBUG_SYMTAB)
         printk(KERN_INFO "kerninst: KERNINST_SYMTAB_STUFF_SIZE size=%d bytes\n", total_nbytes);
      copy_to_user((void *)arg, (void*)&total_nbytes, sizeof(unsigned));
      break;
   }
   case KERNINST_SYMTAB_STUFF_GET: {
      unsigned actual_nbytes;
      struct kernInstSymtabGetStruct *destptr;
      struct kernInstSymtabGetStruct getstruct;
      copy_from_user((void *)&getstruct, (void *)arg, sizeof(getstruct));
      actual_nbytes = kerninst_symtab_get(getstruct.buffer, getstruct.buffer_size);
      destptr = (struct kernInstSymtabGetStruct *)arg;
      copy_to_user((void *)&(destptr->actual_amt_copied), 
		   (void*)&actual_nbytes, sizeof(unsigned));
      break;
   }
   case KERNINST_WRITE1INSN: {
      struct kernInstWrite1Insn s;
      char newval[16];
      copy_from_user((void *)&s, (void *)arg, sizeof(s));
      copy_from_user((void *)&newval[0], (void *)s.bytes, s.length);
      retval = write1Insn(s.destAddrInKernel, &newval[0], s.length);
      if(retval == 0) {
         if(kerninst_debug_bits & KERNINST_DEBUG_MEMORY)
            printk(KERN_INFO "kerninst: KERNINST_WRITE1INSN nbytes=%d @ addr=0x%08lx\n",
                   s.length, (unsigned long)s.destAddrInKernel);
      }
      break;
   }
   case KERNINST_WRUNDOABLE_REG: {
      struct kernInstWrite1UndoableInsn s;
      char newval[16];
      copy_from_user((void *)&s, (void *)arg, sizeof(s));
      s.origval = (char*) kmalloc(16, GFP_KERNEL);
      copy_from_user((void *)&newval[0], (void *)s.newval, s.length);
      retval = write1UndoableInsn(global_loggedWrites,
                                  s.destAddrInKernel,
                                  &newval[0],
                                  s.length,
                                  s.howSoon,
                                  s.origval);
      if(retval == 0) {
         if(kerninst_debug_bits & KERNINST_DEBUG_MEMORY)
            printk(KERN_INFO "kerninst: KERNINST_WRUNDOABLE_REG nbytes=%d @ addr=0x%08lx\n",
                   s.length, (unsigned long)s.destAddrInKernel);
      
         copy_to_user((void *)((struct kernInstWrite1UndoableInsn*)arg)->origval, (void *)s.origval, s.length);
      }
      break;
   }
   case KERNINST_UNDO_WR_REG: {
      struct kernInstUndo1Write s;
      char oldval[16];
      copy_from_user((void *)&s, (void *)arg, sizeof(s));
      copy_from_user((void *)&oldval[0], (void *)s.oldval_to_set_to, s.length);
      retval = undoWrite1Insn(global_loggedWrites,
                              s.addr,
                              s.length,
                              &oldval[0]);
      if(retval == 0) {
         if(kerninst_debug_bits & KERNINST_DEBUG_MEMORY)
            printk(KERN_INFO "kerninst: KERNINST_UNDO_WR_REG nbytes=%d @ addr=0x%08lx\n",
                   s.length, (unsigned long)s.addr);
      }
      break;
   }
   case KERNINST_CHG_UNDO_HOWSOON: {
      struct kernInstChangeUndoHowSoon s;
      copy_from_user((void *)&s, (void *)arg, sizeof(s));
      retval = changeUndoableInsnHowSoon(global_loggedWrites, s.addrInKernel, 
                                         s.newHowSoon);
      break;
   }
   case KERNINST_VIRTUAL_TO_PHYSICAL_ADDR: {
      break;
   }
   case KERNINST_KNOWN_CALLEES_SIZE: {
      struct kernInstCallSiteInfo csi;
      copy_from_user((void*)&csi, (void*)arg, sizeof(csi));
      csi.num_callees = kerninst_num_callees_for_site(csi.addrFrom);
      copy_to_user((void*)arg, (void*)&csi, sizeof(csi));
      break;
   }
   case KERNINST_KNOWN_CALLEES_GET: {      
      struct kernInstCallSiteInfo csi;
      unsigned num_callees;
      unsigned bufsize;
      kptr_t *callee_addrs;
      copy_from_user((void*)&csi, (void*)arg, sizeof(csi));
      num_callees = kerninst_num_callees_for_site(csi.addrFrom);
      if (num_callees != csi.num_callees) {
         printk(KERN_WARNING "KERNINST_KNOWN_CALLEES_GET: mismatch on num_callees\n");
         retval = -EIO;
         break;
      }
      if (num_callees == 0) {
         retval = 0;
         break;
      }
      bufsize = 2 * num_callees * sizeof(kptr_t);
      callee_addrs = (kptr_t *)kmalloc(bufsize, GFP_KERNEL);
      if (callee_addrs == NULL) {
         printk(KERN_WARNING "KERNINST_KNOWN_CALLEES_GET: kmalloc(%u) error while getting callees at 0x%08lx\n", bufsize, (unsigned long)csi.addrFrom);
         retval = -ENOMEM;
         break;
      }
      kerninst_get_callees_for_site(csi.addrFrom, num_callees, callee_addrs);
      copy_to_user((void*)csi.buffer, (void*)callee_addrs, bufsize);
      kfree(callee_addrs);
      break;
   }
   case KERNINST_GET_TICK_RAW: {
      unsigned *dest = (unsigned*)arg;
      unsigned tsc_low=0, tsc_high=0;
      rdtsc(tsc_low, tsc_high);
      if(kerninst_debug_bits & KERNINST_DEBUG_TICK)
         printk(KERN_INFO "kerninst: KERNINST_GET_TICK_RAW - tsc_low=%08x, tsc_high=%08x\n",
	        tsc_low, tsc_high);
      copy_to_user((void*)dest, (void*)&tsc_low, sizeof(unsigned));
      copy_to_user((void*)(dest+1), (void*)&tsc_high, sizeof(unsigned));
      break;
   }
   case KERNINST_GET_ALLVTIMERS_ADDR: {
      kptr_t the_addr = (kptr_t)&av_vtimers;
      copy_to_user((void *)arg, (void *)&the_addr, sizeof(kptr_t));
      break;
   }
   case KERNINST_ADD_TO_ALLVTIMERS: {
      kptr_t vtimer_addr;
      retval = get_user(vtimer_addr, (kptr_t *)arg);
      if(kerninst_debug_bits & KERNINST_DEBUG_VTIMERS)
         printk(KERN_INFO "kerninst: KERNINST_ADD_TO_ALLVTIMERS vtimer_addr=%08lx\n",
                (unsigned long)vtimer_addr);
      if(retval == 0) {
         retval = addto_all_vtimers(&av_vtimers, (void*)vtimer_addr);
      }
      break;
   }
   case KERNINST_REMOVE_FROM_ALLVTIMERS: {
      kptr_t vtimer_addr;
      retval = get_user(vtimer_addr, (kptr_t *)arg);
      if(kerninst_debug_bits & KERNINST_DEBUG_VTIMERS)
         printk(KERN_INFO "kerninst: KERNINST_REMOVE_FROM_ALLVTIMERS vtimer_addr=%08lx\n",
                (unsigned long)vtimer_addr);
      if(retval == 0) {
         retval = removefrom_all_vtimers(&av_vtimers, (void*)vtimer_addr);
      }
      break;
   }
   case KERNINST_CLEAR_CLIENT_STATE: {
      // clear the client structures and re-initialize
      spin_lock(&kerninst_cswitch_lock);
      destroy_all_vtimers(&av_vtimers);
      stacklist_destroy(&sl_heap);
      hash_table_destroy(&ht_vtimers); // actually does initialization too
#ifdef i386_unknown_linux2_4
      hash_table_destroy(&ht_traps); // actually does initialization too
#endif
      initialize_all_vtimers(&av_vtimers);
      stacklist_initialize(&sl_heap);
      spin_unlock(&kerninst_cswitch_lock);
      if(kerninst_debug_bits & KERNINST_DEBUG_IOCTLS)
         printk(KERN_INFO "kerninst: KERNINST_CLEAR_CLIENT_STATE\n");
      break;
   }

#ifdef i386_unknown_linux2_4
   case KERNINST_REGISTER_TRAP_PATCH_MAPPING: {
      struct kernInstTrapMapping m = {0,0};
      copy_from_user((void*)&m, (void*)arg, sizeof(m));
      retval = install_trap_mapping(m.trapAddr, m.patchAddr);
      break;
   }
   case KERNINST_UPDATE_TRAP_PATCH_MAPPING: {
      struct kernInstTrapMapping m = {0,0};
      copy_from_user((void*)&m, (void*)arg, sizeof(m));
      retval = update_trap_mapping(m.trapAddr, m.patchAddr);
      break;
   }
   case KERNINST_UNREGISTER_TRAP_PATCH_MAPPING: {
      get_user(addr, (u_long *)arg);
      retval = uninstall_trap_mapping(addr);
      break;
   }
   case KERNINST_GET_PERF_CTR_STATE: {
      perfctr_state_t the_state;
      if(kerninst_enable_bits & KERNINST_ENABLE_P4COUNTERS) {
         if(kerninst_debug_bits & KERNINST_DEBUG_P4COUNTERS)
            printk(KERN_INFO "kerninst: KERNINST_GET_PERF_CTR_STATE\n");
         retrieve_machine_state(&the_state);
         copy_to_user((void *)arg, (void *)&the_state, sizeof(the_state));
      }
      else {
         printk(KERN_INFO "kerninst:  KERNINST_GET_PERF_CTR_STATE not supported on the machine\n");
         copy_to_user((void *)arg, (void *)&reset_perfctr_state, 
                      sizeof(the_state));
      }
      break;
   }
   case KERNINST_SET_PERF_CTR_STATE: {
      perfctr_state_t the_state;
      int rv;
      if(kerninst_enable_bits & KERNINST_ENABLE_P4COUNTERS) {
         if(kerninst_debug_bits & KERNINST_DEBUG_P4COUNTERS)
            printk(KERN_INFO "kerninst: KERNINST_SET_PERF_CTR_STATE\n");
         copy_from_user((void *)&the_state, (void *)arg, sizeof(the_state));
         rv = on_each_cpu(set_machine_state, &the_state, 0, 1);
	 if(rv != 0) {
	    printk(KERN_WARNING "kerninst: KERNINST_SET_PERF_CTR_STATE failed to set state on all cpus, disabling counter support\n");
            kerninst_enable_bits ^= KERNINST_ENABLE_P4COUNTERS;
	 }
      }
      else {
         printk(KERN_INFO "kerninst: KERNINST_SET_PERF_CTR_STATE not supported on this machine\n");
      }
      break;
   }
   case KERNINST_GET_PERF_CTR_VAL: {
      perfctr_val_t the_val;
      if(kerninst_enable_bits & KERNINST_ENABLE_P4COUNTERS) {
         copy_from_user((void*)&the_val, (void*)arg, sizeof(the_val));
         if(kerninst_debug_bits & KERNINST_DEBUG_P4COUNTERS)
            printk(KERN_INFO "kerninst: KERNINST_GET_PERF_CTR_VAL ctr==%d\n",
                   the_val.ctr);
         the_val.ctr = COUNTER_ADDR(the_val.ctr);
         get_perfctr_value(&the_val);
      }
      else {
         printk(KERN_INFO "kerninst: KERNINST_GET_PERF_CTR_VAL not supported on this machine\n");
         the_val.ctr = 0;
         the_val.high = 0;
         the_val.low = 0;
      }
      copy_to_user((void *)arg, (void *)&the_val, sizeof(the_val));
      break;
   }
   case KERNINST_SET_PERF_CTR_VAL: {
      perfctr_val_t the_val;
      int rv;
      if(kerninst_enable_bits & KERNINST_ENABLE_P4COUNTERS) {
         copy_from_user((void*)&the_val, (void*)arg, sizeof(the_val));
         if(kerninst_debug_bits & KERNINST_DEBUG_P4COUNTERS)
            printk(KERN_INFO "kerninst: KERNINST_SET_PERF_CTR_VAL ctr==%d\n",
                   the_val.ctr);
         the_val.ctr = COUNTER_ADDR(the_val.ctr);
         rv = on_each_cpu(set_perfctr_value, &the_val, 0, 1);
	 if(rv != 0) {
	    printk(KERN_WARNING "kerninst: KERNINST_SET_PERF_CTR_VAL failed to set counter value on all cpus, disabling counter support\n");
            kerninst_enable_bits ^= KERNINST_ENABLE_P4COUNTERS;
	 }
      }
      else {
         printk(KERN_INFO "kerninst: KERNINST_SET_PERF_CTR_VAL not supported on this machine\n");
      }
      break;
   }

   case KERNINST_GET_BUG_UD2_SIZE: {
      unsigned bugsize = 0;
      unsigned ud2size = 2;
      retval = get_user(bugsize, (unsigned *)arg);
        // note that bugsize from user space better be > 0, or we'll hit BUG()
      if(bugsize)
         goto after;
   before:
      BUG();
   after:
      bugsize = (&&after - &&before);
      if(bugsize != 2) {
         if((*(unsigned char*)((char*)&&before) == (unsigned char)0x0F) &&
            (*(unsigned char*)((char*)&&before + 1) == (unsigned char)0x0B))
               ud2size = bugsize;
      }
      retval = put_user(ud2size, (unsigned *)arg);
      break;
   }
#endif //i386_unknown_linux2_4

   case KERNINST_READ_HWC_ALL_CPUS: {
      struct kernInstReadHwcAllCPUs s;
      struct readHwcArgs read_hwc_args;
      uint64_t *cpu_vals = NULL;
      unsigned bufsize = 0;
      copy_from_user((void*)&s, (void*)arg, sizeof(s));
      bufsize = sizeof(uint64_t) * s.num_cpus;
      cpu_vals = (uint64_t*)kmalloc(bufsize, GFP_KERNEL);
      read_hwc_args.cpu_vals = cpu_vals;
      read_hwc_args.type = s.type;     
#ifdef i386_unknown_linux2_4
      on_each_cpu(read_hwc_all_cpus, (void*)&read_hwc_args, 0, 1);
#elif defined(ppc64_unknown_linux2_4)
      kerninst_on_each_cpu_ptr(read_hwc_all_cpus, (void*)&read_hwc_args, 0, 1);
#endif
      copy_to_user((void*)s.cpu_vals, (void*)cpu_vals, bufsize);
      kfree(cpu_vals);
      break;
   }

   case KERNINST_CALLONCE: {
      int i = kerninst_call_once();
      if(kerninst_debug_bits & KERNINST_DEBUG_IOCTLS) {
         printk(KERN_INFO "kerninst: KERNINST_CALLONCE result==%d\n", i);
      }
      break;
   }
   case KERNINST_TOGGLE_ENABLE_BITS: {
      unsigned flag = 0;
      retval = get_user(flag, (unsigned *)arg);
      kerninst_enable_bits ^= flag;
      printk(KERN_INFO "kerninst: KERNINST_TOGGLE_ENABLE_BITS - kerninst_enable_bits = %08x\n", kerninst_enable_bits);
      break;
   }
/*    case KERNINST_SET_ALL_DEBUG_BITS: { */
/*       retval = get_user(kerninst_debug_bits, (unsigned *)arg); */
/*       printk(KERN_INFO "kerninst: KERNINST_SET_ALL_DEBUG_BITS - kerninst_debug_bits = %08x\n", kerninst_debug_bits); */
/*       break; */
/*    } */
   case KERNINST_TOGGLE_DEBUG_BITS: {
      unsigned flag = 0;
      retval = get_user(flag, (unsigned *)arg);
      kerninst_debug_bits ^= flag;
      printk(KERN_INFO "kerninst: KERNINST_TOGGLE_DEBUG_BITS - kerninst_debug_bits = %08x\n", kerninst_debug_bits);
      break;
   }
   case KERNINST_RESET: {
      spin_lock(&kerninst_use_lock);
      in_use = 1; /* still one user, the one that sent this ioctl */
      spin_unlock(&kerninst_use_lock);
      break;
   }
   default:
      printk(KERN_WARNING "kerninst: undefined ioctl cmd=%d\n", cmd);
      retval = -ENOTTY; /* error, undefined ioctl */
      break;
   }

   return retval;
}

/* module functions */

int kerninst_call_me(int i, int j, int k)
{
   int iter;
   int rv = j;
   for(iter=0; iter<=i; iter++) {
      if(iter%3 == 0)
         rv += k;
      if(iter%4 == 0)
         rv -= j;
   }
   return rv; // expected return val is 19458
}

void benchmark_instrumentation(void);

int kerninst_call_once(void)
{
   int rv = 0;
   if(kerninst_debug_bits & KERNINST_DEBUG_IOCTLS)
      printk(KERN_INFO "kerninst: in kerninst_call_once()\n");
#if 1
   rv = kerninst_call_me(123, 456, 789);
#else
   benchmark_instrumentation();
#endif
   return rv;
}

int kerninst_call_once_v2(void)
{
   if(kerninst_debug_bits & KERNINST_DEBUG_IOCTLS)
      printk(KERN_INFO "kerninst: in kerninst_call_once_v2()\n");
   return kerninst_call_me(123, 456, 789);
}

#ifdef i386_unknown_linux2_4
void benchmark_instrumentation(void)
{
   unsigned long long start = 0, stop = 0, total = 0;
   unsigned exec_count = 1000;
   unsigned i = 0;
   for(; i < exec_count; i++) {
      rdtscll(start);
#if 1 /* profile traps */
      __asm__ __volatile__ ("addb $0, %%al\n" : 
                            /*no inputs*/ : 
                            /*no outputs*/);
#else /* profile branches */
      __asm__ __volatile__ ("addl $0x10000000, %%eax\n" : 
                            /*no inputs*/ : 
                            /*no outputs*/);
#endif
      rdtscll(stop);
      total += stop - start;
   }
   printk(KERN_INFO "kerninst: benchmark_instrumentation - total cycles at inst point is %lld for %d executions\n", total, exec_count);
}

void get_idt_base_addr(void)
{
   unsigned space[2];
   __asm__ __volatile__ ("sidt %0\n": :"m" (*((short *)space+1)));
   idt_base = (struct desc_struct *)(space[1]);
}

static unsigned char kprobes_int3 = 0;

void get_int3_handler(unsigned first)
{
   u_long part1, part2 = 0;
   struct desc_struct *temp;
   unsigned char pushed_val = 0;

   if( first ) {
      if( idt_base == NULL ) get_idt_base_addr();

      temp = idt_base + 3;
      part1 = temp->a;
      part2 = temp->b;
      int3_handler_addr = (part2 & 0xFFFF0000) + (part1 & 0x0000FFFF);

      pushed_val = *(((unsigned char*)int3_handler_addr)+1);
      if(pushed_val == 0xFF) {
	 kprobes_int3 = 1;
         /* kprobes patch has been applied, so address of do_int3 can be
            calculated from the call to it at offset 0x1a from
            int3_handler_addr */
         do_int3_addr = *(u_long *)(((char*)int3_handler_addr)+0x1b) + 
                        int3_handler_addr + 0x1f;
      }
      else {
	 /* address of do_int3 is pushed in 2nd instruction, and located
            at offset 3 from int3_handler_addr */
         do_int3_addr = *(u_long *)(((char*)int3_handler_addr)+3);
      }
      do_int3 = (void (*)(struct pt_regs *, long))do_int3_addr;
   }
}

int set_int3_handler(u_long addr)
{
   /* don't set if address given is null */
   if( (void *)addr == NULL ) return 1;
 
   if(kprobes_int3) {
      addr = addr - (int3_handler_addr + 0x1f);
      *(u_long *)(((char*)int3_handler_addr)+0x1b) = addr;
   }
   else
      *(u_long *)(((char*)int3_handler_addr)+3) = addr;
 
   if(kerninst_debug_bits & KERNINST_DEBUG_TRAPS)
      printk(KERN_INFO "kerninst: set int3 handler to call %08lx\n", addr);
   return 0;
}

int install_trap_mapping(u_long trap_addr, u_long patch_addr)
{
   int ret;
 
   /* install new int3 handler if not already installed */
   if(! int3_handler_installed) {
      set_int3_handler((u_long)kerninst_int3_handler);
      int3_handler_installed = 1;
   }

   if(kerninst_debug_bits & KERNINST_DEBUG_TRAPS)
      printk(KERN_INFO "kerninst: adding mapping for trap @ 0x%08lx, corresponding patch @ 0x%08lx\n", trap_addr, patch_addr);

   /* insert mapping into list */
   ret = hash_table_add(&ht_traps, (void*)patch_addr, trap_addr);
   if(ret == 0)
      num_traps_installed++;

   return ret;
}

int update_trap_mapping(u_long trap_addr, u_long patch_addr)
{
   int ret;
   if(kerninst_debug_bits & KERNINST_DEBUG_TRAPS)
      printk(KERN_INFO "kerninst: updating mapping for trap @ 0x%08lx, new patch @ 0x%08lx\n", trap_addr, patch_addr);
   ret = hash_table_update(&ht_traps, trap_addr, (void*)patch_addr);
   return ret;
}

int uninstall_trap_mapping(u_long trap_addr)
{
   void *patch_addr;

   if(kerninst_debug_bits & KERNINST_DEBUG_TRAPS)
      printk(KERN_INFO "kerninst: removing mapping for trap @ 0x%08lx\n", trap_addr);

   /* remove mapping from list, if it exists */
   patch_addr = hash_table_remove(&ht_traps, trap_addr);
   if(patch_addr != NULL) {
      num_traps_installed--;

      /* uninstall int3 handler if there are no more traps */
      if(! num_traps_installed) {
         if( int3_handler_installed ) {
            set_int3_handler(do_int3_addr);
            int3_handler_installed--;
         }
      }
      return 0;
   }
   return -1;
}

u_long match_addr(u_long trap_addr)
{
   void *patch_addr = hash_table_lookup(&ht_traps, trap_addr);
   return (u_long)patch_addr;
}

void kerninst_int3_handler(struct pt_regs *regs, long error_code)
{
   u_long patch_addr = 0;
   u_long trap_addr = (u_long)regs->eip - 1; // '-1' since int3 only one byte
   if( (!(regs->xcs & 3)) && 
       ((patch_addr = match_addr(trap_addr)) != 0) ) {
      /* code segment specifes kernel & lookup of trap_addr successful */
      if(kerninst_debug_bits & KERNINST_DEBUG_TRAPS)
         printk(KERN_INFO "kerninst: handling trap @ 0x%08lx, corresponding patch @ 0x%08lx\n", trap_addr, patch_addr);

      (u_long)regs->eip = patch_addr; /* return to patch, not trap_addr+1 */
   }
   else { /* not our stuff, call original do_int3 handler */
      if(kerninst_debug_bits & KERNINST_DEBUG_TRAPS)
         printk(KERN_INFO "kerninst: passing on int3 @ 0x%08lx to original handler\n", trap_addr);
      do_int3(regs, error_code);
   }
}
#endif // i386_unknown_linux2_4


/* end of kerninst.c */
