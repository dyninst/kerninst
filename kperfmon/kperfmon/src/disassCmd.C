#include <inttypes.h> // uint64_t etc.
#include <tcl.h>
#include <stdlib.h>
#include <ctype.h>
#include "common/h/String.h"
#include "util/h/rope-utils.h"
#include "tkTools.h"
#include "kapi.h"

typedef uint16_t bbid_t;

extern kapi_manager kmgr;
extern Tcl_Interp *global_interp;
extern bool haveGUI; // main.C

static char printcharnicely(char c) {
   if (isalnum(c)) // uppercase char, lowercase char, or digit
      return c;
   else if (c==' ') // better than isspace(), which includes uses newline, tab, etc.
      return c;
//     else if (c=='!' || c=='@' || c == '#' || c == '$' || c == '%' ||
//              c=='^' || c=='&' || c=='*' || c=='(' || c==')' ||
//              c=='-' || c=='_' || c=='=' || c=='+' || c=='\\' || c=='|' ||
//              c=='`' || c=='~' || c=='[' || c==']' || c=='{' || c=='}' ||
//              c==':' || c==';' || c=='\'' || c=='\"' || c==',' || c=='<' ||
//              c=='.' || c=='>' || c=='/' || c=='?')
   else if (c=='!' || c=='@' || c == '#' || c == '$' || c == '%' ||
            c=='&' || c=='*' || c=='(' || c==')' ||
            c=='-' || c=='_' || c=='=' || c=='+' || 
            c=='{' || c=='}' ||
            c==':' || c==';' || c==',' ||
            c=='.')
      // better than using ispunct() or isprint(), which returns true for
      // too many control characters, in my opinion, resulting is bad cut & paste
      // situations
      return c;
   else
      return '.';
}

static pdstring instr2chars(const void *raw, unsigned num_bytes)
{
    const char *buffer = (const char *)raw;
    pdstring result;
    // do not print more than 4 chars -- will screw the alignment
    for (unsigned i=0; i<4 && i<num_bytes; i++) {
	result += pdstring(printcharnicely(buffer[i]));
    }
    return result;
}

void showDisassRange(Tcl_Interp *interp, const pdstring &wname,
		     bool include_ascii,
		     const kapi_disass_object &kdis)
{
    pdstring commandstr = wname + ".top.lab configure -text \"Disassembly\"";
    myTclEval(interp, commandstr, false);

    kapi_disass_object::const_iterator chunkiter = kdis.begin();
    kapi_disass_object::const_iterator chunkfinish = kdis.end();
    for (; chunkiter != chunkfinish; ++chunkiter) {
	const kapi_disass_chunk &theChunk = *chunkiter;
	kptr_t addr = theChunk.getStartAddr();

	kapi_disass_chunk::const_iterator insniter = theChunk.begin();
	kapi_disass_chunk::const_iterator insnfinish = theChunk.end();
	for (; insniter != insnfinish; insniter++) {
	    const kapi_disass_insn &insn = *insniter;

	    pdstring str = "address<" + addr2hex(addr);
	    str += "> \thex_code[";

            char buffer[40];
            const char *raw_bytes = (const char*) insn.getRaw();
            for(unsigned k=0; k<insn.getNumBytes(); k++) {
               sprintf(buffer+(k*2), "%02X", (unsigned char)raw_bytes[k]);
            }
            str += buffer;

            str += "] ";
            while(str.length() < 50) { // 50 is completely arbitrary
               str += " ";
            }

	    if (include_ascii) {
		str += instr2chars(insn.getRaw(), insn.getNumBytes());
	    }

	    str += pdstring(insn.getDisassembly());
            if (insn.hasDestFunc()) {
		str += " ";
	    }

	    pdstring commandstr = wname;
	    commandstr += ".body.txt insert end {";
	    commandstr += str;
	    commandstr += "}";
	    myTclEval(interp, commandstr, false);

	    if (insn.hasDestFunc()) {
		// we've got more to insert: a button containing the dest func
		// name, and then a ")"
		commandstr = wname;
		commandstr += ".body.txt window create end -create "
		    "{disassCreateCalleeButton ";
		commandstr += wname;
		commandstr += " ";
		commandstr += insn.getDestFuncInfo();
		commandstr += "} -stretch true";
		myTclEval(interp, commandstr, false);
	    }

	    commandstr = wname;
	    commandstr += ".body.txt insert end \"\n\"";
	    myTclEval(interp, commandstr, false);
	    
	    addr += insn.getNumBytes();
	}
    }
}

