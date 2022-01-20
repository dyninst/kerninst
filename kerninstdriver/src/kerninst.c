/*
 * kerninst.c
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/modctl.h> /* for struct modldrv */
#include <sys/stat.h> /* S_IFCHR */

#define DRV_NAME "kerninst"

#include "kerninst.h"
#include "kernInstIoctls.h"
#include "module_unloading.h"
#include "calleeCounter.h"

/* These must always be included last: */
#include <sys/ddi.h>
#include <sys/sunddi.h>

/*
. * static declarations of cb_ops entry point functions
 */
static int      kerninst_identify(dev_info_t * dip);
static int      kerninst_attach(dev_info_t * dip, ddi_attach_cmd_t cmd);
static int      kerninst_detach(dev_info_t * dip, ddi_detach_cmd_t cmd);
static int      kerninst_open(dev_t * devp, int flag, int otype, cred_t * credp);
static int      kerninst_close(dev_t dev, int flag, int otype, cred_t * credp);
static int      kerninst_getinfo(dev_info_t * dip, ddi_info_cmd_t infocmd, 
                                 void *arg, void **result);
static int	kerninst_ioctl(dev_t dev, int cmd, intptr_t arg, int mode,
                               cred_t *credp, int *rvalp);
static int	kerninst_mmap(dev_t dev, off_t off, int prot);
static int      kerninst_segmap(dev_t dev, off_t off, struct as *asp, 	
                                caddr_t *addrp, off_t len, unsigned int prot, 
                                unsigned int maxprot, unsigned int flags, 
                                cred_t *credp);

struct cb_ops   kerninst_cb_ops =
{
	kerninst_open,		/* open */
	kerninst_close,		/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	kerninst_ioctl,		/* ioctl */
	nodev,			/* devmap */
	kerninst_mmap,		/* mmap */
	kerninst_segmap,		/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,			/* prop_op */
	0,			/* streamtab */
	D_NEW | D_MP		/* driver compatibility flag */
};

/* global driver information */
static  u_int 		kerninst_is_open = 0;

static  hwprfunit_t*    usp_inf;

/*
 * declare and intialize the "loadable module wrapper"
 */
extern struct mod_ops mod_driverops;

/* The modldrv struc exports driver-specific info to the kernel,
   so says modldrv(9s) */

/*
 * static declarations of dev_ops entry point functions
 * see dev_ops(9s)
 *
 * Why isn't it declared as static in hwprf?  -ari
 */
struct dev_ops  kerninst_ops = {
	DEVO_REV,	/* driver build version; DEVO_REV is mandatory */
	0,		/* device reference count; 0 is mandatory */
	kerninst_getinfo,	/* implements getinfo(9e) */
	kerninst_identify,	/* implements identify(9e) */
	nulldev,	/* implements probe(9e); non-self-identifying devices */
	kerninst_attach,	/* implements attach(9e) */
	kerninst_detach,	/* implements detach(9e) */
	nodev,		/* reset -- nodev is mandatory */
	&kerninst_cb_ops,	/* ptr to cb_ops(9s) struc for leaf drivers (?) */
	NULL	        /* ptr to bus_ops(9s); NULL for leaf driver (?) */
};

static struct modldrv modldrv = {
	&mod_driverops,   /* all drivers must use this value, so says modldrv(9s) */
	"kerninst driver v2.1.1", /* any str up to MODMAXNAMELEN; describes module */
	&kerninst_ops	  /* ptr to dev_ops(9s) structure */
};

static struct modlinkage modlinkage = {
   MODREV_1, /* this value is mandatory; so says modlinkage(9s) */
   (void *) &modldrv, /* ptr to linkage structure...for a driver there's just one;
			 so says modlinkage(9s) */
   NULL /* null-terminated... */
};

/* _init MUST be defined for all loadable modules; so says Intro(9E).
   It'll be called before any other routine in the loadable module.
   It should return whatever mod_install(9F) returns.
   Intro(9E) says everything in a module should be declared static, yet
   it's not done here (nor is it done in Intro(9E)'s sample program).
 */

#define NUM_INSTANCES 1

void* statep;

int _init(void) {
   /* modlinkage structure describes type(s) of module elems that this
      loadable module includes. */

   int ret;

   /* cmn_err(CE_CONT, "kerninst _init: welcome!\n"); */

   ret = mod_install(&modlinkage);

   if (ret == 0) {
      ddi_soft_state_init(&statep, sizeof(hwprfunit_t), NUM_INSTANCES);
         /* statep is an opaque ptr; gets initialized by ddi_soft_state_init()
	    to point to implementation-dependent data.  It seems that the idea
	    of ddi_soft_state in general is to allow drivers to maintain state info
	    on a PER-INSTANCE basis.  ddi_soft_state_zalloc() usually gets called
	    on attach(9e), being passed an item num used in subsequent calls to
	    ddi_get_soft_state() and ddi_soft_state_free(); the item num is usually
	    just the instance num of the devinfo node, obtained via
	    ddi_get_instance(9f).  ddi_soft_state_fini() is usually called
	    from the _fini(9e) routine.
	    These routines coordinate access in an MT-safe fasion, so no
	    additional locks are needed.
	    */
      kerninst_init_callee_counter();
   }

   return ret;
}

