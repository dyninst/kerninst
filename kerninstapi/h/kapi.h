#ifndef _KAPI_H_
#define _KAPI_H_

#include <inttypes.h>
#ifdef ppc64_unknown_linux2_4
#include <set> 
#endif
#include "util/h/kdrvtypes.h"

typedef int ierr;
typedef uint16_t bbid_t;

#ifdef i386_unknown_linux2_4
typedef int32_t kapi_int_t;
#else
typedef int64_t kapi_int_t;
#endif

typedef uint64_t sample_t;

// Sampling callback
typedef int (*data_callback_t)(unsigned reqid, uint64_t time, 
			       const sample_t *values, unsigned numvalues);

class kapi_function;
class kapi_module;

enum kapi_dynload_event_t {
    kapi_module_added,
    kapi_module_removed,
    kapi_function_added,
    kapi_function_removed
};

// New code added/removed callback
typedef int (*dynload_callback_t)(kapi_dynload_event_t event,
				  const kapi_module *kmod,
				  const kapi_function *kfunc);


enum kapi_point_location {
    kapi_pre_instruction,
    kapi_pre_return,
    kapi_cswitch_out,
    kapi_cswitch_in,
    kapi_vrestart_routine, // For internal
    kapi_vstop_routine     // use only
};

class kapi_point {
 private:
    kptr_t address;
    kapi_point_location pt_type;
 public:
    kapi_point();
    kapi_point(kptr_t addr, kapi_point_location tp);
    kptr_t getAddress() const;
    kapi_point_location getType() const;
};

enum kapi_snippet_type {
    kapi_snippet_regular,
    kapi_snippet_vtimer_in,
    kapi_snippet_vtimer_out
};

namespace kcodegen {
    class AstNode;
}

class kapi_snippet {
 protected:
    kcodegen::AstNode *node;
    kapi_snippet_type type;
 public:
    kapi_snippet();
    kapi_snippet(const kapi_snippet &src);
    ~kapi_snippet();

    // We have to increment node's reference count when copying kapi_snippet
    kapi_snippet &operator=(const kapi_snippet &);

    kcodegen::AstNode *getAst() const;
    // Should-be-private routine for distinguishing regular snippets from
    // the hand-crafted context switch vtimer code
    kapi_snippet_type getType() const;
};

enum kapi_arith_operation {
    kapi_atomic_assign,
    kapi_assign,
    kapi_plus,
    kapi_minus,
    kapi_times,
    kapi_divide,
    kapi_bit_and,
    kapi_bit_or,
    kapi_shift_left,
    kapi_shift_right
};

enum kapi_unary_operation {
    kapi_deref
};

enum kapi_relation {
    kapi_lt,
    kapi_eq,
    kapi_gt,
    kapi_le,
    kapi_ne,
    kapi_ge,
    kapi_log_and,
    kapi_log_or
};

template <class T> class vec_stdalloc;
template<class T, class A> class pdvector;

template<class T> class kapi_vector {
    pdvector<T, vec_stdalloc<T> > *data;
 public:
    typedef const T* const_iterator;
    typedef T* iterator;
    typedef const T& const_reference;
    typedef T& reference;

    kapi_vector();
    kapi_vector(const kapi_vector<T> &src);
    ~kapi_vector();
    void clear();
    void push_back(const T &item);
    unsigned size() const;
    // Const iterators
    const_iterator begin() const;
    const_iterator end() const;
    const_reference operator[] (unsigned i) const;
    // Non-const iterators
    iterator begin();
    iterator end();
    reference operator[] (unsigned i);
};

class kapi_sequence_expr : public kapi_snippet {
 public:
    kapi_sequence_expr(const kapi_vector<kapi_snippet> &exprs);
};

class kapi_arith_expr : public kapi_snippet {
 public:
    kapi_arith_expr(kapi_arith_operation kind, 
		    const kapi_snippet &lOpd, const kapi_snippet &rOpd);
    kapi_arith_expr(kapi_unary_operation kind, const kapi_snippet &opd);
};

class kapi_const_expr : public kapi_snippet {
 public:
    kapi_const_expr(kapi_int_t val);
};

class kapi_variable_expr : public kapi_snippet {
};

