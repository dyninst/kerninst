#ifndef _EMIT_H_
#define _EMIT_H_

#include "common/h/String.h"
#include "ast.h"
#include "kludges.h"

// We put everything in a namespace to enable tools that use both
// Kerninst and Dyninst API
namespace kcodegen {

// If [raddr] == src, exchange the contents of dest and [raddr]
// Otherwise, reload src with [raddr] and branch to the restart label
void emitAtomicExchange(Register raddr, Register src, Register dest, 
                        Register temp, //This param only useful for Power
                        const pdstring &restart_label, char *insn);

// Emit a label (destination of a branch)
void emitLabel(const pdstring &name, char *insn);

// for operations requiring an HostAddress to be returned
// (e.g., ifOp/branchOp, trampPreambleOp/trampTrailerOp)
HostAddress emitA(opCode op, Register src1, Register src2, Register dst, 
		  char *insn, HostAddress &base, bool noCost);

// getRetValOp, getParamOp, getSysRetValOp, getSysParamOp
void emitR(opCode op, Register dst, Register scratch,
	   HostAddress entryAddr, HostAddress spliceAddr,
	   char *insn, HostAddress &base, bool noCost);

// for general arithmetic and logic operations which return nothing
void emitV(opCode op, Register src1, Register src2, Register dst, 
	   char *insn, HostAddress &base, bool noCost, int size = 4);

// for loadOp and loadConstOp (reading from an HostAddress)
void emitVload(opCode op, HostAddress src1, Register src2, Register dst, 
	       char *insn, HostAddress &base, bool noCost, int size = 4);

// for storeOp (writing to an HostAddress)
void emitVstore(opCode op, Register src1, Register src2, HostAddress dst, 
                char *insn, HostAddress &base, bool noCost, int size = 4);

void emitLoadPreviousStackFrameRegister(Register srcPrevFrame,
					Register destThisFrame,
					char *insn,
					HostAddress &/*base*/,
					int /*size*/,
					bool /*noCost*/);

// for updateCostOp
void emitVupdate(opCode op, RegContents src1, Register src2, HostAddress dst, 
		 char *insn, HostAddress &base, bool noCost);

// and the retyped original emitImm companion
void emitImm(opCode op, Register src, RegContents src2imm, Register dst, 
	     char *insn, HostAddress &base, bool noCost);

#ifdef BPATCH_LIBRARY
void emitASload(BPatch_addrSpec_NP as, Register dest, char* baseInsn,
		HostAddress &base, bool noCost);

#define emitCSload emitASload
#endif

// VG(11/06/01): moved here and added location
void emitFuncCall(opCode op, registerSpace *rs, char *insn, 
		  HostAddress &base,
		  HostAddress calleeAddr,
		  const pdvector<AstNode *> &operands,
		  process *proc, bool noCost, 
		  Register dest,
		  const pdvector<AstNode *> &ifForks, // control-flow path
		  const instPoint *location = NULL);

int getInsnCost(opCode t);

void emitCondBranch(opCode condition, Register src, pdstring lbl, char *insn);
void emitJump(pdstring label, char *insn);

// Generate code to return from a function
void emitRet(char *insn);
} // namespace
#endif
