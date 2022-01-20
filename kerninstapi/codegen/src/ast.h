/*
 * Copyright (c) 1996 Barton P. Miller
 * 
 * We provide the Paradyn Parallel Performance Tools (below
 * described as Paradyn") on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 * 
 * This license is for research uses.  For such uses, there is no
 * charge. We define "research use" to mean you may freely use it
 * inside your organization for whatever purposes you see fit. But you
 * may not re-distribute Paradyn or parts of Paradyn, in any form
 * source or binary (including derivatives), electronic or otherwise,
 * to any other organization or entity without our permission.
 * 
 * (for other uses, please contact us at paradyn@cs.wisc.edu)
 * 
 * All warranties, including without limitation, any warranty of
 * merchantability or fitness for a particular purpose, are hereby
 * excluded.
 * 
 * By your use of Paradyn, you understand and agree that we (or any
 * other person or entity with proprietary rights in Paradyn) are
 * under no obligation to provide either maintenance services,
 * update services, notices of latent defects, or correction of
 * defects for Paradyn.
 * 
 * Even if advised of the possibility of such damages, under no
 * circumstances shall we (or any other person or entity with
 * proprietary rights in the software licensed hereunder) be liable
 * to you or any third party for direct, indirect, or consequential
 * damages of any character regardless of type of action, including,
 * without limitation, loss of profits, loss of use, loss of good
 * will, or computer failure or malfunction.  You agree to indemnify
 * us (and any other person or entity with proprietary rights in the
 * software licensed hereunder) for any and all liability it may
 * incur to third parties resulting from your use of Paradyn.
 */

// $Id: ast.h,v 1.19 2004/10/25 23:09:51 igor Exp $

#ifndef AST_HDR
#define AST_HDR

//
// Define a AST class for use in generating primitive and pred calls
//

#include <stdio.h>
#include "common/h/Vector.h"
#include "util/h/Dictionary.h"
#include "common/h/String.h"
#include "common/h/Types.h"
#if defined(BPATCH_LIBRARY)
#include "dyninstAPI/h/BPatch_type.h"
#endif

// We put everything in a namespace to enable tools that use both
// Kerninst and Dyninst API
namespace kcodegen {

class process;
class instPoint;
class function_base;

#if defined(sparc_sun_solaris2_7) || defined(ppc64_unknown_linux2_4)
/*
  Register content type. We need it to be 64-bit to work on both 
  32 and 64-bit systems. This is important for kerninstAPI 
  clients, which are 32-bit while the kernel is 64-bit. As a result,
  4 == sizeof(long) != sizeof(kptr_t) == 8
*/
typedef int64_t RegContents;
typedef uint64_t HostAddress;

#elif defined(i386_unknown_linux2_4)

typedef int32_t RegContents;
typedef uint32_t HostAddress;

#endif

// a register number, e.g. [0,31]
// typedef int reg; // see new Register type in "common/h/Types.h"

typedef enum { plusOp,
               minusOp,
               timesOp,
               divOp,
               lessOp,
               leOp,
               greaterOp,
               geOp,
               eqOp,
               neOp,
               loadOp,           
               loadConstOp,
	       loadFrameRelativeOp,
	       loadAndSetAddrOp,
	       loadFrameAddr,
	       loadCPUId,
	       loadThreadId,
	       loadProcessId,
	       loadHwCtrReg,
               storeOp,
               storeAtomicOp,
	       storeFrameRelativeOp,
               ifOp,
	       whileOp,  // Simple control structures will be useful
	       doOp,     // Zhichen
	       callOp,
	       trampPreamble,
	       trampTrailer,
	       noOp,
	       orOp,
	       andOp,
	       shlOp,
	       shrOp,
	       movOp,
	       getRetValOp,
	       getSysRetValOp,
	       getParamOp,
	       getSysParamOp,	   
	       getAddrOp,	// return the address of the operand
	       loadIndirOp,
	       storeIndirOp,
	       saveRegOp,
	       updateCostOp,
	       funcJumpOp,        // Jump to function without linkage
	       branchOp,
	       retOp, // ret or retl opcode
#ifdef sparc_sun_solaris2_7
	       getPIL, // read the current interrupt level
	       setPIL  // set it
#elif defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4)
               disableLocalInterrupts, // mask local interrupts
               enableLocalInterrupts   // unmask local interrupts
#else
               dummy
#endif
} opCode;

class registerSlot {
 public:
    Register number;    // what register is it
    int refCount;      	// == 0 if free
    bool needsSaving;	// been used since last rest
    bool mustRestore;   // need to restore it before we are done.		
    bool startsLive;	// starts life as a live register.
};

class registerSpace {
    public:
	registerSpace(const unsigned int dCount, Register *deads,
                      const unsigned int lCount, Register *lives);
	Register allocateRegister(int iRefCount,
				  char *insn, HostAddress &base, bool noCost);
	// Free the specified register (decrement its refCount)
	void freeRegister(Register k);
	// Free the register even if its refCount is greater that 1
	void forceFreeRegister(Register k);
	void resetSpace();