class kapi_int_variable : public kapi_variable_expr {
 public:
    // Scalar int variable stored at the given address
    kapi_int_variable(kptr_t addr);
    // Declare a scalar variable that corresponds to v[index]
    kapi_int_variable(kptr_t v, const kapi_snippet &index);
    // The most general case: scalar int variable stored at the
    // address specified by the given expression
    kapi_int_variable(const kapi_snippet &addr_expr);
};

#ifdef i386_unknown_linux2_4
class kapi_int64_variable : public kapi_variable_expr {
 public:
    // Scalar int variable stored at the given address
    kapi_int64_variable(kptr_t addr);
    // Declare a scalar variable that corresponds to v[index]
    kapi_int64_variable(kptr_t v, const kapi_snippet &index);
    // The most general case: scalar int variable stored at the
    // address specified by the given expression
    kapi_int64_variable(const kapi_snippet &addr_expr);

};
#else
typedef kapi_int_variable kapi_int64_variable;
#endif

// Declare a vector of count ints
class kapi_int_vector : public kapi_variable_expr {
 private:
    kptr_t addr;
    unsigned count;
 public:
    kapi_int_vector(kptr_t iaddr, unsigned icount);
    kptr_t getBaseAddress() const;
};

class kapi_label_expr : public kapi_snippet {
    char *name;
 public:
    kapi_label_expr(char *label);
    ~kapi_label_expr();
    const char *getName() const;
};

class kapi_goto_expr : public kapi_snippet {
 public:
    kapi_goto_expr(const kapi_label_expr &label);
};

class kapi_bool_expr : public kapi_snippet {
 public:
    kapi_bool_expr(bool value); // constant expression
    kapi_bool_expr(kapi_relation kind, const kapi_snippet &lOpd, 
		   const kapi_snippet &rOpd);
};

class kapi_null_expr : public kapi_snippet {
 public:
    kapi_null_expr();
};

class kapi_if_expr : public kapi_snippet {
 public:
    // if-then
    kapi_if_expr(const kapi_bool_expr &cond, 
		 const kapi_snippet &trueClause);
    // if-then-else
    kapi_if_expr(const kapi_bool_expr &cond, 
		 const kapi_snippet &trueClause, 
		 const kapi_snippet &falseClause);
};

class kapi_cpuid_expr : public kapi_snippet {
 public:
    kapi_cpuid_expr();
};

// Expression which evaluates to the process id of an invoking process
class kapi_pid_expr : public kapi_snippet {
 public:
    kapi_pid_expr();
};

#ifdef sparc_sun_solaris2_7

// Expression which evaluates to the current interrupt priority level (PIL)
class kapi_getpil_expr : public kapi_snippet {
 public:
    kapi_getpil_expr();
};

// Expression useful for its side effects: sets PIL to the given value
class kapi_setpil_expr : public kapi_snippet {
 public:
    kapi_setpil_expr(const kapi_snippet &value);
};

#elif defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4)

// Expression which clears the maskable interrupt flag
class kapi_disableint_expr : public kapi_snippet {
 public:
    kapi_disableint_expr();
};

// Expression which sets the maskable interrupt flag
class kapi_enableint_expr : public kapi_snippet {
 public:
    kapi_enableint_expr();
};

#endif

class kapi_param_expr : public kapi_snippet {
 public:
    kapi_param_expr(unsigned n);
    // Be aware that even if a function has a byte/short/int-sized argument
    // we will still use the whole kapi_int_t for it. It will
    // become a problem when accessing parameters 7 and above which are
    // stored on the stack, so we may load more data than there is. Make
    // sure that you truncate such a parameter before using it 
    // (e.g. "and 0xff" to get a byte-sized argument).
};


// Expression that grabs the return value from a given function when inserted
// at its exit point
class kapi_retval_expr : public kapi_snippet {
 public:
    kapi_retval_expr(const kapi_function &func);
};

// Generates the call to the function starting at entryAddr and passes
// given parameters to it
class kapi_call_expr : public kapi_snippet {
 public:
    kapi_call_expr(kptr_t entryAddr, const kapi_vector<kapi_snippet> &args);
};

// Generates a return instruction. On SPARC that would be retl
class kapi_ret_expr : public kapi_snippet {
 public:
    kapi_ret_expr();
};

