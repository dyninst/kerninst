/*
 * kerninst.h - declarations for functions & variable used by kerninst driver
 */

#include <linux/fs.h> // struct inode
#include <linux/module.h> // module macros
#include <linux/spinlock.h> // spinlock_t
#include <asm/ptrace.h> // struct pt_regs

#include "all_vtimers.h"
#include "hash_table.h"
#include "loggedAllocRequests.h"
#include "loggedWrites.h"
#include "stacklist.h"

// NOTE: This file should only be included by kerninst.c due to the
//       macro and static variable definitions

MODULE_AUTHOR ("Michael J. Brim, mjbrim@cs.wisc.edu");
MODULE_DESCRIPTION ("kerninst driver v2.1.1 for Linux");
MODULE_LICENSE ("Paradyn/Kerninst");

/*--------------- Module File Operations ---------------*/
int kerninst_ioctl(struct inode *, struct file *, u_int, u_long);
int kerninst_open(struct inode *, struct file *);
int kerninst_release(struct inode *, struct file *);

static struct file_operations kerninst_file_ops = {
  owner: THIS_MODULE,
  ioctl: kerninst_ioctl,
  open: kerninst_open,
  release: kerninst_release,
  /* all other fields null */
};

/*--------------- Module Global Variables ---------------*/
static int kerninst_major = 0;
static int in_use = 0;
static spinlock_t kerninst_use_lock = SPIN_LOCK_UNLOCKED;

/*--------------- Context-Switch Handling ---------------*/

/* spinlock for use in treating cswitch code as critical section */
spinlock_t kerninst_cswitch_lock = SPIN_LOCK_UNLOCKED;

/* vector of all vtimer addresses */
struct all_vtimers av_vtimers;

/* stacklist heap of vector-of-vtimers */
struct stacklist sl_heap;

/* hash table of thread/process to vtimer list */
struct hash_table ht_vtimers;

/* count of the number of times we fail to save a vtimer on
   context-switch out due to the stacklist being full */
unsigned number_unsaved_timers;

/*--------------- Undoable Memory Operations ---------------*/

/* maintained on a per-connection basis */
static loggedWrites *global_loggedWrites = NULL;
static loggedAllocRequests *global_loggedAllocRequests = NULL;

/*--------------- Miscellaneous ---------------*/
int kerninst_call_once(void);

uint32_t kerninst_debug_bits; /* bitfield to control debug output */
uint32_t kerninst_enable_bits; /* bitfield to enable/disable functionality */


#ifdef i386_unknown_linux2_4
/*--------------- Hardware Counter State ---------------*/
#include "hwcounter_p4.h"

perfctr_state_t reset_perfctr_state = {
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0}
};

/*--------------- Trap-Based Instrumentation ---------------*/
void (*do_int3)(struct pt_regs *, long);
void kerninst_int3_handler(struct pt_regs *, long);
void get_idt_base_addr(void);
void get_int3_handler(u_int);
int set_int3_handler(u_long);
u_long match_addr(u_long);
int install_trap_mapping(u_long, u_long);
int update_trap_mapping(u_long, u_long);
int uninstall_trap_mapping(u_long);

/* hash table of trap addr to patch addr mappings */
static struct hash_table ht_traps;

static int num_traps_installed = 0;
static u_long do_int3_addr = 0;
static struct desc_struct *idt_base = NULL;
static u_long int3_handler_addr = 0;
static unsigned char int3_handler_installed = 0;

#endif // i386_unknown_linux2_4

/*--------------- END OF KERNINST.H ---------------*/
