// kerninstdClientUser.C

#include "util/h/out_streams.h"
#include "kerninstdClient.xdr.CLNT.h"
#include "moduleMgr.h"
#include "pendingPeekRequests.h"
#include "pendingMMapRequests.h"
#include "pendingRegAnalysisRequests.h"

#ifdef sparc_sun_solaris2_7
#include "groupMgr.h"
#endif

#include "fnCodeObjects.h"
#include "kapi.h"
#include <utility> // pair

extern moduleMgr *global_moduleMgr;
extern kapi_manager kmgr;

bool receivedModuleInformation = false; // kludge

static kapi_manager *callbackDispatcher; // yet another one
void setCallbackDispatcher(kapi_manager *iCallbackDispatcher)
{
    callbackDispatcher = iCallbackDispatcher;
}

void kerninstdClientUser::begin_module_information() {
}

void kerninstdClientUser::module_information(const trick_module &trick_mod) {
   // trick_mod has been created, as usual, by P_xdr_recv(..., trick_module &)
   // The implementation of that routine has intentionally new'd an underlying
   // "loadedModule" as a member of class trick_module, and has graciously transferred
   // ownership of that object to us -- it (the trick_module) promises not to call
   // "delete" on the underlying loadedModule.
   // The bottom line: the loadedModule is allocated, primed, and ready to go!
   // Yea!

   const loadedModule *mod = trick_mod.get();

   moduleMgr &theModuleMgr = *global_moduleMgr;

   loadedModule *mod_nonconst = const_cast<loadedModule*>(mod);

   theModuleMgr.addAllocatedModule(mod_nonconst);
      // append_allocated_module: param must be a loadedModule that was initialized
      // with 'new'.  And so it is, thanks to P_xdr_recv(..., trick_module &).
      // Does not make a copy of the underlying module, so this should be blazingly
      // fast.  All of the expense is pretty much in P_xdr_recv(xdr, trick_module &)

   // Call the user-supplied callback (if any)
   callbackDispatcher->dispatchDynloadEvent(kapi_module_added, mod, 0);
}

void kerninstdClientUser::end_module_information() {
   receivedModuleInformation = true;
}

// --------------------

void kerninstdClientUser::one_function_information(const pdstring &modName,
                                                   const trick_fn &trick_fn) {
   // The implementation of trick_fn's P_xdr_recv routine has new'd an underlying
   // object, filled via XDR, and handed it (the pointer) to us for ownership.  Thanks!

    const moduleMgr &theModuleMgr = *global_moduleMgr;
    const loadedModule *mod = theModuleMgr.find(modName);
    assert(mod != NULL &&
	   "one_function_information igen call: module must already exist");
   
   function_t *fn = const_cast<function_t*>(trick_fn.get());

   global_moduleMgr->addAFullyAllocatedFn(modName, fn); 
   
   dout << "received a one_function_information; put "
        << fn->getPrimaryName() << " into module " << modName << endl;

   // Call the user-supplied callback (if any)
   callbackDispatcher->dispatchDynloadEvent(kapi_function_added, mod, fn);
}

void kerninstdClientUser::delete_one_function(kptr_t fnEntryAddr) {
   pair<pdstring, function_t*> modAndFn =
       global_moduleMgr->findModAndFn(fnEntryAddr, true);
   const pdstring &modName = modAndFn.first;
   loadedModule *mod = global_moduleMgr->find(modName);
   function_t *fn = modAndFn.second;

   dout << "kerninstdClientUser::delete_one_function() "
        << addr2hex(fnEntryAddr) << endl;

   // Call the user-supplied callback (if any)
   callbackDispatcher->dispatchDynloadEvent(kapi_function_removed, mod, fn);

   // Now truly fry the function. The 3rd param is used to remove items
   // from the callGraph.
   global_moduleMgr->fry1fn(mod, fn, fn->getOrigCode());
   

   if (mod->numFns() == 0) {
       // The module has no remaining functions, in which case
       // it should be fried, too.
       
       // Call the user-supplied callback (if any)
       callbackDispatcher->dispatchDynloadEvent(kapi_module_removed, mod, 0);
       global_moduleMgr->fryEmptyModule(mod->getName());
       dout << "kerninstdClientUser::delete_one_function() fried an entire "
	    << "module because it now has 0 fns" << endl;
   }
}

// --------------------

void kerninstdClientUser::peek_response(unsigned request_id,
                                        const pdvector<chunkData> &data) {
   // chunkData is defined in .I file
   pendingPeekRequest::processResponse(request_id, data);
}

void kerninstdClientUser::parseFnIntoCodeObjectsResponse(
    unsigned reqid, const fnCodeObjects &fco)
{
#ifdef sparc_sun_solaris2_7
    extern groupMgr *theGlobalGroupMgr;
    assert(theGlobalGroupMgr);
    theGlobalGroupMgr->handle1FnCodeObjects(reqid, fco);
#endif
}

void kerninstdClientUser::regAnalysisResponse(unsigned reqid,
                                              const pdvector<dataflowFn1Insn> &data) {
   pendingRegAnalysisRequest::processResponse(reqid, data);
}

void kerninstdClientUser::
mMapOfKernelSymIntoKerninstdResponse(unsigned reqid,
                                     dptr_t addrInKerninstd) {
   pendingMMapRequest::processResponse(reqid, addrInKerninstd);
}

void kerninstdClientUser::
presentSampleData(const pdvector<one_sample_data> &data) {
//     cout << "kerninstdClientUser::presentSampleData receipt of " << data.size() << " elems:" << endl;
//     for (pdvector<one_sample_data>::const_iterator iter = data.begin();
//          iter != data.end(); ++iter) {
//        const one_sample_data &d = *iter;
//        cout << "   time=" << d.timeOfSample << " val=" << d.value << endl;
//     }
   
   
   
   callbackDispatcher->dispatchSampleData(&data);
}