// Expression equal to the value of the register specified by regNum
class kapi_raw_register_expr : public kapi_snippet {
 public:
    kapi_raw_register_expr(int regNum);
};


typedef enum {
    HWC_NONE = 0,
    HWC_TICKS,
#ifdef sparc_sun_solaris2_7
    HWC_DCACHE_VREADS,
    HWC_DCACHE_VWRITES,
    HWC_DCACHE_VREAD_HITS,
    HWC_DCACHE_VWRITE_HITS,
    HWC_ICACHE_VREFS,
    HWC_ICACHE_VHITS,
    HWC_ICACHE_STALL_CYCLES,
    HWC_BRANCH_MISPRED_VSTALL_CYCLES,
    HWC_ECACHE_VREFS,
    HWC_ECACHE_VHITS,
    HWC_ECACHE_VREAD_HITS,
    HWC_VINSNS
#elif defined(i386_unknown_linux2_4)
    /* these are the non-retirement events */
    HWC_ITLB_HIT,
    HWC_ITLB_UNCACHEABLE_HIT,
    HWC_ITLB_MISS,
    HWC_ITLB_MISS_PAGE_WALK,
    HWC_DTLB_MISS_PAGE_WALK,
    HWC_L2_READ_HIT_SHR,
    HWC_L2_READ_HIT_EXCL,
    HWC_L2_READ_HIT_MOD,
    HWC_L2_READ_MISS,
    HWC_L3_READ_HIT_SHR,
    HWC_L3_READ_HIT_EXCL,
    HWC_L3_READ_HIT_MOD,
    HWC_L3_READ_MISS,
    HWC_COND_BRANCH_MISPREDICT,
    HWC_COND_BRANCH,
    HWC_CALL_MISPREDICT,
    HWC_CALL,
    HWC_RET_MISPREDICT,
    HWC_RET,
    HWC_INDIRECT_MISPREDICT,
    HWC_INDIRECT,
    /* these are the at-retirement events */
    HWC_MEM_LOAD,
    HWC_MEM_STORE,
    HWC_L1_LOAD_MISS,
    HWC_DTLB_LOAD_MISS,
    HWC_DTLB_STORE_MISS,
    HWC_INSTR_ISSUED,
    HWC_INSTR_RETIRED,
    HWC_UOPS_RETIRED,
    HWC_BRANCH_TAKEN_PREDICT,
    HWC_BRANCH_TAKEN_MISPREDICT,
    HWC_BRANCH_NOTTAKEN_PREDICT,
    HWC_BRANCH_NOTTAKEN_MISPREDICT,
    HWC_PIPELINE_CLEAR,
    HWC_MAX_EVENT // not really an event, just a setinel value
#elif defined(ppc64_unknown_linux2_4)
    HWC_RUN_CYCLES,
    HWC_PROCESSOR_CYCLES,
    HWC_INSTRUCTIONS_COMPLETED,
    HWC_L1_DATA_MISS,
    HWC_L2_DATA_INVALIDATION,
    HWC_INSTRUCTIONS_DISPATCHED,
    HWC_L1_DATA_STORE,
    HWC_L1_DATA_LOAD,
    HWC_L3_DATA_LOAD,
    HWC_DATA_MEMORY_LOAD,
    HWC_L3_DATA_LOAD_MCM,
    HWC_L2_DATA_LOAD,
    HWC_L2_DATA_LOAD_SHARED,
    HWC_L2_DATA_LOAD_MCM,
    HWC_L2_DATA_LOAD_MODIFIED,
    HWC_L2_DATA_LOAD_MODIFIED_MCM,
    HWC_L1_DATA_VALID,
    HWC_L2_DATA_VALID,
    HWC_MEMORY_INSTRUCTIONS_LOAD,
    HWC_L2_INSTRUCTIONS_LOAD,
    HWC_L2_INSTRUCTIONS_LOAD_MCM,
    HWC_L3_INSTRUCTIONS_LOAD,
    HWC_L3_INSTRUCTIONS_LOAD_MCM,
    HWC_L1_INSTRUCTIONS_LOAD,
    HWC_PREFETCHED_INSTRUCTIONS,
    HWC_NO_INSTRUCTIONS,
    HWC_MAX_EVENT
#endif
} kapi_hwcounter_kind;