// Display the given disass_object on the console or in a tk window
void showDisassFunction(Tcl_Interp *interp,
			const pdstring &wname, // empty pdstring --> no GUI
			const kapi_module &kmod,
			const kapi_function &kfunc,
			bbid_t bbid, // (bbid_t)-1 --> no bb (whole fn)
			const kapi_disass_object &kdis,
                        bool useOrigImage)
{
    const unsigned maxlen = 255;
    char buffer[maxlen];
    
    if (kmod.getName(buffer, maxlen) < 0) {
	assert(false && "Module name is too long");
    }
    const pdstring modName(buffer);

    if (kfunc.getName(buffer, maxlen) < 0) {
	assert(false && "Function name is too long");
    }
    const pdstring fnname(buffer);

    // Function aliases, if any:
    if (haveGUI) {
	unsigned num_aliases = kfunc.getNumAliases();
	if (num_aliases > 0) {
	    pdstring commandstr = "disassMakeAliasesGUI ";
	    commandstr += wname;
	    commandstr += " {";
      
	    for (unsigned lcv=0; lcv < num_aliases; ++lcv) {
		if (kfunc.getAliasName(lcv, buffer, maxlen) < 0) {
		    assert(false && "Function alias is too long");
		}
		commandstr += buffer;
		if (lcv + 1 < num_aliases) {
		    commandstr += " ";
		}
	    }
	    commandstr += "}";

	    myTclEval(interp, commandstr);
	}
    }
   
    const bool basicBlock = (bbid != (bbid_t)-1);

    // List the basic blocks, if the entire function is being shown
    if (haveGUI) {
	if (!basicBlock) {
	    pdstring commandstr = "disassMakeBasicBlocksGUI ";
	    commandstr += wname;
	    commandstr += " \"";

	    kapi_vector<kapi_basic_block> allBlocks;
	    if (kfunc.getAllBasicBlocks(&allBlocks) < 0) {
		assert(false);
	    }
	    
            kapi_vector<kptr_t> allAddrs;
	    kapi_vector<kapi_basic_block>::const_iterator bbiter =
		allBlocks.begin();
	    for (; bbiter != allBlocks.end(); bbiter++) {
		allAddrs.push_back(bbiter->getStartAddr());
	    }
            std::sort(allAddrs.begin(), allAddrs.end());

            kapi_vector<kptr_t>::const_iterator addriter = allAddrs.begin();
            for(; addriter != allAddrs.end(); addriter++) {
               const pdstring bbaddr_hexstr = addr2hex(*addriter);
               commandstr += bbaddr_hexstr + " ";
            }

	    commandstr += "\"";
	    myTclEval(interp, commandstr, false);
	}
    }

    // Window "banner":
    if (!haveGUI) {
	cout << "Disassembly of " << modName << '/' << fnname
	     << ':' << endl;
    }
    else {
	pdstring commandstr = wname;
	commandstr += ".top.lab configure -text \"Disassembly ";
        commandstr += (useOrigImage? "(original) of " : "(current memory) of ");
	commandstr += modName;
	commandstr += "/";
	commandstr += fnname;

	if (basicBlock) {
	    commandstr += "/";
      
	    kapi_basic_block kblock;
	    if (kfunc.findBasicBlockById(bbid, &kblock) < 0) {
		assert(false && "Basic block not found");
	    }
	    commandstr += addr2hex(kblock.getStartAddr());
	}

	commandstr += "\"";
	myTclEval(interp, commandstr, false);
    }
   
    bool printedAnythingYet = false;

    unsigned linenum = 1; // tk text widget line numbers start at 1 (!)

    bool lastInsnWasEndOfBB = false;
    // similar to "is this instruction the start of a basic block", but allows
    // to correctly handle data without having 3 "+" buttons printed on the
    // same line...

    kapi_disass_object::const_iterator chunkiter = kdis.begin();
    kapi_disass_object::const_iterator chunkfinish = kdis.end();
    for (; chunkiter != chunkfinish; ++chunkiter) {
	if (chunkiter != kdis.begin()) {
	    pdstring newChunkBanner = "\n(start of chunk #";
	    newChunkBanner += num2string(chunkiter - kdis.begin());
	    newChunkBanner += ")\n";

	    if (!haveGUI)
		cout << newChunkBanner; // endl is already present in the string
	    else {
		pdstring commandstr = wname + ".body.txt insert end {" + newChunkBanner + "}";
		myTclEval(interp, commandstr, false);
	    }
	}

	const kapi_disass_chunk &theChunk = *chunkiter;

	kptr_t addr = chunkiter->getStartAddr();
      
	kapi_disass_chunk::const_iterator insniter = theChunk.begin();
	kapi_disass_chunk::const_iterator insnfinish = theChunk.end();
	for (; insniter != insnfinish; insniter++) {
	    bbid_t bb_id = kfunc.getBasicBlockIdByAddress(addr);
	    // (bbid_t)-1 if e.g. the fn is fried

	    if (lastInsnWasEndOfBB) {
		if (!haveGUI)
		    cout << endl;
		else {
		    pdstring tempcommandstr = wname;
		    tempcommandstr += ".body.txt insert end \"\\n\"";
		    myTclEval(interp, tempcommandstr, false);
		}
            
		++linenum;
	    }

	    printedAnythingYet = true;
      
	    pdstring str = "address<" + addr2hex(addr);
	    str += "> \thex_code[";

	    const kapi_disass_insn &insn = *insniter;

            char buffer[40];
            const char *raw_bytes = (const char*) insn.getRaw();
            for(unsigned k=0; k<insn.getNumBytes(); k++) {
               sprintf(buffer+(k*2), "%02X", (unsigned char)raw_bytes[k]);
            }
            str += buffer;

	    str += "] ";
            while(str.length() < 50) { // 50 is completely arbitrary
               str += " ";
            }

	    str += pdstring(insn.getDisassembly());

	    // Create a mark for register analysis, if appropriate.  We do
	    // a register analysis if bb_id is a legitimate value and we're
            // disassembling the original as parsed image.
	    const bool createMarkForRegAnalysis = (haveGUI &&
						   bb_id != (bbid_t)-1 &&
                                                   useOrigImage);
	    if (createMarkForRegAnalysis) {
		pdstring commandstr = "createRegAnalysisMark ";
		commandstr += wname + ".body.txt " + num2string(addr) + " 1 " +
		    num2string(linenum) + " 1 1";
		// "1" --> before insn, "1 1" --> yes to global analysis, yes to single-insn
		// stats.
		myTclEval(interp, commandstr, false);

		commandstr = "createRegAnalysisButton ";
		commandstr += wname + ".body.txt ";
		commandstr += num2string(addr); // no hex conversion (on purpose)
		commandstr += " 1 1"; // 1 --> before insn, 1 --> global analysis
		myTclEval(interp, commandstr, false);

		// Create a mark for individual instruction analysis:
		commandstr = "createRegAnalysisMark ";
		commandstr += wname + ".body.txt " + num2string(addr) + " 1 " +
		    num2string(linenum) + " 1 1";
		// "1" --> before insn, "0 1" --> no to global analysis, yes to single-insn
		// stats.
		myTclEval(interp, commandstr, false);
      
		commandstr = "createRegAnalysisButton ";
		commandstr += wname + ".body.txt ";
		commandstr += num2string(addr); // no hex conversion (on purpose)
		commandstr += " 1 0"; // 1 --> before insn, 0 --> single insn, not global analysis
		myTclEval(interp, commandstr, false);
	    }

	    if (!haveGUI)
		cout << str; // no endl yet
	    else {
		pdstring commandstr = wname;
		commandstr += ".body.txt insert end {";
		commandstr += str;
		commandstr += "}";
		myTclEval(interp, commandstr, false);
	    }

	    if (haveGUI && insn.hasDestFunc()) {
		// we've got more to insert: a button containing the dest func name, and
		// then a ")"
		pdstring commandstr = wname;
		commandstr += ".body.txt window create end -create {disassCreateCalleeButton ";
		commandstr += wname;
		commandstr += " ";
		commandstr += insn.getDestFuncInfo();
		commandstr += " } -stretch true";
		myTclEval(interp, commandstr, false);
	    }

	    if (!haveGUI)
		cout << endl;
	    else {
		pdstring commandstr = wname;
		commandstr += ".body.txt insert end \"\\n\"";
		myTclEval(interp, commandstr, false);
	    }

	    ++linenum;

	    // Marker ("+") for register analysis at end of bb
	    if (createMarkForRegAnalysis) {
		assert(bb_id != (bbid_t)-1);
		kapi_basic_block kbb;
		if (kfunc.findBasicBlockById(bb_id, &kbb) < 0) {
		    assert(false && "Basic block not found");
		}
		// stopped here
		const bool last_insn_in_bb = 
		    (addr == kbb.getEndAddrPlus1() - 4);

		if (last_insn_in_bb) {
		    pdstring commandstr = "createRegAnalysisMark ";
		    commandstr += wname + ".body.txt " +
			num2string(addr) + 
			" 0 " + // 0 --> ***after*** insn
			num2string(linenum) + " 1 0";
		    // " 1 0" --> yes for global analysis, no for thisInsnAnalysis
		    myTclEval(interp, commandstr, false);

		    myTclEval(interp, pdstring("createRegAnalysisButton ") +
			      wname + ".body.txt " + num2string(addr) + " 0 1",
			      // 0 --> ***after*** insn
			      // 1 --> global analysis, not a single insn usage
			      false);

		    lastInsnWasEndOfBB = true;
		}
		else
		    lastInsnWasEndOfBB = false;
	    }
	    else
		lastInsnWasEndOfBB = false;

	    addr += insn.getNumBytes();
	}
    }
}