	// Check to see if the register is free
	bool isFreeRegister(Register k);

	// Manually set the reference count of the specified register
	// we need to do so when reusing an already-allocated register
	void fixRefCount(Register k, int iRefCount);
	
	// Bump up the reference count. Occasionally, we underestimate it
	// and call this routine to correct this.
	void incRefCount(Register k);

	u_int getRegisterCount() { return numRegisters; }
	registerSlot *getRegSlot(Register k) { return (&registers[k]); }
	bool readOnlyRegister(Register k);
	u_int getMaxNumUsed() {
	    return maxNumUsed;
	}
	// Make sure that no registers remain allocated, except "to_exclude"
	// Used for assertion checking.
	void checkLeaks(Register to_exclude);
    private:
	u_int numRegisters;
	u_int maxNumUsed; // To find register requirements of the code
	Register highWaterRegister;
	registerSlot *registers;
};

class dataReqNode;
class AstNode {
    public:
        enum nodeType { sequenceNode, opCodeNode, operandNode, 
			callNode, gotoNode, labelNode };
        enum operandType { Constant, ConstantPtr, ConstantString,
#if defined(MT_THREAD)
			   OffsetConstant,      // add a OffsetConstant type for offset
			                        // generated for level or index:
			                        //   it is  MAX#THREADS * level * tSize  for level
			                        //     or                 index * tSize  for index
#endif
			   DataValue, DataPtr,  // restore AstNode::DataValue and AstNode::DataPtr
                           DataId, DataIndir, DataIndir64, DataReg,
			   Param, ReturnVal, DataAddr, FrameAddr,
			   SharedData, PreviousStackFrameDataReg,
			   EffectiveAddr, BytesAccessed,
			   CPUId, ThreadId, ProcessId,
			   HwCounterReg
	};
        AstNode(); // mdl.C
	AstNode(const pdstring &func, AstNode *l, AstNode *r);
        AstNode(const pdstring &func, AstNode *l); // needed by inst.C
	AstNode(operandType ot, RegContents arg);
#if defined(MT_THREAD)
	AstNode(operandType ot, RegContents arg, bool isLev, unsigned v_level, unsigned v_index);
#endif
	AstNode(AstNode *l, AstNode *r);

        AstNode(opCode, AstNode *left); 
        // assumes "NULL" for right child ptr
        // needed by inst.C and stuff in ast.C

        AstNode(operandType ot, AstNode *l);
	AstNode(opCode ot, AstNode *l, AstNode *r, AstNode *e = NULL);
        AstNode(opCode); // like AstNode(opCode, ...) above
                         // but assumes "NULL" for both child ptrs

        AstNode(const pdstring &func, pdvector<AstNode *> &ast_args);
        AstNode(function_base *func, pdvector<AstNode *> &ast_args);
	AstNode(function_base *func); // FuncJump (for replaceFunction)

        AstNode(AstNode *src);
	AstNode(nodeType type, const pdstring &label);
	
        AstNode &operator=(const AstNode &src);

       ~AstNode();

