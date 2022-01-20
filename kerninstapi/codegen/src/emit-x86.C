#include "emit.h"
#include "tempBufferEmitter.h"
#include "abi.h"
#include "machineInfo.h"
#include "x86_instr.h"
#include "x86_reg.h"
#include "moduleMgr.h"
#include "kapi.h"

extern moduleMgr *global_moduleMgr;
extern machineInfo *theGlobalKerninstdMachineInfo;

unsigned emit_64bit_code = 0; 
kptr_t cpu_pseudoregs_addr = 0;

static pdstring getUniqueLabel()
{
    static int id = 0;
    char buffer[64];
    sprintf(buffer, "x86label_%d", id++);
    return pdstring(buffer);
}

// We put everything in a namespace to enable tools that use both
// Kerninst and Dyninst API
namespace kcodegen {

// If [raddr] == src, exchange the contents of dest and [raddr]
// Otherwise, reload src with [raddr] and branch to the restart label
void emitAtomicExchange(Register raddr, Register src, Register dest,
                        Register, //This param only used by ppc
			const pdstring &restart_label, char *insn)
{
//      cout << "emitAtomicExchange([Reg" << raddr << "], Reg" << src  
//  	 << ", Reg" << dest << ", " << restart_label << ")\n";

   // raddr holds the address of the variable, src holds the value of
   // the variable before the expression used to calculate the variable's
   // new value held in dest. if [raddr] == src, it's safe to store dest
   // in [raddr] since the original value hasn't been updated, which means
   // the newly computed value is valid. otherwise, you need to reload src
   // with the updated original value and try again. on x86, the cmpxchg
   // instruction does what we need, but src must be the EAX register

   tempBufferEmitter *theEmitter = (tempBufferEmitter *)insn;
   assert(theEmitter != 0);

   instr_t *i;
   pdstring done_label = restart_label + pdstring("_done");

   if(!emit_64bit_code) {
      x86_reg addr_reg(x86_reg::rawIntReg, raddr);
      x86_reg src_reg(x86_reg::rawIntReg, src);
      x86_reg dst_reg(x86_reg::rawIntReg, dest);
      
      if((addr_reg == src_reg) || (addr_reg == dst_reg)) {
         assert(!"address reg must be different from src & dst reg\n");
         return;
      }

      if(src_reg == dst_reg) {
         assert(!"src reg must be different from dst reg\n");
         return;
      }

      bool src_is_eax = (src_reg == x86_reg::eAX ? true : false);
      bool dst_is_eax = (dst_reg == x86_reg::eAX ? true : false);

      if(src_is_eax) {
         if(!dst_is_eax) {
            // All is well, since src_reg is EAX and addr_reg & dst_reg are not
            i = new x86_instr(x86_instr::cmpxchg, addr_reg, dst_reg);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::jcc, x86_instr::NotEqual, 0);
            theEmitter->emitCondBranchToLabel(i, restart_label);
         }
      } 
      else {
         // Now we have to do some trickery to make sure EAX holds src_reg
         if(dst_reg == x86_reg::eAX) {
            // We need to swap dst_reg and src_reg before and after cmpxchg, so
            // the following code is emitted:
            //     push dst_reg
            //     mov src_reg, dst_reg
            //     pop src_reg
            //     cmpxchg [addr_reg], src_reg
            //     je done_label
            //     push dst_reg
            //     mov src_reg, dst_reg
            //     pop src_reg
            //     jmp restart_label
            //  done_label:
            //     push dst_reg
            //     mov src_reg, dst_reg
            //     pop src_reg
         
            i = new x86_instr(x86_instr::push, dst_reg);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::mov, src_reg, dst_reg);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::pop, src_reg);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::cmpxchg, addr_reg, src_reg);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::jcc, x86_instr::Equal, 0);
            theEmitter->emitCondBranchToLabel(i, done_label);
            i = new x86_instr(x86_instr::push, dst_reg);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::mov, src_reg, dst_reg);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::pop, src_reg);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::jmp, (int32_t)0);
            theEmitter->emitCondBranchToLabel(i, restart_label);
            theEmitter->emitLabel(done_label);
            i = new x86_instr(x86_instr::push, dst_reg);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::mov, src_reg, dst_reg);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::pop, src_reg);
            theEmitter->emit(i);
         }
         else if(addr_reg == x86_reg::eAX) {
            // We need to swap addr_reg and src_reg before and after cmpxchg
            
            i = new x86_instr(x86_instr::push, addr_reg);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::mov, src_reg, addr_reg);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::pop, src_reg);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::cmpxchg, src_reg, dst_reg);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::jcc, x86_instr::Equal, 0);
            theEmitter->emitCondBranchToLabel(i, done_label);
            i = new x86_instr(x86_instr::push, addr_reg);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::mov, src_reg, addr_reg);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::pop, src_reg);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::jmp, (int32_t)0);
            theEmitter->emitCondBranchToLabel(i, restart_label);
            theEmitter->emitLabel(done_label);
            i = new x86_instr(x86_instr::push, addr_reg);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::mov, src_reg, addr_reg);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::pop, src_reg);
            theEmitter->emit(i);
         }
         else {
            // We need to push/pop EAX before/after cmpxchg
            
            i = new x86_instr(x86_instr::push, x86_reg::eAX);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::mov, src_reg, x86_reg::eAX);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::cmpxchg, addr_reg, dst_reg);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::jcc, x86_instr::Equal, 0);
            theEmitter->emitCondBranchToLabel(i, done_label);
            i = new x86_instr(x86_instr::mov, x86_reg::eAX, src_reg);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::pop, x86_reg::eAX);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::jmp, (int32_t)0);
            theEmitter->emitCondBranchToLabel(i, restart_label);
            theEmitter->emitLabel(done_label);
            i = new x86_instr(x86_instr::pop, x86_reg::eAX);
            theEmitter->emit(i);
         }
      }
   }
   else { // emit 64-bit atomic exchange using cmpxchg8b

      // OK, here's the situation: raddr, src, and dest are all 64-bit
      // memory pseudoregisters that have already been loaded. So, we 
      // need to extract the address in raddr (lower 32 bits) to a real 
      // register, put src into EDX:EAX, and put dest in ECX:EBX. Thus, 
      // we need to push all the real registers we use before changing 
      // them, and pop them before done (or restart)

#ifdef EXTRA_64BIT_SAVERESTORE
      // Save the five registers used by cmpxchg8b
      i = new x86_instr(x86_instr::push, x86_reg::eDI);
      theEmitter->emit(i);
      i = new x86_instr(x86_instr::push, x86_reg::eDX);
      theEmitter->emit(i);
      i = new x86_instr(x86_instr::push, x86_reg::eAX);
      theEmitter->emit(i);
      i = new x86_instr(x86_instr::push, x86_reg::eCX);
      theEmitter->emit(i);
      i = new x86_instr(x86_instr::push, x86_reg::eBX);
      theEmitter->emit(i);
#endif
      
      // Move the address into EDI
      i = new x86_instr(x86_instr::load, (int32_t)raddr, x86_reg::eBP, 
                        x86_reg::eDI);
      theEmitter->emit(i);
      
      
      // Move src into EDX:EAX
      i = new x86_instr(x86_instr::load, (int32_t)src, 
			x86_reg::eBP, x86_reg::eAX);
      theEmitter->emit(i);
      i = new x86_instr(x86_instr::load, (int32_t)src+4, 
			x86_reg::eBP, x86_reg::eDX);
      theEmitter->emit(i);

      // Move dest into ECX:EBX
      i = new x86_instr(x86_instr::load, (int32_t)dest, 
			x86_reg::eBP, x86_reg::eBX);
      theEmitter->emit(i);
      i = new x86_instr(x86_instr::load, (int32_t)dest+4, 
			x86_reg::eBP, x86_reg::eCX);
      theEmitter->emit(i);

      // Do the cmpxchg8b & jz to done_label
      i = new x86_instr(x86_instr::cmpxchg8b, x86_reg::eDI);
      theEmitter->emit(i);
      i = new x86_instr(x86_instr::jcc, x86_instr::Equal, 0);
      theEmitter->emitCondBranchToLabel(i, done_label);
 
      // Fall through - save new src & jmp to restart_label
      i = new x86_instr(x86_instr::store, x86_reg::eAX, (int32_t)src, 
                        x86_reg::eBP);
      theEmitter->emit(i);
      i = new x86_instr(x86_instr::store, x86_reg::eDX, (int32_t)src+4, 
                        x86_reg::eBP);
      theEmitter->emit(i);

#ifdef EXTRA_64BIT_SAVERESTORE
      i = new x86_instr(x86_instr::pop, x86_reg::eBX);
      theEmitter->emit(i);
      i = new x86_instr(x86_instr::pop, x86_reg::eCX);
      theEmitter->emit(i);
      i = new x86_instr(x86_instr::pop, x86_reg::eAX);
      theEmitter->emit(i);
      i = new x86_instr(x86_instr::pop, x86_reg::eDX);
      theEmitter->emit(i);
      i = new x86_instr(x86_instr::pop, x86_reg::eDI);
      theEmitter->emit(i);
#endif
      i = new x86_instr(x86_instr::jmp, (int32_t)0);
      theEmitter->emitCondBranchToLabel(i, restart_label);

      // Done - pop used registers
      theEmitter->emitLabel(done_label);
#ifdef EXTRA_64BIT_SAVERESTORE
      i = new x86_instr(x86_instr::pop, x86_reg::eBX);
      theEmitter->emit(i);
      i = new x86_instr(x86_instr::pop, x86_reg::eCX);
      theEmitter->emit(i);
      i = new x86_instr(x86_instr::pop, x86_reg::eAX);
      theEmitter->emit(i);
      i = new x86_instr(x86_instr::pop, x86_reg::eDX);
      theEmitter->emit(i);
      i = new x86_instr(x86_instr::pop, x86_reg::eDI);
      theEmitter->emit(i);
#endif
   }
}

