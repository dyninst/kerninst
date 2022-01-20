#include "instrumenter.h"
#include "kapi.h"
#include "kerninstdriver-linux/src/hwcounter_p4.h"

//------------------ Performance Monitoring Events  ------------------

/* See "IA-32 Intel® Architecture Software Developer's Manual Volume 3: 
   System Programming Guide (Order #253668)" for more information */

const int counter_escrs[NUM_COUNTERS][8] = {
   {BPU_ESCR0, IS_ESCR0, MOB_ESCR0, ITLB_ESCR0, PMH_ESCR0, IX_ESCR0, FSB_ESCR0, BSU_ESCR0},
   {BPU_ESCR0, IS_ESCR0, MOB_ESCR0, ITLB_ESCR0, PMH_ESCR0, IX_ESCR0, FSB_ESCR0, BSU_ESCR0},
   {BPU_ESCR1, IS_ESCR1, MOB_ESCR1, ITLB_ESCR1, PMH_ESCR1, IX_ESCR1, FSB_ESCR1, BSU_ESCR1},
   {BPU_ESCR1, IS_ESCR1, MOB_ESCR1, ITLB_ESCR1, PMH_ESCR1, IX_ESCR1, FSB_ESCR1, BSU_ESCR1},
   {MS_ESCR0, TC_ESCR0, TBPU_ESCR0, NO_ESCR, NO_ESCR, NO_ESCR, NO_ESCR, NO_ESCR},
   {MS_ESCR0, TC_ESCR0, TBPU_ESCR0, NO_ESCR, NO_ESCR, NO_ESCR, NO_ESCR, NO_ESCR},
   {MS_ESCR1, TC_ESCR1, TBPU_ESCR1, NO_ESCR, NO_ESCR, NO_ESCR, NO_ESCR, NO_ESCR},
   {MS_ESCR1, TC_ESCR1, TBPU_ESCR1, NO_ESCR, NO_ESCR, NO_ESCR, NO_ESCR, NO_ESCR},
   {FLAME_ESCR0, FIRM_ESCR0, SAAT_ESCR0, U2L_ESCR0, NO_ESCR, DAC_ESCR0, NO_ESCR, NO_ESCR},
   {FLAME_ESCR0, FIRM_ESCR0, SAAT_ESCR0, U2L_ESCR0, NO_ESCR, DAC_ESCR0, NO_ESCR, NO_ESCR},
   {FLAME_ESCR1, FIRM_ESCR1, SAAT_ESCR1, U2L_ESCR1, NO_ESCR, DAC_ESCR1, NO_ESCR, NO_ESCR},
   {FLAME_ESCR1, FIRM_ESCR1, SAAT_ESCR1, U2L_ESCR1, NO_ESCR, DAC_ESCR1, NO_ESCR, NO_ESCR},
   {IQ_ESCR0, ALF_ESCR0, RAT_ESCR0, SSU_ESCR0, CRU_ESCR0, CRU_ESCR2, CRU_ESCR4, NO_ESCR},
   {IQ_ESCR0, ALF_ESCR0, RAT_ESCR0, SSU_ESCR0, CRU_ESCR0, CRU_ESCR2, CRU_ESCR4, NO_ESCR},
   {IQ_ESCR1, ALF_ESCR1, RAT_ESCR1, NO_ESCR, CRU_ESCR1, CRU_ESCR3, CRU_ESCR5, NO_ESCR},
   {IQ_ESCR1, ALF_ESCR1, RAT_ESCR1, NO_ESCR, CRU_ESCR1, CRU_ESCR3, CRU_ESCR5, NO_ESCR},
   {IQ_ESCR0, ALF_ESCR0, RAT_ESCR0, SSU_ESCR0, CRU_ESCR0, CRU_ESCR2, CRU_ESCR4, NO_ESCR},
   {IQ_ESCR1, ALF_ESCR1, RAT_ESCR1, NO_ESCR, CRU_ESCR1, CRU_ESCR3, CRU_ESCR5, NO_ESCR}
};

typedef struct ESCR_restriction {
   int escr;
   int counter_opts[3];
} ESCR_restriction_t;

typedef struct event {
   char *name;
   char *description;
   ESCR_restriction_t restr[2];
   unsigned char escr_event_select;
   unsigned char cccr_escr_select;
} event_t;

