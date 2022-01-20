// loadedModule.C
// Ariel Tamches
// Manages a bunch of function_t's; useful to detect things
// like the gap between functions, and whether we can safely parse functions
// that the symbol table claims are 0 bytes in size.

#include "loadedModule.h"
#include <algorithm> // stl's sort()
#include "instr.h"
#include "util/h/xdr_send_recv.h" // xdr recv for string
#include "util/h/hashFns.h" // cStrHash

template <class T>
static void destruct1(T &t) {
   t.~T();
}

// Sorry for this hack; it's needed by function's XDR ctor since a module
// in the process of being received will not be found by a conventional search.
// Worse yet, because the parameters of several igen messages can be read & buffered
// before the first igen call is processed, we also need a "mostRecentlyReceivedModule"
// hack.  See function::function(XDR *) for main uses of these vrbles (actually, see
// the routines in fn_misc.[hC])
loadedModule *presentlyReceivingModule = NULL;
   // set at entry to loadedModule::loadedModule(XDR *), reset to NULL at exit to same
loadedModule *mostRecentlyReceivedModule = NULL;
   // set as presentlyReceivingModule is being reset to NULL at end of
   // loadedModule::loadedModule(XDR *).  Never set to NULL except at program
   // initialization.

// ----------------------------------------------------------------------

static unsigned StaticStringHash(const StaticString &s) {
   const char *str = s.c_str();
   return cStrHash(str);
}

loadedModule::loadedModule(XDR *xdr) :
   theStringPool(StringPool::Dummy()),
   name2FuncEntryAddr_whenUnique(StaticStringHash),
   name2SeveralFuncEntryAddrs(StaticStringHash)
{
   // format (see kerninstd's xdr_send_recv_kerninstd_only.C
   //    P_xdr_send(xdr, const loadedModule &) for the send side):
   // module name [string], description [string], string pool [StringPool]
   // the functions (each sent with kerninstd's P_xdr_send(xdr, const sparc_fn &))

   destruct1(modName);
   destruct1(modDescription);
   
   if (!P_xdr_recv(xdr, modName) ||
       !P_xdr_recv(xdr, modDescription))
      throw xdr_recv_fail();
   
   destruct1(theStringPool);
   if (!P_xdr_recv(xdr, theStringPool))
      throw xdr_recv_fail();

   destruct1(TOCPtr);
   if (!P_xdr_recv(xdr, TOCPtr))
      throw xdr_recv_fail();
   // fnsvec[] is presently an empty vector.  Let's construct it in place for
   // maximum efficiency.

   uint32_t num_fns;
   if (!P_xdr_recv(xdr, num_fns))
      throw xdr_recv_fail();

//     cout << "module " << modName << "-- about to receive "
//          << num_fns << " functions" << endl;

   presentlyReceivingModule = this;
   
   for (unsigned fnlcv=0; fnlcv < num_fns; ++fnlcv) {
      function_t *fn = function_t::getFunction(xdr);

      addAFullyInitializedFn(fn);
        // appends to fns_vec[]; adds to name2*FuncEntryAddr*
   }
//     assert(name2FuncEntryAddr_whenUnique.size() + name2SeveralFuncEntryAddrs.size() >=
//            fns_vec.size());
      // Due to aliasing (one function at a given address having lots of different
      // names), there can be several entries in the name dictionaries, but only
      // one entry in fns_vec[].  So the assert must be: ">=", not "==".
      // But we have to comment out the assert right now, because
      // name2SeveralFuncEntryAddrs.size() will not return a high enough result;
      // we need it to return the total number of entries in all of the vectors, not
      // just the number of vectors.

   uint32_t checker;
   if (!P_xdr_recv(xdr, checker))
      throw xdr_recv_fail();
   
   assert(checker == 0x11111111);

   presentlyReceivingModule = NULL;
      // don't forget this!
   mostRecentlyReceivedModule = this;
}

loadedModule::loadedModule(const pdstring &iModName,
                           const pdstring &iModDescription,
                           const char *pool, unsigned long pool_nbytes,
                           kptr_t iTOCPtr) :
                           //HomogenousHeapUni<function_t,1> *iFnHeap,
                           //HomogenousHeapUni<basicblock_t,1> *) :
                modName(iModName),
                modDescription(iModDescription),
                theStringPool(pool, pool_nbytes),
                name2FuncEntryAddr_whenUnique(StaticStringHash),
                name2SeveralFuncEntryAddrs(StaticStringHash),
                TOCPtr(iTOCPtr)
   //allFuncsByName(StaticStringHash)
{
   //fnHeap = iFnHeap;
   
   // fns_vec left as an empty vector
   // staticString heap an empty vector too
}

