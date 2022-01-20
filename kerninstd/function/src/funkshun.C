// Yep, you read it right.  We went with this name not just to get down & groove, but
// because "function.h" is already taken by some egcs STL file.
// And although function.C wasn't taken by egcs, it seemed reasonable
// to stay consistent with .h/.C naming.
// The class, however, is still named function.

#include "funkshun.h"
#include "fn_misc.h" // findModuleAndReturnStringPoolReference()
#include "simplePath.h"
#include "controlFlow.h"
#include "util/h/minmax.h"
#include "util/h/hashFns.h"
#include <algorithm> // binary_search(), adjacent_find()
#include "moduleMgr.h" // ugh
#include "util/h/out_streams.h"
#include "util/h/xdr_send_recv.h" // xdr_recv_fail
#include "xdr_send_recv_common.h" // P_xdr_send() & _recv() for insnVec, e.g.
#include "parseOracle.h"

#include "xdr_send_recv_kerninstd_only.h"
#ifdef _KERNINSTD_
#include "cfgCreationOracle.h"
#include "kmem.h"
extern bool verbose_fnParse; 
extern bool verbose_hiddenFns; 
#endif

#ifdef sparc_sun_solaris2_7
#include "sparc_funkshun.h"
#elif defined(rs6000_ibm_aix5_1)
#include "power_funkshun.h"
#elif defined(i386_unknown_linux2_4)
#include "x86_funkshun.h"
#endif

template <class T>
void ari_destruct(T &t) {
   t.~T();
}

function_t * function_t::getFunction(XDR *xdr)
{
   function_t *newFn;
#ifdef sparc_sun_solaris2_7
   newFn = new sparc_function(xdr);
#elif defined(rs6000_ibm_aix5_1)
   newFn = new power_function(xdr);
#elif defined(i386_unknown_linux2_4)
   newFn = new x86_function(xdr);
#endif
   return newFn;
}

function_t * 
function_t::getFunction(kptr_t startAddr, const StaticString &fnName)
{
   function_t *newFn;
#ifdef sparc_sun_solaris2_7  
   newFn = new sparc_function(startAddr, fnName);
#elif defined(rs6000_ibm_aix5_1)
   newFn = new power_function(startAddr, fnName);
#elif defined(i386_unknown_linux2_4)
   newFn = new x86_function(startAddr, fnName);
#endif
   return newFn;
}

function_t * function_t::getFunction(kptr_t startAddr, 
				     const pdvector<StaticString> &fnNames)
{
   function_t *newFn;
#ifdef sparc_sun_solaris2_7
   newFn = new sparc_function(startAddr, fnNames);
#elif defined(rs6000_ibm_aix5_1)
   newFn = new power_function(startAddr, fnNames);
#elif defined(i386_unknown_linux2_4)
   newFn = new x86_function(startAddr, fnNames);
#endif
   return newFn;
}

// pure virtual destructor -> i.e. it can be called & must be defined
function_t::~function_t()
{
   delete [] alternate_names; // harmless if NULL, which is the common case.
   fry(false); // not verbose
}

function_t::function_t(XDR *xdr) :
   origCode(xdr),
   primary_name(StaticString::Dummy())
{
   if (!P_xdr_recv(xdr, entryAddr))
      throw xdr_recv_fail();

   if (!isUnparsed()) {
      assert(origCode.numChunks() > 0);
         // even unparsed fns have a dummy 1-byte chunk with their entry addr
      assert(origCode.enclosesAddr(entryAddr));
   }

   // primary name:
   bool primaryNameSeparateFromModuleStringPool;
   if (!P_xdr_recv(xdr, primaryNameSeparateFromModuleStringPool))
      throw xdr_recv_fail();

   if (primaryNameSeparateFromModuleStringPool) {
      pdstring theFnName;
      ari_destruct(theFnName);
      if (!P_xdr_recv(xdr, theFnName))
         throw xdr_recv_fail();

      pdstring theModName;
      ari_destruct(theModName);
      if (!P_xdr_recv(xdr, theModName))
         throw xdr_recv_fail();

      const char *primary_name_str =
         findModuleAndAddToRuntimeStringPool(theModName, theFnName);
      assert(0==strcmp(primary_name_str, theFnName.c_str()));
      primary_name.set(primary_name_str);
   }
   else {
      // We *must* currently be receiving a module
      const StringPool &theModuleStringPool = findModuleAndReturnStringPoolReference();
      const char *module_string_pool_base = theModuleStringPool.get_raw();
      const unsigned module_string_pool_nbytes = theModuleStringPool.getNumBytes();
      
      unsigned primary_name_offset;
      if (!P_xdr_recv(xdr, primary_name_offset))
         throw xdr_recv_fail();
      assert(primary_name_offset < module_string_pool_nbytes);
   
      const char *primary_name_str = module_string_pool_base + primary_name_offset;
      primary_name.set(primary_name_str);
   }
   
   // aliases:
   if (!P_xdr_recv(xdr, num_alternate_names))
      throw xdr_recv_fail();
   
   if (num_alternate_names == 0)
      alternate_names = NULL;
   else {
      alternate_names = new StaticString[num_alternate_names];
      for (unsigned lcv=0; lcv < num_alternate_names; ++lcv) {
         bool altNameSeparateFromModuleStringPool;
         if (!P_xdr_recv(xdr, altNameSeparateFromModuleStringPool))
            throw xdr_recv_fail();

         if (altNameSeparateFromModuleStringPool) {
            pdstring s;
            ari_destruct(s);
            if (!P_xdr_recv(xdr, s))
               throw xdr_recv_fail();

            pdstring theModName;
            ari_destruct(theModName);
            if (!P_xdr_recv(xdr, theModName))
               throw xdr_recv_fail();

            const char *alt_name_str =
               findModuleAndAddToRuntimeStringPool(theModName, s);
            alternate_names[lcv].set(alt_name_str);
         }
         else {
            // We *must* currently be receiving a module
            const StringPool &theModuleStringPool =
               findModuleAndReturnStringPoolReference();
            const char *module_string_pool_base = theModuleStringPool.get_raw();

            unsigned alt_name_offset;
            if (!P_xdr_recv(xdr, alt_name_offset))
               throw xdr_recv_fail();
            const char *alt_name_str = module_string_pool_base + alt_name_offset;
            alternate_names[lcv].set(alt_name_str);
         }
      }
   }

   // basic blocks:
   uint32_t nelems;
   if (!P_xdr_recv(xdr, nelems))
      throw xdr_recv_fail();

   // assert that the default ctor did exactly what we expected it to do:
   assert(basic_blocks.size() == 0);
   assert(basic_blocks.capacity() == 0);
   assert(basic_blocks.begin() == NULL);

   if (nelems != 0) {
      // don't call reserve_for_inplace_construction(zero)

      // No need to be too clever with reserve_for_inplace_construction() here;
      // since the vector is of pointers, element copying will be cheap, etc.
      pdvector<basicblock_t*>::iterator iter = 
	 basic_blocks.reserve_for_inplace_construction(nelems);
      pdvector<basicblock_t*>::iterator finish = basic_blocks.end();

      // OK for "do" instead of "while"; the check for nelems==0 above ensures 
      // that nelems > 0 here.
      do {
	 basicblock_t *newitem = basicblock_t::getBasicBlock(xdr);
	 assert(newitem);
      
	 *iter = newitem;
      } while (++iter != finish);
   }
   
   // similar for sorted_blocks...
   ari_destruct(sorted_blocks);
   if (!P_xdr_recv(xdr, sorted_blocks))
      throw xdr_recv_fail();

   // since we don't receive unsorted blocks, I guess everything must be sorted
   assert(sorted_blocks.size() == basic_blocks.size());

   bool couldParse;
   if (!P_xdr_recv(xdr, couldParse))
      throw xdr_recv_fail();
   
   if (couldParse) {
      assert(basic_blocks.size() > 0);
      regAnalysisDFSstate = completed;
   
      // And now for the live register analysis
      ari_destruct(liveRegisters);
      if (!P_xdr_recv_ctor(xdr, liveRegisters))
         throw xdr_recv_fail();

      ari_destruct(blocksHavingInterProcFallThru);
      if (!P_xdr_recv(xdr, blocksHavingInterProcFallThru))
         throw xdr_recv_fail();
   }
   else
      assert(basic_blocks.size() == 0);
}

