#include "kernInstIoctls.h"
#include <asm/mmu_context.h>
#include <asm/pgtable.h>
#include <asm/machdep.h>
#include <asm/pmc.h>
#include <asm/abs_addr.h>
#include <asm/hvcall.h>
#include <asm/systemcfg.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include "kerninstmalloc.h"

/* Many functions names in this file are the same as kernel counterparts,
   except they are preceded by kerninst_ prefix.  These are slightly
   modified kernel functions.  The reason for modification
   is stated in the comment preceding the said functions.
*/


int kerninst_on_each_cpu(void (*func) (void *info), void *info,
                              int retry, int wait)
{
        int ret = 0;

#ifdef CONFIG_PREEMPT
        preempt_disable();
#endif
        ret = smp_call_function_ptr(func, info, retry, wait);
        func(info);
#ifdef CONFIG_PREEMPT
        preempt_enable();
#endif
        return ret;
}


/* Get access to  the needed unexported kernel functions. kerninstd
   determines their addresses based on symbol information
 */
int init_requested_pointers_to_kernel (kptr_t kernelptrs[] ) {
    get_lock_slot_ptr = (void *)kernelptrs[KERNINST_GET_LOCK_SLOT_INDEX];
    if (!get_lock_slot_ptr ) {
       printk("get_lock_slot pointer uninitialized\n");
       return -1;
    }
    bolted_pgd = (void *)kernelptrs[KERNINST_BOLTED_DIR_INDEX]; 
    //NOTE: bolted_dir is what we actually want here, no mistake here
    if (!bolted_pgd) {
       printk("bolted_pgd pointer uninitialized\n");
       return -1;
    }

    hash_table_lock = (void *) kernelptrs[KERNINST_HASH_TABLE_LOCK_INDEX];
    if (!hash_table_lock) {
       printk("hash_table_lock pointer uninitialized\n");
       return -1;
    }
    __pmd_alloc_ptr = (void *)kernelptrs[KERNINST_PMD_ALLOC_INDEX];
    if (!__pmd_alloc_ptr) {
       printk("pmd_alloc pointer uninitialized\n");
       return -1;
    }
    pte_alloc_ptr = (void *)kernelptrs[KERNINST_PTE_ALLOC_INDEX];
    if (pte_alloc_ptr == NULL)  //change of name between diff 2.4 versions
       pte_alloc_ptr = (void *)kernelptrs[KERNINST_PTE_ALLOC_KERNEL_INDEX];
    if (!pte_alloc_ptr) {
       printk("pte_alloc pointer uninitialized\n");
       return -1;
    }
    ppc_md_ptr = (void *)kernelptrs[KERNINST_PPC_MD_INDEX];
    if (!ppc_md_ptr) {
       printk("ppc_md pointer uninitialized\n");
       return -1;
    }
    btmalloc_mm = (void *)kernelptrs[KERNINST_BTMALLOC_MM_INDEX];
    if (!btmalloc_mm) {
       printk("btmalloc_mm pointer uninitialized\n");
       return -1;
    }
    local_free_bolted_pages = (void *)kernelptrs[KERNINST_LOCAL_FREE_BOLTED_PAGES_INDEX];
    if (!local_free_bolted_pages) {
       printk("local_free_bolted_pages pointer uninitialized\n");
       return -1;
    }
    smp_call_function_ptr = (void *)kernelptrs[KERNINST_SMP_CALL_FUNCTION_INDEX];
    if (!smp_call_function_ptr) {
       printk("smp_call_function pointer uninitialized\n");
       return -1;
    }
    ste_allocate_ptr = (void *)kernelptrs[KERNINST_STE_ALLOCATE_INDEX];
    if (!ste_allocate_ptr) {
       printk("ste_allocate pointer uninitialized\n");
       return -1;
    }
    htab_data_ptr = (void *)kernelptrs[KERNINST_HTAB_DATA_INDEX];
    if (!htab_data_ptr) {
       printk("htab_data pointer uninitialized\n");
       return -1;
    }  
    hpte_remove_ptr = (void *)kernelptrs[KERNINST_HPTE_REMOVE_INDEX];
    if (!hpte_remove_ptr) {
       printk("hpte_remove pointer uninitialized\n");
       return -1;
    }  
    find_linux_pte_ptr = (void *)kernelptrs[KERNINST_FIND_LINUX_PTE_INDEX];
    if (!find_linux_pte_ptr) {
       printk("find_linux_pte pointer uninitialized\n");
       return -1;
    }  
    pSeries_tlbie_lock_ptr = (void *)kernelptrs[KERNINST_PSERIES_TLBIE_LOCK_INDEX];
    if (!pSeries_tlbie_lock_ptr) {
       printk("pSeries_tlbie_lock pointer uninitialized\n");
       return -1;
    }  
    plpar_pte_remove_ptr = (void *)kernelptrs[KERNINST_PLPAR_PTE_REMOVE_INDEX];
    if (!plpar_pte_remove_ptr) {
       printk("plpar_pte_remove pointer uninitialized\n");
       return -1;
    }  
    plpar_hcall_norets_ptr = (void *)kernelptrs[KERNINST_PLPAR_HCALL_NORETS_INDEX];
    if (!plpar_hcall_norets_ptr) {
       printk("plpar_hcall_norets pointer uninitialized\n");
       return -1;
    }  
    paca_ptr = (void *)kernelptrs[KERNINST_PACA_INDEX];
    if (!paca_ptr) {
       printk("paca pointer uninitialized\n");
       return -1;
    }  
    cur_cpu_spec_ptr = (void *)kernelptrs[KERNINST_CUR_CPU_SPEC_INDEX];
    if (!cur_cpu_spec_ptr) {
       printk("cur_cpu_spec pointer uninitialized\n");
       return -1;
    }  
   
    return 0;
}


