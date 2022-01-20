// symbol_table.C

#include <sys/mutex.h> // mutex_enter()
#include <sys/modctl.h> // mod_unload_lock
#include <sys/kobj.h> // struct module
#include <sys/sysmacros.h> // roundup()
#include <sys/machelf.h> // Solaris 2.6 does not define Elf_ symbols

#include "module_unloading.h"
#include "symbol_table.h"
#include "util/h/kdrvtypes.h"

// These two must always go last
#include <sys/ddi.h>
#include <sys/sunddi.h>


extern char stubs_base[], stubs_end[];

static int skip_symbol(const Sym &sym, const char *symname, struct module *mod) {
   if (ELF_ST_BIND(sym.st_info) == STB_LOCAL && mod != modules.mod_mp &&
       sym.st_shndx == SHN_UNDEF)
      return 1;
   else if (mod == modules.mod_mp && sym.st_value >= (Addr)stubs_base &&
            sym.st_value < (Addr)stubs_end &&
            ELF_ST_TYPE(sym.st_info) == STT_NOTYPE)
      return 1;
   else if (NULL == kobj_lookup(mod, (char*)symname))
      return 1;
   return 0;
}

struct module *getModuleByName(const char *modname) {
   struct modctl *modp = &modules; // <modctl.h>
   do {
      if (0==strcmp(modp->mod_modname, modname))
         return (struct module*)(modp->mod_mp);
   } while ((modp = modp->mod_next) != &modules); // circular linked list (?!)

   return NULL;
}

