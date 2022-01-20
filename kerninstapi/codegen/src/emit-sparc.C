#include "emit.h"
#include "tempBufferEmitter.h"
#include "abi.h"
#include "machineInfo.h"
#include "sparc_instr.h"
#include "moduleMgr.h"
#include "traceBuffer.h"
#include "kapi.h"

// some macros for helping code which contains register symbolic names
#define REG_I(x) (x + 24)
#define REG_L(x) (x + 16) 
#define REG_O(x) (x + 8)
#define REG_G(x) (x)

extern bool usesTick(kapi_hwcounter_kind kind);
extern bool usesPic(kapi_hwcounter_kind kind, int num);
extern moduleMgr *global_moduleMgr;
extern machineInfo *theGlobalKerninstdMachineInfo;

// We put everything in a namespace to enable tools that use both
// Kerninst and Dyninst API
namespace kcodegen {

// If [raddr] == src, exchange the contents of dest and [raddr]
// Otherwise, reload src with [raddr] and branch to the restart label
void emitAtomicExchange(Register raddr, Register src, Register dest,
		        Register /*only used by Power*/, 
			const pdstring &restart_label, char *insn)
			
{
//      cout << "emitAtomicExchange([Reg" << raddr << "], Reg" << src  
//  	 << ", Reg" << dest << ", " << restart_label << ")\n";

    tempBufferEmitter *theEmitter = (tempBufferEmitter *)insn;
    assert(theEmitter != 0);
    sparc_reg addr_reg(sparc_reg::rawIntReg, raddr);
    sparc_reg src_reg(sparc_reg::rawIntReg, src);
    sparc_reg dst_reg(sparc_reg::rawIntReg, dest);

    instr_t *i = new sparc_instr(sparc_instr::cas, true, // extended
				 addr_reg, src_reg, dst_reg);
    theEmitter->emit(i);
    i = new sparc_instr(sparc_instr::sub, src_reg, dst_reg, 
			src_reg); // can't overwrite dst_reg
    theEmitter->emit(i);

    i = new sparc_instr(sparc_instr::bpr,
			sparc_instr::regNotZero,
			false, // not annulled
			false, // predict not taken
			src_reg,
			0); // displacement for now
    theEmitter->emitCondBranchToLabel(i, restart_label);
    i = new sparc_instr(sparc_instr::mov, dst_reg, src_reg);
    theEmitter->emit(i);
}

void emitLabel(const pdstring& name, char *insn)
{
//      cout << "emitLabel(" << name << ")\n";

    tempBufferEmitter *theEmitter = (tempBufferEmitter *)insn;
    assert(theEmitter != 0);
    theEmitter->emitLabel(name);
}

void emitCondBranch(opCode condition, Register src, pdstring label, char *insn)
{
//      cout << "emitCondBranch(" << "Reg" << src << ", " << label << ")\n";
    tempBufferEmitter *em = (tempBufferEmitter *)insn;
    assert(em != 0);
    sparc_reg src_reg(sparc_reg::rawIntReg, src);
    sparc_instr::RegCondCodes cc1;

    switch(condition) {
    case eqOp:
	cc1 = sparc_instr::regZero;
	break;
    case neOp:
	cc1 = sparc_instr::regNotZero;
	break;
    default:
	assert(false && "Not yet implemented");
	break;
    }
    instr_t *i = new sparc_instr(sparc_instr::bpr,
				 cc1,
				 false, // not annulled
				 false, // predict not taken
				 src_reg,
				 0); // displacement for now
    em->emitCondBranchToLabel(i, label);
    i = new sparc_instr(sparc_instr::nop);
    em->emit(i);
}

void emitJump(pdstring label, char *insn)
{
//      cout << "emitJump(" << label << ")\n";
    tempBufferEmitter *em = (tempBufferEmitter *)insn;
    assert(em != 0);
    instr_t *i = new sparc_instr(sparc_instr::bpcc, 
				 sparc_instr::iccAlways,
				 true, true, true, // ba,a,pt,xcc
				 0); // displacement for now
    em->emitCondBranchToLabel(i, label);
}

// Emit code to compute a boolean expression (which puts 0 or 1 into dst)
// We can do this with no branching, thanks to conditional moves
// Here is how to convert (src >= imm) into (0 or 1):
//   sub src,imm,dst          // dst = src - imm
//   movr(r>=0) dst, 1, dst   // if (dst>=0) dst = 1
//   movr(r<0)  dst, 0, dst   // if (dst<0) dst = 0
static void genImmRelOp(opCode op, sparc_reg src_reg, RegContents src2imm, 
			sparc_reg dst_reg, tempBufferEmitter *theEmitter)
{
    const int nKeys = 6;
    struct {
	int key;
	sparc_instr::RegCondCodes cc1;
	int val1;
	bool needsFlip;  // dst contains 0 instead of 1, flip it with xor
	bool skipSecond; // second movr is not always needed (e.g., for neOp)
    } keyval[nKeys] = {{greaterOp, sparc_instr::regGrZero, 1, false, false},
		       {geOp, sparc_instr::regGrOrEqZero, 1, false, false},
		       {lessOp, sparc_instr::regGrOrEqZero, 0, false, false},
		       {leOp, sparc_instr::regGrZero, 1, true, false},
		       {neOp, sparc_instr::regNotZero, 1, false, true},
		       {eqOp, sparc_instr::regNotZero, 1, true, true}};
    
    instr_t *i = new sparc_instr(sparc_instr::sub, src_reg, src2imm,
				 dst_reg); // dest
    theEmitter->emit(i);

    int idx;
    for (idx=0; idx<nKeys && op != keyval[idx].key; idx++);
    assert(idx != nKeys);

    sparc_instr::RegCondCodes cc2 = 
	sparc_instr::reversedRegCondCodes[keyval[idx].cc1];
    int val2 = 1 - keyval[idx].val1;

    i = new sparc_instr(sparc_instr::movr, keyval[idx].cc1,
			dst_reg,   // look at this register
			keyval[idx].val1, // move val1 into dest 
			dst_reg); // if condition matches
    theEmitter->emit(i);
    if (!keyval[idx].skipSecond) {
        i = new sparc_instr(sparc_instr::movr, cc2,
			    dst_reg,   // look at this register
			    val2,      // move val2 into dest 
			    dst_reg); // if condition matches
	theEmitter->emit(i);
    }
    if (keyval[idx].needsFlip) {
        i = new sparc_instr(sparc_instr::logic, 
			    sparc_instr::logic_xor, false, // no cc
			    dst_reg, // dest
			    dst_reg, // src
			    1);
	theEmitter->emit(i);
    }	
}

// Emit code to compute a boolean expression, given two source registers
// The idea is the same as in genImmRelOp above, but the implementation
// is slightly different -- saves us one instruction sometimes
static void genRelOp(opCode op, sparc_reg src1_reg, sparc_reg src2_reg, 
		     sparc_reg dst_reg, tempBufferEmitter *theEmitter)
{
    const int nKeys = 4;
    struct {
	int key;
	sparc_instr::RegCondCodes cc1;
	bool needsFlip;  // dst contains 0 instead of 1, flip it with xor
	bool skipSecond; // second movr is not always needed (e.g., for neOp)
    } keyval[nKeys] = {{greaterOp, sparc_instr::regGrZero, false, false},
		       {geOp, sparc_instr::regGrOrEqZero, false, false},
		       {neOp, sparc_instr::regNotZero, false, true},
		       {eqOp, sparc_instr::regNotZero, true, true}};

    if (op == leOp) {
	genRelOp(geOp, src2_reg, src1_reg, dst_reg, theEmitter);
	return;
    }
    else if (op == lessOp) {
	genRelOp(greaterOp, src2_reg, src1_reg, dst_reg, theEmitter);
	return;
    }
    instr_t *i = new sparc_instr(sparc_instr::sub, src1_reg, src2_reg,
				 dst_reg); // dest
    theEmitter->emit(i);

    int idx;
    for (idx=0; idx<nKeys && op != keyval[idx].key; idx++);
    assert(idx != nKeys);

    sparc_instr::RegCondCodes cc2 = 
	sparc_instr::reversedRegCondCodes[keyval[idx].cc1];

    i = new sparc_instr(sparc_instr::movr, keyval[idx].cc1,
			dst_reg,   // look at this register
			1,         // move 1 into dest 
			dst_reg);  // if condition matches
    theEmitter->emit(i);
    if (!keyval[idx].skipSecond) {
        i = new sparc_instr(sparc_instr::movr, cc2,
			    dst_reg,   // look at this register
			    0,         // move 0 into dest 
			    dst_reg);  // if condition matches
	theEmitter->emit(i);
    }
    if (keyval[idx].needsFlip) {
        i = new sparc_instr(sparc_instr::logic, 
			    sparc_instr::logic_xor, false, // no cc
			    dst_reg, // dest
			    dst_reg, // src
			    1);
	theEmitter->emit(i);
    }	
}

// for general arithmetic and logic operations which return nothing
void emitV(opCode op, Register src1, Register src2, Register dst, 
	   char *insn, HostAddress &base, bool noCost, int size)
{
//      cout << "emitV(" << op << ", Reg" << src1 << ", Reg" << src2 
//  	 << ", Reg" << dst << ", 0x" << hex << base << dec << ")\n";

    tempBufferEmitter *theEmitter = (tempBufferEmitter *)insn;
    assert(theEmitter != 0);
    
    instr_t *i;

    // Instructions with no registers
    if (op == noOp) {
        i = new sparc_instr(sparc_instr::nop);
        theEmitter->emit(i);
	return;
    }

    // Instructions with one source register
    sparc_reg src1_reg(sparc_reg::rawIntReg, src1);
    sparc_reg dst_reg(sparc_reg::rawIntReg, dst);
    if (op == movOp) {
        i = new sparc_instr(sparc_instr::mov, src1_reg, dst_reg);
	theEmitter->emit(i);
	return;
    }

    // Instructions with two source registers
    sparc_reg src2_reg(sparc_reg::rawIntReg, src2);
    if (op == plusOp || op == minusOp || op == timesOp || op == divOp ||
	op == shlOp || op == shrOp) {
	switch(op) {
	case plusOp:
	    i = new sparc_instr(sparc_instr::add, src1_reg, src2_reg,
				dst_reg); // dest
	    theEmitter->emit(i);
	    break;
	case minusOp:
	    i = new sparc_instr(sparc_instr::sub, src1_reg, src2_reg,
				dst_reg); // dest
	    theEmitter->emit(i);
	    break;
	case timesOp:
	    i = new sparc_instr(sparc_instr::mulx, src1_reg, src2_reg,
				dst_reg); // dest
	    theEmitter->emit(i);
	    break;
	case divOp:
	    i = new sparc_instr(sparc_instr::sdivx, src1_reg,src2_reg,
				dst_reg); // dest
	    theEmitter->emit(i);
	    break;
	case shlOp:
	    i = new sparc_instr(sparc_instr::shift, 
				sparc_instr::leftLogical,
				true, // extended
				dst_reg,
				src1_reg, src2_reg);
	    theEmitter->emit(i);
	    break;
	case shrOp:
	    i = new sparc_instr(sparc_instr::shift, 
				sparc_instr::rightLogical,
				true, // extended
				dst_reg,
				src1_reg, src2_reg);
	    theEmitter->emit(i);
	    break;
	default:
	    assert(false && "Internal error");
	}
    }
    else if (op == andOp || op == orOp) {
	sparc_instr::logical_types lopr;
	switch(op) {
	case andOp:
	    lopr = sparc_instr::logic_and;
	    break;
	case orOp:
	    lopr = sparc_instr::logic_or;
	    break;
	default:
	    assert(false && "Unknown opcode in emitV");
	}
	i = new sparc_instr(sparc_instr::logic, 
			    lopr, false, // no cc
			    dst_reg, // dest
			    src1_reg, src2_reg); // src's
	theEmitter->emit(i);
    }
    else if (op == greaterOp || op == geOp || op == lessOp || 
	     op == leOp || op == neOp || op == eqOp) {
	genRelOp(op, src1_reg, src2_reg, dst_reg, theEmitter);
    }
    else if (op == loadIndirOp) {
	emitVload(op, 0, src1, dst, insn, base, noCost, size);
    }
    else if (op == storeIndirOp) {
	emitVstore(op, src1, dst, 0, insn, base, noCost, size);
    }
    else {
	assert(false && "Unknown opcode in emitV");
    }
}

// for loadOp and loadConstOp (reading from an HostAddress)
void emitVload(opCode op, HostAddress src1, Register src2, Register dst, 
	       char *insn, HostAddress&/*base*/, bool /*noCost*/, int /*size*/)
{
     //cout << "emitVload(" << op << ", 0x" << std::ios::hex << src1 << std::ios::dec << ", Reg" 
  //	 << src2 << ", Reg" << dst <<  ")\n";

    tempBufferEmitter *theEmitter = (tempBufferEmitter *)insn;
    assert(theEmitter != 0);
    sparc_reg dst_reg(sparc_reg::rawIntReg, dst);
    
    instr_t *i;
    switch(op) {
    case loadConstOp:
	// sparc: may assert is src1 exceeds 44 bits
	theEmitter->emitImmAddr(src1, dst_reg);
	break;
    case loadOp:
	theEmitter->emitImmAddr(src1, dst_reg);
	i = new sparc_instr(sparc_instr::load, 
			    sparc_instr::ldExtendedWord,
			    dst_reg, 0, dst_reg);
	theEmitter->emit(i);
	break;
    case loadAndSetAddrOp: {
	sparc_reg addr_reg(sparc_reg::rawIntReg, src2);
	theEmitter->emitImmAddr(src1, addr_reg);
	i = new sparc_instr(sparc_instr::load, 
			    sparc_instr::ldExtendedWord,
			    addr_reg, 0, dst_reg);
	theEmitter->emit(i);
	break;
    }
    case loadIndirOp: {
	sparc_reg addr_reg(sparc_reg::rawIntReg, src2);
	i = new sparc_instr(sparc_instr::load, 
			    sparc_instr::ldExtendedWord,
			    addr_reg, 0, dst_reg);
	theEmitter->emit(i);
	break;
    }
    case loadCPUId: {
	assert(theGlobalKerninstdMachineInfo != 0);
	unsigned t_cpu_offset = 
	    theGlobalKerninstdMachineInfo->getKernelThreadCpuOffset();
	// Load the cpu id into dst_reg
	theEmitter->emitLoadKernelNative(sparc_reg::g7, 
					 simmediate<13>(t_cpu_offset), 
					 dst_reg);
	i = new sparc_instr(sparc_instr::load, 
			    sparc_instr::ldUnsignedWord,
			    dst_reg, 0, 
			    dst_reg); // dest
	theEmitter->emit(i);
	break;
    }
    case loadProcessId: {
	// Why is it in traceBuffer? Simply because it also uses it.
	// Maybe we should move the code someplace else
	traceBuffer::generateGetCurrPidx(*theEmitter, false, dst_reg);
	break;
    }
    case loadHwCtrReg: {
	kapi_hwcounter_kind kind = (kapi_hwcounter_kind)src1;
	if (usesTick(kind)) {
	    i = new sparc_instr(sparc_instr::readstatereg,
				4, // 4 --> %tick
				dst_reg);
	    theEmitter->emit(i);
	}
	else if (usesPic(kind, 0)) {
	    i = new sparc_instr(sparc_instr::readstatereg,
				17, // 17 --> %pic
				dst_reg);
	    theEmitter->emit(i);
	    // Zero high 32 bits by issuing a non-extended shift right 
	    // by 0 bits
	    i = new sparc_instr(sparc_instr::shift, 
				sparc_instr::rightLogical,
				false, // not extended
				dst_reg,
				dst_reg, 0);
	    theEmitter->emit(i);
	}
	else if (usesPic(kind, 1)) {
	    i = new sparc_instr(sparc_instr::readstatereg,
				17, // 17 --> %pic
				dst_reg);
	    theEmitter->emit(i);
	    i = new sparc_instr(sparc_instr::shift, 
				sparc_instr::rightLogical,
				true, // extended
				dst_reg,
				dst_reg, 32);
	    theEmitter->emit(i);
	}
	else {
	    assert(false && "Invalid event counter");
	}
	break;
    }
    case getPIL:
	// Read the current interrupt level
        i = new sparc_instr(sparc_instr::readprivreg, 0x8 /*PIL*/, 
			    dst_reg);
	theEmitter->emit(i);
	break;
    case loadFrameAddr:
    case loadFrameRelativeOp:
	assert(false && "Not yet implemented");
    default:
	assert(false && "Unknown opcode in emitVload");
    }
}

// for storeOp (writing to a HostAddress)
void emitVstore(opCode op, Register val, Register scratch, 
		HostAddress dst, char *insn,
		HostAddress &/*base*/, bool /*noCost*/, int /*size*/)
{
//      cout << "emitVstore(" << op << ", Reg" << val << ", Reg" << scratch
//  	 << hex << ", 0x" << dst << ", base = 0x" << base << dec << ")\n";
    tempBufferEmitter *theEmitter = (tempBufferEmitter *)insn;
    assert(theEmitter != 0);
    sparc_reg val_reg(sparc_reg::rawIntReg, val);

    instr_t *i;
    switch(op) {
    case storeOp: {
	sparc_reg addr_reg(sparc_reg::rawIntReg, scratch);
	theEmitter->emitImmAddr(dst, addr_reg);
	i = new sparc_instr(sparc_instr::store, 
			    sparc_instr::stExtended,
			    val_reg, addr_reg, 0);
	theEmitter->emit(i);
	break;
    }
    case storeIndirOp: {
	sparc_reg addr_reg(sparc_reg::rawIntReg, scratch);
	i = new sparc_instr(sparc_instr::store, 
			    sparc_instr::stExtended,
			    val_reg, addr_reg, 0);
	theEmitter->emit(i);
	break;
    }
    case setPIL:
	// Set the current interrupt level to val
	i = new sparc_instr(sparc_instr::writeprivreg,
			    val_reg, 0x8 /*PIL*/);
        theEmitter->emit(i);
	break;
    default:
	assert(false && "Unknown opcode in emitVstore");
    }
}

// and the retyped original emitImm companion
void emitImm(opCode op, Register src, RegContents src2imm, Register dst, 
	     char *insn, HostAddress &/*base*/, bool /*noCost*/)
{
//      cout << "emitImm(" << op << ", Reg" << src << ", " 
//  	 << src2imm << ", Reg" << dst << ", " << hex << base << dec << ")\n";
    tempBufferEmitter *theEmitter = (tempBufferEmitter *)insn;
    assert(theEmitter != 0);
    sparc_reg src_reg(sparc_reg::rawIntReg, src);
    sparc_reg dst_reg(sparc_reg::rawIntReg, dst);

    instr_t *i;
    switch (op) {
    case plusOp:
        i = new sparc_instr(sparc_instr::add, src_reg, src2imm,
			    dst_reg); // dest
	theEmitter->emit(i);
	break;
    case minusOp:
        i = new sparc_instr(sparc_instr::sub, src_reg, src2imm,
			    dst_reg); // dest
	theEmitter->emit(i);
	break;
    case timesOp:
        i = new sparc_instr(sparc_instr::mulx, src_reg, src2imm,
			    dst_reg); // dest
	theEmitter->emit(i);
	break;
    case divOp:
        i = new sparc_instr(sparc_instr::sdivx, src_reg, src2imm,
			    dst_reg); // dest
	theEmitter->emit(i);
	break;
    case orOp:
        i = new sparc_instr(sparc_instr::logic, 
			    sparc_instr::logic_or, false, // no cc
			    dst_reg, // dest
			    src_reg, src2imm); // src's
	theEmitter->emit(i);
	break;
    case andOp:
        i = new sparc_instr(sparc_instr::logic, 
			    sparc_instr::logic_and, false, // no cc
			    dst_reg, // dest
			    src_reg, src2imm); // src's
	theEmitter->emit(i);
	break;
    case shlOp:
        i = new sparc_instr(sparc_instr::shift, 
			    sparc_instr::leftLogical,
			    true, // extended
			    dst_reg,
			    src_reg, src2imm);
	theEmitter->emit(i);
	break;
    case shrOp:
        i = new sparc_instr(sparc_instr::shift, 
			    sparc_instr::rightLogical,
			    true, // extended
			    dst_reg,
			    src_reg, src2imm);
	theEmitter->emit(i);
	break;
    case greaterOp:
    case geOp:
    case lessOp:
    case leOp:
    case neOp:
    case eqOp:
	genImmRelOp(op, src_reg, src2imm, dst_reg, theEmitter);
	break;
    default:
	assert(false && "Unknown opcode in emitImm");
    }
}

void emitFuncCall(opCode op, registerSpace *rs, char *insn, 
		  HostAddress &base,
		  HostAddress calleeAddr,
		  const pdvector<AstNode *> &operands,
		  process *proc, bool noCost, 
		  Register dest,
		  const pdvector<AstNode *> &ifForks, // control-flow path
		  const instPoint *location)
{
    assert(op == callOp);

    pdvector<Register> argRegs;
    pdvector<AstNode *>::const_iterator iarg = operands.begin();
    for (; iarg != operands.end(); iarg++) {
	AstNode *arg = *iarg;
	Register reg = arg->generateCode_phase2(proc, rs, insn, base,
						noCost, ifForks, location);
	argRegs.push_back(reg);
    }

    // Save G's, O's and Condition Codes. Notice that we do this after
    // all args are generated. It allows us to ease the move of argRegs
    // into O's
    tempBufferEmitter *em = (tempBufferEmitter *)insn;
    const abi &theKernelABI = em->getABI();

    // Store g1-g7, o0-o7 in one shot. Notice that we assume that O's come
    // right after G's. TODO: we can be much smarter and do not save neither
    // free registers nor registers that are not modified by the callee
    const int stackBias = theKernelABI.getStackBias();
    int regOffset = stackBias - 8; // first reg goes at [%fp - 8]
    sparc_reg_set regsToSave = sparc_reg_set::allGs + sparc_reg_set::allOs - 
	sparc_reg::g0;

    instr_t *i;
    
    for (sparc_reg_set::const_iterator ireg = regsToSave.begin(); 
	 ireg != regsToSave.end(); ireg++) {
	i = new sparc_instr(sparc_instr::store, sparc_instr::stExtended,
			    (sparc_reg&)*ireg, sparc_reg::fp, regOffset);
        em->emit(i);
	regOffset -= 8;
    }
    // Read condition codes and put them on the stack
    i = new sparc_instr(sparc_instr::readstatereg, 0x2, sparc_reg::o7);
    em->emit(i);
    i = new sparc_instr(sparc_instr::store, sparc_instr::stExtended,
                        sparc_reg::o7,
			sparc_reg::fp, regOffset);
    em->emit(i);

    // Move argRegs into O's
    unsigned numArgs = argRegs.size();
    for (unsigned j=0; j<numArgs; j++) {
	Register reg = argRegs[j];
	sparc_reg dst_reg(sparc_reg::rawIntReg, REG_O(j));
	sparc_reg src_reg(sparc_reg::rawIntReg, reg);
	if (src_reg != dst_reg) {
	    // Needs move
	    unsigned num;
	    if (src_reg.is_oReg(num) && num < j) {
		// Can't mov: src_reg is itself one of the args and
		// have been overwritten by this time. Load the correct
		// value from memory. It's good that we saved it before.
		// num^th O reg comes after 7 G regs:
		const int offs = stackBias - 8 - 8 * (7 + num);
		i = new sparc_instr(sparc_instr::load, 
				    sparc_instr::ldExtendedWord,
				    sparc_reg::fp, offs, dst_reg);
                em->emit(i);
	    }
	    else {
		// Can get by using a regular mov
                i = new sparc_instr(sparc_instr::mov, src_reg, dst_reg);
		em->emit(i);
	    }
	}
	// argRegs[i] is no longer used, free it.
	rs->freeRegister(argRegs[j]);
    }

    // Emit the actual call
    em->emitCall(calleeAddr);
    //em->emitCall(pdstring("kerninst_call_me"));
    i = new sparc_instr(sparc_instr::nop);
    em->emit(i);

    // Move the return value into the dest reg
    sparc_reg dst_reg(sparc_reg::rawIntReg, dest);
    if (dst_reg != sparc_reg::o0) {
        i = new sparc_instr(sparc_instr::mov, sparc_reg::o0, dst_reg);
	em->emit(i);
    }

    // Restore G's, O's and condition codes
    // Start with condition codes
    i = new sparc_instr(sparc_instr::load, sparc_instr::ldExtendedWord,
			sparc_reg::fp, regOffset, sparc_reg::o7);
    em->emit(i);
    i = new sparc_instr(sparc_instr::writestatereg, sparc_reg::o7, 0x2);
    em->emit(i);

    regOffset = stackBias - 8;
    for (sparc_reg_set::const_iterator ireg = regsToSave.begin(); 
	 ireg != regsToSave.end(); ireg++) {
	if (*ireg != dst_reg) { // try not to overwrite dst_reg
            i = new sparc_instr(sparc_instr::load,
				sparc_instr::ldExtendedWord,
				sparc_reg::fp, regOffset, (sparc_reg&)*ireg);
	    em->emit(i);
	}
	regOffset -= 8;
    }
}

int getInsnCost(opCode /*t*/)
{
    assert(false && "Not yet implemented");
}

void emitFuncJump(opCode /*op*/, char */*i*/, HostAddress &/*base*/,
		  const function_base */*func*/, process */*proc*/)
{
    assert(false && "Not yet implemented");
}

bool doNotOverflow(int value)
{
    const int MAX_IMM13 = 4095;
    const int MIN_IMM13 = -4096;
    // we are assuming that we have 13 bits to store the immediate operand.
    return (value <= MAX_IMM13 && value >= MIN_IMM13);
}

void emitLoadPreviousStackFrameRegister(Register srcPrevFrame,
					Register destThisFrame,
					char *insn,
					HostAddress &/*base*/,
					int /*size*/,
					bool /*noCost*/)
{
    tempBufferEmitter *em = (tempBufferEmitter *)insn;
    const abi &theKernelABI = em->getABI();
    const int stackBias = theKernelABI.getStackBias();

    sparc_reg src_reg(sparc_reg::rawIntReg, srcPrevFrame);
    sparc_reg dst_reg(sparc_reg::rawIntReg, destThisFrame);

    // We'll generate code into this vector first and then, emit it
    pdvector<instr_t*> seq;
    sparc_reg recov_reg = sparc_instr::recoverFromPreviousFrame(src_reg, dst_reg, 
                                                                stackBias, &seq);
    if (recov_reg != dst_reg) {
	seq.push_back(new sparc_instr(sparc_instr::mov, recov_reg, dst_reg));
    }
    pdvector<instr_t*>::iterator iter = seq.begin();
    for (; iter != seq.end(); iter++) {
	em->emit(*iter);
    }
}

// for updateCostOp
void emitVupdate(opCode /*op*/, RegContents /*src1*/, Register /*src2*/,
		 HostAddress /*dst*/, char */*insn*/, HostAddress &/*base*/,
		 bool /*noCost*/)
{
    assert(false && "Not yet implemented");
}

void getParamRegister(unsigned paramId, bool hasExtraSave, Register dst,
		      tempBufferEmitter *em)

{
    const abi &theABI = em->getABI();
    const int argOffset = theABI.getMinFrameUnalignedNumBytes() + // unaligned!
	theABI.getStackBias() + (paramId - 6) * sizeof(kptr_t);
    // assumes sizeof(register) == sizeof(kptr_t)
    
    sparc_reg dst_reg(sparc_reg::rawIntReg, dst);
    sparc_reg param_reg = sparc_reg::g0;
    sparc_reg sp_reg = sparc_reg::sp;

    if (paramId < 6) {
	if (!hasExtraSave) {
	    // We have not emitted a save -> params are in O's
	    param_reg = sparc_reg(sparc_reg::rawIntReg, REG_O(paramId));
	}
	else {
	    // We have emitted a save -> params are in I's
	    param_reg = sparc_reg(sparc_reg::rawIntReg, REG_I(paramId));
	}
	if (dst_reg != param_reg) {
	    // Move param to dst
	    instr_t *i = new sparc_instr(sparc_instr::mov, param_reg, dst_reg);
	    em->emit(i);
	}
    }
    else {
	if (hasExtraSave) {
	    sp_reg = sparc_reg::fp;
	}
	em->emitLoadKernelNative(sp_reg, argOffset, dst_reg);
    }
}

// Finds the register returned at the return point spliceAddr
// which belongs to the function at entryAddr
void getReturnRegister(kptr_t entryAddr, kptr_t spliceAddr, bool hasExtraSave, 
		       Register rv, Register scratch,
		       tempBufferEmitter *em)
{
    // This routine may not work on the 32-bit platform: if a function
    // returns a 64-bit value, we'll return just one register (e.g., o0),
    // while on 32-bit sparc we should have returned two (e.g., o0 and o1).
    // Should work fine for functions returning 32 bits or less
    // assert(sizeof(kptr_t) == sizeof(uint64_t));
    const abi &theABI = em->getABI();
    unsigned stackBias = theABI.getStackBias();

    const moduleMgr &theModuleMgr = *global_moduleMgr;
    const function_t *patched_fn = theModuleMgr.findFnOrNULL(entryAddr, true);
    if (patched_fn == 0) {
	assert(false && "Can not find the function specified");
    }

    // We'll generate code into this vector first and then, emit it
    pdvector<instr_t*> seq;

    sparc_reg rv_reg(sparc_reg::rawIntReg, rv);
    sparc_reg scratch_reg(sparc_reg::rawIntReg, scratch);

    const instr_t *entryInsn = patched_fn->get1OrigInsn(entryAddr);
    const bool isLeaf = (!entryInsn->isSave());

    if (isLeaf) {
	const sparc_instr *dsInsn = (const sparc_instr*)patched_fn->get1OrigInsn(spliceAddr+4);
	sparc_instr::sparc_ru dsRu;
	sparc_instr::memUsage dsMu;
	sparc_instr::sparc_cfi dsCflow;

	dsInsn->getInformation(&dsRu, &dsMu, NULL, &dsCflow, NULL);

	if (dsRu.definitelyWritten->exists(sparc_reg::o0) ||
	    dsRu.maybeWritten->exists(sparc_reg::o0)) {
	    // The delay slot of the retl instruction modifies the return
	    // value (which is quite common) so we need to emit its
	    // equivalent to put the retval into rv_reg
	    
	    // If the return value is loaded from memory we can not
	    // execute dsInsn twice. Otherwise, one can modify this
	    // memory location in between reads and we'll return a
	    // bogus return value
	    assert(!dsMu.memRead && "Unsupported return sequence");

	    // For similar reasons, we can not execute rd and rdpr twice
	    assert(!dsInsn->isRd() && !dsInsn->isRdPr() && 
		   "Unsupported return sequence");

	    // Can not execute control flow instructions twice
	    assert(dsCflow.fields.controlFlowInstruction == 0 &&
		   "Unsupported return sequence");

	    dsInsn->changeDestRegister(hasExtraSave, rv_reg, scratch_reg, 
				      stackBias, &seq);
	}
	else {
	    // The delay slot does not touch o0, leave everything as is
	    if (hasExtraSave) {
		seq.push_back(new sparc_instr(sparc_instr::mov,
                                              sparc_reg::i0, rv_reg));
	    }
	    else {
		seq.push_back(new sparc_instr(sparc_instr::mov,
                                              sparc_reg::o0, rv_reg));
	    }
	}
    }
    else {
	// Non-leaf function: quite a few different cases
	// The biggest problem is to handle "restore rs1, rs2, rd"
	// in the delay slot of the return (we do not support anything
	// other than "ret ; restore ..")
	const kptr_t nextInsnAddr = spliceAddr + 4;
	assert(patched_fn->containsAddr(nextInsnAddr));
	
	const sparc_instr *nextInsn = (const sparc_instr*)patched_fn->get1OrigInsn(nextInsnAddr);
	sparc_instr::sparc_ru ru;
	nextInsn->getRegisterUsage(&ru);
	if (!ru.sr.isRestore) {
	    assert(false && "Unsupported return sequence");
	}

	sparc_reg restore_rs1(sparc_reg::g0);
	sparc_reg restore_rs2(sparc_reg::g0);
	sparc_reg restore_rd(sparc_reg::g0);
	int restore_simm13;
	bool usingRs2;

	if (nextInsn->isRestoreUsing2RegInstr(restore_rs1, restore_rs2, 
					      restore_rd)) {
	    usingRs2 = true;
	}
	else if (nextInsn->isRestoreUsingSimm13Instr(restore_rs1, 
						     restore_simm13, 
						     restore_rd)) {
	    usingRs2 = false;
	}
	else {
	    assert(false && "Unknown variant of \"restore\"");
	}
	// The restore instruction writes to o0, we need to compensate
	// for this
	unsigned num;
	bool needsAdd = (restore_rd.is_oReg(num) && num == 0);
	
	if (!hasExtraSave) {
	    // There is no save/restore around our instrumentation, good.
	    if (!needsAdd) {
		// restore which does not write into o0
		seq.push_back(new sparc_instr(sparc_instr::mov,
                                              sparc_reg::i0, rv_reg));
	    }
	    else {
		// restore writes to o0, emit its equivalent
		if (usingRs2) {
		    seq.push_back(new sparc_instr(sparc_instr::add, restore_rs1, 
                                                  restore_rs2, rv_reg));
		}
		else {
		    seq.push_back(new sparc_instr(sparc_instr::add, restore_rs1, 
                                                  restore_simm13, rv_reg));
		}
	    }
	}
	else {
	    if (!needsAdd) {
		sparc_reg recov_reg = sparc_instr::recoverFromPreviousFrame(sparc_reg::i0,
                                                                            rv_reg, 
                                                                            stackBias, 
                                                                            &seq);
		assert(recov_reg == rv_reg);
	    }
	    else {
		sparc_reg src1_reg = sparc_instr::recoverFromPreviousFrame(restore_rs1, 
                                                                           rv_reg, 
                                                                           stackBias, 
                                                                           &seq);
		if (usingRs2) {
                    sparc_reg src2_reg = sparc_instr::recoverFromPreviousFrame(
                                              restore_rs2, scratch_reg, stackBias, &seq);
		    seq.push_back(new sparc_instr(sparc_instr::add, src1_reg, 
                                                  src2_reg, rv_reg));
		}
		else {
		    seq.push_back(new sparc_instr(sparc_instr::add, src1_reg,
                                                  restore_simm13, rv_reg));
		}
	    }
	}
    }
    pdvector<instr_t*>::iterator iter = seq.begin();
    for (; iter != seq.end(); iter++) {
	em->emit(*iter);
    }
}

// for operations requiring a Register to be returned
// (e.g., getRetValOp, getParamOp, getSysRetValOp, getSysParamOp)
// For getParamOp and getSysParamOp scratch is the parameter's number,
// for getRetValOp and getSysRetValOp it is the actual register
void emitR(opCode op, Register dst, Register scratch,
	   HostAddress entryAddr, HostAddress spliceAddr,
	   char *insn, HostAddress &/*base*/, bool /*noCost*/)
{
    tempBufferEmitter *em = (tempBufferEmitter *)insn;

    switch(op) {
    case getSysParamOp:
	// We have not emitted a save -> params are in O's
	getParamRegister(scratch, false, dst, em);
	break;
    case getParamOp:
	// We have emitted a save -> params are in I's
	getParamRegister(scratch, true, dst, em);
	break;
    case getSysRetValOp:
	// We have not emitted a save
	getReturnRegister(entryAddr, spliceAddr, false, dst, scratch, em);
	break;
    case getRetValOp:
	// We have emitted a save
	getReturnRegister(entryAddr, spliceAddr, true, dst, scratch, em);
	break;
    default:
	assert(false && "Unknown opcode in emitR()");
    }
}

// for operations requiring an HostAddress to be returned
// (e.g., ifOp/branchOp, trampPreambleOp/trampTrailerOp)
HostAddress  emitA(opCode /*op*/, Register /*src1*/, Register /*src2*/,
		   Register /*dst*/, char */*insn*/, HostAddress &/*base*/, 
		   bool /*noCost*/)
{
//      cout << "emitA(" << op << ", Reg" << src1 << ", Reg" << src2 
//  	 << ", Reg" << dst << ", 0x" << hex << base << dec << ")\n";
    assert(false && "Not yet implemented");
    return 0;
}

// Generate code to return from a function
void emitRet(char *insn)
{
//      cout << "emitRet\n";
    tempBufferEmitter *em = (tempBufferEmitter *)insn;
    assert(em != 0);
    instr_t *i = new sparc_instr(sparc_instr::retl); 
    em->emit(i);
    i = new sparc_instr(sparc_instr::nop);
    em->emit(i);
}

} // namespace
