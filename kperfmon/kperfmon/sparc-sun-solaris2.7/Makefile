INSTANTIATE_TEMPLATES = true
#USING_PURIFY = true
#DISTRIBUTION = true
#USES_GPROF = true
#NOASSERTS = true

TO_CORE = ../../..
TO_KDRIVER = $(TO_CORE)/kerninstdriver

USES_X11 = true
USES_TCLTK = true

# Include standard make configuration stuff that applies to everything
# in the paradyn tree.

include $(TO_CORE)/make.config 

CXXFLAGS += -D_KDRV32_

# Now make any necessary architecture specific changes to variables:

#OPTIMIZE = 
DEBUG_FLAG = -g
ifdef OPTIMIZE
DEBUG_FLAG =
endif

#ULTRASPARCFLAG = -mcpu=ultrasparc
ASFLAGS		+= -xarch=v8plusa

CXXFLAGS 	+=  $(STLFRIENDLYWARNINGS) $(OPTIMIZE) $(ULTRASPARCFLAG) -D_BIG_ENDIAN_
LIBS		+= 

ifdef NOASSERTS
CXXFLAGS	+= -DNDEBUG
endif

SYSLIBS		+= -lsocket

IFLAGS		+= -I$(TO_KDRIVER)/src

# Include the module-specific Makefile, which defines everything about
# the module that is common across architectures.

include $(TO_CORE)/kperfmon/kperfmon/make.module.tmpl

# Now make any necessary architecture specific changes to variables:

# Include the "standard program template".  This defines all the
# common targets like "clean", "install", etc.

include $(TO_CORE)/make.program.tmpl
