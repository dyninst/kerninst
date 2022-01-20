EXTRA_CFLAGS	+= -Di386_unknown_linux2_4 -Di386_unknown_linux2_6

obj-m := kerninst.o
kerninst-objs := kerninst-main.o \
                 all_vtimers.o \
		 calleeCounter.o \
		 cswitch_inout.o \
		 hash_table.o \
		 kernInstIoctls.o \
		 loggedAllocRequests.o \
		 loggedWrites.o \
		 physMem.o \
		 stacklist.o \
		 symbol_table-2.6.o \
		 hwcounter_p4.o