bool function_t::send(XDR *xdr) const {
   if (!isUnparsed())
      // can't always assert that even unparsed fns have at least 1 chunk; what you're
      // thinking is that even unparsed fns have a dummy 1-byte entry in functionFinder,
      // not that it has a chunk
      assert(origCode.numChunks() > 0);

   if (!origCode.send(xdr))
      return false;

   if (!P_xdr_send(xdr, entryAddr))
      return false;

   // primary name:
   const char *name_str = primary_name.c_str();

   extern moduleMgr *global_moduleMgr;
   const moduleMgr &theModuleMgr = *global_moduleMgr;

   std::pair<pdstring, const function_t*> modAndFn = theModuleMgr.findModAndFn(entryAddr, true);
      // true --> startOnly
   const loadedModule &mod = *theModuleMgr.find(modAndFn.first);
   
   const StringPool &modStringPool = mod.getStringPool();
   const char *module_string_pool_start = modStringPool.get_raw();
   const unsigned module_string_pool_len = modStringPool.getNumBytes();
   
   if (name_str < module_string_pool_start ||
       name_str > module_string_pool_start + module_string_pool_len) {
      bool b = true;
      if (!P_xdr_send(xdr, b))
         return false;
      
      const pdstring s(name_str);
      if (!P_xdr_send(xdr, s))
         return false;

      const pdstring &theModName = mod.getName();
      if (!P_xdr_send(xdr, theModName))
         return false;
   }
   else {
      bool b = false;
      if (!P_xdr_send(xdr, b))
         return false;
      
      const unsigned offset = name_str - module_string_pool_start;
      if (!P_xdr_send(xdr, offset))
         return false;
   }
   
   // aliases:
   if (!P_xdr_send(xdr, num_alternate_names))
      return false;
   for (unsigned lcv=0; lcv < num_alternate_names; ++lcv) {
      const StaticString &name = getAliasName(lcv);
      name_str = name.c_str();

      if (name_str < module_string_pool_start ||
          name_str > module_string_pool_start + module_string_pool_len) {
         bool b = true;
         if (!P_xdr_send(xdr, b))
            return false;
         
         pdstring s(name_str);
         if (!P_xdr_send(xdr, name_str))
            return false;

         const pdstring &theModName = mod.getName();
         if (!P_xdr_send(xdr, theModName))
            return false;
      }
      else {
         bool b = false;
         if (!P_xdr_send(xdr, b))
            return false;
         
         const unsigned offset = name_str - module_string_pool_start;
         if (!P_xdr_send(xdr, offset))
            return false;
      }
   }

   // basic blocks
   if (!P_xdr_send_pointers(xdr, basic_blocks))
      return false;

   // sorted meta-data.
   assert(isSorted());
   if (!P_xdr_send(xdr, sorted_blocks))
      return false;

   // Bool: true iff parsed & analyzed OK (if could parse but couldn't do reg analysis
   // then isUnparsed() will still return false since reg analysis would just be
   // filled in with conservative values)
   const bool couldParse = !isUnparsed();
   if (!P_xdr_send(xdr, couldParse))
      return false;

   if (couldParse) {
      // MJB FIXME: assert(regAnalysisDFSstate == completed);
      // the recv routine pretty much asserts this
      // TO DO -- do better than this

      if (!P_xdr_send_method(xdr, liveRegisters))
         return false;

      if (!P_xdr_send(xdr, blocksHavingInterProcFallThru))
         return false;
   }
   
   return true;
}

function_t::function_t(kptr_t iEntryAddr, const StaticString &fnName) :
                      entryAddr(iEntryAddr),
                      origCode(fnCode_t::Empty()),
                      primary_name(fnName)
{
   alternate_names = NULL;
   num_alternate_names = 0;
   // origInsns is initialized to an empty vector.
   regAnalysisDFSstate = unvisited;
}


function_t::function_t(kptr_t iEntryAddr, 
		       const pdvector<StaticString> &fnNames) :
                       entryAddr(iEntryAddr),
                       origCode(fnCode_t::Empty()),
                       primary_name(fnNames[0])
{
   // origInsns is initialized to an empty vector.
   regAnalysisDFSstate = unvisited;
   num_alternate_names = fnNames.size()-1;
   alternate_names = new StaticString[num_alternate_names];
      // StaticString argument is just to avoid the non-existent default
      // ctor for StaticString
   assert(alternate_names);
   StaticString *destptr = alternate_names;
   const StaticString *srcptr = fnNames.begin() + 1;
   while (srcptr != fnNames.end())
      *destptr++ = *srcptr++;
}


void function_t::fnHasAlternateName(const StaticString &alternate_name) {
   StaticString *new_alternate_names = new StaticString[num_alternate_names+1];
   assert(new_alternate_names);

   for (unsigned lcv=0; lcv < num_alternate_names; lcv++)
      new_alternate_names[lcv] = alternate_names[lcv];
   new_alternate_names[num_alternate_names] = alternate_name;

   delete [] alternate_names; // harmless when NULL
   alternate_names = new_alternate_names;

   num_alternate_names++;
}


