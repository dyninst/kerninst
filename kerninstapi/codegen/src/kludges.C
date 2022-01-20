#include <stdio.h>
#include "kludges.h"

// We put everything in a namespace to enable tools that use both
// Kerninst and Dyninst API
namespace kcodegen {

void logLine(const char *line)
{
    fputs(line, stderr);
}

void showErrorCallback(int num, pdstring msg)
{
    fprintf(stderr, "showErrorCallback(%d, %s)\n", num, msg.c_str());
}

} // namespace
