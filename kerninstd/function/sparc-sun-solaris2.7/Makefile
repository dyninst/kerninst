#NO_IMPLICIT_TEMPLATES = true
INSTANTIATE_TEMPLATES = true
TO_CORE = ../../..
#RUNTEST_OTHER_OPTIONS += -v -v

# Include standard make configuration stuff that applies to everything
# in the paradyn tree.

include $(TO_CORE)/make.config 

# Now make any necessary architecture specific changes to variables:
DEBUG_FLAG	= -g
#DEBUG_FLAG	= 
CXXFLAGS 	+=  $(STLFRIENDLYWARNINGS) -fguiding-decls $(DEBUG_FLAG) -D_BIG_ENDIAN_


SRCS	     += ../src/sparc_templates.C \
		../src/sparc_main.C \
		../../sparc_reg/src/sparc_reg.C \
		../../sparc_reg_set/src/sparc_reg_set.C \
		../../sparc_instr/src/sparc_instr.C

IFLAGS       += -I../../sparc_reg/src -I../../sparc_reg_set/src -I../../sparc_instr/src

VPATH        += ../../sparc_reg/src ../../sparc_reg_set/src ../../sparc_instr/src

LIBS		+= -lelf -lnsl -lsocket -lkvm

# Include the module-specific Makefile, which defines everything about
# the module that is common across architectures.

include ../make.module.tmpl

# Now make any necessary architecture specific changes to variables:

# none to make

# Include the "standard program template".  This defines all the
# common targets like "clean", "install", etc.

include $(TO_CORE)/make.program.tmpl