/* Non-Retirement Events: from table A-1 in manual */
const event_t non_retirement_event_map[] = {
/* ndx == 0 */
   {"TC_deliver_mode", 
    "This event counts the duration (in clock cycles) of the operating modes of the trace cache and decode engine in the processor package. The mode is specified by one or more of the event mask bits.",
    {{TC_ESCR0, {4, 5, NO_CTR}}, {TC_ESCR1, {6, 7, NO_CTR}}},
    0x1, 0x1
   },
   {"BPU_fetch_request", 
    "This event counts instruction fetch requests of specified request type by the Branch Prediction unit. Specify one or more mask bits to qualify the request type(s).",
    {{BPU_ESCR0, {0, 1, NO_CTR}}, {BPU_ESCR1, {2, 3, NO_CTR}}},
    0x3, 0x0
   },
   {"ITLB_reference", 
    "This event counts translations using the Instruction Translation Lookaside Buffer (ITLB).",
    {{ITLB_ESCR0, {0, 1, NO_CTR}}, {ITLB_ESCR1, {2, 3, NO_CTR}}},
    0x18, 0x3
   },
   {"memory_cancel", 
    "This event counts the canceling of various type of request in the Data cache Address Control unit (DAC). Specify one or more mask bits to select the type of requests that are canceled.",
    {{DAC_ESCR0, {8, 9, NO_CTR}}, {DAC_ESCR1, {10, 11, NO_CTR}}},
    0x2, 0x5
   },
/* ndx == 4 */
   {"memory_complete", 
    "This event counts the completion of a load split, store split, uncacheable (UC) split, or UC load. Specify one or more mask bits to select the operations to be counted.",
    {{SAAT_ESCR0, {8, 9, NO_CTR}}, {SAAT_ESCR1, {10, 11, NO_CTR}}},
    0x8, 0x2
   },
   {"load_port_replay", 
    "This event counts replayed events at the load port. Specify one or more mask bits to select the cause of the replay.",
    {{SAAT_ESCR0, {8, 9, NO_CTR}}, {SAAT_ESCR1, {10, 11, NO_CTR}}},
    0x4, 0x2
   },
   {"store_port_replay", 
    "This event counts replayed events at the store port. Specify one or more mask bits to select the cause of the replay.",
    {{SAAT_ESCR0, {8, 9, NO_CTR}}, {SAAT_ESCR1, {10, 11, NO_CTR}}},
    0x5, 0x2
   },
   {"MOB_load_replay", 
    "This event triggers if the memory order buffer (MOB) caused a load operation to be replayed. Specify one or more mask bits to select the cause of the replay.",
    {{MOB_ESCR0, {0, 1, NO_CTR}}, {MOB_ESCR1, {2, 3, NO_CTR}}},
    0x3, 0x2
   },
/* ndx == 8 */
   {"page_walk_type", 
    "This event counts various types of page walks that the page miss handler (PMH) performs.",
    {{PMH_ESCR0, {0, 1, NO_CTR}}, {PMH_ESCR1, {2, 3, NO_CTR}}},
    0x1, 0x4
   },
   {"BSQ_cache_reference", 
    "This event counts cache references (2nd level cache or 3rd level cache) as seen by the bus unit.",
    {{BSU_ESCR0, {0, 1, NO_CTR}}, {BSU_ESCR1, {2, 3, NO_CTR}}},
    0xc, 0x7
   },
   {"IOQ_allocation", 
    "This event counts the various types of transactions on the bus. A count is generated each time a transaction is allocated into the IOQ that matches the specified mask bits.",
    {{FSB_ESCR0, {0, 1, NO_CTR}}, {FSB_ESCR1, {2, 3, NO_CTR}}},
    0x3, 0x6
   },
   {"IOQ_active_entries", 
    "This event counts the number of entries (clipped at 15) in the IOQ that are active. This event must be programmed in conjunction with IOQ_allocation.",
    {{FSB_ESCR1, {2, 3, NO_CTR}}, {NO_ESCR, {NO_CTR, NO_CTR, NO_CTR}}},
    0x1a, 0x6
   },
/* ndx == 12 */
   {"FSB_data_activity", 
    "This event increments once for each DRDY or DBSY event that occurs on the front side bus. The event allows selection of a specific DRDY or DBSY event.",
    {{FSB_ESCR0, {0, 1, NO_CTR}}, {FSB_ESCR1, {2, 3, NO_CTR}}},
    0x17, 0x6
   },
   {"BSQ_allocation", 
    "This event counts allocations in the Bus Sequence Unit (BSQ) according to the specified mask bit encoding.",
    {{BSU_ESCR0, {0, 1, NO_CTR}}, {NO_ESCR, {NO_CTR, NO_CTR, NO_CTR}}},
    0x5, 0x7
   },
   {"BSQ_active_entries", 
    "This event represents the number of BSQ entries (clipped at 15) currently active (valid) which meet the subevent mask criteria during allocation in the BSQ. This event must be programmed in conjunction with BSQ_allocation.",
    {{BSU_ESCR1, {2, 3, NO_CTR}}, {NO_ESCR, {NO_CTR, NO_CTR, NO_CTR}}},
    0x6, 0x7
   },
   {"SSE_input_assist", 
    "This event counts the number of times an assist is requested to handle problems with input operands for SSE/SSE2/SSE3 operations; most notably denormal source operands when the DAZ bit is not set.",
    {{FIRM_ESCR0, {8, 9, NO_CTR}}, {FIRM_ESCR1, {10, 11, NO_CTR}}},
    0x34, 0x1
   },
/* ndx == 16 */
   {"packed_SP_uop", 
    "This event increments for each packed single-precision µop, specified through the event mask for detection.",
    {{FIRM_ESCR0, {8, 9, NO_CTR}}, {FIRM_ESCR1, {10, 11, NO_CTR}}},
    0x8, 0x1
   },
   {"packed_DP_uop", 
    "This event increments for each packed double-precision µop, specified through the event mask for detection.",
    {{FIRM_ESCR0, {8, 9, NO_CTR}}, {FIRM_ESCR1, {10, 11, NO_CTR}}},
    0xc, 0x1
   },
   {"scalar_SP_uop", 
    "This event increments for each scalar single-precision µop, specified through the event mask for detection.",
    {{FIRM_ESCR0, {8, 9, NO_CTR}}, {FIRM_ESCR1, {10, 11, NO_CTR}}},
    0xa, 0x1
   },
   {"scalar_DP_uop", 
    "This event increments for each scalar double-precision µop, specified through the event mask for detection.",
    {{FIRM_ESCR0, {8, 9, NO_CTR}}, {FIRM_ESCR1, {10, 11, NO_CTR}}},
    0xe, 0x1
   },
/* ndx == 20 */
   {"64bit_MMX_uop", 
    "This event increments for each MMX instruction, which operate on 64 bit SIMD operands.",
    {{FIRM_ESCR0, {8, 9, NO_CTR}}, {FIRM_ESCR1, {10, 11, NO_CTR}}},
    0x2, 0x1
   },
   {"128bit_MMX_uop", 
    "This event increments for each integer SIMD SSE2 instruction, which operate on 128 bit SIMD operands.",
    {{FIRM_ESCR0, {8, 9, NO_CTR}}, {FIRM_ESCR1, {10, 11, NO_CTR}}},
    0x1a, 0x1
   },
   {"x87_FP_uop", 
    "This event increments for each x87 floating-point µop, specified through the event mask for detection.",
    {{FIRM_ESCR0, {8, 9, NO_CTR}}, {FIRM_ESCR1, {10, 11, NO_CTR}}},
    0x4, 0x1
   },
   {"TC_misc", 
    "This event counts miscellaneous events detected by the TC. The counter will count twice for each occurrence.",
    {{TC_ESCR0, {4, 5, NO_CTR}}, {TC_ESCR1, {6, 7, NO_CTR}}},
    0x6, 0x1
   },
/* ndx == 24 */
   {"global_power_events", 
    "This event accumulates the time during which a processor is not stopped.",
    {{FSB_ESCR0, {0, 1, NO_CTR}}, {FSB_ESCR1, {2, 3, NO_CTR}}},
    0x13, 0x6
   },
   {"tc_ms_xfer", 
    "This event counts the number of times that uop delivery changed from TC to MS ROM.",
    {{MS_ESCR0, {4, 5, NO_CTR}}, {MS_ESCR1, {6, 7, NO_CTR}}},
    0x5, 0x0
   },
   {"uop_queue_writes", 
    "This event counts the number of valid uops written to the uop queue.",
    {{MS_ESCR0, {4, 5, NO_CTR}}, {MS_ESCR1, {6, 7, NO_CTR}}},
    0x9, 0x0
   },
   {"retired_mispred_branch_type", 
    "This event counts retiring mispredicted branches by type.",
    {{TBPU_ESCR0, {4, 5, NO_CTR}}, {TBPU_ESCR1, {6, 7, NO_CTR}}},
    0x5, 0x2
   },
/* ndx == 28 */
   {"retired_branch_type", 
    "This event counts retiring branches by type.",
    {{TBPU_ESCR0, {4, 5, NO_CTR}}, {TBPU_ESCR1, {6, 7, NO_CTR}}},
    0x4, 0x2
   },
   {"WC_Buffer", 
    "This event counts Write Combining Buffer operations that are selected by the event mask.",
    {{DAC_ESCR0, {8, 9, NO_CTR}}, {DAC_ESCR1, {10, 11, NO_CTR}}},
    0x5, 0x5
   },

   /* NOTE: The rest of the events may not be available on all P4/Xeon models */

   {"resource_stall", 
    "This event monitors the occurrence or latency of stalls in the Allocator.",
    {{ALF_ESCR0, {12, 13, 16}}, {ALF_ESCR1, {14, 15, 17}}},
    0x1, 0x1
   },
   {"b2b_cycles", 
    "This event can be configured to count the number back-to-back bus cycles using sub-event mask bits 1 through 6.",
    {{FSB_ESCR0, {0, 1, NO_CTR}}, {FSB_ESCR1, {2, 3, NO_CTR}}},
    0x16, 0x3
   },
/* ndx == 32 */
   {"bnr", 
    "This event can be configured to count bus not ready conditions using sub-event mask bits 0 through 2.",
    {{FSB_ESCR0, {0, 1, NO_CTR}}, {FSB_ESCR1, {2, 3, NO_CTR}}},
    0x8, 0x3
   },
   {"snoop", 
    "This event can be configured to count snoop hit modified bus traffic using sub-event mask bits 2, 6 and 7.",
    {{FSB_ESCR0, {0, 1, NO_CTR}}, {FSB_ESCR1, {2, 3, NO_CTR}}},
    0x6, 0x3
   },
   {"Response", 
    "This event can be configured to count different types of responses using sub-event mask bits 1,2, 8, and 9.",
    {{FSB_ESCR0, {0, 1, NO_CTR}}, {FSB_ESCR1, {2, 3, NO_CTR}}},
    0x4, 0x3
   }
};   

