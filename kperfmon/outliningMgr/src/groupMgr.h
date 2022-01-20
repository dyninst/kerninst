#ifndef _GROUP_MGR_H_
#define _GROUP_MGR_H_

#include <inttypes.h> // uint32_t
#include <utility> // pair

#include "util/h/Dictionary.h"
#include "outlinedGroup.h"

class groupMgr {
    kerninstdClientUser *connection_to_kerninstd;

    dictionary_hash<kptr_t, outlinedGroup*> allGroups;
    // key: entryAddr of the root function of a given outlined group
    // value: the group (allocated with new)

    unsigned nextParseCodeObjectsReqId;
    dictionary_hash<unsigned, kptr_t> codeObjParseReqId2GroupRootFn;

    // prevent copying:
    groupMgr(const groupMgr &);
    groupMgr &operator=(const groupMgr &);

 public:
    groupMgr(kerninstdClientUser *iconnection_to_kerninstd);
    ~groupMgr();

    // dealing with code object requests (this class handles reqids thereto).
    unsigned reqParseFnIntoCodeObjects(
	const outlinedGroup &groupMakingRequest, kptr_t fnEntryAddr,
	const pdvector<fnAddrBeingRelocated> &fnsBeingRelocated);
    // returns reqid
   
    void handle1FnCodeObjects(unsigned reqid, const fnCodeObjects &fco);

    void asyncOutlineGroup(
	bool doTheReplaceFunction,
	bool replaceFunctionPatchUpCallSitesToo,
	outlinedGroup::blockPlacementPrefs,
	hotBlockCalcOracle::ThresholdChoices,
	kptr_t rootFnAddr,
	const dictionary_hash<kptr_t, pdvector<unsigned> > &fnBlockCounters,
	const pdvector<kptr_t> &forceIncludeDescendants);

    void unOutlineGroup(kptr_t rootFnEntryAddr);

    void unOutlineAll();
};

#endif
