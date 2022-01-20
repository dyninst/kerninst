/*
 * kernInstIoctls.h - declarations for ioctl's implemented by kerninst driver
 */

#ifndef _KERNINST_LINUX_IOCTL_
#define _KERNINST_LINUX_IOCTL_

#include <linux/ioctl.h>
#include <linux/types.h>

#include "kernInstBits.h"

#ifdef i386_unknown_linux2_4
#include "hwcounter_p4.h"
typedef unsigned int kptr_t;
typedef unsigned int dptr_t;

#elif defined(ppc64_unknown_linux2_4)
typedef uint64_t kptr_t;
typedef uint32_t dptr_t;

struct kernInstFlushRange {
   kptr_t addr;
   unsigned numBytes;
};
struct kernInstPowerPerfCtrState {
    uint64_t mmcr0;
    uint64_t mmcr1;
    uint64_t mmcra;
};

/* performance counter operations */
extern void set_power_perfctr_state(struct kernInstPowerPerfCtrState *state);
extern void get_power_perfctr_state(struct kernInstPowerPerfCtrState *curr_state);
uint64_t  read_pmc (unsigned int n);
extern void read_hwc_all_cpus(void *);
#endif

typedef struct kerninst_symbol {
   kptr_t sym_addr;
   kptr_t  sym_name_offset;
} kerninst_symbol;

struct kernInstTrapMapping {
   kptr_t trapAddr;
   kptr_t patchAddr;
};

struct kernInstWrite1Insn {
   kptr_t destAddrInKernel;
#if defined(ppc64_unknown_linux2_4) && !defined(__KERNEL__)
//need to make sure kernel and userspace see the same size
   uint32_t pad1;
#endif   
   const char *bytes;
   unsigned length;
};

struct kernInstWrite1UndoableInsn {
   kptr_t destAddrInKernel;
#if defined(ppc64_unknown_linux2_4) && !defined(__KERNEL__)
//need to make sure kernel and userspace see the same size
   uint32_t pad;
#endif   
   const char *newval;
   unsigned length;
   unsigned howSoon;
      /* 0 --> undo first  1 --> undo after 0's */

#if defined(ppc64_unknown_linux2_4) && !defined(__KERNEL__)
//need to make sure kernel and userspace see the same size
   uint32_t pad2;
#endif   
   /* initially undefined, filled in by /dev/kerninst: */
   char *origval;
};

struct kernInstChangeUndoHowSoon {
   kptr_t addrInKernel;
   unsigned newHowSoon;
      /* 0 --> undo first  1 --> undo after 0's */
};

struct kernInstUndo1Write {
   kptr_t addr;
#if defined(ppc64_unknown_linux2_4) && !defined(__KERNEL__)
//need to make sure kernel and userspace see the same size
   uint32_t pad;
#endif   
   const char *oldval_to_set_to; /* just for asserting */
   unsigned length;
};

#define ALLOC_TYPE_DATA 0
#define ALLOC_TYPE_TRY_NUCLEUS 1
#define ALLOC_TYPE_BELOW_NUCLEUS 2
#define ALLOC_TYPE_NUCLEUS 3
#define ALLOC_TYPE_KMEM32 4

struct kernInstAllocStruct {
   unsigned nbytes;
   unsigned requiredAlignment;
      /* e.g. 8192 if you want to be able to mmap the result */
      /* or e.g. 32 if you want the result to be icache-block aligned */
   unsigned allocType; /* See ALLOC_ constants above */

   /* The remaining params are filled in for you: */
   kptr_t result;
   kptr_t result_mappable;
      /* filled in for you; differs from 'result' only if we allocated from the
         nucleus text and need to present a more mappable kernel vaddr */
};

struct kernInstFreeStruct {
   kptr_t kernelAddr;
   kptr_t mappedKernelAddr;
   unsigned nbytes;

   /* the driver keeps around enough housekeeping info to translate these values
      into actual addresses & nbytes (could differ if padding was added behind the
      scenes to fulfill alignment requirements */
};

struct kernInstSymtabGetStruct {
   kptr_t buffer; /* buffer is in user space...had better be big enough! */
   unsigned buffer_size;

   unsigned actual_amt_copied;
};

#ifdef ppc64_unknown_linux2_4
#define PACA_ACCESS_R13   0
#define PACA_ACCESS_SPRG3 1
#endif