/* _fini(9e) -- prepares a loadable module for unloading; called when system
   wants to unload a module.  If we decide module may be unloaded, return whatever
   mod_remove(9f) returns */

int _fini(void) {
   int ret;

/*   cmn_err(CE_CONT, "kerninst _fini: welcome!\n"); */

   if (kerninst_is_open != 0)
      return (EBUSY);

   ret = mod_remove(&modlinkage); /* seems to call the detach(9e) member! */
   if (ret == 0) {
      /* release soft state that was allocated in _init */
      ddi_soft_state_fini(&statep);
      /* delete the callee counter's hash table */
      kerninst_delete_callee_counter();
   }

   return(ret);
}

/* _info(9e) -- should return whatever mod_info(9f) returns.
 */
int _info(struct modinfo * modinfop) {
   /* cmn_err(CE_CONT, "kerninst _info: welcome!\n"); */
  
   return (mod_info(&modlinkage, modinfop));
}

static int
kerninst_getinfo(dev_info_t* dip, /* 1st param unused, says getinfo(9e) */
	     ddi_info_cmd_t infocmd, /* cmd arg, either DDI_INFO_DEVT2DEVINFO
					or DDI_INFO_DEV2INSTANCE */
	     void* arg, /* cmd-specific arguments */
	     void **result /* ptr to where requested info gets stored */
	     ) {
        hwprfunit_t *usp;
        int retval;
        int instance = 0;

/*        cmn_err(CE_CONT, "ari welcome to kerninst_getinfo!\n");  */

        switch (infocmd) {
        case DDI_INFO_DEVT2DEVINFO:
	        /* in this case we should return (in the field pointed to by
		   result) the dev_info_t ptr associated with the dev_t arg; so
		   says getinfo(9e).  Why doesn't hwprf do that?  -ari */

                usp = (hwprfunit_t *) ddi_get_soft_state(statep, instance);
                *result = (void *)usp->dip;
                retval = DDI_SUCCESS;
                break;
        case DDI_INFO_DEVT2INSTANCE:
		/* in this case we should return the instance number associated
		   with the dev_t arg. */

                *result = (void *)instance;
                retval = DDI_SUCCESS;
                break;
        default: {
	        cmn_err(CE_NOTE, "ari getinfo failure!\n");
                retval = DDI_FAILURE;
	}
        }
        return (retval);
}

static int
kerninst_identify(dev_info_t * dip) {
   /* should return true iff this driver drives the device
      pointed to by 'dip' */

   /* note: DRV_NAME is defined above (formerly in inc/hwprfio.h) */

   /* cmn_err(CE_CONT, "kerninst: welcome to identify --ari\n"); */

   if (strcmp(ddi_get_name(dip), DRV_NAME) == 0) {
      return (DDI_IDENTIFIED);
   } else
      return (DDI_NOT_IDENTIFIED);
}

uint64_t module_load_time = 0;

extern unsigned long long vstop_stats_addr;
extern unsigned long long vrestart_stats_addr;

extern void *initialize_logged_writes(void);
extern void destroy_logged_writes(void *);

extern void *initialize_logged_allocRequests(void);
extern void destroy_logged_allocRequests(void *);

