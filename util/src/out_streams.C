#include <iostream.h>
#include "util/h/out_streams.h"

debug_ostream dout(cout, false);

void init_out_streams(bool debug_output)
{
    if (debug_output) {
	dout.turnOn();
    }
}