struct kernInstOffsetInfo {
#ifdef ppc64_unknown_linux2_4
   unsigned t_pacacurrent_offset; 
   unsigned paca_access;
#endif
   unsigned thr_task_offset; /* offset of task field in thread_info struct */
   unsigned thr_cpu_offset;  /* offset of cpu field in thread_info struct */
   unsigned thr_size;        /* number to be "and"ed to %esp to get current */
   unsigned task_pid_offset; /* offset of pid field in task_struct */
};

struct kernInstCallSiteInfo {
   /* Address of the call site */
   kptr_t addrFrom;

   /* Number of callees for this site. It is set by the
      KERNINST_KNOWN_CALLEES_SIZE ioctl and checked by the
      KERNINST_KNOWN_CALLEES_GET ioctl */
   unsigned num_callees;

   /* Buffer in user space...had better be big enough! We fill it in the
      KERNINST_KNOWN_CALLEES_GET ioctl */
   kptr_t buffer; 
};

struct kernInstCSwitchPoints {
   unsigned type; /* 0 == cswitch_out, 1 == cswitch_in */
   kptr_t *addrs;
   unsigned addrs_len;
};

struct kernInstReadHwcAllCPUs {
   unsigned type;
   unsigned num_cpus;
#if defined(ppc64_unknown_linux2_4) && !defined(__KERNEL__)
//need to make sure kernel and userspace see the same size
   uint32_t pad1;
#endif   
   uint64_t *cpu_vals;
};

struct readHwcArgs {
   unsigned type;
   uint64_t *cpu_vals;
};

#ifdef ppc64_unknown_linux2_4

//see also initRequestedInternalKernelStructures() in kerninstd/driver.C

#define KERNINST_GET_LOCK_SLOT_INDEX 0
#define KERNINST_BOLTED_DIR_INDEX 1
#define KERNINST_HASH_TABLE_LOCK_INDEX 2
#define KERNINST_PMD_ALLOC_INDEX 3
#define KERNINST_PTE_ALLOC_INDEX 4
#define KERNINST_PPC_MD_INDEX 5
#define KERNINST_BTMALLOC_MM_INDEX 6
#define KERNINST_LOCAL_FREE_BOLTED_PAGES_INDEX 7
#define KERNINST_SMP_CALL_FUNCTION_INDEX 8
#define KERNINST_STE_ALLOCATE_INDEX 9
#define KERNINST_HTAB_DATA_INDEX 10
#define KERNINST_HPTE_REMOVE_INDEX 11
#define KERNINST_FIND_LINUX_PTE_INDEX 12
#define KERNINST_PSERIES_TLBIE_LOCK_INDEX 13
#define KERNINST_PLPAR_PTE_REMOVE_INDEX 14
#define KERNINST_MALLOC_INDEX 15
#define KERNINST_FREE_INDEX 16 
#define KERNINST_INIT_REQUESTED_POINTERS_TO_KERNEL_INDEX 17 
#define KERNINST_PLPAR_HCALL_NORETS_INDEX 18 
#define KERNINST_PTE_ALLOC_KERNEL_INDEX 19 
#define KERNINST_PACA_INDEX 20
#define KERNINST_CUR_CPU_SPEC_INDEX 21
#define KERNINST_NUM_REQUESTED_INTERNAL_SYMBOLS 22 

#endif //ppc64_unknown_linux2_4

#define KERNINST_MAGIC 0xCC /* opcode for int3 */

/* these are the ioctls */
#define KERNINST_GET_OFFSET_INFO _IOR(KERNINST_MAGIC, 1, struct kernInstOffsetInfo)

#define KERNINST_ALLOC_BLOCK _IOWR(KERNINST_MAGIC, 2, struct kernInstAllocStruct)
#define KERNINST_FREE_BLOCK _IOW(KERNINST_MAGIC, 3, struct kernInstFreeStruct)

#define KERNINST_SYMTAB_STUFF_SIZE _IOR(KERNINST_MAGIC, 4, unsigned)
#define KERNINST_SYMTAB_STUFF_GET _IOWR(KERNINST_MAGIC, 5, struct kernInstSymtabGetStruct)

#define KERNINST_WRITE1INSN _IOW(KERNINST_MAGIC, 6, struct kernInstWrite1Insn)
#define KERNINST_WRUNDOABLE_REG _IOWR(KERNINST_MAGIC, 7, struct kernInstWrite1UndoableInsn)
#define KERNINST_UNDO_WR_REG _IOW(KERNINST_MAGIC, 8, struct kernInstUndo1Write)
#define KERNINST_CHG_UNDO_HOWSOON _IOW(KERNINST_MAGIC, 9, struct kernInstChangeUndoHowSoon)