static int
kerninst_attach(dev_info_t * dip, ddi_attach_cmd_t cmd) {
   /* If 'cmd' isn't DDI_ATTACH then we should return DDI_FAILURE.  See attach(9e).

      It appears that attach gets called (after _init) upon add_drv(1)
      
      Attach gets called once per _instance_ of the device.  Until it
      succeeds, only open(9e) and getinfo(9e) will be called.  We should
      use ddi_get_instance(9f) to get the instance number.

      Attach won't be called until identify(9e) and probe(9e) succeed.

      When called with DDI_ATTACH, all normal kernel services, such as
      kmem_alloc(9f), are available to us.
    */

	int	i = 0;
	int	instance;
	hwprfunit_t *usp;

/*	cmn_err(CE_CONT, "kerninst: welcome to attach --ari\n"); */

	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);

	/* Get device instance num, as assigned by the system.  Devices attached to the
	   same driver get unique instances; this way, the system and driver can
	   uniquely identify devices of the same type.  Once assigned, the instance num
	   remains unchanged even across reconfigs and reboots.
	   See ddi_get_instance(9f) */
	instance = ddi_get_instance(dip);

	/* _init has called ddi_soft_state_init() [initializes statep]; we call
	   ddi_soft_state_zalloc(9f) */
        if (ddi_soft_state_zalloc(statep, instance) != 0)
	        return (DDI_FAILURE);

	/* Now the soft state's been allocated; grab it and use it */
	usp = (hwprfunit_t *) ddi_get_soft_state(statep, instance);
	usp->dip = dip;
	mutex_init(&usp->kerninst_lock,
		   "kerninst_lock", /* for statistics & debugging */
		   MUTEX_DEFAULT,
		      /* but man page says drivers should use MUTEX_DRIVER! --ari */
		   NULL /* NULL --> mutex won't be used inside of an interrupt hndlr */
		   );

	usp->num_unsaved_timers = 0;
        usp->global_loggedWrites = NULL;
        usp->global_loggedAllocRequests = NULL;
        usp->cswitch_stuff_enabled = 1;
        usp->give_debug_output = 0;

	/* ddi_create_minor_node(9f) -- provide info to enable system to create
	   the /dev and /devices hierarchies */
	if(ddi_create_minor_node(dip,
				 "kerninst", /* name of this minor device; used to
					     create the minor name of the special
					     file under /devices */
				 S_IFCHR, /* this is a character minor device */
				 0, /* the minor number for this minor device */
				 DDI_PSEUDO, /* this is a pseudo device */
				    /* Creates a name in /dev that refers to the
				       names in /devices */
				 0 /* not a clone device (?) */
				 ) == DDI_FAILURE) {

           mutex_destroy(&usp->kerninst_lock);
           ddi_remove_minor_node(dip,
                                 NULL /* remove all minor data strucs from this
                                         dev_info */
                                 );
           return (DDI_FAILURE);
	}

	// read current time & truncate to 48 bits - for debugging timers
        __asm__ __volatile__ ("rd %%tick, %0"
                              : "=r" (module_load_time)
			      : /* no inputs */);
        module_load_time <<= 16;
        module_load_time >>= 16;

	cmn_err(CE_CONT, "kerninst attach: vstop_stats is @ 0x%0llx, vrestart_stats is @ 0x%0llx\n", vstop_stats_addr, vrestart_stats_addr); 

/*	cmn_err(CE_CONT, "kerninst attach: ddi_create_minor_node worked --ari\n"); */

/* 	cmn_err(CE_CONT, "kerninst attach: check /dev and /devices for interesting stuff --ari\n"); */

	return (DDI_SUCCESS);
}

static int
kerninst_detach(dev_info_t * dip, ddi_detach_cmd_t cmd)
{
	hwprfunit_t*	usp;
	int		instance;

/*	cmn_err(CE_CONT, "kerninst detach: welcome --ari\n"); */

	instance = ddi_get_instance(dip);
	usp = (hwprfunit_t *)ddi_get_soft_state(statep, instance);

        /* Close down "usp" shop */

	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);

	/* ddi_remove_minor_node(9f) -- remove a struc from linked list of minor data
	   strucs pointed to by the driver's dev_info structure */
	   
	ddi_remove_minor_node(dip,
			      NULL /* NULL --> remove all minor data strucs from this
				      dev_info */
			      );

/* 	cmn_err(CE_CONT, "kerninst detach finished --ari\n"); */

	return (DDI_SUCCESS);
}

extern uint32_t get_cpu_impl_num();
extern uint32_t getSystemClockFreq();

static int
kerninst_open(dev_t * devp, int flag, int otype, cred_t * credp)
{
	int             retval = 0;
	hwprfunit_t*	usp;
	int		instance = 0;
	int lcv;
        uint32_t impl;

	usp = (hwprfunit_t *)ddi_get_soft_state(statep, instance);
	usp_inf = usp;

	if (kerninst_is_open == 0) {
	    kerninst_is_open = 1;
	} else {
	    cmn_err(CE_WARN, "Driver already open.\n");
	    return(ENXIO);
	}

	mutex_enter(&usp->kerninst_lock);

        /*cmn_err(CE_CONT, "kerninst open: disabling module unloading and initializing logged writes\n");*/

        kerninst_modunload_disable();
        
        if (usp->global_loggedWrites != NULL)
           cmn_err(CE_CONT, "kerninst open: strange, global_loggedWrites not NULL\n");
        else
           usp->global_loggedWrites = initialize_logged_writes();

        if (usp->global_loggedAllocRequests != NULL)
           cmn_err(CE_CONT, "kerninst open: strange, global_loggedAllocRequests not NULL\n");
        else
           usp->global_loggedAllocRequests = initialize_logged_allocRequests();

	kerninst_reset_callee_counter();

        initialize_all_vtimers(&usp->av_vtimers);
        stacklist_initialize(&usp->sl_heap);
        hash_table_initialize(&usp->ht_vtimers);

	mutex_exit(&usp->kerninst_lock);
	
	return (retval);
}


