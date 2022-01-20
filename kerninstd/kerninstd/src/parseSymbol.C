// symbol.C

#include <assert.h>
#include <iostream.h>
#include "parseSymbol.h"
#include "symbolTable.h"
#include "util/h/out_streams.h"

void processFileSymbol(const KELF::ET_Sym &, const char *symbol_name,
		       symbolTable &theSymbolTable // writes to this
		       ) {
   // File symbols have an addr and lenth of 0, which is unfortunate
   // (it'd be nice if the addr specified the address of the first symbol
   // within the file).

   // So how can we determine the start address of the file?  (Needed so that
   // we can attribute symbols to a given file.)

   // Well we are given one rule.  In the symbol table, the file symbol will precede
   // any and all local symbols it contains.  Unfortunately it's not enough.

   dout << "found FILE symbol " << symbol_name << endl;

   theSymbolTable.addNewModule(symbol_name);
}

static symbol::scope getElfSymScope(const KELF::ET_Sym &theSymbol) {
   switch (KELF::ET_ST_BIND(theSymbol.st_info)) {
      case STB_LOCAL:
         return symbol::local;
      case STB_WEAK:
         return symbol::weak;
      case STB_GLOBAL:
         return symbol::global;
      default:
	 assert(false);
   }
}

static const char *unknownModuleStr = NULL;

void processObjectSymbol(const KELF::ET_Sym &theSymbol, const char *symbol_name,
			 void *symbol_addr, const char *file_name,
			 symbolTable &theSymbolTable) {
   symbol::scope thescope = getElfSymScope(theSymbol);
   const bool isLocal = (thescope == symbol::local);
   const char *moduleNameToUse = isLocal ? file_name : unknownModuleStr;

   // local symbols are always associated with a module in the symbol table.
   if (isLocal) {
      assert(file_name != NULL);
      assert(moduleNameToUse != NULL);
   }

   symbol elem((unsigned long)symbol_addr, theSymbol.st_size, symbol_name,
	       symbol::object, thescope);

   theSymbolTable.addNewSymbol(elem, moduleNameToUse);
}

void processFunctionSymbol(const KELF::ET_Sym &theSymbol, const char *symbol_name,
			   void *symbol_addr, const char *file_name,
			   symbolTable &theSymbolTable) {
   symbol::scope thescope = getElfSymScope(theSymbol);
   const bool isLocal = (thescope == symbol::local);
   const char *moduleNameToUse = isLocal ? file_name : unknownModuleStr;

   // local symbols are always associated with a module in the symbol table.
   if (isLocal) {
      assert(file_name != NULL);
      assert(moduleNameToUse != NULL);
   }

   symbol elem((unsigned long)symbol_addr, theSymbol.st_size, symbol_name,
	       symbol::function, thescope);

   theSymbolTable.addNewSymbol(elem, moduleNameToUse);
}

