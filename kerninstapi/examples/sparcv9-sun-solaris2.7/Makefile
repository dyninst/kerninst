CC	= g++
LD	= g++

TO_CORE = ../../..
TO_KAPI = $(TO_CORE)/kerninstapi
TO_KAPI_LIB = $(TO_KAPI)/sparcv9-sun-solaris2.7
TO_UTIL = $(TO_CORE)/util
TO_PD_CORE = $(TO_CORE)/core
TO_PD_UTIL = $(TO_PD_CORE)/pdutil

INCLUDES = -I$(TO_KAPI)/h -I$(TO_CORE) -I$(TO_CORE)/kerninstdriver/src
CFLAGS	= -g $(INCLUDES) -D_KDRV64_ -Dsparc_sun_solaris2_7

LDFLAGS = -L$(TO_KAPI)/sparcv9-sun-solaris2.7 \
	  -L$(TO_UTIL)/sparc-sun-solaris2.7 \
	  -L$(TO_PD_UTIL)/sparc-sun-solaris2.8

OURLIBS = -lkerninstapi -lkerninstutil -lpdutil
SYSLIBS = -lsocket -lnsl -ldl

DEPS = $(TO_KAPI_LIB)/libkerninstapi.a \
       $(TO_KAPI)/h/kapi.h \
       $(TO_KAPI)/examples/src/kapi_timer_lib.h \
       $(TO_CORE)/kerninstdriver/src/vtimer.h

EXAMPLES = count_func time_func sparc_perfctr_example \
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
