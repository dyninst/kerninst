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

// $Id: ast.C,v 1.24 2005/04/11 21:17:29 igor Exp $

#include <string.h> // strdup()
#include <strstream>

#include "util/h/out_streams.h"
#include "ast.h"
#include "kludges.h"
#include "emit.h"

// We put everything in a namespace to enable tools that use both
// Kerninst and Dyninst API
namespace kcodegen {

extern registerSpace *regSpace;
extern bool doNotOverflow(int value);

static char errorLine[255];

registerSpace::registerSpace(const unsigned int deadCount, Register *dead, 
                             const unsigned int liveCount, Register *live)
{
//#if defined(i386_unknown_solaris2_5) || defined(i386_unknown_linux2_0)
//  initTramps();
//#endif

    unsigned i;
    numRegisters = deadCount + liveCount;
    registers = new registerSlot[numRegisters];

    maxNumUsed = 0;

    // load dead ones
    for (i=0; i < deadCount; i++) {
	registers[i].number = dead[i];
	registers[i].refCount = 0;
	registers[i].mustRestore = false;
	registers[i].needsSaving = false;
	registers[i].startsLive = false;
    }

    // load live ones;
    for (i=0; i < liveCount; i++) {
	registers[i+deadCount].number = live[i];
	registers[i+deadCount].refCount = 0;
	registers[i+deadCount].mustRestore = false;
	registers[i+deadCount].needsSaving = true;
	registers[i+deadCount].startsLive = true;
#if defined(SHM_SAMPLING) && defined(MT_THREAD)
        if (registers[i+deadCount].number == REG_MT) {
          registers[i+deadCount].refCount = 1;
          registers[i+deadCount].needsSaving = true;
        }
#endif
    }

}

Register registerSpace::allocateRegister(int iRefCount, char *insn,
					 HostAddress &base, bool noCost) 
{
    unsigned i, numInUse = 0;
    
    assert(iRefCount >= 0);

    // Count registers in use. We could use a counter and increment/decrement
    // it on allocate/free, but a double free will make it unreliable
    for (i=0; i < numRegisters; i++) {
	if (registers[i].refCount > 0) {
	    numInUse++;
	}
    }
    if (maxNumUsed < numInUse + 1) { // +1 accounts for the current reg
	maxNumUsed = numInUse + 1;
    }

    for (i=0; i < numRegisters; i++) {
	if (registers[i].refCount == 0 && !registers[i].needsSaving) {
	    registers[i].refCount = iRefCount;
	    highWaterRegister = (highWaterRegister > i) ? 
		 highWaterRegister : i;
            return(registers[i].number);
	}
    }

    // now consider ones that need saving
    for (i=0; i < numRegisters; i++) {
	if (registers[i].refCount == 0) {
#if !defined(rs6000_ibm_aix4_1)
            // MT_AIX: we are not saving registers on demand on the power
            // architecture. Instead, we save/restore registers in the base
            // trampoline - naim
 	    emitV(saveRegOp, registers[i].number, 0, 0, insn, base, noCost);
#endif
	    registers[i].refCount = 1;
#if !defined(rs6000_ibm_aix4_1)
            // MT_AIX
	    registers[i].mustRestore = true;
#endif
	    // prevent general spill (func call) from saving this register.
	    registers[i].needsSaving = false;
	    highWaterRegister = (highWaterRegister > i) ? 
		 highWaterRegister : i;
            return(registers[i].number);
	}
    }

    logLine("==> WARNING! run out of registers...\n");
    abort();
    return(Null_Register);
}

// Free the specified register (decrement its refCount)
void registerSpace::freeRegister(Register reg) 
{
    for (u_int i=0; i < numRegisters; i++) {
	if (registers[i].number == reg) {
	    registers[i].refCount--;
	    assert(registers[i].refCount >= 0);
	    return;
	}
    }
    // We allow to free the null register, but not any garbage
    assert(reg == Null_Register && "Attempt to free a non-existent register");
}

// Free the register even if its refCount is greater that 1
void registerSpace::forceFreeRegister(Register reg) 
{
    for (u_int i=0; i < numRegisters; i++) {
	if (registers[i].number == reg) {
	    registers[i].refCount = 0;
	    return;
	}
    }
    assert(false && "Attempt to free a non-existent register");
}

// Check to see if the register is free
bool registerSpace::isFreeRegister(Register reg) 
{
    for (u_int i=0; i < numRegisters; i++) {
	if ((registers[i].number == reg) &&
	    (registers[i].refCount == 0)) {
	    return true;
	}
    }
    return false;
}

// Manually set the reference count of the specified register
// we need to do so when reusing an already-allocated register
void registerSpace::fixRefCount(Register reg, int iRefCount)
{
    for (u_int i=0; i < numRegisters; i++) {
	if (registers[i].number == reg) {
	    registers[i].refCount = iRefCount;
	    return;
	}
    }
    assert(false && "Can't find register");
}

// Bump up the reference count. Occasionally, we underestimate it
// and call this routine to correct this.
void registerSpace::incRefCount(Register reg)
{
    for (u_int i=0; i < numRegisters; i++) {
	if (registers[i].number == reg) {
	    registers[i].refCount++;
	    return;
	}
    }
    assert(false && "Can't find register");
}

void registerSpace::resetSpace() {
    for (u_int i=0; i < numRegisters; i++) {
        if (registers[i].refCount > 0 && (registers[i].number != REG_MT)) {
          //sprintf(errorLine,"WARNING: register %d is still in use\n",registers[i].number);
          //logLine(errorLine);
        }
	registers[i].refCount = 0;
	registers[i].mustRestore = false;
	registers[i].needsSaving = registers[i].startsLive;
#if defined(SHM_SAMPLING) && defined(MT_THREAD)
        if (registers[i].number == REG_MT) {
          registers[i].refCount = 1;
          registers[i].needsSaving = true;
        }
#endif
    }
    highWaterRegister = 0;
    maxNumUsed = 0;
}

// Make sure that no registers remain allocated, except "to_exclude"
// Used for assertion checking.
void registerSpace::checkLeaks(Register to_exclude) 
{
    for (u_int i=0; i<numRegisters; i++) {
	assert(registers[i].refCount == 0 || 
	       registers[i].number == to_exclude);
    }
}

//
// How to use AstNodes:
//
// In order to avoid memory leaks, it is important to define and delete
// AstNodes properly. The general rules are the following:
//
// 1.- Any AstNode defined locally, should be destroyed at the end of that
//     procedure. The only exception occurs when we are returning a pointer
//     to the AstNode as a result of the function (i.e. we need to keep the
//     value alive).
// 2.- Every time we assign an AstNode to another, we have to use the
//     "assignAst" function. This function will update the reference count
//     of the AstNode being assigned and it will return a pointer to it. If
//     we are creating a new AstNode (e.g. AstNode *t1 = new AstNode(...))
//     then it is not necessary to use assign, because the constructor will
//     automatically increment the reference count for us.
// 3.- "removeAst" is the procedure to be used everytime we want to delete
//     an AstNode. In general, if an AstNode is re-used several times, it
//     should be enough to delete the root of the DAG to delete all nodes.
//     However, there are exceptions like this one:
//     AstNode *t1, *t2, *t3;
//     t1 = AstNode(...);   rc-t1=1
//     t2 = AstNode(...);   rc-t2=1
//     t3 = AstNode(t1,t2); rc-t1=2, rc-t2=2, rc-t3=1
//     if we say:
//     removeAst(t3);
//     it will delete t3, but not t1 or t2 (because the rc will be 1 for both
//     of them). Therefore, we need to add the following:
//     removeAst(t1); removeAst(t2);
//     We only delete AstNodes when the reference count is 0.
//

AstNode &AstNode::operator=(const AstNode &src) {
   logLine("Calling AstNode COPY constructor\n");
   if (&src == this)
      return *this; // the usual check for x=x

   // clean up self before overwriting self; i.e., release memory
   // currently in use so it doesn't become leaked.
   if (loperand) {
      if (src.loperand) {
        if (loperand!=src.loperand) {
          removeAst(loperand);
        }
      } else {
        removeAst(loperand);
      }
   }
   if (roperand) {
      if (src.roperand) {
        if (roperand!=src.roperand) {
          removeAst(roperand);
        }
      } else {
        removeAst(roperand);
      }
   }
   if (eoperand) {
      if (src.eoperand) {
        if (eoperand!=src.eoperand) {
          removeAst(eoperand);
        }
      } else {
        removeAst(eoperand);
      }
   }
   if (type == operandNode && oType == ConstantString)
       free((char *)oValue);
   referenceCount = src.referenceCount;
   referenceCount++;

   type = src.type;
   if (type == opCodeNode)
      op = src.op; // defined only for operand nodes

   if (type == callNode) {
      callee = src.callee; // defined only for call nodes
      calleefunc = src.calleefunc;
      for (unsigned i=0;i<src.operands.size();i++) 
        operands += assignAst(src.operands[i]); // .push_back(assignAst(src.operands[i]));
   }

   if (type == operandNode) {
      oType = src.oType;
      // XXX This is for the string type.  If/when we fix the string type to
      // make it less of a hack, we'll need to change this.
      if (oType == ConstantString)
	  oValue = (RegContents)strdup((char *)src.oValue);
      else
    	  oValue = src.oValue;
   }

   if (type == labelNode || type == gotoNode) {
       labelName = src.labelName;
   }

   loperand = assignAst(src.loperand);
   roperand = assignAst(src.roperand);
   eoperand = assignAst(src.eoperand);

   noSaveRestore = src.noSaveRestore;
   needSaveRestore = src.needSaveRestore;
   
   size = src.size;
#if defined(BPATCH_LIBRARY)
   bptype = src.bptype;
   doTypeCheck = src.doTypeCheck;
#endif

   return *this;
}

#if defined(ASTDEBUG)
static int ASTcount=0;

void ASTcounter()
{
  ASTcount++;
  sprintf(errorLine,"AstNode CONSTRUCTOR - ASTcount is %d\n",ASTcount);
  //logLine(errorLine);
}

void ASTcounterNP()
{
  ASTcount++;
}
#endif

AstNode::AstNode() {
#if defined(ASTDEBUG)
   ASTcounter();
#endif
   noSaveRestore = false;
   needSaveRestore = false;
   // used in mdl.C
   type = opCodeNode;
   op = noOp;
   loperand = roperand = eoperand = NULL;
   referenceCount = 1;
   useCount = 0;
   kept_register = Null_Register;
   // "operands" is left as an empty vector
   size = 4;
#if defined(BPATCH_LIBRARY)
   bptype = NULL;
   doTypeCheck = true;
#endif
}

AstNode::AstNode(const pdstring &func, AstNode *l, AstNode *r) {
#if defined(ASTDEBUG)
    ASTcounter();
#endif
    noSaveRestore = false;
    needSaveRestore = false;
    referenceCount = 1;
    useCount = 0;
    kept_register = Null_Register;
    type = callNode;
    callee = func;
    calleefunc = NULL;
    loperand = roperand = eoperand = NULL;
    if (l) operands += assignAst(l); // .push_back(assignAst(l));
    if (r) operands += assignAst(r); // .push_back(assignAst(r));
    size = 4;
#if defined(BPATCH_LIBRARY)
    bptype = NULL;
    doTypeCheck = true;
#endif
}

AstNode::AstNode(const pdstring &func, AstNode *l) {
#if defined(ASTDEBUG)
    ASTcounter();
#endif
    noSaveRestore = false;
    needSaveRestore = false;
    referenceCount = 1;
    useCount = 0;
    kept_register = Null_Register;
    type = callNode;
    loperand = roperand = eoperand = NULL;
    callee = func;
    calleefunc = NULL;
    if (l) operands += assignAst(l); // .push_back(assignAst(l));
    size = 4;
#if defined(BPATCH_LIBRARY)
    bptype = NULL;
    doTypeCheck = true;
#endif
}

AstNode::AstNode(const pdstring &func, pdvector<AstNode *> &ast_args) {
#if defined(ASTDEBUG)
   ASTcounter();
#endif
   noSaveRestore = false;
   needSaveRestore = false;
   referenceCount = 1;
   useCount = 0;
   kept_register = Null_Register;
   for (unsigned i=0;i<ast_args.size();i++) 
     if (ast_args[i]) operands += assignAst(ast_args[i]); // .push_back(assignAst(ast_args[i]));
   loperand = roperand = eoperand = NULL;
   type = callNode;
   callee = func;
   calleefunc = NULL;
   size = 4;
#if defined(BPATCH_LIBRARY)
   bptype = NULL;
   doTypeCheck = true;
#endif
}


AstNode::AstNode(function_base *func, pdvector<AstNode *> &ast_args) {
#if defined(ASTDEBUG)
   ASTcounter();
#endif
   noSaveRestore = false;
   needSaveRestore = false;
   referenceCount = 1;
   useCount = 0;
   kept_register = Null_Register;
   for (unsigned i=0;i<ast_args.size();i++) 
     if (ast_args[i]) operands += assignAst(ast_args[i]); // .push_back(assignAst(ast_args[i]));
   loperand = roperand = eoperand = NULL;
   type = callNode;
   callee = pdstring("dummy");
   calleefunc = 0;
   calleeAddr = func->getAddress();
   size = 4;
#if defined(BPATCH_LIBRARY)
   bptype = NULL;
   doTypeCheck = true;
#endif
}


// This is used to create a node for FunctionJump (function call with
// no linkage)
AstNode::AstNode(function_base *func) {
#if defined(ASTDEBUG)
   ASTcounter();
#endif
   noSaveRestore = false;
   needSaveRestore = false;
   referenceCount = 1;
   useCount = 0;
   kept_register = Null_Register;
   loperand = roperand = eoperand = NULL;
   type = opCodeNode;
   op = funcJumpOp;
   callee = func->prettyName();
   calleefunc = func;
   size = 4;
#if defined(BPATCH_LIBRARY)
   bptype = NULL;
   doTypeCheck = true;
#endif
}


AstNode::AstNode(operandType ot, RegContents arg) {
#if defined(ASTDEBUG)
    ASTcounterNP();
#endif
    noSaveRestore = false;
    needSaveRestore = false;
    referenceCount = 1;
    useCount = 0;
    kept_register = Null_Register;
    type = operandNode;
    oType = ot;
    if (ot == ConstantString)
	oValue = (RegContents)strdup((char *)arg);
    else
    	oValue = arg;
    loperand = roperand = eoperand = NULL;
    size = 4;
#if defined(BPATCH_LIBRARY)
    bptype = NULL;
    doTypeCheck = true;
#endif
};

// to create a newly added type for recognizing offset for locating variables
#if defined(MT_THREAD)
AstNode::AstNode(operandType ot, RegContents arg, bool isLev, unsigned v_level, unsigned v_index) {
    assert(ot == OffsetConstant);
#if defined(ASTDEBUG)
    ASTcounterNP();
#endif
    noSaveRestore = false;
    needSaveRestore = false;
    referenceCount = 1;
    useCount = 0;
    kept_register = Null_Register;
    type = operandNode;
    oType = ot;
    if (ot == ConstantString)
	oValue = (RegContents)strdup((char *)arg);
    else
    	oValue = arg;
    loperand = roperand = eoperand = NULL;
    size = 4;
#if defined(BPATCH_LIBRARY)
    bptype = NULL;
    doTypeCheck = true;
#endif
    // so far it's the same as AstNode(operandType ot, void *arg)
    // now assign isLevel and lvlOrIdx
    isLevel = isLev;
    lvl     = v_level;
    idx     = v_index;
};
#endif


AstNode::AstNode(operandType ot, AstNode *l) {
#if defined(ASTDEBUG)
    ASTcounter();
#endif
    noSaveRestore = false;
    needSaveRestore = false;
    referenceCount = 1;
    useCount = 0;
    kept_register = Null_Register;
    type = operandNode;
    oType = ot;
    oValue = 0;
    roperand = NULL;
    eoperand = NULL;
    loperand = assignAst(l);
    size = 4;
#if defined(BPATCH_LIBRARY)
    bptype = NULL;
    doTypeCheck = true;
#endif
};

// for sequence node
AstNode::AstNode(AstNode *l, AstNode *r) {
#if defined(ASTDEBUG)
   ASTcounter();
#endif
   noSaveRestore = false;
   needSaveRestore = false;
   referenceCount = 1;
   useCount = 0;
   kept_register = Null_Register;
   type = sequenceNode;
   loperand = assignAst(l);
   roperand = assignAst(r);
   eoperand = NULL;
   size = 4;
#if defined(BPATCH_LIBRARY)
   bptype = NULL;
   doTypeCheck = true;
#endif
};

AstNode::AstNode(opCode ot) {
#if defined(ASTDEBUG)
   ASTcounter();
#endif
   noSaveRestore = false;
   needSaveRestore = false;
   // a private constructor
   referenceCount = 1;
   useCount = 0;
   kept_register = Null_Register;
   type = opCodeNode;
   op = ot;
   loperand = roperand = eoperand = NULL;
   size = 4;
#if defined(BPATCH_LIBRARY)
   bptype = NULL;
   doTypeCheck = true;
#endif
}

AstNode::AstNode(opCode ot, AstNode *l) {
#if defined(ASTDEBUG)
   ASTcounter();
#endif
   noSaveRestore = false;
   needSaveRestore = false;
   // a private constructor
   referenceCount = 1;
   useCount = 0;
   kept_register = Null_Register;
   type = opCodeNode;
   op = ot;
   loperand = assignAst(l);
   roperand = NULL;
   eoperand = NULL;
   size = 4;
#if defined(BPATCH_LIBRARY)
   bptype = NULL;
   doTypeCheck = true;
#endif
}

AstNode::AstNode(opCode ot, AstNode *l, AstNode *r, AstNode *e) {
#if defined(ASTDEBUG)
   ASTcounter();
#endif
   noSaveRestore = false;
   needSaveRestore = false;
   referenceCount = 1;
   useCount = 0;
   kept_register = Null_Register;
   type = opCodeNode;
   op = ot;
   loperand = assignAst(l);
   roperand = assignAst(r);
   if (ot == storeAtomicOp) {
       // Atomic store gets hold of the variable as well as its address
       // We use the eoperand field to point to the address node
       assert(loperand != 0 && 
              ((loperand->oType == DataIndir)||(loperand->oType == DataIndir64))); // variable
       eoperand = assignAst(loperand->loperand);
   }
   else {
       eoperand = assignAst(e);
   }
   size = 4;
#if defined(BPATCH_LIBRARY)
   bptype = NULL;
   doTypeCheck = true;
#endif
};

AstNode::AstNode(AstNode *src) {
#if defined(ASTDEBUG)
   ASTcounter();
#endif
   noSaveRestore = false;
   needSaveRestore = false;
   referenceCount = 1;
   useCount = 0;
   kept_register = Null_Register;

   type = src->type;   
   if (type == opCodeNode)
      op = src->op;             // defined only for operand nodes

   if (type == callNode) {
      callee = src->callee;     // defined only for call nodes
      calleefunc = src->calleefunc;
      for (unsigned i=0;i<src->operands.size();i++) {
        if (src->operands[i]) operands += assignAst(src->operands[i]); // .push_back(assignAst(src->operands[i]));
      }
   }

   if (type == operandNode) {
      oType = src->oType;
      // XXX This is for the string type.  If/when we fix the string type to
      // make it less of a hack, we'll need to change this.
      if (oType == ConstantString)
	  oValue = (RegContents)strdup((char *)src->oValue);
      else
	  oValue = src->oValue;
   }

   if (type == labelNode || type == gotoNode) {
       labelName = src->labelName;
   }

   loperand = assignAst(src->loperand);
   roperand = assignAst(src->roperand);
   eoperand = assignAst(src->eoperand);
   size = 4;
#if defined(BPATCH_LIBRARY)
   bptype = src->bptype;
   doTypeCheck = src->doTypeCheck;
#endif
}

// AST node representing either goto_expr or label_expr
AstNode::AstNode(nodeType ntype, const pdstring& label) : 
    type(ntype), labelName(label)
{
   referenceCount = 1;
   useCount = 0;
   kept_register = Null_Register;
   loperand = roperand = eoperand = NULL;
   size = 0;
}

#if defined(ASTDEBUG)
#define AST_PRINT
#endif

#if defined(AST_PRINT)
void AstNode::printRC()
{
    sprintf(errorLine,"RC referenceCount=%d\n",referenceCount);
    logLine(errorLine);
    if (loperand) {
      logLine("RC loperand\n");
      loperand->printRC();
    }
    if (roperand) {
      logLine("RC roperand\n");
      roperand->printRC();
    }
    if (eoperand) {
      logLine("RC eoperand\n");
      eoperand->printRC();
    }
}
#endif

AstNode::~AstNode() {
#if defined(ASTDEBUG)
  ASTcount--;
  sprintf(errorLine,"AstNode DESTRUCTOR - ASTcount is %d\n",ASTcount);
  //logLine(errorLine);
#endif
  if (loperand) {
    removeAst(loperand);
  }
  if (roperand) {
    removeAst(roperand);
  }
  if (eoperand) {
    removeAst(eoperand);
  }
  if (type==callNode) {
    for (unsigned i=0;i<operands.size();i++) {
      removeAst(operands[i]);
    }
  } else if (type == operandNode && oType == ConstantString) {
      free((char*)oValue);
  }
}

//
// Increments/decrements the reference counter for the operands of a call 
// node. If "flag" is true, it increments the counter. Otherwise, it 
// decrements it.
//
void AstNode::updateOperandsRC(bool flag)
{
  if (type==callNode) {
    for (unsigned i=0;i<operands.size();i++) {
      if (operands[i]) {
        if (flag) operands[i]->referenceCount++;
        else operands[i]->referenceCount--;
      }
    }
  }
}

//
// This procedure should be used every time we assign an AstNode pointer,
// because it increments the reference counter.
//
AstNode *assignAst(AstNode *src) {
#if defined(ASTDEBUG)
  sprintf(errorLine,"assignAst(0x%08X): ", src);
  logLine(errorLine);
#endif
  if (src) {
    src->referenceCount++;
#if defined(ASTDEBUG)
    sprintf(errorLine,"referenceCount -> %d\n", src->referenceCount);
    logLine(errorLine);
  } else {
    logLine("NULL\n");
#endif
  }
  return(src);
}

//
// Decrements the reference count for "ast". If it is "0", it calls the 
// AstNode destructor.
//
void removeAst(AstNode *&ast) {
#if defined(ASTDEBUG)
  sprintf(errorLine,"removeAst(0x%08X): ", ast);
  logLine(errorLine);
#endif
  if (ast) {
#if defined(ASTDEBUG)
    sprintf(errorLine,"referenceCount=%d ", ast->referenceCount);
    logLine(errorLine);
#endif
    assert(ast->referenceCount>0);
    ast->referenceCount--;
    if (ast->referenceCount==0) {
#if defined(ASTDEBUG)
      logLine("deleting...");
#endif
      delete ast;
      ast=NULL;
#if defined(ASTDEBUG)
      logLine("deleted\n");
    } else {
      sprintf(errorLine,"-> %d\n", ast->referenceCount);
      logLine(errorLine);
#endif
    }
  } else {
#if defined(ASTDEBUG)
    logLine("non-existant!");
#endif
  }
}

//
// This procedure decrements the reference count for "ast" until it is 0.
//
void terminateAst(AstNode *&ast) {
  while (ast) {
    removeAst(ast);
  }
}

#if 0
// VG(11/05/01): The name of this function is misleading, as it
// generates the code for the ast, not just for the trampoline
// VG(11/06/01): Added location, needed by effective address AST node
HostAddress AstNode::generateTramp(process *proc, const instPoint *location,
			       char *i, HostAddress &count,
			       int *trampCost, bool noCost) {
    static AstNode *trailer=NULL;
    if (!trailer) trailer = new AstNode(trampTrailer); // private constructor
                                                       // used to estimate cost
    static AstNode *preambleTemplate=NULL;
    if (!preambleTemplate) {
      AstNode *tmp1 = new AstNode(Constant, (void *) 0);
      preambleTemplate = new AstNode(trampPreamble, tmp1);
      removeAst(tmp1);
    }
    // private constructor; assumes NULL for right child
    
    // we only want to use the cost of the minimum statements that will
    // be executed.  Statements, such as the body of an if statement,
    // will have their costs added to the observed cost global variable
    // only if they are indeed called.  The code to do this in the minitramp
    // right after the body of the if.
    *trampCost = preambleTemplate->maxCost() + minCost() + trailer->maxCost();
    int cycles = *trampCost;

    AstNode *preamble, *tmp2;
    tmp2 = new AstNode(Constant, (void *) cycles);
    preamble = new AstNode(trampPreamble, tmp2); 
    removeAst(tmp2);
    // private constructor; assumes NULL for right child

    initTramps(); // needed to initialize regSpace below
                  // shouldn't we do a "delete regSpace" first to avoid
                  // leaking memory?

    regSpace->resetSpace();
    HostAddress ret=0;
    if (type != opCodeNode || op != noOp) {
        Register tmp;
	preamble->generateCode(proc, regSpace, i, count, noCost, true);
        removeAst(preamble);
	tmp = (Register)generateCode(proc, regSpace, i, count, noCost, true, location);
        regSpace->freeRegister(tmp);
        ret = trailer->generateCode(proc, regSpace, i, count, noCost, true);
    } else {
        removeAst(preamble);
        emitV(op, 1, 0, 0, i, count, noCost);   // op==noOp
    }
    regSpace->resetSpace();
    return(ret);
}
#endif

bool isPowerOf2(int value, int &result)
{
  if (value<=0) return(false);
  if (value==1) {
    result=0;
    return(true);
  }
  if ((value%2)!=0) return(false);
  if (isPowerOf2(value/2,result)) {
    result++;
    return(true);
  }
  else return(false);
}

void AstNode::setUseCount(registerSpace *rs) 
{
    if (useCount == 0 || !canBeKept()) {
	kept_register = Null_Register;

	if (type != opCodeNode || (op != storeOp && op != getAddrOp)) {
            // Common case. Set useCounts for our children.
	    pdvector<AstNode*> children;
	    getChildren(&children);
	    for (unsigned i=0; i<children.size(); i++) {
		children[i]->setUseCount(rs);
	    }
	}
	else { // storeOp and getAddrOp
	    if (loperand != 0) {
		// Bypass loperand->setUseCount
		if ((loperand->oType == DataIndir) ||
                    (loperand->oType == DataIndir64)) {
		    loperand->loperand->setUseCount(rs);
		}
	    }
	    if (roperand != 0) {
		roperand->setUseCount(rs);
	    }
	}
    }
    // Occasionally, we call setUseCount to fix up useCounts in the middle
    // of code generation. If the node has already been computed, we need to
    // bump up the ref count of the register that keeps our value
    if (hasKeptRegister()) {
	rs->incRefCount(kept_register);
    }
    useCount++;
}

void AstNode::cleanUseCount(void)
{
    useCount = 0;
    kept_register = Null_Register;

    pdvector<AstNode*> children;
    getChildren(&children);
    for (unsigned i=0; i<children.size(); i++) {
	children[i]->cleanUseCount();
    }
}

bool AstNode::checkUseCount(registerSpace* rs, bool& err)
{
    assert(useCount == 0);
    
    pdvector<AstNode*> children;
    getChildren(&children);
    for (unsigned i=0; i<children.size(); i++) {
	children[i]->checkUseCount(rs, err);
    }
    return err;
}

void AstNode::checkRefCount()
{
    assert(referenceCount > 0);

    pdvector<AstNode*> children;
    getChildren(&children);
    for (unsigned i=0; i<children.size(); i++) {
	children[i]->checkRefCount();
    }
}

#if defined(ASTDEBUG)
void AstNode::printUseCount(void)
{
}
#endif

static pdstring getUniqueLabel()
{
    static int id = 0;
    char buffer[64];
    sprintf(buffer, "label_%d", id++);
    return pdstring(buffer);
}

// Allocate a register and make it available for sharing if our
// node is shared
Register AstNode::allocateAndKeep(registerSpace *rs, 
				  const pdvector<AstNode*> &ifForks,
				  char *insn, HostAddress &base, bool noCost)
{
    // Allocate a register
    Register dest = rs->allocateRegister(useCount+1, insn, base, noCost);

    // Make this register available for sharing
    if (useCount > 0) {
	keepRegister(dest, ifForks);
    }

    return dest;
}

//
// This procedure generates code for an AST DAG. If there is a sub-graph
// being shared between more than 1 node, then the code is generated only
// once for this sub-graph and the register where the return value of the
// sub-graph is stored, is kept allocated until the last node sharing the
// sub-graph has used it (freeing it afterwards). A count called "useCount"
// is used to determine whether a particular node or sub-graph is being
// shared. At the end of the call to generate code, this count must be 0
// for every node. Another important issue to notice is that we have to make
// sure that if a node is not calling generate code recursively for either
// its left or right operands, we then need to make sure that we update the
// "useCount" for these nodes (otherwise we might be keeping registers
// allocated without reason). 
//
// This code was modified in order to set the proper "useCount" for every
// node in the DAG before calling the original generateCode procedure (now
// generateCode_phase2). This means that we are traversing the DAG twice,
// but with the advantage of potencially generating more efficient code.
//
// Note: a complex Ast DAG might require more registers than the ones 
// currently available. In order to fix this problem, we will need to 
// implement a "virtual" register allocator - naim 11/06/96
//
HostAddress AstNode::generateCode(process *proc,
			      registerSpace *rs,
			      char *insn, 
			      HostAddress &base, bool noCost, bool root,
			      const instPoint *location) {
  // Note: MIPSPro compiler complains about redefinition of default argument
  checkRefCount();
  if (root) {
    cleanUseCount();
    setUseCount(rs);
    computeRegNeedEstimate();
#if defined(ASTDEBUG)
    print() ;
#endif
  }

  rs->checkLeaks(Null_Register);

  // Create an empty vector to keep track of if statements
  pdvector<AstNode*> ifForks;
  // note: this could return the value "(HostAddress)(-1)" -- csserra
  HostAddress tmp = generateCode_phase2(proc, rs, insn, base, noCost, 
					ifForks, location);

  rs->checkLeaks(tmp);

  if (root) {
      bool err = false ;
      checkUseCount(rs, err) ;
  }
  return(tmp);
}

// VG(11/06/01): Make sure location is passed to all descendants
HostAddress AstNode::generateCode_phase2(process *proc,
					 registerSpace *rs,
					 char *insn, 
					 HostAddress &base, 
					 bool noCost,
					 const pdvector<AstNode*> &ifForks,
					 const instPoint *location) {
  // Note: MIPSPro compiler complains about redefinition of default argument
    HostAddress addr;
    HostAddress fromAddr;
    HostAddress startInsn;
    Register src1, src2;
    Register src = Null_Register;
    Register dest = Null_Register;
    Register right_dest = Null_Register;

    useCount--;
    assert(useCount >= 0);
#if defined(ASTDEBUG)
    sprintf(errorLine,"### location: %p ###\n", location);
    logLine(errorLine);
#endif
    if (hasKeptRegister()) {
	// Before using kept_register we need to make sure it was computed
	// on the same path as we are right now
	if (subpath(kept_path, ifForks)) { 
	    if (useCount == 0) { 
		Register tmp = kept_register;
		unkeepRegister();
#if 1 || defined(BPATCH_LIBRARY_F)
		assert(!rs->isFreeRegister(tmp));
#endif
		return (HostAddress)tmp;
	    }
	    return (HostAddress)kept_register;
	}
	else {
	    // Can't keep the register anymore
	    forceUnkeepAndFree(rs);
	    fixChildrenCounts(rs);
	}
    }

    if (type == opCodeNode) {
        if (op == branchOp) {
	    assert(loperand->oType == Constant);
	    unsigned offset = loperand->oValue;
	    loperand->decUseCount(rs);
            (void)emitA(branchOp, 0, 0, (RegContents)offset, insn, base, noCost);
        } 
	else if (op == ifOp) {
            // This ast returns the result of the "then" or "else" clause,
	    // depending on which one is true. Not unlike the "?" operator in C
	    src = (Register)loperand->generateCode_phase2(proc, rs, insn, base,
							  noCost, ifForks, 
							  location);
	    pdstring else_if = getUniqueLabel();
	    emitCondBranch(eqOp, src, else_if, insn);

            rs->freeRegister(src);
	    // Body of the true clause
	    pdvector<AstNode*> thenFork = ifForks;
	    thenFork.push_back(roperand); // Add the forked node to the path
            dest = (Register)roperand->generateCode_phase2(proc, rs, insn,
							   base, noCost,
							   thenFork,
							   location);
	    // Is there an else clause?
	    if (eoperand) {
		// Branch over the else clause at the end of the true clause
		pdstring end_if = getUniqueLabel();
		emitJump(end_if, insn);
		
		// Generate code for the else clause
		emitLabel(else_if, insn);
		pdvector<AstNode*> elseFork = ifForks;
		elseFork.push_back(eoperand);// Add the forked node to the path
		
		Register tmp = 
		    (Register)eoperand->generateCode_phase2(proc, rs, insn,
							    base, noCost, 
							    elseFork,
							    location);
		// Move tmp into dest to return the result
		emitV(movOp, tmp, Null_Register, dest, insn, base, noCost);
		rs->freeRegister(tmp);
		emitLabel(end_if, insn);
	    }
	    else {
		emitLabel(else_if, insn);
	    }
	} 
	else if (op == whileOp) {
	    HostAddress top = base ;
	    src = (Register)loperand->generateCode_phase2(proc, rs, insn, base,
							  noCost, ifForks,
							  location);
	    startInsn = base;
	    fromAddr = emitA(ifOp, src, 0, 0, insn, base, noCost);
            rs->freeRegister(src);
	    if (roperand) {
                Register tmp = 
		    (Register)roperand->generateCode_phase2(proc, rs, insn, 
							    base, noCost, 
							    ifForks, location);
                rs->freeRegister(tmp);
	    }
	    //jump back
	    (void) emitA(branchOp, 0, 0, (Register)(top-base),
			insn, base, noCost) ;

	    // call emit again now with correct offset.
	    (void) emitA(ifOp, src, 0, (Register) (base-fromAddr), 
			insn, startInsn, noCost);
            // sprintf(errorLine,"branch forward %d\n", base - fromAddr);
	} else if (op == doOp) {
	    assert(0) ;
	} else if (op == getAddrOp) {
	    if (loperand->oType == DataAddr) {
		addr = (HostAddress)loperand->oValue;
		assert(addr != 0); // check for NULL
		dest = allocateAndKeep(rs, ifForks, insn, base, noCost);
		emitVload(loadConstOp, addr, dest, dest, insn, base, noCost);
	    } else if (loperand->oType == FrameAddr) {
		// load the address fp + addr into dest
		dest = allocateAndKeep(rs, ifForks, insn, base, noCost);
		Register temp = rs->allocateRegister(1, insn, base, noCost);
		addr = (HostAddress) loperand->oValue;
		emitVload(loadFrameAddr, addr, temp, dest, insn, 
		     base, noCost);
		rs->freeRegister(temp);
	    } else if ((loperand->oType == DataIndir) ||
                       (loperand->oType == DataIndir64)) {	
		// taking address of pointer de-ref returns the original
		//    expression, so we simple generate the left child's 
		//    code to get the address 
		dest = (Register)loperand->loperand->generateCode_phase2(proc, 
		     rs, insn, base, noCost, ifForks, location);
	    } else {
		// error condition
		assert(0);
	    }
	} else if (op == storeOp) {
            // This ast cannot be shared because it doesn't return a register
	    src1 = (Register)roperand->generateCode_phase2(proc, rs, insn, 
							   base, noCost, 
							   ifForks, location);

	    // loperand's value will be invalidated. Discard the cached reg
	    if (loperand->hasKeptRegister()) {
		loperand->forceUnkeepAndFree(rs);
	    }

	    if (loperand->oType == DataAddr ) {
		addr = (HostAddress)loperand->oValue;
		assert(addr != 0); // check for NULL
		Register scratch = rs->allocateRegister(1, insn, base, noCost);
		emitVstore(storeOp, src1, scratch, addr,
			   insn, base, noCost, size);
		rs->freeRegister(scratch);

	    } else if (loperand->oType == FrameAddr) {
		addr = (HostAddress) loperand->oValue;
		assert(addr != 0); // check for NULL
		Register scratch = rs->allocateRegister(1, insn, base, noCost);
		emitVstore(storeFrameRelativeOp, src1, scratch, addr,
			   insn, base, noCost, size);
		rs->freeRegister(scratch);
	    } else if ((loperand->oType == DataIndir) ||
                       (loperand->oType == DataIndir64)) {
		// store to a an expression (e.g. an array or field use)
		// *(+ base offset) = src1
		Register tmp = (Register)loperand->loperand->
		    generateCode_phase2(proc, rs, insn, base, noCost, 
					ifForks, location);
		// tmp now contains address to store into
		emitV(storeIndirOp, src1, 0, tmp, insn, base, noCost, size);
		rs->freeRegister(tmp);
	    } else if (loperand->oType == DataReg) {
		// mov, not really a store: loperand is a fixed register
		if (!noSaveRestore) {
		    dout << "FIXME: store to a potentially wrong register!\n";
		}
		emitV(movOp, src1, Null_Register, (Register)loperand->oValue,
		      insn, base, noCost);
	    } else {
		// invalid oType passed to store
		cerr << "invalid oType passed to store: " << (int)loperand->oType << endl;
		abort();
	    }
	    // src1 keeps the new value for loperand
	    if (loperand->useCount > 0) {
		loperand->keepRegister(src1, ifForks);
		rs->fixRefCount(src1, loperand->useCount);
	    }
	    else {
		rs->freeRegister(src1);
	    }
	} else if (op == storeIndirOp) {
	    src1 = (Register)roperand->generateCode_phase2(proc, rs, insn, 
							   base, noCost, 
							   ifForks, location);
            src2 = (Register)loperand->generateCode_phase2(proc, rs, insn, 
							   base, noCost, 
							   ifForks, location);
            emitV(op, src1, 0, src2, insn, base, noCost);          
	    rs->freeRegister(src1);
            rs->freeRegister(src2);
	} else if (op == storeAtomicOp) {
	    // Get the value of the variable in src1
	    src1 = (Register)loperand->generateCode_phase2(proc, rs, insn, 
							   base, noCost,
							   ifForks, location);

	    // Emit the restart label. Notice that we do not reload src1
	    // on restart -- emitAtomicExchange will do this for us
	    pdstring restart = getUniqueLabel();
	    emitLabel(restart, insn);

	    // We have to prevent the roperand subtree from using cached
	    // values of the nodes that can depend on our CAS variable
	    // (loperand), since it will change if CAS fails
	    roperand->invalidateAncestorsOf(loperand, rs);

	    // Make sure that if a node keeps a live value at the entry
	    // point to this primitive, we do not clobber its value and
	    // keep it till the end, since we may need to reexecute this
	    // code if CAS fails
	    pdvector<AstNode *> live;
	    getAllLive(&live);
	    // Bump-up their useCounts temprorarily
	    for (unsigned i=0; i<live.size(); i++) {
		live[i]->incUseCount(rs);
	    }
	    
	    // compute expr (may involve var)
            src2 = (Register)roperand->generateCode_phase2(proc, rs, insn, 
							   base, noCost,
							   ifForks, location);
	    
	    // Drop the useCounts back
	    for (unsigned i=0; i<live.size(); i++) {
		live[i]->decUseCount(rs);
	    }

	    // Obtain the address of the variable
	    Register addr_reg = 
		(Register)eoperand->generateCode_phase2(proc, rs, insn, base,
							noCost, ifForks,
							location);
	    Register temp;
#ifdef ppc64_unknown_linux2_4
	    temp = rs->allocateRegister(1, insn, base, noCost);
#endif
            emitAtomicExchange(addr_reg, src1, src2, temp, restart, insn);
            rs->freeRegister(src2);
	    rs->freeRegister(addr_reg);
#ifdef ppc64_unknown_linux2_4
	    rs->freeRegister(temp);
#endif
            // loperand's value has been invalidated. Discard the cached reg
            if (loperand->hasKeptRegister()) {
               loperand->forceUnkeepAndFree(rs);
               loperand->fixChildrenCounts(rs);
            } else {
               rs->freeRegister(src1);
            }
   
	} else if (op == trampTrailer) {
	    // What to do with this one? It returns an address, not a register,
	    // but this address is used in Paradyn / Dyninst
	    assert(false);
            // This ast cannot be shared because it doesn't return a register
	    return emitA(op, 0, 0, 0, insn, base, noCost);
	} else if (op == trampPreamble) {
	    // See comments for trampTrailer above
	    assert(false);
            // This ast cannot be shared because it doesn't return a register
#if defined (i386_unknown_solaris2_5) || (sparc_sun_solaris2_4)
	    // loperand is a constant AST node with the cost, in cycles.
	    //int cost = noCost ? 0 : (int) loperand->oValue;
            HostAddress costAddr = 0; // for now... (won't change if noCost is set)
            loperand->useCount--;
	    assert(loperand->useCount >= 0);

#ifndef SHM_SAMPLING
	    bool err;
	    costAddr = proc->findInternalHostAddress("DYNINSTobsCostLow", true, err);
	    if (err) {
                pdstring msg = pdstring("Internal error: "
                        "unable to find addr of DYNINSTobsCostLow\n");
		showErrorCallback(79, msg.string_of());
		P_abort();
	    }
#else
	    if (!noCost)
	       costAddr = (HostAddress)proc->getObsCostLowAddrInApplicSpace();
#endif
	    return emitA(op, 0, 0, 0, insn, base, noCost);
#endif
	} else if (op == funcJumpOp) {
	     emitFuncJump(funcJumpOp, insn, base, calleefunc, proc);
	} else if (op == retOp) {
	    emitRet(insn);
        }
#ifdef sparc_sun_solaris2_7
	else if (op == getPIL) {
	    // Read the priority interrupt level
	    dest = allocateAndKeep(rs, ifForks, insn, base, noCost);
	    emitVload(getPIL, 0 /*src1*/, Null_Register, dest, 
		      insn, base, noCost);
	} else if (op == setPIL) {
	    // Set the priority interrupt level
	    Register newval = (Register)loperand->generateCode_phase2(
		proc, rs, insn, base, noCost, ifForks, location);
	    
	    emitVstore(setPIL, newval, Null_Register /*scratch*/, 0 /*dest*/, 
		       insn, base, noCost);
	    
	    rs->freeRegister(newval);
        }
#elif defined(i386_unknown_linux2_4)
        else if ((op == disableLocalInterrupts) || 
                 (op == enableLocalInterrupts)) {
           emitV(op, 0, 0, 0, insn, base, noCost);
        }
#elif defined(ppc64_unknown_linux2_4) 
        else if ((op == disableLocalInterrupts) ||
                 (op == enableLocalInterrupts)) {
           Register temp;
           temp = rs->allocateRegister(1, insn, base, noCost);
           emitV(op, 0, 0, temp, insn,base,noCost);
           rs->freeRegister(temp);
        }
#endif
	else {
	    AstNode *left = assignAst(loperand);
	    AstNode *right = assignAst(roperand);

	    if (left && right) {
              if (left->type == operandNode && left->oType == Constant) {
                if (type == opCodeNode) {
                  if (op == plusOp) {
  	            AstNode *temp = assignAst(right);
	            right = assignAst(left);
	            left = assignAst(temp);
                    removeAst(temp);
	          } else if (op == timesOp) {
                    if (right->type == operandNode) {
                      if (right->oType != Constant) {
                        AstNode *temp = assignAst(right);
	                right = assignAst(left);
	                left = assignAst(temp);
                        removeAst(temp);
		      }
                      else {
                        int result;
                        if (!isPowerOf2((HostAddress)right->oValue,result) &&
                             isPowerOf2((HostAddress)left->oValue,result))
                        {
                          AstNode *temp = assignAst(right);
	                  right = assignAst(left);
	                  left = assignAst(temp);
                          removeAst(temp);
		        }
		      }
		    }
		  }
	        }
	      }
	    }

	    src = Null_Register;
	    right_dest = Null_Register;

            if (right && (right->type == operandNode) &&
                (right->oType == Constant) &&
                doNotOverflow(right->oValue) &&
                (type == opCodeNode))
	    {
		if (left) {
		    src = (Register)left->generateCode_phase2(proc, rs, insn, 
							      base, noCost, 
							      ifForks,
							      location);
		    rs->freeRegister(src); // may be able to reuse for dest
		}
		dest = allocateAndKeep(rs, ifForks, insn, base, noCost);
#ifdef alpha_dec_osf4_0
	      if (op == divOp)
		{
		  assert(false);
		  /* XXX
		   * The following doesn't work right, because we don't save
		   * and restore the scratch registers before and after the
		   * call!
		   */
		  /*
		  bool err;
		  HostAddress divlAddr = proc->findInternalAddress("divide", true, err);  
		  assert(divlAddr);
		  software_divide(src,right->oValue,dest,insn,base,noCost,divlAddr,true);
		  */
		}
	      else
#endif
	      emitImm(op, src, right->oValue, dest, insn, base, noCost);
	     
	      // We do not .generateCode for right, so need to update its
	      // refcounts manually
              right->decUseCount(rs);
	    }
	    else {
		Register lval = Null_Register, rval = Null_Register;
		// We could try to use the Sethi-Ullman algorithm and emit 
		// the more register-demanding branch first. However,
		// certain nodes rely on the left-first order of evaluation 
		// (e.g., the sequence node)
		if (left) {
		    lval = (Register)left->generateCode_phase2(proc, rs, 
							       insn, base, 
							       noCost,
							       ifForks,
							       location);
		}
		if (right) {
		    rval = (Register)right->generateCode_phase2(proc, rs, 
								insn, base, 
								noCost,
								ifForks,
								location);
		}
		rs->freeRegister(lval); // may be able to reuse for dest
		rs->freeRegister(rval); // may be able to reuse for dest
		dest = allocateAndKeep(rs, ifForks, insn, base, noCost);
	      
#ifdef alpha_dec_osf4_0
	      if (op == divOp)
		{
		  assert(false);
		  /* XXX
		   * The following doesn't work right, because we don't save
		   * and restore the scratch registers before and after the
		   * call!
		   */
		  /*
		  bool err;
		  HostAddress divlAddr = proc->findInternalAddress("divide", true, err);  
		  assert(divlAddr);
		  software_divide(src,right_dest,dest,insn,base,noCost,divlAddr);
		  */
		}
	      else 
#endif
		  emitV(op, lval, rval, dest, insn, base, noCost);
	    }
            removeAst(left);
            removeAst(right);
	}
    } else if (type == operandNode) {
	// Allocate a register to return
	dest = allocateAndKeep(rs, ifForks, insn, base, noCost);

	if (oType == Constant) {
	    emitVload(loadConstOp, (HostAddress)oValue, dest, dest, 
		      insn, base, noCost);
	} else if (oType == ConstantPtr) {
	    emitVload(loadConstOp, (*(HostAddress *) oValue), dest, dest, 
                        insn, base, noCost);
#if defined(MT_THREAD)
	} else if (oType == OffsetConstant) {  // a newly added type for recognizing offset for locating variables
	    emitVload(loadConstOp, (HostAddress)oValue, dest, dest, 
                        insn, base, noCost);
#endif
	} else if (oType == DataPtr) { // restore AstNode::DataPtr type
	  addr = (HostAddress) oValue;
	  assert(addr != 0); // check for NULL
	  emitVload(loadConstOp, addr, dest, dest, insn, base, noCost);
	} else if ((oType == DataIndir)||(oType == DataIndir64)) {
	    src = (Register)loperand->generateCode_phase2(proc, rs, insn, 
							  base, noCost, 
							  ifForks, location);
#ifdef BPATCH_LIBRARY
	    BPatch_type *Type = const_cast<BPatch_type *> (getType());
	    assert(Type);
	    int tSize = Type->getSize();
#else
	    int tSize = sizeof(int);
#endif
	    emitV(loadIndirOp, src, 0, dest, insn, base, noCost, tSize); 
            rs->freeRegister(src);
	} 
	else if (oType == DataReg) {
	    // oValue has not been properly allocated, move it to dest
	    // Furthermore, it may have been concealed by the save that
	    // we emitted, so we may need to uncover it first
	    if (noSaveRestore) {
		// No extra save-restore has been emitted.
		emitV(movOp, (Register)oValue, Null_Register, dest,
		      insn, base, noCost);
	    }
	    else {
		// We have wrapped the expression with save/restore.
		emitLoadPreviousStackFrameRegister((Register)oValue, dest,
						   insn, base, size, noCost);
	    }
	} 
	else if(oType == PreviousStackFrameDataReg)
	  emitLoadPreviousStackFrameRegister((Register)oValue, dest,insn, base,
					     size, noCost);
	else if (oType == DataId) {
	    emitVload(loadConstOp, (HostAddress)oValue, dest, dest, 
                        insn, base, noCost);
	} else if (oType == DataValue) {  // restore AstNode::DataValue type
	  addr = (HostAddress) oValue;
	  assert(addr != 0); // check for NULL
	  emitVload(loadOp, addr, dest, dest, insn, base, noCost);
	} else if (oType == ReturnVal) {
	    Register scratch = rs->allocateRegister(1, insn, base, noCost);
	    HostAddress entryAddr = (HostAddress)oValue;
	    HostAddress spliceAddr = location->insnAddress();
	    assert(location != 0 && 
		   "Can't emit relocatable code with retval_expr");
	    if (noSaveRestore) {
		// No extra save-restore has been emitted. The return value
		// is probably (unless the return sequence is optimized) in O0
		emitR(getSysRetValOp, dest, scratch, entryAddr, spliceAddr,
		      insn, base, noCost);
	    }
	    else {
		// We have wrapped the expression with save/restore. The return
		// value is probably (unless the return sequence is optimized)
		// in I0
		emitR(getRetValOp, dest, scratch, entryAddr, spliceAddr,
		      insn, base, noCost);
	    }
	    rs->freeRegister(scratch);
		
	} else if (oType == Param) {
#ifdef sparc_sun_solaris2_7 
	    if (noSaveRestore) {
		// No extra save-restore has been emitted. Params must be
		// in O regs
		emitR(getSysParamOp, dest, (Register)oValue, 0, 0,
		      insn, base, noCost);
	    }
	    else 
#endif
		// We have wrapped the expression with save/restore. Params
		// are in I regs
		emitR(getParamOp, dest, (Register)oValue, 0, 0,
		      insn, base, noCost);
	    
	} else if (oType == DataAddr) {
	  addr = (HostAddress)oValue;
	  emitVload(loadOp, addr, dest, dest, insn, base, noCost, size);
	} else if (oType == FrameAddr) {
	  addr = (HostAddress) oValue;
	  Register temp = rs->allocateRegister(1, insn, base, noCost);
	  emitVload(loadFrameRelativeOp, addr, temp, dest, insn, base, noCost);
	  rs->freeRegister(temp);
	} else if (oType == EffectiveAddr) {
	  // VG(11/05/01): get effective address
#ifdef BPATCH_LIBRARY
	  // 1. get the point being instrumented & memory access info
	  assert(location);
	  const BPatch_memoryAccess* ma = location->getBPatch_point()->getMemoryAccess();
	  if(!ma) {
	    fprintf(stderr, "Memory access information not available at this point.\n");
	    fprintf(stderr, "Make sure you create the point in a way that generates it.\n");
	    fprintf(stderr, "E.g.: find*Point(const BPatch_Set<BPatch_opCode>& ops).\n");
	    assert(0);
	  }
	  BPatch_addrSpec_NP start = ma->getStartAddr();
	  emitASload(start, dest, insn, base, noCost);
#else
	  fprintf(stderr, "Effective address feature not supported w/o BPatch!\n");
	  assert(0);
#endif
	} else if (oType == BytesAccessed) {
#ifdef BPATCH_LIBRARY
	  // 1. get the point being instrumented & memory access info
	  assert(location);
	  const BPatch_memoryAccess* ma = location->getBPatch_point()->getMemoryAccess();
	  if(!ma) {
	    fprintf(stderr, "Memory access information not available at this point.\n");
	    fprintf(stderr, "Make sure you create the point in a way that generates it.\n");
	    fprintf(stderr, "E.g.: find*Point(const BPatch_Set<BPatch_opCode>& ops).\n");
	    assert(0);
	  }
	  BPatch_countSpec_NP count = ma->getByteCount();
	  emitCSload(count, dest, insn, base, noCost);
#else
	  fprintf(stderr, "Byte count feature not supported w/o BPatch!\n");
	  assert(0);
#endif
	} else if (oType == ConstantString) {
	    assert(false);
#if 0
	  // XXX This is for the string type.  If/when we fix the string type
	  // to make it less of a hack, we'll need to change this.
	  int len = strlen((char *)oValue) + 1;
	  addr = (HostAddress) inferiorMalloc(proc, len, textHeap); //dataheap
	  if (!proc->writeDataSpace((char *)addr, len, (char *)oValue))
	    perror("ast.C(1351): writing string value");
	  emitVload(loadConstOp, addr, dest, dest, insn, base, noCost);
#endif
	} else if (oType == CPUId) {
	    emitVload(loadCPUId, 0 /*src1*/, Null_Register, dest, 
		      insn, base, noCost);
	} else if (oType == ProcessId) {
	    emitVload(loadProcessId, 0 /*src1*/, Null_Register, dest, 
		      insn, base, noCost);
	} else if (oType == HwCounterReg) {
	    emitVload(loadHwCtrReg, oValue, Null_Register, dest, 
		      insn, base, noCost);
	}
    } else if (type == callNode) {
	// Call node's value can be shared. To force reevaluation of the node
	// the API user should create another call node for the same function
	dest = allocateAndKeep(rs, ifForks, insn, base, noCost);
	emitFuncCall(callOp, rs, insn, base, calleeAddr, operands,
		     proc, noCost, dest, ifForks, location);
    } else if (type == labelNode) {
	emitLabel(labelName, insn);
    } else if (type == gotoNode) {
	// FIXME
	assert(false && "implement me");
    } else if (type == sequenceNode) {
#if 0 && defined(BPATCH_LIBRARY_F)
	(void) loperand->generateCode_phase2(proc, rs, insn, base, noCost, 
					     ifForks, location);
	// mirg: shouldn't we free the returned register?
#else
	Register tmp = loperand->generateCode_phase2(proc, rs, insn, base, 
						     noCost, ifForks, 
						     location);
	rs->freeRegister(tmp);
#endif
 	dest =  roperand->generateCode_phase2(proc, rs, insn, base, 
					      noCost, ifForks);
    }

    // assert (dest != Null_Register); // oh dear, it seems this happens!

    return (HostAddress)dest;
}


#if defined(AST_PRINT)
pdstring getOpString(opCode op)
{
    switch (op) {
	case plusOp: return("+");
	case minusOp: return("-");
	case timesOp: return("*");
	case divOp: return("/");
	case lessOp: return("<");
	case leOp: return("<=");
	case greaterOp: return(">");
	case geOp: return(">=");
	case eqOp: return("==");
	case neOp: return("!=");
	case loadOp: return("lda");
	case loadConstOp: return("load");
	case storeOp: return("=");
	case ifOp: return("if");
	case whileOp: return("while") ;
	case doOp: return("while") ;
	case trampPreamble: return("preTramp");
	case trampTrailer: return("goto");
	case branchOp: return("goto");
	case noOp: return("nop");
	case andOp: return("and");
	case orOp: return("or");
	case shlOp: return("shl");
	case shrOp: return("shr");
	case movOp: return("mov");
	case loadIndirOp: return("load&");
	case storeIndirOp: return("=&");
	case loadFrameRelativeOp: return("load $fp");
        case loadFrameAddr: return("$fp");
	case storeFrameRelativeOp: return("store $fp");
	case getAddrOp: return("&");
	default: return("ERROR");
    }
}
#endif

#undef MIN
#define MIN(x,y) ((x)>(y) ? (y) : (x))
#undef MAX
#define MAX(x,y) ((x)>(y) ? (x) : (y))
#undef AVG
#define AVG(x,y) (((x)+(y))/2)

int AstNode::costHelper(enum CostStyleType costStyle) const {
    int total = 0;
    int getInsnCost(opCode t);

    if (type == opCodeNode) {
        if (op == ifOp) {
	    // loperand is the conditional expression
	    if (loperand) total += loperand->costHelper(costStyle);
	    total += getInsnCost(op);
	    int rcost = 0, ecost = 0;
	    if (roperand) {
		rcost = roperand->costHelper(costStyle);
		if (eoperand)
		    rcost += getInsnCost(branchOp);
	    }
	    if (eoperand)
		ecost = eoperand->costHelper(costStyle);
	    if(ecost == 0) { // ie. there's only the if body
	      if(costStyle      == Min)  total += 0;
	                                  //guess half time body not executed
	      else if(costStyle == Avg)  total += rcost / 2;
	      else if(costStyle == Max)  total += rcost;
	    } else {  // ie. there's an else block also, for the statements
	      if(costStyle      == Min)  total += MIN(rcost, ecost);
	      else if(costStyle == Avg)  total += AVG(rcost, ecost);
	      else if(costStyle == Max)  total += MAX(rcost, ecost);
	    }
	} else if (op == storeOp) {
	    if (roperand) total += roperand->costHelper(costStyle);
	    total += getInsnCost(op);
	} else if (op == storeIndirOp) {
            if (loperand) total += loperand->costHelper(costStyle);
	    if (roperand) total += roperand->costHelper(costStyle);
	    total += getInsnCost(op);
	} else if (op == trampTrailer) {
	    total = getInsnCost(op);
	} else if (op == trampPreamble) {
	    total = getInsnCost(op);
	} else {
	    if (loperand) 
		total += loperand->costHelper(costStyle);
	    if (roperand) 
		total += roperand->costHelper(costStyle);
	    total += getInsnCost(op);
	}
    } else if (type == operandNode) {
	if (oType == Constant) {
	    total = getInsnCost(loadConstOp);
#if defined(MT_THREAD)
	} else if (oType == OffsetConstant) {  // a newly added type for recognizing offset for locating variables
	    total = getInsnCost(loadConstOp);
#endif
	} else if (oType == DataPtr) {  // restore AstNode::DataPtr type
	  total = getInsnCost(loadConstOp);
	} else if (oType == DataValue) {  // restore AstNode::DataValue type
	  total = getInsnCost(loadOp); 
	} else if (oType == DataId) {
	    total = getInsnCost(loadConstOp);
	} else if ((oType == DataIndir) ||(oType == DataIndir64)) {
	    total = getInsnCost(loadIndirOp);
            total += loperand->costHelper(costStyle);
	} else if (oType == DataAddr) {
	    total = getInsnCost(loadOp);
	} else if (oType == DataReg) {
	    total = getInsnCost(loadIndirOp);
	} else if (oType == Param) {
	    total = getInsnCost(getParamOp);
	}
    } else if (type == callNode) {
	total = 0; // getPrimitiveCost(callee);
	for (unsigned u = 0; u < operands.size(); u++)
	  if (operands[u]) total += operands[u]->costHelper(costStyle);
    } else if (type == sequenceNode) {
	if (loperand) total += loperand->costHelper(costStyle);
	if (roperand) total += roperand->costHelper(costStyle);
    }
    return(total);
}

#if defined(AST_PRINT)
void AstNode::print() const {
  if (this) {
#if defined(ASTDEBUG)
    sprintf(errorLine,"{%d}", referenceCount) ;
    logLine(errorLine) ;
#endif
    if (type == operandNode) {
      if (oType == Constant) {
	sprintf(errorLine, " %lld", (HostAddress) oValue);
	logLine(errorLine);
      } else if (oType == ConstantString) {
        sprintf(errorLine, " %s", (char *)oValue);
	logLine(errorLine) ;
#if defined(MT_THREAD)
      } else if (oType == OffsetConstant) {  // a newly added type for recognizing offset for locating variables
	sprintf(errorLine, " %lld", (HostAddress) oValue);
	logLine(errorLine);
#endif
      } else if (oType == DataPtr) {  // restore AstNode::DataPtr type
	sprintf(errorLine, " %lld", (HostAddress) oValue);
	logLine(errorLine);
      } else if (oType == DataValue) {  // restore AstNode::DataValue type
	sprintf(errorLine, " @%lld", (HostAddress) oValue);
	logLine(errorLine); 
      } else if ((oType == DataIndir) ||(oType == DataIndir64)) {
	logLine(" @[");
        loperand->print();
	logLine("]");
      } else if (oType == DataReg) {
	sprintf(errorLine," reg%lld ", (HostAddress)oValue);
        logLine(errorLine);
        loperand->print();
      } else if (oType == Param) {
	sprintf(errorLine, " param[%lld]", (HostAddress) oValue);
	logLine(errorLine);
      } else if (oType == ReturnVal) {
	sprintf(errorLine, "retVal");
	logLine(errorLine);
      } else if (oType == DataAddr)  {
	sprintf(errorLine, " [0x%llx]", (HostAddress)oValue);
	logLine(errorLine);
      } else if (oType == FrameAddr)  {
	sprintf(errorLine, " [$fp + %lld]", (HostAddress) oValue);
	logLine(errorLine);
      } else if (oType == EffectiveAddr)  {
	sprintf(errorLine, " <<effective address>>");
	logLine(errorLine);
      } else {
	logLine(" <Unknown Operand>");
      }
    } else if (type == opCodeNode) {
      std::ostrstream os(errorLine, 1024, std::ios::out);
      os << "(" << getOpString(op) << std::ends;
      logLine(errorLine);
      if (loperand) loperand->print();
      if (roperand) roperand->print();
      if (eoperand) eoperand->print();
      logLine(")\n");
    } else if (type == callNode) {
      std::ostrstream os(errorLine, 1024, std::ios::out);
      os << "(" << callee << std::ends;
      logLine(errorLine);
      for (unsigned u = 0; u < operands.size(); u++)
	operands[u]->print();
      logLine(")\n");
    } else if (type == labelNode) {
	sprintf(errorLine, "%s:\n", labelName.c_str());
	logLine(errorLine);
    } else if (type == gotoNode) {
	sprintf(errorLine, "goto %s\n", labelName.c_str());
	logLine(errorLine);
    } else if (type == sequenceNode) {
      if (loperand) loperand->print();
      logLine(",");
      if (roperand) roperand->print();
      logLine("\n");
    }
  }
}
#endif

// If a process is passed, the returned Ast code will also update the
// observed cost of the process according to the cost of the if-body when the
// if-body is executed.  However, it won't add the the update code if it's
// considered that the if-body code isn't significant.  We consider an
// if-body not worth updating if (cost-if-body < 5*cost-of-update).  proc ==
// NULL (the default argument) will mean to not add to the AST's the code
// which adds this observed cost update code.
AstNode *createIf(AstNode *expression, AstNode *action, process *proc) 
{
  if(proc != NULL) {
#ifndef BPATCH_LIBRARY
    // add code to the AST to update the global observed cost variable
    // we want to add the minimum cost of the body.  Observe the following 
    // example
    //    if(condA) { if(condB) { STMT-Z; } }
    // This will generate the following code:
    //    if(condA) { if(condB) { STMT-Z; <ADD-COSTOF:STMT-Z>; }
    //               <ADD-COSTOF:if(condB)>;  
    //              }   
    // the <ADD-COSTOF: > is what's returned by minCost, which is what
    // we want.
    // Each if statement will be constructed by createIf().    
    int costOfIfBody = action->minCost();
    HostAddress globalObsCostVar = 0; //proc->getObsCostLowAddrInApplicSpace();
    AstNode *globCostVar = new AstNode(AstNode::DataAddr, globalObsCostVar);
    AstNode *addCostConst = new AstNode(AstNode::Constant, 
					costOfIfBody);
    AstNode *addCode = new AstNode(plusOp, globCostVar, addCostConst);
    AstNode *updateCode = new AstNode(storeOp, globCostVar, addCode);
    int updateCost = updateCode->minCost();
    addCostConst->setOValue(costOfIfBody+updateCost);
    AstNode *newAction = new AstNode(action);
    // eg. if costUpdate=10 cycles, won't do update if bodyCost=40 cycles
    //                               will do update if bodyCost=60 cycles
    const int updateThreshFactor = 5;
    if(costOfIfBody > updateThreshFactor * updateCost)
      action = new AstNode(newAction, updateCode);
#endif
  }

  AstNode *t = new AstNode(ifOp, expression, action);

  return(t);
}

#ifndef BPATCH_LIBRARY

// Multithreaded case with shared memory sampling

#if defined(MT_THREAD)

AstNode *computeAddress(void *level, void *index, int type)
{
  int tSize;

  /* DYNINSTthreadTable[0][thr_self()] */
  AstNode* t0 = new AstNode(AstNode::DataReg, (void *)REG_MT);
  AstNode* t7 ;

  /* Now we compute the offset for the corresponding level. We assume */
  /* that the DYNINSTthreadTable is stored by rows - naim 4/18/97 */
  //  if ((int)level != 0) {
    tSize = sizeof(unsigned);

    // AstNode* t5 = new AstNode(AstNode::Constant, 
    // (void*) (MAX_NUMBER_OF_THREADS*((unsigned) level)*tSize)) ;
    AstNode* t5 = new AstNode(AstNode::OffsetConstant, 
			      (void*) (MAX_NUMBER_OF_THREADS*((unsigned) level)*tSize),
			      true,  // this IS level
			      (unsigned) level,
			      (unsigned) index) ;  // value of level

    /* Given the level and tid, we compute the position in the thread */
    /* table. */
    AstNode* t6 = new AstNode(plusOp, t0, t5);

    removeAst(t0);
    removeAst(t5);
    /* We then read the address, which is really the base address of the */
    /* vector of counters and timers in the shared memory segment. */
    t7 = new AstNode(AstNode::DataIndir, t6); 
    removeAst(t6);

    // remove 0 as a special case, so that we know the type "OffsetConstant"
    //  } else {
    /* if level is 0, we don't need to compute the offset */
    //    t7 = new AstNode(AstNode::DataIndir, t0);
    //    removeAst(t0);
    //  }
  return(t7);
}


AstNode *checkAddress(AstNode *addr, opCode op)
{  
  AstNode *null_value, *expression;
  null_value = new AstNode(AstNode::Constant,(void *)0);
  expression = new AstNode(op, addr, null_value);
  removeAst(null_value) ;
  return(expression);
}


AstNode *addIndexToAddress(AstNode *addr, void *level, void *index, int type)
{
  AstNode *t10, *t11;
  int tSize;

  /* Finally, using the index as an offset, we compute the address of the */
  /* counter/timer. */
  if (type==0) {
    /* intCounter */
    tSize = sizeof(intCounter);
  } else {
    /* Timer */
    tSize = sizeof(tTimer);
  }

  // t10 = new AstNode(AstNode::Constant, (void*) (((unsigned)index)*tSize)) ;
  t10 = new AstNode(AstNode::OffsetConstant,
		    (void*) (((unsigned)index)*tSize),
		    false,  // this is NOT level
		    (unsigned) level,
		    (unsigned) index);  // value of index
  t11 = new AstNode(plusOp, addr, t10); /* address of counter/timer */
  removeAst(t10) ;

  return(t11);
}


AstNode *computeTheAddress(void *level, void *index, int type) {
  AstNode* base = computeAddress(level, index, type) ;
  AstNode* addr = addIndexToAddress(base, level, index, type) ;
  removeAst(base) ;
  return addr ;
}

AstNode *createTimer(const pdstring &func, void *level, void *index,
                     pdvector<AstNode *> &ast_args)
{
  // t29: 
  // DYNINST_not_deleted
  pdvector<AstNode *> arg;
  AstNode* t29 = new AstNode("DYNINST_not_deleted", arg);

  //t18:
  // while (computeAddress() == 0) ;
  //
  pdvector<AstNode *> dummy ;
  AstNode* t30 = new AstNode("DYNINSTloop", dummy) ;
  // WHY IS THIS CONSTANT HERE? FIXME
#ifdef rs6000_ibm_aix4_1
  AstNode* end = new AstNode(AstNode::Constant, (void*) (3*sizeof(int))) ;
#else // sparc
  AstNode* end = new AstNode(AstNode::Constant, (void *)36);
#endif
  AstNode* t32 = new AstNode(branchOp, end) ;
  removeAst(end) ;
  AstNode* t31 = new AstNode(ifOp, t30, t32) ;
  removeAst(t30);
  removeAst(t32); 
  AstNode* t0 = computeAddress(level, index, 1); /* 1 means tTimer */
  AstNode* t2 = checkAddress(t0, eqOp);
  removeAst(t0); 
  AstNode* t18 = new AstNode(whileOp, t2, t31) ;
  removeAst(t31) ;
  removeAst(t2) ; 

  //t1:
  // Timer
  AstNode* t3 = addIndexToAddress(t0, level, index, 1); /* 1 means tTimer */
  ast_args += (t3);
  AstNode* t1 = new AstNode(func, ast_args);
  for(unsigned i=0; i<ast_args.size(); i++)
    removeAst(ast_args[i]) ;

  //t5:
  // {
  //   while(T/C==0) ;
  //   Timer
  // }
  AstNode* t5 = new AstNode(t18, t1);
  removeAst(t18) ;
  removeAst(t1) ;

  //if(t29) t5 ;
  AstNode* ret = new AstNode(ifOp, t29, t5) ;
  removeAst(t29);
  removeAst(t5) ;

  return(ret);
}

AstNode *createCounter(const pdstring &func, void *level, void *index, 
                       AstNode *ast) 
{ 
  AstNode *t0=NULL,*t1=NULL,*t2=NULL,*t3=NULL;
  AstNode *t4=NULL,*t5=NULL,*t6=NULL;

  t0 = computeAddress(level, index, 0); /* 0 means intCounter */
  t4 = checkAddress(t0, neOp);
  t5 = addIndexToAddress(t0, level, index, 0); /* 0 means intCounter */
  removeAst(t0) ;

  if (func == "addCounter") {
    t1 = new AstNode(AstNode::DataIndir, t5);
    t2 = new AstNode(plusOp, t1, ast);
    t3 = new AstNode(storeIndirOp, t5, t2);
    removeAst(t2) ;
  } else if (func == "subCounter") {
    t1 = new AstNode(AstNode::DataIndir, t5);
    t2 = new AstNode(minusOp, t1, ast);
    t3 = new AstNode(storeIndirOp, t5, t2);
    removeAst(t2);
  } else if (func == "setCounter") {
    t3 = new AstNode(storeIndirOp, t5, ast);
  }
  removeAst(t5) ;

  t6 = new AstNode(ifOp, t4, t3) ;
  removeAst(t4);
  removeAst(t3);

  return(t6);
}

#else

// Single threaded case

AstNode *createTimer(const pdstring &func, HostAddress dataPtr, 
                     pdvector<AstNode *> &ast_args)
{
  AstNode *t0=NULL,*t1=NULL;

  // t0 = new AstNode(AstNode::Constant, (void *) dataPtr);  // This was AstNode::DataPtr
  t0 = new AstNode(AstNode::DataPtr, dataPtr);  // restore AstNode::DataPtr
  ast_args += assignAst(t0);
  removeAst(t0);
  t1 = new AstNode(func, ast_args);
  for (unsigned i=0;i<ast_args.size();i++) removeAst(ast_args[i]);  
  return(t1);
}

AstNode *createCounter(const pdstring &func, HostAddress dataPtr, 
                       AstNode *ast) 
{
   AstNode *t0=NULL, *t1=NULL, *t2=NULL;

   t0 = new AstNode(AstNode::DataAddr, dataPtr);  // This was AstNode::DataValue
   // t0 = new AstNode(AstNode::DataValue, (void *)dataPtr);  // restore AstNode::DataValue
   if (func=="addCounter") {
     t1 = new AstNode(plusOp,t0,ast);
     t2 = new AstNode(storeOp,t0,t1);
     removeAst(t1);
   } else if (func=="subCounter") {
     t1 = new AstNode(minusOp,t0,ast);
     t2 = new AstNode(storeOp,t0,t1);
     removeAst(t1);
   } else if (func=="setCounter") {
     t2 = new AstNode(storeOp,t0,ast);
   } else abort();
   removeAst(t0);
   return(t2);
}

#endif
#endif

#ifdef BPATCH_LIBRARY
BPatch_type *AstNode::checkType()
{
    BPatch_type *ret = NULL;
    BPatch_type *lType = NULL, *rType = NULL, *eType = NULL;
    bool errorFlag = false;

    assert(BPatch::bpatch != NULL);	/* We'll use this later. */

    if ((loperand || roperand) && getType()) {
	// something has already set the type for us.
	// this is likely an expression for array access
	ret = const_cast<BPatch_type *>(getType());
	return ret;
    }

    if (loperand)
	lType = loperand->checkType();

    if (roperand)
	rType = roperand->checkType();

    if (eoperand)
	eType = eoperand->checkType();

    if (lType == BPatch::bpatch->type_Error ||
	rType == BPatch::bpatch->type_Error)
	errorFlag = true;

    
    switch (type) { // Type here is nodeType, not BPatch library type
	case sequenceNode:
	    ret = rType;
	    break;
	case opCodeNode:
	    if (op == ifOp) {
		// XXX No checking for now.  Should check that loperand
		// is boolean.
		ret = BPatch::bpatch->type_Untyped;
	    } else if (op == noOp) {
		ret = BPatch::bpatch->type_Untyped;
	    } else if (op == funcJumpOp) {
		ret = BPatch::bpatch->type_Untyped;
	    } else if (op == getAddrOp) {
		// Should set type to the infered type not just void * 
		//  - jkh 7/99
		ret = BPatch::bpatch->stdTypes->findType("void *");
		assert(ret != NULL);
		break;
	    } else {
		// XXX The following line must change to decide based on the
		// types and operation involved what the return type of the
		// expression will be.
		ret = lType;
		if (lType != NULL && rType != NULL) {
		    if (!lType->isCompatible(rType)) {
			errorFlag = true;
		    }
		}
	    }
	    break;
	case operandNode:
	    if (oType == DataIndir) {
		assert(roperand == NULL);
		// XXX Should really be pointer to lType -- jkh 7/23/99
		ret = BPatch::bpatch->type_Untyped;
	    } else {
		assert(loperand == NULL && roperand == NULL);
		if ((oType == Param) || (oType == ReturnVal)) {
		    // XXX Params and ReturnVals untyped for now
		    ret = BPatch::bpatch->type_Untyped; 
		} else {
		    ret = const_cast<BPatch_type *>(getType());
		}
		assert(ret != NULL);
	    }
	    break;
	case callNode:
            unsigned i;
	    for (i = 0; i < operands.size(); i++) {
		BPatch_type *operandType = operands[i]->checkType();
		/* XXX Check operands for compatibility */
		if (operandType == BPatch::bpatch->type_Error) {
		    errorFlag = true;
		}
	    }
	    /* XXX Should set to return type of function. */
	    ret = BPatch::bpatch->type_Untyped;
	    break;

      default:
	assert(0);
    }

    assert(ret != NULL);

    if (errorFlag && doTypeCheck) {
	ret = BPatch::bpatch->type_Error;
    } else if (errorFlag) {
	ret = BPatch::bpatch->type_Untyped;
    }

#if defined(ASTDEBUG)
    // it would be useful to have some indication of what the type applied to
    // (currently it appears to be copious amounts of contextless junk)
    if (ret) {
	logLine(" type is ");
	if (ret->getName()) 
	     logLine(ret->getName());
	else
	     logLine(" <NULL Name String>");
	logLine("\n");
    }
#endif

    // remember what type we are
    setType(ret);

    return ret;
}
#endif

bool AstNode::findFuncInAst(pdstring func) {
  if (type == callNode && callee == func) 
      return true ;

  if (loperand && loperand->findFuncInAst(func))
      return true ;

  if (roperand && roperand->findFuncInAst(func))
      return true ;

  for (unsigned i=0; i<operands.size(); i++)
    if (operands[i]->findFuncInAst(func))
      return true ;

  return false ;
} 

// The following two functions are broken because some callNodes have
// a function_base* member (`calleefunc') that is not replaced by
// these functions.  For such Callnodes, the corresponding
// function_base* for func2 must be provided.  Neither of these
// functions is called these days.

// Looks for function func1 in ast and replaces it with function func2
void AstNode::replaceFuncInAst(function_base *func1, function_base *func2)
{
  if (type == callNode) {
       if (calleefunc) {
	    if (calleefunc == func1) {
		 callee = func2->prettyName();
		 calleefunc = func2;
	    }
       } else if (callee == func1->prettyName()) {
	    // Not all call nodes are initialized with function_bases.
	    // Preserve that, continue to deal in names only.
	    if (callee == func1->prettyName())
		 callee = func2->prettyName();
       }
  }
  if (loperand) loperand->replaceFuncInAst(func1,func2);
  if (roperand) roperand->replaceFuncInAst(func1,func2);
  for (unsigned i=0; i<operands.size(); i++)
    operands[i]->replaceFuncInAst(func1, func2) ;
} 

void AstNode::replaceFuncInAst(function_base *func1, function_base *func2,
			       pdvector<AstNode *> &more_args, int index)
{
  unsigned i ;
  if (type == callNode) {
      bool replargs = false;
      if (calleefunc) {
	   if (calleefunc == func1 || calleefunc == func2) {
		replargs = true;
		calleefunc = func2;
		callee = func2->prettyName();
	   }
      } else if (callee == func1->prettyName() || callee == func2->prettyName()) {
	   replargs = true;
	   callee = func2->prettyName();
      }
      if (replargs) {
	  unsigned int j = 0 ;
	  for (i=index; i< operands.size() && j <more_args.size(); i++){
	    removeAst(operands[i]) ;
	    operands[i] = assignAst(more_args[j++]) ;
	  }
	  while (j<more_args.size()) {
	    operands += assignAst(more_args[j++]); // .push_back(assignAst(more_args[j++]));
	  }
      }
  }
  if (loperand) loperand->replaceFuncInAst(func1, func2, more_args, index) ;
  if (roperand) roperand->replaceFuncInAst(func1, func2, more_args, index) ;
  for (i=0; i<operands.size(); i++)
    operands[i]->replaceFuncInAst(func1, func2, more_args, index) ;
}


// This is not the most efficient way to traverse a DAG
bool AstNode::accessesParam(void)
{
  bool ret = false;

  ret = (type == operandNode && oType == Param);

  if (!ret && loperand) 
       ret = loperand->accessesParam();
  if (!ret && roperand) 
      ret = roperand->accessesParam();
  if (!ret && eoperand) 
      ret = eoperand->accessesParam();
  for (unsigned i=0;i<operands.size(); i++) {
      if (!ret)
        ret = operands[i]->accessesParam() ;
      else
        break;
  }

  return ret;
}

// Estimate register usage of the node via the Sethi-Ullman algorithm
unsigned AstNode::computeRegNeedEstimate()
{
    unsigned lNeed = 0, rNeed = 0;
    if (loperand) {
	lNeed = loperand->computeRegNeedEstimate();
    }
    if (roperand) {
	rNeed = roperand->computeRegNeedEstimate();
    }
    if (lNeed == rNeed) {
	regNeedEstimate = lNeed + 1;
    }
    else if (lNeed > rNeed) {
	regNeedEstimate = lNeed;
    }
    else {
	regNeedEstimate = rNeed;
    }
    return regNeedEstimate;
}

void AstNode::keepRegister(Register r, pdvector<AstNode*> path)
{
    kept_register = r;
    kept_path = path;
}

void AstNode::unkeepRegister()
{
    assert(kept_register != Null_Register);
    assert(useCount == 0); // Use force-unkeep otherwise
    kept_register = Null_Register;
}

// Do not keep the register and force-free it
void AstNode::forceUnkeepAndFree(registerSpace *rs)
{
    assert(kept_register != Null_Register);
    rs->forceFreeRegister(kept_register);
    kept_register = Null_Register;
}

// Our children may have incorrect useCounts (most likely they 
// assume that we will not bother them again, which is wrong)
void AstNode::fixChildrenCounts(registerSpace *rs)
{
    pdvector<AstNode*> children;
    getChildren(&children);
    for (unsigned i=0; i<children.size(); i++) {
	children[i]->setUseCount(rs);
    }
}

// Check to see if the value had been computed earlier
bool AstNode::hasKeptRegister() const
{
    return (kept_register != Null_Register);
}

// Occasionally, we do not call .generateCode_phase2 for the referenced node, 
// but generate code by hand. This routine decrements its use count properly
void AstNode::decUseCount(registerSpace *rs)
{
    useCount--;
    assert(useCount >= 0);

    if (hasKeptRegister()) { // kept_register still thinks it will be used
	rs->freeRegister(kept_register);
	if (useCount == 0) {
	    unkeepRegister();
	}
    }
}

// Check if the node can be kept at all. Some nodes (e.g., storeOp)
// can not be cached. In fact, there are fewer nodes that can be cached.
bool AstNode::canBeKept() const
{
    if (type == opCodeNode) {
	if (op == branchOp || op == ifOp || op == whileOp || op == doOp ||
	    op == storeOp || op == storeIndirOp || op == trampTrailer ||
	    op == trampPreamble || op == funcJumpOp) {
	    return false;
	}
    }
    else if (type == operandNode) {
	// All operands seem to be cacheable
    }
    else if (type == sequenceNode) {
	return false;
    }
    else {
	assert(false && "AstNode::canBeKept: Unknown node type");
	return false;
    }
    return true;
}

// Bump up the useCount. We use it to prevent freeing the kept
// register when it is unsafe.
void AstNode::incUseCount(registerSpace *rs)
{
    useCount++;

    if (hasKeptRegister()) {
	rs->incRefCount(kept_register);
    }
}

// Check to see if path1 is a subpath of path2
bool AstNode::subpath(const pdvector<AstNode*> &path1, 
		      const pdvector<AstNode*> &path2) const
{
    if (path1.size() > path2.size()) {
	// cout << "subpath returning false, since path1.size > path2.size\n";
	return false;
    }
    for (unsigned i=0; i<path1.size(); i++) {
	if (path1[i] != path2[i]) {
	    // cout << "subpath returning false, since path1[i] != path2[i]\n";
	    return false;
	}
    }
    // cout << "subpath returning true\n";
    return true;
}

// Return all children of this node ([lre]operand, ..., operands[])
void AstNode::getChildren(pdvector<AstNode*> *children) const
{
    if (loperand) {
	children->push_back(loperand);
    }
    if (roperand) {
	children->push_back(roperand);
    }
    if (eoperand) {
	children->push_back(eoperand);
    }
    for (unsigned i=0; i<operands.size(); i++) {
	children->push_back(operands[i]);
    }
}

// If "goal" can be reached from this node, invalidate our
// kept_register and return true. Otherwise, return false.
bool AstNode::invalidateAncestorsOf(AstNode *goal, registerSpace *rs)
{
    bool needsInvalidate = false;

    if (this == goal) {
	// We guarantee that this node will be recomputed anyway
	return false;
    }
    
    pdvector<AstNode*> children;
    getChildren(&children);

    // If "goal" is our child, we have to invalidate this node
    for (unsigned i=0; i<children.size(); i++) {
	needsInvalidate |= (children[i] == goal);
    }

    // Descend into this child's subtree recursively
    for (unsigned i=0; i<children.size(); i++) {
	needsInvalidate |= children[i]->invalidateAncestorsOf(goal, rs);
    }

    // Perform the invalidation itself
    if (needsInvalidate && hasKeptRegister()) {
	forceUnkeepAndFree(rs);
	fixChildrenCounts(rs);
    }

    return needsInvalidate;
}

// Return all live nodes in the subtree
void AstNode::getAllLive(pdvector<AstNode*> *live)
{
    pdvector<AstNode*> children;
    getChildren(&children);
    
    // Descend into the subtrees recursively
    for (unsigned i=0; i<children.size(); i++) {
	children[i]->getAllLive(live);
    }
    
    // Check if the node keeps a live value and push it onto the list
    if (useCount > 0 && hasKeptRegister()) {
	// Ugly
	for (unsigned i=0; i<live->size(); i++) {
	    if ((*live)[i] == this) {
		return; // No duplicates please
	    }
	}
	live->push_back(this);
    }
}

// Set the no-save-restore flag in our subtree
void AstNode::setNoSaveRestoreFlag(bool value)
{
    pdvector<AstNode*> children;
    getChildren(&children);
    for (unsigned i=0; i<children.size(); i++) {
	children[i]->setNoSaveRestoreFlag(value);
    }
    noSaveRestore = value;
}

// Get the need-save-restore flag in our subtree
bool AstNode::getNeedSaveRestoreFlag() 
{
    pdvector<AstNode*> children;
    getChildren(&children);
    for (unsigned i=0; i<children.size(); i++) {
       if(children[i]->getNeedSaveRestoreFlag())
          needSaveRestore = true;
    }
    return needSaveRestore;
}

bool AstNode::containsCall() const
{
    bool rv = (type == callNode);

    pdvector<AstNode*> children;
    getChildren(&children);
    for (unsigned i=0; i<children.size(); i++) {
	rv |= children[i]->containsCall();
    }
    return rv;
}

#ifdef i386_unknown_linux2_4
bool AstNode::is64BitCode() const
{
    bool rv = ((type == operandNode) && (oType == DataIndir64));

    pdvector<AstNode*> children;
    getChildren(&children);
    for (unsigned i=0; i<children.size(); i++) {
	rv |= children[i]->is64BitCode();
    }
    return rv;
}
#endif

} // namespace