void function_t::fnHasSeveralAlternateNames(const pdvector<StaticString> &newlyAddedAltNames) {
   assert(newlyAddedAltNames.size() > 0);
   StaticString *new_alternate_names
	= new StaticString[num_alternate_names+newlyAddedAltNames.size()];
      // funny arg to ctor necessary, since no default ctor for StaticString
   assert(new_alternate_names);

   // copy the old names:
   StaticString *dest = new_alternate_names;
   const StaticString *src = alternate_names;
   unsigned num = num_alternate_names;
   while (num--)
      *dest++ = *src++;

   // copy the new name(s):
   src = newlyAddedAltNames.begin();
   num = newlyAddedAltNames.size();
   while (num--)
      *dest++ = *src++;

   delete [] alternate_names; // harmless when NULL
   alternate_names = new_alternate_names;

   num_alternate_names += newlyAddedAltNames.size();
}

bool function_t::matchesName(const pdstring &name) const {
   if (0==strcmp(getPrimaryName().c_str(), name.c_str()))
      return true;
   
   const StaticString *iter = alternate_names;
   const StaticString *finish = alternate_names + num_alternate_names;
   for (; iter != finish; ++iter) {
      if (0==strcmp(iter->c_str(), name.c_str()))
         return true;
   }
   
   return false;
}


unsigned function_t::totalNumBytes_fromBlocks() const {
   // Iterates thru each bb (--> slow!), counting the number of instruction
   // bytes.  Can give a different result from
   // getOrigCode().totalNumBytes_fromChunks(), which includes any embedded
   // data and dead code, if any.

   unsigned result = 0;
   
   const_iterator iter = begin();
   const_iterator finish = end();
   for (; iter != finish; ++iter) {
      result += (*iter)->getNumInsnBytes();
   }

   return result;
}

// ------------------------------------------------------------

pdvector<kptr_t> function_t::getAllBasicBlockStartAddressesSorted() const {
   assert_sorted();
   
   pdvector<kptr_t> result;
   result.reserve_exact(sorted_blocks.size());

   pdvector<bbid_t>::const_iterator iter = sorted_blocks.begin();
   pdvector<bbid_t>::const_iterator finish = sorted_blocks.end();
   for (; iter != finish; ++iter) {
      const bbid_t bb_id = *iter;
      result += basic_blocks[bb_id]->getStartAddr();
   }

   assert(result.size() == numBBs());
   return result;
}


pdvector< function_t::bbid_t >
function_t::getAllBasicBlockIdsSortedByAddress() const {
   assert_sorted();
   
   pdvector<bbid_t> result;
   result.reserve_exact(sorted_blocks.size());

   pdvector<bbid_t>::const_iterator iter = sorted_blocks.begin();
   pdvector<bbid_t>::const_iterator finish = sorted_blocks.end();
   for (; iter != finish; ++iter) {
      const bbid_t bb_id = *iter;
      result += bb_id;
         // this is the only difference from the above routine; we file a bb's
         // id instead of its start addr.
   }

   assert(result.size() == numBBs());
   return result;
}


void function_t::markBlockAsHavingInterProcFallThru(bbid_t bbid) {
   assert(getBasicBlockFromId(bbid)->hasAsASuccessor(basicblock_t::xExitBBId));

   assert(std::find(blocksHavingInterProcFallThru.begin(),
               blocksHavingInterProcFallThru.end(), bbid) ==
          blocksHavingInterProcFallThru.end());

   blocksHavingInterProcFallThru += bbid;
}


kptr_t function_t::getBBInterProcFallThruAddrIfAny(bbid_t bbid) const {
   pdvector<bbid_t>::const_iterator iter =
      std::find(blocksHavingInterProcFallThru.begin(),
                blocksHavingInterProcFallThru.end(), bbid);
   if (iter == blocksHavingInterProcFallThru.end())
      return (kptr_t)-1;
   else {
      const kptr_t result = getBasicBlockFromId(bbid)->getEndAddrPlus1();
      assert(!containsAddr(result));
      return result;
   }
}


// ------------------------------------------------------------

template <class T>
void destruct1(T &t) {
   t.~T();
}

void function_t::performDFS(pdvector<bbid_t> &preOrderNum2BlockId,
			    pdvector<bbid_t> &postOrderNum2BlockId,
			    pdvector<uint16_t> &blockId2PreOrderNum,
			    pdvector<uint16_t> &blockId2PostOrderNum) const {
   destruct1(preOrderNum2BlockId);
   destruct1(postOrderNum2BlockId);
   (void)new((void*)&preOrderNum2BlockId)pdvector<bbid_t>(numBBs(), (bbid_t)-1);
   (void)new((void*)&postOrderNum2BlockId)pdvector<bbid_t>(numBBs(), (bbid_t)-1);

   destruct1(blockId2PreOrderNum);
   destruct1(blockId2PostOrderNum);
   (void)new((void*)&blockId2PreOrderNum)pdvector<uint16_t>(numBBs(), (uint16_t)-1);
   (void)new((void*)&blockId2PostOrderNum)pdvector<uint16_t>(numBBs(), (uint16_t)-1);

   uint16_t currPreOrderNum = 0;
   uint16_t currPostOrderNum = 0;

   if (numBBs() > 0) { // 0 if we've skipped this fn, or had a problem parsing it
      bbid_t entry_bb_id = xgetEntryBB();
      
      performDFS(entry_bb_id,
                 currPreOrderNum, currPostOrderNum,
                 preOrderNum2BlockId, postOrderNum2BlockId,
                 blockId2PreOrderNum, blockId2PostOrderNum);
   }
}


