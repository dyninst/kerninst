INSTANTIATE_TEMPLATES = true
#EVEN_EXPENSIVE_ASSERTS = true
#USING_PURIFY = true
#APP_PURE_OPTIONS += 
#USES_GPROF = true
#NOASSERTS = true
#DISTRIBUTION = true

TO_CORE = ../../..

# Include standard make configuration stuff that applies to everything
# in the paradyn tree.

ifdef DISTRIBUTION
LIBDIR  += -L. -lstdc++
endif
# power test
#LIBDIR += -L/p/paradyn/kerninst/development/igor/kerninst/core/dyninstAPI/rs6000-ibm-aix4.2 -L$(TO_CORE)/util/rs6000-ibm-aix-4.2 -L$(TO_CORE)/core/pdutil/rs6000-ibm-aix4.2 /s/gcc-3.2.1/lib/gcc-lib/powerpc-ibm-aix5.1.0.0/3.2.1/libgcc.a

LIBDIR = /p/paradyn/kerninst/development/igor/arch-generic/core/dyninstAPI/rs6000-ibm-aix5.1/libdyninstAPI.a $(TO_CORE)/util/rs6000-ibm-aix5.1/libkerninstutil.a $(TO_CORE)/core/pdutil/rs6000-ibm-aix5.1/libpdutil.a  /s/gcc-3.2.1/lib/gcc-lib/powerpc-ibm-aix5.1.0.0/3.2.1/libgcc.a /s/gcc-3.2.1/lib/libstdc++.a

include $(TO_CORE)/make.config 

ifdef EVEN_EXPENSIVE_ASSERTS
CXXFLAGS += -D_DO_EVEN_EXPENSIVE_ASSERTS_
endif

# Now make any necessary architecture specific changes to variables:

OPTIMIZE = 
DEBUG_FLAG = -g
ifdef OPTIMIZE
DEBUG_FLAG = 
endif

# ultrapower flag is presently commented out because gcc 2.95 actually emits
# ultrapower instructions in that case, which binutils 2.9.1's assembler
# doesn't recognize
#ULTRASPARCFLAG = -mcpu=ultrapower

CXXFLAGS 	+=  $(STLFRIENDLYWARNINGS) -D_BIG_ENDIAN_ -Drs6000_ibm_aix5_1 -Drs6000_ibm_aix4_1 #-DOPTIMIZE
#LIBS		+= -ldyninstAPI -lgcc

ifdef NOASSERTS
CXXFLAGS	+= -DNDEBUG
endif

# Include the module-specific Makefile, which defines everything about
# the module that is common across architectures.

include $(TO_CORE)/kerninstd/emitters/make.module.tmpl

# Now make any necessary architecture specific changes to variables:

SRCS	+= $(TO_CORE)/kerninstd/emitters/src/power_tempBufferEmitter.C \
	   $(TO_CORE)/kerninstd/emitters/src/power_main.C \
	   $(TO_CORE)/kerninstd/insnVec/src/power_insnVec.C \
	   $(TO_CORE)/kerninstd/power_reg/src/power_reg.C \
	   $(TO_CORE)/kerninstd/power_reg_set/src/power_reg_set.C \
	   $(TO_CORE)/kerninstd/power_reg_set/src/power_misc_regset.C \
	   $(TO_CORE)/kerninstd/power_reg_set/src/power_int_regset.C \
	   $(TO_CORE)/kerninstd/power_reg_set/src/power_fp_regset.C \
	   $(TO_CORE)/kerninstd/power_instr2/src/getxform_test.C \
	   $(TO_CORE)/kerninstd/power_instr/src/power_instr.C 

VPATH	+= $(TO_CORE)/kerninstd/power_bb/src \
	   $(TO_CORE)/kerninstd/power_fn/src \
	   $(TO_CORE)/kerninstd/power_instr/src \
	   $(TO_CORE)/kerninstd/power_reg/src \
	   $(TO_CORE)/kerninstd/power_instr2/src \
	   $(TO_CORE)/kerninstd/power_reg_set/src

IFLAGS	+= -I$(TO_CORE)/kerninstd/power_bb/src \
	   -I$(TO_CORE)/kerninstd/power_fn/src \
	   -I$(TO_CORE)/kerninstd/power_instr/src \
	   -I$(TO_CORE)/kerninstd/power_reg/src \
	   -I$(TO_CORE)/kerninstd/power_reg_set/src \
	   -I$(TO_CORE)/core/dyninstAPI/src

LDFLAGS -=  -lpmapi
LDFLAGS += -Wl,-R -Wl,/s/gcc-3.2.1/lib/gcc-lib/powerpc-ibm-aix5.1.0.0/3.2.1 
#ASFLAGS += -xarch=v8plusa

# Include the "standard program template".  This defines all the
# common targets like "clean", "install", etc.

include $(TO_CORE)/make.program.tmpl