/* close is called on last close only */
static int
kerninst_close(dev_t dev, int flag, int otype, cred_t * credp)
{
	hwprfunit_t* 	usp;
	int		i;
	int		instance = 0;
	int             retval = 0;

	usp = (hwprfunit_t *)ddi_get_soft_state(statep, instance);

	mutex_enter(&usp->kerninst_lock);
	if (kerninst_is_open == 0) {
	    return (0);
	} else { 
		kerninst_is_open = 0;
	}

        cmn_err(CE_CONT, "kerninst close.  Undoing logged writes (if any)\n");
        destroy_logged_writes(usp->global_loggedWrites);
        usp->global_loggedWrites = NULL;

        cmn_err(CE_CONT, "kerninst close.  Undoing leftover alloc requests (if any)\n");
        destroy_logged_allocRequests(usp->global_loggedAllocRequests);
        usp->global_loggedAllocRequests = NULL;

        cmn_err(CE_CONT, "kerninst close.  Clearing client state\n");
        destroy_all_vtimers(&usp->av_vtimers);
        stacklist_destroy(&usp->sl_heap);
        hash_table_destroy(&usp->ht_vtimers); 

        cmn_err(CE_CONT, "kerninst close.  Re-enabling module unloading\n");
        kerninst_modunload_enable();
        
	mutex_exit(&usp->kerninst_lock);

	return (retval);
}

extern char *stubs_base, *stubs_end;

