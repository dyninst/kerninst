INSTANTIATE_TEMPLATES = true
#USING_PURIFY = true
#DISTRIBUTION = true
#USES_GPROF = true
#NOASSERTS = true

TO_CORE = ../../..
TO_KDRIVER = $(TO_CORE)/kerninstdriver-linux

USES_X11 = true
USES_TCLTK = true

# Include standard make configuration stuff that applies to everything
# in the paradyn tree.

include $(TO_CORE)/make.config 

# Now make any necessary architecture specific changes to variables:

#OPTIMIZE = 
DEBUG_FLAG = -g
ifdef OPTIMIZE
DEBUG_FLAG =
endif

CXXFLAGS 	+=  -D_KDRV32_
CXXFLAGS 	+=  $(STLFRIENDLYWARNINGS) $(OPTIMIZE) $(ULTRASPARCFLAG) -D_LITTLE_ENDIAN_
LIBS		+= 

ifdef NOASSERTS
CXXFLAGS	+= -DNDEBUG
endif

IFLAGS 		+= -I$(TO_KDRIVER)/src

# Include the module-specific Makefile, which defines everything about
# the module that is common across architectures.

include $(TO_CORE)/kperfmon/kperfmon/make.module.tmpl

# Include the "standard program template".  This defines all the
# common targets like "clean", "install", etc.

include $(TO_CORE)/make.program.tmpl
