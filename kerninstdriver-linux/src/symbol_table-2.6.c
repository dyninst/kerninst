/* symbol_table.c */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/kallsyms.h>
#include <asm/uaccess.h>
#include "symbol_table.h"

unsigned kerninst_symtab_do(int fillin, unsigned char *buf)
{
   unsigned long nbytes = 0;

   const char *name = NULL;
   unsigned char nmods = 0, name_len = 0;
   unsigned long name_offset = 0;
  
   unsigned char *bufp = buf;
   struct module *start = THIS_MODULE;
   struct module *modp = start;  
  
   struct kernel_symbol *ksymp = NULL, *gpl_ksymp = NULL; 
   unsigned int num_ksyms = 0, num_gpl_ksyms = 0;

   unsigned char *str_tab_start = NULL;
   unsigned long str_tab_len, *str_tab_len_ptr = NULL;

   Elf_Sym *kallsymp = NULL;
   unsigned long num_kallsyms;

   struct kerninst_symbol ksym;

   struct list_head *modules_head = NULL, *modules_iter = NULL;

   /* sanity check */
   if( fillin && !bufp ) return 0;

   /* kludge to find head of modules list */
   modules_iter = modp->list.next;
   while(modules_iter != &(modp->list)) {
      modp = list_entry(modules_iter, struct module, list);
      if((modp->state != MODULE_STATE_LIVE) &&
         (modp->state != MODULE_STATE_COMING) &&
         (modp->state != MODULE_STATE_GOING)) {
      	 modules_head = modules_iter;
	 break;
      }
      modules_iter = modules_iter->next;
   }
   if(modules_head == NULL) return nbytes;
   
   /* record number of modules, including kernel */
   modp = start;
   list_for_each_entry(modp, modules_head, list) {
      nmods++;
   }
   nbytes += sizeof(unsigned char); /* to hold number of modules */
   if(fillin) { 
      *(unsigned char*)bufp = nmods + 1; /* +1 for "kernel" module */
      bufp += sizeof(unsigned char); 
   }

   /* record info on each module */
   nbytes += nmods*sizeof(unsigned char); /* to hold per-module name length */
   modp = start;
   list_for_each_entry(modp, modules_head, list) {
      name = modp->name;
      /* get module name information */
      name_len = strlen(name) + 1;
      if(fillin) {
         *(unsigned char*)bufp = name_len;
         bufp += sizeof(unsigned char);
         strncpy(bufp, name, name_len);
         bufp += name_len;
      }
      nbytes += name_len;

      nbytes += 2 * (sizeof(kptr_t) + sizeof(unsigned long));
      /* to hold start & size of text & data sections */
      if(fillin) {
         *(kptr_t *)bufp = (kptr_t)modp->module_core;
         bufp += sizeof(kptr_t);
         *(unsigned long*)bufp = modp->core_text_size;
         bufp += sizeof(unsigned long);
         *(kptr_t *)bufp = (kptr_t)((char *)modp->module_core + modp->core_text_size);
         bufp += sizeof(kptr_t);
         *(unsigned long*)bufp = modp->core_size - modp->core_text_size;
         bufp += sizeof(unsigned long);
      }

      /* loop through all symbols to find size of string table */
      str_tab_len = 0;
      str_tab_len_ptr = (unsigned long*)bufp;
      bufp += sizeof(unsigned long);

#ifdef CONFIG_KALLSYMS
      num_kallsyms = modp->num_symtab;
      kallsymp = modp->symtab;
      str_tab_start = modp->strtab;
      while(num_kallsyms--) {
         name_len = strlen(str_tab_start + kallsymp->st_name) + 1;
         str_tab_len += name_len;
         if(fillin) {
            strncpy(bufp, str_tab_start + kallsymp->st_name, name_len);
            bufp += name_len;
         }
         if(num_kallsyms)
            kallsymp++;
      }

      if(fillin) 
	*str_tab_len_ptr = str_tab_len;
      nbytes += sizeof(unsigned long) + str_tab_len;

      /* a cookie marking the beginning of symbols */
      if (fillin) {
         *(unsigned*)bufp = 0x12345678;
	 bufp += sizeof(unsigned);
      }
      nbytes += sizeof(unsigned);

      /* now process symbols */
      nbytes += sizeof(unsigned long); /* to hold number of symbols */
      num_kallsyms = modp->num_symtab;
      if(fillin) {
         *(unsigned long*)bufp = num_kallsyms;
         bufp += sizeof(unsigned long);
      }
      nbytes += num_kallsyms * sizeof(struct kerninst_symbol);
      if(fillin) {
         kallsymp = modp->symtab;
         name_offset = 0;
         while(num_kallsyms--) {
            ksym.sym_addr = kallsymp->st_value;
            ksym.sym_name_offset = name_offset;
            memcpy(bufp, &ksym, sizeof(struct kerninst_symbol));
            name_len = strlen(str_tab_start + kallsymp->st_name);
            name_offset += name_len + 1;
            bufp += sizeof(struct kerninst_symbol);
            if(num_kallsyms)
               kallsymp++;
         }
         if(name_offset != str_tab_len)
            printk(KERN_WARNING "mismatch on string table\n");
      }

#else // !CONFIG_KALLSYMS
      num_ksyms = modp->num_syms;
      ksymp = modp->syms;
      while(num_ksyms--) {
         name_len = strlen(ksymp->name) + 1;
         str_tab_len += name_len;
         if(fillin) {
            strncpy(bufp, ksymp->name, name_len);
            bufp += name_len;
         }
         if(num_ksyms)
            ksymp++;
      }

      num_gpl_ksyms = modp->num_gpl_syms;
      gpl_ksymp = modp->gpl_syms;
      while(num_gpl_ksyms--) {
         name_len = strlen(gpl_ksymp->name) + 1;
         str_tab_len += name_len;
         if(fillin) {
            strncpy(bufp, gpl_ksymp->name, name_len);
            bufp += name_len;
         }
         if(num_gpl_ksyms)
            gpl_ksymp++;
      }

      if(fillin) 
	*str_tab_len_ptr = str_tab_len;
      nbytes += sizeof(unsigned long) + str_tab_len;

      /* a cookie marking the beginning of symbols */
      if (fillin) {
         *(unsigned*)bufp = 0x12345678;
	 bufp += sizeof(unsigned);
      }
      nbytes += sizeof(unsigned);

      /* now process symbols */
      num_kallsyms = modp->num_syms + modp->num_gpl_syms;
      nbytes += sizeof(unsigned long); /* to hold number of symbols */
      if(fillin) {
         *(unsigned long*)bufp = num_kallsyms;
         bufp += sizeof(unsigned long);
      }
      nbytes += num_kallsyms * sizeof(struct kerninst_symbol);
      if(fillin) {
         ksymp = modp->syms;
         num_ksyms = modp->num_syms;
         name_offset = 0;
         while(num_ksyms--) {
            ksym.sym_addr = ksymp->value;
            ksym.sym_name_offset = name_offset;
            memcpy(bufp, &ksym, sizeof(struct kerninst_symbol));
            name_len = strlen(ksymp->name);
            name_offset += name_len + 1;
            bufp += sizeof(struct kerninst_symbol);
            if(num_ksyms)
               ksymp++;
         }
         gpl_ksymp = modp->gpl_syms;
         num_gpl_ksyms = modp->num_gpl_syms;
         while(num_gpl_ksyms--) {
            ksym.sym_addr = gpl_ksymp->value;
            ksym.sym_name_offset = name_offset;
            memcpy(bufp, &ksym, sizeof(struct kerninst_symbol));
            name_len = strlen(gpl_ksymp->name);
            name_offset += name_len + 1;
            bufp += sizeof(struct kerninst_symbol);
            if(num_gpl_ksyms)
               gpl_ksymp++;
         }
         if(name_offset != str_tab_len)
            printk(KERN_WARNING "mismatch on string table\n");
      }
#endif // CONFIG_KALLSYMS
   }

   /* record info on "kernel" module */
   nbytes += sizeof(unsigned char) + 7 + 2*sizeof(kptr_t) 
             + 4*sizeof(unsigned long) + sizeof(unsigned);

   if(fillin) {
      /* module name */
      *(unsigned char*)bufp = 7;
      bufp += sizeof(unsigned char);
      strncpy(bufp, "kernel", 7);
      bufp += 7;

      /* text and data sections */
      *(kptr_t *)bufp = (kptr_t)0;
      bufp += sizeof(kptr_t);
      *(unsigned long*)bufp = 0;
      bufp += sizeof(unsigned long);
      *(kptr_t *)bufp = (kptr_t)0;
      bufp += sizeof(kptr_t);
      *(unsigned long*)bufp = 0;
      bufp += sizeof(unsigned long);

      /* empty string table */
      *(unsigned long*)bufp = 0;
      bufp += sizeof(unsigned long);

      /* cookie & no symbols */
      *(unsigned*)bufp = 0x12345678;
      bufp += sizeof(unsigned);
      *(unsigned long*)bufp = 0;
      bufp += sizeof(unsigned long);
   }

   return nbytes;
}

