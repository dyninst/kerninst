// tclCommands.C

#include <inttypes.h> // uint64_t etc.
#include "tclCommands.h"
#include "allComplexMetricFocuses.h"
#include "allComplexMetrics.h"
#include "cmfSubscriber.h"
#include "visiSubscriber.h"
#include "tkTools.h"
#include "abstractions.h" // where axis & stuff
#include "kpmChildrenOracle.h"
#include "util/h/hashFns.h" // addrHash4()
#include "util/h/rope-utils.h" // num2string(), addr2hex()
#include "common/h/Ident.h"
#include "outliningMgr.h"
#include "instrument.h" // launchInstrumentFromCurrentSelections()

#include <ctype.h>

extern pdvector<focus> getCurrSelectedFoci(Tcl_Interp *); // instrument.C
extern pdvector<unsigned> instrumentFromCurrentSelections(cmfSubscriber &,
                                                        Tcl_Interp *interp);
extern dictionary_hash<unsigned, unsigned> moduleUniqueId2ModuleNum;
extern dictionary_hash<unsigned, unsigned> fnUniqueId2FnNumWithinModule;

extern Tcl_Interp *global_interp;

extern kapi_manager kmgr;

extern allComplexMetrics global_allMetrics;
extern abstractions<kpmChildrenOracle> *global_abstractions;

extern Tcl_TimerToken theTimerToken;

static int getNumMetrics_cmd(ClientData, Tcl_Interp *interp, int, TCLCONST char **) {
   char buffer[40];
   sprintf(buffer, "%u", global_allMetrics.size());
   Tcl_SetResult(interp, buffer, TCL_VOLATILE);
   
   return TCL_OK;
}

static int getCurrSelectedCode_cmd(ClientData, Tcl_Interp *interp,
                                   int, TCLCONST char **) {
   typedef uint16_t bbid_t;
   pdvector<focus> foci = getCurrSelectedFoci(interp);

   pdstring result;
   for (pdvector<focus>::const_iterator iter = foci.begin(); iter != foci.end(); iter++) {
      const focus &theFocus = *iter;

      pdstring buffer;
      if (theFocus.isRoot())
         buffer = "{}";
      else if (theFocus.getBBId() == (bbid_t)-1)
         buffer = pdstring("{\"") + theFocus.getModName() + "\" " +
            num2string(theFocus.getFnAddr()) + "}";
      else
         buffer = pdstring("{\"") + theFocus.getModName() + "\" " +
            num2string(theFocus.getFnAddr()) + " " +
            num2string(theFocus.getBBId()) + "}";
      
      result += buffer;
      result += " "; // space needed in case there will be other elements after this one
   }

   Tcl_SetResult(interp, const_cast<char*>(result.c_str()), TCL_VOLATILE);
   return TCL_OK;
}

static int reqRegAnalInfoForInsn_cmd(ClientData, Tcl_Interp *interp,
                                     int argc, TCLCONST char **argv) {
   // args: wname, addr, beforeInsnFlag, globalAnalysisFlag (is false, single insn)
   if (argc != 5) {
      Tcl_SetResult(interp, "incorrect # args", TCL_STATIC);
      return TCL_ERROR;
   }

   const char *wname = argv[1];
#ifdef ppc64_unknown_linux2_4
   const kptr_t addr = strtoull(argv[2], NULL, 10);
#else
   const kptr_t addr = strtoul(argv[2], NULL, 10);
#endif
   const bool beforeInsn = (atoi(argv[3]) != 0);
   const bool globalAnalysis = (atoi(argv[4]) != 0);

   if (!globalAnalysis && !beforeInsn) {
      Tcl_SetResult(interp, "reqRegAnalInfoForInsn_cmd: if single insn, then "
		    "beforeInsn must be true!", TCL_STATIC);
      return TCL_ERROR;
   }

   const unsigned maxBytes = 1024;
   char buffer[maxBytes];
   if (kmgr.getRegAnalysisResults(addr, beforeInsn, globalAnalysis,
				  buffer, maxBytes) < 0) {
       assert(false);
   }
   myTclEval(global_interp, pdstring("insertRegAnalInfoForInsn ") + wname +
	     pdstring(" ") + num2string(addr) + pdstring(" ") +
	     (beforeInsn ? pdstring("1 ") : pdstring("0 ")) +
	     (globalAnalysis ? pdstring("1") : pdstring("0")) + pdstring(" \"") +
	     pdstring(buffer) + pdstring("\n\""), false);

   return TCL_OK;
}

static void visi_igen_fd_callback(ClientData cd, int mask) {
   visiSubscriber *theVisi = (visiSubscriber *)cd;
   visualizationUser *igenHandle = theVisi->getIGenHandle();

   bool alreadyKilled = false;

   if (mask & TCL_EXCEPTION) {
      Tcl_DeleteFileHandler(theVisi->getIGenHandle_fd());
      fry1CmfSubscriberAndUninstrument(theVisi, false);
         // false --> kperfmon is not dying now
      alreadyKilled = true;
   }
   else if (mask & TCL_READABLE) {
      while (!alreadyKilled && igenHandle->buffered_requests()) {
         T_visi::message_tags ret = igenHandle->process_buffered();
         if (ret == T_visi::error) {
            Tcl_DeleteFileHandler(theVisi->getIGenHandle_fd());
            fry1CmfSubscriberAndUninstrument(theVisi, false);
               // false --> kperfmon is not dying now
            alreadyKilled = true;
            break;
         }
      }
      do {
         if (!alreadyKilled) {
            T_visi::message_tags ret = igenHandle->waitLoop();
            if (ret == T_visi::error) {
               Tcl_DeleteFileHandler(theVisi->getIGenHandle_fd());
               fry1CmfSubscriberAndUninstrument(theVisi, false);
                  // false --> kperfmon is not dying now
               alreadyKilled = true;
               break;
            }
         }
         else
            break;
      } while (!xdrrec_eof(igenHandle->net_obj()));
   }
   else if (mask & TCL_WRITABLE)
      cerr << "visi_igen_callback_proc: unexpected: writable" << endl;
}

static bool parse1FixedDescendant(Tcl_Interp * /*interp*/,
                                  char *the_fixed_descendant_string,
                                  std::pair<pdstring, kapi_function> *info)
{
   // the_fixed_descendant_string will be of the form modName/fnName
   const char *str = strchr(the_fixed_descendant_string, '/');
   if (str == NULL)
      return false;
   
   const pdstring modName(the_fixed_descendant_string, str - the_fixed_descendant_string);
   const pdstring fnName(str+1);

   kapi_module kmod;
   if (kmgr.findModule(modName.c_str(), &kmod) < 0) {
       cout << "Module not found\n";
       return false;
   }

   kapi_function kfunc;
   if (kmod.findFunction(fnName.c_str(), &kfunc) < 0) {
       cout << "Function not found or ambiguous\n";
       return false;
   }
   info->first = modName;
   info->second = kfunc;

   return true;
}

static int stringListToFunctionVector(
    Tcl_Interp *interp, const char *stringList,
    pdvector< std::pair<pdstring, kapi_function> > *funcVector)
{
    int num_strings;
    char **the_strings;
    int rv;

    if ((rv = Tcl_SplitList(interp, stringList,
			    &num_strings, 
                            (TCLCONST char***)&the_strings)) != TCL_OK) {
	return rv;
    }

    char **the_strings_iter = the_strings;
    while (num_strings--) {
	char *funcName = *the_strings_iter++;
            
	std::pair<pdstring, kapi_function> func;
	if (!parse1FixedDescendant(interp, funcName, &func)) {
	    cout << "Sorry, could not parse function: ";
	    cout << funcName << endl;

	    Tcl_SetResult(interp, "outline: could not understand fixed "
			  "descendants setting for this root function.",
			  TCL_STATIC);
               
	    return TCL_ERROR;
	}

	funcVector->push_back(func);
    }

    Tcl_Free((char*)the_strings);
    // NOT the_fixed_descendant_strings_iter

    return TCL_OK;
}

#ifdef sparc_sun_solaris2_7