loadedModule::~loadedModule() {
   iterator fniter = fns_vec.begin();
   iterator fnfinish = fns_vec.end();
   for (; fniter != fnfinish; ++fniter) {
      function_t *fn = *fniter;

      delete fn;

      *fniter = NULL; // help purify find mem leaks
   }
   fns_vec.clear(); // hardly necessary, since we're already in the dtor, but no harm

   name2FuncEntryAddr_whenUnique.clear();
   name2SeveralFuncEntryAddrs.clear();
//   allFuncsByName.clear(); // also hardly necessary, but harmless

   pdvector<const char *>::const_iterator striter = runtimeAddedStringPool.begin();
   pdvector<const char *>::const_iterator strfinish = runtimeAddedStringPool.end();
   for (; striter != strfinish; ++striter) {
      const char *s = *striter;
      delete s;
   }
}
   
void loadedModule::getMemUsage(unsigned &forBasicBlocksGrandTotal,
                               unsigned &forFnOrigCodesGrandTotal,
                               unsigned &forStringPools,
                               unsigned &forLiveRegAnalysisInfo,
                               unsigned &liveRegAnalysisInfo_num1Levels,
                               unsigned &liveRegAnalysisInfo_num2Levels,
                               unsigned &liveRegAnalysisInfo_numOtherLevels,
                               unsigned &forFnStructAndSmallOther
                               ) const {
   unsigned forBasicBlocks = 0;
   unsigned forFnOrigCodes = 0;
   
   const_iterator fniter = begin();
   const_iterator fnfinish = end();
   for (; fniter != fnfinish; ++fniter) {
      const function_t *fn = *fniter;
      
      fn->getMemUsage(forBasicBlocks,
                      forFnOrigCodes,
                      forLiveRegAnalysisInfo,
                      liveRegAnalysisInfo_num1Levels,
                      liveRegAnalysisInfo_num2Levels,
                      liveRegAnalysisInfo_numOtherLevels
                      );

      forFnStructAndSmallOther += sizeof(function_t);
      forFnStructAndSmallOther += fn->getNumAliases() * sizeof(StaticString);
   }

   forBasicBlocksGrandTotal += forBasicBlocks;
   forFnOrigCodesGrandTotal += forFnOrigCodes;

   forStringPools += theStringPool.getNumBytes();
}


function_t *loadedModule::addButDontParse(kptr_t startAddr,
					  const StaticString &fnName) {
   function_t *newFn = function_t::getFunction(startAddr, fnName);

   addAFullyInitializedFn(newFn);
      // adds to fns_vec[] and to name2*FuncEntryAddr*
      // NOTE: a fn with this addr may already exist, in which case we'll (later)
      // remove all duplicates (and note the aliased names in the sole survivor)

   return newFn;
}

void loadedModule::
add1NameToAddrMapping(const StaticString &str, kptr_t fnEntryAddr) {
   if (!name2FuncEntryAddr_whenUnique.defines(str)) {
      // There isn't exactly 1 entry
      if (name2SeveralFuncEntryAddrs.defines(str)) {
         // there are already >= 2 entries; add another
         pdvector<kptr_t> &v = name2SeveralFuncEntryAddrs.get(str);
         assert(std::find(v.begin(), v.end(), fnEntryAddr) == v.end());
            // duplicate names are allowed, but only if the addresses differ.
         v += fnEntryAddr;
      }
      else {
         // There were 0 entries.  Add the first.
         name2FuncEntryAddr_whenUnique.set(str, fnEntryAddr);
      }
   }
   else {
      // There was exactly 1 entry; make it two.
      assert(!name2SeveralFuncEntryAddrs.defines(str));
      pdvector<kptr_t> v;
      v += name2FuncEntryAddr_whenUnique.get_and_remove(str);
#ifndef rs6000_ibm_aix5_1 //duplicate symbols happen to exist on power
      assert(v[0] != fnEntryAddr);
         // duplicate names are allowed, but only if the addresses differ.
#endif
      v += fnEntryAddr;
      name2SeveralFuncEntryAddrs.set(str, v);
   }
}