void emitLabel(const pdstring& name, char *insn)
{
//      cout << "emitLabel(" << name << ")\n";

   tempBufferEmitter *theEmitter = (tempBufferEmitter *)insn;
   assert(theEmitter != 0);
   theEmitter->emitLabel(name);
}

void emitCondBranch(opCode /*condition*/, Register src, pdstring label, 
                    char *insn)
{
//      cout << "emitCondBranch(" << "Reg" << src << ", " << label << ")\n";

   // This is contrived, and deserves a bit of explanation. Due to the way
   // code is generated from ASTs, there is no way to take advantage of the
   // x86 EFLAGS register. Instead, all conditions are evaluated and the
   // result stored in a register as 1 (true) or 0 (not true). Thus, we just
   // do a compare of the src reg to 1. Since the label is the elsif case,
   // we jump to it when src != 1

   tempBufferEmitter *em = (tempBufferEmitter *)insn;
   assert(em != 0);

   instr_t *i;

   if(!emit_64bit_code) { // 32-bit code, pseudoregs == archregs
      x86_reg src_reg(x86_reg::rawIntReg, src);
      i = new x86_instr(x86_instr::cmp, (uint32_t)1, src_reg);
      em->emit(i);
      i = new x86_instr(x86_instr::jcc, 
                        x86_instr::NotEqual, 
                        0); // displacement for now
      em->emitCondBranchToLabel(i, label);
   }
   else { // 64-bit code, pseudoregs == mem locations
      pdstring true_label(label);
      true_label += pdstring("_TRUE");
      pdstring past_true_label(label);
      past_true_label += pdstring("_PAST_TRUE");
#ifdef EXTRA_64BIT_SAVERESTORE
      i = new x86_instr(x86_instr::push, x86_reg::eAX);
      em->emit(i);
#endif
      i = new x86_instr(x86_instr::load, (int32_t)src, 
			x86_reg::eBP, x86_reg::eAX);
      em->emit(i);
      i = new x86_instr(x86_instr::cmp, (uint32_t)1, x86_reg::eAX);
      em->emit(i);
      i = new x86_instr(x86_instr::jcc, 
                        x86_instr::NotEqual, 
                        0); // displacement for now
      em->emitCondBranchToLabel(i, true_label);
#ifdef EXTRA_64BIT_SAVERESTORE
      i = new x86_instr(x86_instr::pop, x86_reg::eAX);
      em->emit(i);
#endif
      i = new x86_instr(x86_instr::jmp, (int32_t)0);
      em->emitCondBranchToLabel(i, past_true_label);
      em->emitLabel(true_label);
#ifdef EXTRA_64BIT_SAVERESTORE
      i = new x86_instr(x86_instr::pop, x86_reg::eAX);
      em->emit(i);
#endif
      i = new x86_instr(x86_instr::jmp, (int32_t)0);
      em->emitCondBranchToLabel(i, label);
      em->emitLabel(past_true_label);
   }
}

void emitJump(pdstring label, char *insn)
{
//      cout << "emitJump(" << label << ")\n";
   tempBufferEmitter *em = (tempBufferEmitter *)insn;
   assert(em != 0);
   instr_t *i = new x86_instr(x86_instr::jmp,  
                              (int32_t)0); // displacement for now
   em->emitCondBranchToLabel(i, label);
}

// Emit code to compute a boolean expression (which puts 0 or 1 into dst)
// We can do this with no branching, thanks to setcc. Note that dst
// is expected to be an 8-bit register, so dst_reg can only be a register
// that has a valid low part (EAX-EDX). This is checked by setcc x86_instr
// Here is how to convert (src >= imm) into (0 or 1):
//   cmp src, imm         // compare src to imm, set flags
//   setge dst            // if(>=) little byte of dst=1, else =0
//   and dst, 0xFF        // clear out anything left in upper 3 bytes of dst 
static void genImmRelOp(opCode op, x86_reg src_reg, RegContents src2imm, 
			x86_reg dst_reg, tempBufferEmitter *theEmitter)
{
   // The following is correct even if src_reg == dst_reg
   instr_t *i = new x86_instr(x86_instr::cmp, src2imm, src_reg);
   theEmitter->emit(i);
   x86_instr::CondCodes cond;
   switch(op) {
   case lessOp:
      cond = x86_instr::LessThan;
      break;
   case leOp:
      cond = x86_instr::LessThanEqual;
      break;
   case greaterOp:
      cond = x86_instr::GreaterThan;
      break;
   case geOp:
      cond = x86_instr::GreaterThanEqual;
      break;
   case eqOp:
      cond = x86_instr::Equal;
      break;
   case neOp:
      cond = x86_instr::NotEqual;
      break;
   default:
      assert(!"genImmRelOp - unsupported relative op\n");
      break;
   }
   i = new x86_instr(x86_instr::setcc, cond, dst_reg);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::andL, (uint32_t)0xFF, dst_reg);
   theEmitter->emit(i);
}

static void gen64BitImmRelOp(opCode op, Register src, RegContents src2imm, 
                             Register dst, tempBufferEmitter *theEmitter)
{
   // Same as genImmRelOp above, but treat pseudoregs as 64-bit mem locations

   bool negate = false;
   x86_instr::CondCodes cond;
   switch(op) {
   case lessOp:
      cond = x86_instr::LessThan;
      break;
   case leOp:
      cond = x86_instr::LessThanEqual;
      break;
   case greaterOp:
      cond = x86_instr::LessThanEqual;
      negate = true;
      break;
   case geOp:
      cond = x86_instr::LessThan;
      negate = true;
      break;
   case eqOp:
      cond = x86_instr::Equal;
      break;
   case neOp:
      cond = x86_instr::Equal;
      negate = true;
      break;
   default:
      assert(!"gen64BitImmRelOp - unsupported relative op\n");
      break;
   }

   instr_t *i;

#ifdef EXTRA_64BIT_SAVERESTORE
  // push EAX-EDI
   i = new x86_instr(x86_instr::push, x86_reg::eDI);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::push, x86_reg::eDX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::push, x86_reg::eCX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::push, x86_reg::eBX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::push, x86_reg::eAX);
   theEmitter->emit(i);
#endif
   
   // mov src to ECX:EBX
   i = new x86_instr(x86_instr::load, (int32_t)src, 
		     x86_reg::eBP, x86_reg::eBX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::load, (int32_t)src+4, 
		     x86_reg::eBP, x86_reg::eCX);
   theEmitter->emit(i);
   
   // mov 0:src2imm to EDX:EAX
   i = new x86_instr(x86_instr::mov, (uint32_t)src2imm, x86_reg::eAX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::mov, (uint32_t)0, x86_reg::eDX);
   theEmitter->emit(i);
   
   // do compare, set EDI to 1 if true, else 0
   pdstring true_label = getUniqueLabel();
   pdstring false_label = getUniqueLabel();
   pdstring done_label = getUniqueLabel();
   switch(cond) {
    case x86_instr::Equal: {
       i = new x86_instr(x86_instr::cmp, x86_reg::eBX, x86_reg::eAX);
       theEmitter->emit(i);
       i = new x86_instr(x86_instr::jcc, x86_instr::NotEqual, 0);
       theEmitter->emitCondBranchToLabel(i, false_label);
       i = new x86_instr(x86_instr::cmp, x86_reg::eCX, x86_reg::eDX);
       theEmitter->emit(i);
       i = new x86_instr(x86_instr::jcc, x86_instr::NotEqual, 0);
       theEmitter->emitCondBranchToLabel(i, false_label);
       i = new x86_instr(x86_instr::mov, (uint32_t)1, x86_reg::eDI);
       theEmitter->emit(i);
       i = new x86_instr(x86_instr::jmp, (int32_t)0);
       theEmitter->emitCondBranchToLabel(i, done_label);
       theEmitter->emitLabel(false_label);
       i = new x86_instr(x86_instr::mov, (uint32_t)0, x86_reg::eDI);
       theEmitter->emit(i);
       theEmitter->emitLabel(done_label);
       break;
    }
    case x86_instr::LessThan:
    case x86_instr::LessThanEqual: {
       i = new x86_instr(x86_instr::cmp, x86_reg::eCX, x86_reg::eDX);
       theEmitter->emit(i);
       i = new x86_instr(x86_instr::jcc, x86_instr::LessThan, 0);
       theEmitter->emitCondBranchToLabel(i, true_label);
       i = new x86_instr(x86_instr::cmp, x86_reg::eCX, x86_reg::eDX);
       theEmitter->emit(i);
       i = new x86_instr(x86_instr::jcc, x86_instr::NotEqual, 0);
       theEmitter->emitCondBranchToLabel(i, false_label);
       i = new x86_instr(x86_instr::cmp, x86_reg::eBX, x86_reg::eAX);
       theEmitter->emit(i);
       i = new x86_instr(x86_instr::jcc, cond, 0);
       theEmitter->emitCondBranchToLabel(i, true_label);
       theEmitter->emitLabel(false_label);
       i = new x86_instr(x86_instr::mov, (uint32_t)0, x86_reg::eDI);
       theEmitter->emit(i);
       i = new x86_instr(x86_instr::jmp, (int32_t)0);
       theEmitter->emitCondBranchToLabel(i, done_label);
       theEmitter->emitLabel(true_label);
       i = new x86_instr(x86_instr::mov, (uint32_t)1, x86_reg::eDI);
       theEmitter->emit(i);
       theEmitter->emitLabel(done_label);
       break;
    }
    default:
       assert(!"unhandled condition in emit-x86.C//gen64BitImmRelOp()\n");
       break;
   }
   if(negate) {
      i = new x86_instr(x86_instr::xorL, (uint32_t)0x00000001, x86_reg::eDI);
      theEmitter->emit(i);
   }

   // mov 0:EDI to dst
   i = new x86_instr(x86_instr::mov, x86_reg::eBP, x86_reg::eAX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::add, (uint32_t)dst, x86_reg::eAX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::store, x86_reg::eDI, x86_reg::eAX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::add, (uint32_t)4, x86_reg::eAX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::store, (uint32_t)0, x86_reg::eAX);
   theEmitter->emit(i);