// Disassemble kmod/kfunc/bbid and display it in interp. If useOrigImage
// is true, disassemble the original image (as if no instrumentation
// took place)
int disassAndDisplay(Tcl_Interp *interp, const kapi_module &kmod,
		     const kapi_function& kfunc, bbid_t bbid,
		     bool useOrigImage)
{
    kapi_disass_object kdis;
    if (kmgr.getDisassObject(kfunc, bbid, useOrigImage, &kdis) < 0) {
	assert(false);
    }
    
    pdstring wname;
    if (haveGUI) {
	myTclEval(interp, "disassMakeWindowCommon");
	// result: window name
	wname = Tcl_GetStringResult(interp);
    }
    
    showDisassFunction(interp, wname, kmod, kfunc, bbid, kdis, useOrigImage);
    return 0;
}

// Invoked from the "kperfmon>" prompt via the "disass" command
int disass_cmd(ClientData, Tcl_Interp *interp, int argc, TCLCONST char **argv) {
    // "disass [hex-addr]" or "disass [modname] [fnname]"
    if (argc != 2 && argc != 3) {
	Tcl_SetResult(interp, "disass: needs either 1 arg (hex addr) or "
		      "2 args (modname fnname)", TCL_STATIC);
	return TCL_ERROR;
    }
    
    kapi_module kmod;
    kapi_function kfunc;
   
    if (argc == 2) {
	const kptr_t addr = strtoul(argv[1], NULL, 16);
      
	if (kmgr.findModuleAndFunctionByAddress(addr, &kmod, &kfunc) < 0) {
	    // FIXME: original disass command did not require the address to
	    // be the entry address of a function
	    Tcl_SetResult(interp, "disass [addr]: could not find any function "
			  "with that addr", TCL_STATIC);
	    return TCL_ERROR;
	}
    }
    else if (argc == 3) {
	const char *modName = argv[1];
	if (kmgr.findModule(modName, &kmod) < 0) {
	    Tcl_SetResult(interp, "disass [modname] [fnname]: could not find "
			  "a module with that name", TCL_STATIC);
	    return TCL_ERROR;
	}

	const char *fnname = argv[2];
	if (kmod.findFunction(fnname, &kfunc) < 0) {
	    Tcl_SetResult(interp, "disass [modname] [fnname]: cannot find the "
			  "function specified or the function name is not "
			  "unique. Retry by specifying a function addr",
			  TCL_STATIC);
	    return TCL_ERROR;
	}
    }
    else {
	assert(false);
    }
    bbid_t bbid = (bbid_t)(-1);

    // Finally, do the disassembly
    if (disassAndDisplay(interp, kmod, kfunc, bbid, false) < 0) {
	assert(false);
    }
    return TCL_OK;
}

