#include "x86_bb.h"

// all of x86_bb's member functions are static, so
// they must be defined in the .h file. the reason they
// must be static is that basicblock makes calls of the form
// arch_bb_t::doLiveRegAnalysis(...)