#ifdef EXTRA_64BIT_SAVERESTORE
   // pop EAX-EDI
   i = new x86_instr(x86_instr::pop, x86_reg::eAX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::pop, x86_reg::eBX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::pop, x86_reg::eCX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::pop, x86_reg::eDX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::pop, x86_reg::eDI);
   theEmitter->emit(i);
#endif
}

// Emit code to compute a boolean expression, given two source registers
// The idea is the same as in genImmRelOp above, but the implementation
// is slightly different
static void genRelOp(opCode op, x86_reg src1_reg, x86_reg src2_reg, 
		     x86_reg dst_reg, tempBufferEmitter *theEmitter)
{
   // The following is correct even if src_reg == dst_reg
   instr_t *i = new x86_instr(x86_instr::cmp, src2_reg, src1_reg);
   theEmitter->emit(i);
   x86_instr::CondCodes cond;
   switch(op) {
   case lessOp:
      cond = x86_instr::LessThan;
      break;
   case leOp:
      cond = x86_instr::LessThanEqual;
      break;
   case greaterOp:
      cond = x86_instr::GreaterThan;
      break;
   case geOp:
      cond = x86_instr::GreaterThanEqual;
      break;
   case eqOp:
      cond = x86_instr::Equal;
      break;
   case neOp:
      cond = x86_instr::NotEqual;
      break;
   default:
      assert(!"genRelOp - unsupported relative op\n");
      break;
   }
   i = new x86_instr(x86_instr::setcc, cond, dst_reg);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::andL, (uint32_t)0xFF, dst_reg);
   theEmitter->emit(i);
}

static void gen64BitRelOp(opCode op, Register src1, Register src2, 
                          Register dst, tempBufferEmitter *theEmitter)
{
   // Same as genRelOp above, but treat pseudoregs as 64-bit mem locations

   bool negate = false;
   x86_instr::CondCodes cond;
   switch(op) {
   case lessOp:
      cond = x86_instr::LessThan;
      break;
   case leOp:
      cond = x86_instr::LessThanEqual;
      break;
   case greaterOp:
      cond = x86_instr::LessThanEqual;
      negate = true;
      break;
   case geOp:
      cond = x86_instr::LessThan;
      negate = true;
      break;
   case eqOp:
      cond = x86_instr::Equal;
      break;
   case neOp:
      cond = x86_instr::Equal;
      negate = true;
      break;
   default:
      assert(!"gen64BitRelOp - unsupported relative op\n");
      break;
   }

   instr_t *i;

#ifdef EXTRA_64BIT_SAVERESTORE
   // push EAX-EDI
   i = new x86_instr(x86_instr::push, x86_reg::eDI);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::push, x86_reg::eDX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::push, x86_reg::eCX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::push, x86_reg::eBX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::push, x86_reg::eAX);
   theEmitter->emit(i);
#endif
   
   // mov src1 to ECX:EBX
   i = new x86_instr(x86_instr::load, (int32_t)src1, 
		     x86_reg::eBP, x86_reg::eBX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::load, (int32_t)src1+4,
		     x86_reg::eBP, x86_reg::eCX);
   theEmitter->emit(i);

   // mov src2 to EDX:EAX
   i = new x86_instr(x86_instr::load, (int32_t)src2, 
		     x86_reg::eBP, x86_reg::eAX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::load, (int32_t)src2+4,
		     x86_reg::eBP, x86_reg::eDX);
   theEmitter->emit(i);

   // do compare, set EDI to 1 if true, else 0
   pdstring true_label = getUniqueLabel();
   pdstring false_label = getUniqueLabel();
   pdstring done_label = getUniqueLabel();
   switch(cond) {
    case x86_instr::Equal: {
       i = new x86_instr(x86_instr::cmp, x86_reg::eBX, x86_reg::eAX);
       theEmitter->emit(i);
       i = new x86_instr(x86_instr::jcc, x86_instr::NotEqual, 0);
       theEmitter->emitCondBranchToLabel(i, false_label);
       i = new x86_instr(x86_instr::cmp, x86_reg::eCX, x86_reg::eDX);
       theEmitter->emit(i);
       i = new x86_instr(x86_instr::jcc, x86_instr::NotEqual, 0);
       theEmitter->emitCondBranchToLabel(i, false_label);
       i = new x86_instr(x86_instr::mov, (uint32_t)1, x86_reg::eDI);
       theEmitter->emit(i);
       i = new x86_instr(x86_instr::jmp, (int32_t)0);
       theEmitter->emitCondBranchToLabel(i, done_label);
       theEmitter->emitLabel(false_label);
       i = new x86_instr(x86_instr::mov, (uint32_t)0, x86_reg::eDI);
       theEmitter->emit(i);
       theEmitter->emitLabel(done_label);
       break;
    }
    case x86_instr::LessThan:
    case x86_instr::LessThanEqual: {
       i = new x86_instr(x86_instr::cmp, x86_reg::eCX, x86_reg::eDX);
       theEmitter->emit(i);
       i = new x86_instr(x86_instr::jcc, x86_instr::LessThan, 0);
       theEmitter->emitCondBranchToLabel(i, true_label);
       i = new x86_instr(x86_instr::cmp, x86_reg::eCX, x86_reg::eDX);
       theEmitter->emit(i);
       i = new x86_instr(x86_instr::jcc, x86_instr::NotEqual, 0);
       theEmitter->emitCondBranchToLabel(i, false_label);
       i = new x86_instr(x86_instr::cmp, x86_reg::eBX, x86_reg::eAX);
       theEmitter->emit(i);
       i = new x86_instr(x86_instr::jcc, cond, 0);
       theEmitter->emitCondBranchToLabel(i, true_label);
       theEmitter->emitLabel(false_label);
       i = new x86_instr(x86_instr::mov, (uint32_t)0, x86_reg::eDI);
       theEmitter->emit(i);
       i = new x86_instr(x86_instr::jmp, (int32_t)0);
       theEmitter->emitCondBranchToLabel(i, done_label);
       theEmitter->emitLabel(true_label);
       i = new x86_instr(x86_instr::mov, (uint32_t)1, x86_reg::eDI);
       theEmitter->emit(i);
       theEmitter->emitLabel(done_label);
       break;
    }
    default:
       assert(!"unhandled condition in emit-x86.C//gen64BitImmRelOp()\n");
       break;
   }
   if(negate) {
      i = new x86_instr(x86_instr::notL, x86_reg::eDI);
      theEmitter->emit(i);
   }

   // mov 0:EDI to dst
   i = new x86_instr(x86_instr::mov, x86_reg::eBP, x86_reg::eAX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::add, (uint32_t)dst, x86_reg::eAX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::store, x86_reg::eDI, x86_reg::eAX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::add, (uint32_t)4, x86_reg::eAX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::store, (uint32_t)0, x86_reg::eAX);
   theEmitter->emit(i);

#ifdef EXTRA_64BIT_SAVERESTORE
   // pop EAX-EDI
   i = new x86_instr(x86_instr::pop, x86_reg::eAX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::pop, x86_reg::eBX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::pop, x86_reg::eCX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::pop, x86_reg::eDX);
   theEmitter->emit(i);
   i = new x86_instr(x86_instr::pop, x86_reg::eDI);
   theEmitter->emit(i);
