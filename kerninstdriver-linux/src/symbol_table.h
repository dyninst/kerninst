/* symbol_table.h */

#ifndef _KERNINST_SYMBOL_TABLE_
#define _KERNINST_SYMBOL_TABLE_

#include "kernInstIoctls.h" // kptr_t

unsigned kerninst_symtab_size(void);
unsigned kerninst_symtab_get(kptr_t user_buf, int buf_size);
unsigned kerninst_symtab_do(int fillin, unsigned char *buf);

#endif