class kapi_hwcounter_expr : public kapi_snippet {
 public:
    kapi_hwcounter_expr(kapi_hwcounter_kind type);
    void resetType(kapi_hwcounter_kind type);
};

class kapi_vtimer_cswitch_expr : public kapi_snippet {
    kptr_t all_vtimers_addr; // vector of all vtimers in the kernel
 public:
    kapi_vtimer_cswitch_expr(kapi_point_location ptype,
			     kptr_t all_vtimers);

    // Returns all_vtimers_addr
    kptr_t getAllVtimersAddr() const;

#ifdef sparc_sun_solaris2_7
    // vrestart routines expect the following expressions to
    // be available
    kapi_snippet getVRestartAddrExpr() const;
    kapi_snippet getAuxdataExpr() const;

    // vstop routines expect the following expressions to
    // be available
    kapi_snippet getVStopAddrExpr() const;
    kapi_snippet getStartExpr() const;
    kapi_snippet getIterExpr() const;
#endif
};

class function_t;
class basicblock_t;

class kapi_basic_block {
 private:
    const basicblock_t *theBasicBlock;
    const function_t *theFunction;

 public:
    kapi_basic_block();
    kapi_basic_block(const basicblock_t *block,
		     const function_t *func);

    // Return the starting address
    kptr_t getStartAddr() const;

    // Return the exit address of the block. The definition of this address is
    // a bit tricky due to the number of various ways a basic block
    // can end. But basically you need to insertSnippet at that address
    // to catch the exit from the block.
    kptr_t getExitAddr() const;

    // Return the address after the last address in the block
    kptr_t getEndAddrPlus1() const;
};

class kapi_function {
 private:
    const function_t *theFunction;

 public:
    enum error_codes {
	not_enough_space = -1,
	invalid_point_type = -2
    };

    kapi_function() : theFunction(0) {
    }

    kapi_function(const function_t *func);

    // Return the address of the entry point
    kptr_t getEntryAddr() const;

    // Fill-in the name of the function. Returns not_enough_space if
    // max_bytes is insufficient to store the name
    ierr getName(char *name, unsigned max_bytes) const;

    // Return the number of basic blocks in the function
    unsigned getNumBasicBlocks() const;

    // Find the basic block starting at addr and initialize *bb with it
    ierr findBasicBlock(kptr_t addr, kapi_basic_block *bb) const;

    // Find the basic block by its id and initialize *bb with it
    ierr findBasicBlockById(bbid_t bbid, kapi_basic_block *bb) const;

    // Retrieve bbid by the starting address. Return bbid_t(-1) if
    // not found
    bbid_t getBasicBlockIdByAddress(kptr_t addr) const;

    // Fill-in a vector of all basic blocks in the function
    ierr getAllBasicBlocks(kapi_vector<kapi_basic_block> *vec) const;

    // Fill-in a list of inst points in a function
    ierr findEntryPoint(kapi_vector<kapi_point> *points) const;
    ierr findExitPoints(kapi_vector<kapi_point> *points) const;

    // Some functions can not be parsed (and hence instrumented) at
    // this time
    bool isUnparsed() const;

    // There can be several names mapping to the same address
    unsigned getNumAliases() const;

    // Fill-in the name of the i-th alias
    ierr getAliasName(unsigned ialias, char *buffer, unsigned buflen) const;

    // Monster method: find and report all callees of this
    // function. Both regular calls and interprocedural branches are
    // located. If the blocks argument is not NULL, only calls made in these
    // blocks are reported.
    ierr getCallees(const kapi_vector<bbid_t> *blocks,
		    kapi_vector<kptr_t> *regCallsFromAddr,
		    kapi_vector<kptr_t> *regCallsToAddr,
		    kapi_vector<kptr_t> *interprocBranchesFromAddr,
		    kapi_vector<kptr_t> *interprocBranchesToAddr,
		    kapi_vector<kptr_t> *unanalyzableCallsFromAddr) const;
};

class loadedModule;
class kapi_manager;

class kapi_module {
 private:
    const loadedModule *theModule;
    kapi_manager *theManager;
 public:
    enum error_codes {
	not_enough_space = -1,
	function_not_found = -2,
	function_not_unique = -3
    };

    kapi_module() : theModule(0), theManager(0) {
    }

