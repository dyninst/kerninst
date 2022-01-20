// regAnalysisOracles.h

#ifndef _REG_ANALYSIS_ORACLES_H_
#define _REG_ANALYSIS_ORACLES_H_

#include "bitwise_dataflow_fn.h"
#include "LatticeInfo.h"
#include "funkshun.h"
#include "bb.h"

class moduleMgr; // avoids recursive #include's

class RegAnalysisHandleCall_FirstTime {
 private:
   typedef monotone_bitwise_dataflow_fn dataflow_fn;
   typedef basicblock_t::bbid_t bbid_t;
   typedef LatticeInfo lattice_t;
   
   moduleMgr &theModuleMgr;
      // can't be const since we may invoke doGlobalRegisterAnalysis1Fn()

   const lattice_t &theLatticeInfo;
      // merge op and top/bottom (optimistic/pessimistic) dataflow functions

   bool verbose;
   
   RegAnalysisHandleCall_FirstTime &operator=(const RegAnalysisHandleCall_FirstTime &);
   RegAnalysisHandleCall_FirstTime(const RegAnalysisHandleCall_FirstTime&);
   
 public:
   class LatticeIsAReference {};
   
   RegAnalysisHandleCall_FirstTime(moduleMgr &iModuleMgr,
                                   LatticeIsAReference, // dummy param that forces
                                   // the caller to think about something: that
                                   // the next argument will not be copied; only
                                   // a reference will be stored.  So be careful
                                   // about dangling references...refs to stack
                                   // variables, and so forth
                                   const lattice_t &iLatticeInfo,
                                   bool iverbose) :
      theModuleMgr(iModuleMgr),
      theLatticeInfo(iLatticeInfo),
      verbose(iverbose) {
   }

   const lattice_t &getLatticeInfo() const {
      return theLatticeInfo;
   }
   
   dataflow_fn::mergeoperators getMergeOp() const {
      return theLatticeInfo.getMergeOp();
   }
   
   const dataflow_fn* getOptimisticDataflowFn() const {
      return theLatticeInfo.getOptimisticDataflowFn();
   }
   
   const dataflow_fn* getPessimisticDataflowFn(kptr_t destAddr) const {
      return theLatticeInfo.getPessimisticDataflowFn(destAddr);
   }
   
   const dataflow_fn* getIdentityDataflowFn() const {
      return theLatticeInfo.getIdentityDataflowFn();
   }
   
   // Regrettably, whenDestAddrIsKnown() cannot return a const reference,
   // because in one rare case, whenDestAddrIsKnown_force() has to do some
   // calculations on the result before returning it.
   const dataflow_fn* whenDestAddrIsKnown(kptr_t destaddr) const {
      extern bool interprocedural_register_analysis;
      
      if (!interprocedural_register_analysis)
         return theLatticeInfo.getPessimisticDataflowFn(destaddr);
      else
         return whenDestAddrIsKnown_force(destaddr);
   }
   
   // Too bad this method can't return a const reference
   const dataflow_fn* whenDestAddrIsKnown_force(kptr_t destaddr) const;
   
   const dataflow_fn* whenDestAddrIsUnknown() const {
      // I guess this is the best we can do.  To bad too, since if we know even a
      // little (e.g. the dest is bracketed by save/restore), it can help a lot.
      return getPessimisticDataflowFn(0);
   }
};

class RegAnalysisHandleCall_AlreadyKnown {
 private:
   typedef monotone_bitwise_dataflow_fn dataflow_fn;
   typedef basicblock_t::bbid_t bbid_t;
   typedef LatticeInfo lattice_t;

   const moduleMgr &theModuleMgr;
   const lattice_t &theLatticeInfo;
      // merge op and top/bottom (optimistic/pessimistic) dataflow functions
   bool verbose;

   RegAnalysisHandleCall_AlreadyKnown &operator=(const RegAnalysisHandleCall_AlreadyKnown &);
   RegAnalysisHandleCall_AlreadyKnown(const RegAnalysisHandleCall_AlreadyKnown&);
   
 public:
   class LatticeIsAReference {};

