// rope_utils.h

#ifndef _ROPE_UTILS_H_
#define _ROPE_UTILS_H_

#include "common/h/String.h"
#include <inttypes.h> // uint32_t

pdstring num2string(unsigned short num);
pdstring num2string(unsigned num);
pdstring num2string(short int num);
pdstring num2string(int num);
pdstring num2string(long num);
pdstring num2string(unsigned long num);
pdstring num2string(unsigned long long num);
pdstring num2string(float);
pdstring num2string(double);

// pads to 8 hex digits; adds leading zeros if necessary
pdstring addr2hex(uint32_t); 
// pads to 16 hex digits; adds leading zeros if necessary
pdstring addr2hex(uint64_t); 
pdstring tohex(unsigned); // no padding
pdstring tohex(int); // no padding
pdstring tohex(long); // no padding
pdstring tohex(unsigned long); // no padding
pdstring tohex(uint64_t);

#endif
