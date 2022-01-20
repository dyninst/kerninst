// symbol_table_2.7.C

#include "module_unloading.h"
#include "symbol_table.h"

/* These must always be included last: */
#include <sys/ddi.h>
#include <sys/sunddi.h>

void kerninst_modunload_disable() {
   modunload_disable();
}

void kerninst_modunload_enable() {
   modunload_enable();
}

int kerninst_mod_hold_by_modctl_shared(struct modctl *modp) {
   return mod_hold_by_modctl(modp, MOD_SHARED);
}

void kerninst_mod_release_mod_shared(struct modctl *modp) {
   mod_release_mod(modp, MOD_SHARED);
}