    kapi_module(const loadedModule *mod, kapi_manager *theManager);

    // Fill-in module's name
    ierr getName(char *name, unsigned max_len_bytes) const;

    // Fill-in module's description if any
    ierr getDescription(char *name, unsigned max_len_bytes) const;
    
    // Get the number of functions in the module
    unsigned getNumFunctions() const;

    // Find a function with this name and initialize *func
    ierr findFunction(const char *func_name, kapi_function *func) const;

    // Return a vector of all functions in the module
    ierr getAllFunctions(kapi_vector<kapi_function> *vec) const;
};
    
class instrumenter;

struct kapi_mem_region {
    kptr_t addr;
    unsigned nbytes;
};

template<class K, class V> class dictionary_hash;

class kapi_hwcounter_set {
    // Platform specific state
#ifdef sparc_sun_solaris2_7
    static const unsigned numCounters = 2;
    kapi_hwcounter_kind state[numCounters];
#elif defined(i386_unknown_linux2_4)
    static const unsigned numCounters = 18;
    typedef struct ctr_state {
       unsigned cccr_vals[numCounters];
       unsigned escr_vals[numCounters];
       unsigned pebs_msrs[2];
    } ctr_state_t;
    ctr_state_t state;
#elif ppc64_unknown_linux2_4
 public:
    typedef struct ppc64_event_group {
       unsigned group_num;
       uint64_t mmcr0;
       uint64_t mmcr1;
       uint64_t mmcra;
    } ppc64_event_group_t;
 private:
    static const unsigned numCounters = 8;
    static std::set<kapi_hwcounter_kind> active_metrics;
    ppc64_event_group_t state;
#else
    static const unsigned numCounters = 1;
#endif

 public:
    // Create an empty counter set
    kapi_hwcounter_set();
    
    // Insert a counter in the set. If its slot is already occupied,
    // force our counter in this slot. Return the old value of the slot
    kapi_hwcounter_kind insert(kapi_hwcounter_kind kind);

    // Check to see if the given counter can be enabled with no conflicts
    bool conflictsWith(kapi_hwcounter_kind kind) const;

    // Check to see if two sets are equal
    bool equals(const kapi_hwcounter_set &hwset) const;

#ifndef sparc_sun_solaris2_7
    // Find the index of a counter currently enabled for given counter kind
    unsigned findCounter(kapi_hwcounter_kind kind, bool must_find=true) const;
#endif

    // Free a slot occupied by the counter
    void free(kapi_hwcounter_kind kind);

    // Find what counters are enabled in the hardware
    ierr readConfig();

    // Do the actual programming of the counters
    ierr writeConfig();
};

// Disassembled instruction. Higher-level disassembly objects are collections
// of instructions
class kapi_disass_insn {
    void *raw;
    unsigned num_bytes;
    char *disassembly;
    bool has_dest_func;
    char *dest_func_info;
 public:
    kapi_disass_insn(const void *iraw, unsigned inum_bytes,
		     const char *idisassembly,
		     bool ihas_dest_func, const char *idest_func_info);
    kapi_disass_insn(const kapi_disass_insn &src);

    ~kapi_disass_insn();

    // Get the binary representation of the instruction
    const void *getRaw() const;

    // Get the size of the binary representation
    unsigned getNumBytes() const;

    // Get the textual representation of the instruction
    const char *getDisassembly() const;

    // True if insn is a call and we know its destination
    bool hasDestFunc() const;

    // The name of the function we are calling
    const char *getDestFuncInfo() const;
};

// The chunk is a contiguous region of code. Each function contains one
// or more chunks (we do support non-contiguous functions, thanks to the
// outliner for their existence :)
class kapi_disass_chunk {
    kptr_t start_addr;
    kapi_vector<kapi_disass_insn> rep;
 public:
    // Functions to construct/populate/destroy the chunk object
    kapi_disass_chunk(kptr_t istart_addr);
    void push_back(const kapi_disass_insn &kinsn);

    // Functions for users of the API
    kptr_t getStartAddr() const;
    typedef const kapi_disass_insn *const_iterator;
    const_iterator begin() const;
    const_iterator end() const;
};

