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

# Build for the 32-bit kernels
CXXFLAGS += -D_KDRV32_

# Now make any necessary architecture specific changes to variables:

#OPTIMIZE = 
DEBUG_FLAG = -g
ifdef OPTIMIZE
DEBUG_FLAG =
endif

CXXFLAGS 	+=  $(STLFRIENDLYWARNINGS) $(OPTIMIZE) -D_LITTLE_ENDIAN_
LIBS		+= 

ifdef NOASSERTS
CXXFLAGS	+= -DNDEBUG
endif

# Include the module-specific Makefile, which defines everything about
# the module that is common across architectures.

include $(TO_CORE)/kerninstapi/make.module.tmpl

# Now make any necessary architecture specific changes to variables:
SRCS += \
	$(TO_CORE)/kerninstd/basicblock/src/x86_basicblock.C \
	$(TO_CORE)/kerninstd/basicblock/src/x86_bb.C \
	$(TO_CORE)/kerninstd/bitwise_dataflow_fn/src/x86_mbdf_window.C \
	$(TO_CORE)/kerninstd/bitwise_dataflow_fn/src/x86_bitwise_dataflow_fn.C \
	$(TO_CORE)/kerninstd/function/src/x86_funkshun.C \
	$(TO_CORE)/kerninstd/insnVec/src/x86_insnVec.C \
	$(TO_CORE)/kerninstd/relocatableCode/src/x86_relocatableCode.C \
	$(TO_CORE)/kerninstd/emitters/src/x86_tempBufferEmitter.C \
	$(TO_CORE)/kerninstd/x86_instr/src/x86_instr.C \
	$(TO_CORE)/kerninstd/x86_instr/src/x86_parse.C \
	$(TO_CORE)/kerninstd/x86_reg/src/x86_reg.C \
	$(TO_CORE)/kerninstd/x86_reg_set/src/x86_reg_set.C \
	$(TO_KAPI)/src/kapi_hwcounter_set_x86.C \
	$(TO_KAPI)/codegen/src/emit-x86.C

VPATH += $(TO_CORE)/kerninstd/x86_instr/src \
	 $(TO_CORE)/kerninstd/x86_reg/src \
	 $(TO_CORE)/kerninstd/x86_reg_set/src

IFLAGS += -I$(TO_CORE)/kerninstd/x86_instr/src \
	  -I$(TO_CORE)/kerninstd/x86_reg/src \
	  -I$(TO_CORE)/kerninstd/x86_reg_set/src

# Include the "standard program template".  This defines all the
# common targets like "clean", "install", etc.

include $(TO_CORE)/make.library.tmpl
