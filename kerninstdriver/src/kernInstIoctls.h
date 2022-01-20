/* kernInstIoctls.h */

#ifndef _KERNINST_IOCTLS_H_
#define _KERNINST_IOCTLS_H_

#include "util/h/kdrvtypes.h" // kptr_t, uint32_t

/* I am not 100% certain of this, but it seems that when
   KERNINST_WRUNDOABLE_NUCLEUS was a much longer name, it
   did not work.  More specifically, copyin() failed in
   the ioctl.  I still can't believe this, and would welcome
   an alternative explanation to "#defines can't be more than
   X characters" --ari  4/30/99 */
#define KERNINST_ALLOC_BLOCK 100
#define KERNINST_FREE_BLOCK  101
#define KERNINST_SYMTAB_STUFF_SIZE 102
#define KERNINST_SYMTAB_STUFF_GET 103
#define KERNINST_VIRTUAL_TO_PHYSICAL_ADDR 106
#define KERNINST_GET_TICK_RAW 109
#define KERNINST_GET_PCR_RAW 110
#define KERNINST_SET_PCR_RAW 111
#define KERNINST_GET_PIC_RAW 112
#define KERNINST_SET_PIC_RAW 113
#define KERNINST_DISABLE_TICK_PROTECTION 114
#define KERNINST_CALLONCE 116
#define KERNINST_CALLTWICE 117
#define KERNINST_CALLTENTIMES 118
#define KERNINST_WRITE1ALIGNEDWORDTOPHYSMEM 119
#define KERNINST_WRUNDOABLE_NUCLEUS 120
#define KERNINST_WRUNDOABLE_REG 121
#define KERNINST_UNDO_WR_NUCLEUS 122
#define KERNINST_UNDO_WR_REG 123
#define KERNINST_GET_TEXTNUCLEUS_BOUNDS 124
#define KERNINST_CHG_UNDO_HOWSOON 125
#define KERNINST_GET_OFFSET_INFO 126
#define KERNINST_GET_ADDRESS_MAP 127
#define KERNINST_GET_DISP_LEVEL 128
#define KERNINST_GET_CLOCK_FREQ 129
#define KERNINST_GET_CPU_IMPL_NUM 130
#define KERNINST_GET_STICK_RAW 131
#define KERNINST_KNOWN_CALLEES_SIZE 132
#define KERNINST_KNOWN_CALLEES_GET 133
#define KERNINST_GET_ALLVTIMERS_ADDR 134
#define KERNINST_ADD_TO_ALLVTIMERS 135
#define KERNINST_REMOVE_FROM_ALLVTIMERS 136
#define KERNINST_CLEAR_CLIENT_STATE 137
#define KERNINST_ENABLE_CSWITCH_HANDLING 138
#define KERNINST_SET_DEBUG_OUTPUT 139

struct kernInstWrite1Word {
   kptr_t destAddrInKernel;
   uint32_t val;
};

struct kernInstWrite1UndoableWord {
   kptr_t destAddrInKernel;
   uint32_t newval;
   uint32_t howSoon;
      /* 0 --> undo first  1 --> undo after 0's */

   /* initially undefined, filled in by /dev/kerninst: */
   uint32_t origval;
};

struct kernInstChangeUndoHowSoon {
   kptr_t addrInKernel;
   uint32_t newHowSoon;
      /* 0 --> undo first  1 --> undo after 0's */
};

struct kernInstUndo1Write {
   kptr_t addr;
   uint32_t oldval_to_set_to; /* just for asserting? (We keep this anyway) */
};

#define ALLOC_TYPE_DATA 0
#define ALLOC_TYPE_TRY_NUCLEUS 1
#define ALLOC_TYPE_BELOW_NUCLEUS 2
#define ALLOC_TYPE_NUCLEUS 3
#define ALLOC_TYPE_KMEM32 4

struct kernInstAllocStruct {
   uint32_t nbytes;
   uint32_t requiredAlignment;
      /* e.g. 8192 if you want to be able to mmap the result */
      /* or e.g. 32 if you want the result to be icache-block aligned */
   uint32_t allocType; /* See ALLOC_ constants above */

   /* The remaining params are filled in for you: */
   kptr_t result;
   kptr_t result_mappable;
      /* filled in for you; differs from 'result' only if we allocated from the
         nucleus text and need to present a more mappable kernel vaddr */
};

struct kernInstFreeStruct {
   kptr_t kernelAddr;
   kptr_t mappedKernelAddr;
   uint32_t nbytes;

   /* the driver keeps around enough housekeeping info to translate these values
      into actual addresses & nbytes (could differ if padding was added behind the
      scenes to fulfill alignment requirements */
};

struct kernInstAddressMap {
    kptr_t startBelow; /* Region below the nucleus */
    kptr_t endBelow; /* not inclusive */
    kptr_t startNucleus;
    kptr_t endNucleus; /* 1 byte past end...e.g., start + nbytes */
};

struct kernInstSymtabGetStruct {
   kptr_t buffer; /* buffer is in user space...had better be big enough! */
   uint32_t buffer_size;

   uint32_t actual_amt_copied;
};

/*
  Structures that are not used in the daemon
*/
struct kernInstSegKMemStruct {
   void *addr;
   uint32_t len;
};

struct kernInstVirtualToPhysicalAddr {
   void *virtual_addr;
   void *physical_addr;
};

/* Structure that contains offsets of fields of interest within 
   several kernel structures (kthread, ...) */
struct kernInstOffsetInfo {
   uint32_t t_cpu_offset;
   uint32_t t_procp_offset;
   uint32_t p_pidp_offset;
   uint32_t pid_id_offset;
};

/* layout of what's returned by kernInstSymtabStuff:

unsigned num-modules

for each module:
   unsigned length of module name;
   the module name (no \0), padded to 4-byte bounds.

   unsigned length of module description string
   the module description (no \0), padded to 4-byte bounds.

   unsigned long address of text section's beginning
   unsigned long text section length
   unsigned long address of data section's beginning
   unsigned long data section length

   unsigned long stringTabNumBytes;
   the string table, padded to 4-byte bounds

   unsigned long num-symbols
   one Elf_Sym per symbol
end

*/

struct kernInstCallSiteInfo {
    /* Address of the call site */
    kptr_t addrFrom;

    /* Number of callees for this site. It is set by the
       KERNINST_KNOWN_CALLEES_SIZE ioctl and checked by the
       KERNINST_KNOWN_CALLEES_GET ioctl */
    uint32_t num_callees;

    /* Buffer in user space...had better be big enough! We fill it in the
       KERNINST_KNOWN_CALLEES_GET ioctl */
    kptr_t buffer; 
};

/* ------------------------------------------------------------------ */

#ifdef __cplusplus
extern "C" {
#endif
    void kerninst_alloc_block(void *loggedAllocRequests,
			      struct kernInstAllocStruct *);
    void kerninst_free_block(void *loggedAllocRequests,
			     struct kernInstFreeStruct *);
    void kerninst_call_once();
    void kerninst_call_twice();
    void kerninst_call_tentimes();
    void getKernInstAddressMap(struct kernInstAddressMap *bounds);
    uint32_t getSystemClockFreq();
#ifdef __cplusplus
}; // extern "C"
#endif



#endif
