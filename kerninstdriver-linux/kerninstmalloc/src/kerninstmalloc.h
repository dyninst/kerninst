/* This module contains kerninst memory allocation and performance counter
   initialization routines. 
 */

MODULE_AUTHOR ("Igor Grobman, igor@cs.wisc.edu");
MODULE_DESCRIPTION ("kerninst memory allocation for ppc64 version 2.1");
MODULE_LICENSE ("GPL");


#define KTALLOC_START 0xbfffffffff000000ULL
#define KTALLOC_END   0xbfffffffffffffffULL //end of addr space


/* Below are functions/variables used for memory allocation
   Some are not exported, so we need the daemon to let us 
   know their addresses via init_requested_pointers_to_kernel  */
static struct vm_struct *get_kerninst_area(unsigned long size, unsigned long flags); 
static spinlock_t *hash_table_lock = NULL;
static unsigned long (*get_lock_slot_ptr)(unsigned long vpn) = NULL;
static pmd_t * (*__pmd_alloc_ptr)(struct mm_struct *mm, pgd_t *pgd, unsigned long address) = NULL;
static pte_t * (*pte_alloc_ptr)(struct mm_struct *mm, pmd_t *pmd, unsigned long address) = NULL;
static int (*local_free_bolted_pages)(unsigned long ea, unsigned long num)= NULL;
static pgd_t *bolted_pgd = NULL;
static struct mm_struct *btmalloc_mm = NULL;
static struct machdep_calls *ppc_md_ptr = NULL;
static struct vm_struct *ktmlist = NULL; 
int (*smp_call_function_ptr) (void (*func) (void *info), void *info, int nonatomic, int wait);
int (*ste_allocate_ptr) ( unsigned long ea, unsigned long trap);
void (*print_hpte_ptr)(unsigned long ea);
int *kerninst_debug_flag_ptr;
void (*print_ste_ptr) (unsigned long ea);
int (*find_ste_ptr) (unsigned long ea);
HTAB *htab_data_ptr;
long (*hpte_remove_ptr)(unsigned long hpte_group);
pte_t * (*find_linux_pte_ptr)(pgd_t *pgdir, unsigned long ea);
static spinlock_t *pSeries_tlbie_lock_ptr;
long (*plpar_pte_remove_ptr)(unsigned long flags,
		      unsigned long ptex,
		      unsigned long avpn,
			     unsigned long *old_pteh_ret, unsigned long *old_ptel_ret);

long (*plpar_hcall_norets_ptr)(unsigned long opcode, ...);
struct paca_struct *paca_ptr;
struct cpu_spec               *cur_cpu_spec_ptr;