// Invoked when the user tries to disassemble the currently selected func
static int launchDisassFnFromMem(ClientData, Tcl_Interp *interp, int argc, 
                                 TCLCONST char **argv, bool useOrigImage)
{
    // args: modnum, fnaddr, bbid ((bbid_t)-1 if none)
    if (argc != 4) {
	Tcl_SetResult(interp, "incorrect # args", TCL_STATIC);
	return TCL_ERROR;
    }

    const char *modName = argv[1];
#ifdef ppc64_unknown_linux2_4
    kptr_t fnaddr = strtoull(argv[2], NULL, 0);
#else
    kptr_t fnaddr = strtoul(argv[2], NULL, 0);
#endif
    bbid_t bbid = atoi(argv[3]);

    kapi_module kmod;
    if (kmgr.findModule(modName, &kmod) < 0) {
	Tcl_SetResult(interp, "disass [modname] [fnname]: could not find "
		      "a module with that name", TCL_STATIC);
	return TCL_ERROR;
    }

    kapi_function kfunc;
    if (kmgr.findFunctionByAddress(fnaddr, &kfunc) < 0) {
	Tcl_SetResult(interp, "disass [modname] [fnname]: could not find "
		      "a function with that address", TCL_STATIC);
	return TCL_ERROR;
    }

    // Finally, do the disassembly
    if (disassAndDisplay(interp, kmod, kfunc, bbid, useOrigImage) < 0) {
	assert(false);
    }
    return TCL_OK;
}