static int outline_cmd(ClientData, Tcl_Interp *interp, int argc, TCLCONST char **argv) {
   // args: outliningMethod modname fnname
   // If specified, we'll make use of fixedOutlineDescendants("$modname/$fnname"),
   // which must be a list of functions that looks like this:
   // {unix/mutex_enter genunix/.urem}

   if (argc != 3) {
      Tcl_SetResult(interp, "outline: incorrect # args.  Must be: modname fnname",
                    TCL_STATIC);
      return TCL_ERROR;
   }

   // asynchronously start an outline
   extern outliningMgr *theOutliningMgr;
   const pdstring modname(argv[1]);
   const char *fnname = argv[2];
   
   kapi_module kmod;
   if (kmgr.findModule(modname.c_str(), &kmod) < 0) {
       Tcl_SetResult(interp, "outline: could not find that module",TCL_STATIC);
       return TCL_ERROR;
   }
   
   kapi_function kfunc;
   if (kmod.findFunction(fnname, &kfunc) < 0) {
       Tcl_SetResult(interp, "outline: found that module, but not that "
		     "function (or the function name is not unique in which "
		     "case specify a specific function addr instead)",
		     TCL_STATIC);
       return TCL_ERROR;
   }
   const kptr_t fnEntryAddr = kfunc.getEntryAddr();
   
   // NOTE: When gathering fixed descendants, be sure to use the
   // "getOutlinedFnOriginalFn[]" level of indirection, so things work as
   // expected when outlining an already-outlined group.
   kptr_t origFnAddr = kmgr.getOutlinedFnOriginalFn(fnEntryAddr);
   kapi_module origMod;
   kapi_function origFunc;
   
   if (kmgr.findModuleAndFunctionByAddress(origFnAddr,
					   &origMod, &origFunc) < 0) {
       assert(false && "Function not found");
   }
   const unsigned maxlen = 255;
   char buffer[maxlen];
   if (origMod.getName(buffer, maxlen) < 0) {
       assert(false && "Module name is too long");
   }
   pdstring origModName(buffer);

   if (origFunc.getName(buffer, maxlen) < 0) {
       assert(false && "Function name is too long");
   }
   pdstring origFuncName(buffer);

   const pdstring canonicalOrigModAndOrigFnName =
       origModName + "/" + origFuncName;

   const char *fixed_descendants_str = Tcl_GetVar2(interp, "fixedOutlineDescendants",
                                                   const_cast<char*>(canonicalOrigModAndOrigFnName.c_str()),
                                                   TCL_GLOBAL_ONLY);
   
   pdvector< std::pair<pdstring, kapi_function> > fixedDescendants; // if empty, not fixed
   if (fixed_descendants_str) {
      int rv = stringListToFunctionVector(interp, fixed_descendants_str,
					  &fixedDescendants);
      if (rv != TCL_OK) {
	  return rv;
      }

      cout << "For outlining of " << modname << '/' << fnname
           << ", we have the following fixed descendants:" << endl;
      if (fixedDescendants.size() == 0)
         cout << "   [none!]" << endl;
      
      pdvector< std::pair<pdstring, kapi_function> >::const_iterator iter =
	  fixedDescendants.begin();
      pdvector< std::pair<pdstring, kapi_function> >::const_iterator finish =
	  fixedDescendants.end();
      for (; iter != finish; ++iter) {
	  const std::pair<pdstring, kapi_function> &info = *iter;
	  if (info.second.getName(buffer, maxlen) < 0) {
	      assert(false && "Function name is too long");
	  }
	  cout << info.first << "/" << buffer << " ";
      }
      cout << endl;
   }
   
   const char *force_include_descendants_str =
      Tcl_GetVar2(interp, "forceIncludeOutlineDescendants",
                  const_cast<char*>(canonicalOrigModAndOrigFnName.c_str()),
                  TCL_GLOBAL_ONLY);

   pdvector< std::pair<pdstring, kapi_function> > forceIncludeDescendants; // could be empty
   if (force_include_descendants_str) {
      int rv = stringListToFunctionVector(interp,
					  force_include_descendants_str,
					  &forceIncludeDescendants);
      if (rv != TCL_OK) {
	  return rv;
      }

      cout << "Using the following force-include-descendants:" << endl;
      if (forceIncludeDescendants.size() == 0)
         cout << "   [none!]" << endl;
      
      pdvector< std::pair<pdstring, kapi_function> >::const_iterator iter = 
         forceIncludeDescendants.begin();
      pdvector< std::pair<pdstring, kapi_function> >::const_iterator finish = 
         forceIncludeDescendants.end();
      for (; iter != finish; ++iter) {
	  const std::pair<pdstring, kapi_function> &info = *iter;
	  if (info.second.getName(buffer, maxlen) < 0) {
	      assert(false && "Function name is too long");
	  }
	  cout << info.first << "/" << buffer << " ";
      }
      cout << endl;
   }

   const char *force_exclude_descendants_str =
       Tcl_GetVar(interp, "forceExcludeOutlineDescendants",
		  TCL_GLOBAL_ONLY);

   pdvector< std::pair<pdstring, kapi_function> > forceExcludeDescendants;
   if (force_exclude_descendants_str) {
      int rv = stringListToFunctionVector(interp,
					  force_exclude_descendants_str,
					  &forceExcludeDescendants);
      if (rv != TCL_OK) {
	  return rv;
      }

      cout << "Using the following force-exclude-descendants:" << endl;
      if (forceExcludeDescendants.size() == 0)
         cout << "   [none!]" << endl;
      
      pdvector< std::pair<pdstring, kapi_function> >::const_iterator iter = 
	  forceExcludeDescendants.begin();
      pdvector< std::pair<pdstring, kapi_function> >::const_iterator finish = 
	  forceExcludeDescendants.end();
      for (; iter != finish; ++iter) {
	  const std::pair<pdstring, kapi_function> &info = *iter;
	  if (info.second.getName(buffer, maxlen) < 0) {
	      assert(false && "Function name is too long");
	  }
	  cout << info.first << "/" << buffer << " ";
      }
      cout << endl;
   }

   theOutliningMgr->asyncOutlineFn(modname, kfunc, fixedDescendants,
                                   forceIncludeDescendants,
				   forceExcludeDescendants);

   return TCL_OK;
}

// Outline starting from a profile stored in the argv[1] file name
static int outlineUsingProfile_cmd(ClientData, Tcl_Interp *interp,
				   int argc, TCLCONST char **argv) 
{
    // args: file name with the profile info

    if (argc != 2) {
	Tcl_SetResult(interp, "outlineUsingProfile: incorrect # args."
		      "Must be: profile-file-name", TCL_STATIC);
	return TCL_ERROR;
    }

    const char *filename = argv[1];

   // asynchronously start an outline
    extern outliningMgr *theOutliningMgr;
   
    theOutliningMgr->asyncOutlineUsingProfile(filename);
   
    return TCL_OK;
}

static int unOutline_cmd(ClientData, Tcl_Interp *interp, int argc, TCLCONST char **argv) {
   // args: modname fnname
   if (argc != 3) {
      Tcl_SetResult(interp, "outline: incorrect # args.  Must be: modname fnname",
                    TCL_STATIC);
      return TCL_ERROR;
   }

   const char *modname = argv[1];
   const char *fnname = argv[2];

   kapi_module kmod;
   if (kmgr.findModule(modname, &kmod) < 0) {
       Tcl_SetResult(interp, "outline: could not find module", TCL_STATIC);
       return TCL_ERROR;
   }

   kapi_function kfunc;
   if (kmod.findFunction(fnname, &kfunc) < 0) {
       Tcl_SetResult(interp, "unOutline: found that module, but not that "
		     "function (or function name is ambiguous in which case, "
		     "retry by specifying a specific function addr instead)",
		     TCL_STATIC);
       return TCL_ERROR;
   }
   extern outliningMgr *theOutliningMgr;
   assert(theOutliningMgr != 0);
   theOutliningMgr->unOutlineGroup(modname, kfunc);

   return TCL_OK;
}

static bool parseOneCodeRangeSubArg(Tcl_Interp *interp, const char *istr,
                                    pdvector< std::pair<kptr_t, unsigned> > &result) {
   // appends to result and returns true iff successful.
   // "istr" should be a two-elem tcl list: startaddr (in hex) and nbytes (decimal).

   int num;
   char **strings;
   if (Tcl_SplitList(interp, istr, &num, (const char***)&strings) != TCL_OK 
       || num != 2)
      return false;

   char *str1 = strings[0];
   char *str2 = strings[1];
   kptr_t startAddr = strtoul(str1, NULL, 16);
   unsigned nbytes = strtoul(str2, NULL, 10);
   Tcl_Free((char*)strings);

   if (startAddr == 0 || nbytes == 0)
      return false;

   result += std::make_pair(startAddr, nbytes);

   return true;
}

static pdvector< std::pair<kptr_t, unsigned> >
parseCodeRangeArg(Tcl_Interp *interp, const char *istr) {
   static const pdvector< std::pair<kptr_t, unsigned> > error_result;
   pdvector< std::pair<kptr_t, unsigned> > result;

   int num_chunks;
   char **strings;
   
   if (Tcl_SplitList(interp, istr, &num_chunks, (const char***)&strings) != TCL_OK)
      return error_result;

   char **str_ptr = strings;
   while (num_chunks--) {
      const char *str = *str_ptr++;
      
      // "str" is itself a two-element tcl list
      if (!parseOneCodeRangeSubArg(interp, str, result))
         return error_result;
   }

   Tcl_Free((char*)strings);
   return result;
}

static int unParseFn_cmd(ClientData, Tcl_Interp *interp, int argc, TCLCONST char **argv) {
   // args:
   // 1) modname
   // 2) fnName

   if (argc != 3) {
      Tcl_SetResult(interp, "incorrect #args, must be: modname fnName", TCL_STATIC);
      return TCL_ERROR;
   }

   const pdstring modName(argv[1]);
   const pdstring fnName(argv[2]);

   assert(false && "sorry, unParseFn_cmd() is not yet implemented");

   return TCL_ERROR;
}

static int parseNewFn_cmd(ClientData, Tcl_Interp *interp, int argc, TCLCONST char **argv) {
   // args:
   // 1) modname,
   // 2) moddescription (used if module is new),
   // 3) functionName
   // 4) range of addrs (a tcl list of pairs, each pair being two
   //    numbers: startaddr (hex)/nbytes (decimal, not hex))
   //    For example:   "{ {0x10234534 40} {0x10400000 60} }"
   //    for a two-chunk range, or "{ {0x12345678 20} }" for a one-chunk range.
   // 5) index (decimal): which of the above lindex's is the entry point one
   //    (in the above example, this param would have to be either 0 or 1)
   // 6) index (decimal): which of the above lindex's contains useful data that
   //    should not be discarded after parsing shows that no basic blocks fall within it
   //    (make it -1 if none)

   if (argc != 7) {
      Tcl_SetResult(interp, "incorrect # args, must be: modname, moddescription, fnname, addrRange (a tcl list, each element being a tcl list of 2 elems: hex startaddr and decimal numbytes), entryIndex, dataIndex", TCL_STATIC);
      return TCL_ERROR;
   }

   const char *modName = argv[1];
   const char *modDescriptionIfNew = argv[2];
   const char *fnName = argv[3];

   pdvector< std::pair<kptr_t, unsigned> > codeRanges = parseCodeRangeArg(interp, argv[4]);
   unsigned numChunks = codeRanges.size();
   if (numChunks == 0) {
      Tcl_SetResult(interp, "args must be: modname, moddescription, fnname, addrRange (a tcl list, each element being a tcl list of 2 elems: hex startaddr and decimal numbytes), entryIndex, dataIndex.\nThe addrRange arg did not parse correctly.", TCL_STATIC);
      return TCL_ERROR;
   }
   kapi_vector<kptr_t> chunkStarts;
   kapi_vector<unsigned> chunkSizes;
   pdvector< pair<kptr_t, unsigned> >::const_iterator irange =
       codeRanges.begin();
   for (; irange != codeRanges.end(); irange++) {
       chunkStarts.push_back(irange->first);
       chunkSizes.push_back(irange->second);
   }
   
   const unsigned entry_ndx = atoi(argv[5]);
   if (entry_ndx >= numChunks) {
      Tcl_SetResult(interp, "args must be: modname, moddescription, fnname, addrRange (a tcl list, each element being a tcl list of 2 elems: hex startaddr and decimal numbytes), entryIndex, dataIndex.\nThe entryIndex did not parse correctly; make sure it is within range", TCL_STATIC);
      return TCL_ERROR;
   }

   const unsigned data_ndx = (unsigned)atoi(argv[6]); // may be -1
   if (data_ndx != -1U && data_ndx >= numChunks) {
      Tcl_SetResult(interp, "args must be: modname, moddescription, fnname, addrRange (a tcl list, each element being a tcl list of 2 elems: hex startaddr and decimal numbytes), entryIndex, dataIndex.\nThe dataIndex did not parse correctly; make sure it is within range, or that it is -1", TCL_STATIC);
      return TCL_ERROR;
   }

   // We only have the ability to parse a single function, for now at least...
   if (kmgr.parseNewFn(modName, modDescriptionIfNew, fnName,
		       chunkStarts, chunkSizes,
		       entry_ndx, data_ndx) < 0) {
       Tcl_SetResult(interp, "Internal error", TCL_STATIC);
       return TCL_ERROR;
   }

   return TCL_OK;
}

