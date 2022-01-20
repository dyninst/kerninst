#include <sys/time.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include "util/h/mrtime.h"

mrtime_t getmrtime()
{
    struct timeval tv;
    struct timezone tz;

    if (gettimeofday(&tv, &tz) < 0) {
	perror("gettimeofday error");
	assert(false);
    }
    return (1000 * ((mrtime_t)tv.tv_usec + 1000000 * (mrtime_t)tv.tv_sec));
}
