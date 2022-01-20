#include "funkshun.h"
#include "kapi.h"

kapi_function::kapi_function(const function_t *func) :
    theFunction(func)
{}

// Return the address of the entry point
kptr_t kapi_function::getEntryAddr() const
{
    assert(theFunction != 0);
    return theFunction->getEntryAddr();
}

ierr kapi_function::findEntryPoint(kapi_vector<kapi_point> *points) const
{
    kptr_t addr = theFunction->getEntryAddr();
    points->push_back(kapi_point(addr, kapi_pre_instruction));

    return 0;
}

ierr kapi_function::findExitPoints(kapi_vector<kapi_point> *points) const
{
    pdvector<kptr_t> addrs = theFunction->getCurrExitPoints();
    pdvector<kptr_t>::const_iterator iter = addrs.begin();
    for (; iter != addrs.end(); iter++) {
	points->push_back(kapi_point(*iter, kapi_pre_return));
    }

    return 0;
}

// Fill-in the name of the function. Returns not_enough_space if
// max_bytes is insufficient to store the name
ierr kapi_function::getName(char *name, unsigned max_bytes) const
{
    StaticString func_name = theFunction->getPrimaryName();
    unsigned len = func_name.size();
    if (len+1 > max_bytes) {
	return not_enough_space;
    }
    strcpy(name, func_name.c_str());
    return 0;
}

// Return the number of basic blocks in the function
unsigned kapi_function::getNumBasicBlocks() const
{
    return theFunction->numBBs();
}

// Find the basic block by its id and initialize *bb with it
ierr kapi_function::findBasicBlockById(bbid_t bbid,
				       kapi_basic_block *kBB) const
{
    const basicblock_t *bb = theFunction->getBasicBlockFromId(bbid);
    *kBB = kapi_basic_block(bb, theFunction);

    return 0;
}

// Return a vector of all basic blocks in the function
// delete[] the returned kapi_basic_block* to free memory when done
ierr kapi_function::getAllBasicBlocks(
    kapi_vector<kapi_basic_block> *pAllBlocks) const
{
    function_t::const_iterator iter = theFunction->begin();
    function_t::const_iterator finish = theFunction->end();
    for (; iter != finish; iter++) {
	pAllBlocks->push_back(kapi_basic_block(*iter, theFunction));
    }
/*
    for (unsigned i=0; i<numBlocks && iter != finish; i++, iter++) {
        allBlocks[i] = kapi_basic_block(*iter, theFunction);
*/
    return 0;
}

// Some functions can not be parsed (and hence instrumented) at
// this time
bool kapi_function::isUnparsed() const
{
    return theFunction->isUnparsed();
}

// Retrieve bbid by the starting address. Return unsigned(-1) if
// not found
bbid_t kapi_function::getBasicBlockIdByAddress(kptr_t addr) const
{
    return theFunction->searchForBB(addr);
}

// There can be several names mapping to the same address
unsigned kapi_function::getNumAliases() const
{
    return theFunction->getNumAliases();
}

// Fill-in the name of the i-th alias
ierr kapi_function::getAliasName(unsigned ialias, char *buffer,
				 unsigned buflen) const
{
    const StaticString &theAlias = theFunction->getAliasName(ialias);
    const char *str = theAlias.c_str();
    if (strlen(str) > buflen) {
	return not_enough_space;
    }
    strcpy(buffer, str);

    return 0;
}

// Monster method: find and report all callees of this
// function. Both regular calls and interprocedural branches are
// located. If the blocks argument is not NULL, only calls made in these
// blocks are reported.
ierr kapi_function::getCallees(const kapi_vector<bbid_t> *blocks,
			       kapi_vector<kptr_t> *regCallsFromAddr,
			       kapi_vector<kptr_t> *regCallsToAddr,
			       kapi_vector<kptr_t> *interProcBranchesFromAddr,
			       kapi_vector<kptr_t> *interProcBranchesToAddr,
			       kapi_vector<kptr_t> *unAnalyzableCallsFromAddr)
    const
{
    pdvector< pair<kptr_t, kptr_t> > regCalls, interProcBranches;
    pdvector<kptr_t> unAnalyzableCalls;

    if (blocks == 0) {
	theFunction->getCallsMadeBy_WithSuppliedCode(
	    theFunction->getOrigCode(), regCalls,
	    true, interProcBranches,
	    true, unAnalyzableCalls);
    }
    else {
	pdvector<bbid_t> bbids;
	kapi_vector<bbid_t>::const_iterator ibid = blocks->begin();
	for (; ibid != blocks->end(); ibid++) {
	    bbids.push_back(*ibid);
	}
	theFunction->getCallsMadeBy_WithSuppliedCodeAndBlocks(
	    theFunction->getOrigCode(), bbids, regCalls,
	    true, interProcBranches,
	    true, unAnalyzableCalls);
    }

    // Split the vector of pairs into two vectors
    pdvector< pair<kptr_t, kptr_t> >::const_iterator ipair = regCalls.begin();
    for (; ipair != regCalls.end(); ipair++) {
	regCallsFromAddr->push_back(ipair->first);
	regCallsToAddr->push_back(ipair->second);
    }

    // Split the vector of pairs into two vectors
    pdvector< pair<kptr_t, kptr_t> >::const_iterator jpair = 
	interProcBranches.begin();
    for (; jpair != interProcBranches.end(); jpair++) {
	interProcBranchesFromAddr->push_back(jpair->first);
	interProcBranchesToAddr->push_back(jpair->second);
    }

    // Convert pdvector to kapi_vector
    pdvector<kptr_t>::const_iterator isingle = unAnalyzableCalls.begin();
    for (; isingle != unAnalyzableCalls.end(); isingle++) {
	unAnalyzableCallsFromAddr->push_back(*isingle);
    }

    return 0;
}