void function_t::performDFS(bbid_t bb_id,
			    uint16_t &currPreOrderNum,
			    uint16_t &currPostOrderNum,
			    pdvector<bbid_t> &preOrderNum2BlockId,
			    pdvector<bbid_t> &postOrderNum2BlockId,
			    pdvector<uint16_t> &blockId2PreOrderNum,
			    pdvector<uint16_t> &blockId2PostOrderNum) const {
   // a private method; helper to the above fn

   assert(preOrderNum2BlockId[currPreOrderNum] == (bbid_t)-1);
   preOrderNum2BlockId[currPreOrderNum] = bb_id;
   assert(blockId2PreOrderNum[bb_id] == (uint16_t)-1);
   blockId2PreOrderNum[bb_id] = currPreOrderNum;

   currPreOrderNum++;

   // Now work on the successors
   // QUESTION: what to do about ExitBB and UnanalyzableBB?
   //           UnanalyzableBB is especially tricky, since it can appear
   //           in different places in the graph.

   // Some assertion checks to make sure the graph was constructed properly
   // would be nice right about now.

   // Assert that each successor of this node has this node in its predecessor list.
   // Yes, this includes recursive nodes.  Beware of UnanalyzableBB
   // and ExitBB, which can't be dereferenced.  NOTE: I'm getting worried
   // about ExitBB; it would be nice to have a "real" basic block there (with no
   // instructions), so we can keep track of its predecessors, among other things.
   // DO THIS!!!

   if (true) {
      const basicblock_t::SuccIter finish = basic_blocks[bb_id]->endSuccIter();
      for (basicblock_t::SuccIter succiter = basic_blocks[bb_id]->beginSuccIter();
	   succiter != finish;
           ++succiter) {
         bbid_t succ_id = *succiter;
         if (succ_id != basicblock_t::xExitBBId &&
             succ_id != basicblock_t::interProcBranchExitBBId &&
             succ_id != basicblock_t::UnanalyzableBBId)
            assert(basic_blocks[succ_id]->hasAsAPredecessor(bb_id));
      }
   }

   const basicblock_t::SuccIter finish = basic_blocks[bb_id]->endSuccIter();
   for (basicblock_t::SuccIter succiter = basic_blocks[bb_id]->beginSuccIter();
	succiter != finish;
        ++succiter) {
      bbid_t succ_id = *succiter;

      if (succ_id == basicblock_t::UnanalyzableBBId ||
          succ_id == basicblock_t::xExitBBId ||
          succ_id == basicblock_t::interProcBranchExitBBId)
         continue;

      //const basicblock_t &succ_bb = basic_blocks[succ_id];
      if (blockId2PreOrderNum[succ_id] == (uint16_t)-1) {
         // we haven't visited the_succ yet.
         // The edge (bb, the_succ) is a 'tree edge'.

	 //childrenInDFSTree += the_succ;
	 //the_succ->setParentInDFSTree(bb);

         performDFS(succ_id, currPreOrderNum, currPostOrderNum,
                    preOrderNum2BlockId, postOrderNum2BlockId,
                    blockId2PreOrderNum, blockId2PostOrderNum);
      }
      else if (blockId2PostOrderNum[succ_id] == (uint16_t)-1)
         // we have visited the_succ but it hasn't completed.  (bb, the_succ) is a back
         // edge.
         ;
      else
         // chord or cross edge
         ;
   }

   // do post-order stuff:
   assert(postOrderNum2BlockId[currPostOrderNum] == (bbid_t)-1);
   postOrderNum2BlockId[currPostOrderNum] = bb_id;
   assert(blockId2PostOrderNum[bb_id] == (uint16_t)-1);
   blockId2PostOrderNum[bb_id] = currPostOrderNum;
   
   currPostOrderNum++;
}

// ------------------------------------------------------------


void function_t::
ensureBlockStartInsnIsAlsoBlockOnlyInsn(kptr_t addr, instr_t *i) {
   bbid_t bb_id = searchForBB(addr);
   assert(bb_id != (bbid_t)-1);
   
   const basicblock_t *bb = getBasicBlockFromId(bb_id);
   assert(bb->getStartAddr() == addr);
   assert(bb->getNumInsnBytes() >= i->getNumBytes());
   if (bb->getNumInsnBytes() > i->getNumBytes()) {
      (void)splitBasicBlock(bb_id, addr + i->getNumBytes());
   }
}


void function_t::ensure_start_of_bb(kptr_t destAddr) {
   // make sure that this fn has a bb starting at 'destAddr'.  If it doesn't,
   // then split() the appropriate bb to make it so.

   bbid_t bb_id = searchForBB(destAddr);
   if (bb_id == (bbid_t)-1) {
      //cout << "WARNING: function_t::ensure_start_of_bb found fn but not bb" << endl;
      return;
   }
   
   if (basic_blocks[bb_id]->getStartAddr() == destAddr)
      return; // doesn't need to be split

   (void)splitBasicBlock(bb_id, destAddr);
}


void function_t::fry(bool verbose) {
   if (verbose)
      dout << "frying fn @" << addr2hex(entryAddr) << ", name " << primary_name << endl;
   
   // called by dtor, mainly.
   pdvector<basicblock_t *>::iterator iter = basic_blocks.begin();
   pdvector<basicblock_t *>::iterator finish = basic_blocks.end();
   for (; iter != finish; ++iter) {
      basicblock_t *bb = *iter;
      delete bb;
   }
			      
   basic_blocks.clear();
   sorted_blocks.clear();
   unsorted_blocks.clear();
   liveRegisters.clear();

   assert(numBBs() == 0);

   assert(isUnparsed());
   //assert(isUnanalyzed()); this routine no longer exists
}

function_t::bbid_t function_t::splitBasicBlock(bbid_t bb_id,
					       kptr_t splitAddr) {
   // bb_id identifies a basic block; splitAddr is an address w/in that bb.
   // Create a new basic block starting at splitAddr; bb_id falls through to
   // this new block.  bb's predecessors don't change.
   // Return the id of the new bb.

// cout << "Splitting bb #" << bb_id + 1 << " at addr " << addr2hex(splitAddr) << endl;

   const unsigned origNumInsnBytes = basic_blocks[bb_id]->getNumInsnBytes();
   
   assert(splitAddr > basic_blocks[bb_id]->getStartAddr());
      // don't split on the first instr
   assert(splitAddr <= basic_blocks[bb_id]->getEndAddr());

   basicblock_t *bb = basicblock_t::getBasicBlock(basicblock_t::Split(), bb_id,
				    basic_blocks[bb_id], splitAddr,
				    origCode); // split constructor
   bbid_t new_bb_id = addBlock_new(bb);

   // note: any basicblock& before this insn may now be dangling!  That's 
   // why we try to avoid basicblock references these days.

   // The new basic block is a copy of us: same insns (except starting a 
   // little later), same successors.
   // We now need to trim ourselves down...use only instructions upto but not
   // including splitAddr, and have just one successor: the new basic block.

   // Trim contents:
   // WARNING: since we're implicitly changing endAddr, we must also update any
   // spatial structures that map addresses to basic blocks.  Currently we 
   // have none.

   const kptr_t endAddr = splitAddr-1;
   bb = basic_blocks[bb_id];   
   unsigned bb_newNumBytes = (endAddr - bb->getStartAddr() + 1);
   bb->trimNumInsnBytesTo(bb_newNumBytes);

   // Update ptrs:
   bb->clearAllSuccessors();
   bb->addSuccessorNoDup(new_bb_id);

   basicblock_t *newbb = basic_blocks[new_bb_id];
   
   // newbb->addPredecessor(bb) should already have been done by the ctor

   // Note: this->predecessors don't change.

   assert((bb->getNumInsnBytes() + newbb->getNumInsnBytes()) 
	  == origNumInsnBytes);

   // Final step: our old successors (we've fried them, but we can still get 
   // access to them via the new bb's successors, which are our old 
   // successors) need to have "us" replaced by "new bb" for a predecessor
   for (basicblock_t::SuccIter succiter = newbb->beginSuccIter();
        succiter != newbb->endSuccIter(); ++succiter) {
      bbid_t succ_bb_id = *succiter;
      if (succ_bb_id == basicblock_t::UnanalyzableBBId ||
	  succ_bb_id == basicblock_t::xExitBBId ||
	  succ_bb_id == basicblock_t::interProcBranchExitBBId)
         continue;
      basicblock_t *succ_bb = basic_blocks[succ_bb_id];
      assert(succ_bb->hasAsAPredecessor(bb_id));
      succ_bb->replace1Predecessor(bb_id, new_bb_id);
   }

   return new_bb_id;
}