void loadedModule::
remove1NameFromAddrMapping(const StaticString &fnName, kptr_t fnEntryAddr) {
   if (!name2FuncEntryAddr_whenUnique.defines(fnName)) {
      // There wasn't exactly 1 entry
      if (name2SeveralFuncEntryAddrs.defines(fnName)) {
         // There are already >= 2 entries.  Maybe it'll now be down to 1; maybe not
         pdvector<kptr_t> &v = name2SeveralFuncEntryAddrs.get(fnName);
         pdvector<kptr_t>::iterator iter = std::find(v.begin(), v.end(), fnEntryAddr);
         assert(iter != v.end());
         *iter = v.back();
         v.pop_back();

         // Now 1 entry?  If so, remove from name2SeveralFuncEntryAddrs[] and
         // add it to name2FuncEntryAddr_whenUnique[]
         assert(v.size() >= 1);
         if (v.size() == 1) {
            const kptr_t remainingAddr = v[0];
            (void)name2SeveralFuncEntryAddrs.get_and_remove(fnName);
            name2FuncEntryAddr_whenUnique.set(fnName, remainingAddr);
         }
         else {
            // Still >= 2 entries; keep in name2SeveralFuncEntryAddrs[].
         }
      }
      else {
         // There were 0 entries.  Not found.
         assert(false);
      }
   }
   else {
      // There was exactly 1 entry.  Fry it
      assert(!name2SeveralFuncEntryAddrs.defines(fnName));
      (void)name2FuncEntryAddr_whenUnique.get_and_remove(fnName);
   }
}

void loadedModule::addAFullyInitializedFn(function_t *fn) {
   fns_vec += fn;

   // Add to "name2*FuncEntryAddr*[]": the primary name and all of its duplicates
   const StaticString &thePrimaryName = fn->getPrimaryName();
   add1NameToAddrMapping(thePrimaryName, fn->getEntryAddr());
      // duplicate names are allowed
      
   const unsigned num_aliases = fn->getNumAliases();
   for (unsigned alias_lcv=0; alias_lcv < num_aliases; ++alias_lcv) {
      const StaticString &theAliasName = fn->getAliasName(alias_lcv);
      add1NameToAddrMapping(theAliasName, fn->getEntryAddr());
         // duplicate names are allowed: this is a multi-hashmap
   }
}

function_t * 
loadedModule::addButDontParse(kptr_t startAddr,
			      const pdvector<StaticString> &fnNames) {
   function_t *newFn = function_t::getFunction(startAddr, fnNames);

   addAFullyInitializedFn(newFn);
      // adds to fns_vec[] and to name2*FuncEntryAddr*[]
      // NOTE: a fn with this addr may already exist, in which case we'll (later)
      // remove all duplicates (and note the aliased names in the sole survivor)

   return newFn;
}

static bool staticStringIsUsedByFunction(const char *str,
                                         const function_t *fn) {
   if (fn->getPrimaryName().c_str() == str)
      // yes, we can (and in fact should) directly compare pointers
      return true;

   unsigned num = fn->getNumAliases();
   for (unsigned lcv=0; lcv < num; ++lcv) {
      if (fn->getAliasName(lcv).c_str() == str)
         return true;
   }

   return false;
}

void loadedModule::fry1fn(function_t *fn) {
   // remove from "name2*FuncEntryAddr*[]":
   remove1NameFromAddrMapping(fn->getPrimaryName(), fn->getEntryAddr());

   const unsigned num_aliases = fn->getNumAliases();
   for (unsigned alias_lcv=0; alias_lcv < num_aliases; ++alias_lcv) {
      const StaticString &theAliasName = fn->getAliasName(alias_lcv);
      remove1NameFromAddrMapping(theAliasName, fn->getEntryAddr());
   }

   // we're gonna be _real_ lazy in updating fns_vec.  But then, nobody
   // said this routine should be well-optimized.
   pdvector<function_t*> newvec(fns_vec.size()-1);
   unsigned ndx=0;
   for (unsigned lcv=0; lcv < fns_vec.size(); lcv++)
      if (fns_vec[lcv] != fn) // check to skip the removed element
         newvec[ndx++] = fns_vec[lcv];

   assert(newvec.size() == fns_vec.size()-1);
   fns_vec = newvec;

   // If "fn" contains anything from runtimeAddedStringPool then remove it now.
   // TODO: optimize
   unsigned numDeleted = 0;
   pdvector<const char *>::iterator iter = runtimeAddedStringPool.begin();
   pdvector<const char *>::iterator finish = runtimeAddedStringPool.end();
   while (iter != finish) { // note that finish changes on the fly
      const char *str = *iter;
      if (staticStringIsUsedByFunction(str, fn)) {
         delete [] str;
         
         *iter = runtimeAddedStringPool.back();
         ++numDeleted;
         --finish;
      }
      else
         // only in this case should we advance the iterator
         ++iter;
   }
   if (numDeleted > 0)
      runtimeAddedStringPool.shrink(runtimeAddedStringPool.size() - numDeleted);

   delete fn;
}

const char *loadedModule::addToRuntimeStringPool(const pdstring &str) {
   char *newStr = new char[str.length() + 1];
   strcpy(newStr, str.c_str());
   
   runtimeAddedStringPool += newStr;
   
   return newStr;
}


