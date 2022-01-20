INSTANTIATE_TEMPLATES = true
USES_X11 = true
USES_TCLTK = true
#USING_PURIFY = true
#DISTRIBUTION = true
#USES_GPROF = true
#NOASSERTS = true

TO_CORE = ../../..
TO_KDRIVER = $(TO_CORE)/kerninstdriver-linux

# Include standard make configuration stuff that applies to everything
# in the paradyn tree.

ifdef DISTRIBUTION
LDFLAGS	+= -L. -lstdc++
endif

include $(TO_CORE)/make.config 

# Now make any necessary architecture specific changes to variables:

#OPTIMIZE = 
DEBUG_FLAG = -g
ifdef OPTIMIZE
DEBUG_FLAG =
endif

CXXFLAGS 	+=  $(STLFRIENDLYWARNINGS) $(OPTIMIZE)  -Drs6000_ibm_aix5_1 -Dppc64_unknown_linux2_4 -D_KDRV64_
LIBS		+= -L/usr/X11R6/lib

ifdef NOASSERTS
CXXFLAGS	+= -DNDEBUG
endif

IFLAGS		+= -I$(TO_KDRIVER)/src

# Include the module-specific Makefile, which defines everything about
# the module that is common across architectures.

include $(TO_CORE)/kperfmon/kperfmon/make.module.tmpl

# Include the "standard program template".  This defines all the
# common targets like "clean", "install", etc.

include $(TO_CORE)/make.program.tmpl

#CC = powerpc64-linux-gcc
#CXX = powerpc64-linux-g++
#LD = powerpc64-linux-ld

