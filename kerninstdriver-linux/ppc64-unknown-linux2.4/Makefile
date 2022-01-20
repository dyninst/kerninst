SRCS            += ../src/hwcounter_power.c 
include ../make.module.tmpl

KERNEL_VERSION  = $(shell uname -r)
MODULES_INCLUDE = -I/lib/modules/$(KERNEL_VERSION)/build/include

KERNEL_DIR 	= /usr/src/linux-2.4
KERNEL_CONFIG	= $(shell ./get_kernel_config) 

include $(KERNEL_CONFIG)

MACHINE_DEFS	= -Dppc64_unknown_linux2_4
ifdef CONFIG_SMP
MACHINE_DEFS 	+= -D__SMP__ -DSMP
endif


# Hack to figure out if distribution is using O(1) scheduler patch
TASK_PROC       := $(shell ../get_task_proc $(CC))

# Hack to figure out how PACA is accessed
PACA_ACCESS_R13 = $(shell grep r13 /lib/modules/`uname -r`/build/include/asm/paca.h )
PACA_ACCESS_SPRG3 = $(shell grep SPRG3 /lib/modules/`uname -r`/build/include/asm/paca.h )

ifneq (${PACA_ACCESS_R13} ,)
PACA_ACCESS = -DPACA_ACCESS=0
endif
ifneq (${PACA_ACCESS_SPRG3} ,)
PACA_ACCESS = -DPACA_ACCESS=1
endif


ifdef CROSS_COMPILER_ONLY
CC = powerpc64-linux-gcc
LD = powerpc64-linux-ld
CFLAGS 		+= -O2  -mregnames $(MODULES_INCLUDE) $(MACHINE_DEFS) $(TASK_PROC) -msoft-float ${PACA_ACCESS} #-Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -fomit-frame-pointer  -fsigned-char -msoft-float -pipe -Wno-uninitialized -mminimal-toc -fno-builtin -mtraceback=full -DMODULE  -nostdinc -iwithprefix include
else
CFLAGS 		+= -O2 -m64 -mregnames $(MODULES_INCLUDE) $(MACHINE_DEFS) $(TASK_PROC) -msoft-float ${PACA_ACCESS} #-Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -fomit-frame-pointer  -fsigned-char -msoft-float -pipe -Wno-uninitialized -mminimal-toc -fno-builtin -mtraceback=full -DMODULE  -nostdinc -iwithprefix include
endif

LDFLAGS         += -m elf64ppc
TFLAGS 		+= $(MACHINE_DEFS)
all: 	$(MODULE)

veryclean:
	/bin/rm -f $(MODULE) *.o *~