// The toplevel object representing results of disassembly -- collection
// disassembled chunks.
class kapi_disass_object {
    kapi_vector<kapi_disass_chunk> rep;
    kapi_disass_object(const kapi_disass_object &src); // disallow
 public:
    // Functions to construct/populate/destroy the disass object
    kapi_disass_object();
    void push_back(const kapi_disass_chunk &kchunk);

    // Functions for users of the API
    typedef const kapi_disass_chunk *const_iterator;
    const_iterator begin() const;
    const_iterator end() const;
};


class stuffToSampleAndReport;
class memAllocator;

class kapi_manager {
 private:
    ierr err_code;
    memAllocator *memAlloc;
    dictionary_hash<unsigned, data_callback_t> *callbackMap;
    dictionary_hash<unsigned, unsigned> *samplingIntervals;
    uint64_t last_timeOfSample; // for rollback detection
    unsigned switchin_reqid;
    unsigned switchout_reqid;
    dynload_callback_t dynload_cb;
    dictionary_hash<kptr_t, unsigned> *calleeWatchers;

    dptr_t kernel_addr_to_kerninstd_addr(kptr_t addr);
    void convertRegionsIntoStuffToSample(kapi_mem_region *data, unsigned n,
					 stuffToSampleAndReport *stuff);
    void requestSampleSeveralValues(unsigned requestId, kapi_mem_region *data,
                                    unsigned n, unsigned ms);
 public:
    enum error_codes {
	no_error = 0,
	connection_failure = -1,
	module_not_found = -2,
	function_not_found = -3,
	function_not_unique = -4,
	invalid_point_type = -5,
	not_attached = -6,
	alloc_failed = -7,
	poke_failed = -7,
	not_enough_space = -8,
	function_not_analyzable = -9,
	internal_error = -10,
	not_indirect_call = -11,
	site_already_watched = -12,
	site_not_watched = -13,
	communication_error = -14,
        points_not_found = -15,
        unsupported_hwcounter = -16
    };

    enum blockPlacementPrefs {origSeq, chains};
    enum ThresholdChoices {AnyNonZeroBlock, FivePercent};

    kapi_manager();

    // Start working with the kernel on a specified machine 
    ierr attach(const char *machine, unsigned port);
    // Local version: no kerninstd, local machine
    ierr attach();
    // Detach from the kernel, remove all instrumentation, etc.
    ierr detach();

    // Kernel memory operations
    kptr_t malloc(unsigned size); // allocate
    ierr free(kptr_t addr); // free
    ierr meminit(kptr_t addr, int c, unsigned n); // initialize
    ierr memcopy(kptr_t from, void *to, unsigned n); // copy to the user space
    ierr memcopy(void *from, kptr_t to, unsigned n); // copy to the kernel

    // This routine byteswaps the given memory region if client's ordering
    // does not match that of the kernel
    void to_kernel_byte_order(void *data, unsigned size);

    // This routine byteswaps the given memory region if client's ordering
    // does not match that of the kernel
    void to_client_byte_order(void *data, unsigned size);

    // Copy memory from the kernel periodically. Return a request handle.
    // We assume that we are sampling a collection of 64-bit integers,
    // so the data is converted accordingly if the daemon and the client
    // run on different architectures
    int sample_periodically(kapi_mem_region *regions, unsigned n, // n regions
			    data_callback_t to, // callback on data arrival
			    unsigned ms); // sampling interval. If 
                                          // ms is zero then sample just once
    // Stop the sampling request given a handle
    ierr stop_sampling(int handle);

    // If new_ms == 0 -> stop sampling. If old_ms == 0, new_ms != 0 -> start
    // sampling. In all other cases -> change the sampling interval
    ierr adjust_sampling_interval(int handle, unsigned ms);

    // Return the total number of modules loaded in the kernel
    unsigned getNumModules() const;

    // Find a module with this name and initialize *mod
    ierr findModule(const char *mod_name, kapi_module *mod);

    // Create a vector of all modules in the kernel
    ierr getAllModules(kapi_vector<kapi_module> *vec);

    // Register a callback for getting notifications about
    // new code being loaded/removed. Not fully implemented yet.
    ierr registerDynloadCallback(dynload_callback_t cb);

    // Create instrumentation point at an arbitrary address
    ierr createInstPointAtAddr(kptr_t address, kapi_point *point);