/* This is a copy of a kernel function in a header file with call 
   through pointer substituted in */
static inline pmd_t *kerninst_pmd_alloc(struct mm_struct *mm, pgd_t *pgd, unsigned long address)
{
        if (pgd_none(*pgd))
                return __pmd_alloc_ptr(mm, pgd, address);
        return pmd_offset(pgd, address);
}

#if 0
/* This is a copy of a kernel function that is here because
   it needs to be modified for removal of bolted entries.  
   FIXME: does not work.  Is invalidation even necessary? */
 
static void kerninst_rpa_lpar_hpte_invalidate(unsigned long slot, 
				     unsigned long secondary,
				     unsigned long va,
				     int large, int local)
{
	unsigned long lpar_rc;
	unsigned long dummy1, dummy2;

	/* 
	 * Do remove a bolted entry.  Hence  flags are zero 
	 */
	lpar_rc = plpar_pte_remove_ptr(0, slot, (0x1UL << 4), 
				   &dummy1, &dummy2);

	if (lpar_rc != H_Success)
		panic("Bad return code from invalidate rc = %lx\n", lpar_rc);
}
#endif

/* Modified copy of a kernel function.  Takes care of removal
   of bolted entries */
static void kerninst_hpte_invalidate(unsigned long slot, 
			    unsigned long secondary,
			    unsigned long va,
			    int large, int local)
{
	HPTE *hptep = htab_data_ptr->htab + slot;
	Hpte_dword0 dw0;
	unsigned long vpn, avpn;
	unsigned long flags;

	if (large)
		vpn = va >> LARGE_PAGE_SHIFT;
	else
		vpn = va >> PAGE_SHIFT;

	avpn = vpn >> 11;

	dw0 = hptep->dw0.dw0;

	/*
	 * Do not remove bolted entries.  Alternatively, we could check
	 * the AVPN, hash group, and valid bits.  By doing it this way,
	 * it is common with the pSeries LPAR optimal path.
	 */
//	if (dw0.bolted) return;
	//note that we DO want to remove a bolted entry !

	/* Invalidate the hpte. */
	hptep->dw0.dword0 = 0;

	/* Invalidate the tlb */
	spin_lock_irqsave(pSeries_tlbie_lock_ptr, flags);
	_tlbie(va, large);
	spin_unlock_irqrestore(pSeries_tlbie_lock_ptr, flags);
}

