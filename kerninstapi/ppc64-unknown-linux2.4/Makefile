INSTANTIATE_TEMPLATES = true
USES_X11 = true
USES_TCLTK = true
#USING_PURIFY = true
#DISTRIBUTION = true
#USES_GPROF = true
#NOASSERTS = true

TO_CORE = ../..



# Include standard make configuration stuff that applies to everything
# in the paradyn tree.

include $(TO_CORE)/make.config 

# Build for the 64-bit kernels
CXXFLAGS += -D_KDRV64_ -Dppc64_unknown_linux2_4 -Drs6000_ibm_aix5_1

#CC = powerpc64-linux-gcc
#CXX = powerpc64-linux-g++
#LD = powerpc64-linux-ld
# Now make any necessary architecture specific changes to variables:

#OPTIMIZE = 
DEBUG_FLAG = -g
ifdef OPTIMIZE
DEBUG_FLAG =
endif

CXXFLAGS 	+=  $(STLFRIENDLYWARNINGS) $(OPTIMIZE) 
LIBS		+= 

ifdef NOASSERTS
CXXFLAGS	+= -DNDEBUG
endif

# Include the module-specific Makefile, which defines everything about
# the module that is common across architectures.

include $(TO_CORE)/kerninstapi/make.module.tmpl

# Now make any necessary architecture specific changes to variables:
SRCS += \
	$(TO_CORE)/kerninstd/basicblock/src/power_basicblock.C \
	$(TO_CORE)/kerninstd/basicblock/src/power_bb.C \
	$(TO_CORE)/kerninstd/bitwise_dataflow_fn/src/power_bitwise_dataflow_fn.C \
	$(TO_CORE)/kerninstd/function/src/power_funkshun.C \
	$(TO_CORE)/kerninstd/insnVec/src/power_insnVec.C \
	$(TO_CORE)/kerninstd/relocatableCode/src/power_relocatableCode.C \
	$(TO_CORE)/kerninstd/emitters/src/power_tempBufferEmitter.C \
	$(TO_CORE)/kerninstd/power_instr/src/power_instr.C \
	$(TO_CORE)/kerninstd/power_reg/src/power_reg.C \
	$(TO_CORE)/kerninstd/power_reg_set/src/power_reg_set.C \
	$(TO_CORE)/kerninstd/power_reg_set/src/power_fp_regset.C \
	$(TO_CORE)/kerninstd/power_reg_set/src/power_misc_regset.C \
	$(TO_CORE)/kerninstd/power_reg_set/src/power_int_regset.C \
	$(TO_KAPI)/codegen/src/emit-power.C \
	$(TO_KAPI)/src/kapi_hwcounter_set_power4.C

VPATH += $(TO_CORE)/kerninstd/power_instr/src \
	 $(TO_CORE)/kerninstd/power_reg/src \
	  -I$(TO_CORE)/kerninstdriver-linux/src \
	 $(TO_CORE)/kerninstd/power_reg_set/src

IFLAGS += -I$(TO_CORE)/kerninstd/power_instr/src \
	  -I$(TO_CORE)/kerninstd/power_reg/src \
	  -I$(TO_CORE)/kerninstdriver-linux/src \
	  -I$(TO_CORE)/kerninstd/power_reg_set/src

# Include the "standard program template".  This defines all the
# common targets like "clean", "install", etc.

include $(TO_CORE)/make.library.tmpl


