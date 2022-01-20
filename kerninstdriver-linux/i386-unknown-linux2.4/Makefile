KERNEL_VERSION  = $(shell uname -r)
MODULES_INCLUDE = /lib/modules/$(KERNEL_VERSION)/build/include

KERNEL_DIR 	= /usr/src/linux-2.4
KERNEL_CONFIG	= /boot/config-$(KERNEL_VERSION)

include $(KERNEL_CONFIG)

SRCS		+= ../src/hwcounter_p4.c

include ../make.module.tmpl

MACHINE_DEFS	+= -Di386_unknown_linux2_4
ifdef CONFIG_SMP
MACHINE_DEFS 	+= -D__SMP__ -DSMP
endif

ifdef CONFIG_MODVERSIONS
CFLAGS += -DMODVERSIONS -include $(MODULES_INCLUDE)/linux/modversions.h
endif

# Hack to figure out if distribution is using O(1) scheduler patch
TASK_PROC	:= $(shell ../get_task_proc $(CC))

# set the following to -I<path to modutils/include> if your kernel doesn't provide kallsyms.h
KALLSYMS_INCLUDE = 

INCLUDES 	+= -I$(MODULES_INCLUDE) -I$(MODULES_INCLUDE)/linux $(KALLSYMS_INCLUDE)
CFLAGS 		+= $(INCLUDES) $(MACHINE_DEFS) $(TASK_PROC)
TFLAGS 		+= $(MACHINE_DEFS)