/* Removes a hash page, removing bolted entries, unlike kernel counterpart */
void kerninst_flush_hash_page(unsigned long context, unsigned long ea, pte_t *ptep)
{
	unsigned long vsid, vpn, va, hash, secondary, slot, flags, lock_slot;
	unsigned long large = 0, local = 0;
	pte_t pte;

	if ((ea >= USER_START) && (ea <= USER_END))
		vsid = get_vsid(context, ea);
	else
		vsid = get_kernel_vsid(ea);

	va = (vsid << 28) | (ea & 0x0fffffff);
	if (large)
		vpn = va >> LARGE_PAGE_SHIFT;
	else
		vpn = va >> PAGE_SHIFT;

	lock_slot = get_lock_slot_ptr(vpn); 
	hash = hpt_hash(vpn, large);

	spin_lock_irqsave(&hash_table_lock[lock_slot], flags);

	pte = __pte(pte_update(ptep, _PAGE_HPTEFLAGS, 0));
	secondary = (pte_val(pte) & _PAGE_SECONDARY) >> 15;
	if (secondary) hash = ~hash;
	slot = (hash & htab_data_ptr->htab_hash_mask) * HPTES_PER_GROUP;
	slot += (pte_val(pte) & _PAGE_GROUP_IX) >> 12;
	
	if (pte_val(pte) & _PAGE_HASHPTE) {
             switch(systemcfg->platform) {
	        case PLATFORM_PSERIES:
		    kerninst_hpte_invalidate(slot, secondary, va, large, local);
                    break;
		case PLATFORM_PSERIES_LPAR:
                    //FIXME: invalidate doesn't work!  Is it even possible to invalidate bolted pages on lpar??
//		    kerninst_rpa_lpar_hpte_invalidate(slot, secondary, va, large, local);
                    break;
                default:
                    printk("kerninst_flush_hash_page: Invalid platform %d, could not remove page\n", systemcfg->platform);
	    }
	}

	spin_unlock_irqrestore(&hash_table_lock[lock_slot], flags);
}

/* Removes pages used by kerninst */
static int kerninst_local_free_bolted_pages(unsigned long ea, unsigned long num) {
	int i;
	pte_t pte;

	for(i=0; i<num; i++) {
		pte_t *ptep = find_linux_pte_ptr(bolted_pgd, ea);
		if(!ptep) {
			panic("free_bolted_pages - page being freed "
			      "(0x%lx) is not bolted", ea );
		}
		pte = *ptep;
		pte_clear(ptep);
		__free_pages(pte_page(pte), 0);
		kerninst_flush_hash_page(0, ea, ptep); 
		ea += PAGE_SIZE;
	}
	return 1;
}





/* This calculation is the same as assembly code, but different (?)
   from the kernel function 
   FIXME: is this still necessary ?? 
*/

static inline unsigned long
kerninst_get_kernel_vsid( unsigned long ea )
{
        unsigned long ordinal, vsid;

//        ordinal = ((ea >> 28) * LAST_USER_CONTEXT) | (ea >> 60);
        ordinal = ( (  (ea &  0x0ffffffff0000000) >> 28) << 15 ) | (ea >> 60);
//        vsid = (ordinal * VSID_RANDOMIZER) & VSID_MASK;
        vsid = (ordinal * VSID_RANDOMIZER) ;

        ifppcdebug(PPCDBG_HTABSTRESS) {
                /* For debug, this path creates a very poor vsid distribuition.
                 * A user program can access virtual addresses in the form
                 * 0x0yyyyxxxx000 where yyyy = xxxx to cause multiple mappings
                 * to hash to the same page table group.
                 */
                ordinal = ((ea >> 28) ) | (ea >> 44);
                vsid = ordinal & VSID_MASK;
        }

        return vsid;
}

/* This is the main reason of existence for this module: allocate memory 
   and map it into 0xb region of kernel space.  Based on btmalloc() */