#define KERNINST_VIRTUAL_TO_PHYSICAL_ADDR _IOWR(KERNINST_MAGIC, 10, kptr_t)

#define KERNINST_KNOWN_CALLEES_SIZE _IOWR(KERNINST_MAGIC, 11, struct kernInstCallSiteInfo) 
#define KERNINST_KNOWN_CALLEES_GET _IOW(KERNINST_MAGIC, 12, struct kernInstCallSiteInfo)

#define KERNINST_GET_TICK_RAW _IOR(KERNINST_MAGIC, 13, unsigned)

#define KERNINST_CALLONCE _IO(KERNINST_MAGIC, 14)

#define KERNINST_GET_ALLVTIMERS_ADDR _IOR(KERNINST_MAGIC, 15, kptr_t)
#define KERNINST_ADD_TO_ALLVTIMERS _IOW(KERNINST_MAGIC, 16, kptr_t)
#define KERNINST_REMOVE_FROM_ALLVTIMERS _IOW(KERNINST_MAGIC, 17, kptr_t)
#define KERNINST_CLEAR_CLIENT_STATE _IO(KERNINST_MAGIC, 18)

#define KERNINST_READ_HWC_ALL_CPUS _IOWR(KERNINST_MAGIC, 19, struct kernInstReadHwcAllCPUs)

#ifdef i386_unknown_linux2_4

#define KERNINST_REGISTER_TRAP_PATCH_MAPPING _IOW(KERNINST_MAGIC, 100, struct kernInstTrapMapping)
#define KERNINST_UPDATE_TRAP_PATCH_MAPPING _IOW(KERNINST_MAGIC, 101, struct kernInstTrapMapping)
#define KERNINST_UNREGISTER_TRAP_PATCH_MAPPING _IOW(KERNINST_MAGIC, 102, kptr_t)

#define KERNINST_GET_BUG_UD2_SIZE _IOWR(KERNINST_MAGIC, 103, unsigned)

#define KERNINST_GET_PERF_CTR_STATE _IOR(KERNINST_MAGIC, 104, perfctr_state_t)
#define KERNINST_SET_PERF_CTR_STATE _IOW(KERNINST_MAGIC, 105, perfctr_state_t)
#define KERNINST_GET_PERF_CTR_VAL _IOWR(KERNINST_MAGIC, 106, perfctr_val_t)
#define KERNINST_SET_PERF_CTR_VAL _IOWR(KERNINST_MAGIC, 107, perfctr_val_t)

#elif defined(ppc64_unknown_linux2_4)

#define KERNINST_GET_TICKS_PER_USEC _IOR(KERNINST_MAGIC, 100, kptr_t)
#define KERNINST_PUT_INTERNAL_KERNEL_SYMBOLS _IOW(KERNINST_MAGIC, 101, kptr_t)
#define KERNINST_FLUSH_ICACHE_RANGE _IOW(KERNINST_MAGIC, 102, struct kernInstFlushRange)
#define KERNINST_GET_POWER_PERF_CTR_STATE _IOR(KERNINST_MAGIC, 103, struct kernInstPowerPerfCtrState)
#define KERNINST_SET_POWER_PERF_CTR_STATE _IOW(KERNINST_MAGIC, 104, struct kernInstPowerPerfCtrState)
#define KERNINST_CSWITCH_TEST _IO(KERNINST_MAGIC, 105)
#endif // platform-specific ioctl defines

#define KERNINST_TOGGLE_ENABLE_BITS _IOW(KERNINST_MAGIC, 200, uint32_t)
/*#define KERNINST_SET_ALL_DEBUG_BITS _IOW(KERNINST_MAGIC, 210, uint32_t)*/
#define KERNINST_TOGGLE_DEBUG_BITS _IOW(KERNINST_MAGIC, 211, uint32_t)

#define KERNINST_RESET _IO(KERNINST_MAGIC, 255)

/* NOTE: last valid ioctl number is 255, don't define anything past it */

int kerninst_alloc_block(void *loggedAllocRequests,
                         struct kernInstAllocStruct *);
int kerninst_free_block(void *loggedAllocRequests,
                        struct kernInstFreeStruct *);

#endif

