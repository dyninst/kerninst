/* functionality only needed by power linux driver */

void* (*kerninst_malloc_ptr) (unsigned long size);
void (*kerninst_free_ptr)(void *ea);

int (*init_requested_pointers_to_kernel_ptr) (kptr_t kernelptrs[]);
int (*kerninst_on_each_cpu_ptr) (void (*func) (void *info), void *info, int nonatomic, int wait);
void (*ppc64_enable_pmcs_ptr)(void);

extern void kerninst_flush_icache_range (void *data);
void kerninst_touch_bolted (void *data) {
   volatile int touch;
   touch = *((int *) (data)); 
}

void kerninst_enable_pmcs(void *dummy) {
   ppc64_enable_pmcs_ptr();
}

void print_slb() {
   union {
      unsigned long word0;
      slb_dword0    data;
   } esid_data;
   union {
      unsigned long word0;
      slb_dword1    data;
   } vsid_data;
   unsigned long entry;
   for(entry = 0; entry < 64; entry++) {
      __asm__ __volatile__("slbmfee  %0,%1"
                                     : "=r" (esid_data) : "r" (entry));
      __asm__ __volatile__("slbmfev  %0,%1"
                                     : "=r" (vsid_data) : "r" (entry));
      printk("esid data is %lx, vsid data is %lx\n",esid_data.word0, vsid_data.word0);
   } 
}

void print_bats() {
   unsigned long batcontents=0;
   asm ("mfspr %[batu], 528" :[batu] "=r" (batcontents):);
   printk("ibatu 1 is %lx\n", batcontents);
   asm ("mfspr %[batu], 530" :[batu] "=r" (batcontents):);
   printk("ibatu 2 is %lx\n", batcontents);
   asm ("mfspr %[batu], 532" :[batu] "=r" (batcontents):);
   printk("ibatu 3 is %lx\n", batcontents);
   batcontents=0;
   asm ("mfspr %[batu], 534" :[batu] "=r" (batcontents):);
   printk("ibatu 4 is %lx\n", batcontents);
   asm ("mfspr %[batu], 536" :[batu] "=r" (batcontents):);
   printk("dbatu 1 is %lx\n", batcontents);
   asm ("mfspr %[batu], 538" :[batu] "=r" (batcontents):);
   printk("dbatu 2 is %lx\n", batcontents);
   asm ("mfspr %[batu], 540" :[batu] "=r" (batcontents):);
   printk("dbatu 3 is %lx\n", batcontents);
   asm ("mfspr %[batu], 542" :[batu] "=r" (batcontents):);
   printk("dbatu 4 is %lx\n", batcontents);
}