void function_t::bumpUpBlockSizeBy1Insn(bbid_t bb_id, 
				      const instr_t *instr) {
   // didn't want to pass in basicblock& since refs are so volatile
   basicblock_t *bb = basic_blocks[bb_id]; // not const; we bump up numInsns

   const kptr_t instrAddr = bb->getStartAddr() + bb->getNumInsnBytes();

   // NEW: Now that the function's contents are initialized en masse by the first
   // initial call to parse(), we can assert that "instr" has already been filled in
   // at this address!
   assert(get1OrigInsn(instrAddr)->getRaw() == instr->getRaw());

   bb->bumpUpNumInsnsBy1(instr->getNumBytes());
}


basicblock_t::bbid_t function_t::addBlock_new(basicblock_t *newbb) {
   const bbid_t newbb_id = basic_blocks.size();
   basic_blocks += newbb;
   check_no_dups();
   unsorted_blocks += newbb_id;
   assert(sorted_blocks.size() + unsorted_blocks.size() == basic_blocks.size());
   makeSortedIf10(); // TODO -- this is rather arbitrary!

   return newbb_id;
}


void function_t::
recreateJumpTableEdgesForFewerEntries(bbid_t bb_id,
                                      const pdvector<bbid_t> &inewSuccessors) {
   pdvector<bbid_t> newSuccessors(inewSuccessors);
   std::sort(newSuccessors.begin(), newSuccessors.end());
   // remove duplicates:
   pdvector<bbid_t>::iterator newend = std::unique(newSuccessors.begin(),
						   newSuccessors.end());
   newSuccessors.shrink(newend - newSuccessors.begin()); // ptr arith
   
   // We can assert that each of "newSuccessors" will presently be a successor.
   basicblock_t *bb = basic_blocks[bb_id];
   pdvector<bbid_t>::const_iterator iter = newSuccessors.begin();
   pdvector<bbid_t>::const_iterator finish = newSuccessors.end();
   for (; iter != finish; ++iter) {
      bbid_t succ_bb_id = *iter;
      
      assert(bb->hasAsASuccessor(succ_bb_id));
      
      assert(basic_blocks[succ_bb_id]->hasAsAPredecessor(bb_id));
   }

   // OK now it's time to shrink things down.
   // newSuccessors[] is sorted, so we can binary search it.  Loop thru all of the
   // present successors of "bb", and if
   // one isn't in newSuccessors[] then remove the successor/predecessor combo.
   pdvector<bbid_t> presentSuccessors;
   {
   basicblock_t::SuccIter siter = bb->beginSuccIter();
   basicblock_t::SuccIter sfinish = bb->endSuccIter();
   for (; siter != sfinish; ++siter)
      presentSuccessors += *siter;
   }

   // Determine what successors need to be removed:
   pdvector<bbid_t> successorsToRemove;
   iter = presentSuccessors.begin();
   finish = presentSuccessors.end();
   for (; iter != finish; ++iter) {
      const bbid_t succ_bb_id = *iter;
      if (!std::binary_search(newSuccessors.begin(), newSuccessors.end(), succ_bb_id)) {
         successorsToRemove += succ_bb_id;
      }
   }

   // Now time to do the removing:
   bb->clearAllSuccessors();
   iter = newSuccessors.begin();
   finish = newSuccessors.end();
   for (; iter != finish; ++iter)
      bb->addSuccessorNoDup(*iter);
   
   iter = successorsToRemove.begin();
   finish = successorsToRemove.end();
   for (; iter != finish; ++iter) {
      const bbid_t succ_bb_id = *iter;
      
      assert(!bb->hasAsASuccessor(succ_bb_id));
      
      basicblock_t *succbb = basic_blocks[succ_bb_id];
      succbb->erase1Predecessor(bb_id);
   }

   // And some final asserting:
   basicblock_t::SuccIter siter = bb->beginSuccIter();
   basicblock_t::SuccIter sfinish = bb->endSuccIter();
   for (; siter != sfinish; ++siter) {
      const bbid_t succ_bb_id= *siter;
      
      assert(basic_blocks[succ_bb_id]->hasAsAPredecessor(bb_id));
   }
}


void function_t::check_no_dups() {
   // sorry, but this routine's too expensive to do casually, so we only
   // do it if a certain flag is set at compile time:
#ifdef _DO_EVEN_EXPENSIVE_ASSERTS_
   pdvector<kptr_t> v;
   for (unsigned lcv=0; lcv < basic_blocks.size(); lcv++)
      v += basic_blocks[lcv]->getStartAddr();

   std::sort(v.begin(), v.end());
   
   for (unsigned lcv=1; lcv < v.size(); lcv++)
      assert(v[lcv] > v[lcv-1]);
#endif
}


void function_t::assertSorted() const {
   // assert sorted:
   assert(unsorted_blocks.size() == 0);
   assert(sorted_blocks.size() == basic_blocks.size());

#ifdef _DO_EVEN_EXPENSIVE_ASSERTS_
   if (sorted_blocks.size() > 0)
      for (pdvector<bbid_t>::const_iterator iter = sorted_blocks.begin();
           iter < sorted_blocks.end()-1; ++iter) {
         assert(basic_blocks[*iter]->getStartAddr() < basic_blocks[*(iter+1)]->getStartAddr());
      }
#endif
}