void* kerninst_malloc (unsigned long size) {
	pgd_t *pgdp;
	pmd_t *pmdp;
	pte_t *ptep, pte;
	unsigned long ea_base, ea, hpteflags, lock_slot;
	struct vm_struct *area;
	unsigned long pa, pg_count, page, vsid, slot, va, rpn, vpn;
  
	size = PAGE_ALIGN(size);
	if (!size || (size >> PAGE_SHIFT) > num_physpages) return NULL;

	spin_lock(&btmalloc_mm->page_table_lock);

	/* Get a virtual address region in the bolted space */
	area = get_kerninst_area(size, 0);
	if (!area) {
		spin_unlock(&btmalloc_mm->page_table_lock);
		return NULL;
	}

	ea_base = (unsigned long) area->addr;
	pg_count = (size >> PAGE_SHIFT);
	/* Create a Linux page table entry and an HPTE for each page */
	for(page = 0; page < pg_count; page++) {
                pa = (unsigned long) __v2a(get_free_page(GFP_KERNEL));
		ea = ea_base + (page * PAGE_SIZE);
		vsid = kerninst_get_kernel_vsid(ea);
		va = ( vsid << 28 ) | ( ea & 0xfffffff );
		vpn = va >> PAGE_SHIFT;
		lock_slot = get_lock_slot_ptr(vpn); 
		rpn = pa >> PAGE_SHIFT;

		spin_lock(&hash_table_lock[lock_slot]);
		/* Get a pointer to the linux page table entry for this page
		 * allocating pmd or pte pages along the way as needed.  Note
		 * that the pmd & pte pages are not themselfs bolted.
		 */
		pgdp = pgd_offset_b(ea);
		pmdp = kerninst_pmd_alloc(btmalloc_mm, pgdp, ea);
		ptep = pte_alloc_ptr(btmalloc_mm, pmdp, ea);
		pte = *ptep;

		/* Clear any old hpte and set the new linux pte */
		set_pte(ptep, mk_pte_phys(pa & PAGE_MASK, PAGE_KERNEL));

		hpteflags = _PAGE_ACCESSED|_PAGE_COHERENT|PP_RWXX;

		pte_val(pte) &= ~_PAGE_HPTEFLAGS;
		pte_val(pte) |= _PAGE_HASHPTE;
		pte_val(pte) |= _PAGE_PRESENT; //this is needed so that find_linux_pte works when we are freeing

                //finally, insert into HPT
		slot = ppc_md_ptr->hpte_insert(vpn, rpn, hpteflags, 1, 0);  
         
		pte_val(pte) |= ((slot<<12) & 
				 (_PAGE_GROUP_IX | _PAGE_SECONDARY));

		*ptep = pte;

		spin_unlock(&hash_table_lock[lock_slot]);
	}
	spin_unlock(&btmalloc_mm->page_table_lock);
	return (void*)ea_base;
}

/*
 * Free a range of bolted pages that were allocated with kerninst_malloc
 */
void kerninst_free(void *ea) {
	struct vm_struct **p, *tmp;
	unsigned long size = 0;
	if ((!ea) || ((PAGE_SIZE-1) & (unsigned long)ea)) {
		printk(KERN_ERR "Trying to kerninst_free() bad address (%p)\n", ea);
		return;
	}

	spin_lock(&btmalloc_mm->page_table_lock);

	/* Scan the bolted memory list for an entry matching
	 * the address to be freed, get the size (in bytes)
	 * and free the entry.  The list lock is not dropped
	 * until the page table entries are removed.
	 */
	for(p = &ktmlist; (tmp = *p); p = &tmp->next ) {
		if ( tmp->addr == ea ) {
			size = tmp->size;
			break;
		}
	}

	/* If no entry found, it is an error */
	if ( !size ) {
		printk(KERN_ERR "Trying to kerninst_free() bad address (%p)\n", ea);
		spin_unlock(&btmalloc_mm->page_table_lock);
		return;
	}

	/* Free up the bolted pages and remove the page table entries */
	if(kerninst_local_free_bolted_pages((unsigned long)ea, size >> PAGE_SHIFT)) {
		*p = tmp->next;
		kfree(tmp);
	} else {
                printk(KERN_ERR "could not free bolted page\n");
        }

	spin_unlock(&btmalloc_mm->page_table_lock);
}



