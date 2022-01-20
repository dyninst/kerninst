// parseSymbol.h

#ifndef _PARSE_SYMBOL_H_
#define _PARSE_SYMBOL_H_

#include "elftraits.h"
#include "symbolTable.h"

void processSymbol(const KELF::ET_Sym &theSymbol, const char *strings,
		   const char * &current_file_name,
		   symbolTable &theSymbolTable // writes to this
		   );

#endif