#endif
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

   if (op == noOp) {
      i = new x86_instr(x86_instr::nop);
      theEmitter->emit(i);
      return;
   }
   else if (op == disableLocalInterrupts) {
      i = new x86_instr(x86_instr::pushfd);
      theEmitter->emit(i);
      i = new x86_instr(x86_instr::cli);
      theEmitter->emit(i);
      return;
   }
   else if (op == enableLocalInterrupts) {
      i = new x86_instr(x86_instr::popfd);
      theEmitter->emit(i);
      return;
   }
   
   if(!emit_64bit_code) { // 32-bit code, pseudoregs == archregs

      // Instructions with one source register
      x86_reg src1_reg(x86_reg::rawIntReg, src1);
      x86_reg dst_reg(x86_reg::rawIntReg, dst);
      if (op == movOp) {
         if(src1 != dst) {
            i = new x86_instr(x86_instr::mov, src1_reg, dst_reg);
            theEmitter->emit(i);
         }
         return;
      }
      else if (op == loadIndirOp) {
         emitVload(op, 0, src1, dst, insn, base, noCost, size);
         return;
      }
      else if (op == storeIndirOp) {
         emitVstore(op, src1, dst, 0, insn, base, noCost, size);
         return;
      }
      
      // Instructions with two source registers
      assert(src1 != src2);
      x86_reg src2_reg(x86_reg::rawIntReg, src2);
      if (op == plusOp || op == minusOp || op == timesOp || op == divOp ||
          op == andOp || op == orOp || op == shlOp || op == shrOp) {
         switch(op) {
         case plusOp:
            if(src1 == dst) {
               i = new x86_instr(x86_instr::add, src2_reg, dst_reg);
               theEmitter->emit(i);
            } 
            else if(src2 == dst) {
               i = new x86_instr(x86_instr::add, src1_reg, dst_reg);
               theEmitter->emit(i);
            }
            else {
               i = new x86_instr(x86_instr::mov, src1_reg, dst_reg);
               theEmitter->emit(i);
               i = new x86_instr(x86_instr::add, src2_reg, dst_reg);
               theEmitter->emit(i);
            }
            break;
         case minusOp:
            if(src1 == dst) {
               i = new x86_instr(x86_instr::sub, src2_reg, dst_reg);
               theEmitter->emit(i);
            } 
            else if(src2 == dst) {
               i = new x86_instr(x86_instr::neg, dst_reg);
               theEmitter->emit(i);
               i = new x86_instr(x86_instr::add, src1_reg, dst_reg);
               theEmitter->emit(i);
            }
            else {
               i = new x86_instr(x86_instr::mov, src1_reg, dst_reg);
               theEmitter->emit(i);
               i = new x86_instr(x86_instr::sub, src2_reg, dst_reg);
               theEmitter->emit(i);
            }
            break;
         case timesOp:
            if(src1 == dst) {
               i = new x86_instr(x86_instr::imul, src2_reg, dst_reg);
               theEmitter->emit(i);
            } 
            else if(src2 == dst) {
               i = new x86_instr(x86_instr::imul, src1_reg, dst_reg);
               theEmitter->emit(i);
            }
            else {
               i = new x86_instr(x86_instr::mov, src1_reg, dst_reg);
               theEmitter->emit(i);
               i = new x86_instr(x86_instr::imul, src2_reg, dst_reg);
               theEmitter->emit(i);
            }
            break;
         case divOp:
            // this one is complicated since src1/dest must be %eax for idiv
            // and %edx is used as the upper 32-bits of src1 (assumes it is 
            // 64-bits when using 32-bit divisor) and as the remainder

            // save some registers we're going to use
            if(dst_reg != x86_reg::eDX) {
               i = new x86_instr(x86_instr::push, x86_reg::eDX);
               theEmitter->emit(i);
            }
            if(dst_reg != x86_reg::eCX) {
               i = new x86_instr(x86_instr::push, x86_reg::eCX);
               theEmitter->emit(i);
            }
            if(dst_reg != x86_reg::eAX) {
               i = new x86_instr(x86_instr::push, x86_reg::eAX);
               theEmitter->emit(i);
            }
            
            // put pieces where we want them
            if(src1_reg != x86_reg::eAX) {
               if(src2_reg == x86_reg::eAX) {
                  i = new x86_instr(x86_instr::mov, src2_reg, x86_reg::eDX);
                  theEmitter->emit(i);
                  i = new x86_instr(x86_instr::mov, src1_reg, x86_reg::eAX);
                  theEmitter->emit(i);
                  i = new x86_instr(x86_instr::mov, x86_reg::eDX, x86_reg::eCX);
                  theEmitter->emit(i);
               }
               else {
                  i = new x86_instr(x86_instr::mov, src1_reg, x86_reg::eAX);
                  theEmitter->emit(i);
                  i = new x86_instr(x86_instr::mov, src2_reg, x86_reg::eCX);
                  theEmitter->emit(i);
               }
            }
            else if(src2_reg != x86_reg::eCX) {
               // src1 already in place, just worry about src2
               i = new x86_instr(x86_instr::mov, src2_reg, x86_reg::eCX);
               theEmitter->emit(i);
            }

            // set up signed upper 32-bits of src1
            i = new x86_instr(x86_instr::mov, (uint32_t)0, x86_reg::eDX);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::push, x86_reg::eBX);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::mov, (uint32_t)0xFFFFFFFF, x86_reg::eBX);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::test, (uint32_t)0x80000000, x86_reg::eAX);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::cmovcc, x86_instr::Sign, 
                              x86_reg::eBX, x86_reg::eDX);
            i = new x86_instr(x86_instr::pop, x86_reg::eBX);
            theEmitter->emit(i);

            // now do the idiv
            i = new x86_instr(x86_instr::idiv, x86_reg::eCX, x86_reg::eAX);
            theEmitter->emit(i);
            if(dst_reg != x86_reg::eAX) {
               i = new x86_instr(x86_instr::mov, x86_reg::eAX, dst_reg);
               theEmitter->emit(i);
            }
            
            // and don't forget to restore non-dest registers
            if(dst_reg != x86_reg::eAX) {
               i = new x86_instr(x86_instr::pop, x86_reg::eAX);
               theEmitter->emit(i);
            }
            if(dst_reg != x86_reg::eCX) {
               i = new x86_instr(x86_instr::pop, x86_reg::eCX);
               theEmitter->emit(i);
            }
            if(dst_reg != x86_reg::eDX) {
               i = new x86_instr(x86_instr::pop, x86_reg::eDX);
               theEmitter->emit(i);
            }
            break;
         case andOp:
            if(src1 == dst) {
               i = new x86_instr(x86_instr::andL, src2_reg, dst_reg);
               theEmitter->emit(i);
            } 
            else if(src2 == dst) {
               i = new x86_instr(x86_instr::andL, src1_reg, dst_reg);
               theEmitter->emit(i);
            }
            else {
               i = new x86_instr(x86_instr::mov, src1_reg, dst_reg);
               theEmitter->emit(i);
               i = new x86_instr(x86_instr::andL, src2_reg, dst_reg);
               theEmitter->emit(i);
            }
            break;
         case orOp:
            if(src1 == dst) {
               i = new x86_instr(x86_instr::orL, src2_reg, dst_reg);
               theEmitter->emit(i);
            } 
            else if(src2 == dst) {
               i = new x86_instr(x86_instr::orL, src1_reg, dst_reg);
               theEmitter->emit(i);
            }
            else {
               i = new x86_instr(x86_instr::mov, src1_reg, dst_reg);
               theEmitter->emit(i);
               i = new x86_instr(x86_instr::orL, src2_reg, dst_reg);
               theEmitter->emit(i);
            }
            break;
         case shlOp:
         case shrOp:
            assert(!"emitV() - shl/shr reg1, reg2 not supported on x86\n");
            break;
         default:
            assert(false && "Internal error");
         }
      }
      else if (op == greaterOp || op == geOp || op == lessOp || 
               op == leOp || op == neOp || op == eqOp) {
         genRelOp(op, src1_reg, src2_reg, dst_reg, theEmitter);
      }
      else {
         assert(false && "Unknown opcode in emitV");
      }
   }
   else { // 64-bit code, pseudoregs == mem locations

      // Instructions with one source register
      if (op == movOp) {
#ifdef EXTRA_64BIT_SAVERESTORE
         // save temp registers
         i = new x86_instr(x86_instr::push, x86_reg::eDX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::push, x86_reg::eBX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::push, x86_reg::eAX);
         theEmitter->emit(i);
#endif

         // do the move
	 i = new x86_instr(x86_instr::load, (int32_t)src1, 
			   x86_reg::eBP, x86_reg::eBX);
	 theEmitter->emit(i);
	 i = new x86_instr(x86_instr::load, (int32_t)src1+4, 
			   x86_reg::eBP, x86_reg::eDX);
	 theEmitter->emit(i);
         i = new x86_instr(x86_instr::store, x86_reg::eBX, (int32_t)dst, 
                           x86_reg::eBP);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::store, x86_reg::eDX, (int32_t)dst+4, 
                           x86_reg::eBP);
         theEmitter->emit(i);
	         
#ifdef EXTRA_64BIT_SAVERESTORE
         // restore temp registers
         i = new x86_instr(x86_instr::pop, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::pop, x86_reg::eBX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::pop, x86_reg::eDX);
         theEmitter->emit(i);
#endif
         return;
      }
      else if (op == loadIndirOp) {
         emitVload(op, 0, src1, dst, insn, base, noCost, size);
         return;
      }
      else if (op == storeIndirOp) {
         emitVstore(op, src1, dst, 0, insn, base, noCost, size);
         return;
      }
      
      // Instructions with two source registers
      if (op == plusOp || op == minusOp || op == timesOp || op == divOp ||
          op == andOp || op == orOp || op == shlOp || op == shrOp) {
         switch(op) {
         case plusOp:
         case minusOp:
         case andOp:
         case orOp:
#ifdef EXTRA_64BIT_SAVERESTORE
            // push EAX-EDX
            i = new x86_instr(x86_instr::push, x86_reg::eDX);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::push, x86_reg::eCX);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::push, x86_reg::eBX);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::push, x86_reg::eAX);
            theEmitter->emit(i);
#endif
            // mov src1 to ECX:EBX
	    i = new x86_instr(x86_instr::load, (int32_t)src1, 
			      x86_reg::eBP, x86_reg::eBX);
	    theEmitter->emit(i);
	    i = new x86_instr(x86_instr::load, (int32_t)src1+4, 
			      x86_reg::eBP, x86_reg::eCX);
	    theEmitter->emit(i);

            // mov src2 to EDX:EAX
	    i = new x86_instr(x86_instr::load, (int32_t)src2, 
			      x86_reg::eBP, x86_reg::eAX);
	    theEmitter->emit(i);
	    i = new x86_instr(x86_instr::load, (int32_t)src2+4, 
			      x86_reg::eBP, x86_reg::eDX);
	    theEmitter->emit(i);

            // do op
            if(op == plusOp) {
               i = new x86_instr(x86_instr::add, x86_reg::eAX, x86_reg::eBX);
               theEmitter->emit(i);
               i = new x86_instr(x86_instr::adc, x86_reg::eDX, x86_reg::eCX);
               theEmitter->emit(i);
            }
            else if(op == minusOp) {
               i = new x86_instr(x86_instr::sub, x86_reg::eAX, x86_reg::eBX);
               theEmitter->emit(i);
               i = new x86_instr(x86_instr::sbb, x86_reg::eDX, x86_reg::eCX);
               theEmitter->emit(i);
            }
            else if(op == andOp) {
               i = new x86_instr(x86_instr::andL, x86_reg::eAX, x86_reg::eBX);
               theEmitter->emit(i);
               i = new x86_instr(x86_instr::andL, x86_reg::eDX, x86_reg::eCX);
               theEmitter->emit(i);
            }
            else if(op == orOp) {
               i = new x86_instr(x86_instr::orL, x86_reg::eAX, x86_reg::eBX);
               theEmitter->emit(i);
               i = new x86_instr(x86_instr::orL, x86_reg::eDX, x86_reg::eCX);
               theEmitter->emit(i);
            }
            // mov ECX:EBX to dst
            i = new x86_instr(x86_instr::store, x86_reg::eBX, (int32_t)dst, 
                              x86_reg::eBP);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::store, x86_reg::eCX, (int32_t)dst+4, 
                              x86_reg::eBP);
            theEmitter->emit(i);

