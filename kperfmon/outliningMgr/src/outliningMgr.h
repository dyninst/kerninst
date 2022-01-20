// outliningMgr.h
// The main "entry-point" class for handling outlining a group of functions
//

#ifndef _OUTLINING_MGR_H_
#define _OUTLINING_MGR_H_

#include <inttypes.h> // uint32_t
#include <utility> // pair
#include "kapi.h"

#include "util/h/Dictionary.h"
#include "outlining1FnCounts.h"
#include "blockCounters.h"
using std::pair;

class outliningMgr {
 private:
   // preferences:
   bool doTheReplaceFunction;
   bool replaceFunctionPatchUpCallSitesToo;
   
   unsigned outliningBlockCountDelay;
      // in millisecs
   bool doChildrenToo;
      // usually true, make it false if you just want to test outlining
      // for a single function w/o outlining any callees (e.g. to test a bug fix
      // that can be repeated when just outlining a single fn).
   bool forceOutliningOfAllChildren;
      // usually false; set to true to ensure inclusion in the inlined/outlined group
      // of all children (callees, including those via interprocedural branches).
      // Usually, only those callees that are called from hot blocks are included,
      // which can make it hard to torture test their inclusion until, in practice,
      // the block calling them is indeed measured as hot.  This bypasses such
      // a requirement.
   kapi_manager::blockPlacementPrefs theBlockPlacementPrefs;

   bool usingFixedDescendants;
      // iff true, then we'll only outline the descendants called in hot blocks
      // that are also present in the tcl variable
      // fixedOutlineDescendants("$modName/$fnName")
   
   kapi_manager::ThresholdChoices theHotBlockThresholdPrefs;

   dictionary_hash<kptr_t, blockCounters*> allBlockCounters;
      // key: entryAddr of the root function of a given outlined group
      // value: the counter for a group of functions (allocated with new)

   // prevent copying:
   outliningMgr(const outliningMgr &);
   outliningMgr &operator=(const outliningMgr &);

 public:
   outliningMgr(// the params are "preferences"; see the .h file for
                // a description:
                bool doTheReplaceFunction,
                bool replaceFunctionPatchUpCallSitesToo,
                unsigned outliningBlockCountDelay,
                bool doChildrenToo,
                bool forceOutliningOfAllChildren,
                kapi_manager::blockPlacementPrefs,
                bool usingFixedDescendants,
                kapi_manager::ThresholdChoices);
  ~outliningMgr();

   void kperfmonIsGoingDownNow();
      // clean everything up...remove leftover instrumentation.  dtor will assert
      // as such, so don't skimp on this step.

   // dealing with prefs:
   void changeOutliningDoTheReplaceFunctionFlag(bool);
   bool getOutliningDoTheReplaceFunctionFlag() const;

   void changeOutliningReplaceFunctionPatchUpCallSitesTooFlag(bool);
   bool getOutliningReplaceFunctionPatchUpCallSitesTooFlag() const;

   void changeOutliningBlockCountDelay(unsigned msecs);
   unsigned getOutliningBlockCountDelay() const; // returns a value in millisecs

   void changeOutlineChildrenTooFlag(bool);
   bool getOutlineChildrenTooFlag() const;
   
   void changeForceOutliningOfAllChildrenFlag(bool);
   bool getForceOutliningOfAllChildrenFlag() const;

   void changeOutliningBlockPlacementTo(kapi_manager::blockPlacementPrefs);
   void changeOutliningBlockPlacementToOrigSeq();
   void changeOutliningBlockPlacementToChains();

   void changeUsingFixedDescendantsFlag(bool);
   bool getUsingFixedDescendantsFlag() const;

   void changeOutliningHotBlocksThreshold(kapi_manager::ThresholdChoices);
   kapi_manager::ThresholdChoices getOutliningHotBlocksThreshold() const;

   // Will be called when all block counts for a group of functions have
   // been received
   void doneCollectingBlockCounts(
       kptr_t fnEntryAddr,
       const dictionary_hash<kptr_t, outlining1FnCounts*> &fnBlockCounters,
       const kapi_vector<kptr_t> forceIncludeDescendants);

   // dealing with groups: (the main part of this class)
   void asyncOutlineFn(
       const pdstring &modName, const kapi_function &fn,
       const pdvector< pair<pdstring, kapi_function> > &fixedDescendants,
       const pdvector< pair<pdstring, kapi_function> > &forceIncludeDescendants,
       const pdvector< pair<pdstring, kapi_function> > &forceExcludeDescendants);

   // Read the profile from disk and outline the functions specified there
   void asyncOutlineUsingProfile(const char *profileFileName);

   void unOutlineGroup(const pdstring &modName, const kapi_function &fn);
};

#endif
