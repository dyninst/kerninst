/* symbol_table.c */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/kallsyms.h>
#include <asm/uaccess.h>
#include "symbol_table.h"

#ifdef ppc64_unknown_linux2_4
#define roundup(x, y)   ((((x)+((y)-1))/(y))*(y))
#endif

#define MAXLEN 100 /* arbitrary for now */

unsigned kerninst_symtab_do(int fillin, unsigned char *buf)
{
  unsigned nbytes = 0, nsyms = 0;
  kptr_t data_addr = 0, text_addr = 0;
  unsigned data_size = 0, text_size = 0;
#ifdef ppc64_unknown_linux2_4
  kptr_t toc_addr=0;
  unsigned toc_size=0;
#endif
  
  unsigned str_tab_len, needed_tab_len, name_offset = 0;
  int nsec = 0;
  const char *name = NULL;
  const char *mod_name = NULL;
  unsigned char name_len = 0, nmods = 0;
  unsigned int *len_ptr = NULL;
  struct module *start = THIS_MODULE;
  struct module *modp = start;  
  struct module_symbol *symp = NULL;

  const struct kallsyms_header *ka_hdr = NULL;
  const struct kallsyms_section *ka_sec = NULL;
  const struct kallsyms_symbol *ka_sym = NULL;
  const struct kallsyms_symbol *ka_sym_tmp = NULL;
  struct kerninst_symbol ksym;
  const char *ka_str = NULL;
 
  /* sanity check */
  if( fillin && !buf ) return 0;

  /* record number of modules, including kernel */
  do {
     nmods++;
     modp = modp->next;
  } while((modp != NULL) && (modp != start));
  if(fillin)
     *(unsigned char*)(buf + nbytes) = nmods;
  nbytes += sizeof(unsigned char); /* to hold number of modules */
  
  /* record info on each module */
  modp = start;

  do {
     symp = modp->syms;
     mod_name = modp->name;
     /* get module name information */
     name_len = strlen(mod_name); 
     if(fillin) {
        if(name_len == 0) { /* the "kernel" module */
           *(unsigned char*)( buf + nbytes) = 7;
           strncpy(buf + nbytes + sizeof(unsigned char), "kernel", 7);
        } else { /* regular module */
           *(unsigned char*)(buf + nbytes) = name_len + 1;
           strncpy(buf + nbytes + sizeof(unsigned char), mod_name, name_len+1);
        }
     }
     nbytes += sizeof(unsigned char);
     nbytes += ( name_len ? name_len + 1 : 7 );
#ifdef ppc64_unknown_linux2_4
     nbytes = roundup(nbytes, sizeof(kptr_t));
#endif

     /* get rest of module information */
     if( mod_member_present(modp,kallsyms_start) && 
         mod_member_present(modp,kallsyms_end) &&
         (modp->kallsyms_start < modp->kallsyms_end) ) { 
        /* the module is giving us what we want -> ALL the symbols */
        ka_hdr = (struct kallsyms_header *)modp->kallsyms_start;
        ka_sec = (struct kallsyms_section *)((char *)ka_hdr + ka_hdr->section_off);
        ka_sym = (struct kallsyms_symbol *)((char *)ka_hdr + ka_hdr->symbol_off);
        ka_str = ((char *)ka_hdr + ka_hdr->string_off);

        nsec = ka_hdr->sections;
       
        while(nsec--) {
           name = ka_str + ka_sec->name_off;
           if( (! strncmp(name, ".data", 5)) && (strlen(name)==5) ) {
              data_addr = ka_sec->start;
              data_size = ka_sec->size;
           } else if( (! strncmp(name, ".text", 5)) && (strlen(name)==5) ) {
              text_addr = ka_sec->start;
              text_size = ka_sec->size;
           }
#ifdef ppc64_unknown_linux2_4
           else if(! strncmp(name, ".toc", 4) ) {
              toc_addr = ka_sec->start;
              toc_size = ka_sec->size;  
           }
#endif
           if(nsec)
              kallsyms_next_sec(ka_hdr, ka_sec); 
        }
        if(fillin)
           *(kptr_t *)(buf + nbytes) = text_addr;
        nbytes += sizeof(kptr_t);
        if(fillin)
           *(unsigned *)(buf + nbytes) = text_size;
        nbytes += sizeof(kptr_t); //for alignment purposes, it's kptr_t
        if(fillin)
           *(kptr_t *)(buf + nbytes) = data_addr;
        nbytes += sizeof(kptr_t); 
        if(fillin)
           *(unsigned *)(buf + nbytes) = data_size;
        nbytes += sizeof(kptr_t); //for alignment purposes, it's kptr_t
#ifdef ppc64_unknown_linux2_4
        if(fillin)
           *(kptr_t *)(buf + nbytes) = toc_addr;
        nbytes += sizeof(kptr_t); 
        if(fillin)
           *(unsigned *)(buf + nbytes) = toc_size;
        nbytes += sizeof(kptr_t); //for alignment purposes, it's kptr_t
#endif
      
        /* loop through all symbols to find size of string table */
        nsyms = ka_hdr->symbols;
        ka_sym_tmp = ka_sym;
        str_tab_len = 0;
        needed_tab_len = 0;
        while(nsyms--) {
           name_len = strnlen(ka_str + ka_sym_tmp->name_off, MAXLEN);
           if(name_len == MAXLEN) {
              printk("Invalid symbol name encountered\n");
              continue;
           }
           needed_tab_len = ka_sym_tmp->name_off + name_len + 1;
           if(needed_tab_len > str_tab_len)
              str_tab_len = needed_tab_len;
           if(nsyms)
              kallsyms_next_sym(ka_hdr, ka_sym_tmp);
        }
        if(fillin)
           *(unsigned *)(buf + nbytes)  = str_tab_len;
        nbytes += sizeof(unsigned);
        if(fillin) {
           memcpy(buf+nbytes, ka_str, str_tab_len);
        }
        nbytes += str_tab_len;
#ifdef ppc64_unknown_linux2_4
        nbytes = roundup(nbytes, sizeof(kptr_t));
#endif
        /* a cookie marking the beginning of symbols */
        if(fillin) 
           *(unsigned*)(buf + nbytes) = 0x12345678;
        nbytes += sizeof(unsigned);
        
        /* now process symbols */
        nsyms = ka_hdr->symbols;
        if(fillin) 
           *(unsigned *)(buf + nbytes) = nsyms;
        nbytes += sizeof(unsigned); /* to hold number of symbols */
#ifdef ppc64_unknown_linux2_4
        nbytes = roundup(nbytes, sizeof(kptr_t));
#endif
	while(nsyms--) {
           ksym.sym_addr = ka_sym->symbol_addr;
           ksym.sym_name_offset = ka_sym->name_off;
           if(fillin) 
              memcpy(buf + nbytes, &ksym, sizeof(struct kerninst_symbol));
           if(nsyms) 
              kallsyms_next_sym(ka_hdr, ka_sym);
           nbytes += sizeof(struct kerninst_symbol);
#ifdef ppc64_unknown_linux2_4
            nbytes = roundup(nbytes, sizeof(kptr_t));
#endif
	}
     } else { /* we'll have to settle for the exported symbols */
        if(fillin)
           *(kptr_t *)(buf + nbytes) = 0;
        nbytes += sizeof(kptr_t);
        if(fillin)
           *(unsigned *)(buf + nbytes) = 0;
        nbytes += sizeof(kptr_t); //for alingnment, we increment by kptr_t
        if(fillin)
           *(kptr_t *)(buf + nbytes)  = 0;
        nbytes += sizeof(kptr_t);
        if(fillin)
           *(unsigned *)(buf + nbytes) = 0;
        nbytes += sizeof(kptr_t); //for alignment, we increment by kptr_t
#ifdef ppc64_unknown_linux2_4
        if(fillin)
           *(kptr_t *)(buf + nbytes) = 0;
        nbytes += sizeof(kptr_t);
        if(fillin)
           *(unsigned *)(buf + nbytes) = 0;
        nbytes += sizeof(kptr_t);
#endif
        /* loop through all symbols to find size of string table */
        nsyms = modp->nsyms;
        symp = modp->syms;
        str_tab_len = 0;
        len_ptr = (unsigned int*)(buf + nbytes);
        nbytes += sizeof(unsigned);
        while(nsyms--) {
           name_len = strnlen(symp->name, MAXLEN) + 1;
           if(name_len == MAXLEN) {
              continue;
           }
           str_tab_len += name_len;
           if(fillin) {
              strncpy(buf + nbytes, symp->name, name_len);
           }
           nbytes += name_len;
           if(nsyms)
              symp++;
        }
#ifdef ppc64_unknown_linux2_4
        nbytes = roundup(nbytes, sizeof(kptr_t));
#endif
        if(fillin)
           *len_ptr = str_tab_len;

        /* a cookie marking the beginning of symbols */
        if(fillin) 
           *(unsigned*)(buf + nbytes) = 0x12345678;
        nbytes += sizeof(unsigned);

        /* now process symbols */
        nsyms = modp->nsyms;
        if(fillin)
           *(unsigned *)(buf + nbytes) = nsyms;
        nbytes += sizeof(unsigned); /* to hold number of symbols */
#ifdef ppc64_unknown_linux2_4
        nbytes = roundup (nbytes, sizeof(kptr_t));
#endif
	symp = modp->syms;
	name_offset = 0;
	while(nsyms--) {
           ksym.sym_addr = symp->value;
           ksym.sym_name_offset = name_offset;
           if(fillin) 
              memcpy(buf + nbytes, &ksym, sizeof(struct kerninst_symbol));
           name_len = strlen(symp->name);
           name_offset += name_len + 1;
           if(nsyms)
              symp++;
           nbytes += sizeof(struct kerninst_symbol);
        }
        name_offset += name_len + 1;
        if(nsyms)
           symp++;
	if(name_offset != str_tab_len)
           printk(KERN_WARNING "mismatch on string table\n");
     }
     modp = modp->next;
  } while((modp != NULL) && (modp != start));

  return nbytes;
}

unsigned kerninst_symtab_size()
{
   return kerninst_symtab_do(0, NULL);
}

unsigned kerninst_symtab_get(kptr_t user_buf, int buf_size)
{
   void *buffer;
   unsigned result;
   unsigned using_vmalloc = 0;
   u_long pages, order = 0, base = 1;
   if( buf_size < kerninst_symtab_size() ) {
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
   result = kerninst_symtab_do(1, (unsigned char *)buffer);
   copy_to_user((void *)user_buf, buffer, result);
   if(using_vmalloc)
      vfree(buffer);
   else
      free_pages((u_long)buffer, order);
   return result;
}
