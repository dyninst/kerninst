// simplePath.C

#include "simplePath.h"

// ----------------------------------------------------------------------

int simplePath_counter::count; // initialized to 0

HomogenousHeapUni<refCounterK<pdvector<unsigned> >::ss, 1> *
refCounterK<pdvector<unsigned> >::ssHeap;

// ----------------------------------------------------------------------
