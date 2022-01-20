#
# $Id: Makefile,v 1.2 2002/02/01 21:43:54 mirg Exp $
#

# Define any symbols needed to invoke configuration changes in make.config

TO_CORE	= ../..
INSTANTIATE_TEMPLATES	= true
#USES_GPROF		= true
#NOASSERTS		= true
#USING_PURIFY		= true

# Include standard make configuration stuff that applies to everything
# in the paradyn tree.

include $(TO_CORE)/make.config 

# Include the module-specific Makefile, which defines everything about
# the module that is common across architectures.

include ../make.module.tmpl

# Now make any necessary architecture specific changes to variables:

OPTIMIZE = -O2
DEBUG_FLAG = -g
ifdef OPTIMIZE
DEBUG_FLAG =
endif

ifdef USES_GPROF
CXXFLAGS += -pg
endif

ifdef NOASSERTS
CXXFLAGS += -DNDEBUG
endif

#ULTRASPARCFLAG = -mcpu=ultrasparc

CXXFLAGS += $(USEFULWARNINGS) $(ULTRASPARCFLAG) $(OPTIMIZE) $(DEBUG_FLAG)
CXXFLAGS += -DMATH_EXCEPTION_STRUCT=__exception

SRCS     += ../src/mrtime-linux.C

# Include the "standard library template".  This defines all the
# common targets like "clean", "install", etc.

include $(TO_CORE)/make.library.tmpl
