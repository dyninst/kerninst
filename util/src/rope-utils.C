// rope-utils.C

#include <stdio.h> // sprintf()
#include "util/h/rope-utils.h"

pdstring num2string(unsigned short num) {
   char buffer[40];
   sprintf(buffer, "%u", (unsigned)num);
   pdstring result(buffer);
   return result;
}

pdstring num2string(unsigned num) {
   char buffer[40];
   sprintf(buffer, "%u", num);
   pdstring result(buffer);
   return result;
}

pdstring num2string(short int num) {
   char buffer[40];
   sprintf(buffer, "%d", (int)num);
   pdstring result(buffer);
   return result;
}

pdstring num2string(int num) {
   char buffer[40];
   sprintf(buffer, "%d", num);
   pdstring result(buffer);
   return result;
}

pdstring num2string(long num) {
   char buffer[40];
   sprintf(buffer, "%ld", num);
   pdstring result(buffer);
   return result;
}

pdstring num2string(unsigned long num) {
   char buffer[40];
   sprintf(buffer, "%lu", num);
   pdstring result(buffer);
   return result;
}

pdstring num2string(unsigned long long num) {
   char buffer[40];
   sprintf(buffer, "%llu", num);
   pdstring result(buffer);
   return result;
}

pdstring num2string(float num) {
   char buffer[40];
   sprintf(buffer, "%f", num);
   pdstring result(buffer);
   return result;
}

pdstring num2string(double num) {
   char buffer[40];
   sprintf(buffer, "%g", num);
   pdstring result(buffer);
   return result;
}


// --------------------------------------------------------------------------------

// An explanation of the below conversion specifiers:
// -- the leading 0x means nothing; it gets printed verbatim
// -- the %08x means: 8-width hex conversion, using '0' as padding (if needed)
//    (if '0' was omitted in the below specifiers, space padding would instead be used)
// addr2hex() adds leading hex zeros, to always ensure an 8-hex-digit address.
// the tohex() variants (formerly num2hex()) do not.

pdstring addr2hex(uint32_t num) {
   char buffer[40];
   sprintf(buffer, "0x%08X", num);
   pdstring result(buffer);
   return result;
}

pdstring addr2hex(uint64_t num) {
   char buffer[40];
   sprintf(buffer, "0x%016llX", num);
   pdstring result(buffer);
   return result;
}

pdstring tohex(unsigned num) {
   char buffer[40];
   sprintf(buffer, "0x%X", num);
   pdstring result(buffer);
   return result;
}

pdstring tohex(int num) {
   char buffer[40];
   sprintf(buffer, "0x%X", num);
   pdstring result(buffer);
   return result;
}

pdstring tohex(unsigned long num) {
   char buffer[40];
   sprintf(buffer, "0x%lX", num);
   pdstring result(buffer);
   return result;
}
pdstring tohex(uint64_t num) {
   char buffer[40];
   sprintf(buffer, "0x%lX", num);
   pdstring result(buffer);
   return result;
}

pdstring tohex(long num) {
   char buffer[40];
   sprintf(buffer, "0x%lX", num);
   pdstring result(buffer);
   return result;
}