    // Insert a snippet at the specified points and return its handle
    int insertSnippet(const kapi_snippet &snippet, const kapi_point &point);

    // Remove the previously inserted snippet
    ierr removeSnippet(int handle);

#ifdef sparc_sun_solaris2_7
    // Upload a snippet into the kernel, but do not splice it yet. It
    // can later be spliced at any of the given points. The points vector
    // may be empty (which will generate code that can be spliced
    // anywhere) at the expense of having less efficient instrumentation.
    int uploadSnippet(const kapi_snippet &snippet,
		      const kapi_vector<kapi_point> &points);

    // Find where the snippet has been uploaded
    kptr_t getUploadedAddress(unsigned handle);

    // Remote a previously-uploaded snippet
    ierr removeUploadedSnippet(unsigned handle);
#endif

    // Wrap a snippet with preemption protection code
    kapi_snippet getPreemptionProtectedCode(const kapi_snippet &origCode) const;

    // Manage in-kernel all_vtimers array
    kptr_t getAllVTimersAddr();
    void addTimerToAllVTimers(kptr_t);
    void removeTimerFromAllVTimers(kptr_t);

    // Wait for incoming events (callbacks and data samples).
    // timeoutMilliseconds specifies the maximum amount of time you want to
    // wait (set it to 0 if you do not want to block or to UINT_MAX to wait
    // forever). Return 1 if events are pending, 0 if timed out, <0 on error
    ierr waitForEvents(unsigned timeoutMilliseconds);
    
    // Alternative way of waiting for events: call getEventQueueDescriptor and
    // then select() on it (read and error descriptors). You must then
    // call handleEvents to process them
    int getEventQueueDescriptor();

    // Handle pending events
    ierr handleEvents();
    
    // Find the function starting at addr and initialize *func
    ierr findFunctionByAddress(kptr_t addr, kapi_function *func);

    // Find the module which has a function starting at addr
    ierr findModuleAndFunctionByAddress(kptr_t addr,
					kapi_module *mod, kapi_function *func);

    // Find instrumentation points that may be of interest to users: context
    // switch code, system call path, ... Presently, only the switch-in-out
    // points are implemented
    ierr findSpecialPoints(kapi_point_location type, 
			   kapi_vector<kapi_point> *points) const;

    // Collect callee addresses for a given callsite of an indirect call
    ierr watchCalleesAt(kptr_t addrFrom);

    // Stop collecting callee addresses for a given callsite
    ierr unWatchCalleesAt(kptr_t addrFrom);

    // Fill-in the supplied vectors with callee addresses and execution
    // counts collected so far
    ierr getCalleesForSite(kptr_t siteAddr, kapi_vector<kptr_t> *calleeAddrs,
			   kapi_vector<unsigned> *calleeCounts);

    // Replace the function starting at oldFnStartAddr with the one
    // starting at newFnStartAddr. Returns a request id to be used later 
    // to unreplace the function.
    int replaceFunction(kptr_t oldFnStartAddr,
                        kptr_t newFnStartAddr);

    // Undo the effect of replaceFunction request denoted by reqid
    ierr unreplaceFunction(unsigned reqid);

    // Replace the destination of the call at callSiteAddr with newDestAddr.
    // Returns a request id to be used later to unreplace the call.
    int replaceFunctionCall(kptr_t callSiteAddr,
                            kptr_t newFnStartAddr);

    // Undo the effect of replaceFunctionCall request denoted by reqid
    ierr unreplaceFunctionCall(unsigned reqid);

    // Toggle the testing flag. When this flag is set, we download the
    // instrumentation to the kernel, but do not splice it in
    ierr setTestingFlag(bool flag);

    // Get a printable string of register analysis info for a given
    // address. If beforeInsn is false, we include the effects of the
    // instruction at addr in the analysis. If globalAnalysis is false, the
    // results for this instruction alone are reported.
    ierr getRegAnalysisResults(kptr_t addr, bool beforeInsn,
			       bool globalAnalysis,
			       char *buffer, unsigned maxBytes);

    // Disassemble the specified function/bb and fill-in the disass object
    // If useOrigImage is true, we disassemble the original image (as if
    // no instrumentation took place)
    ierr getDisassObject(const kapi_function &kfunc, bbid_t bbid,
			 bool useOrigImage, kapi_disass_object *pdis);
    // Disassemble the specified range of addresses and fill-in the disass
    // object
    ierr getDisassObject(kptr_t start, kptr_t finish,
			 kapi_disass_object *kdis);