void function_t::sort_1() const { // yes, const (just plays with mutables)
   assert(unsorted_blocks.size() == 1);
   
   // A special case sort to make things go a little quicker.

   // Find where the unsorted block (there's just 1) should go
   const bbid_t new_bb_id = unsorted_blocks[0];
   const kptr_t new_addr = basic_blocks[new_bb_id]->getStartAddr();
   
   unsigned left=0;
   unsigned right = sorted_blocks.size() - 1 + 1;
   while (left != right) {
      const unsigned mid = (left + right) / 2;
      const bbid_t bb_id = sorted_blocks[mid];
      const basicblock_t *bb = basic_blocks[bb_id];
      
      const kptr_t bbStartAddr = bb->getStartAddr();

      if (new_addr < bbStartAddr) {
         // The new block shouldn't go after "mid"
         right = mid;
         continue;
      }
      else if (new_addr == bbStartAddr)
         assert(false); // duplicates frowned upon
      else {
         // The new block should go after "mid"
         left = mid + 1;
         assert(left <= right);
      }
   }

   assert(left == right);
   if (left != sorted_blocks.size())
      assert(basic_blocks[new_bb_id]->getEndAddr() <
             basic_blocks[sorted_blocks[left]]->getStartAddr());
   else
      assert(basic_blocks[new_bb_id]->getStartAddr() >
             basic_blocks[sorted_blocks.back()]->getEndAddr());

   // Insert!
   sorted_blocks.grow(sorted_blocks.size() + 1);

   assert(left <= sorted_blocks.size() - 1);
   if (left != sorted_blocks.size() - 1)
      memmove(sorted_blocks.begin() + left + 1, // dest
              sorted_blocks.begin() + left, // src
              sizeof(bbid_t) * (sorted_blocks.size() - 1 - left));
   sorted_blocks[left] = new_bb_id;

   unsorted_blocks.clear();

   assert(sorted_blocks.size() + unsorted_blocks.size() == basic_blocks.size());
}


void function_t::merge(pdvector<bbid_t> &newSortedSet,
		       const pdvector<bbid_t> &sortedSet,
		       const pdvector<bbid_t> &unsortedSet) const {
   // const method -- only plays with mutables
   unsigned sortedNdx=0;
   unsigned unsortedNdx=0;
   unsigned ndx=0;

   while (true) {
      if (sortedNdx >= sortedSet.size()) {
	 // done with sorted set.  Not done with unsorted set.
	 const unsigned numToCopy = unsortedSet.size() - unsortedNdx;
	 memcpy(&newSortedSet[ndx], &unsortedSet[unsortedNdx],
		sizeof(bbid_t) * numToCopy);
	 ndx += numToCopy;
	 break;
      }
       
      if (unsortedNdx >= unsortedSet.size()) {
	 // done with unsorted set.  Note done with sorted set.
	 const unsigned numToCopy = sortedSet.size() - sortedNdx;
	 memcpy(&newSortedSet[ndx], &sortedSet[sortedNdx],
		sizeof(bbid_t) * numToCopy);
	 ndx += numToCopy;
	 break;
      }

      if (ndx >= 2) {
	assert(newSortedSet[ndx-1] != newSortedSet[ndx-2]);
	assert(basic_blocks[newSortedSet[ndx-1]]->getStartAddr()
	       != basic_blocks[newSortedSet[ndx-2]]->getStartAddr());
	assert(basic_blocks[newSortedSet[ndx-1]]->getStartAddr()
	       > basic_blocks[newSortedSet[ndx-2]]->getStartAddr());
      }

      // both sets have at least one item left
      const basicblock_t *sortedSetBB   = basic_blocks[sortedSet[sortedNdx]];
      const basicblock_t *unsortedSetBB = basic_blocks[unsortedSet[unsortedNdx]];
      assert(sortedSetBB->getStartAddr() != unsortedSetBB->getStartAddr());
      if (sortedSetBB->getStartAddr() < unsortedSetBB->getStartAddr())
	 newSortedSet[ndx++] = sortedSet[sortedNdx++];
      else
	 newSortedSet[ndx++] = unsortedSet[unsortedNdx++];

      if (ndx >= 2) {
         assert(newSortedSet[ndx-1] != newSortedSet[ndx-2]);
         assert(basic_blocks[newSortedSet[ndx-1]]->getStartAddr()
		!= basic_blocks[newSortedSet[ndx-2]]->getStartAddr());
         assert(basic_blocks[newSortedSet[ndx-1]]->getStartAddr()
		> basic_blocks[newSortedSet[ndx-2]]->getStartAddr());
      }
   }

   assert(ndx == newSortedSet.size());
}

class bbcmp {
 private:
   const function_t &the_fn;
 public:
   bbcmp(const function_t &ifn) : the_fn(ifn) { }
   
   bool operator()(function_t::bbid_t id1,
                   function_t::bbid_t id2) const {
      const basicblock_t *bb1 = the_fn.getBasicBlockFromId(id1);
      const basicblock_t *bb2 = the_fn.getBasicBlockFromId(id2);
      return (bb1->getStartAddr() < bb2->getStartAddr());
   }
};


void function_t::mergeSort() const { // const: just plays with mutable members
   assert(unsorted_blocks.size() > 0);
   assert(sorted_blocks.size() + unsorted_blocks.size() == basic_blocks.size());

   if (sorted_blocks.size() == 0 && unsorted_blocks.size() == 1) {
      // A neat way to handle a special case
      sorted_blocks.swap(unsorted_blocks); // very fast (low constant time)
      return;
   }
   else if (unsorted_blocks.size() == 1) {
      sort_1();
      return;
   }

   // More than 1 unsorted block.  Sort them now.
   // NOTE: Performance check with gprof shows that this call to sort() is *not* a
   // bottleneck.
   std::sort(unsorted_blocks.begin(), unsorted_blocks.end(), bbcmp(*this));

   if (sorted_blocks.size() == 0) {
      // A neat way to handle a special case
      sorted_blocks.swap(unsorted_blocks); // very fast (low constant time)
      return;
   }
   
// I thought this sequence would be faster than inplace_merge(), since inplace_merge() allocates
// its own temp buffer.  But we're slightly slower, so we go with the inplace_merge() version.
//          pdvector<bbid_t> new_sorted_blocks;
//          new_sorted_blocks.grow(basic_blocks.size()); // allocate; could be expensive (but no copying)

//          ::merge(sorted_blocks.begin(), // start of src1
//                  sorted_blocks.end(),   // end of src1
//                  unsorted_blocks.begin(), // start of src2
//                  unsorted_blocks.end(),     // end of src2
//                  new_sorted_blocks.begin(), // start of destination
//                  bbcmp(*this));
        
//          sorted_blocks.swap(new_sorted_blocks); // hope this is fast (constant time)

   const unsigned num_sorted_blocks_before = sorted_blocks.size();
   sorted_blocks.grow(basic_blocks.size());

   // we use memcpy() instead of stl's copy() because, due to semantics of stl's copy()
   // (overlapping is permitted), it can only optimize the copy() to memmove(), not to memcpy().
   memcpy(sorted_blocks.begin() + num_sorted_blocks_before, // dest
          unsorted_blocks.begin(), // src
          unsorted_blocks.size() * sizeof(bbid_t));
//          copy(unsorted_blocks.begin(), // start of src
//               unsorted_blocks.end(), // end of src
//               sorted_blocks.begin() + num_sorted_blocks_before // dest
//               );

   // NOTE: gprof shows that this call to inplace_merge() is a bottleneck:
   // (Actually, a lot of that bottleneck is in bbcmp() itself, which we wrote)
   std::inplace_merge(sorted_blocks.begin(), // first
                      sorted_blocks.begin() + num_sorted_blocks_before, // middle
                      sorted_blocks.end(),
                      bbcmp(*this));

   unsorted_blocks.clear();

//   assert(unsorted_blocks.size() == 0);
//   assert(sorted_blocks.size() == basic_blocks.size());
   assertSorted();
}