static int
kerninst_ioctl(dev_t dev, int cmd, intptr_t arg, int mode,
               cred_t *credp, int *rvalp) {
   /* see ioctl(9e).
      'cmd' gives the ioctl command; 'arg' (often a pointer) is the user argument.
      Use copyin(9f) to bring in data from user space, and copyout(9f) to
      copy data out to user space.
      'mode' contains the mode flags when the device was opened; can use to check
      permissions.  In some cases it provides addr space info about 'arg'.
      'cred_p' points to user cred structure.
      'rvalp" points to the return value for the caller.
   */


   int retval = 0;
   int instance = 0; /* is this always right?  May be safer to do a getminor(dev)!!! */
   hwprfunit_t* usp = (hwprfunit_t *)ddi_get_soft_state(statep, instance);

   switch(cmd) {
      case KERNINST_GET_TICK_RAW: {
         extern u_longlong_t ari_get_tick_raw(void); /* asm file */
         u_longlong_t raw;

         raw = ari_get_tick_raw(); /* asm file */

         if (copyout((void*)&raw, (void*)arg, 8) < 0) {
	     retval = EFAULT;
	 }
         break;
      }

      case KERNINST_GET_STICK_RAW: {
         extern u_longlong_t ari_get_stick_raw(void); /* asm file */
         u_longlong_t raw;

         raw = ari_get_stick_raw(); /* asm file */

         if (copyout((void*)&raw, (void*)arg, 8) < 0) {
	     retval = EFAULT;
	 }
         break;
      }

      case KERNINST_GET_PCR_RAW: {
         extern u_longlong_t ari_get_pcr_raw(void); /* asm file */
         u_longlong_t raw;

         raw = ari_get_pcr_raw(); /* asm file */

         if (copyout((void*)&raw, (void*)arg, 8) < 0) {
	     retval = EFAULT;
	 } 
         break;
      }

      case KERNINST_SET_PCR_RAW: {
         extern void ari_set_pcr_raw(u_longlong_t); /* asm file */

         u_longlong_t raw = 0;
         if (copyin((void*)arg, (void*)&raw, sizeof(raw)) < 0) {
	     retval = EFAULT;
	     break;
	 }
	 ari_set_pcr_raw(raw); /* asm file */

         break;
      }

      case KERNINST_GET_PIC_RAW: {
         extern u_longlong_t ari_get_pic_raw(void); /* asm file */
         u_longlong_t raw;

         raw = ari_get_pic_raw(); /* asm file */

         if (copyout((void*)&raw, (void*)arg, 8) < 0) {
	     retval = EFAULT;
	 }
         break;
      }

      case KERNINST_SET_PIC_RAW: {
         extern void ari_set_pic_raw(u_longlong_t); /* asm file */

         u_longlong_t raw = 0;
         if (copyin((void*)arg, (void*)&raw, sizeof(raw)) < 0) {
	     retval = EFAULT;
	     break;
	 }
	 ari_set_pic_raw(raw); /* asm file */

         break;
      }

      case KERNINST_DISABLE_TICK_PROTECTION: {
         extern void ari_disable_tick_protection();

         /* cmn_err(CE_CONT, "kerninst: welcome to disabletick\n"); */
	   
         /* WARNING! Certain software is sensitive to the %tick register
            setting; we mustn't touch anything except the privileged bit!
            We try hard to do that, but it's not really possible to guarantee
            that time won't go backwards slightly, due to the read-modify-write
            sequence that's required */
	   
         ari_disable_tick_protection();

         /* cmn_err(CE_CONT, "kerninst: done with disabling tick protection!\n"); */
	   
         break;
      }
      case KERNINST_VIRTUAL_TO_PHYSICAL_ADDR: {
         struct kernInstVirtualToPhysicalAddr s;
         if (copyin((void*)arg, (void*)&s, sizeof(s)) < 0) {
            retval = EFAULT;
            break;
         }

         s.physical_addr = (void*)ari_virtual_to_physical_addr(s.virtual_addr);
         if (copyout((void*)&s, (void*)arg, sizeof(s)) < 0) {
	     retval = EFAULT;
	 }
           
         break;
           
      }
           
      case KERNINST_ALLOC_BLOCK: {
         struct kernInstAllocStruct input_output_args;
         retval = copyin((void*)arg, (void*)&input_output_args,
                         sizeof(input_output_args));
         if (retval == -1) {
            cmn_err(CE_CONT, "kerninst alloc block copying failed\n");
            retval = EFAULT;
            break;
         }

         kerninst_alloc_block(usp->global_loggedAllocRequests, &input_output_args);
            /* kernInstIoctls.C */
         
         if(usp->give_debug_output)         
            cmn_err(CE_CONT, "KERNINST_ALLOC_BLOCK nbytes=%d, allocType=%d, result=0x%lx\n",
                    input_output_args.nbytes,
                    input_output_args.allocType,
                    input_output_args.result);
         
         if (copyout((void*)&input_output_args, (void*)arg,
		     sizeof(input_output_args)) < 0) {
	     retval = EFAULT;
	 }

         break;
      }
      case KERNINST_FREE_BLOCK: {
         struct kernInstFreeStruct input_args;
         retval = copyin((void*)arg, (void*)&input_args, sizeof(input_args));
         if (retval == -1) {
            retval = EFAULT;
            break;
         }
         
         if(usp->give_debug_output)
            cmn_err(CE_CONT, "KERNINST_FREE_BLOCK addr=0x%lx, mapped at 0x%lx, nbytes=%d\n",
                    input_args.kernelAddr,
                    input_args.mappedKernelAddr,
                    input_args.nbytes);
         
         kerninst_free_block(usp->global_loggedAllocRequests, &input_args);

         break;
      }
      case KERNINST_SYMTAB_STUFF_SIZE: 
      {
         /* see kernInstIoctls.h for a description of the "structure" used */

         uint32_t total_nbytes = kerninst_symtab_size();
           
         if(usp->give_debug_output)
            cmn_err(CE_CONT, "KERNINST_SYMTAB_STUFF_SIZE size=%d bytes\n", total_nbytes);

         if (copyout((void*)&total_nbytes, /* src in driver */
		     (void*)arg, /* dest in user */
		     sizeof(total_nbytes)) < 0) {
	     retval = EFAULT;
	 }
         break;
      }
      case KERNINST_SYMTAB_STUFF_GET: 
      {
         /* see kernInstIoctls.h for a description of the "structure" used */

         uint32_t actual;
         struct kernInstSymtabGetStruct *destptr;
           
         struct kernInstSymtabGetStruct getstruct;
         if (copyin((void*)arg, /* src in user space */
		    (void*)&getstruct, /* dest in kernel space */
		    sizeof(getstruct)) < 0) {
	      retval = EFAULT;
	      break;
	  }

         actual = kerninst_symtab_get(getstruct.buffer,
                                      getstruct.buffer_size);

         destptr = (struct kernInstSymtabGetStruct*)arg;
           
         if (copyout((void*)&actual, /* src in kernel */
		     (void*)&destptr->actual_amt_copied, /*dest in user space*/
		     sizeof(destptr->actual_amt_copied)) < 0) {
	     retval = EFAULT;
	 }
           
         break;
      }

      case KERNINST_CALLONCE:
         kerninst_call_once();
         break;
      case KERNINST_CALLTWICE:
         kerninst_call_twice();
         break;
      case KERNINST_CALLTENTIMES:
         kerninst_call_tentimes();
         break;
         
      case KERNINST_WRITE1ALIGNEDWORDTOPHYSMEM: {
         struct kernInstWrite1Word input_args;

         if (copyin((void*)arg, /* src in user space */
		    (void*)&input_args, /* dest in kernel space */
		    sizeof(input_args)) < 0) {
	      retval = EFAULT;
	      break;
	 }
	 retval = write1AlignedWordToNucleus(input_args.destAddrInKernel,
					     input_args.val);
         if(retval != -1) {
            if(usp->give_debug_output)
               cmn_err(CE_CONT, "KERNINST_WRITE1ALIGNEDWORDTOPHYSMEM addr=0x%lx, val=0x%x\n",
                       input_args.destAddrInKernel, input_args.val);
         }
         break;
      }

      case KERNINST_WRUNDOABLE_NUCLEUS: {
         struct kernInstWrite1UndoableWord input_args;

         retval = copyin((void*)arg, /* src in user space */
                         (void*)&input_args, /* dest in kernel space */
                         sizeof(input_args));
         if (retval != -1)
            retval = write1UndoableAlignedWord(usp->global_loggedWrites,
                                               input_args.destAddrInKernel,
                                               1, /* maybe nucleus */
                                               input_args.newval,
                                               input_args.howSoon,
                                               &input_args.origval);
               /* can return 0 or 1 for success; -1 for failure */

         if (retval != -1) {
            if(usp->give_debug_output)
               cmn_err(CE_CONT, "KERNINST_WRUNDOABLE_NUCLEUS addr=0x%lx, newval=0x%x\n",
                       input_args.destAddrInKernel, input_args.newval);
            retval = copyout((void*)&input_args, /* source, in kernel */
                             (void*)arg, /* dest, in user space */
                             sizeof(input_args));
         }
         else
            retval = EFAULT;
         
         break;
      }
      
      case KERNINST_WRUNDOABLE_REG: {
         struct kernInstWrite1UndoableWord input_args;

         retval = copyin((void*)arg, /* src in user space */
                         (void*)&input_args, /* dest in kernel space */
                         sizeof(input_args));
         
         if (retval != -1) {
            retval = write1UndoableAlignedWord(usp->global_loggedWrites,
                                               input_args.destAddrInKernel,
                                               0, /* not nucleus */
                                               input_args.newval,
                                               input_args.howSoon,
                                               &input_args.origval);
            /* NOTE: can return 0 or 1 for success; -1 for failure */
         }
         
         if (retval != -1) {
            if(usp->give_debug_output)
               cmn_err(CE_CONT, "KERNINST_WRUNDOABLE_REG addr=0x%lx, newval=0x%x\n",
                       input_args.destAddrInKernel, input_args.newval);
            retval = copyout((void*)&input_args, /* source, in kernel */
                             (void*)arg, /* dest, in user space */
                             sizeof(input_args));
         }
         else {
            retval = EFAULT;
            cmn_err(CE_CONT, "retval -1 so making it EFAULT\n");
         }

         break;
      }

      case KERNINST_UNDO_WR_NUCLEUS: {
         struct kernInstUndo1Write input_args;
         retval = copyin((void*)arg, /* src in user space */
                         (void*)&input_args, /* dest in kernel space */
                         sizeof(input_args));
         if (retval != -1)
            retval = undoWriteWordNucleus(usp->global_loggedWrites,
                                          input_args.addr,
                                          input_args.oldval_to_set_to);

         if (retval != -1) {
            if(usp->give_debug_output)
               cmn_err(CE_CONT, "KERNINST_UNDO_WR_NUCLEUS addr=0x%lx, val=0x%x\n",
                       input_args.addr, input_args.oldval_to_set_to);
         }
         else
            retval = EFAULT;

         break;
      }
      
      case KERNINST_UNDO_WR_REG: {
         struct kernInstUndo1Write input_args;
         retval = copyin((void*)arg, /* src in user space */
                         (void*)&input_args, /* dest in kernel space */
                         sizeof(input_args));
         if (retval != -1)
            retval = undoWriteWordNonNucleus(usp->global_loggedWrites,
                                             input_args.addr,
                                             input_args.oldval_to_set_to);

         if (retval != -1) {
            if(usp->give_debug_output)
               cmn_err(CE_CONT, "KERNINST_UNDO_WR_REG addr=0x%lx, val=0x%x\n",
                       input_args.addr, input_args.oldval_to_set_to);
         }
         else
            retval = EFAULT;

         break;
      }
      
      case KERNINST_GET_TEXTNUCLEUS_BOUNDS:
	  cmn_err(CE_CONT, "Obsolete ioctl, use KERNINST_GET_ADDRESS_MAP"
		  " instead\n");
	  retval = EFAULT;
	  break;

      case KERNINST_GET_ADDRESS_MAP: {
         struct kernInstAddressMap address_map;
	 getKernInstAddressMap(&address_map);
         if (copyout(&address_map, (void*)arg, sizeof(address_map)) < 0) {
	     retval = EFAULT;
	 }
         
         break;
      }

      case KERNINST_CHG_UNDO_HOWSOON: {
         struct kernInstChangeUndoHowSoon info;
         retval = copyin((void*)arg, /* src in user space */
                         (void*)&info, /* dest in kernel space */
                         sizeof(info));
         if (retval != -1) {
            changeUndoWriteWordHowSoon(usp->global_loggedWrites,
                                       info.addrInKernel, info.newHowSoon);
         }

         if (retval == -1)
            retval = EFAULT;
      }
      
      case  KERNINST_GET_OFFSET_INFO: {
	  struct kernInstOffsetInfo koi;
	  kthread_t *dummy_thread = 0;
	  proc_t *dummy_proc = 0;
	  struct pid *dummy_pid = 0;

	  koi.t_cpu_offset = 
	      (char *)&dummy_thread->t_cpu - (char *)dummy_thread;
	  koi.t_procp_offset = 
	      (char *)&dummy_thread->t_procp - (char *)dummy_thread;
	  koi.p_pidp_offset = 
	      (char *)&dummy_proc->p_pidp - (char *)dummy_proc;
	  koi.pid_id_offset = 
	      (char*)&dummy_pid->pid_id - (char *)dummy_pid;

	  if (copyout((void *)&koi, (void *)arg, sizeof(koi)) < 0) {
	      retval = EFAULT;
	  }
	  break;
      }

      case KERNINST_GET_DISP_LEVEL: {
	  extern uint32_t get_disp_level();
	  uint32_t hi_pil = get_disp_level();
	  if (copyout(&hi_pil, (void *)arg, sizeof(uint32_t)) < 0) {
	      retval = EFAULT;
	  }
	  break;
      }

      case KERNINST_GET_CLOCK_FREQ: {
	  uint32_t freq = getSystemClockFreq();
	  if (copyout(&freq, (void *)arg, sizeof(uint32_t)) < 0) {
	      retval = EFAULT;
	  }
	  break;
      }

      case KERNINST_GET_CPU_IMPL_NUM: {
	  uint32_t impl = get_cpu_impl_num();
	  if (copyout(&impl, (void *)arg, sizeof(uint32_t)) < 0) {
	      retval = EFAULT;
	  }
	  break;
      }

      case KERNINST_KNOWN_CALLEES_SIZE: {
	  struct kernInstCallSiteInfo csi;
	  if (copyin((void*)arg, (void*)&csi, sizeof(csi)) < 0) {
	      retval = EFAULT;
	      break;
	  }
	  csi.num_callees = kerninst_num_callees_for_site(csi.addrFrom);
	  if (copyout(&csi, (void *)arg, sizeof(csi)) < 0)  {
	      retval = EFAULT;
	  }
	  
	  break;
      }

      case KERNINST_KNOWN_CALLEES_GET: {
	  struct kernInstCallSiteInfo csi;
	  unsigned num_callees;
	  unsigned bufsize;
	  kptr_t *callee_addrs;

	  if (copyin((void*)arg, (void*)&csi, sizeof(csi)) < 0) {
	      retval = EFAULT;
	      break;
	  }
	  num_callees = kerninst_num_callees_for_site(csi.addrFrom);
	  if (num_callees != csi.num_callees) {
	      cmn_err(CE_WARN, "Attempt to get callees for a watched site");
	      retval = EIO;
	      break;
	  }
	  if (num_callees == 0) {
	      retval = 0;
	      break;
	  }
	  bufsize = 2 * num_callees * sizeof(kptr_t);
	  callee_addrs = (kptr_t *)kmem_alloc(bufsize, KM_SLEEP);
	  if (callee_addrs == NULL) {
	      cmn_err(CE_WARN,
		      "KERNINST_KNOWN_CALLEES_GET: kmem_alloc(%u) error while"
		      " getting callees at 0x%lx", bufsize, csi.addrFrom);
	      retval = ENOMEM;
	      break;
	  }
	  kerninst_get_callees_for_site(csi.addrFrom, num_callees,
					callee_addrs);
	  if (copyout(callee_addrs, (void*)csi.buffer, bufsize) < 0) {
	      retval = EFAULT;
	  }

	  kmem_free(callee_addrs, bufsize);
	  
	  break;
      }

      case KERNINST_GET_ALLVTIMERS_ADDR: {
          kptr_t the_addr = (kptr_t)&usp->av_vtimers;
          if (copyout(&the_addr, (void *)arg, sizeof(kptr_t)) < 0) {
	      retval = EFAULT;
	  }
          break;
      }

      case KERNINST_ADD_TO_ALLVTIMERS: {
          kptr_t vtimer_addr;
          if (copyin((void*)arg, (void*)&vtimer_addr, sizeof(kptr_t)) < 0) {
	      retval = EFAULT;
	      break;
	  }
          if(usp->give_debug_output)
             cmn_err(CE_CONT, "KERNINST_ADD_TO_ALLVTIMERS vtimer_addr=0x%lx\n",
                    (unsigned long)vtimer_addr);
          retval = addto_all_vtimers(&usp->av_vtimers, (void*)vtimer_addr);
          break;
      }

      case KERNINST_REMOVE_FROM_ALLVTIMERS: {
          kptr_t vtimer_addr;
          if (copyin((void*)arg, (void*)&vtimer_addr, sizeof(kptr_t)) < 0) {
	      retval = EFAULT;
	      break;
	  }
          if(usp->give_debug_output)
             cmn_err(CE_CONT, "KERNINST_REMOVE_FROM_ALLVTIMERS vtimer_addr=0x%lx\n",
                    (unsigned long)vtimer_addr);
          retval = removefrom_all_vtimers(&usp->av_vtimers, (void*)vtimer_addr);
          break;
      }

      case KERNINST_CLEAR_CLIENT_STATE: {
          // clear the client structures and re-initialize
          uint32_t old_level = grab_cswitch_lock(&usp->kerninst_cswitch_lock);
          destroy_all_vtimers(&usp->av_vtimers);
          stacklist_destroy(&usp->sl_heap);
          hash_table_destroy(&usp->ht_vtimers); // does initialization too
          initialize_all_vtimers(&usp->av_vtimers);
          stacklist_initialize(&usp->sl_heap);
          release_cswitch_lock(old_level, &usp->kerninst_cswitch_lock);
          if(usp->give_debug_output) {
             cmn_err(CE_CONT, "KERNINST_CLEAR_CLIENT_STATE\n");
          }
          break;
      }

      case KERNINST_ENABLE_CSWITCH_HANDLING: {
          if(usp->cswitch_stuff_enabled) {
              usp->cswitch_stuff_enabled = 0;
              cmn_err(CE_CONT, "Disabled context-switch handling\n");
          }
          else {
              usp->cswitch_stuff_enabled = 1;
              cmn_err(CE_CONT, "Enabled context-switch handling\n");
          }
          break;
      }

      case KERNINST_SET_DEBUG_OUTPUT: {
          if(usp->give_debug_output)
              usp->give_debug_output = 0;
          else
              usp->give_debug_output = 1;
          cmn_err(CE_CONT, "KERNINST_SET_DEBUG_OUTPUT=%d\n", 
                  usp->give_debug_output);
          break;
      } 

      default:
         cmn_err(CE_CONT, "kerninst ioctl: unknown command!!!\n");
         retval = -1;
         break;
   }
	
   return (retval);
}

