PLATFORM = ppc64-unknown-linux2.4
CC	= g++
LD	= g++

TO_CORE = ../../..
TO_KAPI = $(TO_CORE)/kerninstapi
TO_KAPI_LIB = $(TO_KAPI)/$(PLATFORM)
TO_UTIL = $(TO_CORE)/util
TO_PD_CORE = $(TO_CORE)/core
TO_PD_UTIL = $(TO_PD_CORE)/pdutil

INCLUDES = -I$(TO_KAPI)/h -I$(TO_CORE) -I$(TO_CORE)/kerninstdriver-linux/src -I${TO_CORE}/core
CFLAGS	= -g $(INCLUDES) -D_KDRV64_ -Dppc64_unknown_linux2_4

LDFLAGS = -L$(TO_KAPI_LIB) \
	  -L$(TO_UTIL)/$(PLATFORM) \
	  -L$(TO_PD_UTIL)/$(PLATFORM)

OURLIBS = -lkerninstapi -lkerninstutil -lpdutil
SYSLIBS = -lnsl -ldl

DEPS = $(TO_KAPI_LIB)/libkerninstapi.a \
       $(TO_KAPI)/h/kapi.h \
       $(TO_KAPI)/examples/src/kapi_timer_lib.h \
       $(TO_CORE)/kerninstdriver-linux/src/vtimer.h

EXAMPLES = count_func time_func power4_perfctr_example \
           replace_function replace_function_call \
           watch_callees callee_params_retval \
	   pid_predicate register_analysis

all: $(EXAMPLES)

%: %.o $(DEPS)
	$(LD) $(LDFLAGS)  -o $@ $< $(OURLIBS) $(SYSLIBS)
%.o: $(TO_KAPI)/examples/src/%.C
	$(CC) $(CFLAGS) -c $^
clean:
	rm -f *.o $(EXAMPLES)