static struct vm_struct *get_kerninst_area(unsigned long size, 
				      unsigned long flags) {
	unsigned long addr;
	struct vm_struct **p, *tmp, *area;
  
	area = (struct vm_struct *) kmalloc(sizeof(*area), GFP_KERNEL);
	if (!area) return NULL;

	addr = KTALLOC_START;
	for (p = &ktmlist; (tmp = *p) ; p = &tmp->next) {
		if (size + addr < (unsigned long) tmp->addr)
			break;
		addr = tmp->size + (unsigned long) tmp->addr;
		if (addr + size > KTALLOC_END) {
			kfree(area);
			return NULL;
		}
	}

	if (addr + size > KTALLOC_END) {
		kfree(area);
		return NULL;
	}
	area->flags = flags;
	area->addr = (void *)addr;
	area->size = size;
	area->next = *p;
	*p = area;
	return area;
}

/* This is lifted directly from 2.6.8 kernel */
void ppc64_enable_pmcs(void)
{
	unsigned long hid0;
	unsigned long set, reset;
	int ret;
	unsigned int ctrl;

	switch (systemcfg->platform) {
		case PLATFORM_PSERIES:
			hid0 = mfspr(HID0);
			hid0 |= 1UL << (63 - 20);

			/* POWER4 requires the following sequence */
			asm volatile(
				"sync\n"
				"mtspr	%1, %0\n"
				"mfspr	%0, %1\n"
				"mfspr	%0, %1\n"
				"mfspr	%0, %1\n"
				"mfspr	%0, %1\n"
				"mfspr	%0, %1\n"
				"mfspr	%0, %1\n"
				"isync" : "=&r" (hid0) : "i" (HID0), "0" (hid0):
				"memory");
			break;

		case PLATFORM_PSERIES_LPAR:
			set = 1UL << 63;
			reset = 0;
			ret = plpar_hcall_norets_ptr(H_PERFMON, set, reset);
			if (ret)
				printk(KERN_ERR "H_PERFMON call returned %d",
				       ret);
			break;
		default:
			break;
	}

#define FW_FEATURE_SPLPAR          (1UL<<20)
#define CPU_FTR_SMT                     0x0000010000000000
	/* instruct hypervisor to maintain PMCs */
/*
	if (cur_cpu_spec_ptr->firmware_features & FW_FEATURE_SPLPAR) {
		char *ptr = (char *)paca_ptr[smp_processor_id()].xLpPacaPtr;
		ptr[0xBB] = 1;
	}
*/
	/*
	 * On SMT machines we have to set the run latch in the ctrl register
	 * in order to make PMC6 spin.
	 */
/*
	if (cur_cpu_spec_ptr->cpu_features & CPU_FTR_SMT) {
		ctrl = mfspr(CTRLF);
		ctrl |= RUNLATCH;
		mtspr(CTRLT, ctrl);
	}
*/
}

int init_kerninstmalloc() {
    inter_module_register("kerninst_malloc", THIS_MODULE, kerninst_malloc);
    inter_module_register("kerninst_free", THIS_MODULE, kerninst_free);
    inter_module_register("init_requested_pointers_to_kernel", THIS_MODULE,
			  init_requested_pointers_to_kernel);
    inter_module_register("ppc64_enable_pmcs", THIS_MODULE, ppc64_enable_pmcs);
    inter_module_register("kerninst_on_each_cpu", THIS_MODULE, kerninst_on_each_cpu);
    return 0;
}

void cleanup_kerninstmalloc() {
   inter_module_unregister("kerninst_malloc");
   inter_module_unregister("kerninst_free");
   inter_module_unregister("init_requested_pointers_to_kernel");
   inter_module_unregister("ppc64_enable_pmcs");
   inter_module_unregister("kerninst_on_each_cpu");
}

module_init(init_kerninstmalloc);
module_exit(cleanup_kerninstmalloc);