#ifdef EXTRA_64BIT_SAVERESTORE
            // pop EAX-EDX
            i = new x86_instr(x86_instr::pop, x86_reg::eAX);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::pop, x86_reg::eBX);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::pop, x86_reg::eCX);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::pop, x86_reg::eDX);
            theEmitter->emit(i);
#endif
            break;
         case timesOp:
         case divOp:
            assert(!"emitV() - 64-bit mult/div not yet supported\n");
            break;
         case shlOp:
         case shrOp:
            assert(!"emitV() - shl/shr reg1, reg2 not supported on x86\n");
            break;
         default:
            assert(false && "Internal error - impossible case");
         }
      }
      else if (op == greaterOp || op == geOp || op == lessOp || 
               op == leOp || op == neOp || op == eqOp) {
         gen64BitRelOp(op, src1, src2, dst, theEmitter);
      }
      else {
         assert(false && "Unknown opcode in emitV");
      }
   }
}

// for loadOp and loadConstOp (reading from an HostAddress)
void emitVload(opCode op, HostAddress src1, Register src2, Register dst, 
	       char *insn, HostAddress&/*base*/, bool /*noCost*/, int /*size*/)
{
//      cout << "emitVload(" << op << ", 0x" << hex << src1 << dec << ", Reg" 
//  	 << src2 << ", Reg" << dst << ", " << hex << base << dec << ")\n";

   tempBufferEmitter *theEmitter = (tempBufferEmitter *)insn;
   assert(theEmitter != 0);
    
   instr_t *i;

   if(!emit_64bit_code) { // 32-bit code, pseudoregs == archregs
 
      x86_reg dst_reg(x86_reg::rawIntReg, dst);

      switch(op) {
      case loadConstOp:
         theEmitter->emitImmAddr(src1, dst_reg);
         break;
      case loadIndirOp: {
         x86_reg addr_reg(x86_reg::rawIntReg, src2);
         i = new x86_instr(x86_instr::load, addr_reg, dst_reg);
         theEmitter->emit(i);
         break;
      }
      case loadOp: {
         theEmitter->emitImmAddr(src1, dst_reg);
         i = new x86_instr(x86_instr::load, dst_reg, dst_reg);
         theEmitter->emit(i);
         break;
      }
      case loadAndSetAddrOp: {
         x86_reg addr_reg(x86_reg::rawIntReg, src2);
         theEmitter->emitImmAddr(src1, addr_reg);
         i = new x86_instr(x86_instr::load, addr_reg, dst_reg);
         theEmitter->emit(i);
         break;
      }
      case loadCPUId: {
         assert(theGlobalKerninstdMachineInfo != 0);
         unsigned thr_cpu_offset = 
            theGlobalKerninstdMachineInfo->getKernelThreadCpuOffset();
         unsigned thr_size = 
            theGlobalKerninstdMachineInfo->getKernelThreadSize();
         /* Getting current->cpu; current can be obtained by:
            
                 movl $-thr_size, %reg
                 andl %esp, %reg

            which puts the current pointer in %reg. then we add the offset
            of the processor field:

                 addl thr_cpu_offset, %reg

            so that we have the address of current_thread->cpu in %reg. 
            finally, we actually grab its value and store in dst_reg

                 movl [%reg], %dst_reg
         */
         i = new x86_instr(x86_instr::mov, -thr_size, dst_reg);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::andL, x86_reg::eSP, dst_reg);
         theEmitter->emit(i);
         if(thr_cpu_offset) {
            i = new x86_instr(x86_instr::add, thr_cpu_offset, dst_reg);
            theEmitter->emit(i);
         }
         i = new x86_instr(x86_instr::load, dst_reg, dst_reg);
         theEmitter->emit(i);
         break;
      }
      case loadProcessId: {
         assert(theGlobalKerninstdMachineInfo != 0);
         unsigned task_pid_offset = 
            theGlobalKerninstdMachineInfo->getKernelProcPidOffset();
         unsigned thr_size = 
            theGlobalKerninstdMachineInfo->getKernelThreadSize();
         unsigned thr_task_offset = 
            theGlobalKerninstdMachineInfo->getKernelThreadProcOffset();
         unsigned os_version =
            theGlobalKerninstdMachineInfo->getOSVersion();
         
         /* Getting current_task->pid; current can be obtained by:

                 movl $-thr_size, %reg
                 andl %esp, %reg 

            On Linux 2.4.x kernels, this puts the address of the current 
            task in %reg.
 
            On Linux 2.6.x kernels, this puts the address of current thread 
            in %reg, so we need an extra step to get the address of the
            current task, which has the pid member. so we do the following 
            to put the address of the current task in %reg:

                 addl thr_task_offset, %reg
                 movl [%reg], %reg 

            Next, we add the offset of the pid field so that we have the 
            address of current_task->pid in %reg:

                 addl task_pid_offset, %reg

            Finally, we actually grab its value and store in dst_reg

                 movl [%reg], %dst_reg 
         */
         i = new x86_instr(x86_instr::mov, -thr_size, dst_reg);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::andL, x86_reg::eSP, dst_reg);
         theEmitter->emit(i);
         if(os_version == 26) { // Linux 2.6.x
            if(thr_task_offset) {
               i = new x86_instr(x86_instr::add, thr_task_offset, dst_reg);
               theEmitter->emit(i);
            }
            i = new x86_instr(x86_instr::load, dst_reg, dst_reg);
            theEmitter->emit(i);
         }
         if(task_pid_offset) {
            i = new x86_instr(x86_instr::add, task_pid_offset, dst_reg);
            theEmitter->emit(i);
         }
         i = new x86_instr(x86_instr::load, dst_reg, dst_reg);
         theEmitter->emit(i);
         break;
      }
      case loadHwCtrReg:
      case loadFrameAddr:
      case loadFrameRelativeOp:
         assert(false && "emitVload - case not yet implemented");
      default:
         assert(false && "Unknown opcode in emitVload");
      }
   }
   else { // 64-bit code, pseudoregs == mem locations

      switch(op) {
      case loadConstOp:
         // Even though this is 64-bit code, we assume a 32-bit constant
#ifdef EXTRA_64BIT_SAVERESTORE
         i = new x86_instr(x86_instr::push, x86_reg::eAX);
         theEmitter->emit(i);
#endif
	 i = new x86_instr(x86_instr::mov, x86_reg::eBP, x86_reg::eAX);
	 theEmitter->emit(i);
         i = new x86_instr(x86_instr::add, (uint32_t)dst, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::store, (uint32_t)src1, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::add, (uint32_t)4, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::store, (uint32_t)0, x86_reg::eAX);
         theEmitter->emit(i);
#ifdef EXTRA_64BIT_SAVERESTORE
         i = new x86_instr(x86_instr::pop, x86_reg::eAX);
         theEmitter->emit(i);
#endif
         break;
      case loadIndirOp: {
#ifdef EXTRA_64BIT_SAVERESTORE
         // save temp regs
         i = new x86_instr(x86_instr::push, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::push, x86_reg::eCX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::push, x86_reg::eDX);
         theEmitter->emit(i);
#endif

         // this is tricky: the pseudoreg src2 holds the address to load from,
         // so we load the address it contains into a real reg
	 i = new x86_instr(x86_instr::load, (int32_t)src2, 
			   x86_reg::eBP, x86_reg::eAX);
	 theEmitter->emit(i);

         // now we need to get the data @ [addr_reg]
         i = new x86_instr(x86_instr::load, x86_reg::eAX, x86_reg::eCX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::add, (uint32_t)4, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::load, x86_reg::eAX, x86_reg::eDX);
         theEmitter->emit(i);

         // and finally we load the data into pseudoreg dst
         i = new x86_instr(x86_instr::store, x86_reg::eCX, (int32_t)dst, 
                           x86_reg::eBP);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::store, x86_reg::eDX, (int32_t)dst+4, 
                           x86_reg::eBP);
         theEmitter->emit(i);

#ifdef EXTRA_64BIT_SAVERESTORE
         // restore temp regs
         i = new x86_instr(x86_instr::pop, x86_reg::eDX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::pop, x86_reg::eCX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::pop, x86_reg::eAX);
         theEmitter->emit(i);
#endif
         break;
      }
      case loadOp: {
#ifdef EXTRA_64BIT_SAVERESTORE
         // save temp regs
         i = new x86_instr(x86_instr::push, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::push, x86_reg::eCX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::push, x86_reg::eDX);
         theEmitter->emit(i);
#endif
         
         // load the memory at address src1 into pseudoreg dst
         i = new x86_instr(x86_instr::mov, (uint32_t)src1, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::load, x86_reg::eAX, x86_reg::eCX);
	 theEmitter->emit(i);
	 i = new x86_instr(x86_instr::add, (uint32_t)4, x86_reg::eAX); 
	 theEmitter->emit(i);
	 i = new x86_instr(x86_instr::load, x86_reg::eAX, x86_reg::eDX);
	 theEmitter->emit(i);
	 
         i = new x86_instr(x86_instr::store, x86_reg::eCX, (int32_t)dst, 
                           x86_reg::eBP);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::store, x86_reg::eDX, (int32_t)dst+4, 
                           x86_reg::eBP);
         theEmitter->emit(i);

#ifdef EXTRA_64BIT_SAVERESTORE
         // restore temp regs
         i = new x86_instr(x86_instr::pop, x86_reg::eDX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::pop, x86_reg::eCX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::pop, x86_reg::eAX);
         theEmitter->emit(i);
#endif
         break;
      }
      case loadAndSetAddrOp: {
#ifdef EXTRA_64BIT_SAVERESTORE
         // save temp regs
         i = new x86_instr(x86_instr::push, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::push, x86_reg::eCX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::push, x86_reg::eDX);
         theEmitter->emit(i);
#endif
         
         // store the address src1 in pseudoreg src2
	 i = new x86_instr(x86_instr::mov, x86_reg::eBP, x86_reg::eAX);
	 theEmitter->emit(i);
         i = new x86_instr(x86_instr::add, (uint32_t)src2, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::store, (uint32_t)src1, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::add, (uint32_t)4, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::store, (uint32_t)0, x86_reg::eAX);
         theEmitter->emit(i);
         
         // load the memory at address src1 into pseudoreg dst
	 i = new x86_instr(x86_instr::mov, (uint32_t)src1, x86_reg::eAX);
	 theEmitter->emit(i);
         i = new x86_instr(x86_instr::load, x86_reg::eAX, x86_reg::eCX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::add, (uint32_t)4, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::load, x86_reg::eAX, x86_reg::eDX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::store, x86_reg::eCX, (int32_t)dst, 
                           x86_reg::eBP);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::store, x86_reg::eDX, (int32_t)dst+4, 
                           x86_reg::eBP);
         theEmitter->emit(i);