void function_t::makeSorted() const { // const -- just plays with mutable members
     assert(sorted_blocks.size() + unsorted_blocks.size() == basic_blocks.size());
     
     if (unsorted_blocks.size() > 0)
	  mergeSort();
     
     assertSorted();
}


basicblock_t::bbid_t function_t::binary_search(kptr_t addr) const {
   // binary search.  Return (bbid_t)-1 on failure.
   assert(unsorted_blocks.size() == 0);

   if (sorted_blocks.size() == 0)
      return (bbid_t)-1;
   
   unsigned left = 0;
   unsigned right = sorted_blocks.size()-1;

   while (true) {
      unsigned ndx = (left + right) / 2;
      bbid_t bb_id = sorted_blocks[ndx];
      const basicblock_t *bb = basic_blocks[bb_id];
      
      const kptr_t bbStartAddr = bb->getStartAddr();
      const kptr_t bbEndAddr   = bb->getEndAddr();
      
      if (addr >= bbStartAddr && addr <= bbEndAddr)
         return bb_id;

      if (addr < bbStartAddr) {
	 // move left
	 right = ndx - 1;
         if (ndx == 0)
            // unsigned wraparound
            return (bbid_t)-1;
      }
      else {
	 assert(addr > bbEndAddr);
	 left = ndx + 1;
      }

      if (left > right)
         return (bbid_t)-1;
   }

   assert(false);
}


basicblock_t::bbid_t function_t::searchForFirstBlockStartAddrGThan(kptr_t addr) const {
   assert(sorted_blocks.size() + unsorted_blocks.size() == basic_blocks.size());
   makeSorted();
   assert(unsorted_blocks.size() == 0);
   assert(sorted_blocks.size() == basic_blocks.size());

   if (sorted_blocks.size() == 0)
      return (bbid_t)-1;
   
   unsigned left = 0;
   unsigned right = sorted_blocks.size()-1;
   while (true) {
      unsigned mid = (left + right) / 2;
      bbid_t bb_id = sorted_blocks[mid];
      const basicblock_t *bb = basic_blocks[bb_id];
      
      const kptr_t bbStartAddr = bb->getStartAddr();
      const kptr_t bbEndAddr   = bb->getEndAddr();

      if (addr >= bbStartAddr && addr <= bbEndAddr) {
         // "addr" is within this basic block.  But that's not what we're searching
         // for; we're searching for the first block that's all *higher* than
         // "addr".
         left = mid+1;
      }
      else if (addr < bbStartAddr)
         // midbb's startAddr is greater than addr, so it's a potential match.
         // But is it the first such block?  Fortunately we can still make some
         // progress: everything greater than midbb is certainly too high:
         right = mid;
      else {
         assert(addr > bbEndAddr);
         
         // midbb's startAddr is less than addr, so it (and everything below it)
         // doesn't satisfy the > relation.  So throw out left thru mid, inclusive.
         left = mid + 1;
      }
      
      if (left > right)
         return (bbid_t)-1; // not found
      else if (left == right) {
         // Last chance.  If relation is satisfied, we have a match.
         // Otherwise, give up.

         bbid_t bb_id = sorted_blocks[left];
         if (basic_blocks[bb_id]->getStartAddr() > addr)
            return bb_id;
         else
            return (bbid_t)-1;
      }
   }
}


void function_t::prepareLiveRegisters(const dataflowfn_t &optimistic_dataflowfn) {
   // resize 'liveRegisters' to numBBs; initialize all to stopAllRegs
   assert(liveRegisters.size() == 0);

   const unsigned num_blocks = numBBs();
   pdvector<liveRegInfoPerBB>::iterator iter = liveRegisters.reserve_for_inplace_construction(num_blocks);

   for (unsigned lcv=0; lcv < num_blocks; ++lcv) {
      // iter points to uninitialized memory, we can call a non-copy-ctor
      // via dummy placement operator new
      new((void*)iter++)liveRegInfoPerBB(false, optimistic_dataflowfn);
         // boolean argument to liveRegInfoPerBB ctor sets it to "uninitialized"
   }
}

// Entry and exit points:

pdvector<kptr_t> function_t::getCurrEntryPoints() const {
   // If we had a designated fixed EntryBasicBlock, then it would easy to find
   // the entry points: its successors.  But we don't have that.

   // For now, we use getEntryBB(), which simply returns the basic block containing
   // what is statically the first instruction.

   const bbid_t bb_id = xgetEntryBB();

   assert(basic_blocks[bb_id]->getStartAddr() == entryAddr);
   
   kptr_t result = basic_blocks[bb_id]->getStartAddr();

   // TO DO: we need to actually find multiple entry points.
   // TO DO: what if the first instruction has been transformed and thus now
   // resides in a patch somewhere?  I.e., checking origInsns[] is suspect.

   pdvector<kptr_t> v;
   v += result;
   return v;
}

//  
//  uint32_t function_t::getCurrPoint(unsigned insn_ndx,
//  					    bool /* before */) const {
//     // to do: account for insns that have moved, when 'before' param will be needed.
//     return startAddr + origInsns.insnNdx2ByteOffset(insn_ndx, false);
//  }

struct mycmp {
   bool operator()(const std::pair<kptr_t, kptr_t> &one,
                   const std::pair<kptr_t, kptr_t> &two) {
      // sort by insnAddr
      return (one.first < two.first);
   }
};