static int
kerninst_mmap(dev_t dev, off_t off, int prot)
{
  	int		lo_part;
	int		hi_part;
  	int		retval = -1;
	u_int		pfn;
	u_int		i;
	caddr_t		kva;
	int		instance = 0; /* better to do getminor(dev)? */
	hwprfunit_t*	usp;

	void *virtual_address;
	int page_frame_num;

	/* The purpose of mmap(9e) is simply to calculate the physical
	   page frame number of offset 'off' for device 'dev', while
	   also checking access permissions.

	   In particular, mmap(9e) doesn't actually map anything */

	usp = (hwprfunit_t *)ddi_get_soft_state(statep, instance);
	if (usp == NULL)
	   return -1;

	/* Check for a valid offset: */
	if (off < 0 || off > 1000) {
	   cmn_err(CE_CONT, "ari mmap: bad offset %lx so returning -1\n", off);
	   return -1;
	}

	/* Check access permissions. */
	if (prot & PROT_WRITE)
	   /* write access isn't allowed yet */
	   return -1;

	/* virtual_address = (void*)((unsigned)usp->ari_big_heap + off); */
/*  	virtual_address = &ari_global_buffer[off]; */

	page_frame_num = hat_getkpfnum(virtual_address);

	cmn_err(CE_CONT, "ari mmap: hat_getkpfnum for vaddr %lx returned %d\n",
		(uintptr_t)virtual_address, page_frame_num);

	return page_frame_num;
}

static int 
kerninst_segmap(dev_t dev, off_t off, struct as *asp, caddr_t *addrp,
                off_t len, unsigned int prot, unsigned int maxprot,
                unsigned int flags, cred_t *credp)
{
        int result;

	cmn_err(CE_CONT, "ari welcome to segmap\n");

        if ((prot != (PROT_USER | PROT_READ)) && 
		(prot != (PROT_USER | PROT_WRITE)) &&
		(prot != (PROT_USER | PROT_WRITE | PROT_READ))) {
			cmn_err(CE_NOTE, "kerninst_segmap: prot is 0x%x\n", prot);
                        return (EACCES);
        }

        result = ddi_segmap(dev, off, asp, addrp, len, prot, maxprot, flags, 
			    credp);
        return result;
}
