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
CXXFLAGS 	+=  $(STLFRIENDLYWARNINGS) -fguiding-decls $(DEBUG_FLAG)


SRCS	     += ../src/x86_templates.C \
		../src/x86_main.C \
		../../x86_reg/src/x86_reg.C \
		../../x86_reg_set/src/x86_reg_set.C \
		../../insnVec/src/x86_insnVec.C \
		../../x86_instr/src/x86_instr.C \
		../../x86_instr/src/x86_operand_code.C

IFLAGS       += -I../../x86_reg/src -I../../x86_reg_set/src -I../../x86_instr/src

VPATH        += ../../x86_reg/src ../../x86_reg_set/src ../../x86_instr/src

LIBS		+= -lelf -lnsl -lsocket -lkvm

# Include the module-specific Makefile, which defines everything about
# the module that is common across architectures.

include ../make.module.tmpl

# Now make any necessary architecture specific changes to variables:

# none to make

# Include the "standard program template".  This defines all the
# common targets like "clean", "install", etc.

include $(TO_CORE)/make.program.tmpl