#endif // sparc_sun_solaris2_7

static int newVisi_cmd(ClientData, Tcl_Interp *interp,
                       int argc, TCLCONST char **argv) {
   // arguments: visi name (must be in search path)
   if (argc != 2) {
      Tcl_SetResult(interp, "incorrect # args", TCL_STATIC);
      return TCL_ERROR;
   }

   const char *visiProgramName = argv[1];

   // launch visi:
   pdvector<pdstring> visi_args;
   visi_args += pdstring("--paradyn"); // visis only run when passed this
   int igenfd = RPCprocessCreate("", // local host
                                 "", // user name
                                 visiProgramName, // cmd name
                                 "", // remote shell to use
                                 visi_args);
   if (igenfd < 0) {
      cerr << "failed to launch visi \"" << visiProgramName << "\"";
      Tcl_SetResult(interp, "failed to launch visi", TCL_STATIC);
      return TCL_ERROR;
   }

   const unsigned mhz = kmgr.getSystemClockMHZ();
   const unsigned mhz1000 = mhz * 1000;

   extern uint64_t millisecs2cycles(unsigned, unsigned mhzTimes1000);
   visiSubscriber *theVisi =
      new visiSubscriber(igenfd,kmgr.getKerninstdMachineTime(),
                         mhz, millisecs2cycles(200, mhz1000));

   // adds to static dictionary of all cmfSubscribers.
   // 2d param is the "genesis" time of the visi process (on the kerninstd machine)

   if (!theVisi->igenHandleExists()) {
      // igen startup must have failed!
      Tcl_SetResult(interp, "igen startup failed", TCL_STATIC);
      return TCL_ERROR;
   }
   
   assert(theVisi->igenHandleExists());

   // Set up file handler for the igen connection with this visi
   Tcl_CreateFileHandler(theVisi->getIGenHandle_fd(),
                         TCL_READABLE | TCL_EXCEPTION,
                         visi_igen_fd_callback, // in this file
                         (ClientData)theVisi);

   pdstring result_string = num2string(theVisi->getId());
   Tcl_SetResult(interp, const_cast<char*>(result_string.c_str()), TCL_VOLATILE);
   return TCL_OK;
}

static int closeVisi_cmd(ClientData, Tcl_Interp *interp,
                         int argc, TCLCONST char **argv) {
   // argument: visi id
   if (argc != 2) {
      Tcl_SetResult(interp, "incorrect # args", TCL_STATIC);
      return TCL_ERROR;
   }

   unsigned visi_id = atoi(argv[1]);

   cmfSubscriber *theCmfSubscriber = cmfSubscriber::findCmfSubscriber(visi_id);
   if (theCmfSubscriber == NULL) {
      Tcl_SetResult(interp, "failed to close down visi; couldn't find the visi",
                    TCL_STATIC);
      return TCL_ERROR;
   }

   visiSubscriber *theVisi = dynamic_cast<visiSubscriber*>(theCmfSubscriber);
   if (theVisi == NULL)
      // not a visi, some other kind of cmfSubscriber.  unexpected.
      assert(false);
   
   if (!theVisi->igenHandleExists()) {
      Tcl_SetResult(interp, "Failed to close down visi: igenHandle NULL (already closed down or never got set up)", TCL_STATIC);
      return TCL_ERROR;
   }

   Tcl_DeleteFileHandler(theVisi->getIGenHandle_fd());

   fry1CmfSubscriberAndUninstrument(theVisi, false);
      // false --> kperfmon is not dying now

   return TCL_OK;
}

/* ************************************************************ */

#ifdef sparc_sun_solaris2_7

static char *outliningDoReplaceFunctionHasChanged_proc(ClientData, 
						       Tcl_Interp *interp,
                                                       const char *, 
						       const char *,
                                                       int /*flags*/) {
   const char *str = Tcl_GetVar(interp, "outliningDoReplaceFunction", TCL_GLOBAL_ONLY);
   assert(str);
   
   const int val_int = atoi(str);
   const bool val_bool = val_int;

   extern outliningMgr *theOutliningMgr;
   theOutliningMgr->changeOutliningDoTheReplaceFunctionFlag(val_bool);

   cout << "outliningDoReplaceFunction is now: "
        << theOutliningMgr->getOutliningDoTheReplaceFunctionFlag() << endl;
   
   return NULL;
}

static char *
outliningReplaceFunctionPatchUpCallSitesTooHasChanged_proc(ClientData,
                                                           Tcl_Interp *interp,
                                                           const char *, 
							   const char *,
                                                           int /*flags*/) {
   const char *str = Tcl_GetVar(interp, "outliningReplaceFunctionPatchUpCallSitesToo",
                                TCL_GLOBAL_ONLY);
   assert(str);
   
   const int val_int = atoi(str);
   const bool val_bool = val_int;

   extern outliningMgr *theOutliningMgr;
   theOutliningMgr->changeOutliningReplaceFunctionPatchUpCallSitesTooFlag(val_bool);

   cout << "outliningReplaceFunctionPatchUpCallSitesToo is now: "
        << theOutliningMgr->getOutliningReplaceFunctionPatchUpCallSitesTooFlag()
        << endl;
   
   return NULL;
}

static char *outliningBlockCountDelayHasChanged_proc(ClientData, 
						     Tcl_Interp *interp,
                                                     const char *, 
						     const char *,
                                                     int /*flags*/) {
   const char *str = Tcl_GetVar(interp, "outliningBlockCountDelay", TCL_GLOBAL_ONLY);
   assert(str);
   
   const int val_int = atoi(str);

   extern outliningMgr *theOutliningMgr;
   theOutliningMgr->changeOutliningBlockCountDelay(val_int);
   
   cout << "outliningBlockCountDelay is now: "
        << theOutliningMgr->getOutliningBlockCountDelay() << " msecs" << endl;
   
   return NULL;
}

static char *outlineChildrenTooHasChanged_proc(ClientData, Tcl_Interp *interp,
                                               const char *, const char *,
                                               int /*flags*/) {
   const char *str = Tcl_GetVar(interp, "outlineChildrenToo", TCL_GLOBAL_ONLY);
   assert(str);
   
   const int val_int = atoi(str);
   const bool val_bool = val_int;

   extern outliningMgr *theOutliningMgr;
   theOutliningMgr->changeOutlineChildrenTooFlag(val_bool);
   
   cout << "outlineChildrenToo flag is now: "
        << theOutliningMgr->getOutlineChildrenTooFlag() << endl;
   
   return NULL;
}

static char *forceOutliningOfAllChildrenHasChanged_proc(ClientData, 
							Tcl_Interp *interp,
                                                        const char *, 
							const char *,
                                                        int /*flags*/) {
   const char *str = Tcl_GetVar(interp, "forceOutliningOfAllChildren",
                                TCL_GLOBAL_ONLY);
   assert(str);
   
   const int val_int = atoi(str);
   const bool val_bool = val_int;

   extern outliningMgr *theOutliningMgr;
   theOutliningMgr->changeForceOutliningOfAllChildrenFlag(val_bool);
   
   cout << "forceOutliningOfAllChildren flag is now: "
        << theOutliningMgr->getForceOutliningOfAllChildrenFlag() << endl;
   
   return NULL;
}

static char *outliningBlockPlacementHasChanged_proc(ClientData, 
						    Tcl_Interp *interp,
                                                    const char *, const char *,
                                                    int /*flags*/) {
   const char *str = Tcl_GetVar(interp, "outliningBlockPlacement",
                                TCL_GLOBAL_ONLY);
   assert(str);

   extern outliningMgr *theOutliningMgr;
   
   if (0==strcmp(str, "origSeq")) {
      theOutliningMgr->changeOutliningBlockPlacementToOrigSeq();

      cout << "changed outliningBlockPlacement to origSeq OK" << endl;
   }
   else if (0==strcmp(str, "chains")) {
      theOutliningMgr->changeOutliningBlockPlacementToChains();

      cout << "changed outliningBlockPlacement to chains OK" << endl;
   }
   else {
      cout << "unrecognized outliningBlockPlacement setting...ignoring"
           << endl;
      cout << "   (valid values are \"origSeq\" or \"chains\")" << endl;
   }

   return NULL;
}

static char *outliningUsingFixedDescendantsHasChanged_proc(ClientData,
                                                           Tcl_Interp *interp,
                                                           const char *, 
							   const char *,
                                                           int /*flags*/) {
   const char *str = Tcl_GetVar(interp, "outliningUsingFixedDescendants",
                                TCL_GLOBAL_ONLY);
   assert(str);

   extern outliningMgr *theOutliningMgr;
   
   const int val_int = atoi(str);
   const bool val_bool = val_int;

   extern outliningMgr *theOutliningMgr;
   theOutliningMgr->changeUsingFixedDescendantsFlag(val_bool);
   
   cout << "outliningUsingFixedDescendants flag is now: "
        << theOutliningMgr->getUsingFixedDescendantsFlag() << endl;
   
   return NULL;
}

static char *
outliningHotBlockThresholdChoiceHasChanged_proc(ClientData,
                                                Tcl_Interp *interp,
                                                const char *, const char *,
                                                int /*flags*/) {
   const char *str = Tcl_GetVar(interp, "outliningHotBlockThresholdChoice",
                                TCL_GLOBAL_ONLY);
   assert(str);

   extern outliningMgr *theOutliningMgr;

   if (0==strcmp(str, "FivePercent"))
      theOutliningMgr->changeOutliningHotBlocksThreshold(kapi_manager::
							 FivePercent);
   else if (0==strcmp(str, "AnyNonZeroBlock"))
      theOutliningMgr->changeOutliningHotBlocksThreshold(kapi_manager::
							 AnyNonZeroBlock);
   else {
      cout << "unrecognized outliningHotBlockThresholdChoice setting...ignoring"
           << endl;
      cout << "   (valid values are \"xxx\" or \"xxx\")" << endl;
   }

   return NULL;
}