    ////////////
    // The rest of the methods are for internal use only. Some of them
    // are used only by kperfmon, some of them are for debugging. Use
    // only if you know what you are doing.
    ////////////

    // Meke the kernel invoke function kerninst_call_once in
    // the /dev/kerninst driver. Useful for debugging.
    ierr callOnce() const;

    // For internal use only: gets called when a data sample arrives.
    // Will trigger the appropriate callback
    void dispatchSampleData(const void *data);

    // For internal use only: gets called when new modules/functions get
    // loaded into the kernel.
    void dispatchDynloadEvent(kapi_dynload_event_t event,
			      const loadedModule *kmod,
			      const function_t *func);

    // Read the value of a hardware counter across all cpus
    ierr readHwcAllCPUs(kapi_hwcounter_kind kind, 
                        kapi_vector<sample_t> *cpu_samples);

    // Return the number of CPUs in the system being instrumented
    unsigned getNumCPUs() const;

    // Return the maximum physical id among CPUs
    unsigned getMaxCPUId() const;

    // Return the physical id of the i-th CPU (may be different from "i")
    unsigned getIthCPUId(unsigned i) const;

    // Return the frequency of the i-th CPU
    unsigned getCPUClockMHZ(unsigned icpu) const;

    // Return the system clock frequency (may not equal %tick frequency
    // if %stick is present)
    unsigned getSystemClockMHZ() const;

    // Return a time stamp from the machine being instrumented
    uint64_t getKerninstdMachineTime() const;

    // Return the <kernel abi, kerninstd abi> vector
    kapi_vector<unsigned> getABIs() const;

    // Return the priority interrupt level used for running the dispatcher (
    // the context-switch code)
    unsigned getDispatcherLevel() const;
    
#ifdef sparc_sun_solaris2_7
    // Convert a function into its outlined group id (UINT_MAX if none)
    unsigned parseOutlinedLevel(kptr_t fnEntryAddr) const;

    // The next fn converts from a possibly-outlined function addr to
    // the entryAddr of its original function. If the function has not been
    // outlined, simply returns its current entry address
    kptr_t getOutlinedFnOriginalFn(kptr_t fnEntryAddr) const;

    // The next fn is complex, so pay attention: it uses an addr
    // of some function to figure out what group that function belonged to,
    // if any.  With that info in mind, it then returns the addr of a
    // similarly-outlined function representing the second arg.
    kptr_t getSameOutlinedLevelFunctionAs(kptr_t guideFnOutlinedAddr,
					  kptr_t desiredFnOrigEntryAddr) const;

    void asyncOutlineGroup(
	bool doTheReplaceFunction,
	bool replaceFunctionPatchUpCallSitesToo,
	kapi_manager::blockPlacementPrefs,
	kapi_manager::ThresholdChoices,
	kptr_t rootFnAddr,
	const dictionary_hash<kptr_t, kapi_vector<unsigned> > &fnBlockCounters,
	const kapi_vector<kptr_t> &forceIncludeDescendants);

    void unOutlineGroup(kptr_t rootFnEntryAddr);

    void unOutlineAll();

    ierr testOutliningOneFn(
	const char *modname, kptr_t fnEntryAddr,
	const dictionary_hash<const char*,kptr_t> &iknownDownloadedCodeAddrs);

    ierr parseNewFn(const char *modName, const char *modDescriptionIfNew,
		    const char *fnName,
		    const kapi_vector<kptr_t> &chunkStarts,
		    const kapi_vector<unsigned> &chunkSizes,
		    unsigned entryNdx, unsigned dataNdx);

#elif (defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4))

    // Toggle the debug bits in the kerninst driver by XOR'ing the bitfield
    // with the supplied bitmask, which should set each bit to be toggled
    ierr toggleDebugBits(uint32_t bitmask) const;

    // Toggle the enable bits in the kerninst driver by XOR'ing the bitfield
    // with the supplied bitmask, which should set each bit to be toggled
    ierr toggleEnableBits(uint32_t bitmask) const;

#endif // platform-defines
};

#endif
