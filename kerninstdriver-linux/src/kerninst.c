/*
 * kerninst.c - implementation of kerninst driver for Linux 2.4
 */

#include "kernInstIoctls.h"
#include "calleeCounter.h"
#include "physMem.h"
#include "symbol_table.h"
#include "kerninst.h"

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/processor.h>

#ifdef i386_unknown_linux2_4

#include <linux/smp.h> // smp_num_cpus
#include <asm/msr.h> // rdtsc()
#include <asm/desc.h>
extern struct cpuinfo_x86 boot_cpu_data;

#elif defined(ppc64_unknown_linux2_4)

#include <asm/pmc.h>
#include <sys/mman.h>
#include <asm/processor.h> //tb()
#include <linux/syscall.h> //sys_ioctl
#include <asm/naca.h>
#include <asm/pgtable.h>
#include <asm/hvcall.h>
#include <asm/iSeries/LparData.h>
#include <asm/iSeries/HvCallHpt.h>
#include "kerninst-ppc64.h"
#endif

int init_kerninst(void)
{
   kerninst_major = register_chrdev(0, /* for dynamic major assignment */
                                    "kerninst", &kerninst_file_ops);

   if( kerninst_major < 0 ) {
      printk(KERN_WARNING "kerninst: error - unable to obtain device major number\n");
      return kerninst_major;
   }

   kerninst_debug_bits = 0; /* no debugging by default */
   kerninst_enable_bits = KERNINST_ENABLE_ALL; /* enable everything */
//   kerninst_enable_bits &= ~KERNINST_ENABLE_CONTEXTSWITCH;

#ifdef i386_unknown_linux2_4
   get_idt_base_addr();
   get_int3_handler(1); /* 1 == first call */

   if(boot_cpu_data.x86_vendor == X86_VENDOR_INTEL) {
      if(boot_cpu_data.x86 == 15) // family = P4
         kerninst_enable_bits |= KERNINST_ENABLE_P4COUNTERS;
   }
#endif

#ifdef ppc64_unknown_linux2_4    
   kerninst_malloc_ptr = inter_module_get("kerninst_malloc");
   kerninst_free_ptr = inter_module_get("kerninst_free");
   init_requested_pointers_to_kernel_ptr = inter_module_get(
       "init_requested_pointers_to_kernel");
   kerninst_on_each_cpu_ptr = inter_module_get("kerninst_on_each_cpu");

   unsigned int pvr = _get_PVR();
   if ( (PVR_VER(pvr) == PV_POWER4) || PVR_VER(pvr) == PV_POWER4p) {
      kerninst_enable_bits |= KERNINST_ENABLE_POWER4COUNTERS;
      ppc64_enable_pmcs_ptr = inter_module_get("ppc64_enable_pmcs");
   }
   if (!kerninst_malloc_ptr ) 
     printk("failed intermodule function request\n");
   
   registerIoctl32Conversions();
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

#ifdef ppc64_unknown_linux2_4   
   inter_module_put("kerninst_malloc");
   inter_module_put("kerninst_free");
   inter_module_put("init_requested_pointers_to_kernel");
   inter_module_put("kerninst_on_each_cpu");
   if (kerninst_enable_bits & KERNINST_ENABLE_POWER4COUNTERS )
      inter_module_put("ppc64_enable_pmcs");

   unregisterIoctl32Conversions();
#endif
}

module_init(init_kerninst);
module_exit(cleanup_kerninst);

/* file operations */

