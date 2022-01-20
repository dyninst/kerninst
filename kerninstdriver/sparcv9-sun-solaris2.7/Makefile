# get the definitions common across all platforms
include ../Makefile.platform_common1

# update definitions for this platform
VPATH   += ../src/sparc-sun-solaris2.7

SRCS	+= ../src/sparc-sun-solaris2.7/symbol_table_2.7.C \
           ../src/sparc-sun-solaris2.7/mm_map_2.7.C \
           ../src/sparc-sun-solaris2.7/nucleusTextAllocation_2.7.C \
	   ../src/sparc-sun-solaris2.7/belowAllocation_2.7.C \
	   ../src/sparc-sun-solaris2.7/kmem32Allocation_sparcv9_2.7.C \
           ../src/ari_ops_v9.o

SRC = /p/src/solaris7/sparc/os_net/src_ws/usr/src

ARCH = v9a
ARCHDEFS = -D_ELF64 -U_ELF32_COMPAT -D_KDRV64_ -m64 -mcpu=v9 -mvis

# get the rest of the rules that use the platform specifics
include ../Makefile.rules
