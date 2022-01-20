#include <tcl.h>
#include "kapi.h"
#include "util/h/out_streams.h"

#include "kpmChildrenOracle.h"
#include "abstractions.h"

extern dictionary_hash<pdstring, unsigned> moduleName2ModuleWhereAxisId;
extern dictionary_hash<unsigned, pdstring> xmoduleWhereAxisId2ModuleName;
extern dictionary_hash<kptr_t, unsigned> fnAddr2FnWhereAxisId;
extern dictionary_hash<unsigned, kptr_t> xfnWhereAxisId2FnAddr;
extern abstractions<kpmChildrenOracle> *global_abstractions;
extern unsigned codeWhereAxisId;
extern bool haveGUI;

extern void initiateWhereAxisRedraw(Tcl_Interp *, bool);

// This function is a callback that we register through the API's
// registerDynloadCallback to get notified of code additions / deletions.
// Our goal here is to update / redraw the where axis in sync with code
// updates.
int dynloadCallback(kapi_dynload_event_t event,
		    const kapi_module *kmod, const kapi_function *kfunc)
{
    if (haveGUI) {
	assert(global_abstractions);
	abstractions<kpmChildrenOracle> &theAbstractions =*global_abstractions;
	const unsigned maxlen = 256;
	char buffer[maxlen];

	switch (event) {
	case kapi_module_added: {
	    const pdstring whereAxisName("whereAxisName");
	    assert(-1 != theAbstractions.name2index(whereAxisName));
	    whereAxis<kpmChildrenOracle> &theWhereAxis =
		theAbstractions[whereAxisName];

	    theWhereAxis.recursiveDoneAddingChildren(true); // true --> resort

	    initiateWhereAxisRedraw(NULL, true); // true --> dbl buffer
	    break;
	}
	case kapi_module_removed: {
	    assert(kmod != 0);
	    if (kmod->getName(buffer, maxlen) < 0) {
		assert(false && "Module name is too long");
	    }
	    pdstring modName(buffer);
	    // As with the function, it's conceivable (though less likely)
	    // that the module never got added to the where axis because
	    // the module's name never had to be drawn on the screen.
	    // So be tolerant of that possibility.
	    if (moduleName2ModuleWhereAxisId.defines(modName)) {
		assert(codeWhereAxisId == 1);
		global_abstractions->deleteItemInCurrent(
		    moduleName2ModuleWhereAxisId.get(modName),
		    codeWhereAxisId,
		    true, // rethink gfx now
		    true, // resort now
		    false); // may not exist?  No, it will exist
		
		xmoduleWhereAxisId2ModuleName.get_and_remove(
		    moduleName2ModuleWhereAxisId.get(modName));
		moduleName2ModuleWhereAxisId.get_and_remove(modName);

		dout << "removed " << modName
		     << " from moduleName2ModuleWhereAxisId" << endl;
	    }
	    initiateWhereAxisRedraw(NULL, true); // true --> dbl buffer
	    break;
	}
	case kapi_function_added: {
	    initiateWhereAxisRedraw(NULL, true); // true --> dbl buffer
	    break;
	}
	case kapi_function_removed: {
	    assert(kmod != 0);
	    if (kmod->getName(buffer, maxlen) < 0) {
		assert(false && "Module name is too long");
	    }
	    assert(kfunc != 0);
	    kptr_t fnEntryAddr = kfunc->getEntryAddr();
	    pdstring modName(buffer);
	    // If this fn doesn't exist in fnAddr2FnWhereAxisId then it
	    // means it never got added to the where axis to begin with;
	    // because the oracle wasn't asked for it (user never caused
	    // it to be drawn on the screen).  So be tolerant of that
	    // possibility.
	    if (fnAddr2FnWhereAxisId.defines(fnEntryAddr)) {
		theAbstractions.deleteItemInCurrent(
		    fnAddr2FnWhereAxisId.get(fnEntryAddr),
		    moduleName2ModuleWhereAxisId.get(modName),
		    true, // rethink gfx now
		    true, // resort now
		    false); // mayNotExist?  No, it should exist
		initiateWhereAxisRedraw(NULL, true); // true --> dbl buffer
	    }
   
	    // If this fn doesn't exist in fnAddr2FnWhereAxisId then it
	    // means it never got added to the where axis to begin with;
	    // because the oracle wasn't asked for it (user never caused
	    // it to be drawn on the screen).  So be tolerant of that
	    // possibility.
	    if (fnAddr2FnWhereAxisId.defines(fnEntryAddr)) {
		xfnWhereAxisId2FnAddr.get_and_remove(
		    fnAddr2FnWhereAxisId.get(fnEntryAddr));
		fnAddr2FnWhereAxisId.get_and_remove(fnEntryAddr);
	    }
	    break;
	}
	default:
	    assert(false);
	    break;
	}
    }
    return 0;
}
