INSTANTIATE_TEMPLATES = true
#EVEN_EXPENSIVE_ASSERTS = true
#USING_PURIFY = true
#APP_PURE_OPTIONS += 
#USES_GPROF = true
#NOASSERTS = true
#DISTRIBUTION = true
USES_ELF = true

TO_CORE = ../../..

TO_KERNINST_DRIVER = $(TO_CORE)/kerninstdriver-linux/src
TO_KERNINSTD = $(TO_CORE)/kerninstd
KERNEL_VERSION  = $(shell uname -r)
KERNEL_INCLUDE = /lib/modules/$(KERNEL_VERSION)/build/include

# Include standard make configuration stuff that applies to everything
# in the paradyn tree.

ifdef DISTRIBUTION
LIBDIR  += -L. -lstdc++
endif

include $(TO_CORE)/make.config 

ifdef EVEN_EXPENSIVE_ASSERTS
CXXFLAGS += -D_DO_EVEN_EXPENSIVE_ASSERTS_ 
endif
CXXFLAGS += -D_KDRV64_
# Now make any necessary architecture specific changes to variables:
#CC = powerpc64-linux-gcc
#CXX = powerpc64-linux-g++
#LD = powerpc64-linux-ld

OPTIMIZE = 
DEBUG_FLAG = -g
ifdef OPTIMIZE
DEBUG_FLAG = 
endif

CXXFLAGS 	+=  $(STLFRIENDLYWARNINGS) $(OPTIMIZE) -Drs6000_ibm_aix5_1 -Dppc64_unknown_linux2_4
LIBS		+=  

ifdef NOASSERTS
CXXFLAGS	+= -DNDEBUG
endif

# Include the module-specific Makefile, which defines everything about
# the module that is common across architectures.

include $(TO_CORE)/kerninstd/kerninstd/make.module.tmpl

# Now make any necessary architecture specific changes to variables:

SRCS	+= $(TO_KERNINSTD)/basicblock/src/power_basicblock.C \
	   $(TO_KERNINSTD)/basicblock/src/power_bb.C \
	   $(TO_KERNINSTD)/bitwise_dataflow_fn/src/power_bitwise_dataflow_fn.C \
	   $(TO_KERNINSTD)/launchSite/src/power_callLauncher.C \
	   $(TO_KERNINSTD)/LRAndR/src/power_LRAndR.C \
	   $(TO_KERNINSTD)/LRAndR/src/power_branchAlways_LRAndR.C \
	   $(TO_KERNINSTD)/LRAndR/src/power_condBranch_LRAndR.C \
	   $(TO_KERNINSTD)/LRAndR/src/power_nonDCTI_LRAndR.C \
	   $(TO_KERNINSTD)/LRAndR/src/power_regBranch_LRAndR.C \
	   $(TO_KERNINSTD)/emitters/src/power_tempBufferEmitter.C \
	   $(TO_KERNINSTD)/emitters/src/power_directToMemoryEmitter.C \
	   $(TO_KERNINSTD)/function/src/power_funkshun.C \
	   $(TO_KERNINSTD)/function/src/power_cfgCreationOracle.C \
	   $(TO_KERNINSTD)/insnVec/src/power_insnVec.C \
	   $(TO_KERNINSTD)/kmem/src/power_kmem.C \
	   $(TO_KERNINSTD)/launchSite/src/power_launcher.C \
	   $(TO_KERNINSTD)/power_instr/src/power_instr.C \
	   $(TO_KERNINSTD)/launchSite/src/power_QuickBranchLauncher.C \
	   $(TO_KERNINSTD)/power_reg/src/power_reg.C \
	   $(TO_KERNINSTD)/power_reg_set/src/power_reg_set.C \
	   $(TO_KERNINSTD)/power_reg_set/src/power_misc_regset.C \
	   $(TO_KERNINSTD)/power_reg_set/src/power_fp_regset.C \
	   $(TO_KERNINSTD)/power_reg_set/src/power_int_regset.C \
	   $(TO_KERNINSTD)/relocatableCode/src/power_relocatableCode.C \
	   $(TO_KERNINSTD)/kerninstd/src/gatherMod-linux.C \
	   $(TO_KERNINSTD)/launchSite/src/power_CondRegClearLauncher.C \
#	   $(TO_KERNINSTD)/launchSite/src/power_TrapLauncher.C \

SRCS -= $(TO_KERNINSTD)replaceFunction/src/replaceFunction.C
VPATH	+= $(TO_KERNINST_DRIVER) \
	   $(TO_KERNINSTD)/power_instr/src \
	   $(TO_KERNINSTD)/power_reg/src \
	   $(TO_KERNINSTD)/power_reg_set/src

IFLAGS	+= -I$(TO_KERNINST_DRIVER) \
	   -I$(TO_KERNINSTD)/power_instr/src \
	   -I$(TO_KERNINSTD)/power_reg/src \
	   -I$(TO_KERNINSTD)/power_reg_set/src \
           -I$(KERNEL_INCLUDE) \
           -I/usr/include/libelf

# Include the "standard program template".  This defines all the
# common targets like "clean", "install", etc.
include $(TO_CORE)/make.program.tmpl