/* At-Retirement Events: from table A-2 in manual */
const event_t at_retirement_event_map[] = {
/* ndx == 0 */
   {"front_end_event", 
    "This event counts the retirement of tagged µops, which are specified through the front-end tagging mechanism. The event mask specifies bogus or non-bogus µops.",
    {{CRU_ESCR2, {12, 13, 16}}, {CRU_ESCR3, {14, 15, 17}}},
    0x8, 0x5
   },
   {"execution_event", 
    "This event counts the retirement of tagged µops, which are specified through the execution tagging mechanism. The event mask allows from one to four types of µops to be specified as either bogus or nonbogus µops to be tagged.",
    {{CRU_ESCR2, {12, 13, 16}}, {CRU_ESCR3, {14, 15, 17}}},
    0xc, 0x5
   },
   {"replay_event", 
    "This event counts the retirement of tagged µops, which are specified through the replay tagging mechanism. The event mask specifies bogus or non-bogus µops.",
    {{CRU_ESCR2, {12, 13, 16}}, {CRU_ESCR3, {14, 15, 17}}},
    0x9, 0x5
   },
   {"instr_retired", 
    "This event counts instructions that are retired during a clock cycle. Mask bits specify bogus or nonbogus (and whether they are tagged using the front-end tagging mechanism).",
    {{CRU_ESCR0, {12, 13, 16}}, {CRU_ESCR1, {14, 15, 17}}},
    0x2, 0x4
   },
/* ndx == 4 */
   {"uops_retired", 
    "This event counts µops that are retired during a clock cycle. Mask bits specify bogus or non-bogus.",
    {{CRU_ESCR0, {12, 13, 16}}, {CRU_ESCR1, {14, 15, 17}}},
    0x1, 0x4
   },
   {"uop_type", 
    "This event is used in conjunction with the front-end at-retirement mechanism to tag load and store µops.",
    {{RAT_ESCR0, {12, 13, 16}}, {RAT_ESCR1, {14, 15, 17}}},
    0x2, 0x2
   },
   {"branch_retired", 
    "This event counts the retirement of a branch. Specify one or more mask bits to select any combination of taken, not-taken, predicted and mispredicted.",
    {{CRU_ESCR2, {12, 13, 16}}, {CRU_ESCR3, {14, 15, 17}}},
    0x6, 0x5
   },
   {"mispred_branch_retired", 
    "This event represents the retirement of mispredicted IA-32 branch instructions.",
    {{CRU_ESCR0, {12, 13, 16}}, {CRU_ESCR1, {14, 15, 17}}},
    0x3, 0x4
   },
/* ndx == 8 */
   {"x87_assist", 
    "This event counts the retirement of x87 instructions that required special handling. Specifies one or more event mask bits to select the type of assistance.",
    {{CRU_ESCR2, {12, 13, 16}}, {CRU_ESCR3, {14, 15, 17}}},
    0x3, 0x5
   },
   {"machine_clear", 
    "This event increments according to the mask bit specified while the entire pipeline of the machine is cleared. Specify one of the mask bit to select the cause.",
    {{CRU_ESCR2, {12, 13, 16}}, {CRU_ESCR3, {14, 15, 17}}},
    0x2, 0x5
   },
   
   /* NOTE: the following is only available on P4 model 3 */

   {"instr_completed", 
    "This event counts instructions that have completed and retired during a clock cycle. Mask bits specify whether the instruction is bogus or non-bogus.",
    {{CRU_ESCR0, {12, 13, 16}}, {CRU_ESCR1, {14, 15, 17}}},
    0x7, 0x4
   }
};

