#include "emit.h"
#include "tempBufferEmitter.h"
#include "abi.h"
#include "power64_abi.h"
#include "machineInfo.h"
#include "power_instr.h"
#include "power_funkshun.h" //getTOCPtr
#include "moduleMgr.h"
#include "kapi.h"
#include "kernInstIoctls.h"
extern moduleMgr *global_moduleMgr;
extern machineInfo *theGlobalKerninstdMachineInfo;

#define MSR_EE (1<<(15)) 
//kernel defines all MSR fields, but we don't have access to it from userspace. //Note that hardcoding is relatively safe, since MSR layout should not change.

// We put everything in a namespace to enable tools that use both
// Kerninst and Dyninst API
namespace kcodegen {

// If [raddr] == src, exchange the contents of dest and [raddr]
// Otherwise, reload src with [raddr] and branch to the restart label
void emitAtomicExchange(Register raddr, Register src, Register dest,
			Register temp,
			const pdstring &restart_label, char *insn)
{
//      cout << "emitAtomicExchange([Reg" << raddr << "], Reg" << src  
//  	 << ", Reg" << dest << ", " << restart_label << ")\n";
    // raddr holds the address of the variable, src holds the value of
    // the variable before the expression used to calculate the variable's
    // new value held in dest. if [raddr] == src, it's safe to store dest
    // in [raddr] since the original value hasn't been updated, which means
    // the newly computed value is valid. otherwise, you need to reload src
    // with the updated original value and try again. The sequence below
    // is largely based on "Compare and Swap" example from "PowerPC 
    // Architecture", page 252.  The code should look like this:
    // loop: ldarx temp_reg, 0(addr_reg) #load with a reservation
    //       cmpd  temp_reg, src_reg     #do we still have the same value?
    //       bne   redo                  #redo the calculation if not
    //       stdcx dst_reg, 0(addr_reg)  #store new value if still reserved
    //       bne   loop                  #loop if reservation is gone
    // redo: mr  src_reg, temp_reg       #move new value into src_reg
    //       bne restart_label

    tempBufferEmitter *theEmitter = (tempBufferEmitter *)insn;
    assert(theEmitter != 0);
    power_reg addr_reg(power_reg::rawIntReg, raddr);
    power_reg src_reg(power_reg::rawIntReg, src);
    power_reg dst_reg(power_reg::rawIntReg, dest);
    power_reg temp_reg(power_reg::rawIntReg, temp);
    

    tempBufferEmitter::insnOffset loopLabelOffset = theEmitter->currInsnOffset();
    instr_t *i = new power_instr(power_instr::load, 
				 power_instr::ldDoubleReserveX,
				 temp_reg, power_reg::gpr0, // no offset  
				 addr_reg);
    theEmitter->emit(i);
    i = new power_instr(power_instr::cmp, 
			false, //not logical
			0, //CR0
			1, //compare both words
			src_reg, 
			temp_reg); 
    theEmitter->emit(i);

    i = new power_instr(power_instr::branchcond,
			power_instr::notEqual,
			0, //CR0
			0, //no displacement for now
			0, //not absolute
			0); // no linking
    pdstring redo_label = restart_label + pdstring("_redo");
    theEmitter->emitCondBranchToForwardLabel(i, redo_label);
    
    i = new power_instr(power_instr::store, 
			power_instr::stDoubleCondX,
			dst_reg, power_reg::gpr0, // no offset  
			addr_reg);
    theEmitter->emit(i);

    i = new power_instr(power_instr::branchcond,
			power_instr::notEqual,
			0, //CR0
			loopLabelOffset - theEmitter->currInsnOffset(), //displacement
			0, //not absolute
			0); // no linking
    theEmitter->emit(i);
   
    theEmitter->emitLabel(redo_label);
    i = new power_instr(power_instr::logicor, false, //no complement 
			src_reg, temp_reg, temp_reg, 
			0); //no CR modification
    theEmitter->emit(i);
    
    i = new power_instr(power_instr::branchcond,
			power_instr::notEqual,
			0, //CR0
			0, //no displacement for now
			0, //not absolute
			0); // no linking
    theEmitter->emitCondBranchToLabel(i, restart_label);    
}

void emitLabel(const pdstring& name, char *insn)
{
//      cout << "emitLabel(" << name << ")\n";

    tempBufferEmitter *theEmitter = (tempBufferEmitter *)insn;
    assert(theEmitter != 0);
    theEmitter->emitLabel(name);
}

void emitCondBranch(opCode /*condition */, Register src, pdstring label, char *insn)
{
//      cout << "emitCondBranch(" << "Reg" << src << ", " << label << ")\n";
    //Due to the way code is generated from ASTs, there is no way to take
    //advantage of the condition register.  Conditions are evaluated
    //and stored in a register as 1 (true) or 0 (not true).  Thus,
    //we just compare the src_reg to 1 and jump to the else label if not equal.
    //(above paraphrased from emit-x86 comment -Igor)
    tempBufferEmitter *em = (tempBufferEmitter *)insn;
    assert(em != 0);
    power_reg src_reg(power_reg::rawIntReg, src);

    instr_t *i = new power_instr(power_instr::cmp, 
				 false, //not logical
				 0, //CR0
				 1, //use all 64-bits
				 src_reg, 
				 1); 
    em->emit(i);
    i = new power_instr(power_instr::branchcond,
			power_instr::notEqual,
			0, //CR0
			0, //no displacement for now
			0, //not absolute
			0); // no linking
    
    em->emitCondBranchToLabel(i, label);
    
}

void emitJump(pdstring label, char *insn)
{
//      cout << "emitJump(" << label << ")\n";
    tempBufferEmitter *em = (tempBufferEmitter *)insn;
    assert(em != 0);
    instr_t *i = new power_instr(power_instr::branch, 
				 0, //displacement for now
				 0, //not absolute
				 0); //no linking
    em->emitCondBranchToLabel(i, label);
}

// Emit code to compute a boolean expression (which puts 0 or 1 into dst)
// Unfortunately, Power lacks conditional moves, so branching is 
//inevitable.  Some day, we might want to optimize this by using
//CR fields, which will save a branch and an extra compare
//in emitCondJump.
//Here is the code that we generate:
//          mov dst, 1
//          cmp src, imm
//          bcond done     //jump to done if cond is true
//          mov dst, 0
//    done:
//NOTE: this only works if src != dst.  Otherwise, we are forced to
//use yet another branch:
//          cmp src, imm
//          bcond setone     //jump to instruction that moves 1 in
//          mov dst, 0
//          b done
//   setone:mov dst, 1
//    done:



static void genImmRelOp(opCode op, power_reg src_reg, RegContents src2imm, 
			power_reg dst_reg, tempBufferEmitter *theEmitter)
{ 
    bool needsExtraBranch = (src_reg == dst_reg);
    instr_t *i;

    if (!needsExtraBranch) {
       i = new power_instr(power_instr::addsub,
                                    power_instr::addD,
                                    dst_reg,
                                    power_reg::gpr0, //no source register
                                    1); //move constant 1 into dst_reg
       theEmitter->emit(i);
   }
    
    i = new power_instr(power_instr::cmp, 
			false, //not logical
			0, //CR0
			1, //use all 64-bits
			src_reg, 
			src2imm);
    theEmitter->emit(i);

    power_instr::CondCode cond;
    switch(op) {
	case lessOp:
	    cond = power_instr::lessThan;
	    break;
	case leOp:
	    cond = power_instr::lessThanEqual;
	    break;
	case greaterOp:
	    cond = power_instr::greaterThan;
	    break;
	case geOp:
	    cond = power_instr::greaterThanEqual;
	    break;
	case eqOp:
	    cond = power_instr::equal;
	    break;
	case neOp:
	    cond = power_instr::notEqual;
	    break;
	default:
	    assert(!"genRelOp - unsupported relative op\n");
	    break;
    }

    i = new power_instr(power_instr::branchcond,
			cond,
			0, //CR0
			(needsExtraBranch? 12 : 8), //displacement to the imaginary done label
			0, //not absolute
			0); // no linking
    theEmitter->emit(i);


    i = new power_instr(power_instr::addsub,
				power_instr::addD,
				dst_reg,
				power_reg::gpr0, //no source register
				0); //move constant 0 into dst_reg
    theEmitter->emit(i);

    if (needsExtraBranch) {
       i = new power_instr(power_instr::branch, 
                                    8, //displacement 
                                    0, //not absolute
                                    0); //no linking
       theEmitter->emit(i);

       i = new power_instr(power_instr::addsub,
                                    power_instr::addD,
                                    dst_reg,
                                    power_reg::gpr0, //no source register
                                    1); //move constant 1 into dst_reg
       theEmitter->emit(i);
    }
}
//same as above, but with both sources in registers
static void genRelOp(opCode op, power_reg src1_reg, power_reg src2_reg, 
			power_reg dst_reg, tempBufferEmitter *theEmitter)
{
    bool needsExtraBranch = ( (src1_reg == dst_reg) || (src2_reg == dst_reg) );
    instr_t *i;
    
    if (!needsExtraBranch) {
       i = new power_instr(power_instr::addsub,
                                    power_instr::addD,
				    dst_reg,
				    power_reg::gpr0, //no source register
				    1); //move constant 1 into dst_reg
       theEmitter->emit(i);
    }
    
    i = new power_instr(power_instr::cmp, 
			false, //not logical
			0, //CR0
			1, //use all 64-bits
			src1_reg, 
			src2_reg);
    theEmitter->emit(i);

    power_instr::CondCode cond;
    switch(op) {
	case lessOp:
	    cond = power_instr::lessThan;
	    break;
	case leOp:
	    cond = power_instr::lessThanEqual;
	    break;
	case greaterOp:
	    cond = power_instr::greaterThan;
	    break;
	case geOp:
	    cond = power_instr::greaterThanEqual;
	    break;
	case eqOp:
	    cond = power_instr::equal;
	    break;
	case neOp:
	    cond = power_instr::notEqual;
	    break;
	default:
	    assert(!"genRelOp - unsupported relative op\n");
	    break;
    }

    i = new power_instr(power_instr::branchcond,
			cond,
			0, //CR0
			needsExtraBranch? 12 : 8, //displacement to the imaginary done label
			0, //not absolute
			0); // no linking
    theEmitter->emit(i);
    i = new power_instr(power_instr::addsub,
				power_instr::addD,
				dst_reg,
				power_reg::gpr0, //no source register
				0); //move constant 0 into dst_reg
    theEmitter->emit(i);

    if (needsExtraBranch) {
       i = new power_instr(power_instr::branch, 
        			    8, //displacement 
	        		    0, //not absolute
		        	    0); //no linking
       theEmitter->emit(i);

       i = new power_instr(power_instr::addsub,
        			    power_instr::addD,
	        		    dst_reg,
		                    power_reg::gpr0, //no source register
			            1); //move constant 1 into dst_reg
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
    const power64_abi &theKernelABI = (const power64_abi &)theEmitter->getABI();
    assert(theEmitter != 0);
    
    instr_t *i;

    // Instructions with no registers
    if (op == noOp) {
        i = new power_instr(power_instr::nop);
        theEmitter->emit(i);
	return;
    }
    else if (op == disableLocalInterrupts) {
       unsigned saveMachineStateRegOffset = 
                           theKernelABI.getMachineStateRegSaveFrameOffset();
       power_reg dst_reg = power_reg(power_reg::rawIntReg, dst);
       i = new power_instr(power_instr::movefrommachinestatereg, dst_reg);
       theEmitter->emit(i);
       instr_t *i = new power_instr(power_instr::store, power_instr::stDoubleDS, 
			dst_reg, 
			power_reg::gpr1, //stack pointer
			saveMachineStateRegOffset); 
       theEmitter->emit(i);
       i = new power_instr(power_instr::logicand, false /* not shifted */,
			   dst_reg, dst_reg, ~MSR_EE & 0x0000ffff);
       theEmitter->emit(i);
       i = new power_instr(power_instr::movetomachinestatereg, dst_reg);
       theEmitter->emit(i);
       return;
   }
   else if (op == enableLocalInterrupts) {
      unsigned saveMachineStateRegOffset = 
                          theKernelABI.getMachineStateRegSaveFrameOffset();
      power_reg dst_reg = power_reg(power_reg::rawIntReg, dst);
      i = new power_instr(power_instr::load, power_instr::ldDoubleDS, 
			dst_reg, 
			power_reg::gpr1, //stack pointer
			saveMachineStateRegOffset); //load from lr position
      theEmitter->emit(i);
      i = new power_instr(power_instr::movetomachinestatereg, dst_reg);
      theEmitter->emit(i);
      return;
   }


    // Instructions with one source register
    power_reg src1_reg(power_reg::rawIntReg, src1);
    power_reg dst_reg(power_reg::rawIntReg, dst);
    if (op == movOp) {
         if (src1 == CTR_AST_DATAREG) { // ctr
            i = new power_instr(power_instr::movefromspr,
                                power_reg::ctr_type,
                                dst_reg);   
         }
         if (src1 == LR_AST_DATAREG) { //lr
            i = new power_instr(power_instr::movefromspr,
                                power_reg::lr_type,
                                dst_reg);   
         } else {
	    i = new power_instr(power_instr::addsub,
                                power_instr::addD,
                                dst_reg,
                                src1_reg,
                                0); //no offset
        }
	theEmitter->emit(i);
	return;
    }

    // Instructions with two source registers
    power_reg src2_reg(power_reg::rawIntReg, src2);
    if (op == plusOp || op == minusOp || op == timesOp || op == divOp ||
	op == shlOp || op == shrOp) {
	switch(op) {
	case plusOp:
	    i = new power_instr(power_instr::addsub, power_instr::addXO,
				dst_reg, 
				src1_reg, src2_reg,
				0, 0); // no overflow or CR setting
	    theEmitter->emit(i);
	    break;
	case minusOp:
	    // need to flip operands since subf computes rb-ra not ra-rb
	    i = new power_instr(power_instr::addsub, power_instr::subXO,
				dst_reg, 
				src2_reg, src1_reg,
				0, 0); // no overflow or CR setting
	    theEmitter->emit(i);
	    break;
	case timesOp:
	    //this will multiply two 32-bit numbers.  Is this what we want??
	    i = new power_instr(power_instr::mul, 
				power_instr::word, //take only 32 bits from sources 
				power_instr::low, //take the low 32 bits
				true, //signed numbers
				dst_reg,
				src1_reg, 
				src2_reg,
				0, 0); // no overflow or CR setting
	    theEmitter->emit(i);
	    break;
	case divOp:
	    i = new power_instr(power_instr::div, 
				power_instr::word,
				true, //signed numbers
				dst_reg,  
				src1_reg, src2_reg,   
				0, 0); // no overflow or CR setting
	    theEmitter->emit(i);
	    break;
	case shlOp:
	    i = new power_instr(power_instr::shift, 
				power_instr::shiftLDouble,
				dst_reg,   //shifted result placed here
				src1_reg,  //source reg
				src2_reg,  //number of positions to shift
				0); // no CR setting
	    theEmitter->emit(i);
	    break;
	case shrOp:
	    i = new power_instr(power_instr::shift, 
				power_instr::shiftRDouble,
				dst_reg,   //shifted result placed here
				src1_reg,  //source reg
				src2_reg,  //number of positions to shift
				0); // no CR setting
	    theEmitter->emit(i);
	    break;
	default:
	    assert(false && "Internal error");
	}
    }
    else if (op == andOp || op == orOp) {
	switch(op) {
	case andOp:
	    i = new power_instr(power_instr::logicand, 
				false, //no complement
				dst_reg, 
				src1_reg, 
				src2_reg,
				0); // no CR setting
	    theEmitter->emit(i);
	    break;
	case orOp:
	    i = new power_instr(power_instr::logicor, 
				false, //no complement
				dst_reg, 
				src1_reg, 
				src2_reg,
				0); // no CR setting
	    theEmitter->emit(i);
	    break;
	default:
	    assert(false && "Unknown opcode in emitV");
	}
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
    power_reg dst_reg(power_reg::rawIntReg, dst);
    
    instr_t *i;
    switch(op) {
    case loadConstOp:
	theEmitter->emitImmAddr(src1, dst_reg);
	break;
    case loadOp:
	theEmitter->emitImmAddr(src1, dst_reg);
	i = new power_instr(power_instr::load, 
			    power_instr::ldDoubleX,
			    dst_reg, 
			    power_reg::gpr0, //gpr0 means no offset 
			    dst_reg);
	theEmitter->emit(i);
	break;
    case loadAndSetAddrOp: {
	power_reg addr_reg(power_reg::rawIntReg, src2);
	theEmitter->emitImmAddr(src1, addr_reg);
	i = new power_instr(power_instr::load, 
			    power_instr::ldDoubleX,
			    dst_reg, 
			    power_reg::gpr0, //no offset 
			    addr_reg);
	theEmitter->emit(i);
	break;
    }
    case loadIndirOp: {
	power_reg addr_reg(power_reg::rawIntReg, src2);
	i = new power_instr(power_instr::load, 
			    power_instr::ldDoubleX,
			    dst_reg, 
			    power_reg::gpr0, //no offset
			    addr_reg);
	theEmitter->emit(i);
	break;
    }
    case loadCPUId: {
	//Getting current->processor;  
        //current is always stored in local_paca->xCurrent where 
        //pointer to local_paca  is in r13 
	//The offset off of current is in t_cpu_offset
	//The code we generate is this:
        // ld dst_reg, t_pacacurrent_offset(r13)
	// lwz dst_reg, t_cpu_offset(dst_reg)
	assert(theGlobalKerninstdMachineInfo != 0);
	unsigned t_cpu_offset = 
	    theGlobalKerninstdMachineInfo->getKernelThreadCpuOffset();
	unsigned t_pacacurrent_offset = 
	    theGlobalKerninstdMachineInfo->getKernelPacaCurrentOffset();

        unsigned paca_access = 
           theGlobalKerninstdMachineInfo->getKernelPacaAccess();
        
        // Load the current pointer into dst_reg

        if (paca_access == PACA_ACCESS_SPRG3) {
           i = new power_instr(power_instr::movefromspr, power_reg::sprg3,
                               dst_reg);
           theEmitter->emit(i);
           theEmitter->emitLoadKernelNative(dst_reg, 
                                            t_pacacurrent_offset, 
                                            dst_reg);
        } else if (paca_access == PACA_ACCESS_R13) {
	
           theEmitter->emitLoadKernelNative(power_reg::gpr13, 
					 t_pacacurrent_offset, 
					 dst_reg);
        } else {
           assert (false && "unknown paca access method");
        }
	i = new power_instr(power_instr::load, 
			    power_instr::ldWordZeroD,
			    dst_reg,dst_reg,  t_cpu_offset);
	theEmitter->emit(i);
	break;
    }
    case loadProcessId: {
	//loading process id is pretty much the same as cpu id above
	assert(theGlobalKerninstdMachineInfo != 0);
	unsigned t_pid_offset = 
	    theGlobalKerninstdMachineInfo->getKernelProcPidOffset();
	unsigned t_pacacurrent_offset = 
	    theGlobalKerninstdMachineInfo->getKernelPacaCurrentOffset();
	
        unsigned paca_access = 
           theGlobalKerninstdMachineInfo->getKernelPacaAccess();
        
        // Load the current pointer into dst_reg
         
        if (paca_access == PACA_ACCESS_SPRG3) {
           i = new power_instr(power_instr::movefromspr, power_reg::sprg3,
                               dst_reg);
           theEmitter->emit(i);
           theEmitter->emitLoadKernelNative(dst_reg, 
                                            t_pacacurrent_offset, 
                                            dst_reg);
        } else if (paca_access == PACA_ACCESS_R13) {
	
           theEmitter->emitLoadKernelNative(power_reg::gpr13, 
					 t_pacacurrent_offset, 
					 dst_reg);
        } else {
           assert (false && "unknown paca access method");
        }


	i = new power_instr(power_instr::load, 
			    power_instr::ldWordZeroD,
			    dst_reg,dst_reg,  t_pid_offset);
	theEmitter->emit(i);
	break;

    }
    case loadHwCtrReg: {
       kapi_hwcounter_kind kind = (kapi_hwcounter_kind)src1;
       if (kind == HWC_TICKS) {
          i = new power_instr(power_instr::movefromtimebase,
                              dst_reg, power_reg::tb);
          theEmitter->emit(i);
          break;      
       } else {
          // set  to current counter for kind (hi 16 bits of src)
          uint32_t pmc_num = (src1 & 0xffff0000) >> 16;
          power_reg pmc_reg = power_reg(power_reg::pmc_type, pmc_num);
          i = new power_instr(power_instr::movefromspr, 
                              pmc_reg, dst_reg);
          theEmitter->emit(i);
       }
       break;
    }
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
    power_reg val_reg(power_reg::rawIntReg, val);

    instr_t *i;
    switch(op) {
    case storeOp: {
	power_reg addr_reg(power_reg::rawIntReg, scratch);
	theEmitter->emitImmAddr(dst, addr_reg);
	i = new power_instr(power_instr::store, 
			    power_instr::stDoubleX,
			    val_reg,
			    power_reg::gpr0, //no offset
			    addr_reg);
	theEmitter->emit(i);
	break;
    }
    case storeIndirOp: {
	power_reg addr_reg(power_reg::rawIntReg, scratch);
	i = new power_instr(power_instr::store, 
			    power_instr::stDoubleX,
			    val_reg, 
			    power_reg::gpr0, //no offset
			    addr_reg);
	theEmitter->emit(i);
	break;
    }
    default:
	assert(false && "Unknown opcode in emitVstore");
    }
}

void emitImm(opCode op, Register src, RegContents src2imm, Register dst, 
	     char *insn, HostAddress &/*base*/, bool /*noCost*/)
{
//      cout << "emitImm(" << op << ", Reg" << src << ", " 
//  	 << src2imm << ", Reg" << dst << ", " << hex << base << dec << ")\n";
    tempBufferEmitter *theEmitter = (tempBufferEmitter *)insn;
    assert(theEmitter != 0);
    power_reg src_reg(power_reg::rawIntReg, src);
    power_reg dst_reg(power_reg::rawIntReg, dst);

    instr_t *i;
    switch (op) {
	case plusOp:
	    i = new power_instr(power_instr::addsub,
				power_instr::addD,
				dst_reg, 
				src_reg, 
				src2imm);
	    theEmitter->emit(i);
	    break;
	case minusOp:
	    //add dst + (-value) is the standard way to subtract on power
	    i = new power_instr(power_instr::addsub, power_instr::addD,
				dst_reg, 
				src_reg, 
				-src2imm);
	    theEmitter->emit(i);
	    break;
	case timesOp:
            i = new power_instr(power_instr::mul,
                                power_instr::low, //take the low 32 bits
                                dst_reg,
                                src_reg, 
                                src2imm);
            theEmitter->emit(i);
            break;
	case divOp:
	    //there is no divide immediate for Power, so we are forced 
	    // to emit the immediate into a register first
	    theEmitter->emitImm32(src2imm, dst_reg);
	    i = new power_instr(power_instr::div, 
				power_instr::word,
                                true, //signed numbers
                                dst_reg,  
                                src_reg, dst_reg,   
                                0, 0); // no overflow or CR setting
            theEmitter->emit(i);
            break;
	case orOp:
            i = new power_instr(power_instr::logicor, 
                                false, //not shifted
                                dst_reg, 
                                src_reg, 
                                src2imm);
            theEmitter->emit(i);
            break;
	case andOp:
	    i = new power_instr(power_instr::logicand, 
                                false, //not shifted
                                dst_reg, 
                                src_reg, 
                                src2imm);
            theEmitter->emit(i);
            break;
	case shlOp:
            assert(src2imm < 64);
	    //similar to above, shift right immediate is 
	    //shift left immediate is actually 
	    // rldicr dst, src, shiftnum, 63-shiftnum
	    //(yep, it's magic, see page 230 of powerpc manual)
            i = new power_instr(power_instr::rotate, 
                                power_instr::rotateLeftDoubleImmClearRightMD,
                                dst_reg,   //shifted result placed here
                                src_reg,  //source reg
                                src2imm,  //number of positions to shift
				63-src2imm,
                                0); // no CR setting
            theEmitter->emit(i);
            break;
	case shrOp:
            assert(src2imm < 64);
	    //similar to above, shift right immediate is 
	    // rldicl dst,src,  64-shiftnum, shiftnum 
	    i = new power_instr(power_instr::rotate, 
                                power_instr::rotateLeftDoubleImmClearLeftMD,
                                dst_reg,   //shifted result placed here
                                src_reg,  //source reg
                                64 -  src2imm,  //number of positions to shift
				src2imm, 
                                0); // no CR setting
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
    tempBufferEmitter *em = (tempBufferEmitter *) insn;
    const power64_abi &theKernelABI = (const power64_abi &)em->getABI();
    unsigned GPRSize = theKernelABI.getGPRSize();
    power_reg dst_reg(power_reg::rawIntReg, dest);


    const moduleMgr &theModuleMgr = *global_moduleMgr;
    const function_t *callee_fn = theModuleMgr.findFnOrNULL(calleeAddr, true);
    if (callee_fn == 0) {
	assert(false && "Can not find the function to call in emitFuncCall");
    }

    std::pair<pdstring, const function_t*> modAndFn = theModuleMgr.findModAndFn
                        (callee_fn->getEntryAddr(), true); //true-->startOnly
    const loadedModule &mod = *theModuleMgr.find(modAndFn.first);
    kptr_t toc_anchor = mod.getTOCPtr();

     unsigned saveVolatileOffset = 
	 theKernelABI.getLocalVariableSpaceFrameOffset();
    unsigned saveLinkRegOffset = 
         theKernelABI.getLinkRegSaveFrameOffset();
    pdvector<Register> argRegs;
    pdvector<AstNode *>::const_iterator iarg = operands.begin();
    for (; iarg != operands.end(); iarg++) {
	AstNode *arg = *iarg;
	Register reg = arg->generateCode_phase2(proc, rs, insn, base,
						noCost, ifForks, location);
	argRegs.push_back(reg);
    }
    power_reg_set regsToSave = power_reg_set(power_reg::gpr0);
   
    //Save r0, which we will use as a temp register
    instr_t *i = new power_instr(power_instr::store, power_instr::stDoubleDS, 
			power_reg::gpr0, 
			power_reg::gpr1, //stack pointer
			saveVolatileOffset); //save at r0 position
    em->emit(i);
    
    //Save register 2 (TOC)
    i = new power_instr(power_instr::store, power_instr::stDoubleDS, 
			power_reg::gpr2, 
			power_reg::gpr1, //stack pointer
			saveVolatileOffset+2*GPRSize); //save at r2 position
    em->emit(i);
    //Save all volatile regs.   Just as in SPARC,  we can be much smarter and 
    //avoid saving  free registers and registers that are not modified 
    //by the callee.  This is still a TODO.
    for (unsigned j = 3; j <= 13; j++) {
	i = new power_instr(power_instr::store, power_instr::stDoubleDS, 
			    power_reg(power_reg::rawIntReg,j), 
			    power_reg::gpr1, //stack pointer
			    saveVolatileOffset+j*GPRSize); //save at rx position
	em->emit(i);
    }
    

    //  Save the link register.  We need a temp register to move it to.
    // Why not r0, since we just saved it above.
    // mflr r0
    i = new power_instr(power_instr::movefromspr, 
				 power_reg::lr, power_reg::gpr0);
    // Register 0 is actually the link register now. 
   
    em->emit(i);
    i = new power_instr(power_instr::store, power_instr::stDoubleDS, 
			power_reg::gpr0, 
			power_reg::gpr1, //stack pointer
			saveLinkRegOffset); 
    em->emit(i);

    unsigned numArgs = argRegs.size();
    for (unsigned j=0; j<numArgs; j++) {
	Register reg = argRegs[j];
	power_reg dst_reg(power_reg::rawIntReg, j+3); //params start with r3
	power_reg src_reg(power_reg::rawIntReg, reg);
	if (src_reg != dst_reg) {
	    // Needs move
	    unsigned num;
	    if (src_reg.isParamReg(num) && num < j) {
		// Can't mov: src_reg is itself one of the args and
		// have been overwritten by this time. Load the correct
		// value from memory. It's good that we saved it before.
		// num^th parameter reg comes after r0-r2 (3 regs to skip)
		i = new power_instr(power_instr::load, 
				    power_instr::ldDoubleDS,
				    dst_reg,
				    power_reg::gpr1, 
				    saveVolatileOffset+(reg+3)*GPRSize 
				    );
                em->emit(i);
	    }
	    else {
		// Can get by using a regular mov
                i = new power_instr(power_instr::logicor, 
				    false, //no complement
				    dst_reg, src_reg, src_reg,
				    0); //no CR modification
		em->emit(i);
	    }
	}
	// argRegs[i] is no longer used, free it.
	rs->freeRegister(argRegs[j]);
    }
    //set up new TOC value
    insnVec_t *piv = insnVec_t::getInsnVec();
    power_instr::genImmAddr(piv, toc_anchor, power_reg::gpr2);
    em->emitSequence(piv);
    // Emit the actual call by putting the address into r0, then 
    // branching to LR.
    piv = insnVec_t::getInsnVec(); 
    //piv->clear();
    power_instr::genImmAddr(piv, calleeAddr, power_reg::gpr0);
    em->emitSequence(piv);
    i = new power_instr(power_instr::movetospr, power_reg::lr, power_reg::gpr0);
    em->emit(i);
    i = new power_instr(power_instr::branchcondlinkreg, 
			power_instr::always,
			0, //CR field is not checked...
			1); //yes, link
    em->emit(i);
    
    // Move the return value into the dest reg
/* 
    if (dst_reg != power_reg::gpr3) {
        i = new power_instr(power_instr::logicor,
			    false, //no complement
			    dst_reg, power_reg::gpr3,power_reg::gpr3, 
			    0); //no CR modification
	em->emit(i);
    } 
*/  
    //  Restore the link register.
    // mtlr r3
    i = new power_instr(power_instr::load, power_instr::ldDoubleDS, 
			power_reg::gpr0, 
			power_reg::gpr1, //stack pointer
			saveLinkRegOffset); //load from lr position
    em->emit(i);
    i = new power_instr(power_instr::movetospr, power_reg::lr, power_reg::gpr0);
    em->emit(i);

    //Restore regs.   
    for (unsigned j = 2; j <= 13; j++) {
	if (dest != j) {
	    //try not to overwrite destination register
	    i = new power_instr(power_instr::load, power_instr::ldDoubleDS, 
				power_reg(power_reg::rawIntReg,j),
				power_reg::gpr1, //stack pointer
				saveVolatileOffset+j*GPRSize); //restore from rx position
	    em->emit(i);
	}
    }
    //restore register 0
    i = new power_instr(power_instr::load, power_instr::ldDoubleDS, 
			power_reg::gpr0,
			power_reg::gpr1, //stack pointer
			saveVolatileOffset); //restore from r0 position
    em->emit(i);
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
    power_reg dst_reg(power_reg::rawIntReg, destThisFrame);
    instr_t *i;
    if (srcPrevFrame == CTR_AST_DATAREG) { // ctr
       i = new power_instr(power_instr::movefromspr,
                           power_reg::ctr_type,
                           dst_reg);   
    }
    else if (srcPrevFrame == LR_AST_DATAREG) { //lr
       i = new power_instr(power_instr::movefromspr,
                           power_reg::lr_type,
                           dst_reg);   
    } else {
       //FIXME: for int registers, we don't know if this one has been saved
       //We need to emit a load here, and possibly fix it up later,
       //if this register wasn't saved.
       assert(false);
    }
    em->emit(i);
}

// for updateCostOp
void emitVupdate(opCode /*op*/, RegContents /*src1*/, Register /*src2*/,
		 HostAddress /*dst*/, char */*insn*/, HostAddress &/*base*/,
		 bool /*noCost*/)
{
    assert(false && "Not yet implemented");
}

void getParamRegister(unsigned paramId, bool /* hasExtraSave */, Register dst,
		      tempBufferEmitter *em)

{
    const abi &theABI = em->getABI();
    const int argOffset = theABI.getMinFrameUnalignedNumBytes() + // unaligned!
        paramId * sizeof(kptr_t);
    // assumes sizeof(register) == sizeof(kptr_t)
    
    power_reg dst_reg(power_reg::rawIntReg, dst);
    power_reg param_reg = power_reg(power_reg::rawIntReg, paramId+3);
    power_reg sp_reg = power_reg::gpr1;
    if (paramId < 8) {
	if ( (reg_t &)dst_reg != (reg_t &)param_reg) {
	    // Move param to dst
	    instr_t *i = new power_instr(power_instr::logicor, 
					 false, //no complement 
					 dst_reg,param_reg, param_reg,
					 0); //no CR modification
	    em->emit(i);
	}
    } else {
	em->emitLoadKernelNative(sp_reg, argOffset, dst_reg);
    }
}

// Finds the register returned at the return point spliceAddr
// which belongs to the function at entryAddr
void getReturnRegister(kptr_t entryAddr, kptr_t /*spliceAddr */, 
                       bool /* hasExtraSave */, 
		       Register rv, Register /* scratch */,
		       tempBufferEmitter *em)
{

    const moduleMgr &theModuleMgr = *global_moduleMgr;
    const function_t *patched_fn = theModuleMgr.findFnOrNULL(entryAddr, true);
    if (patched_fn == 0) {
	assert(false && "Can not find the function specified");
    }
    
    power_reg rv_reg(power_reg::rawIntReg, rv);
    if (rv_reg != power_reg::gpr3) {
	// Move return value to dst
	instr_t *i = new power_instr(power_instr::logicor,
				     false, //no complement
				     rv_reg, 
				     power_reg::gpr3, power_reg::gpr3, 
				     0); //no CR modification
	em->emit(i);
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
	assert(false && "getSysParamOp is not meaningful on Power");
	break;
    case getParamOp:
	getParamRegister(scratch, true, dst, em);
	break;
    case getSysRetValOp:
	assert(false && "getSysRetValOp is not meaningful on Power");
	break;
    case getRetValOp:
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
    instr_t *i = new power_instr(power_instr::branchcondlinkreg, 
				 power_instr::always,
				 0, //CR field does not matter here
				 0);//no link 
    em->emit(i);
}

} // namespace
