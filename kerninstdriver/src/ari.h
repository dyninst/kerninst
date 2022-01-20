// ari.h

#include "kernInstIoctls.h"
#include <sys/types.h>

extern "C" {
uint32_t kerninst_symtab_size();
uint32_t kerninst_symtab_get(unsigned char *userbuffer,
			     uint32_t buffer_size);
int kerninst_segkmem(void*, int len);
static int ari_segkmem_getprot(register struct seg *, register caddr_t,
                               register u_int, register u_int *);

void* ari_virtual_to_physical_addr(void *vaddr);

void kerninst_pokeblock_nosafetychex(const unsigned char *srcAddrInUser,
                                     unsigned char *destAddrInKernel,
                                     unsigned len);
}