void registerIoctl32Conversions() 
{
   int ioctl_reg_err;
   /* Register ioctl conversions to 32-bit since the daemon is a 32-bit app */
   ioctl_reg_err = register_ioctl32_conversion(KERNINST_GET_OFFSET_INFO, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to register ioctl32 conversion\n"); 
   ioctl_reg_err = register_ioctl32_conversion(KERNINST_ALLOC_BLOCK, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to register ioctl32 conversion\n"); 
   ioctl_reg_err = register_ioctl32_conversion(KERNINST_FREE_BLOCK, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to register ioctl32 conversion\n"); 
   ioctl_reg_err = register_ioctl32_conversion(KERNINST_SYMTAB_STUFF_SIZE, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to register ioctl32 conversion\n"); 
   ioctl_reg_err = register_ioctl32_conversion(KERNINST_SYMTAB_STUFF_GET, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to register ioctl32 conversion\n"); 
   ioctl_reg_err = register_ioctl32_conversion(KERNINST_WRUNDOABLE_REG, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to register ioctl32 conversion\n"); 
   ioctl_reg_err = register_ioctl32_conversion(KERNINST_UNDO_WR_REG, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to register ioctl32 conversion\n"); 
   ioctl_reg_err = register_ioctl32_conversion(KERNINST_VIRTUAL_TO_PHYSICAL_ADDR, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to register ioctl32 conversion\n"); 
   ioctl_reg_err = register_ioctl32_conversion(KERNINST_KNOWN_CALLEES_SIZE, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to register ioctl32 conversion\n"); 
   ioctl_reg_err = register_ioctl32_conversion(KERNINST_KNOWN_CALLEES_GET, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to register ioctl32 conversion\n"); 
   ioctl_reg_err = register_ioctl32_conversion(KERNINST_GET_TICK_RAW, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to register ioctl32 conversion\n"); 
   ioctl_reg_err = register_ioctl32_conversion(KERNINST_CALLONCE, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to register ioctl32 conversion\n"); 
   ioctl_reg_err = register_ioctl32_conversion(KERNINST_RESET, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to register ioctl32 conversion\n"); 
   ioctl_reg_err = register_ioctl32_conversion(KERNINST_GET_ALLVTIMERS_ADDR, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to register ioctl32 conversion\n"); 
   ioctl_reg_err = register_ioctl32_conversion(KERNINST_ADD_TO_ALLVTIMERS, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to register ioctl32 conversion\n"); 
   ioctl_reg_err = register_ioctl32_conversion(KERNINST_REMOVE_FROM_ALLVTIMERS, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to register ioctl32 conversion\n"); 
   ioctl_reg_err = register_ioctl32_conversion(KERNINST_GET_TICKS_PER_USEC, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to register ioctl32 conversion\n"); 
   ioctl_reg_err = register_ioctl32_conversion(KERNINST_CLEAR_CLIENT_STATE, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to register ioctl32 conversion\n"); 
   ioctl_reg_err = register_ioctl32_conversion(KERNINST_PUT_INTERNAL_KERNEL_SYMBOLS, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to register ioctl32 conversion\n"); 
   ioctl_reg_err = register_ioctl32_conversion(KERNINST_WRITE1INSN, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to register ioctl32 conversion\n"); 
   ioctl_reg_err = register_ioctl32_conversion(KERNINST_FLUSH_ICACHE_RANGE, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to register ioctl32 conversion\n");
   ioctl_reg_err = register_ioctl32_conversion(KERNINST_GET_POWER_PERF_CTR_STATE, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to register ioctl32 conversion\n");
   ioctl_reg_err = register_ioctl32_conversion(KERNINST_SET_POWER_PERF_CTR_STATE, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to register ioctl32 conversion\n");
   ioctl_reg_err = register_ioctl32_conversion(KERNINST_CSWITCH_TEST, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to register ioctl32 conversion\n");
   ioctl_reg_err = register_ioctl32_conversion(KERNINST_READ_HWC_ALL_CPUS, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to register ioctl32 conversion\n");
}

void unregisterIoctl32Conversions()
{
   int ioctl_reg_err;
   /* Unregister ioctl conversions to 32-bit since the daemon is 32-bit app */
   ioctl_reg_err = unregister_ioctl32_conversion(KERNINST_GET_OFFSET_INFO, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to unregister ioctl32 conversion\n"); 
   ioctl_reg_err = unregister_ioctl32_conversion(KERNINST_ALLOC_BLOCK, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to unregister ioctl32 conversion\n"); 
   ioctl_reg_err = unregister_ioctl32_conversion(KERNINST_FREE_BLOCK, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to unregister ioctl32 conversion\n"); 
   ioctl_reg_err = unregister_ioctl32_conversion(KERNINST_SYMTAB_STUFF_SIZE, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to unregister ioctl32 conversion\n"); 
   ioctl_reg_err = unregister_ioctl32_conversion(KERNINST_SYMTAB_STUFF_GET, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to unregister ioctl32 conversion\n"); 
   ioctl_reg_err = unregister_ioctl32_conversion(KERNINST_WRUNDOABLE_REG, sys_ioctl);
   ioctl_reg_err = unregister_ioctl32_conversion(KERNINST_UNDO_WR_REG, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to unregister ioctl32 conversion\n"); 
   ioctl_reg_err = unregister_ioctl32_conversion(KERNINST_VIRTUAL_TO_PHYSICAL_ADDR, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to unregister ioctl32 conversion\n"); 
   ioctl_reg_err = unregister_ioctl32_conversion(KERNINST_KNOWN_CALLEES_SIZE, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to unregister ioctl32 conversion\n"); 
   ioctl_reg_err = unregister_ioctl32_conversion(KERNINST_KNOWN_CALLEES_GET, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to unregister ioctl32 conversion\n"); 
   ioctl_reg_err = unregister_ioctl32_conversion(KERNINST_GET_TICK_RAW, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to unregister ioctl32 conversion\n"); 
   ioctl_reg_err = unregister_ioctl32_conversion(KERNINST_CALLONCE, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to unregister ioctl32 conversion\n"); 
   ioctl_reg_err = unregister_ioctl32_conversion(KERNINST_RESET, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to unregister ioctl32 conversion\n"); 
   ioctl_reg_err = unregister_ioctl32_conversion(KERNINST_CLEAR_CLIENT_STATE, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to unregister ioctl32 conversion\n"); 
   ioctl_reg_err = unregister_ioctl32_conversion(KERNINST_PUT_INTERNAL_KERNEL_SYMBOLS, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to unregister ioctl32 conversion\n"); 
   ioctl_reg_err = unregister_ioctl32_conversion(KERNINST_GET_TICKS_PER_USEC, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to unregister ioctl32 conversion\n"); 
   ioctl_reg_err = unregister_ioctl32_conversion(KERNINST_WRITE1INSN, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to unregister ioctl32 conversion\n"); 
   ioctl_reg_err = unregister_ioctl32_conversion(KERNINST_FLUSH_ICACHE_RANGE, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to unregister ioctl32 conversion\n");
   ioctl_reg_err = unregister_ioctl32_conversion(KERNINST_GET_POWER_PERF_CTR_STATE, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to unregister ioctl32 conversion\n");
   ioctl_reg_err = unregister_ioctl32_conversion(KERNINST_SET_POWER_PERF_CTR_STATE, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to unregister ioctl32 conversion\n");
   ioctl_reg_err = unregister_ioctl32_conversion(KERNINST_CSWITCH_TEST, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to unregister ioctl32 conversion\n");
   ioctl_reg_err = unregister_ioctl32_conversion(KERNINST_READ_HWC_ALL_CPUS, sys_ioctl);
   if (ioctl_reg_err < 0)
      printk("unable to unregister ioctl32 conversion\n");
}