   RegAnalysisHandleCall_AlreadyKnown(const moduleMgr &iModuleMgr,
                                      LatticeIsAReference, // dummy param that forces
                                      // the caller to think about something: that
                                      // the next argument will not be copied; only
                                      // a reference will be stored.  So be careful
                                      // about dangling references...refs to stack
                                      // variables, and so forth
                                      const lattice_t &iLatticeInfo,
                                      bool iverbose) :
      theModuleMgr(iModuleMgr), theLatticeInfo(iLatticeInfo), verbose(iverbose) {
   }

   const lattice_t &getLatticeInfo() const {
      return theLatticeInfo;
   }
   
   dataflow_fn::mergeoperators getMergeOp() const {
      return theLatticeInfo.getMergeOp();
   }
   
   const dataflow_fn* getOptimisticDataflowFn() const {
      return theLatticeInfo.getOptimisticDataflowFn();
   }
   
   const dataflow_fn* getPessimisticDataflowFn(kptr_t destAddr) const {
      return theLatticeInfo.getPessimisticDataflowFn(destAddr);
   }
   
   const dataflow_fn* getIdentityDataflowFn() const {
      return theLatticeInfo.getIdentityDataflowFn();
   }

   // This method can return a const reference, but because
   // the RegAnalysisHandleCall_FirstTime variant, above, can't, it may not be helpful.
   const dataflow_fn* whenDestAddrIsKnown(kptr_t destaddr) const;
   
   const dataflow_fn* whenDestAddrIsUnknown() const {
      // I guess this is the best we can do.  Too bad, since if we know even a little
      // about the destination function -- like whether it's bracketed by
      // save/restore -- we could do much better.

      return theLatticeInfo.getPessimisticDataflowFn(0);
   }
};

class RegAnalysisHandleCall_AlreadyKnown_BailOnRecursion {
 private:
   typedef monotone_bitwise_dataflow_fn dataflow_fn;
   typedef basicblock_t::bbid_t bbid_t;
   typedef LatticeInfo lattice_t;

   const moduleMgr &theModuleMgr;
   kptr_t fnStartAddr;
   const lattice_t &theLatticeInfo;
      // merge op and top/bottom (optimistic/pessimistic) dataflow functions
   bool verbose;

   RegAnalysisHandleCall_AlreadyKnown_BailOnRecursion &operator=(const RegAnalysisHandleCall_AlreadyKnown_BailOnRecursion &);
   RegAnalysisHandleCall_AlreadyKnown_BailOnRecursion(const RegAnalysisHandleCall_AlreadyKnown_BailOnRecursion&);
   
 public:
   class LatticeIsAReference {};
   RegAnalysisHandleCall_AlreadyKnown_BailOnRecursion(const moduleMgr &iModuleMgr,
                                                      kptr_t ifnStartAddr,
                                                      LatticeIsAReference, // reminder
                                                         // param
                                                      const lattice_t &iLatticeInfo,
                                                      bool iverbose) :
      theModuleMgr(iModuleMgr), fnStartAddr(ifnStartAddr),
      theLatticeInfo(iLatticeInfo),
      verbose(iverbose) {
   }

   const lattice_t &getLatticeInfo() const {
      return theLatticeInfo;
   }
   
   dataflow_fn::mergeoperators getMergeOp() const {
      return theLatticeInfo.getMergeOp();
   }
   
   const dataflow_fn* getOptimisticDataflowFn() const {
      return theLatticeInfo.getOptimisticDataflowFn();
   }
   
   const dataflow_fn* getPessimisticDataflowFn(kptr_t destAddr) const {
      return theLatticeInfo.getPessimisticDataflowFn(destAddr);
   }
   
   const dataflow_fn* getIdentityDataflowFn() const {
      return theLatticeInfo.getIdentityDataflowFn();
   }

   // Although this variant can return a const reference, it may not do much
   // good, since the RegAnalysisHandleCall_FirstTime, above, cannot.
   const dataflow_fn* whenDestAddrIsKnown(kptr_t destaddr) const;
   
   const dataflow_fn* whenDestAddrIsUnknown() const {
      // I guess this is the best we can do.  Too bad, since if we know even a little
      // about the destination function -- like whether it's bracketed by
      // save/restore -- we could do much better.

      return theLatticeInfo.getPessimisticDataflowFn(0);
   }
};

#endif
