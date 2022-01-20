#include <sys/time.h>
#include "util/h/mrtime.h"

mrtime_t getmrtime()
{
    return gethrtime();
}
