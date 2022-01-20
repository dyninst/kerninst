#!/bin/bash

if [ ! -f make.config ]; then
   echo "ERROR: not in top-level directory - please rectify"
   exit -1
fi

cd core/pdutil
if [ ! -f sparc-sun-solaris2.7 ]; then
   ln -s sparc-sun-solaris2.8 sparc-sun-solaris2.7
fi
cd ../..

cd kerninstapi
if [ ! -f sparc-sun-solaris2.7 ]; then
   ln -s sparcv9-sun-solaris2.7 sparc-sun-solaris2.7
fi
cd examples
if [ ! -f sparc-sun-solaris2.7 ]; then
   ln -s sparcv9-sun-solaris2.7 sparc-sun-solaris2.7
fi
cd ../..

cd kperfmon/kperfmon/sparcv9-sun-solaris2.7
if [ ! -f kperfmon.tcl ]; then
   ln -s ../tcl/kperfmon.tcl .
fi
if [ ! -f generic.tcl ]; then
   ln -s ../tcl/generic.tcl .
fi
cd ../sparc-sun-solaris2.7
if [ ! -f kperfmon.tcl ]; then
   ln -s ../tcl/kperfmon.tcl .
fi
if [ ! -f generic.tcl ]; then
   ln -s ../tcl/generic.tcl .
fi
cd ../../..