        HostAddress generateTramp(process *proc, const instPoint *location, 
				  char *i, HostAddress &base, int *trampCost, 
				  bool noCost);
	HostAddress generateCode(process *proc, registerSpace *rs, char *i, 
				 HostAddress &base, bool noCost, bool root,
				 const instPoint *location = NULL);
	HostAddress generateCode_phase2(process *proc, registerSpace *rs, 
					char *i, HostAddress &base, 
					bool noCost,
					const pdvector<AstNode*> &ifForks,
					const instPoint *location = NULL);

	enum CostStyleType { Min, Avg, Max };
	int AstNode::minCost() const {  return costHelper(Min);  }
	int AstNode::avgCost() const {  return costHelper(Avg);  }
	int AstNode::maxCost() const {  return costHelper(Max);  }

	// return the # of instruction times in the ast.
	int costHelper(enum CostStyleType costStyle) const;	
	void print() const;
        int referenceCount;     // Reference count for freeing memory
        int useCount;           // Reference count for generating code
        void checkRefCount();
        void setUseCount(registerSpace *rs); // Set values for useCount
        int getSize() { return size; };
        void cleanUseCount(void);
        bool checkUseCount(registerSpace*, bool&);
        void printUseCount(void);

	// Bump up the useCount. We use it to prevent freeing the kept
	// register when it is unsafe.
	void incUseCount(registerSpace *rs);

	// Occasionally, we do not call .generateCode_phase2 for the
	// referenced node, but generate code by hand. This routine decrements
	// its use count properly
	void decUseCount(registerSpace *rs);

        Register kept_register; // Use when generating code for shared nodes

	// Path from the root to this node which resulted in computing the
	// kept_register. It contains only the nodes where the control flow
	// forks (e.g., "then" or "else" clauses of an if statement)
	pdvector<AstNode*> kept_path;

	// Record the register to share as well as the path that lead
	// to its computation
	void keepRegister(Register r, pdvector<AstNode*> path);
	
	// Do not keep the register anymore
	void unkeepRegister();

	// Do not keep the register and force-free it
	void forceUnkeepAndFree(registerSpace *rs);

	// Our children may have incorrect useCounts (most likely they 
	// assume that we will not bother them again, which is wrong)
	void fixChildrenCounts(registerSpace *rs);

	// Check to see if the value had been computed earlier
	bool hasKeptRegister() const;

	// Check if the node can be kept at all. Some nodes (e.g., storeOp)
	// can not be cached
	bool canBeKept() const;

	// Allocate a register and make it available for sharing if our
        // node is shared
	Register allocateAndKeep(registerSpace *rs, 
				 const pdvector<AstNode*> &ifForks,
				 char *insn, HostAddress &base, bool noCost);

	// Check to see if path1 is a subpath of path2
	bool subpath(const pdvector<AstNode*> &path1, 
		     const pdvector<AstNode*> &path2) const;

	// If "goal" can be reached from this node, invalidate our
	// kept_register and return true. Otherwise, return false.
	bool invalidateAncestorsOf(AstNode *goal, registerSpace *rs);

	// Return all children of this node ([lre]operand, ..., operands[])
	void getChildren(pdvector<AstNode*> *children) const;

	// Return all live nodes in the subtree
	void getAllLive(pdvector<AstNode*> *live);

        void updateOperandsRC(bool flag); // Update operand's referenceCount
                                          // if "flag" is true, increments the
                                          // counter, otherwise it decrements 
                                          // the counter.
        void printRC(void);
	bool findFuncInAst(pdstring func) ;
	void replaceFuncInAst(function_base *func1, function_base *func2);
	void replaceFuncInAst(function_base *func1, function_base *func2, pdvector<AstNode *> &ast_args, int index=0);
	bool accessesParam(void);         // Does this AST access "Param"

	// Set the no-save-restore flag in our subtree
	void setNoSaveRestoreFlag(bool value);

        // Get the need-save-restore flag
        bool getNeedSaveRestoreFlag();

	bool containsCall() const;

	void optRetVal(AstNode *opt);