void traceOutliningParameters()
{
    // Trace all changes to them
    Tcl_TraceVar(global_interp, "outliningDoReplaceFunction",
		 TCL_TRACE_WRITES | TCL_GLOBAL_ONLY,
		 outliningDoReplaceFunctionHasChanged_proc,
		 NULL);
    Tcl_TraceVar(global_interp, "outliningReplaceFunctionPatchUpCallSitesToo",
		 TCL_TRACE_WRITES | TCL_GLOBAL_ONLY,
		 outliningReplaceFunctionPatchUpCallSitesTooHasChanged_proc,
		 NULL);
    Tcl_TraceVar(global_interp, "outliningBlockCountDelay",
		 TCL_TRACE_WRITES | TCL_GLOBAL_ONLY,
		 outliningBlockCountDelayHasChanged_proc,
		 NULL);
    Tcl_TraceVar(global_interp, "outlineChildrenToo",
		 TCL_TRACE_WRITES | TCL_GLOBAL_ONLY,
		 outlineChildrenTooHasChanged_proc,
		 NULL);
    Tcl_TraceVar(global_interp, "forceOutliningOfAllChildren",
		 TCL_TRACE_WRITES | TCL_GLOBAL_ONLY,
		 forceOutliningOfAllChildrenHasChanged_proc,
		 NULL);
    Tcl_TraceVar(global_interp, "outliningBlockPlacement",
		 TCL_TRACE_WRITES | TCL_GLOBAL_ONLY,
		 outliningBlockPlacementHasChanged_proc,
		 NULL);
    Tcl_TraceVar(global_interp, "outliningUsingFixedDescendants",
		 TCL_TRACE_WRITES | TCL_GLOBAL_ONLY,
		 outliningUsingFixedDescendantsHasChanged_proc,
		 NULL);
    Tcl_TraceVar(global_interp, "outliningHotBlockThresholdChoice",
		 TCL_TRACE_WRITES | TCL_GLOBAL_ONLY,
		 outliningHotBlockThresholdChoiceHasChanged_proc,
		 NULL);
}

#endif // sparc_sun_solaris2_7

static bool haveSeenFirstGoodWhereAxisWid = false;

static bool tryFirstGoodWhereAxisWid(Tcl_Interp *interp) {
   // Tk_WindowId() returns 0 until the tk window has been mapped, which takes quite
   // a bit oof time.  Hence, this is needed.  Returns true iff it's okay to start
   // drawing in the tk window.

   if (haveSeenFirstGoodWhereAxisWid)
      return true;

   Tk_Window topLevelTkWindow = Tk_MainWindow(interp);
   Tk_Window theTkWindow = Tk_NameToWindow(interp,
                                           ".spec.right.whereAxis.code.mid.body", 
                                           topLevelTkWindow);
   assert(theTkWindow);
   
   if (Tk_WindowId(theTkWindow) == 0)
      return false;
   
   // Finally, it's ok to start drawing in that window
   haveSeenFirstGoodWhereAxisWid = true;

   const char *whereAxisRootItemFontName = Tcl_GetVar(interp,
                                                      "whereAxisRootItemFontName",
                                                      TCL_GLOBAL_ONLY);
   assert(whereAxisRootItemFontName);
   
   const char *whereAxisListboxItemFontName = Tcl_GetVar(interp,
                                                         "whereAxisListboxItemFontName",
                                                         TCL_GLOBAL_ONLY);
   assert(whereAxisListboxItemFontName);

   assert(NULL == global_abstractions);
   global_abstractions = new abstractions<kpmChildrenOracle>
      ("", // no abstraction menu
       "", // no navigate menu
       ".spec.right.whereAxis.code.bot.sb",
       ".spec.right.whereAxis.code.mid.sb",
       "", // find window
       interp,
       theTkWindow,
       whereAxisRootItemFontName, whereAxisListboxItemFontName);
   assert(global_abstractions);

   whereAxis<kpmChildrenOracle> *theWhereAxis =
          new whereAxis<kpmChildrenOracle>(interp, theTkWindow,
                                           "root",
                                           ".spec.right.whereAxis.code.bot.sb", // horiz
                                           ".spec.right.whereAxis.code.mid.sb", // vert
                                           "", // no navigate menu
                                           whereAxisRootItemFontName,
                                           whereAxisListboxItemFontName);
   assert(theWhereAxis);

   // add items to the where axis.  The root item, by definition, has a unique
   // resource id of 0.

   theWhereAxis->addItem("Code",
                         0, // parent uniqueid
                         1, // uniqueid of this node
                         false, false, false);
   extern unsigned codeWhereAxisId; // instrument.C
   codeWhereAxisId = 1;
   
   theWhereAxis->recursiveDoneAddingChildren(true); // true --> resort
   
   global_abstractions->add(theWhereAxis, "whereAxisName");

   // afterFirstGoodTkWid(); // Done in kapi_manager::attach

   return true;
}

/* ************************************************************ */

static void whereAxisDrawWhenIdleRoutine(ClientData cd); // prototype
static tclInstallIdle whereAxisDrawWhenIdle(&whereAxisDrawWhenIdleRoutine);

static void whereAxisDrawWhenIdleRoutine(ClientData cd) {
   assert(haveSeenFirstGoodWhereAxisWid);
   const bool doubleBuffer = (bool)cd;
   assert(doubleBuffer == true || doubleBuffer == false);
   const bool xsyncOn = false;

   assert(global_abstractions);
   const bool needsRedrawing = global_abstractions->drawCurrent(doubleBuffer, xsyncOn);
   if (needsRedrawing) {
      cout << "whereAxisDrawWhenIdleRoutine: needs redrawing, so scheduling such" 
           << endl;
      whereAxisDrawWhenIdle.install(cd);
   }
}
void initiateWhereAxisRedraw(Tcl_Interp *, bool doubleBuffer) {
   whereAxisDrawWhenIdle.install((ClientData)doubleBuffer);
}
void initiateWhereAxisRedrawNow(Tcl_Interp *, bool doubleBuffer) {
   whereAxisDrawWhenIdleRoutine((ClientData)doubleBuffer);
}


static int resources_clear(Tcl_Interp *interp) {
   assert(haveSeenFirstGoodWhereAxisWid);
   assert(global_abstractions);
   if (global_abstractions->existsCurrent()) {
      global_abstractions->clearSelections(); // doesn't redraw
      initiateWhereAxisRedraw(interp, true); // dbl buffer
   }

   return TCL_OK;
}

static int resources_select_unselect_by_addr(kptr_t addr,
					     kapi_module *kMod,
					     kapi_function *kFunc)
{
    int rv;
    if ((rv = kmgr.findModuleAndFunctionByAddress(addr, kMod, kFunc)) < 0) {
	return rv;
    }
    return 0;
}

static int
resources_select_unselect_by_names(const pdstring &modname, const pdstring &fnname,
				   kapi_module *kMod, kapi_function *kFunc)
{
    int rv;

    // First find the module
    if ((rv = kmgr.findModule(modname.c_str(), kMod)) < 0) {
	return rv;
    }
    // Now look for the fn within the module:
    if ((rv = kMod->findFunction(fnname.c_str(), kFunc)) < 0) {
	if (rv == kapi_module::function_not_unique) {
	    cout << "\"" << modname << " " << fnname << "\""
		 << " is ambiguous (has several matches)" << endl
		 << "To disambiguate, retry by specifying a specific "
		 << "function addr instead" << endl;
	}
	return rv;
    }
    return 0;
}