static unsigned cached_symtab_size = 0;

unsigned kerninst_symtab_size(void)
{
   cached_symtab_size =  kerninst_symtab_do(0, (unsigned char *)NULL);
   return cached_symtab_size;
}

unsigned kerninst_symtab_get(kptr_t user_buf, int buf_size)
{
   void *buffer;
   unsigned result;
   unsigned using_vmalloc = 0;
   u_long pages, order = 0, base = 1;
   if( buf_size < cached_symtab_size ) {
      printk(KERN_WARNING "kerninst_symtab_get: buffer of size %d bytes too small to hold symbol table info\n", buf_size);
      return 0;
   }
   pages = (buf_size / 4096) + ( buf_size % 4096 ? 1 : 0 );
   while( base < pages ) {
      base <<= 1;
      order++;
   }
   buffer = (void *)__get_free_pages(GFP_KERNEL, order);
   if(!buffer) {
      using_vmalloc = 1;
      buffer = vmalloc(buf_size);
      if(!buffer) {
         printk(KERN_WARNING "kerninst_symtab_get: could not allocate in-kernel buffer of size %d for symbol table info\n", buf_size);
         return 0;
      }
   }
   copy_from_user(buffer, (void *)user_buf, (4 * sizeof(kptr_t)));
   result = kerninst_symtab_do(1, (unsigned char *)buffer);
   copy_to_user((void *)user_buf, buffer, result);
   if(using_vmalloc)
      vfree(buffer);
   else
      free_pages((u_long)buffer, order);
   return result;
}