void processSymbol(const KELF::ET_Sym &theSymbol, const char *strings,
		   const char *&curr_module_name, // can write to this
		   symbolTable &theSymbolTable // writes to this
		   ) {
   // fields of Elf32_Sym:
   // st_name (index into string table).  If 0 then the symbol has no name.
   // st_value (address).  For our purposes, holds a virtual addr.  Modules have 0 here.
   // st_size (int).  Modules will have a size of 0.
   // st_info -- symbol type and binding attributes.
   //      possible values for ELF32_ST_BIND(st_info):
   //         STB_LOCAL -- local only to the obj file where it's defined.  Another
   //              symbol (in a diff obj file) can have the same name.
   //         STB_GLOBAL -- obvious.
   //         STB_WEAK -- like global but with a lower precedence.
   //              Multiple definitions of a STB_GLOBAL symbol isn't allowed, but if a
   //              defined global symbol exists when a weak one is encountered, there's
   //              no error (the weak one is discarded).
   //              Another difference: when the linker searches archives, members
   //              defining heretofore undefined global symbols are extracted (whether
   //              the defining member is global or weak).  But it won't extract to
   //              resolve a heretofore undefined weak symbol; unresolved weak symbols
   //              have a zero value.
   //         STB_LOPROC thru STB_HIPROC -- reserved (processor-specific)
   //     possible values for ELF32_ST_TYPE(st_info):
   //         STT_NOTYPE  -- unspecified type
   //         STT_OBJECT  -- a data object, e.g. a variable, array, etc.
   //         STT_FUNC    -- a function or other executable code.
   //                        If in a shared obj file, has special significance: when
   //                        another file references it, the linker creates a plt
   //                        (proc linkage table) entry for it.  Shared obj symbols
   //                        that aren't STT_FUNC aren't automatically referenced thru
   //                        the plt.
   //         STT_SECTION -- a section (usually for relocation & has a STB_LOCAL
   //                        binding)
   //         STT_FILE    -- names the source file associated with the obj file.
   //                        A file symbol has STB_LOCAL binding, SHM_ABS section
   //                        index, and precedes the other STB_LOCAL symbols for the
   //                        file.  All this provided that the file symbol exists.
   // st_other -- unused
   // st_shndx -- If the symbol refers to a location w/in a section, this gives the
   //             index into the section's header table.  Some special reserved values:
   //         SHN_ABS -- symbol has an absolute value that won't change due to
   //                    relocation.  According to ksyms(7d), this will be the case
   //                    for each symbol in the symbol table, since any necessary
   //                    relocations were performed by the kernel link editor at module
   //                    load time.
   //         SHN_COMMON -- a common block not yet allocated.  Symbol value gives
   //                    alignment constraint; symbol size give the # of bytes required.
   //         SHN_UNDEF -- undefined symbol...shouldn't happen in the kernel.

   const char *symbol_name = NULL;
   if (theSymbol.st_name != 0)
      symbol_name = &strings[theSymbol.st_name];

   const int symbol_type = KELF::ET_ST_TYPE(theSymbol.st_info);
   if (symbol_type == KELF::ET_STT_NOTYPE) {
      // If we can't whether the symbol is a function vs. a variable, for example,
      // then it's useless to us.
      // HOWEVER, this does occur quite a bit with the kernel...with symbol names that
      // appear useful.  So, what to do???  For now, we ignore the symbol.
      cout << "found a symbol with no type...discarding...name=" << symbol_name << endl;
      return;
   }

   if (symbol_type == KELF::ET_STT_FILE) {
      // According to "Linkers and Libraries Manual -- November, 1993":
      // The binding will be STB_LOCAL, the section index will be SHN_ABS,
      // and (here's what we care about) it precedes the other STB_LOCAL symbols
      // for the file (all assuming this symbol is present in the first place, which
      // it should be if it wasn't somehow optimized out).
      //
      // Reading between the lines, there is a critical conclusions:
      //     We can be sure that following STB_LOCAL symbols 'belong' to this file,
      //     but we **cannot** be sure that following global (or weak) symbols do.
      //
      //     In fact, it seems that global symbols appear all over the place, where
      //     you'd least expect them.
      //     That's why, in paradynd, we don't use anything in the .symtab section
      //     to determine the module for a global object...instead, we grab that
      //     information from the .stab section, which (I think) is what you get when
      //     you compile with -g.  In paradynd, when you parse a program not compiled
      //     with -g, all functions appear in the "module" called DEFAULT, or something
      //     like that.

      assert(symbol_name != NULL);
      processFileSymbol(theSymbol, symbol_name, theSymbolTable);
      curr_module_name = symbol_name;

      return;
   }

   if (symbol_type == KELF::ET_STT_SECTION) {
      // I sense that this could be useful (and it does occur quite a bit in the
      // kernel)...but I haven't yet understood the documentation for this...TO DO!

      cout << "Found obj of type STT_SECTION...ignoring [since not implemented]" << endl;
      return;
   }

   if (symbol_type >= KELF::ET_STT_LOPROC && 
       symbol_type <= KELF::ET_STT_HIPROC) {
      cout << "Found obj of type between STT_LOPROC and STT_HIPROC...ignoring" << endl;
      return;
   }

   // remaining types: STT_OBJECT (varaible), STT_FUNC (function)
   assert(symbol_type == KELF::ET_STT_OBJECT || 
	  symbol_type == KELF::ET_STT_FUNC);

   if (symbol_name == NULL) {
      // if the symbol has no name, how interesting can it be?
      cerr << "WARNING: STT_OBJECT or STT_FUNC symbol with no name; type=" << symbol_type << endl;
      return;
   }
   
   if (theSymbol.st_shndx == SHN_UNDEF) {
      cerr << "WARNING: found an undefined STT_OBJECT or STT_FUNC symbol...name=" << symbol_name << endl;
      return;
   }

   if (theSymbol.st_shndx == SHN_COMMON) {
      cerr << "WARNING: found a SHN_COMMON symbol...name=" << symbol_name
           << "...don't know what to do with it" << endl;
      return;
   }

   if (theSymbol.st_shndx != SHN_ABS) {
      // This symbol is relative to a section.  NOTE: This is the usual case for
      // an object file, but it doesn't seem to happen for the kernel.
       cout << "WARNING: STT_OBJECT or STT_FUNC symbol name=" << symbol_name << " is relative to a section" << endl;
      return;
   }

   if (symbol_type == KELF::ET_STT_OBJECT) {
      processObjectSymbol(theSymbol, symbol_name, (void*)theSymbol.st_value,
			  curr_module_name,
			  theSymbolTable // adds to this
			  );
      return;
   }
   else if (symbol_type == KELF::ET_STT_FUNC) {
      processFunctionSymbol(theSymbol, symbol_name, (void*)theSymbol.st_value,
			    curr_module_name,
			    theSymbolTable // adds to this
			    );
      return;
   }
   else
      assert(false);
}