	void setOValue(RegContents arg) { oValue = arg; }
	// only function that's defined in metric.C (only used in metri.C)
	bool condMatch(AstNode* a,
		       pdvector<dataReqNode*> &data_tuple1,
		       pdvector<dataReqNode*> &data_tuple2,
		       pdvector<dataReqNode*> datareqs1,
		       pdvector<dataReqNode*> datareqs2);
	// Compute and return the number of registers used in this code
	unsigned computeRegNeedEstimate();
	unsigned getRegNeedEstimate() const {
	    return regNeedEstimate;
	}

#ifdef i386_unknown_linux2_4
        bool is64BitCode() const;
#endif
    private:
	nodeType type;
	opCode op;		    // only for opCode nodes
	pdstring callee;		    // only for call nodes
	function_base *calleefunc;  // only for call nodes
	HostAddress calleeAddr;     // only for call nodes
	pdvector<AstNode *> operands; // only for call nodes
	operandType oType;	    // for operand nodes
	RegContents oValue;	    // operand value for operand nodes
	pdstring labelName;           // only for label and goto nodes
#if defined(MT_THREAD)              // for OffsetConstant type for offset
	bool isLevel;               // true  if lvlOrIdx is level
	                            // false if lvlOrIdex is idex
	unsigned lvl;               // if level:  MAX#THREADS * level * tSize
	unsigned idx;               // if index:                index * tSize
	                            // lvl AND idx together identify a variable
#endif
#ifdef BPATCH_LIBRARY
	const BPatch_type *bptype;  // type of corresponding BPatch_snippet
	bool doTypeCheck;	    // should operands be type checked
#endif
	int size;		    // size of the operations (in bytes)

        // These 2 vrbles must be pointers; otherwise, we'd have a recursive
        // data structure with an infinite size.
        // The only other option is to go with references, which would have
        // to be initialized in the constructor and can't use NULL as a
        // sentinel value...
	AstNode *loperand;
	AstNode *roperand;
	AstNode *eoperand;
	
	// How many registers are needed to generate code for this node
	// The estimate may not be entirely accurate, but it does not
	// have to be (it'll just lead to sub-optimal register usage)
	unsigned regNeedEstimate; 
	                          
	// SPARC-specific: the no-save-restore flag, which is true iff our
        // expression has NOT been wrapped with save/restore.
	// Apparently, dyninst uses astFlag for this purpose, which is not
	// the best name, so we'll change it to something more sensible
	bool noSaveRestore;  

        // x86-specific: the need-save-restore flag, which is set during
        // code generation if we use instructions requiring specific
        // registers
        bool needSaveRestore;

    public:
	// Functions for getting and setting type decoration used by the
	// dyninst API library
#ifdef BPATCH_LIBRARY
	const BPatch_type *getType() { return bptype; };
	void		  setType(const BPatch_type *t) { 
				bptype = t; 
				size = t->getSize(); }
	void		  setTypeChecking(bool x) { doTypeCheck = x; }
	BPatch_type	  *checkType();
#endif
};

AstNode *assignAst(AstNode *src);
void removeAst(AstNode *&ast);
void terminateAst(AstNode *&ast);
AstNode *createIf(AstNode *expression, AstNode *action, process *proc=NULL);
#if defined(MT_THREAD)
AstNode *createCounter(const pdstring &func, void *, void *, AstNode *arg);
AstNode *createTimer(const pdstring &func, void *, void *, 
                     pdvector<AstNode *> &arg_args);
AstNode *computeAddress(void *level, int type);
AstNode *addIndexToAddress(AstNode *addr, void *index, int type);
AstNode *computeTheAddress(void *level, void *index, int type);
#else
AstNode *createCounter(const pdstring &func, void *, AstNode *arg);
AstNode *createTimer(const pdstring &func, void *, 
                     pdvector<AstNode *> &arg_args);
#endif
// VG(11/06/01): This should be in inst.h I suppose; moved there...
/*
Register emitFuncCall(opCode op, registerSpace *rs, char *i, HostAddress &base, 
		      const pdvector<AstNode *> &operands, const pdstring &func,
		      process *proc, bool noCost, const function_base *funcbase,
		      const instPoint *location = NULL);
*/
void emitLoadPreviousStackFrameRegister(HostAddress register_num,
					Register dest,
					char *insn,
					HostAddress &base,
					int size,
					bool noCost);
void emitFuncJump(opCode op, char *i, HostAddress &base,
		  const function_base *func, process *proc);
} // namespace
#endif /* AST_HDR */