// Invoked when the user tries to disassemble the current contents of a 
// selected function
int launchDisassFnFromMem_cmd(ClientData cd, Tcl_Interp *interp,
			      int argc, TCLCONST char **argv)
{
    return launchDisassFnFromMem(cd, interp, argc, argv, false);
}

// Invoked when the user tries to disassemble the original contents of a 
// selected function
int disassFnAsParsed_cmd(ClientData cd, Tcl_Interp *interp,
			 int argc, TCLCONST char **argv)
{
    return launchDisassFnFromMem(cd, interp, argc, argv, true);
}

// Invoked when the user tries to disassemble a callee of a function
// currently shown in the disassembly window
int launchDisassFnByAddrFromMem_cmd(ClientData, Tcl_Interp *interp,
				    int argc, TCLCONST char **argv) 
{
    // args: any addr w/in a function.  Must be in decimal, though.
    if (argc != 2) {
	Tcl_SetResult(interp, "incorrect # args", TCL_STATIC);
	return TCL_ERROR;
    }

    if (argv[1][0] == '0' && argv[1][1] == 'x') {
	Tcl_SetResult(interp, "launchDisassFnByAddrFromMem_cmd: arg must be"
		      " a decimal address; you passed hex", TCL_STATIC);
	return TCL_ERROR;
    }
   
    const kptr_t addr = strtoul(argv[1], NULL, 10);
    kapi_module kmod;
    kapi_function kfunc;
    if (kmgr.findModuleAndFunctionByAddress(addr, &kmod, &kfunc) < 0) {
	// FIXME: original disass command did not require the address to
	// be the entry address of a function
	Tcl_SetResult(interp, "disass [addr]: could not find any function "
		      "with that addr", TCL_STATIC);
	return TCL_ERROR;
    }
    bbid_t bbid = (bbid_t)(-1);

    // Finally, do the disassembly
    if (disassAndDisplay(interp, kmod, kfunc, bbid, false) < 0) {
	assert(false);
    }
    return TCL_OK;
}