void function_t::getCallsMadeBy_WithSuppliedCodeAndBlocks
                    (const fnCode_t &fnCode,
                     const pdvector<bbid_t> &bbsOfInterest,
                     pdvector< std::pair<kptr_t, kptr_t> > &regularCalls,
                     bool interProcBranchesToo,
                     pdvector< std::pair<kptr_t, kptr_t> > &interProcBranches,
                     bool unAnalyzableCallsToo,
                     pdvector<kptr_t> &unAnalyzableCallsThruRegister) const {
   // Appends to "regularCalls[]".
   // if interProcBranchesToo==true, appends to interProcBranches[]
   // if unAnalyzableCallsToo==true, appends to unAnalyzableCallsThruRegister[]
   //    XXX but can only include jmpl [addr], %o7 in this group; optimized
   //    register-indirect tail calls will not be identified -- since I don't know
   //    if they're really a tail call or whether they're a jump table.
   //
   // We do NOT assert that these vectors are empty when passed in.
   
   // Note that by passing in "theCode", you can use getOrigInsns() or you can
   // be more clever and take into effect replaceFunction_Ts that have already been
   // done. XXX is this an obsolete statement to make?

   // We can be quicker in calculating interProcBranches by checking for exit
   // edges out of a given basic block.  However, as usual, things would get a little
   // hairy when a basic block it split on the delay slot, so for now I look the other
   // way on this optimization.

   if (bbsOfInterest.size() == 0)
      return;
   
#ifndef NDEBUG
   const unsigned regularCallsOrigSize = regularCalls.size();
   const unsigned interProcBranchesOrigSize = interProcBranches.size();
#endif

   // Loop thru the basic blocks.  The "do" loop is OK since we checked for
   // bbsOfInterest.size()==0 earlier.  It allows easier management of "chunk_iter".
   fnCode_t::const_iterator chunk_iter = fnCode.findChunk(entryAddr, false);
   assert(chunk_iter != fnCode.end());

   pdvector<bbid_t>::const_iterator iter = bbsOfInterest.begin();
   pdvector<bbid_t>::const_iterator finish = bbsOfInterest.end();
   assert(iter != finish && "empty bbsOfInterest");
      // shouldn't occur; we already checked for size==0 above
   
   do {
      const bbid_t bbid = *iter;
      const basicblock_t *bb = getBasicBlockFromId(bbid);
      
      const kptr_t bbStartAddr = bb->getStartAddr();
      const kptr_t bbFinishAddr = bb->getEndAddrPlus1(); // STL style

      // Work with a different chunk if needed:
      if (!chunk_iter->enclosesAddr(bbStartAddr)) {
         chunk_iter = fnCode.findChunk(bbStartAddr, false);
         assert(chunk_iter != fnCode.end());
      }

      const fnCode_t::codeChunk &theCodeChunk = *chunk_iter;
      
      for (kptr_t insnAddr=bbStartAddr; insnAddr < bbFinishAddr; ) {
         const instr_t *i = theCodeChunk.get1Insn(insnAddr);

         kptr_t calleeAddr = 0;
         if (i->isCallToFixedAddress(insnAddr, calleeAddr) && // a true call insn...
             !fnCode.enclosesAddr(calleeAddr)) { // ...and interprocedural
            assert(calleeAddr != 0);
            regularCalls += std::make_pair(insnAddr, calleeAddr);
         }
         else if (interProcBranchesToo &&
                  i->isBranchToFixedAddress(insnAddr, calleeAddr) && // a branch insn...
                  !fnCode.enclosesAddr(calleeAddr)) { // ...and interprocedural
            assert(calleeAddr != 0);
            interProcBranches += std::make_pair(insnAddr, calleeAddr);
         }
         else if (unAnalyzableCallsToo && i->isUnanalyzableCall()) {
            // A call thru a register...jmpl [register-indirect address], %o7
            // XXX But does not include jmp [register-indirect address], because
            // I don't know if it's really a call (tail call), or if it's a 
            // jump table.
            unAnalyzableCallsThruRegister += insnAddr;
         }

         insnAddr += i->getNumBytes();
      }

      ++iter;
   } while (iter != finish);

   // assert no duplicates
#ifndef NDEBUG
   assert(regularCalls.size() >= regularCallsOrigSize);
   std::sort(regularCalls.begin() + regularCallsOrigSize, regularCalls.end(), mycmp());
   assert(regularCalls.end() ==
          std::adjacent_find(regularCalls.begin() + regularCallsOrigSize,
                             regularCalls.end()));
   
   if (interProcBranchesToo) {
      assert(interProcBranches.size() >= interProcBranchesOrigSize);
      std::sort(interProcBranches.begin() + interProcBranchesOrigSize,
           interProcBranches.end(), mycmp());
      assert(interProcBranches.end() ==
             std::adjacent_find(interProcBranches.begin() + 
				interProcBranchesOrigSize,
				interProcBranches.end()));
   }
#endif
}

// ----------------------------------------------------------------------
// getCallsMadeTo - type routines: not to be confused with getCallsMadeFrom routines
// ----------------------------------------------------------------------

pdvector<kptr_t> // .first: insnAddr   .second: callee
function_t::getCallsMadeTo_withSuppliedCode(kptr_t destAddr, 
					    const fnCode_t &fnCode,
					    bool interProcBranchesToo) const {
   // Note that by passing in "theInsns", you can use getOrigInsns() or you can
   // be more clever and take into effect replaceFunctions that have already 
   // been done. XXX is this obsolete?
   
   pdvector<kptr_t> result;

   // loop thru actual code (basic blocks) instead of blindly iterating thru
   // origInsns[], which could lead to parsing data and thinking we're making a call!
   
   const_iterator bbiter = begin();
   const_iterator bbfinish = end();
   for (; bbiter != bbfinish; ++bbiter) {
      const kptr_t bbStartAddr = (*bbiter)->getStartAddr();
      const kptr_t bbFinishAddr = (*bbiter)->getEndAddrPlus1(); // STL style

      fnCode_t::const_iterator chunk_iter = fnCode.findChunk(bbStartAddr, false);
      assert(chunk_iter != fnCode.end());
      const fnCode_t::codeChunk &theCodeChunk = *chunk_iter;
      
      for (kptr_t insnAddr=bbStartAddr; insnAddr < bbFinishAddr; ) {
         const instr_t *i = theCodeChunk.get1Insn(insnAddr);

         kptr_t calleeAddr = 0;
         if (i->isCallToFixedAddress(insnAddr, calleeAddr) &&
             !fnCode.enclosesAddr(calleeAddr)) { // make sure interprocedural
            assert(calleeAddr != 0);

            if (calleeAddr == destAddr)
               result += insnAddr;
         }
         else if (interProcBranchesToo &&
                  i->isBranchToFixedAddress(insnAddr, calleeAddr) &&
                  !fnCode.enclosesAddr(calleeAddr)) {
            assert(calleeAddr != 0);
            
            if (calleeAddr == destAddr)
               result += insnAddr;
         }

         insnAddr += i->getNumBytes();
      }
   }

   // assert no duplicates
   std::sort(result.begin(), result.end());
   assert(result.end() == std::adjacent_find(result.begin(), result.end()));
   
   return result;
}

void function_t::processNonControlFlowCode(bbid_t bb_id,
					   kptr_t codeStartAddr,
					   unsigned &codeNumInsns,
					   const parseOracle *theParseOracle) const {
   if (codeNumInsns == 0)
      // a little optimization:
      return;
   
   theParseOracle->processSeveralNonControlFlowInsnsAsInsnVecSubset(bb_id,
								 codeStartAddr,
								 codeNumInsns);
   
   codeNumInsns = 0;
}
