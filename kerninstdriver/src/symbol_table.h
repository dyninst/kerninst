// symbol_table.h

#ifndef _SYMBOL_TABLE_H_
#define _SYMBOL_TABLE_H_

#include <sys/modctl.h> // mod_unload_lock

extern "C" {
   struct module *getModuleByName(const char *modname);
   
   void modunload_disable();
   void modunload_enable();

   int kerninst_mod_hold_by_modctl_shared(struct modctl *modp);
   void kerninst_mod_release_mod_shared(struct modctl *modp);
};

#endif
