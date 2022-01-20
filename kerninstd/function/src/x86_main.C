// main.C

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <iostream.h>
#include <fstream.h>
#include <stdio.h>
#include <stdlib.h> // exit()
#include <time.h>

#include "x86Traits.h"
#include "x86_instr.h"
#include "insnVec.h"
#include "basicblock.h"
#include "funkshun.h"
#include "x86_kmem.h"

#include "util/h/hashFns.h"

/* These are tested in the parsing code */
bool global_verbose3 = false;
bool verbose_fnParse = true;

static FILE *savefp = NULL;
static FILE *symsfp = NULL;

static int
nextlibfn(char *fnname,
	  uint32_t *addr,   // Address in library
	  uint32_t *len)    // Len of function
{
     char buf[1024], *p;
     if (!fgets(buf, 1024, symsfp)) {
	  /* Out of data */
	  fclose(symsfp);
	  return -1;
     }
     p = strtok(buf, " ");
     strcpy(fnname, p);
     p = strtok(NULL, " ");
     *addr = strtoul(p, NULL, 16);
     p = strtok(NULL, " ");
     *len = strtoul(p, NULL, 16);
     return 0;
}

static kmem<x86Traits>
open_syms_file(const char *filename) {
     /* Each line names one function from the object: NAME, ADDR,
        LEN. */
     symsfp = fopen(filename, "r");
     if (!symsfp) {
	  fprintf(stderr, "Can't open %s for reading\n", filename);
	  exit(1);
     }
     return kmem<x86Traits>(kmem<x86Traits>::openKern);
}

static void
open_save_file(const char *filename) {
     time_t tt;
     savefp = fopen(filename, "w");
     if (!savefp) {
	  fprintf(stderr, "Can't open %s for writing\n", filename);
	  exit(1);
     }
     time(&tt);
     fprintf(savefp, "Save file created %s\n", ctime(&tt));
     fflush(savefp);
}

static void
save_failed_parse_info(const char *fnname,
		       uint32_t fnaddr,
		       uint32_t fnlen,
		       uint32_t badaddr) {
     if (!savefp)
	  return;
     fprintf(savefp, "%s %x %x PARSEERROR %x\n", fnname, fnaddr, fnlen, badaddr);
     fflush(savefp);
}

static void
save_jmptbl_info(const char *fnname,
		 uint32_t fnaddr,
		 uint32_t fnlen,
		 uint32_t badaddr) {
     if (!savefp)
	  return;
     fprintf(savefp, "%s %x %x JMPTBL %x\n", fnname, fnaddr, fnlen, badaddr);
     fflush(savefp);
}

static void
save_outofrange_info(const char *fnname,
		     uint32_t fnaddr,
		     uint32_t fnlen,
		     uint32_t badaddr) {
     if (!savefp)
	  return;
     fprintf(savefp, "%s %x %x OUTOFRANGE %x\n", fnname, fnaddr, fnlen, badaddr);
     fflush(savefp);
}

static void
save_parse_info(const char *fnname,
		uint32_t fnaddr,
		uint32_t fnlen,
		function<x86Traits> &fn) {
     if (!savefp)
	  return;

     fprintf(savefp,"%s 0x%x 0x%x\n", fnname, fnaddr, fnlen);
     
     // Print basic blocks
     unsigned bbCount = 0;
     for (function<x86Traits>::const_iterator bbiter = fn.begin(); bbiter != fn.end(); ++bbiter) {
	  const basicblock<x86Traits> &bb = *bbiter;
	  fprintf(savefp,
		  "%2d %x %x: ",
		  ++bbCount,
		  bb.getStartAddr(),
		  // FIXME: We added 1 byte to assist elisp interpretation of these
		  bb.getEndAddr() + 1);

	  const basicblock<x86Traits>::ConstPredIter pss = bb.beginPredIter();
	  const basicblock<x86Traits>::ConstPredIter pes = bb.endPredIter();
	  for (basicblock<x86Traits>::ConstPredIter iter = pss; iter != pes; ++iter) {
		    unsigned pred_id = *iter;
		    if (pred_id == basicblock<x86Traits>::ExitBB)
			 fprintf(savefp, "Exit ");
		    else
			 fprintf(savefp, "%d ", pred_id + 1);
	  }

	  fprintf(savefp, "| ");

	  const basicblock<x86Traits>::ConstSuccIter ss = bb.beginSuccIter();
	  const basicblock<x86Traits>::ConstSuccIter es = bb.endSuccIter();
	  for (basicblock<x86Traits>::ConstSuccIter iter = ss; iter != es; ++iter) {
		    unsigned succ_id = *iter;
		    if (succ_id == basicblock<x86Traits>::ExitBB)
			 fprintf(savefp, "Exit ");
		    else
			 fprintf(savefp, "%d ", succ_id + 1);
	  }
	  fprintf(savefp, "\n");
     }
     fflush(savefp);
}