int resources_select_unselect(Tcl_Interp *interp, int argc, 
                              TCLCONST char **argv, bool selectFlag) {
   // arguments: modulename fnname
   // or, function address (in hex)

   if (argc != 2 && argc != 3) {
      Tcl_SetResult(interp, "incorrect usage for resources select command",
                    TCL_STATIC);
      return TCL_ERROR;
   }

   kapi_module kMod;
   kapi_function kFunc;
   int rv;
   if (argc == 2) {
#ifdef ppc64_unknown_linux2_4
      kptr_t addr = strtoul(argv[1], NULL, 16);
      if((addr == ULONG_LONG_MAX) || (addr == 0)) {
#else
      unsigned long addr = strtoul(argv[1], NULL, 16);
      if((addr == ULONG_MAX) || (addr == 0)) {
#endif
         pdstring msg("resources select/unselect: ");
         msg += argv[1];
         msg += " is not a valid hex address.";
         Tcl_SetResult(interp, const_cast<char*>(msg.c_str()), TCL_VOLATILE);
         return TCL_ERROR;
      }  
      else {
         rv = resources_select_unselect_by_addr(addr, &kMod, &kFunc);
         if (rv < 0) {
            Tcl_SetResult(interp, "Sorry, couldn't find module/function with that address", TCL_STATIC);
            return TCL_ERROR;
         }
      }
   }
   else {
      rv = resources_select_unselect_by_names(argv[1], argv[2],
                                              &kMod, &kFunc);
      if (rv < 0) {
         Tcl_SetResult(interp, "Sorry, couldn't find module/function with that name", TCL_STATIC);
         return TCL_ERROR;
      }
   }

   
   // fullPathName needs to be in a format understood by the method.
   // In particular, the components (separated by slashes) must *exactly*
   // match the names as they appear in the where axis.  In particular, this
   // means that we need to massage the module name and funtion name as follows:
   // module:
   // -- prepend hex address of module
   // -- append the module comment, if any
   // function:
   // -- prepend hex address of function
   // -- if fnname is an alias, return the "actual" function, since that's
   //    the only one that the where axis can find.

   pdstring fullPathName = pdstring("root/Code/");

   extern pdstring module2ItsWhereAxisString(const kapi_module &kMod);
   fullPathName += module2ItsWhereAxisString(kMod); // misc.C

   fullPathName += "/";

   extern pdstring fn2ItsWhereAxisString(const kapi_function &);
   fullPathName += fn2ItsWhereAxisString(kFunc); // misc.C

   simpSeq<unsigned> thePath;
   if (!global_abstractions->fullName2PathInCurr(fullPathName, thePath)) {
      pdstring msg("resources select/unselect: didn't find specified path: \"");
      msg += fullPathName;
      msg += "\"";
      
      Tcl_SetResult(interp, const_cast<char*>(msg.c_str()), TCL_VOLATILE);
      return TCL_ERROR;
   }

   bool needRedraw = global_abstractions->forciblyExpandAndScrollToEndOfPath(thePath);

   if (global_abstractions->selectUnSelectFromPath(thePath, selectFlag))
      needRedraw = true;

   if (needRedraw)
      initiateWhereAxisRedraw(interp, true); // true --> dbl buffer

   return TCL_OK;
}

static int resources_select(Tcl_Interp *interp, int argc, TCLCONST char **argv) {
   // arguments: modulename fnname  or  fn addr
   return resources_select_unselect(interp, argc, argv, true);
}

static int resources_unselect(Tcl_Interp *interp, int argc, TCLCONST char **argv) {
   // arguments: modulename fnname  or  fn addr
   return resources_select_unselect(interp, argc, argv, false);
}

static int resourcs_cmd(ClientData, Tcl_Interp *interp,
                         int argc, TCLCONST char **argv) {
   // possibilities for argv[1]: clear, select, unselect

   if (argc < 2) {
      Tcl_SetResult(interp, "resources command: needs more args",
                    TCL_STATIC);
      return TCL_ERROR;
   }
   
   const char *cmd = argv[1];
   if (0==strcmp(cmd, "clear"))
      return resources_clear(interp);
   else if (0==strcmp(cmd, "select"))
      return resources_select(interp, argc-1, argv+1);
   else if (0==strcmp(cmd, "unselect"))
      return resources_unselect(interp, argc-1, argv+1);
   else {
      pdstring msg = "resources command: unknown modifier \"";
      msg += cmd;
      msg += "\"";
      Tcl_SetResult(interp, const_cast<char*>(msg.c_str()), TCL_VOLATILE);
      return TCL_ERROR;
   }
}

#ifdef sparc_sun_solaris2_7

static int test_virtualization_cmd(Tcl_Interp *interp, int argc, TCLCONST char **argv) {
   if (argc < 2) {
      Tcl_SetResult(interp, "test virtualization: not enough args", TCL_STATIC);
      return TCL_ERROR;
   }

   const char *first_param = argv[1];
   
   if (0 == strcmp(first_param, "all_vtimers") ||
       0 == strcmp(first_param, "stacklist") ||
       0 == strcmp(first_param, "hashtable") ||
       0 == strcmp(first_param, "fullensemble")) {
       Tcl_SetResult(interp, "No longer supported", TCL_STATIC);
       return TCL_ERROR;
   }
   else {
      pdstring str("test virtualization: unrecognized arg ");
      str += pdstring(first_param);
      Tcl_SetResult(interp, const_cast<char *>(str.c_str()), TCL_VOLATILE);
      return TCL_ERROR;
   }
}

static int test_outlining_1fn_inisolation(const pdstring &modName,
                                          kptr_t fnEntryAddr)
{
   const dictionary_hash<const char *, kptr_t> 
       knownDownloadedCodeAddrsDict(cStrHash);
      // empty on purpose
   return kmgr.testOutliningOneFn(modName.c_str(), fnEntryAddr,
				  knownDownloadedCodeAddrsDict);
}

static int test_outlining_1module_inisolation(Tcl_Interp *interp,
					      const kapi_module &kmod)
{
    const unsigned maxlen = 255;
    char buffer[maxlen];
    if (kmod.getName(buffer, maxlen) < 0) {
	assert(false && "Module name is too long");
    }

    cout << "Testing outlining for functions in module \""
	 << buffer << '\"' << endl;

   const dictionary_hash<const char*, kptr_t> 
       knownDownloadedCodeAddrsDict(cStrHash);
      // empty

   kapi_vector<kapi_function> allFunctions;
   if (kmod.getAllFunctions(&allFunctions) < 0) {
       assert(false);
   }
   kapi_vector<kapi_function>::const_iterator fn_iter = allFunctions.begin();
   for (; fn_iter != allFunctions.end(); fn_iter++) {
       const kapi_function &kfunc = *fn_iter;
       if (kmgr.testOutliningOneFn(buffer /*modname*/, kfunc.getEntryAddr(),
				   knownDownloadedCodeAddrsDict) < 0) {
	   Tcl_SetResult(interp, "Internal error", TCL_STATIC);
	   return TCL_ERROR;
       }
   }

   return TCL_OK;
}

static int test_outlining_allfns_inisolation(Tcl_Interp *interp) {
   cout << "Test outlining all fns in isolation...welcome" << endl;

   kapi_vector<kapi_module> allModules;
   if (kmgr.getAllModules(&allModules) < 0) {
       assert(false);
   }
   kapi_vector<kapi_module>::const_iterator imod = allModules.begin();
   for (; imod != allModules.end(); imod++) {
       const kapi_module &kmod = *imod;
       
       int result = test_outlining_1module_inisolation(interp, kmod);
       if (result != TCL_OK) {
	   return result;
       }
   }

   cout << "Test outlining all fns in isolation...done" << endl;
   return TCL_OK;
}

static int test_outlining_cmd(Tcl_Interp *interp, int argc, TCLCONST char **argv) {
   // "test outlining", "test outlining modname", or 
   // "test outlining modname fnname"

   if (argc == 1)
      return test_outlining_allfns_inisolation(interp);
   else if (argc == 2 || argc == 3) {
      // an entire module, or a specific function w/in a module
      const char *modname = argv[1];

      kapi_module kmod;
      if (kmgr.findModule(modname, &kmod) < 0) {
         Tcl_SetResult(interp, "test outlining: unknown modname", TCL_STATIC);
         return TCL_ERROR;
      }

      if (argc == 2) {
         // an entire module
         return test_outlining_1module_inisolation(interp, kmod);
      }
      else {
         // one function w/in this module
      
         const char *fnname = argv[2];
	 
	 kapi_function kfunc;
	 if (kmod.findFunction(fnname, &kfunc) < 0) {
	     Tcl_SetResult(interp, "test outlining modname fnname: found that "
			   "module, but not that function (or function name "
			   "is ambiguous, in which case retry by specifying "
			   "a specific addr instead)\n", TCL_STATIC);
	     return TCL_ERROR;
         }
         else {
	     const kptr_t fnEntryAddr = kfunc.getEntryAddr();
	     return test_outlining_1fn_inisolation(modname, fnEntryAddr);
         }
      }
   }
   else {
      Tcl_SetResult(interp, "test outlining: incorrect params", TCL_STATIC);
      return TCL_ERROR;
   }
}

static int test_cmd(ClientData, Tcl_Interp *interp,
                    int argc, TCLCONST char **argv) {
   if (argc < 2) {
      Tcl_SetResult(interp, "test cmd: needs at least 1 arg", TCL_STATIC);
      return TCL_ERROR;
   }
   
   const char *first_param = argv[1];

   if (0 == strcmp(first_param, "virtualization"))
      return test_virtualization_cmd(interp, argc - 1, argv + 1);
   else if (0 == strcmp(first_param, "outlining"))
      return test_outlining_cmd(interp, argc - 1, argv + 1);
   else {
      pdstring str("test cmd: unknown arg ");
      str += pdstring(first_param);
      
      Tcl_SetResult(interp, const_cast<char *>(str.c_str()), TCL_VOLATILE);
      return TCL_ERROR;
   }
}

#endif // sparc_sun_solaris2_7

static int metrics_cmd(ClientData, Tcl_Interp *interp,
                       int argc, TCLCONST char **argv) {
   // possibilities for argv[1]: clear, select, unselect

   if (argc < 2) {
      Tcl_SetResult(interp, "metrics command: needs more args",
                    TCL_STATIC);
      return TCL_ERROR;
   }
   
   const char *cmd = argv[1];
   if (0==strcmp(cmd, "clear"))
      myTclEval(interp, "metricsClear", false); // implemented in .tcl file
   else if (0==strcmp(cmd, "select")) {
      if(argc != 3) {
         Tcl_SetResult(interp, "metrics select command: needs 1 more arg", TCL_STATIC);
         return TCL_ERROR;
      }
      else {
         pdstring cmd = "metricsSelect {";
         cmd += argv[2];
         cmd += "}";
         myTclEval(interp, cmd, false); // implemented in .tcl file
      }
   }
   else if (0==strcmp(cmd, "unselect")) {
      if(argc != 3) {
         Tcl_SetResult(interp, "metrics unselect command: needs 1 more arg", TCL_STATIC);
         return TCL_ERROR;
      }
      else {
         pdstring cmd = "metricsUnselect {";
         cmd += argv[2];
         cmd += "}";
         myTclEval(interp, cmd, false); // implemented in .tcl file
      }
   }
   else {
      pdstring msg = "metrics command: unknown modifier \"";
      msg += cmd;
      msg += "\"";
      Tcl_SetResult(interp, const_cast<char*>(msg.c_str()), TCL_VOLATILE);
      return TCL_ERROR;
   }

   return TCL_OK;
}

static int justTestingChange_cmd(ClientData, Tcl_Interp *, int argc, TCLCONST char **argv) {
   if (argc != 2) {
      cerr << "justTestingChange_cmd: got argc of " << argc << ", expected 2"
           << endl;
      return TCL_ERROR;
   }
   
   int newValue = atoi(argv[1]);
   bool val = newValue ? true : false;

   kmgr.setTestingFlag(val);
   
   return TCL_OK;
}

static int cpuSelectionChange_cmd(ClientData, Tcl_Interp *, 
				  int argc, TCLCONST char **argv) 
{
    if (argc != 3) {
	cerr << "cpuSelectionChange_cmd: got argc of " << argc 
	     << ", expected 3" << endl;
	return TCL_ERROR;
    }
   
    int cpu_id = atoi(argv[1]);
    int state = atoi(argv[2]);
    
    global_abstractions->updateCurrentCPUSelections(cpu_id, state);

    // cout << "Changed state of cpu" << cpu_id << " to " << state << endl;

    return TCL_OK;
}

#ifdef sparc_sun_solaris2_7

// Functions that deal with reading/writing %pcr and %pic are no
// longer supported, since they are platform-dependent. However,
// kperfmon.tcl still calls them on startup, so we must provide
// some dummy stubs here.
static int setPcrCmd_Cmd(ClientData, Tcl_Interp *,
                         int argc, TCLCONST char **/*argv*/) {
   // args: pic0, pic1, userflag, sysflag, privflag
   if (argc != 6) {
      cerr << "setPcrCmd_Cmd: got argc of " << argc << ", expected 6" << endl;
      return TCL_ERROR;
   }

   return TCL_OK;
}

int getCurrPcrSettings_Cmd(ClientData, Tcl_Interp *interp,
                           int, TCLCONST char **) {
   // return value is a list of the following elements (all numbers):
   // pic0choice, pic1choice, userflag, sysflag, privflag

   const char *strings_to_merge[5];
   strings_to_merge[0] = "0";
   strings_to_merge[1] = "0";
   strings_to_merge[2] = "0";
   strings_to_merge[3] = "0";
   strings_to_merge[4] = "0";

   char *result_str = Tcl_Merge(5, const_cast<char **> (strings_to_merge));
   // result string is allocated with Tcl_Alloc(); we should eventually
   // free it with a call to Tcl_Free() (actually, by using TCL_DYNAMIC
   // argument to Tcl_SetResult(), tcl will do this for us when necessary)

   Tcl_SetResult(interp, result_str, TCL_DYNAMIC);

   return TCL_OK;
}

static int getPicRawCmd_Cmd(ClientData, Tcl_Interp *,
                            int, TCLCONST char **) {
   return TCL_OK;
}

static int clearPicCmd_Cmd(ClientData, Tcl_Interp *, int, TCLCONST char **) {
   return TCL_OK;
}

#endif // sparc_sun_solaris2_7

static int callOnceCmd_Cmd(ClientData, Tcl_Interp *, int, TCLCONST char **) {
   kmgr.callOnce();
   return TCL_OK;
}

static int whereAxisLeftClick_cmd(ClientData, Tcl_Interp *interp,
                                  int argc, TCLCONST char **argv) {
   if (!tryFirstGoodWhereAxisWid(interp))
      return TCL_OK;

   // args: x, y coords
   if (argc != 3) {
      Tcl_SetResult(interp, "incorrect # args", TCL_STATIC);
      return TCL_ERROR;
   }

   const int x = atoi(argv[1]);
   const int y = atoi(argv[2]);

   assert(global_abstractions);
   if (global_abstractions->existsCurrent())
      global_abstractions->processSingleClick(x, y);

   return TCL_OK;
}

static int whereAxisDoubleClick_cmd(ClientData, Tcl_Interp *interp,
                                    int argc, TCLCONST char **argv) {
   if (!tryFirstGoodWhereAxisWid(interp))
      return TCL_OK;

   // args: x, y coords
   if (argc != 3) {
      Tcl_SetResult(interp, "incorrect # args", TCL_STATIC);
      return TCL_ERROR;
   }

   const int x = atoi(argv[1]);
   const int y = atoi(argv[2]);

   assert(global_abstractions);
   if (global_abstractions->existsCurrent()) {
      bool needToRedrawAll = global_abstractions->processDoubleClick(x, y);
      if (needToRedrawAll)
         initiateWhereAxisRedraw(interp, true); // true --> dbl buffer
   }
   
   return TCL_OK;
}

static int whereAxisCtrlDoubleClick_cmd(ClientData, Tcl_Interp *interp,
                                        int argc, TCLCONST char **argv) {
   if (!tryFirstGoodWhereAxisWid(interp))
      return TCL_OK;

   // args: x, y coords
   if (argc != 3) {
      Tcl_SetResult(interp, "incorrect # args", TCL_STATIC);
      return TCL_ERROR;
   }

   const int x = atoi(argv[1]);
   const int y = atoi(argv[2]);

   assert(global_abstractions);
   if (global_abstractions->existsCurrent()) {
      bool needToRedrawAll = global_abstractions->processCtrlDoubleClick(x, y);
      if (needToRedrawAll)
         initiateWhereAxisRedraw(interp, true); // true --> dbl buffer
   }
   
   return TCL_OK;
}

static int whereAxisMidClick_cmd(ClientData, Tcl_Interp *, int, TCLCONST char **) {
   cerr << "mid click -- nyi" << endl;
   return TCL_OK;
}

static int altAnchorX, altAnchorY;
static bool currentlyInstalledAltMoveHandler = false;
static bool ignoreNextAltMove = false;

static int whereAxisAltPress_cmd(ClientData, Tcl_Interp *interp,
                                 int argc, TCLCONST char **argv) {
   if (!tryFirstGoodWhereAxisWid(interp))
      return TCL_OK;

   assert(global_abstractions);
   if (!global_abstractions->existsCurrent())
      return TCL_OK;

   // args: x, y coords
   if (argc != 3) {
      Tcl_SetResult(interp, "incorrect # args", TCL_STATIC);
      return TCL_ERROR;
   }

   int x = atoi(argv[1]);
   int y = atoi(argv[2]);

   if (currentlyInstalledAltMoveHandler) {
      if (ignoreNextAltMove) {
         ignoreNextAltMove = false;
         return TCL_OK;
      }

      int deltax = x - altAnchorX;
      int deltay = y - altAnchorY;

      // add some extra speedup juice as an incentive to use alt-mousemove scrolling
      deltax *= 4;
      deltay *= 4;

      global_abstractions->adjustHorizSBOffsetFromDeltaPix(deltax);
      global_abstractions->adjustVertSBOffsetFromDeltaPix(deltay);

      initiateWhereAxisRedraw(interp, true);

      Tk_Window theTkWindow = global_abstractions->getTkWindow();

      XWarpPointer(Tk_Display(theTkWindow),
		   Tk_WindowId(theTkWindow),
		   Tk_WindowId(theTkWindow),
		   0, 0, 0, 0,
		   altAnchorX, altAnchorY);

      ignoreNextAltMove = true;

      return TCL_OK;
   }
   else {
      altAnchorX = x;
      altAnchorY = y;

      currentlyInstalledAltMoveHandler = true;
   }

   return TCL_OK;
}

static int whereAxisAltRelease_cmd(ClientData, Tcl_Interp *, int, TCLCONST char **) {
   currentlyInstalledAltMoveHandler = false;
   return TCL_OK;
}

static int whereAxisConfigure_cmd(ClientData, Tcl_Interp *interp, int argc, 
                                  TCLCONST char **) {
   // configure, aka resize

   // args: none
   if (argc != 1) {
      Tcl_SetResult(interp, "incorrect # args", TCL_STATIC);
      return TCL_ERROR;
   }
   
   if (!tryFirstGoodWhereAxisWid(interp))
      return TCL_OK;

   assert(global_abstractions);
   if (global_abstractions->existsCurrent()) {
      global_abstractions->resizeCurrent();

      initiateWhereAxisRedraw(interp, true); // true --> dbl buffer
   }
   return TCL_OK;
}

static int whereAxisExpose_cmd(ClientData, Tcl_Interp *interp, int argc, 
                               TCLCONST char **argv) {
   // args: xevent count field
   if (argc != 2) {
      Tcl_SetResult(interp, "incorrect # args", TCL_STATIC);
      return TCL_ERROR;
   }

   int count = atoi(argv[1]); // xevent count field: redraw only if 0
   
   if (!tryFirstGoodWhereAxisWid(interp))
      return TCL_OK;

   assert(global_abstractions);
   if (global_abstractions->existsCurrent() && count == 0)
      initiateWhereAxisRedraw(interp, true); // true --> dbl buffer

   return TCL_OK;
}

static int whereAxisVisibility_cmd(ClientData, Tcl_Interp *interp,
                                   int argc, TCLCONST char **argv) {
   if (argc != 2) {
      Tcl_SetResult(interp, "incorrect # args", TCL_STATIC);
      return TCL_ERROR;
   }

   const char *newVisibility = argv[1];
   
   if (!tryFirstGoodWhereAxisWid(interp))
      return TCL_OK;

   if (0==strcmp(newVisibility, "VisibilityUnobscured"))
      global_abstractions->makeVisibilityUnobscured();
   else if (0==strcmp(newVisibility, "VisibilityPartiallyObscured"))
      global_abstractions->makeVisibilityPartiallyObscured();
   else if (0==strcmp(newVisibility, "VisibilityFullyObscured"))
      global_abstractions->makeVisibilityFullyObscured();
   else {
      cerr << "unrecognized visibility " << newVisibility << endl;
      return TCL_ERROR;
   }

   // Needed?
   if (global_abstractions->existsCurrent())
      initiateWhereAxisRedraw(interp, true); // true --> dbl buffer

   return TCL_OK;
}

static int whereAxisNewVertScrollPosition_cmd(ClientData, Tcl_Interp *interp,
                                              int argc, TCLCONST char **argv) {
   if (!tryFirstGoodWhereAxisWid(interp))
      return TCL_OK;

   assert(global_abstractions);
   if (!global_abstractions->existsCurrent())
      return TCL_OK;
   
   // The arguments will be one of:
   // 1) moveto [fraction]
   // 2) scroll [num-units] unit   (num-units is always either -1 or 1)
   // 3) scroll [num-pages] page   (num-pages is always either -1 or 1)

   float newFirst;
   bool anyChanges = processScrollCallback(interp, argc, (const char **)argv,
                                           global_abstractions->getVertSBName(),
                                           global_abstractions->getVertSBOffset(),
                                              // <= 0
                                           global_abstractions->getTotalVertPixUsed(),
                                           global_abstractions->getVisibleVertPix(),
                                           newFirst); // tkTools.C

   if (anyChanges)
      anyChanges = global_abstractions->adjustVertSBOffset(newFirst);
   
   if (anyChanges)
      initiateWhereAxisRedraw(interp, true);

   return TCL_OK;
}

static int whereAxisNewHorizScrollPosition_cmd(ClientData, Tcl_Interp *interp,
                                               int argc, TCLCONST char **argv) {
   if (!tryFirstGoodWhereAxisWid(interp))
      return TCL_OK;

   assert(global_abstractions);
   if (!global_abstractions->existsCurrent())
      return TCL_OK;
   
   // The arguments will be one of:
   // 1) moveto [fraction]
   // 2) scroll [num-units] unit   (num-units is always either -1 or 1)
   // 3) scroll [num-pages] page   (num-pages is always either -1 or 1)

   float newFirst;
   bool anyChanges = processScrollCallback(interp, argc, (const char **)argv,
			   global_abstractions->getHorizSBName(),
			   global_abstractions->getHorizSBOffset(), // <= 0
			   global_abstractions->getTotalHorizPixUsed(),
			   global_abstractions->getVisibleHorizPix(),
			   newFirst);
   if (anyChanges)
      anyChanges = global_abstractions->adjustHorizSBOffset(newFirst);

   if (anyChanges)
      initiateWhereAxisRedraw(interp, true);   

   return TCL_OK;
}



static int whereAxisDestroyed_cmd(ClientData, Tcl_Interp *, int, TCLCONST char **) {
   delete global_abstractions;
   global_abstractions = NULL; // necessary; we later check for this

   return TCL_OK;
}

// ----------------------------------------------------------------------

static int getMetricName_cmd(ClientData, Tcl_Interp *interp, int argc, 
                             TCLCONST char **argv) {
   if (argc != 2) {
      Tcl_SetResult(interp, "getMetricName: needs 1 arg (metric num)", TCL_STATIC);
      return TCL_ERROR;
   }
   
   const pdstring &the_name = global_allMetrics.getById(atoi(argv[1])).getName();

   Tcl_SetResult(interp, const_cast<char*>(the_name.c_str()), TCL_VOLATILE);
      // we could probably optimize this to TCL_STATIC, but it's not a high
      // enough priority to bother to check whether it's safe in this case.

   return TCL_OK;
}

static int getMetricDetails_cmd(ClientData, Tcl_Interp *interp, int argc, 
                                TCLCONST char **argv) {
   if (argc != 2) {
      Tcl_SetResult(interp, "getMetricDetails: needs 1 arg (metric num)",
                    TCL_STATIC);
      return TCL_ERROR;
   }
   
   const pdstring &the_details = global_allMetrics.getById(atoi(argv[1])).getDetails();

   Tcl_SetResult(interp, const_cast<char*>(the_details.c_str()), TCL_VOLATILE);
      // we could probably optimize this to TCL_STATIC, but it's not a high
      // enough priority to bother to check whether it's safe in this case.

   return TCL_OK;
}

static int getMetricClusterId_cmd(ClientData, Tcl_Interp *interp,
                                  int argc, TCLCONST char **argv) {
   if (argc != 2) {
      Tcl_SetResult(interp, "getMetricClusterId: needs 1 arg (metric num)", TCL_STATIC);
      return TCL_ERROR;
   }
   
   const unsigned clusterid = global_allMetrics.getById(atoi(argv[1])).getClusterId();

   Tcl_SetResult(interp, const_cast<char*>(num2string(clusterid).c_str()), TCL_VOLATILE);

   return TCL_OK;
}

static int kperfmon_version_string_cmd(ClientData, Tcl_Interp *interp, int, 
                                       TCLCONST char **) {
   extern const Ident kperfmon_ident;

   Tcl_SetResult(interp, const_cast<char*>(kperfmon_ident.release()), TCL_STATIC);
   return TCL_OK;
}

// ----------------------------------------------------------------------

static int disassHelp(Tcl_Interp *, int, TCLCONST char **) {
   cout << "disass: 1 arg: hex addr of fn to disassemble" << endl;
   cout << "The address can be any address of the function; it does" << endl;
   cout << "not need to be the function's entry address." << endl;
   
   return TCL_OK;
}

static int getNumMetricsHelp(Tcl_Interp *, int, TCLCONST char **) {
   cout << "getNumMetrics: no args, returns an int" << endl;
   cout << "Use in conjunction with getMetricName an getMetricDetails," << endl;
   cout << "for example" << endl;

   return TCL_OK;
}

static int getCurrSelectedCodeHelp(Tcl_Interp *, int, TCLCONST char **) {
   cout << "getCurrSelectedCode: no args, returns ???" << endl;

   return TCL_OK;
}

static int newVisiHelp(Tcl_Interp *, int, TCLCONST char **) {
   cout << "newVisi: 1 arg: visi name; returns visi-id for later" << endl;
   cout << "use in closeVisi and addCurrSelectionsToVisi" << endl;
   
   return TCL_OK;
}

static int closeVisiHelp(Tcl_Interp *, int, TCLCONST char **) {
   cout << "closeVisi: 1 arg: visi-id, as returned from a prior newVisi cmd" << endl;
   
   return TCL_OK;
}

static int addCurrSelectionsToVisiHelp(Tcl_Interp *, int, TCLCONST char **) {
   cout << "addCurrSelectionsToVisi: 1 arg: visi-id, as returned from newVisi" << endl;
   cout << "Takes the current metric and resource selections, and adds them"
        << endl << "to that visi.  This will instrument the kernel." << endl;

   return TCL_OK;
}

static int resourcesHelp(Tcl_Interp *, int, TCLCONST char **) {
   cout << "resources clear: clear all resource selections" << endl;
   cout << "resources [un]select <modname> <fnname>" << endl;
   cout << "resources [un]select <hexaddr>" << endl;

   return TCL_OK;
}

static int metricsHelp(Tcl_Interp *, int, TCLCONST char **) {
   cout << "metrics clear: clear all metric selections" << endl;
   cout << "metrics [un]select <metricname>" << endl;
   cout << "(tip: use getMetricName command to see metric names)" << endl;

   return TCL_OK;
}

static int getMetricNameHelp(Tcl_Interp *, int, TCLCONST char **) {
   cout << "getMetricName <metricNum>: return the name of a metric" << endl;
   cout << "   when given its metric number (from 0 to [getNumMetrics]-1)" << endl;

   return TCL_OK;
}

static int getMetricDetailsHelp(Tcl_Interp *, int, TCLCONST char **) {
   cout << "getMetricDetails <metricNum>: return a long (paragraph-length)" << endl;
   cout << "   description of a metric, identified by its metric number" << endl;
   cout << "   (from 0 to [getNumMetrics]-1)" << endl;
   
   return TCL_OK;
}

#ifdef sparc_sun_solaris2_7

static int outlineHelp(Tcl_Interp *, int, TCLCONST char **) {
   cout << "outline <modname> <fnname>" << endl;
   
   return TCL_OK;
}

static int unOutlineHelp(Tcl_Interp *, int, TCLCONST char **) {
   cout << "unOutline <modname> <fnname>" << endl;
   
   return TCL_OK;
}

static int parseNewFnHelp(Tcl_Interp *, int, TCLCONST char **) {
   cout << "parseNewFn <modname> <moddescription> <funcname> <addrrange> <entryndx> <preFnDataNdx>"
        << endl;
   cout << "- modname can be either an existing module name or a new one" << endl;
   cout << "- moddescription is only used if this is a new module" << endl;
   cout << "- addrrange is a tcl list of pairs, each pair being" << endl;
   cout << "  a startAddr (in hex) and numBytes (in decimal)" << endl;
   cout << "  example of an addrrange is {{0x10234534 40} {0x10400000 60}}" << endl;
   cout << "  for a two-chunk range, or {{0x10234534 40}} for a one-chunk range"
        << endl;
   cout << "- entryndx is a decimal number indicating which addrrange pair" << endl;
   cout << "  is the entry point for this new function.  In the above example,"
        << endl;
   cout << "  entryndx would be either 0 or 1." << endl;
   cout << "- preFnDataNdx is a decimal number indicating which addrrange pair" << endl;
   cout << "  should be treated as data (such as jump table data) and not parsed"
        << endl;
   cout << "  (-1 if none; only one can be specified).  In the above example,"
        << endl
        << "  this would be either 0 or 1 (to specify one), or -1 (for none)."
        << endl;

   return TCL_OK;
}

static int unParseFnHelp(Tcl_Interp *, int, TCLCONST char **) {
   cout << "unParseFn <modname> <funcname>" << endl;
   cout << "Be sure to call only for a function that had been previously 'created'"
        << endl
        << "via the parseNewFn command.  Results otherwise haven't been tested and"
        << endl
        << "may lead to mayhem." << endl;
   
   return TCL_OK;
}

#endif // sparc_sun_solaris2_7

struct cmdNameHelpPair {
   const char *name;
   int (*help_cmd)(Tcl_Interp *,int, TCLCONST char **);
};

static int help_cmd(ClientData, Tcl_Interp *interp, int argc, TCLCONST char **argv) {
   const cmdNameHelpPair help_commands[] = {
      {"disass", disassHelp},
      {"getNumMetrics", getNumMetricsHelp},
      {"getCurrSelectedCode", getCurrSelectedCodeHelp},
      {"newVisi", newVisiHelp},
      {"closeVisi", closeVisiHelp},
      {"addCurrSelectionsToVisi", addCurrSelectionsToVisiHelp},
      {"resources", resourcesHelp},
      {"metrics", metricsHelp},
      {"getMetricName", getMetricNameHelp},
      {"getMetricDetails", getMetricDetailsHelp},
#ifdef sparc_sun_solaris2_7
      {"parseNewFn", parseNewFnHelp},
      {"unParseFn", unParseFnHelp},
      {"outline", outlineHelp},
      {"unOutline", unOutlineHelp},
#endif
      {NULL, NULL}
   };
   
   if (argc == 1) {
      cout << "kperfmon command-line interface:" << endl;
   
      cout << "disass" << endl;
      cout << "getNumMetrics" << endl;
      cout << "getCurrSelectedCode" << endl;
      cout << "newVisi" << endl;
      cout << "closeVisi" << endl;
      cout << "addCurrSelectionsToVisi" << endl;
      cout << "resources" << endl;
      cout << "metrics" << endl;
      cout << "getMetricName" << endl;
      cout << "getMetricDetails" << endl;
#ifdef sparc_sun_solaris2_7
      cout << "parseNewFn" << endl;
      cout << "unParseFn" << endl;
      cout << "outline" << endl;
      cout << "unOutline" << endl;
      cout << "set outliningDoReplaceFunction [bool]" << endl;
      cout << "set outliningReplaceFunctionPatchUpCallSitesToo [bool]" << endl;
      cout << "set outliningBlockCountDelay [millisecs]" << endl;
      cout << "set outlineChildrenToo [bool]" << endl;
      cout << "set forceOutliningOfAllChildren [bool]" << endl;
      cout << "set outliningBlockPlacement [origSeq | chains]" << endl;
#endif
      cout << endl << "Type help followed by a command for more info on that command"
           << endl;
   
      return TCL_OK;
   }

   const cmdNameHelpPair *iter = &help_commands[0];
   for (; iter->name != NULL; ++iter) {
      if (0 != strcmp(iter->name, argv[1]))
         continue;
      
      return iter->help_cmd(interp, argc-1, argv+1);
   }

   // help not found
   pdstring s("no help is available for the command \"");
   s += argv[1];
   s += pdstring("\"");
   Tcl_SetResult(interp, const_cast<char*>(s.c_str()), TCL_VOLATILE);

   return TCL_ERROR;
}


// ----------------------------------------------------------------------

static int addCurrSelectionsToVisi_cmd(ClientData, Tcl_Interp *interp,
                                       int argc, TCLCONST char **argv) {
   if (argc != 2) {
      Tcl_SetResult(interp, "Needs 1 arg: visi id", TCL_STATIC);
      return TCL_ERROR;
   }
   const int id = atoi(argv[1]);
   
   cmfSubscriber *theCmfSubscriber = cmfSubscriber::findCmfSubscriber(id);
   if (NULL == theCmfSubscriber) {
      Tcl_SetResult(interp, "failed to addCurrSelectionsToVisi; couldn't find the visi",
                    TCL_STATIC);
      return TCL_ERROR;
   }

   visiSubscriber *theVisi = dynamic_cast<visiSubscriber*>(theCmfSubscriber);
   if (theVisi == NULL)
      assert(false); // some other kind of cmfSubscriber.  unexpected.
   
   if (!theVisi->igenHandleExists()) {
      Tcl_SetResult(interp, "Failed to addCurrSelectionsToVisi: igenHandle NULL (already closed down or never got set up)", TCL_STATIC);
      return TCL_ERROR;
   }

   asyncInstrumentCurrentSelections_thenSample(*theVisi,
                                               interp); // main.C

   return TCL_OK;
}

// ----------------------------------------------------------------------

extern int disass_cmd(ClientData, Tcl_Interp *interp, int argc, TCLCONST char **argv);
extern int launchDisassFnFromMem_cmd(ClientData, Tcl_Interp *interp,
				     int argc, TCLCONST char **argv);
extern int launchDisassFnByAddrFromMem_cmd(ClientData, Tcl_Interp *interp,
					   int argc, TCLCONST char **argv);
extern int launchDisassRangeFromMem_cmd(ClientData, Tcl_Interp *interp,
					int argc, TCLCONST char **argv);
extern int disassFnAsParsed_cmd(ClientData, Tcl_Interp *interp,
				int argc, TCLCONST char **argv);

void installTclCommands(Tcl_Interp *interp) {
   (void)Tcl_CreateCommand(interp, "help",
                           help_cmd, NULL, NULL);
   
   (void)Tcl_CreateCommand(interp, "get_kperfmon_version_string",
                           kperfmon_version_string_cmd,
                           NULL, NULL // clientdata, deleteproc
                           );

   (void)Tcl_CreateCommand(interp, "disass",
                           disass_cmd, NULL, NULL);
   
   (void)Tcl_CreateCommand(interp, "whereAxisLeftClick",
                           whereAxisLeftClick_cmd,
                           NULL, NULL // clientdata, deleteproc
      );
   (void)Tcl_CreateCommand(interp, "whereAxisDoubleClick",
                           whereAxisDoubleClick_cmd,
                           NULL, NULL // clientdata, deleteproc
      );
   (void)Tcl_CreateCommand(interp, "whereAxisCtrlDoubleClick",
                           whereAxisCtrlDoubleClick_cmd,
                           NULL, NULL // clientdata, deleteproc
      );
   (void)Tcl_CreateCommand(interp, "whereAxisMidClick",
                           whereAxisMidClick_cmd,
                           NULL, NULL // clientdata, deleteproc
      );
   (void)Tcl_CreateCommand(interp, "whereAxisAltPress",
                           whereAxisAltPress_cmd,
                           NULL, NULL // clientdata, deleteproc
      );
   (void)Tcl_CreateCommand(interp, "whereAxisAltRelease",
                           whereAxisAltRelease_cmd,
                           NULL, NULL // clientdata, deleteproc
      );
   (void)Tcl_CreateCommand(interp, "whereAxisConfigure",
                           whereAxisConfigure_cmd,
                           NULL, NULL // clientdata, deleteproc
      );
   (void)Tcl_CreateCommand(interp, "whereAxisExpose",
                           whereAxisExpose_cmd,
                           NULL, NULL // clientdata, deleteproc
      );
   (void)Tcl_CreateCommand(interp, "whereAxisVisibility",
                           whereAxisVisibility_cmd,
                           NULL, NULL // clientdata, deleteproc
      );
   (void)Tcl_CreateCommand(interp, "whereAxisNewVertScrollPosition",
                           whereAxisNewVertScrollPosition_cmd,
                           NULL, NULL // clientdata, deleteproc
      );
   (void)Tcl_CreateCommand(interp, "whereAxisNewHorizScrollPosition",
                           whereAxisNewHorizScrollPosition_cmd,
                           NULL, NULL // clientdata, deleteproc
      );

   // tk wants us to clean up some things we allocated before Tk_MainLoop() completes;
   // the following gives a hook into that.
   (void)Tcl_CreateCommand(interp, "whereAxisDestroyed",
                           whereAxisDestroyed_cmd,
                           NULL, NULL // clientdata, deleteproc
      );

   (void)Tcl_CreateCommand(interp, "getNumMetrics",
                           getNumMetrics_cmd,
                           NULL, // clientdata
                           NULL // deleteproc
      );

   (void)Tcl_CreateCommand(interp, "getCurrSelectedCode",
                           getCurrSelectedCode_cmd,
                           NULL, // clientdata
                           NULL // deleteproc
      );

   (void)Tcl_CreateCommand(interp, "launchDisassFnFromMem",
                           launchDisassFnFromMem_cmd,
                           NULL, // clientdata
                           NULL // deleteproc
      );
   (void)Tcl_CreateCommand(interp, "launchDisassRangeFromMem",
                           launchDisassRangeFromMem_cmd,
                           NULL, // clientdata
                           NULL // deleteproc
      );
   (void)Tcl_CreateCommand(interp, "launchDisassFnByAddrFromMem",
                           launchDisassFnByAddrFromMem_cmd,
                           NULL, // clientdata
                           NULL // deleteproc
      );

   (void)Tcl_CreateCommand(interp, "reqRegAnalInfoForInsn",
                           reqRegAnalInfoForInsn_cmd,
                           NULL, NULL);

   (void)Tcl_CreateCommand(interp, "disassFnAsParsed",
                           disassFnAsParsed_cmd,
                           NULL, // clientdata
                           NULL // deleteproc
      );

   // Controlling visis:
   (void)Tcl_CreateCommand(interp, "newVisi",
                           newVisi_cmd,
                           NULL, // clientdata
                           NULL // deleteproc
      );
   (void)Tcl_CreateCommand(interp, "newvisi", // make it case insensitive
                           newVisi_cmd,
                           NULL, // clientdata
                           NULL // deleteproc
      );
   (void)Tcl_CreateCommand(interp, "closeVisi",
                           closeVisi_cmd,
                           NULL, // clientdata
                           NULL // deleteproc
      );
   (void)Tcl_CreateCommand(interp, "closevisi", // make it case insensitive
                           closeVisi_cmd,
                           NULL, // clientdata
                           NULL // deleteproc
      );

   // Instrumenting on behalf of visis:
   (void)Tcl_CreateCommand(interp, "addCurrSelectionsToVisi", 
                           addCurrSelectionsToVisi_cmd,
                           NULL, NULL);

   // Resources / whereAxis:
   (void)Tcl_CreateCommand(interp, "whereAxis",
                           resourcs_cmd,
                           NULL, // clientdata
                           NULL // deleteproc
      );
   (void)Tcl_CreateCommand(interp, "resources",
                           resourcs_cmd,
                           NULL, // clientdata
                           NULL // deleteproc
      );

#ifdef sparc_sun_solaris2_7
   (void)Tcl_CreateCommand(interp, "test",
                           test_cmd, NULL, NULL);
#endif

   // Selecting/unselecting metrics, getting info about metrics:
   (void)Tcl_CreateCommand(interp, "metrics",
                           metrics_cmd,
                           NULL, // clientdata
                           NULL // deleteproc
                           );
   (void)Tcl_CreateCommand(interp, "getMetricName",
                           getMetricName_cmd,
                           NULL, NULL);
   (void)Tcl_CreateCommand(interp, "getMetricDetails", 
                          getMetricDetails_cmd,
                           NULL, NULL);
   (void)Tcl_CreateCommand(interp, "getMetricClusterId", 
                          getMetricClusterId_cmd,
                           NULL, NULL);


   (void)Tcl_CreateCommand(interp, "justTestingChange",
                           justTestingChange_cmd,
                           NULL, // clientdata
                           NULL // deleteproc
                           );
   (void)Tcl_CreateCommand(interp, "cpuSelectionChange",
                           cpuSelectionChange_cmd,
                           NULL, // clientdata
                           NULL // deleteproc
                           );


#ifdef sparc_sun_solaris2_7
   (void)Tcl_CreateCommand(interp, "getCurrPcrSettings",
                           getCurrPcrSettings_Cmd,
                           NULL, // clientdata
                           NULL // deleteproc
                           );
      // returns in format: pic0choice pic1choice userflag sysflag privflag

   (void)Tcl_CreateCommand(interp, "setPcrCmd",
                           setPcrCmd_Cmd,
                           NULL, // clientdata
                           NULL // deleteproc
                           );

   (void)Tcl_CreateCommand(interp, "getPicRawCmd",
                           getPicRawCmd_Cmd,
                           NULL, // clientdata
                           NULL // deleteproc
                           );
   (void)Tcl_CreateCommand(interp, "clearPicCmd",
                           clearPicCmd_Cmd,
                           NULL, // clientdata
                           NULL // deleteproc
                           );
#endif

   (void)Tcl_CreateCommand(interp, "callOnceCmd",
                           callOnceCmd_Cmd,
                           NULL, // clientdata
                           NULL // deleteproc
                           );

#ifdef sparc_sun_solaris2_7
   (void)Tcl_CreateCommand(interp, "outline",
                           outline_cmd, NULL, NULL);
   (void)Tcl_CreateCommand(interp, "outlineUsingProfile",
                           outlineUsingProfile_cmd, NULL, NULL);
   (void)Tcl_CreateCommand(interp, "unOutline",
                           unOutline_cmd, NULL, NULL);

   (void)Tcl_CreateCommand(interp, "parseNewFn",
                           parseNewFn_cmd, NULL, NULL);
   (void)Tcl_CreateCommand(interp, "unParseFn",
                           unParseFn_cmd, NULL, NULL);
#endif
}