int kerninst_open(struct inode *node, struct file *filp)
{
   spin_lock(&kerninst_use_lock);
   if(in_use) {
      if( in_use > 1 ) {
         spin_unlock(&kerninst_use_lock);
         return 0; /* device already opened, but use count is screwy, 
                      so allow one user to enter in order to reset */
      }
      spin_unlock(&kerninst_use_lock);
      return -EBUSY; /* device already opened, only allow one user */
   }
   in_use++;
   spin_unlock(&kerninst_use_lock);
   MOD_INC_USE_COUNT;

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

   /* clear machine perfctr state */
#ifdef i386_unknown_linux2_4
   if(kerninst_enable_bits & KERNINST_ENABLE_P4COUNTERS) {
      int i;
      perfctr_val_t zero_val = {0,0,0};
      int rv = on_each_cpu(set_machine_state, (void*)&reset_perfctr_state, 0, 1);
      if(rv == 0) {
         for(i = 0; i < NUM_COUNTERS; i++) {
	    zero_val.ctr = COUNTER_ADDR(i);
            rv = on_each_cpu(set_perfctr_value, (void*)&zero_val, 0, 1);
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
   spin_lock(&kerninst_use_lock);
   in_use--;
   spin_unlock(&kerninst_use_lock);
   MOD_DEC_USE_COUNT;

   printk(KERN_INFO "kerninst: kerninst_release\n");

   /* destroy structures */
   destroy_all_vtimers(&av_vtimers);
   stacklist_destroy(&sl_heap);
   hash_table_destroy(&ht_vtimers);
#ifdef i386_unknown_linux2_4
   hash_table_destroy(&ht_traps);
#endif

   /* destroy logged actions */
   destroy_logged_writes(global_loggedWrites);
   destroy_logged_allocRequests(global_loggedAllocRequests);

#ifdef i386_unknown_linux2_4
   /* clear machine perfctr state */
   if(kerninst_enable_bits & KERNINST_ENABLE_P4COUNTERS) {
      int i;
      perfctr_val_t zero_val = {0,0,0};
      int rv = on_each_cpu(set_machine_state, (void*)&reset_perfctr_state, 0, 1);
      if(rv == 0) {
         for(i = 0; i < NUM_COUNTERS; i++) {
	    zero_val.ctr = COUNTER_ADDR(i);
            rv = on_each_cpu(set_perfctr_value, (void*)&zero_val, 0, 1);
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
#if defined(USE_TASK_CPU)
      output_args.thr_cpu_offset = 
         (unsigned)(&(((struct task_struct*)0)->cpu));
#elif defined(USE_TASK_PROC)
      output_args.thr_cpu_offset = 
         (unsigned)(&(((struct task_struct*)0)->processor));
#else
# error "Either USE_TASK_CPU or USE_TASK_PROC must be defined"
#endif

#ifdef ppc64_unknown_linux2_4
      output_args.t_pacacurrent_offset = (unsigned) &((struct paca_struct *)0)->xCurrent;
      output_args.paca_access = PACA_ACCESS;
#endif
      output_args.thr_size = 8192; /* the number that should be negated 
                                      and "and"ed to %esp to get current */
      output_args.task_pid_offset = 
         (unsigned)(&(((struct task_struct*)0)->pid));
      
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
      unsigned total_nbytes = kerninst_symtab_size();
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
      } else {
         retval = -ENOMEM;
      }
      kfree(s.origval);
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
#ifdef i386_unknown_linux2_4
      unsigned *dest = (unsigned*)arg;
      unsigned tsc_low=0, tsc_high=0;
      rdtsc(tsc_low, tsc_high);
      if(kerninst_debug_bits & KERNINST_DEBUG_TICK)
         printk(KERN_INFO "kerninst: KERNINST_GET_TICK_RAW - tsc_low=%08x, tsc_high=%08x\n",
	        tsc_low, tsc_high);
      copy_to_user((void*)dest, (void*)&tsc_low, sizeof(unsigned));
      copy_to_user((void*)(dest+1), (void*)&tsc_high, sizeof(unsigned));
#elif defined(ppc64_unknown_linux2_4)
      long *dest = (long*)arg;
      long result;
      result = mftb();
      if(kerninst_debug_bits & KERNINST_DEBUG_TICK)
         printk(KERN_INFO "kerninst: KERNINST_GET_TICK_RAW - tb=%16lx\n", 
	        result);
      retval = put_user(result, (long *)dest);
#endif
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

#ifdef ppc64_unknown_linux2_4
   case KERNINST_PUT_INTERNAL_KERNEL_SYMBOLS: {
      kptr_t kernelptrs[KERNINST_NUM_REQUESTED_INTERNAL_SYMBOLS];
      copy_from_user((void *)kernelptrs, (void *)arg, 
		     KERNINST_NUM_REQUESTED_INTERNAL_SYMBOLS*sizeof(kptr_t));
      if (! init_requested_pointers_to_kernel_ptr ) {
         printk("Failed to initialize kernel function pointers,  please load kerninstmalloc module\n");
        retval = -ENOSYS; 
      }
      if ( (init_requested_pointers_to_kernel_ptr(kernelptrs)) < 0) {
         retval = -ENOSYS;
         break;
      }
      if (kerninst_enable_bits & KERNINST_ENABLE_POWER4COUNTERS) {
         kerninst_on_each_cpu_ptr(kerninst_enable_pmcs, NULL, 0, 1);
         struct kernInstPowerPerfCtrState curr_state;
         get_power_perfctr_state (&curr_state);
         kerninst_on_each_cpu_ptr(set_power_perfctr_state, 
                                  (void*)&curr_state, 0, 1); 
      }
      if(kerninst_debug_bits & KERNINST_DEBUG_IOCTLS)
         printk(KERN_INFO "kerninst: KERNINST_PUT_INTERNAL_KERNEL_SYMBOLS\n");
      break;
   }
   case KERNINST_FLUSH_ICACHE_RANGE: {
      struct kernInstFlushRange r;
      copy_from_user((void*)&r, (void*)arg, sizeof(r));
      if(kerninst_debug_bits & KERNINST_DEBUG_IOCTLS)
         printk(KERN_INFO "kerninst: KERNINST_FLUSH_ICACHE_RANGE addr=%lx, numbytes=%d\n", r.addr, r.numBytes);
      kerninst_on_each_cpu_ptr(kerninst_flush_icache_range, (void *) &r, 0, 1);
      break;
   }
   case KERNINST_GET_TICKS_PER_USEC: {
      copy_to_user((void *)arg, (void *)&tb_ticks_per_usec, sizeof(tb_ticks_per_usec));
      break;
   }
   case KERNINST_GET_POWER_PERF_CTR_STATE: {
      if (kerninst_enable_bits & KERNINST_ENABLE_POWER4COUNTERS) {
         struct kernInstPowerPerfCtrState the_state;
         if(kerninst_debug_bits & KERNINST_DEBUG_IOCTLS)
            printk(KERN_INFO "kerninst: KERNINST_GET_PERF_CTR_STATE\n");
         get_power_perfctr_state(&the_state);
         copy_to_user((void *)arg, (void *)&the_state, sizeof(struct kernInstPowerPerfCtrState));
      } else {
         printk(KERN_INFO "kerninst:  KERNINST_GET_POWER_PERF_CTR_STATE not supported on the machine\n");
      }
      break;
   }
   case KERNINST_SET_POWER_PERF_CTR_STATE: {
      if (kerninst_enable_bits & KERNINST_ENABLE_POWER4COUNTERS) {
         struct kernInstPowerPerfCtrState the_state;
         if(kerninst_debug_bits & KERNINST_DEBUG_IOCTLS)
               printk(KERN_INFO "kerninst: KERNINST_SET_PERFCTR_STATE\n");
            copy_from_user((void *)&the_state, (void *)arg, sizeof(struct kernInstPowerPerfCtrState));
            kerninst_on_each_cpu_ptr(set_power_perfctr_state, (void*)&the_state,
                                     0, 1);
      } else {
         printk(KERN_INFO "kerninst:  KERNINST_SET_POWER_PERF_CTR_STATE not supported on the machine\n");
      }
      break;
   }
#if 0
   case KERNINST_CSWITCH_TEST: {
      kerninst_enable_bits = kerninst_enable_bits & ~KERNINST_ENABLE_CONTEXTSWITCH;
      on_switch_in_test();
      on_switch_out_test();
      kerninst_enable_bits = kerninst_enable_bits | KERNINST_ENABLE_CONTEXTSWITCH;
      break;
   }
#endif
#endif // ppc64_unknown_linux2_4

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
	 rv = on_each_cpu(set_machine_state, (void*)&the_state, 0, 1);
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
         rv = on_each_cpu(set_perfctr_value, (void*)&the_val, 0, 1);
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
#endif // i386_unknown_linux2_4

   case KERNINST_CALLONCE: {
      int i = kerninst_call_once();
      if(kerninst_debug_bits & KERNINST_DEBUG_IOCTLS)
         printk(KERN_INFO "kerninst: KERNINST_CALLONCE result==%d\n", i);
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
      while( MOD_IN_USE ) MOD_DEC_USE_COUNT;
      MOD_INC_USE_COUNT; /* still one user, the one that sent this ioctl */
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

void benchmark_instrumentation();

int kerninst_call_once()
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

int kerninst_call_once_v2()
{
   int rv = 0;
   if(kerninst_debug_bits & KERNINST_DEBUG_IOCTLS)
      printk(KERN_INFO "kerninst: in kerninst_call_once_v2()\n");
   rv = kerninst_call_me(123, 456, 789);
   return rv;
}


// BEGIN X86-SPECIFIC CODE
#ifdef i386_unknown_linux2_4

void benchmark_instrumentation()
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

void get_idt_base_addr()
{
   unsigned space[2];
   __asm__ __volatile__ ("sidt %0\n": :"m" (*((short *)space+1)));
   idt_base = (struct desc_struct *)(space[1]);
}

void get_int3_handler(unsigned first)
{
   u_long part1, part2 = 0;
   struct desc_struct *temp;

   if( first ) {
      if( idt_base == NULL ) get_idt_base_addr();

      temp = idt_base + 3;
      part1 = temp->a;
      part2 = temp->b;
      int3_handler_addr = (part2 & 0xFFFF0000) + (part1 & 0x0000FFFF);
      do_int3_addr = *(u_long *)(((char*)int3_handler_addr)+3);
      do_int3 = (void (*)(struct pt_regs *, long))do_int3_addr;
   }
}

int set_int3_handler(u_long addr)
{
   /* don't set if address given is null */
   if( (void *)addr == NULL ) return 1;

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
      int3_handler_installed = 0;
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
            int3_handler_installed = 0;
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
#endif //i386_unknown_linux2_4


/* end of kerninst.c */