template <class function_t>
class fnptrcmp {
 public:
   bool operator()(const function_t *f1, const function_t *f2) const {
      return f1->getEntryAddr() < f2->getEntryAddr();
   }
};

//  template <class function_t>
//  static bool fnPtrNotNull(function_t *fn) {
//     // predicate used in an stl count_if(), below.
//     return fn != NULL;
//  }
static bool fnPtrNotNull(function_t *fn) {
   return fn != NULL;
}

// Mysteriously left out of stl (Stroustrup 3d, p. 530) so we define it.
template <class In, class Out, class Pred>
Out copy_if(In first, In last, Out res, Pred p) {
   while (first != last) {
      if (p(*first))
         *res++ = *first;
      ++first;
   }
   return res;
}

void loadedModule::removeAliases() {
   // the fact that we sort is indicental.  We still advertise that fns_vec[] isn't
   // every really kept sorted.
   std::sort(fns_vec.begin(), fns_vec.end(), fnptrcmp<function_t>());
      // we sort by fn entry addr.  It doesn't really matter what we sort by,
      // as long as it leaves aliases next to each other.
   
   for (iterator iter=fns_vec.begin();
        iter != fns_vec.end(); iter++) {
      function_t *fn = *iter;
      if (fn == NULL)
	 continue; // skip over fried alias

      for (iterator iter2 = iter+1;
           iter2 != fns_vec.end(); iter2++) {
	 function_t *alias_fn = *iter2;

	 if (alias_fn->getEntryAddr() != fn->getEntryAddr())
	    break;

	 // We have an alias.
         // Interestingly, we don't need to modify name2*FuncEntryAddr*[], since both
         // the key (the name) and the value (fn entry addr) would be unchanged.

	 fn->fnHasAlternateName(alias_fn->getPrimaryName());

         // destroy the alias function:
	 delete alias_fn;
         *iter2 = NULL;
      }
   }

   // Now that aliases are marked as NULL in the vector, we can obtain
   // an accurate count for the # of fns.
//     unsigned numFns = count_if(begin(), end(), fnPtrNotNull<function_t>);
//     unsigned numFns = count_if(begin(), end(), fnPtrNotNull);
   unsigned numFns = 0;
   for (const_iterator iter = fns_vec.begin();
        iter != fns_vec.end(); ++iter) {
      function_t *fn = *iter;
      if (fn)
         ++numFns;
   }

   // Allocate new fns_vec & fill in fns
   pdvector<function_t*> replacement_fns_vec(numFns);
   
//     iterator the_end = copy_if(begin(), end(), replacement_fns_vec.begin(),
//                                fnPtrNotNull<function_t>);
   iterator the_end = copy_if(fns_vec.begin(), fns_vec.end(),
                              replacement_fns_vec.begin(),
                              fnPtrNotNull);
   assert(the_end - replacement_fns_vec.begin() == numFns);

   fns_vec = replacement_fns_vec;
   
   // Assert no more aliases left & sorted properly
   // (sorted properly?  I thought that fns_vec[] was no longer kept sorted)x
   if (fns_vec.size())
      for (const_iterator iter = fns_vec.begin(); ; ) {
         function_t *fn1 = *iter++;
         assert(fn1 != NULL);
         
         if (iter == fns_vec.end())
            break; // nothing left
         
         function_t *fn2 = *iter;
         assert(fn2 != NULL);
         
         assert(fn1->getEntryAddr() < fn2->getEntryAddr());
      }
   
}

pdvector<kptr_t>
loadedModule::find_fn_by_name(const StaticString &fnName) const {
   // sorry, can't safely return a reference.
   if (name2FuncEntryAddr_whenUnique.defines(fnName)) {
      assert(!name2SeveralFuncEntryAddrs.defines(fnName));
      pdvector<kptr_t> result;
      result += name2FuncEntryAddr_whenUnique.get(fnName);
      return result;
   }
   else {
      if (!name2SeveralFuncEntryAddrs.defines(fnName)) {
         pdvector<kptr_t> nothing;
         return nothing;
      }
      else {
         const pdvector<kptr_t> &result = name2SeveralFuncEntryAddrs.get(fnName);
         return result;
      }
   }
}

kptr_t
loadedModule::find_1fn_by_name(const StaticString &fnName) const {
   pdvector<kptr_t> result = find_fn_by_name(fnName);
   if (result.size() == 0)
      return 0;
   else if (result.size() > 1)
      assert(false && "find_1fn_by_name(): duplicates are not allowed");
   else
      return result[0];
}
