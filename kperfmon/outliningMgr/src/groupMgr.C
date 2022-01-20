#include "groupMgr.h"
#include "moduleMgr.h"
#include "util/h/hashFns.h"

groupMgr::groupMgr(kerninstdClientUser *iconnection_to_kerninstd) : 
    connection_to_kerninstd(iconnection_to_kerninstd),
    allGroups(addrHash4), nextParseCodeObjectsReqId(0),
    codeObjParseReqId2GroupRootFn(unsignedIdentityHash)
{
}

groupMgr::~groupMgr()
{
    // Any pendingCodeObjectParseRequests[] will be dropped on the floor.
    // Don't assert that pendingCodeObjectParseRequests.size() is 0.  Same
    // for "codeObjectParseRequestsNotYetSent[]".

    // Note: instead of doing the following work, we could probably just
    // assert that allGroups[].size() is zero, which is simply asserting that
    // kperfmonIsGoingDownNow() was properly called.
    dictionary_hash<kptr_t, outlinedGroup*>::const_iterator iter = 
	allGroups.begin();
    dictionary_hash<kptr_t, outlinedGroup*>::const_iterator finish = 
	allGroups.end();
    for (; iter != finish; ++iter) {
	outlinedGroup *theGroup = *iter;
	delete theGroup;
    }
}

unsigned groupMgr::reqParseFnIntoCodeObjects(
    const outlinedGroup &groupMakingRequest, kptr_t fnEntryAddr,
    const pdvector<fnAddrBeingRelocated> &fnsBeingRelocated)
{
   const unsigned reqid = nextParseCodeObjectsReqId++;

   connection_to_kerninstd->reqParseFnIntoCodeObjects(reqid, fnEntryAddr,
                                                      fnsBeingRelocated);
      // note: fnsBeingRelocated should have fn names of the format
      // "modName/fnName".

   codeObjParseReqId2GroupRootFn.set(reqid,
                                     groupMakingRequest.getRootFnEntryAddr());

   return reqid;
}

void groupMgr::handle1FnCodeObjects(unsigned reqid, const fnCodeObjects &fco)
{
    // called from kerninstdClientUser.C (for now at least)
    // We need to find the group to forward "fco" to, and forward it.

    kptr_t rootFnEntryAddr = 0;
    if (!codeObjParseReqId2GroupRootFn.find(reqid, rootFnEntryAddr)) {
	cout << "Received code object info for an unknown reqid "
	     << reqid << " -- ignoring" << endl;
	return;
    }
    codeObjParseReqId2GroupRootFn.undef(reqid);

    outlinedGroup *theGroup = NULL;
    if (!allGroups.find(rootFnEntryAddr, theGroup)) {
	cout << "Received code object info for an unknown group -- ignoring\n";
	return;
    }

    theGroup->handle1FnCodeObjects(reqid, fco);
}

void groupMgr::asyncOutlineGroup(
    bool doTheReplaceFunction,
    bool replaceFunctionPatchUpCallSitesToo,
    outlinedGroup::blockPlacementPrefs placementPrefs,
    hotBlockCalcOracle::ThresholdChoices thresholdPrefs,
    kptr_t rootFnAddr,
    const dictionary_hash<kptr_t, pdvector<unsigned> > &fnBlockCounters,
    const pdvector<kptr_t> &forceIncludeDescendants)
{
    if (allGroups.defines(rootFnAddr)) {
	cout << "That function is already being outlined. If you want to redo"
	     << endl
	     << "the outlining, first \"unOutline\" that function." << endl;
	return;
    }

    extern moduleMgr *global_moduleMgr;
    assert(global_moduleMgr != 0);
    pair<pdstring, function_t*> modfn = 
	global_moduleMgr->findModAndFn(rootFnAddr, true); // start only

    outlinedGroup *theOutlinedGroup =
	new outlinedGroup(doTheReplaceFunction,
			  replaceFunctionPatchUpCallSitesToo,
			  placementPrefs,
			  thresholdPrefs,
			  modfn.first, *(modfn.second),
			  forceIncludeDescendants,
			  fnBlockCounters,
			  *this,
			  connection_to_kerninstd);
    assert(theOutlinedGroup);
    allGroups.set(rootFnAddr, theOutlinedGroup);

    theOutlinedGroup->asyncOutline();
}

void groupMgr::unOutlineGroup(kptr_t rootFnEntryAddr)
{
    outlinedGroup *theOutlinedGroup = NULL;
    if (!allGroups.find(rootFnEntryAddr, theOutlinedGroup)) {
	cout << "That group is not currently outlined; ignoring" << endl;
	return;
    }
    assert(theOutlinedGroup != NULL);

    theOutlinedGroup->unOutline();
    // does: un-replaceFn, removes downloaded chunks.

    delete theOutlinedGroup;
    theOutlinedGroup = NULL;

    (void)allGroups.get_and_remove(rootFnEntryAddr);
}

void groupMgr::unOutlineAll()
{
    dictionary_hash<kptr_t, outlinedGroup*>::const_iterator iter =
	allGroups.begin();
    for (; iter != allGroups.end(); iter++) {
	outlinedGroup *theOutlinedGroup = iter.currval();
	theOutlinedGroup->unOutline();
	delete theOutlinedGroup;
    }
    allGroups.clear();
}
