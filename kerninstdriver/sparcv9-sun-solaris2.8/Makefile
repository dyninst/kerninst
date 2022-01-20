# get the definitions common across all platforms
include ../Makefile.platform_common1

# update definitions for this platform
VPATH   += ../src/sparc-sun-solaris2.7 \
           ../src/sparc-sun-solaris2.8

SRCS	+= ../src/sparc-sun-solaris2.7/symbol_table_2.7.C \
	   ../src/sparc-sun-solaris2.7/mm_map_2.7.C \
	   ../src/sparc-sun-solaris2.8/nucleusTextAllocation_2.8.C \
	   ../src/sparc-sun-solaris2.8/kmem32Allocation_2.8.C \
	   ../src/sparc-sun-solaris2.8/belowAllocation_2.8.C \
	   ../src/ari_ops_v9.s

SRC = /p/src/solaris8/osnet_volume.orig/usr/src

ARCH = v9a
ARCHDEFS = -D_ELF64 -U_ELF32_COMPAT -D_KDRV64_ -m64 -mcpu=v9 -mvis

# get the rest of the rules that use the platform specifics
include ../Makefile.rules