// Invoked when the user tries to disassemble a range of memory addresses
int launchDisassRangeFromMem_cmd(ClientData, Tcl_Interp *interp,
				 int argc, TCLCONST char **argv) 
{
    // args: [0=kernel | 1=kerninstd], [decimal | hex], startaddr, endaddr,
    //       include_ascii [0 or 1]
    if (argc != 6) {
	Tcl_SetResult(interp, "incorrect # args" , TCL_STATIC);
	return TCL_ERROR;
    }

    const char *is_kernel_str = argv[1];
    const unsigned is_kernel_unsigned = atoi(is_kernel_str);
    if (is_kernel_unsigned > 1) {
	Tcl_SetResult(interp, "first arg must be 0 [kerninstd] or 1 [kernel]",
		      TCL_STATIC);
	return TCL_ERROR;
    }
    const bool inKernel = (is_kernel_unsigned == 1);

    const char *type_str = argv[2];
    bool hex;
    if (0 == strcmp(type_str, "hex"))
	hex = true;
    else if (0 == strcmp(type_str, "decimal"))
	hex = false;
    else {
	Tcl_SetResult(interp, "first arg must be \"decimal\" or \"hex\"" ,
		      TCL_STATIC);
	return TCL_ERROR;
    }
#ifdef ppc64_unknown_linux2_4   
    kptr_t startaddr = strtoull(argv[3], NULL, hex ? 16 : 10);
    kptr_t endaddr   = strtoull(argv[4], NULL, hex ? 16 : 10);
#else
    kptr_t startaddr = strtoul(argv[3], NULL, hex ? 16 : 10);
    kptr_t endaddr   = strtoul(argv[4], NULL, hex ? 16 : 10);
#endif
    if (endaddr <= startaddr) {
	Tcl_SetResult(interp, "disass range: endaddr must be > startaddr",
		      TCL_STATIC);
	return TCL_ERROR;
    }
    if (endaddr > (startaddr + 10000)) {
	Tcl_SetResult(interp, "disass range: presently limited to 10k",
		      TCL_STATIC);
	return TCL_ERROR;
    }

    int include_ascii_int = atoi(argv[5]);
    const bool include_ascii = (include_ascii_int == 1);
   
    if (inKernel) {
	// Obtain the disass object
	kapi_disass_object kdis;
	if (kmgr.getDisassObject(startaddr, endaddr, &kdis) < 0) {
	    assert(false);
	}
    
	// Create a Tk window
	pdstring wname;
	myTclEval(interp, "disassMakeWindowCommon");
	wname = Tcl_GetStringResult(interp);

	// Display the disassembly in a window
	showDisassRange(interp, wname, include_ascii, kdis);
	return TCL_OK;
    }
    else {
	Tcl_SetResult(interp, "disass range: disassembly of kerninstd "
		      "addresses is no longer supported", TCL_STATIC);
	return TCL_ERROR;
    }
}
