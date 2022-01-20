#ifndef _KLUDGES_H_
#define _KLUDGES_H_

#include <inttypes.h>
#include "common/h/String.h"
#include "ast.h"

#define REG_MT 23

// We put everything in a namespace to enable tools that use both
// Kerninst and Dyninst API
namespace kcodegen {

extern void logLine(const char *line);
extern void showErrorCallback(int num, pdstring msg);
extern char *P_strdup(const char *s1);

class function_base {
    HostAddress entryAddr;
    pdstring funcName;
 public:
    function_base(HostAddress addr, pdstring name) : 
	entryAddr(addr), funcName(name)	{
    }

    HostAddress getAddress() const {
	return entryAddr;
    }
    pdstring prettyName() const {
	return funcName;
    }
};

class instPoint {
    HostAddress addr;
 public:
    instPoint(HostAddress iAddr) : addr(iAddr) {
    }

    HostAddress insnAddress() const {
	return addr;
    }
};

class process {
};

} // namespace
#endif