//------------------------------------------------------------------------

typedef struct component_event {
   unsigned nr_ndx; // non-retirement map index, if applicable, -1 else
   unsigned ar_ndx; // at-retirement map index, if applicable, -1 else
   unsigned base_cccr_val;
   unsigned base_escr_val;
} component_event_t;

typedef struct hwcounter_setting {
   const char *ctr_kind;
   const component_event_t evts[2];
} hwcounter_setting_t;

const unsigned invalid = (unsigned)-1;

#define CCCR_BASE (CCCR_ENABLE | CCCR_ACTIVE_THREAD(0x3))

// NOTE: the order of definition in the counter_settings matrix *MUST* 
//       match the order as defined in kapi.h for kapi_hwcounter_kind

const hwcounter_setting_t counter_settings[HWC_MAX_EVENT] = {

   /* arch-generic counters */

   {"NONE", {{invalid,invalid,0,0},{invalid,invalid,0,0}}},
   {"Elapsed cycles", {{invalid,invalid,0,0},{invalid,invalid,0,0}}},

   /* non-retirement counters */

   {"I-TLB hits", 
    {{2, invalid, CCCR_BASE, (ESCR_EVENT_MASK(1<<0) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"I-TLB hits (uncacheable)", 
    {{2, invalid, CCCR_BASE, (ESCR_EVENT_MASK(1<<2) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"I-TLB misses", 
    {{2, invalid, CCCR_BASE, (ESCR_EVENT_MASK(1<<1) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"I-TLB miss page walks", 
    {{8, invalid, CCCR_BASE, (ESCR_EVENT_MASK(1<<1) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"D-TLB miss page walks",
    {{8, invalid, CCCR_BASE, (ESCR_EVENT_MASK(1<<0) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"L2-$ read hits (shared)", 
    {{9, invalid, CCCR_BASE, (ESCR_EVENT_MASK(1<<0) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"L2-$ read hits (exclusive)", 
    {{9, invalid, CCCR_BASE, (ESCR_EVENT_MASK(1<<1) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"L2-$ read hits (modified)", 
    {{9, invalid, CCCR_BASE, (ESCR_EVENT_MASK(1<<2) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"L2-$ read misses", 
    {{9, invalid, CCCR_BASE, (ESCR_EVENT_MASK(1<<8) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"L3-$ read hits (shared)", 
    {{9, invalid, CCCR_BASE, (ESCR_EVENT_MASK(1<<3) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"L3-$ read hits (exclusive)", 
    {{9, invalid, CCCR_BASE, (ESCR_EVENT_MASK(1<<4) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"L3-$ read hits (modified)", 
    {{9, invalid, CCCR_BASE, (ESCR_EVENT_MASK(1<<5) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"L3-$ read misses", 
    {{9, invalid, CCCR_BASE, (ESCR_EVENT_MASK(1<<9) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"conditional branches mispredicted", 
    {{27, invalid, CCCR_BASE, (ESCR_EVENT_MASK(1<<1) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"conditional branches predicted", 
    {{28, invalid, CCCR_BASE, (ESCR_EVENT_MASK(1<<1) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"calls mispredicted", 
    {{27, invalid, CCCR_BASE, (ESCR_EVENT_MASK(1<<2) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"calls predicted", 
    {{28, invalid, CCCR_BASE, (ESCR_EVENT_MASK(1<<2) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"returns mispredicted", 
    {{27, invalid, CCCR_BASE, (ESCR_EVENT_MASK(1<<3) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"returns predicted", 
    {{28, invalid, CCCR_BASE, (ESCR_EVENT_MASK(1<<3) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"indirects mispredicted", 
    {{27, invalid, CCCR_BASE, (ESCR_EVENT_MASK(1<<4) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"indirects predicted", 
    {{28, invalid, CCCR_BASE, (ESCR_EVENT_MASK(1<<4) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },

   /* at-retirement counters */
   
   {"memory loads", 
    {{invalid, 0, CCCR_BASE, (ESCR_EVENT_MASK(1<<0) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid, 5, CCCR_BASE, (ESCR_EVENT_MASK(1<<1) | ESCR_TAG_ENABLE | ESCR_T0_OS | ESCR_T1_OS)}}
   },
   {"memory stores", 
    {{invalid, 0, CCCR_BASE, (ESCR_EVENT_MASK(1<<0) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid, 5, CCCR_BASE, (ESCR_EVENT_MASK(1<<2) | ESCR_TAG_ENABLE | ESCR_T0_OS | ESCR_T1_OS)}}
   },
   {"L1-$ load misses", 
    {{invalid, 2, CCCR_BASE, (ESCR_EVENT_MASK(1<<0) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid, invalid, /*ia32_pebs_enable*/((1<<24)|(1<<0)), /*msr_pebs_matrix_vert*/(1<<0)}}
   },
   {"D-TLB load misses", 
    {{invalid, 2, CCCR_BASE, (ESCR_EVENT_MASK(1<<0) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid, invalid, /*ia32_pebs_enable*/((1<<24)|(1<<2)), /*msr_pebs_matrix_vert*/(1<<0)}}
   },
   {"D-TLB store misses", 
    {{invalid, 2, CCCR_BASE, (ESCR_EVENT_MASK(1<<0) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid, invalid, /*ia32_pebs_enable*/((1<<24)|(1<<2)), /*msr_pebs_matrix_vert*/(1<<1)}}
   },
   {"instructions issued", 
    {{invalid, 3, CCCR_BASE, (ESCR_EVENT_MASK((1<<0)|(1<<1)) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"instructions retired",
    {{invalid, 10, CCCR_BASE, (ESCR_EVENT_MASK(1<<0) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"micro-ops retired",
    {{invalid, 4, CCCR_BASE, (ESCR_EVENT_MASK(1<<0) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"taken branches predicted",
    {{invalid, 6, CCCR_BASE, (ESCR_EVENT_MASK(1<<2) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"taken branches mispredicted",
    {{invalid, 6, CCCR_BASE, (ESCR_EVENT_MASK(1<<3) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"not-taken branches predicted",
    {{invalid, 6, CCCR_BASE, (ESCR_EVENT_MASK(1<<0) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"not-taken branches mispredicted",
    {{invalid, 6, CCCR_BASE, (ESCR_EVENT_MASK(1<<1) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   },
   {"pipeline flushes",
    {{invalid, 9, (CCCR_BASE | CCCR_EDGE), (ESCR_EVENT_MASK(1<<0) | ESCR_T0_OS | ESCR_T1_OS)},
     {invalid,invalid,0,0}}
   }
};

//------------------------------------------------------------------------

// Create an empty counter set
kapi_hwcounter_set::kapi_hwcounter_set()
{
   for (unsigned i=0; i<numCounters; i++) {
      state.cccr_vals[i] = 0;
      state.escr_vals[i] = 0;
   }
   state.pebs_msrs[0] = 0; // ia32_pebs_enable
   state.pebs_msrs[1] = 0; // msr_pebs_matrix_vert
}

// Force insert the supplied event
static inline void insertEvent(const event_t *evt, unsigned *cccr_vals,
                               unsigned *escr_vals, 
                               const component_event_t *ce)
{
   int i, j, k;
   unsigned desired_escr_val = ce->base_escr_val | 
                               ESCR_EVENT_SELECT(evt->escr_event_select);
   unsigned desired_cccr_val = ce->base_cccr_val | 
                               CCCR_ESCR_SELECT(evt->cccr_escr_select);
   int unused_escr_ndx = 0;
   bool found_unused_escr = false;
   for(i=0; i<2; i++) {
      bool escr_used = false;
      unsigned escr_ndx = 0;
      for(j=0; j<NUM_COUNTERS; j++) {
	 if(counter_escrs[j][ESCR_OF_CCCR(cccr_vals[j])] == evt->restr[i].escr) {
            if(CCCR_IS_ENABLED(cccr_vals[j])) {
               escr_used = true;
               escr_ndx = j;
               break;
            }
	 }
      }
      if(escr_used) {
	 /* if ESCR in use, check for desired settings */
         if(escr_vals[escr_ndx] == desired_escr_val) {
            for(k=0; k<3; k++) {
               unsigned ctr = evt->restr[i].counter_opts[k];
               if(CCCR_IS_ENABLED(cccr_vals[ctr])) {
                  if(counter_escrs[ctr][ESCR_OF_CCCR(cccr_vals[ctr])] 
                     == evt->restr[i].escr) {
                     /* if ctr enabled and using this ESCR which has the
                        desired settings, then no need to do anything */
                     return;
                  }
               }
               else {
                  /* if ESCR has desired settings and an associated ctr is 
                     not enabled, just need to setup the ctr */
                  cccr_vals[ctr] = desired_cccr_val;
                  escr_vals[ctr] = desired_escr_val;
                  return;
               }
            }
         }
      }
      else {
         found_unused_escr = true;
         unused_escr_ndx = evt->restr[i].escr;
	 for(k=0; k<3; k++) {
	    unsigned ctr = evt->restr[i].counter_opts[k];
	    if(! CCCR_IS_ENABLED(cccr_vals[ctr])) {
               /* this ctr is not enabled, so setup this ESCR and the ctr */
	       cccr_vals[ctr] = desired_cccr_val;
               escr_vals[ctr] = desired_escr_val;
               return;
            }
	 }
      }
   }
   /* no way to insert without a conflict, time to steal */
   if(found_unused_escr) {
      /* need to steal an associated ctr - what heuristic should we use?
         for now, we'll just arbitrarity take the first ctr */
      unsigned ctr;
      if(unused_escr_ndx == evt->restr[0].escr)
         ctr = evt->restr[0].counter_opts[0]; 
      else
         ctr = evt->restr[1].counter_opts[0];
      cccr_vals[ctr] = desired_cccr_val;
      escr_vals[ctr] = desired_escr_val;
   }
   else {
      /* need to steal both - what heuristic should we use? for now,
         we'll just arbitrarily take the first ESCR and its first ctr */
      unsigned ctr = evt->restr[0].counter_opts[0];
      cccr_vals[ctr] = desired_cccr_val;
      escr_vals[ctr] = desired_escr_val;
   }
}
    
// Insert a counter in the set. If its slot is already occupied,
// force our counter in this slot. Return the old value of the slot
kapi_hwcounter_kind kapi_hwcounter_set::insert(kapi_hwcounter_kind kind)
{
   kapi_hwcounter_kind ret = HWC_NONE;

   const hwcounter_setting_t *ctr_setting = &(counter_settings[kind]);
   const component_event_t *comp_evt1 = &(ctr_setting->evts[0]);
   const component_event_t *comp_evt2 = &(ctr_setting->evts[1]);
   const event_t *evt;

   if(comp_evt1->nr_ndx != (unsigned)-1) {
      /* non retirement events */
      evt = &(non_retirement_event_map[comp_evt1->nr_ndx]);
      insertEvent(evt, &state.cccr_vals[0], 
                  &state.escr_vals[0], comp_evt1);
   }
   else if(comp_evt1->ar_ndx != (unsigned)-1) {
      /* at retirement events */
      evt = &(at_retirement_event_map[comp_evt1->ar_ndx]);
      insertEvent(evt, &state.cccr_vals[0], 
                  &state.escr_vals[0], comp_evt1);
      
      if(comp_evt1->ar_ndx == 2) {
	 /* special case for replay events - treat second event
            values as setting for ia32_pebs_enable and 
	    msr_pebs_matrix_vert registers */
         state.pebs_msrs[0] = comp_evt2->base_cccr_val;
         state.pebs_msrs[1] = comp_evt2->base_escr_val;
      } 
      else if(comp_evt2->ar_ndx != (unsigned)-1) {
	 /* need to insert second component event */
	 evt = &(at_retirement_event_map[comp_evt2->ar_ndx]);
	 insertEvent(evt, &state.cccr_vals[0], 
                     &state.escr_vals[0], comp_evt2);
      } 
   }
   return ret;
}

// Checks for conflicts between desired event and current MSR settings.
// Returns true only if a conflict is found.
static inline bool doCheck(const event_t *evt, const unsigned *cccr_vals,
			   const unsigned *escr_vals, 
                           const component_event_t *ce)
{
   int i, j, k;
   for(i=0; i<2; i++) {
      bool escr_used = false;
      unsigned escr_ndx = 0;
      for(j=0; j<NUM_COUNTERS; j++) {
	 if(counter_escrs[j][ESCR_OF_CCCR(cccr_vals[j])] == evt->restr[i].escr) {
	    if(CCCR_IS_ENABLED(cccr_vals[j])) {
               escr_used = true;
               escr_ndx = j;
               break;
            }
	 }
      }
      if(escr_used) {
	 /* if ESCR in use, check for desired settings */
	unsigned curr_escr_val = escr_vals[escr_ndx];
	unsigned desired_val = ESCR_EVENT_SELECT(evt->escr_event_select)
	                       | ce->base_escr_val;
	if(curr_escr_val == desired_val) {
	   for(k=0; k<3; k++) {
	      unsigned ctr = evt->restr[i].counter_opts[k];
	      if((ctr != NO_CTR) && CCCR_IS_ENABLED(cccr_vals[ctr])) {
		 /* if ctr enabled and using this ESCR, 
		    then no conflict */
		 if(counter_escrs[ctr][ESCR_OF_CCCR(cccr_vals[ctr])] 
                    == evt->restr[i].escr)
		    return false;
	      }
	   }
	}
      }
   }
   return true;
}

// Check to see that the supplied kind does not conflict with the 
// current settings (i.e., it's still enabled)
bool kapi_hwcounter_set::conflictsWith(kapi_hwcounter_kind kind) const
{
   const hwcounter_setting_t *ctr_setting = &(counter_settings[kind]);
   const component_event_t *comp_evt1 = &(ctr_setting->evts[0]);
   const component_event_t *comp_evt2 = &(ctr_setting->evts[1]);
   const event_t *evt;

   if(comp_evt1->nr_ndx != (unsigned)-1) {
      /* non retirement events */
      evt = &(non_retirement_event_map[comp_evt1->nr_ndx]);
      return doCheck(evt, &state.cccr_vals[0], 
                     &state.escr_vals[0], comp_evt1);
   }
   else if(comp_evt1->ar_ndx != (unsigned)-1) {
      /* at retirement events */
      evt = &(at_retirement_event_map[comp_evt1->ar_ndx]);
      if(doCheck(evt, &state.cccr_vals[0], 
                 &state.escr_vals[0], comp_evt1))
	 return true;
      if(comp_evt1->ar_ndx == 2) {
	 /* special case for replay events - treat second event
            values as setting for ia32_pebs_enable and 
	    msr_pebs_matrix_vert registers */
         if((comp_evt2->base_cccr_val != state.pebs_msrs[0]) ||
            (comp_evt2->base_escr_val != state.pebs_msrs[1]))
            return true;
      } 
      else if(comp_evt2->ar_ndx != (unsigned)-1) {
	 /* need to check second component event */
	 evt = &(at_retirement_event_map[comp_evt2->ar_ndx]);
	 return doCheck(evt, &state.cccr_vals[0], 
                        &state.escr_vals[0], comp_evt2);
      } 
   }
   return false;
}

// Check to see if two sets are equal
bool kapi_hwcounter_set::equals(const kapi_hwcounter_set &hwset) const
{
   for (unsigned i=0; i<numCounters; i++) {
      if((state.cccr_vals[i] != hwset.state.cccr_vals[i]) ||
         (state.escr_vals[i] != hwset.state.escr_vals[i]))
         return false;
   }
   if((state.pebs_msrs[0] != hwset.state.pebs_msrs[0]) ||
      (state.pebs_msrs[1] != hwset.state.pebs_msrs[1]))
         return false;
   return true;
}

// Returns the index of a counter that is currently enabled to count
// the specified event, or -1 if not found
static inline unsigned findCtr(const event_t *evt, const unsigned *cccr_vals,
			       const unsigned *escr_vals, 
			       const component_event_t *ce)
{
   int i, j, k;
   for(i=0; i<2; i++) {
      bool escr_used = false;
      unsigned escr_ndx = 0;
      for(j=0; j<NUM_COUNTERS; j++) {
	 if(counter_escrs[j][ESCR_OF_CCCR(cccr_vals[j])] == evt->restr[i].escr) {
	    if(CCCR_IS_ENABLED(cccr_vals[j])) {
               escr_used = true;
               escr_ndx = j;
               break;
            }
	 }
      }
      if(escr_used) {
	 /* if ESCR in use, check for desired settings */
	 unsigned curr_escr_val = escr_vals[escr_ndx];
	 unsigned desired_val = ESCR_EVENT_SELECT(evt->escr_event_select)
	                        | ce->base_escr_val;
	 if(curr_escr_val == desired_val) {
	    for(k=0; k<3; k++) {
	       unsigned ctr = evt->restr[i].counter_opts[k];
	       if((ctr != NO_CTR) && CCCR_IS_ENABLED(cccr_vals[ctr])) {
		  /* if ctr enabled and using this ESCR, 
		     then this is the one we're looking for */
		  if(counter_escrs[ctr][ESCR_OF_CCCR(cccr_vals[ctr])] 
                     == evt->restr[i].escr)
		     return ctr;
	       }
	    }
	 }
      }
   }
   return (unsigned)-1;
}

// Find an enabled counter for this kind of counter, assert if not found
unsigned kapi_hwcounter_set::findCounter(kapi_hwcounter_kind kind, 
                                         bool must_find) const
{
   const hwcounter_setting_t *ctr_setting = &(counter_settings[kind]);
   const component_event_t *comp_evt1 = &(ctr_setting->evts[0]);
   const component_event_t *comp_evt2 = &(ctr_setting->evts[1]);
   const event_t *evt;

   unsigned the_ctr = 0;
   if(comp_evt1->nr_ndx != (unsigned)-1) {
      /* non retirement events */
      evt = &(non_retirement_event_map[comp_evt1->nr_ndx]);
      the_ctr = findCtr(evt, &state.cccr_vals[0], 
                        &state.escr_vals[0], comp_evt1);
   }
   else if(comp_evt1->ar_ndx != (unsigned)-1) {
      /* at retirement events */
      evt = &(at_retirement_event_map[comp_evt1->ar_ndx]);
      the_ctr = findCtr(evt, &state.cccr_vals[0], 
                        &state.escr_vals[0], comp_evt1);
      if(comp_evt1->ar_ndx == 2) {
	 /* special case for replay events - treat second event
            values as setting for ia32_pebs_enable and 
	    msr_pebs_matrix_vert registers */
         if((comp_evt2->base_cccr_val != state.pebs_msrs[0]) ||
            (comp_evt2->base_escr_val != state.pebs_msrs[1]))
            the_ctr = (unsigned)-1;
      }
      else if(comp_evt2->ar_ndx != (unsigned)-1) {
	 /* need to check second component event */
	 evt = &(at_retirement_event_map[comp_evt2->ar_ndx]);
	 if((unsigned)-1 == findCtr(evt, &state.cccr_vals[0], 
                                    &state.escr_vals[0], comp_evt2))
            the_ctr = (unsigned)-1;
      }
   }
   if(must_find && (the_ctr == (unsigned)-1)) {
      cerr << "kapi_hwcounter_set::findCounter(" << kind 
	   << ") - no counter of this kind enabled\n";
      assert(false);
   }
   return the_ctr;
}

// Free a slot occupied by the counter
void kapi_hwcounter_set::free(kapi_hwcounter_kind kind)
{
   unsigned ctr_num = findCounter(kind, false);
      // false - ok if not found, may have already been freed
   if(ctr_num != (unsigned)-1) {
      state.cccr_vals[ctr_num] = 0;
      state.escr_vals[ctr_num] = 0;
   }

   const hwcounter_setting_t *ctr_setting = &(counter_settings[kind]);
   const component_event_t *comp_evt1 = &(ctr_setting->evts[0]);
   const component_event_t *comp_evt2 = &(ctr_setting->evts[1]);
   const event_t *evt;

   if(comp_evt1->ar_ndx == 2) {
      /* special case for replay events - treat second event
         values as setting for ia32_pebs_enable and 
         msr_pebs_matrix_vert registers */
      if(comp_evt2->base_cccr_val == state.pebs_msrs[0])
         state.pebs_msrs[0] = 0;
      if(comp_evt2->base_escr_val == state.pebs_msrs[1])
         state.pebs_msrs[1] = 0;
   }
   else if(comp_evt2->ar_ndx != (unsigned)-1) {
      /* need to free second counter */
      evt = &(at_retirement_event_map[comp_evt2->ar_ndx]);
      ctr_num = findCtr(evt, &state.cccr_vals[0], 
                        &state.escr_vals[0], comp_evt2);
      if(ctr_num != (unsigned)-1) {
         state.cccr_vals[ctr_num] = 0;
         state.escr_vals[ctr_num] = 0;
      }
   }
}

extern instrumenter *theGlobalInstrumenter;

// Find what counters are enabled in the hardware
ierr kapi_hwcounter_set::readConfig()
{
   assert(theGlobalInstrumenter != 0);

   pdvector< std::pair<unsigned,unsigned> > curr_state;
   curr_state = theGlobalInstrumenter->getP4PerfCtrState();
   if(curr_state.size() != numCounters+1) {
      cerr << "kapi_hwcounter_set::readConfig() - curr_state vector has "
           << curr_state.size() << " entries, expecting " << numCounters+1
           << endl;
      assert(false);
   }
   for (unsigned i=0; i<numCounters; i++) {
      state.cccr_vals[i] = curr_state[i].first;
      state.escr_vals[i] = curr_state[i].second;
   }
   state.pebs_msrs[0] = curr_state[numCounters].first;
   state.pebs_msrs[1] = curr_state[numCounters].second;
   return 0;
}

// Do the actual programming of the counters
ierr kapi_hwcounter_set::writeConfig()
{
    assert(theGlobalInstrumenter != 0);

    pdvector< std::pair<unsigned,unsigned> > curr_state, new_state;
    curr_state = theGlobalInstrumenter->getP4PerfCtrState();
    if(curr_state.size() != (numCounters+1)) {
       cerr << "kapi_hwcounter_set::writeConfig() - curr_state vector has "
            << curr_state.size() << " entries, expecting " << numCounters+1
           << endl;
       assert(false);
    }

    bool modified = false;

    for (unsigned i=0; i<numCounters; i++) {
       if((curr_state[i].first != state.cccr_vals[i]) ||
          (curr_state[i].second != state.escr_vals[i])) {
          new_state.push_back(
                   std::make_pair<unsigned,unsigned>(state.cccr_vals[i],
                                                     state.escr_vals[i]));
          modified = true;
       } else {
          new_state.push_back(
                std::make_pair<unsigned,unsigned>(invalid,invalid));
       }
    }
    if((curr_state[numCounters].first != state.pebs_msrs[0]) ||
       (curr_state[numCounters].second != state.pebs_msrs[1])) {
       new_state.push_back(
                  std::make_pair<unsigned,unsigned>(state.pebs_msrs[0],
                                                    state.pebs_msrs[1]));
       modified = true;
    } else {
       new_state.push_back(std::make_pair<unsigned,unsigned>(invalid,invalid));
    }
 
    if (modified) {
       theGlobalInstrumenter->setP4PerfCtrState(new_state);
    }
    return 0;
}
