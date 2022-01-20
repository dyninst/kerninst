INSTANTIATE_TEMPLATES = true
#EVEN_EXPENSIVE_ASSERTS = true
#USING_PURIFY = true
#APP_PURE_OPTIONS += 
#USES_GPROF = true
#NOASSERTS = true
#DISTRIBUTION = true

TO_CORE = ../../..

TO_KERNINST_DRIVER = $(TO_CORE)/kerninstdriver-linux/src
TO_KERNINSTD = $(TO_CORE)/kerninstd

USES_ELF = true

# Include standard make configuration stuff that applies to everything
# in the paradyn tree.

include $(TO_CORE)/make.config 

ifdef EVEN_EXPENSIVE_ASSERTS
CXXFLAGS += -D_DO_EVEN_EXPENSIVE_ASSERTS_
endif

KERNEL_VERSION  = $(shell uname -r)
KERNEL_CONFIG	= /boot/config-$(KERNEL_VERSION)

include $(KERNEL_CONFIG)
ifdef CONFIG_SMP
CXXFLAGS 	+= -D_SMP_
endif

# Now make any necessary architecture specific changes to variables:

OPTIMIZE = 
DEBUG_FLAG = -g
ifdef OPTIMIZE
DEBUG_FLAG = 
endif

CXXFLAGS 	+=  $(STLFRIENDLYWARNINGS) $(OPTIMIZE)
CXXFLAGS        += -D_KDRV32_
LIBS		+= 

ifdef NOASSERTS
CXXFLAGS	+= -DNDEBUG
endif

# Include the module-specific Makefile, which defines everything about
# the module that is common across architectures.

include $(TO_CORE)/kerninstd/kerninstd/make.module.tmpl

# Now make any necessary architecture specific changes to variables:

SRCS	+= $(TO_KERNINSTD)/basicblock/src/x86_basicblock.C \
	   $(TO_KERNINSTD)/basicblock/src/x86_bb.C \
	   $(TO_KERNINSTD)/bitwise_dataflow_fn/src/x86_bitwise_dataflow_fn.C \
	   $(TO_KERNINSTD)/bitwise_dataflow_fn/src/x86_mbdf_window.C \
	   $(TO_KERNINSTD)/emitters/src/x86_tempBufferEmitter.C \
	   $(TO_KERNINSTD)/emitters/src/x86_directToMemoryEmitter.C \
	   $(TO_KERNINSTD)/function/src/x86_funkshun.C \
	   $(TO_KERNINSTD)/function/src/x86_cfgCreationOracle.C \
	   $(TO_KERNINSTD)/insnVec/src/x86_insnVec.C \
	   $(TO_KERNINSTD)/kerninstd/src/gatherMod-linux.C \
	   $(TO_KERNINSTD)/kmem/src/x86_kmem.C \
	   $(TO_KERNINSTD)/launchSite/src/x86_launcher.C \
	   $(TO_KERNINSTD)/launchSite/src/x86_callLauncher.C \
	   $(TO_KERNINSTD)/launchSite/src/x86_QuickBranchLauncher.C \
	   $(TO_KERNINSTD)/launchSite/src/x86_TrapLauncher.C \
	   $(TO_KERNINSTD)/relocatableCode/src/x86_relocatableCode.C \
	   $(TO_KERNINSTD)/x86_instr/src/x86_instr.C \
	   $(TO_KERNINSTD)/x86_instr/src/x86_parse.C \
	   $(TO_KERNINSTD)/x86_reg/src/x86_reg.C \
	   $(TO_KERNINSTD)/x86_reg_set/src/x86_reg_set.C \
	   $(TO_KERNINSTD)/LRAndR/src/x86_LRAndR.C \
	   $(TO_KERNINSTD)/LRAndR/src/x86_branch_LRAndR.C

VPATH	+= $(TO_KERNINST_DRIVER) \
	   $(TO_KERNINSTD)/x86_instr/src \
	   $(TO_KERNINSTD)/x86_reg/src \
	   $(TO_KERNINSTD)/x86_reg_set/src

IFLAGS	+= -I$(TO_KERNINST_DRIVER) \
	   -I$(TO_KERNINSTD)/x86_instr/src \
	   -I$(TO_KERNINSTD)/x86_reg/src \
	   -I$(TO_KERNINSTD)/x86_reg_set/src

# Include the "standard program template".  This defines all the
# common targets like "clean", "install", etc.

include $(TO_CORE)/make.program.tmpl