#ifdef EXTRA_64BIT_SAVERESTORE
         // restore temp regs
         i = new x86_instr(x86_instr::pop, x86_reg::eDX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::pop, x86_reg::eCX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::pop, x86_reg::eAX);
         theEmitter->emit(i);
#endif
         break;
      }
      case loadCPUId: {

         // See description in 32-bit case above for details

	 assert(theGlobalKerninstdMachineInfo != 0);
         unsigned thr_cpu_offset = 
            theGlobalKerninstdMachineInfo->getKernelThreadCpuOffset();
         unsigned thr_size =
            theGlobalKerninstdMachineInfo->getKernelThreadSize();

#ifdef EXTRA_64BIT_SAVERESTORE
         // save temp regs
         i = new x86_instr(x86_instr::push, x86_reg::eAX);
         theEmitter->emit(i);
	 i = new x86_instr(x86_instr::push, x86_reg::eCX);
         theEmitter->emit(i);
#endif

         // Get the processor id
	 i = new x86_instr(x86_instr::mov, -thr_size, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::andL, x86_reg::eSP, x86_reg::eAX);
         theEmitter->emit(i);
         if(thr_cpu_offset) {
            i = new x86_instr(x86_instr::add, thr_cpu_offset, x86_reg::eAX);
            theEmitter->emit(i);
         }
         i = new x86_instr(x86_instr::load, x86_reg::eAX, x86_reg::eAX);
         theEmitter->emit(i);

         // store it in pseudoreg dst
         i = new x86_instr(x86_instr::mov, x86_reg::eBP, x86_reg::eCX);
	 theEmitter->emit(i);
         i = new x86_instr(x86_instr::add, (uint32_t)dst, x86_reg::eCX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::store, x86_reg::eAX, x86_reg::eCX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::add, (uint32_t)4, x86_reg::eCX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::store, (uint32_t)0, x86_reg::eCX);
         theEmitter->emit(i);

#ifdef EXTRA_64BIT_SAVERESTORE
         // restore tmp regs
         i = new x86_instr(x86_instr::pop, x86_reg::eCX);
         theEmitter->emit(i);
	 i = new x86_instr(x86_instr::pop, x86_reg::eAX);
         theEmitter->emit(i);
#endif
         break;
      }
      case loadProcessId: {

         // See description in 32-bit case above for details

         assert(theGlobalKerninstdMachineInfo != 0);
         unsigned thr_task_offset = 
            theGlobalKerninstdMachineInfo->getKernelThreadProcOffset();
         unsigned task_pid_offset = 
            theGlobalKerninstdMachineInfo->getKernelProcPidOffset();
         unsigned thr_size =
            theGlobalKerninstdMachineInfo->getKernelThreadSize();
         unsigned os_version =
            theGlobalKerninstdMachineInfo->getOSVersion();
        
#ifdef EXTRA_64BIT_SAVERESTORE
         // save temp regs
         i = new x86_instr(x86_instr::push, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::push, x86_reg::eCX);
         theEmitter->emit(i);
#endif
         
         // get the pid
         i = new x86_instr(x86_instr::mov, -thr_size, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::andL, x86_reg::eSP, x86_reg::eAX);
         theEmitter->emit(i);
         if(os_version == 26) { // Linux 2.6.x
            if(thr_task_offset) {
               i = new x86_instr(x86_instr::add, thr_task_offset, x86_reg::eAX);
               theEmitter->emit(i);
            }
            i = new x86_instr(x86_instr::load, x86_reg::eAX, x86_reg::eAX);
            theEmitter->emit(i);
         }
         if(task_pid_offset) {
            i = new x86_instr(x86_instr::add, task_pid_offset, x86_reg::eAX);
            theEmitter->emit(i);
         }
         i = new x86_instr(x86_instr::load, x86_reg::eAX, x86_reg::eCX);
         theEmitter->emit(i);

         // store it in pseudoreg dst
         i = new x86_instr(x86_instr::mov, x86_reg::eBP, x86_reg::eAX);
	 theEmitter->emit(i);
         i = new x86_instr(x86_instr::add, (uint32_t)dst, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::store, x86_reg::eCX, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::add, (uint32_t)4, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::store, (uint32_t)0, x86_reg::eAX);
         theEmitter->emit(i);

#ifdef EXTRA_64BIT_SAVERESTORE
         // restore tmp regs
         i = new x86_instr(x86_instr::pop, x86_reg::eCX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::pop, x86_reg::eAX);
         theEmitter->emit(i);
#endif
         break;
      }
      case loadHwCtrReg: {
#ifdef EXTRA_64BIT_SAVERESTORE
         // save temp regs
         i = new x86_instr(x86_instr::push, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::push, x86_reg::eCX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::push, x86_reg::eDX);
         theEmitter->emit(i);
#endif
         kapi_hwcounter_kind kind = (kapi_hwcounter_kind)(src1 & 0x0000ffff);
         if (kind == HWC_TICKS) {
            // read TSC into EDX:EAX
            i = new x86_instr(x86_instr::rdtsc);
            theEmitter->emit(i);
         } 
         else {
            // set ECX to current counter for kind (hi 16 bits of src)
            uint32_t pmc_num = (src1 & 0xffff0000) >> 16;
            i = new x86_instr(x86_instr::mov, pmc_num, x86_reg::eCX);
            theEmitter->emit(i);

            // read performance counter into EDX:EAX
            i = new x86_instr(x86_instr::rdpmc);
            theEmitter->emit(i);
         }

         // store contents in dst
         i = new x86_instr(x86_instr::store, x86_reg::eAX, (int32_t)dst, 
                           x86_reg::eBP);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::store, x86_reg::eDX, (int32_t)dst+4, 
                           x86_reg::eBP);
         theEmitter->emit(i);

#ifdef EXTRA_64BIT_SAVERESTORE
         // restore temp regs
         i = new x86_instr(x86_instr::pop, x86_reg::eDX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::pop, x86_reg::eCX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::pop, x86_reg::eAX);
         theEmitter->emit(i);
#endif
         break;
      }
      case loadFrameAddr:
      case loadFrameRelativeOp:
         assert(false && "emitVload - case not yet implemented");
      default:
         assert(false && "Unknown opcode in emitVload");
      }
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

   instr_t *i;

   if(!emit_64bit_code) { // 32-bit code, pseudoregs == archregs
      x86_reg val_reg(x86_reg::rawIntReg, val);
      x86_reg addr_reg(x86_reg::rawIntReg, scratch);

      switch(op) {
      case storeOp: {
         // store contents of val_reg to addr dst
         theEmitter->emitImmAddr(dst, addr_reg);
         i = new x86_instr(x86_instr::store, val_reg, addr_reg);
         theEmitter->emit(i);
         break;
      }
      case storeIndirOp: {
         // store contents of val_reg to addr held in addr_reg
         i = new x86_instr(x86_instr::store, val_reg, addr_reg);
         theEmitter->emit(i);
         break;
      }
      default:
         assert(false && "Unknown opcode in emitVstore");
      }
   }
   else { // 64-bit code, pseudoregs == mem locations
      switch(op) {
      case storeOp: {
         // store contents of val pseudoreg to addr dst

#ifdef EXTRA_64BIT_SAVERESTORE
         // save tmp regs
         i = new x86_instr(x86_instr::push, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::push, x86_reg::eCX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::push, x86_reg::eDX);
         theEmitter->emit(i);
#endif

         // get val contents into EDX:ECX
	 i = new x86_instr(x86_instr::load, (int32_t)val, 
			   x86_reg::eBP, x86_reg::eCX);
	 theEmitter->emit(i);
	 i = new x86_instr(x86_instr::load, (int32_t)val+4, 
			   x86_reg::eBP, x86_reg::eDX);
	 theEmitter->emit(i);
	 
         // store EDX:ECX at addr dst
         i = new x86_instr(x86_instr::mov, (uint32_t)dst, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::store, x86_reg::eCX, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::add, (uint32_t)4, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::store, x86_reg::eDX, x86_reg::eAX);
         theEmitter->emit(i);

#ifdef EXTRA_64BIT_SAVERESTORE
         // restore tmp regs
         i = new x86_instr(x86_instr::pop, x86_reg::eDX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::pop, x86_reg::eCX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::pop, x86_reg::eAX);
         theEmitter->emit(i);
#endif
         break;
      }
      case storeIndirOp: {
         // store contents of val pseudoreg to addr held in scratch pseudoreg

#ifdef EXTRA_64BIT_SAVERESTORE
         // save tmp regs
         i = new x86_instr(x86_instr::push, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::push, x86_reg::eCX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::push, x86_reg::eDX);
         theEmitter->emit(i);
#endif

         // get val contents into EDX:ECX
	 i = new x86_instr(x86_instr::load, (int32_t)val, 
			   x86_reg::eBP, x86_reg::eCX);
	 theEmitter->emit(i);
	 i = new x86_instr(x86_instr::load, (int32_t)val+4, 
			   x86_reg::eBP, x86_reg::eDX);
	 theEmitter->emit(i);

         // store EDX:ECX at [scratch]
	 i = new x86_instr(x86_instr::load, (int32_t)scratch, 
			   x86_reg::eBP, x86_reg::eAX);
	 theEmitter->emit(i);
         i = new x86_instr(x86_instr::store, x86_reg::eCX, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::add, (uint32_t)4, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::store, x86_reg::eDX, x86_reg::eAX);
         theEmitter->emit(i);

#ifdef EXTRA_64BIT_SAVERESTORE
         // restore tmp regs
         i = new x86_instr(x86_instr::pop, x86_reg::eDX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::pop, x86_reg::eCX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::pop, x86_reg::eAX);
         theEmitter->emit(i);
#endif
         break;
      }
      default:
         assert(false && "Unknown opcode in emitVstore");
      }
   }
}