uint32_t kerninst_symtab_do(int fillin, unsigned char *buffer) {
   // There's no modunloadctrl() in 2.6, so it would be best to verify what
   // the real 2.6 src code does when filling in /dev/ksyms...that goes for
   // much of this routine.

   kerninst_modunload_disable();
   // differs on 2.5.1 vs. 2.6
   
   // now loop thru all the modules.  See uts/common/io/ksyms.c's
   // ksyms_image_create()
   
   // NOTE: Best place to check for solaris 7 is now: ksyms_update.c

   struct modctl *modp = &modules; // modules is a global vrble...<modctl.h>

   unsigned char num_modules = 0;
   do {
      mutex_enter(&mod_lock);
      while (kerninst_mod_hold_by_modctl_shared(modp) == 1)
         ;
      struct module *mod = (struct module *)(modp->mod_mp);
      mutex_exit(&mod_lock);

      if (mod == NULL) {
         kerninst_mod_release_mod_shared(modp);
         continue;
      }
      num_modules++;
      kerninst_mod_release_mod_shared(modp); // don't forget this line!
   } while ((modp = modp->mod_next) != &modules); /* circular linked list (?) */

   uint32_t result = 0;

   if (fillin)
      *(unsigned char*)buffer = num_modules;
   result = sizeof(num_modules);

   modp = &modules;
   do {
      mutex_enter(&mod_lock);
      while (kerninst_mod_hold_by_modctl_shared(modp) == 1)
         ;
      struct module *mod = (struct module *)(modp->mod_mp);
      mutex_exit(&mod_lock);

      if (mod == NULL) {
         kerninst_mod_release_mod_shared(modp); // don't forget this line!
         continue;
      }

      // Length of module name:
      const unsigned char modnamelen = strlen(modp->mod_modname) + 1;
      if (fillin)
         *(unsigned char*)(buffer + result) = modnamelen;
      result += sizeof(modnamelen); /* len of module name */
      
      // The actual module name, rounded up
      if (fillin)
         strncpy((char*)buffer + result, modp->mod_modname, modnamelen);
      result += modnamelen;
      result = roundup(result, sizeof(kptr_t));

      // Length of module description:
      const char *module_description = modp->mod_linkage == NULL ? "" :
         ((struct modldrv *)(modp->mod_linkage->ml_linkage[0]))->drv_linkinfo;
      const uint32_t module_description_len = strlen(module_description);
      
      if (fillin)
         *(uint32_t*)(buffer + result) = module_description_len;
      result += sizeof(module_description_len);
      
      // The actual module description:
      if (fillin)
         strncpy((char*)buffer + result, module_description, 
		 module_description_len);
      result += module_description_len;

      // Fields of interest:
      // within "struct modctl" <modctl.h>
      // mod_modname -- the short name of the module
      // mod_filename -- a slightly longer name; includes some path info.  But not
      //                 all modules report these paths in a consistent format;
      //                 some prepend /kernel, thus giving a full file path, while
      //                 others just give the tail 1-3 parts of the path.  Still useful
      //                 though.
      // ((struct modldrv *)(mod_linkage->ml_linkage[0]))->drv_linkinfo -- high-level
      //                 description of the module.  Note that the typecast to
      //                 struct modldrv at first glance looks awfully buggy, since
      //                 not all types of modules are drivers (there's also
      //                 modlsys, modlfs, modlmisc, modlstrmod, modlsched, and
      //                 modlexec).  But it's ok here since the 1st two fields -- and
      //                 that includes the 2d field which we use here -- are always
      //                 the same. WARNING -- the nucleus modules have a NULL
      //                 ml_linkage so this doesn't even remotely work for them!

      // start of text seg
      result = roundup(result, sizeof(kptr_t));
      if (fillin)
         *(char **)(buffer + result) = mod->text;
      result += sizeof(char *);

      // len of text seg
      if (fillin)
         *(uint32_t *)(buffer + result) = mod->text_size;
      result += sizeof(uint32_t);

      // start of data seg
      result = roundup(result, sizeof(kptr_t));
      if (fillin)
         *(char **)(buffer + result) = mod->data;
      result += sizeof(char *);
      
      // len of data seg
      if (fillin)
         *(uint32_t *)(buffer + result) = mod->data_size;
      result += sizeof(uint32_t);

      
      
      // Go thru all symbols of the module, finding the end of the string table
      // (is there an easier way?)
      if ((kptr_t)mod % sizeof(kptr_t) != 0) {
         cmn_err(CE_WARN, "ari -- unaligned mod ptr!");
         delay(drv_usectohz(5000000));
      }
      
      uint32_t string_table_nbytes = 0;
      const Sym *symtable = (Sym *)mod->symtbl;
      if ((kptr_t)symtable % sizeof(kptr_t) != 0) {
         cmn_err(CE_WARN, "ari -- unaligned symbol table! (0)");
         delay(drv_usectohz(5000000));
      }

      uint32_t num_unskipped_symbols = 0;
      for (uint32_t symlcv=0; symlcv < mod->nsyms; symlcv++) {
         const Sym &sym = symtable[symlcv];
         if ((kptr_t)&sym % sizeof(kptr_t) != 0) {
            cmn_err(CE_WARN, "ari -- unaligned symab entry! (1)");
            delay(drv_usectohz(5000000));
         }
         
         if (sym.st_name == 0)
            // a no-name symbol.  Skip it.
            continue;
         
         char *symname = (char*)mod->strings + sym.st_name;

         if (skip_symbol(sym, symname, mod))
            continue;

         num_unskipped_symbols++;
         
         uint32_t needed_table_len = sym.st_name;
         if (needed_table_len)
            needed_table_len += (strlen(symname) + 1);

         if (needed_table_len > string_table_nbytes)
            string_table_nbytes = needed_table_len;
      }

//      cmn_err(CE_CONT, ", needs %lu bytes for strtab\n", string_table_nbytes);
//      delay(drv_usectohz(1000000)); // 1 second

      // Fill in the length of the string table:
      if (fillin)
         *(uint32_t*)(buffer + result) = string_table_nbytes;
      result += sizeof(string_table_nbytes);

      // Now fill in the actual string table:
      if (fillin)
         bcopy(mod->strings, // src
               (char*)buffer + result, // dest
               string_table_nbytes);

      result += string_table_nbytes;
      result = roundup(result, sizeof(kptr_t));

      // TEMP FOR DEBUG: a cookie marking the beginning of symbols
      if (fillin)
         *(uint32_t*)(buffer + result) = 0x12345678;
      result += sizeof(uint32_t);
         
      // The number of symbols
      if (fillin)
         *(uint32_t*)(buffer + result) = num_unskipped_symbols;
      result += sizeof(num_unskipped_symbols);
      
      // The actual symbols
      for (uint32_t symlcv=0; symlcv < mod->nsyms; symlcv++) {
         const Sym &sym = symtable[symlcv];
         if (sym.st_name == 0) continue;
         char *symname = (char*)mod->strings + sym.st_name;
         if (skip_symbol(sym, symname, mod))
            continue;

         if (fillin)
            bcopy((char*)&sym, // src addr
                  (char*)buffer + result, // dest addr
                  sizeof(Sym));
         result += sizeof(Sym);
         result = roundup(result, sizeof(kptr_t));
      }

      kerninst_mod_release_mod_shared(modp); // don't forget this line!
   } while ((modp = modp->mod_next) != &modules); /* circular linked list (?) */

   kerninst_modunload_enable();

//   cmn_err(CE_CONT, "kerninst_symtab_do: I'm returning %lu\n now",
//           result);
//   delay(drv_usectohz(1000000)); // 1 second

   return result;
}


