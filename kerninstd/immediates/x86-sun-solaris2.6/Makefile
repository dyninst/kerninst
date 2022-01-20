INSTANTIATE_TEMPLATES = true
TO_CORE = ../../..
USES_LIB_KERNINST_UTIL = true

# Include standard make configuration stuff that applies to everything
# in the paradyn tree.

include $(TO_CORE)/make.config 

# Now make any necessary architecture specific changes to variables:

CXXFLAGS 	+=  $(STLFRIENDLYWARNINGS) -fguiding-decls
CXXFLAGS 	+=  -g

LIBS		+= 

# Include the module-specific Makefile, which defines everything about
# the module that is common across architectures.

include ../make.module.tmpl

# Now make any necessary architecture specific changes to variables:

# none to make

# Include the "standard program template".  This defines all the
# common targets like "clean", "install", etc.

include $(TO_CORE)/make.program.tmpl