static int
do_parse_fn(kmem<x86Traits> km,
	    const char *fnname,
	    uint32_t fnaddr,
	    uint32_t fnlen) {
	  cout << fnname << "()  [" << (void*)fnaddr
	       << " " << (void*)(fnaddr + fnlen)
	       << "]:" << endl << endl;

	  uint32_t currAddr = fnaddr;
	  uint32_t nextFnAddr = fnaddr + fnlen;

	  // Disassemble
	  while (currAddr < nextFnAddr) {
	       x86_instr i = km.getinsn(currAddr);
	       /* FIXME: The disassemble needs to know the PC
		  following this insn to get branch targets right. */
	       cout << (void*)currAddr << "    " <<  i.disassemble(currAddr + i.numBytes()) << endl;
	       currAddr += i.numBytes();
	  }
	  cout << endl;

	  // Parse
	  StaticString fnName(fnname);
	  function<x86Traits> fn(fnaddr, fnName);
	  dictionary_hash<uint32_t,bool> interProcJumps
	       = dictionary_hash<uint32_t,bool>(addrHash);

	  try {
	       fn.parse(nextFnAddr,
			km,
			interProcJumps);
	  } catch (function<x86Traits>::parsefailed e) {
	       /* Try to print the basic block info so we can debug it */
	       save_parse_info(fnname, fnaddr, fnlen, fn);
	       throw e;
	  }

	  // Save It
	  save_parse_info(fnname, fnaddr, fnlen, fn);

	  // Print basic blocks
	  unsigned bbCount = 0;
	  for (function<x86Traits>::const_iterator bbiter = fn.begin(); bbiter != fn.end(); ++bbiter) {
	       const basicblock<x86Traits> &bb = *bbiter;
	       const bool is_entry = (bbCount == fn.getEntryBB());
	       cout << "for basic block #" << ++bbCount << " of "
		    << fn.numBBs() << ":" << endl;
	       if (is_entry)
		    cout << "(The entry basic block)" << endl;
	       cout << "startAddr=" << (void*)bb.getStartAddr() << endl;
	       cout << "  endAddr=" << (void*)bb.getEndAddr() << endl; 
	       const basicblock<x86Traits>::ConstSuccIter ss = bb.beginSuccIter();
	       const basicblock<x86Traits>::ConstSuccIter es = bb.endSuccIter();
	       if (bb.numSuccessors() == 1 && *ss == basicblock<x86Traits>::ExitBB)
		    cout << "Successors(1): Exit" << endl;
	       else {
		    cout << "Successors(" << bb.numSuccessors() << "): ";
		    for (basicblock<x86Traits>::ConstSuccIter iter = ss; iter != es; ++iter) {
			 unsigned succ_id = *iter;
			 cout << succ_id + 1 << " ";
		    }
		    cout << endl;
	       }
	       cout << endl;
	  }

	  return 0;
}


int main(int argc, char *argv[]) {
     char fnname[64];
     uint32_t fnaddr;
     uint32_t fnlen;
     kmem<x86Traits> km;

     load_ia(); // Load the IA insn set
     if (argc < 2) {
	  fprintf(stderr, "Usage: %s SYMSFILE [SAVEFILE]\n", argv[0]);
	  exit(1);
     }
     try {
	  km = open_syms_file(argv[1]);
     } catch (kmem<x86Traits>::openfailedexception e) {
	  fprintf(stderr, "Can't open /dev/kmem\n");
	  exit(1);
     }

     if (argc > 2)
	  open_save_file(argv[2]);

     while (0 == nextlibfn(fnname, &fnaddr, &fnlen)) {
	  try {
	       km.fetch(fnaddr, fnlen);
	       do_parse_fn(km, fnname, fnaddr, fnlen);
	  } catch (kmem<x86Traits>::fetchfailedexception e) {
	       fprintf(stderr, "Fetch failure for address %x\n",
		       fnaddr, fnaddr + fnlen);
	       save_outofrange_info(fnname, fnaddr, fnlen, e.getAddr());
	  } catch (x86_instr::unknowninsn e) {
	       save_failed_parse_info(fnname, fnaddr, fnlen, e.getAddr());
	       cerr << "Parse error for " << fnname
		    << " [" << (void*)fnaddr << " " << (void*)(fnaddr+fnlen) << "] "
		    << " @ " << (void*)e.getAddr()
		    << endl;
	  } catch (function<x86Traits>::parsefailed e) {
	       cerr << "Jmp tbl exception (unhandled)" << endl;
#if 0
	       save_jmptbl_info(fnname, fnaddr, fnlen, e.getAddr());
	       cerr << "Jmp tbl for " << fnname
		    << " [" << (void*)fnaddr << " " << (void*)(fnaddr+fnlen) << "] "
		    << " @ " << (void*)e.getAddr()
		    << endl;
#endif
	  } 
     }
     return 0;
}
