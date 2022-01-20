// hwcounter_p4.h - definitions to represent Pentium4/Xeon hardware counters

#ifndef _HWCOUNTER_P4_H_
#define _HWCOUNTER_P4_H_

/* See "IA-32 Intel® Architecture Software Developer's Manual Volume 3: 
   System Programming Guide (Order #253668)" for more information */

//--------- Counters & Counter Configuration Control Register (CCCRs) ---------

#define COUNTER_ADDR(e) (0x300 + (e))
#define NUM_COUNTERS 18
#define NO_CTR -1

typedef enum {
   BPU_COUNTER0 = 0,
   BPU_COUNTER1,
   BPU_COUNTER2,
   BPU_COUNTER3,
   MS_COUNTER0,
   MS_COUNTER1,
   MS_COUNTER2,
   MS_COUNTER3,
   FLAME_COUNTER0,
   FLAME_COUNTER1,
   FLAME_COUNTER2,
   FLAME_COUNTER3,
   IQ_COUNTER0,
   IQ_COUNTER1,
   IQ_COUNTER2,
   IQ_COUNTER3,
   IQ_COUNTER4,
   IQ_COUNTER5
} COUNTER_id;

#define CCCR_ADDR(e) (0x360 + (e))

#define CCCR_ENABLE (1 << 12) /* enable corresponding counter */
#define CCCR_ESCR_SELECT(e) (((e) & 0x7) << 13) /* selects the ESCR to use */
#define CCCR_ACTIVE_THREAD(t) (((t) & 0x3) << 16) /* mask of which active logical
                                                   processors to count for */
#define CCCR_COMPARE (1 << 18) /* filter event count when set */
#define CCCR_COMPLEMENT (1 << 19) /* how to compare evt cnt to threshold val */
#define CCCR_THRESHOLD(t) (((t) & 0xf) << 20) /* threshold val for comparison */
#define CCCR_EDGE (1 << 24) /* enables rising edge detection of threshold 
                               comparison output when set */
#define CCCR_FORCE_OVF (1 << 25) /* forces counter overflow every increment */
#define CCCR_OVF_PMI_T0 (1 << 26) /* causes a perf monitor interrupt (PMI) 
                                     to logical processor 0 on overflow */
#define CCCR_OVF_PMI_T1 (1 << 27) /* causes a perf monitor interrupt (PMI) 
                                     to logical processor 1 on overflow */
#define CCCR_CASCADE (1 << 30) /* enable cascading of counters for ctr pair */
#define CCCR_OVF (1 << 31) /* sticky flag that indicates overflow occurred */

#define CCCR_IS_ENABLED(c) ((c) & CCCR_ENABLE)
#define ESCR_OF_CCCR(c) (((c) >> 13) & 0x7)

//------------------ Event Selection Control Registers (ESCRs) ------------------

#define ESCR_ADDR(e) (0x3a0 + (e))

#define ESCR_T1_USR (1 << 0) /* count logical processor 1 events at CPL>0 */
#define ESCR_T1_OS (1 << 1) /* count logical processor 1 events at CPL==0 */
#define ESCR_T0_USR (1 << 2) /* count logical processor 0 events at CPL>0 */
#define ESCR_T0_OS (1 << 3) /* count logical processor 0 events at CPL==0 */
#define ESCR_TAG_ENABLE (1 << 4) /* enable tagging of u-ops */
#define ESCR_TAG_VALUE(v) (((v) & 0xf) << 5) /* value to associate with tagged u-op */
#define ESCR_EVENT_MASK(m) (((m) & 0x7fff) << 9) /* mask to select events w/in class */
#define ESCR_EVENT_SELECT(e) (((e) & 0x3f) << 25) /* selects events class */

#define NO_ESCR -1

typedef enum {
   BSU_ESCR0 = 0,
   BSU_ESCR1,
   FSB_ESCR0,
   FSB_ESCR1,
   FIRM_ESCR0,
   FIRM_ESCR1,
   FLAME_ESCR0,
   FLAME_ESCR1,
   DAC_ESCR0,
   DAC_ESCR1,
   MOB_ESCR0,
   MOB_ESCR1,
   PMH_ESCR0,
   PMH_ESCR1,
   SAAT_ESCR0,
   SAAT_ESCR1,
   U2L_ESCR0,
   U2L_ESCR1,
   BPU_ESCR0,
   BPU_ESCR1,
   IS_ESCR0,
   IS_ESCR1,
   ITLB_ESCR0,
   ITLB_ESCR1,
   CRU_ESCR0,
   CRU_ESCR1,
   IQ_ESCR0,
   IQ_ESCR1,
   RAT_ESCR0,
   RAT_ESCR1,
   SSU_ESCR0,
   MS_ESCR0 = 32,
   MS_ESCR1,
   TBPU_ESCR0,
   TBPU_ESCR1,
   TC_ESCR0,
   TC_ESCR1,
   IX_ESCR0 = 40,
   IX_ESCR1,
   ALF_ESCR0,
   ALF_ESCR1,
   CRU_ESCR2,
   CRU_ESCR3,
   CRU_ESCR4 = 64,
   CRU_ESCR5 
} ESCR_id;

// Other model-specific registers (MSRs) used for event selection
#define TC_PRECISE_EVENT 0x3f0
#define IA32_PEBS_ENABLE 0x3f1
#define PEBS_MATRIX_VERT 0x3f2

//------------------ Current Performance Counters State ------------------

/* arrays, indexed by counter number, indicating the value of the
   counter's CCCR and the value of the ESCR specified by the CCCR 
   if enabled (else 0) */
typedef struct perfctr_state {
   unsigned cccr_val[NUM_COUNTERS];
   unsigned escr_val[NUM_COUNTERS];
   unsigned pebs_msr[2];
} perfctr_state_t;

typedef struct perfctr_val {
   unsigned ctr;
   unsigned low;
   unsigned high;
} perfctr_val_t;

/* routines to get/set current machine perfctr state */
void retrieve_machine_state(perfctr_state_t *);
void set_machine_state(perfctr_state_t *);
void get_perfctr_value(perfctr_val_t *);
void set_perfctr_value(perfctr_val_t *);

void read_hwc_all_cpus(void *);

#endif // _HWCOUNTER_P4_H_
