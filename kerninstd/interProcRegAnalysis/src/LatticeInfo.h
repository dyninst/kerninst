// LatticeInfo.h

#ifndef _LATTICE_INFO_H_
#define _LATTICE_INFO_H_

#include "bitwise_dataflow_fn.h"
#include <algorithm>
#include "util/h/Dictionary.h"

struct oneFnWithKnownNumLevelsByAddr {
   kptr_t fnEntryAddr;
   int numLevels;
      // 0 not allowed.  Negative --> pre-save frames.  Positive --> no pre-save frames
};

class functionsWithKnownNumLevelsByAddrCmp {
 public:
   bool operator()(const oneFnWithKnownNumLevelsByAddr &first,
                   const oneFnWithKnownNumLevelsByAddr &second) const {
      return first.fnEntryAddr < second.fnEntryAddr;
   }

   bool operator()(kptr_t addr, const oneFnWithKnownNumLevelsByAddr &other) const {
      return addr < other.fnEntryAddr;
   }
   bool operator()(const oneFnWithKnownNumLevelsByAddr &other, kptr_t addr) const {
      return other.fnEntryAddr < addr;
   }
};

class LatticeInfo {   
 public:
   typedef monotone_bitwise_dataflow_fn dataflow_fn;
 private:
   dataflow_fn::mergeoperators merge_op;
   dataflow_fn *optimisticDataflowFn; // top of lattice
   dataflow_fn *pessimisticDataflowFn; // bottom of lattice

   pdvector<oneFnWithKnownNumLevelsByAddr> fnsWithKnownNumLevelsByAddr;

   // private to ensure it's not called:
   LatticeInfo &operator=(const LatticeInfo &src);
   LatticeInfo(const LatticeInfo &src) :
      merge_op(src.merge_op),
      pessimisticDataflowFnsByLevel(intIdentityHash) {
         
	 optimisticDataflowFn = dataflow_fn::getDataflowFn(*(src.optimisticDataflowFn));
	 pessimisticDataflowFn = dataflow_fn::getDataflowFn(*(src.pessimisticDataflowFn));
      }

   void sortFnsWithKnownNumLevelsByAddr() {
      std::sort(fnsWithKnownNumLevelsByAddr.begin(),
                fnsWithKnownNumLevelsByAddr.end(),
                functionsWithKnownNumLevelsByAddrCmp());
   }

   mutable dictionary_hash<int, dataflow_fn*> pessimisticDataflowFnsByLevel;
   
   void makePessimisticIfNeeded(int numLevels) const {
      assert(numLevels >= -99 && numLevels <= 99); // sanity check
      
      if (!pessimisticDataflowFnsByLevel.defines(numLevels)) {
         const int origNumLevels = numLevels;
         
         dataflow_fn *result = dataflow_fn::getDataflowFn(dataflow_fn::startall);
         while (numLevels < 0) {
            result->processSaveInBackwardsDFProblem();
            ++numLevels;
         }
         while (numLevels > 1) { // yes, 1 (since result started out w/1 level)
            result->processRestoreInBackwardsDFProblem();
            --numLevels;
         }

         const regset_t &thru = regset_t::getEmptySet(); // conservative: start all regs
         const regset_t &gen = regset_t::getFullSet(); // conservative: start all regs
         result->setEachLevelTo(&thru, &gen); // start all regs (all regs made live)

         if (origNumLevels < 0) {
            assert(result->getNumPreSaveFramesManufactured() ==
                   (unsigned)(-origNumLevels));
            assert(result->getNumLevels() == 1);
         }
         else if (origNumLevels == 0)
            assert(false);
         else {
            assert(result->getNumLevels() == (unsigned)origNumLevels);
            assert(result->getNumPreSaveFramesManufactured() == 0);
         }

         pessimisticDataflowFnsByLevel.set(origNumLevels, result);
      }
   }
   dataflow_fn *getPessimistic(int numLevels) const {
      return pessimisticDataflowFnsByLevel.get(numLevels);
   }
   
   static unsigned intIdentityHash(const int &i) {
      unsigned result = i;
      return result;
   }
   
 public:
   LatticeInfo(dataflow_fn::mergeoperators imerge_op,
                dataflow_fn *ioptimDataflowFn,
                dataflow_fn *ipessimDataflowFn) :
      merge_op(imerge_op),
      optimisticDataflowFn(ioptimDataflowFn),
      pessimisticDataflowFn(ipessimDataflowFn),
      pessimisticDataflowFnsByLevel(intIdentityHash) {
   }
   
  ~LatticeInfo() {
     delete optimisticDataflowFn;
     delete pessimisticDataflowFn;
  }

   void addFnsWithKnownNumLevelsByAddr(const pdvector<oneFnWithKnownNumLevelsByAddr> &v,
                                       bool resort_now) {
      fnsWithKnownNumLevelsByAddr += v;
      if (resort_now)
         sortFnsWithKnownNumLevelsByAddr();
   }

   dataflow_fn::mergeoperators getMergeOp() const {
      return merge_op;
   }
   dataflow_fn *getOptimisticDataflowFn() const {
      return optimisticDataflowFn;
   }
   dataflow_fn *getPessimisticDataflowFn(kptr_t destAddr) const {
      pdvector<oneFnWithKnownNumLevelsByAddr>::const_iterator iter =
         std::lower_bound(fnsWithKnownNumLevelsByAddr.begin(),
                          fnsWithKnownNumLevelsByAddr.end(),
                          destAddr,
                          functionsWithKnownNumLevelsByAddrCmp());
      if (iter == fnsWithKnownNumLevelsByAddr.end() || iter->fnEntryAddr != destAddr)
         return pessimisticDataflowFn;
      else {
         const int numLevels = iter->numLevels;

         makePessimisticIfNeeded(numLevels);
         return getPessimistic(numLevels);
         //return pessimisticDataflowFn;
      }
   }
   dataflow_fn *getIdentityDataflowFn() const {
      return dataflow_fn::getDataflowFn(dataflow_fn::passall);
   }
};

#endif