void emitImm(opCode op, Register src, RegContents src2imm, Register dst, 
	     char *insn, HostAddress &/*base*/, bool /*noCost*/)
{
//      cout << "emitImm(" << op << ", Reg" << src << ", " 
//  	 << src2imm << ", Reg" << dst << ", " << hex << base << dec << ")\n";
   tempBufferEmitter *theEmitter = (tempBufferEmitter *)insn;
   assert(theEmitter != 0);

   instr_t *i;

   if(!emit_64bit_code) { // 32-bit code, psuedoregs == archregs

      x86_reg src_reg(x86_reg::rawIntReg, src);
      x86_reg dst_reg(x86_reg::rawIntReg, dst);

      switch (op) {
      case plusOp:
         if(src != dst) {
            i = new x86_instr(x86_instr::mov, src_reg, dst_reg);
            theEmitter->emit(i);
         }
         i = new x86_instr(x86_instr::add, src2imm, dst_reg); // dst += src2imm
         theEmitter->emit(i);
         break;
      case minusOp:
         if(src != dst) {
            i = new x86_instr(x86_instr::mov, src_reg, dst_reg);
            theEmitter->emit(i);
         }
         i = new x86_instr(x86_instr::sub, src2imm, dst_reg); // dst -= src2imm
         theEmitter->emit(i);
         break;
      case timesOp:
         i = new x86_instr(x86_instr::imul, src_reg, src2imm, dst_reg);
         theEmitter->emit(i);
         break;
      case divOp:
         // this one is complicated since src1/dest must be %eax for idiv
         // and %edx is used as the upper 32-bits of src1 (assumes it is 
         // 64-bits when using 32-bit divisor) and as the remainder

         // save some registers we're going to use
         if(dst_reg != x86_reg::eDX) {
            i = new x86_instr(x86_instr::push, x86_reg::eDX);
            theEmitter->emit(i);
         }
         if(dst_reg != x86_reg::eCX) {
            i = new x86_instr(x86_instr::push, x86_reg::eCX);
            theEmitter->emit(i);
         }
         if(dst_reg != x86_reg::eAX) {
            i = new x86_instr(x86_instr::push, x86_reg::eAX);
            theEmitter->emit(i);
         }
            
         // put pieces where we want them
         if(src_reg != x86_reg::eAX) {
            i = new x86_instr(x86_instr::mov, src_reg, x86_reg::eAX);
            theEmitter->emit(i);
         }
         i = new x86_instr(x86_instr::mov, src2imm, x86_reg::eCX);
         theEmitter->emit(i);

         // set up signed upper 32-bits of src1
         i = new x86_instr(x86_instr::mov, (uint32_t)0, x86_reg::eDX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::push, x86_reg::eBX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::mov, (uint32_t)0xFFFFFFFF, x86_reg::eBX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::test, (uint32_t)0x80000000, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::cmovcc, x86_instr::Sign, 
                           x86_reg::eBX, x86_reg::eDX);
         i = new x86_instr(x86_instr::pop, x86_reg::eBX);
         theEmitter->emit(i);

         // now do the idiv
         i = new x86_instr(x86_instr::idiv, x86_reg::eCX, x86_reg::eAX);
         theEmitter->emit(i);
         if(dst_reg != x86_reg::eAX) {
            i = new x86_instr(x86_instr::mov, x86_reg::eAX, dst_reg);
            theEmitter->emit(i);
         }
            
         // and don't forget to restore non-dest registers
         if(dst_reg != x86_reg::eAX) {
            i = new x86_instr(x86_instr::pop, x86_reg::eAX);
            theEmitter->emit(i);
         }
         if(dst_reg != x86_reg::eCX) {
            i = new x86_instr(x86_instr::pop, x86_reg::eCX);
            theEmitter->emit(i);
         }
         if(dst_reg != x86_reg::eDX) {
            i = new x86_instr(x86_instr::pop, x86_reg::eDX);
            theEmitter->emit(i);
         }
         break;
      case orOp:
         if(src != dst) {
            i = new x86_instr(x86_instr::mov, src_reg, dst_reg);
            theEmitter->emit(i);
         }
         i = new x86_instr(x86_instr::orL, src2imm, dst_reg); // dst |= src2imm
         theEmitter->emit(i);
         break;
      case andOp:
         if(src != dst) {
            i = new x86_instr(x86_instr::mov, src_reg, dst_reg);
            theEmitter->emit(i);
         }
         i = new x86_instr(x86_instr::andL, src2imm, dst_reg); // dst &= src2imm
         theEmitter->emit(i);
         break;
      case shlOp:
         if(src != dst) {
            i = new x86_instr(x86_instr::mov, src_reg, dst_reg);
            theEmitter->emit(i);
         }
         i = new x86_instr(x86_instr::shl, src2imm, dst_reg); // dst <<= src2imm
         theEmitter->emit(i);
         break;
      case shrOp:
         if(src != dst) {
            i = new x86_instr(x86_instr::mov, src_reg, dst_reg);
            theEmitter->emit(i);
         }
         i = new x86_instr(x86_instr::shr, src2imm, dst_reg); // dst >>= src2imm
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
   else { // 64-bit code, pseudoregs == mem locations
      switch (op) {
      case plusOp:
      case minusOp:
      case andOp:
      case orOp:
      case shlOp:
      case shrOp:
#ifdef EXTRA_64BIT_SAVERESTORE
         // save temp regs
         i = new x86_instr(x86_instr::push, x86_reg::eAX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::push, x86_reg::eCX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::push, x86_reg::eDX);
         theEmitter->emit(i);
#endif

         // get src contents into EDX:ECX
	 i = new x86_instr(x86_instr::load, (int32_t)src, 
			   x86_reg::eBP, x86_reg::eCX);
	 theEmitter->emit(i);
	 i = new x86_instr(x86_instr::load, (int32_t)src+4, 
			   x86_reg::eBP, x86_reg::eDX);
	 theEmitter->emit(i);

         // op src2imm to EDX:ECX
         if(op == plusOp) {
            i = new x86_instr(x86_instr::add, src2imm, x86_reg::eCX);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::adc, (uint32_t)0, x86_reg::eDX);
            theEmitter->emit(i);
         }
         else if(op == minusOp) {
            i = new x86_instr(x86_instr::sub, src2imm, x86_reg::eCX);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::sbb, (uint32_t)0, x86_reg::eDX);
            theEmitter->emit(i);
         }
         else if(op == andOp) {
            i = new x86_instr(x86_instr::andL, src2imm, x86_reg::eCX);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::andL, (uint32_t)0, x86_reg::eDX);
            theEmitter->emit(i);
         }
         else if(op == orOp) {
            i = new x86_instr(x86_instr::orL, src2imm, x86_reg::eCX);
            theEmitter->emit(i);
            i = new x86_instr(x86_instr::orL, (uint32_t)0, x86_reg::eDX);
            theEmitter->emit(i);
         }
         else if(op == shlOp) {
            if(src2imm < 32) {
               i = new x86_instr(x86_instr::shld, src2imm, x86_reg::eCX, 
                                 x86_reg::eDX);
               theEmitter->emit(i);
               i = new x86_instr(x86_instr::shl, src2imm, x86_reg::eCX);
               theEmitter->emit(i);
            }
            else {
               i = new x86_instr(x86_instr::mov, x86_reg::eCX, x86_reg::eDX);
               theEmitter->emit(i);
               i = new x86_instr(x86_instr::mov, (uint32_t)0, x86_reg::eCX);
               theEmitter->emit(i);
               i = new x86_instr(x86_instr::shl, src2imm-32, x86_reg::eDX);
               theEmitter->emit(i);
            }
         }
         else if(op == shrOp) {
            if(src2imm < 32) {
               i = new x86_instr(x86_instr::shrd, src2imm, x86_reg::eDX,
                                 x86_reg::eCX);
               theEmitter->emit(i);
               i = new x86_instr(x86_instr::shr, src2imm, x86_reg::eDX);
               theEmitter->emit(i);
            }
            else {
               i = new x86_instr(x86_instr::mov, x86_reg::eDX, x86_reg::eCX);
               theEmitter->emit(i);
               i = new x86_instr(x86_instr::mov, (uint32_t)0, x86_reg::eDX);
               theEmitter->emit(i);
               i = new x86_instr(x86_instr::shr, src2imm-32, x86_reg::eCX);
               theEmitter->emit(i);
            }
         }

         // store EDX:ECX in dst
         i = new x86_instr(x86_instr::store, x86_reg::eCX, (int32_t)dst, 
                           x86_reg::eBP);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::store, x86_reg::eDX, (int32_t)dst+4, 
                           x86_reg::eBP);
         theEmitter->emit(i);

#ifdef EXTRA_64BIT_SAVERESTORE
         // restore tmp regs
         i = new x86_instr(x86_instr::pop, x86_reg::eDX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::pop, x86_reg::eCX);
         theEmitter->emit(i);
         i = new x86_instr(x86_instr::pop, x86_reg::eAX);
         theEmitter->emit(i);
#endif
         break;
      case timesOp:
      case divOp:
         assert(!"emitImm - 64-bit mult/div of imm32 not yet supported on x86\n");
         break;
      case greaterOp:
      case geOp:
      case lessOp:
      case leOp:
      case neOp:
      case eqOp:
         gen64BitImmRelOp(op, src, src2imm, dst, theEmitter);
         break;
      default:
         assert(false && "Unknown opcode in emitImm");
      }
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

   tempBufferEmitter *em = (tempBufferEmitter *)insn;   
   instr_t *i;
   unsigned num_pushes = 0;

   if(operands.size()) {
      // Push operands onto the stack from right to left
      pdvector<AstNode *>::const_iterator iarg = operands.end();
      do {
         iarg--;
         AstNode *arg = *iarg;
         Register reg = arg->generateCode_phase2(proc, rs, insn, base,
                                                 noCost, ifForks, location);
         x86_reg src_reg(x86_reg::rawIntReg, reg);
         i = new x86_instr(x86_instr::push, src_reg);
         em->emit(i);
         rs->freeRegister(reg);
         num_pushes++;
      } while(iarg != operands.begin());
   }

   // Emit the actual call
   em->emitCall(calleeAddr);

   // Move the return value into the dest reg
   x86_reg dst_reg(x86_reg::rawIntReg, dest);
   if (dst_reg != x86_reg::eAX) {
      i = new x86_instr(x86_instr::mov, x86_reg::eAX, dst_reg);
      em->emit(i);
   }

   if(operands.size()) {
      // Free stack space allocated by pushing args above
      i = new x86_instr(x86_instr::add, num_pushes*4, x86_reg::eSP);
      em->emit(i);
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

bool doNotOverflow(int /*value*/)
{
   // we can use the whole 32-bit int as a displacement on x86
   return true;
}

void emitLoadPreviousStackFrameRegister(Register srcPrevFrame,
					Register destThisFrame,
					char *insn,
					HostAddress &/*base*/,
					int /*size*/,
					bool /*noCost*/)
{
   tempBufferEmitter *em = (tempBufferEmitter*)insn;
   instr_t *i;
   signed char reg_disp8 = 32 - ((signed char)srcPrevFrame * sizeof(kptr_t));
      // 32 is the offset to get to %EAX (Register==0), the first pushed reg

   if(!emit_64bit_code) { // 32-bit code, psuedoregs == archregs
      x86_reg dst_reg(x86_reg::rawIntReg, destThisFrame);
      // The value of %ESP will point to the top of the saved regs
      // when we're beginning to emit code for this AST expression, since 
      // whenever we directly manipulate the stack in emitted code, it is 
      // with matched push/pop pairs. Therefore, we just need to grab the 
      // saved register and move it to destThisFrame.
      i = new x86_instr(x86_instr::mov, x86_reg::eSP, dst_reg);
      em->emit(i);
      i = new x86_instr(x86_instr::load, reg_disp8, dst_reg, dst_reg);
      em->emit(i);
   }
   else { // 64-bit code, psuedoregs == mem locations
      // We always set up our own stack frame for 64-bit code, and %EBP
      // thus points to the top of the saved regs (more specifically, it
      // points to the saved EFLAGS reg). Therefore, we just need
      // to grab the saved register and move it to pseudoregister dst.
      i = new x86_instr(x86_instr::load, reg_disp8, x86_reg::eBP, 
                        x86_reg::eCX);
      em->emit(i);
      i = new x86_instr(x86_instr::mov, (uint32_t)0, x86_reg::eDX);
      em->emit(i);

      // store EDX:ECX in dst
      i = new x86_instr(x86_instr::store, x86_reg::eCX, 
                        (int32_t)destThisFrame, x86_reg::eBP);
      em->emit(i);
      i = new x86_instr(x86_instr::store, x86_reg::eDX, 
                        (int32_t)destThisFrame+4, x86_reg::eBP);
      em->emit(i);
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
   instr_t *i;
   if(!emit_64bit_code) { // 32-bit code, psuedoregs == archregs
      x86_reg dst_reg(x86_reg::rawIntReg, dst);
      if(hasExtraSave) {
         // The value of %ESP will point to the top of the saved regs
         // when we're beginning to emit code for this AST expression, since 
         // whenever we directly manipulate the stack in emitted code, it is 
         // with matched push/pop pairs. Therefore, we just need to grab the 
         // saved %ESP and access the param using the appropriate offset.
         signed char esp_disp8 = 16;
         i = new x86_instr(x86_instr::mov, x86_reg::eSP, dst_reg);
         em->emit(i);
         i = new x86_instr(x86_instr::load, esp_disp8, dst_reg, dst_reg);
         em->emit(i);
         assert(paramId < (128/sizeof(kptr_t)));
         paramId *= sizeof(kptr_t);
         signed char param_disp8 = (signed char)paramId + 4; 
            // +4 since origESP points to the return EIP
         i = new x86_instr(x86_instr::load, param_disp8, dst_reg, dst_reg);
         em->emit(i);
      }
      else {
         // The value of %ESP will be its value at the entry point of the
         // called function when we're beginning to emit code for this AST 
         // expression, since whenever we directly manipulate the stack in 
         // emitted code, it is with matched push/pop pairs. Therefore, we 
         // just need to access the param using the appropriate offset.
         assert(paramId < (128/sizeof(kptr_t)));
         paramId *= sizeof(kptr_t);
         signed char param_disp8 = (signed char)paramId + 4; 
            // +4 since ESP points to the return EIP
         i = new x86_instr(x86_instr::mov, x86_reg::eSP, dst_reg);
         em->emit(i);
         i = new x86_instr(x86_instr::load, param_disp8, dst_reg, dst_reg);
         em->emit(i);
      }
   }
   else { // 64-bit code, psuedoregs == mem locations
      assert(hasExtraSave && "emitted 64-bit x86 code must do a save\n");
      // We always set up our own stack frame for 64-bit code, and %EBP
      // thus points to the top of the saved regs (more specifically, it
      // points to the saved EFLAGS reg). Therefore, we just need
      // to grab the saved %ESP, get the param from the appropriate offset, 
      // and move it to pseudoregister dst.
      signed char esp_disp8 = 16;
      i = new x86_instr(x86_instr::load, esp_disp8, x86_reg::eBP, 
                        x86_reg::eCX);
      em->emit(i);
      assert(paramId < (128/sizeof(kptr_t)));
      paramId *= sizeof(kptr_t);
      signed char param_disp8 = (signed char)paramId + 4; 
         // +4 since origESP points to the return EIP
      i = new x86_instr(x86_instr::load, param_disp8, x86_reg::eCX, 
                        x86_reg::eCX);
      em->emit(i);
      i = new x86_instr(x86_instr::mov, (uint32_t)0, x86_reg::eDX);
      em->emit(i);

      // store EDX:ECX in dst
      i = new x86_instr(x86_instr::store, x86_reg::eCX, (int32_t)dst, 
                        x86_reg::eBP);
      em->emit(i);
      i = new x86_instr(x86_instr::store, x86_reg::eDX, (int32_t)dst+4, 
                        x86_reg::eBP);
      em->emit(i);
   }
}

// Finds the register returned at the return point spliceAddr
// which belongs to the function at entryAddr
void getReturnRegister(kptr_t /*entryAddr*/, kptr_t /*spliceAddr*/, 
                       bool hasExtraSave, 
                       Register dst, Register /*scratch*/, 
                       tempBufferEmitter *em)
{
   instr_t *i;
   if(!emit_64bit_code) { // 32-bit code, psuedoregs == archregs
      x86_reg dst_reg(x86_reg::rawIntReg, dst);
      if(hasExtraSave) {
         // The value of %ESP will point to the top of the saved regs
         // when we're beginning to emit code for this AST expression, since 
         // whenever we directly manipulate the stack in emitted code, it is 
         // with matched push/pop pairs. Therefore, we just need to grab the 
         // saved %EAX and move it to dst.
         signed char eax_disp8 = 32;
         i = new x86_instr(x86_instr::mov, x86_reg::eSP, dst_reg);
         em->emit(i);
         i = new x86_instr(x86_instr::load, eax_disp8, dst_reg, dst_reg);
         em->emit(i);
      }
      else {
         // We need the value of %EAX as it was before we executed any
         // of the emitted code at this exit point, since the code might 
         // have used it. Unfortunately, there's really no way to get it, 
         // so print a warning and just use current value.
         cerr << "WARNING: x86 code emitter - getReturnRegister() using current value of %eax register as return value,\nbut can't guarantee the register hasn't been updated by previously emitted code\n";
         if(dst_reg != x86_reg::eAX) {
            i = new x86_instr(x86_instr::mov, x86_reg::eAX, dst_reg);
            em->emit(i);
         }
      }
   }
   else { // 64-bit code, psuedoregs == mem locations
      assert(hasExtraSave && "emitted 64-bit x86 code must do a save\n");
      // We always set up our own stack frame for 64-bit code, and %EBP
      // thus points to the top of the saved regs (more specifically, it
      // points to the saved EFLAGS reg). Therefore, we just need
      // to grab the saved %EAX and move it to pseudoregister dst.
      signed char eax_disp8 = 32;
      i = new x86_instr(x86_instr::load, eax_disp8, x86_reg::eBP, x86_reg::eCX);
      em->emit(i);
      i = new x86_instr(x86_instr::mov, (uint32_t)0, x86_reg::eDX);
      em->emit(i);

      // store EDX:ECX in dst
      i = new x86_instr(x86_instr::store, x86_reg::eCX, (int32_t)dst, 
                        x86_reg::eBP);
      em->emit(i);
      i = new x86_instr(x86_instr::store, x86_reg::eDX, (int32_t)dst+4, 
                        x86_reg::eBP);
      em->emit(i);
   }
}

// for operations requiring a Register to be returned
// (e.g., getRetValOp, getParamOp, getSysRetValOp, getSysParamOp)
// For getParamOp and getSysParamOp scratch is the parameter's number,
// For getRetValOp and getSysRetValOp it is an actual register
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
   instr_t *i = new x86_instr(x86_instr::ret); 
   em->emit(i);
}

} // namespace
