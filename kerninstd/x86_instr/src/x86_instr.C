// x86_instr.C

#include "x86_instr.h"
#include "x86_reg.h"
#include "x86_reg_set.h"
#include "insnVec.h"
#include "pdutil/h/xdr_send_recv.h"
#include "util/h/rope-utils.h" // addr2hex()

void x86_instr::print_raw_bytes() const
{
   for(unsigned i=0; i<num_bytes; i++) {
      printf("%02x", *(const unsigned char*)(&raw_bytes[0] + i));
   }
}

// global variables
unsigned ud2_bug_size = 0;

/* ------------------------------------------------------------------- */

class x86_instr_heap {
 private:
   // unsigned num_allocs;
//    unsigned num_deallocs;
//    unsigned num_reuses;

   // The following deserve explanation: we want to pre-allocate memory
   // for x86_instr's in large chunks (multiples of a page). Currently, 
   // sizeof(x86_instr)==28, so 9362 x86_instr's will fit in 64 4KB pages.
   // If you change the sizeof(x86_instr) by adding/removing data members,
   // please recalculate the number of insns
   static const unsigned chk_num_insns = 9362;
   static const unsigned chk_size = chk_num_insns*sizeof(x86_instr);

   pdvector<void*> chunks;
   pdvector<unsigned> chk_numelems;
   pdvector<unsigned> chk_deletes;
   unsigned curr_fill_chk;
   bool delete_spot;
   std::pair<unsigned, void*> the_delete; 
      // first is chk ndx, second the newly available location
 public:
   x86_instr_heap() : //num_allocs(0), num_deallocs(0), num_reuses(0),
      delete_spot(false) {}

   ~x86_instr_heap() {
      if(chunks.size()) {
         int i = 0;
         int finish = chunks.size();
         for(; i != finish; i++) {
             free(chunks[i]);
         }
      }
      //printUsage();
   }

   void* alloc() {
      void *result = NULL;
      //num_allocs++;

      // first time in alloc
      if(chunks.size() == 0) {
         result = malloc(chk_size);
         if(!result)
            assert(!"x86_instr_heap::alloc - could not allocate memory\n");
         chunks.push_back(result);
         chk_numelems.push_back(1);
         chk_deletes.push_back(0);
         curr_fill_chk = 0;
         return result;
      }

      // reuse last deleted, if available. this is an optimization for the
      // behavior of getting a copy of an instr using getInstr() for use in
      // some sort of analysis, then deleting the copy before going onto the
      // next instr
      if(delete_spot) {
         //num_reuses++;
         unsigned chk = the_delete.first;
         result = the_delete.second;
         chk_deletes[chk]--;
         delete_spot = false;
         return result;
      }

      // find non-full chunk, if exists
      unsigned num_chunks = chunks.size();
      if(curr_fill_chk < num_chunks) {
         unsigned elems = chk_numelems[curr_fill_chk];
         if(elems < chk_num_insns) {
            result = (void*)((x86_instr*)chunks[curr_fill_chk] 
                             + chk_numelems[curr_fill_chk]);
            chk_numelems[curr_fill_chk]++;
            if(chk_numelems[curr_fill_chk] == chk_num_insns)
               curr_fill_chk++;
            return result;
         }
      }

      // we're all full, create new chunk
      result = malloc(chk_size);
      if(!result)
         assert(!"x86_instr_heap::alloc - could not allocate memory\n");
      chunks.push_back(result);
      chk_numelems.push_back(1);
      chk_deletes.push_back(0);
      return result;
   }

   void dealloc(void *insn) {
      //num_deallocs++;
      unsigned chks_size = chunks.size();
      int i = curr_fill_chk < chks_size ? curr_fill_chk : (chks_size-1);
      for(; i>=0; i--) {
         char *chk_start = (char*)chunks[i];
         char *chk_end = chk_start + chk_size;
         if(((char*)insn >= chk_start) && ((char*)insn < chk_end)) {
            chk_deletes[i]++;
            if(chk_deletes[i] == chk_num_insns) {
               void *last_chk_start = chunks[chks_size-1];
               unsigned last_chk_elems = chk_numelems[chks_size-1];
               unsigned last_chk_deletes = chk_deletes[chks_size-1];
               if(last_chk_start != chk_start) {
                  chunks[i] = last_chk_start;
                  chk_numelems[i] = last_chk_elems;
                  chk_deletes[i] = last_chk_deletes;
               }
               free((void*)chk_start);
               chunks.pop_back();
               chk_numelems.pop_back();
               chk_deletes.pop_back();
               if(delete_spot) {
                  if(the_delete.first == (unsigned)i) {
                     // delete spot was in this freed chunk, so clobber it
                     delete_spot = false;
                  }
               }
            }
            else {
               delete_spot = true;
               the_delete.first = i;
               the_delete.second = insn;
            }
            break;
         } 
      }
   }

   // void printUsage() {
//       cerr << "x86_instr_heap Usage:\n" << "- #allocs : " << num_allocs
//            << "\n- #deallocs : " << num_deallocs << "\n- #reuses : "
//            << num_reuses << "\n- #chks : " << chunks.size() 
//            << "\n- chksize (in bytes) : " << chk_size << endl;
//    }
};

// noone outside this .C should mess with the_insn_heap
static x86_instr_heap the_insn_heap;

void* x86_instr::operator new(size_t) 
{
   return the_insn_heap.alloc();
}

void x86_instr::operator delete(void *insn) 
{
   if(!insn) return;
   the_insn_heap.dealloc(insn);
}

/* ------------------------------------------------------------------- */

x86_instr::x86_instr(const char *addr) : instr_t() 
{
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((const unsigned char*)addr, insn_type, insn);
   memcpy(&raw_bytes[0], addr, num_bytes * sizeof(unsigned char));
}

x86_instr::x86_instr(XDR *xdr) : instr_t() 
{
   // Receive total number of bytes
   unsigned total_num_bytes;
   if(!P_xdr_recv(xdr, total_num_bytes))
      assert(!"xdr recv of x86_instr::num_bytes failed");
   num_bytes = total_num_bytes;

   // Receive the raw insn bytes
   char *buffer = &raw_bytes[0];
   if(!xdr_bytes(xdr, &buffer, &num_bytes, total_num_bytes))
      assert(!"xdr recv of x86_instr::raw_bytes failed");   
}

bool x86_instr::send(XDR *xdr) const 
{
   // Send total number of bytes
   if(!P_xdr_send(xdr, num_bytes))
      return false;

   // Send actual insn bytes
   unsigned total_num_bytes = num_bytes;
   char *buffer = const_cast<char*>(&raw_bytes[0]); //since const method
   if(!xdr_bytes(xdr, &buffer, &total_num_bytes, total_num_bytes))
      return false;

   return true;
}

//-----------------------------------------------------------------------

x86_instr::Adc x86_instr::adc;
x86_instr::Add x86_instr::add;
x86_instr::And x86_instr::andL;
x86_instr::Call x86_instr::call;
x86_instr::Cli x86_instr::cli;
x86_instr::Cmov x86_instr::cmovcc;
x86_instr::Cmp x86_instr::cmp;
x86_instr::CmpXChg x86_instr::cmpxchg;
x86_instr::CmpXChg8b x86_instr::cmpxchg8b;
x86_instr::Div x86_instr::idiv;
x86_instr::Int3 x86_instr::int3;
x86_instr::Jcc x86_instr::jcc;
x86_instr::Jmp x86_instr::jmp;
x86_instr::Leave x86_instr::leave;
x86_instr::Load x86_instr::load;
x86_instr::Neg x86_instr::neg;
x86_instr::Nop x86_instr::nop;
x86_instr::Not x86_instr::notL;
x86_instr::Mov x86_instr::mov;
x86_instr::Mult x86_instr::imul;
x86_instr::Or x86_instr::orL;
x86_instr::Pop x86_instr::pop;
x86_instr::PopAD x86_instr::popad;
x86_instr::PopFD x86_instr::popfd;
x86_instr::Push x86_instr::push;
x86_instr::PushAD x86_instr::pushad;
x86_instr::PushFD x86_instr::pushfd;
x86_instr::Rdpmc x86_instr::rdpmc;
x86_instr::Rdtsc x86_instr::rdtsc;
x86_instr::Ret x86_instr::ret;
x86_instr::SetCC x86_instr::setcc;
x86_instr::SHL x86_instr::shl;
x86_instr::SHR x86_instr::shr;
x86_instr::SHLd x86_instr::shld;
x86_instr::SHRd x86_instr::shrd;
x86_instr::Sti x86_instr::sti;
x86_instr::Store x86_instr::store;
x86_instr::Sbb x86_instr::sbb;
x86_instr::Sub x86_instr::sub;
x86_instr::Test x86_instr::test;
x86_instr::Xor x86_instr::xorL;

x86_instr::x86_instr(Adc, uint32_t imm_val, const x86_reg &src_dst) : instr_t()
{
   // adc src_dst, imm_val
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x81;
   unsigned char modrm = 3 << 6; // mod = '11'
   unsigned char reg = 2 << 3; // reg = '010'
   unsigned char rm = getRegRMVal(src_dst);
   modrm |= reg;
   modrm |= rm;
   *(buffer+1) = (char)modrm;
   *(uint32_t*)(buffer+2) = imm_val;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Adc, const x86_reg &src2, 
                     const x86_reg &src_dst) : instr_t()
{
   // adc src_dst, src2
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x11;
   unsigned char modrm = 3 << 6; // mod = '11'
   unsigned char reg = getRegRMVal(src2);
   unsigned char rm = getRegRMVal(src_dst);
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+1) = (char)modrm;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Add, uint32_t imm_val, const x86_reg &src_dst) : instr_t()
{
   // add src_dst, imm_val
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x81;
   unsigned char modrm = 3 << 6; // mod = '11'
   unsigned char rm = getRegRMVal(src_dst);
   modrm |= rm;
   *(buffer+1) = (char)modrm;
   *(uint32_t*)(buffer+2) = imm_val;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Add, const x86_reg &src2, 
                     const x86_reg &src_dst) : instr_t()
{
   // add src_dst, src2
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x01;
   unsigned char modrm = 3 << 6; // mod = '11'
   unsigned char reg = getRegRMVal(src2);
   unsigned char rm = getRegRMVal(src_dst);
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+1) = (char)modrm;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(And, uint32_t imm_val, const x86_reg &src_dst) : instr_t()
{
   // and src_dst, imm_val
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x81;
   unsigned char modrm = 3 << 6; // mod = '11'
   unsigned reg = 4;
   unsigned char rm = getRegRMVal(src_dst);
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+1) = (char)modrm;
   *(uint32_t*)(buffer+2) = imm_val;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(And, const x86_reg &src2, const x86_reg &src_dst)
{
   // and src_dst, src2
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x21;
   unsigned char modrm = 3 << 6; // mod = '11'
   unsigned char reg = getRegRMVal(src2);
   unsigned char rm = getRegRMVal(src_dst);
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+1) = (char)modrm;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Call, kaddrdiff_t displacement) : instr_t() 
{
   // pc-relative call
   char *buffer = &raw_bytes[0];
   *buffer = (char)0xE8;
   *(kaddrdiff_t*)(buffer+1) = displacement;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Call, const x86_reg &src) : instr_t() 
{
   // call *src
   char *buffer = &raw_bytes[0];
   *buffer = (char)0xFF;
   unsigned char modrm = 0xD0; // mod = '11', reg='010'
   modrm |= getRegRMVal(src); // rm
   *(buffer+1) = (char)modrm;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Cli) : instr_t() 
{
   // clear maskable interrupt flag
   char *buffer = &raw_bytes[0];
   *buffer = (char)0xFA; //cli

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Cmov, CondCodes code, 
                     const x86_reg &src, const x86_reg &dst) : instr_t()
{
   // mov src to dst if condition true
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x0F;
   *(buffer+1) = (char)(0x40 + (unsigned char)code);
   unsigned char modrm = 0xC0; // mod='11'
   unsigned char reg = getRegRMVal(dst);
   unsigned char rm = getRegRMVal(src);
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+2) = (char)modrm;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Cmp, uint32_t imm_val, const x86_reg &src) : instr_t()
{
   // cmp src, imm_val
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x81;
   unsigned char modrm = 0x1F << 3; // mod='11' reg='111'
   unsigned char rm = getRegRMVal(src);
   modrm |= rm;
   *(buffer+1) = (char)modrm;
   *(uint32_t*)(buffer+2) = imm_val;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Cmp, const x86_reg &src1, const x86_reg &src2) : instr_t()
{
   // cmp src1, src2
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x39;
   unsigned char modrm = 3 << 6; // mod='11'
   unsigned char reg = getRegRMVal(src1);
   unsigned char rm = getRegRMVal(src2);
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+1) = (char)modrm;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}
   
x86_instr::x86_instr(CmpXChg, const x86_reg &addr_reg, 
                     const x86_reg &new_val) : instr_t()
{
   // atomic cmpxchg [addr_reg], new_val
   char *buffer = &raw_bytes[0];
   *buffer = (char)0xF0; // lock prefix
   *(buffer+1) = (char)0x0F;
   *(buffer+2) = (char)0xB1;
   unsigned char modrm = 0; // mod='00', so effective address is [rm]
   unsigned char reg = getRegRMVal(new_val);
   unsigned char rm = getRegRMVal(addr_reg);
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+3) = (char)modrm;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(CmpXChg8b, const x86_reg &addr_reg) : instr_t()
{
   // atomic cmpxchg [addr_reg], new_val
   char *buffer = &raw_bytes[0];
   *buffer = (char)0xF0; // lock prefix
   *(buffer+1) = (char)0x0F;
   *(buffer+2) = (char)0xC7;
   unsigned char modrm = 0; // mod='00', so effective address is [rm]
   unsigned char reg = 1;
   unsigned char rm = getRegRMVal(addr_reg);
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+3) = (char)modrm;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Div, const x86_reg &src2, 
                     const x86_reg &src_dst) : instr_t()
{
   // idiv src_dst, src2
   // On input, src1 must be %edx:%eax (%edx must contain the upper 32-bits,
   //                                   lower 32-bits in %eax)
   // On output, %eax contains the quotient & %edx contains the remainder
   
   assert(src_dst == x86_reg::eAX);

   char *buffer = &raw_bytes[0];
   *buffer = (char)0xF7;
   unsigned char modrm = 0xF8; // mod='11', so addr is reg indicated by rm
   modrm |= getRegRMVal(src2);
   *(buffer+1) = (char)modrm;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Int3) : instr_t() 
{
   // single-step trap interrupt
   char *buffer = &raw_bytes[0];
   *buffer = (char)0xCC; //int3

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Jcc, CondCodes code, int32_t displacement) : instr_t()
{
   // pc-relative jump
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x0F;
   *(buffer+1) = (char)(0x80 + (unsigned char)code);
   *(int32_t*)(buffer+2) = displacement;
  
   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Jmp, int16_t displacement) : instr_t() 
{
   // pc-relative jump
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x66;
   *(buffer+1) = (char) 0xE9;
   *(int16_t*)(buffer+2) = displacement;
  
   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Jmp, int32_t displacement) : instr_t() 
{
   // pc-relative jump
   char *buffer = &raw_bytes[0];
   *buffer = (char)0xE9;
   *(int32_t*)(buffer+1) = displacement;
  
   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Leave) : instr_t()
{
   // leave -> mov %ebp, %esp; pop %ebp
   char *buffer = &raw_bytes[0];
   *buffer = (char)0xC9;
  
   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Load, const x86_reg &addr, const x86_reg &dst) : instr_t()
{
   // load value at address stored in addr into dst, done using mov
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x8B; //mov r32, r/m32
   unsigned char modrm = 0; // mod == '00', so effective addr is [rm]
   unsigned char rm = getRegRMVal(addr);  // addr reg
   unsigned char reg = getRegRMVal(dst); // dst reg
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+1) = (char)modrm;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Load, signed char disp8, const x86_reg &addr, 
                     const x86_reg &dst) : instr_t()
{
   if(addr == x86_reg::eSP)
      assert(!"x86_instr(Load, disp8, %esp, dst) - addr mode not supported\n");

   // move val @ disp8[addr] to dst
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x8B;
   unsigned char modrm = 1 << 6; // mod == '01'
   unsigned char rm = getRegRMVal(addr);
   unsigned char reg = getRegRMVal(dst);
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+1) = (char)modrm;
   *(buffer+2) = (char)disp8;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Load, int32_t disp32, const x86_reg &addr, 
                     const x86_reg &dst) : instr_t()
{
   if(addr == x86_reg::eSP)
      assert(!"x86_instr(Load, disp32, %esp, dst) - addr mode not supported\n");

   // move val @ disp32[addr] to dst
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x8B;
   unsigned char modrm = (unsigned char)0x80; // mod == '10'
   unsigned char rm = getRegRMVal(addr);
   unsigned char reg = getRegRMVal(dst);
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+1) = (char)modrm;
   *(int32_t*)(buffer+2) = disp32;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Mov, uint32_t val, const x86_reg &dst) : instr_t()
{
   // mov dst, val
   char *buffer = &raw_bytes[0];
   unsigned char opcode = 0xB8; // mov r32, imm_val (short encoding)
   opcode |= getRegRMVal(dst); // reg specified in opcode for short encoding
   *buffer = (char)opcode;
   *(uint32_t*)(buffer+1) = val;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Mov, const x86_reg &src, const x86_reg &dst) : instr_t()
{
   // mov dst, src
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x89; // mov r32, r32
   unsigned char modrm = 3 << 6; // mod == '11'
   unsigned char rm = getRegRMVal(dst);  // to
   unsigned char reg = getRegRMVal(src); // from
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+1) = (char)modrm;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Mult, const x86_reg &src2, 
                     const x86_reg &src_dst) : instr_t()
{
   // imul src_dst, src2
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x0F;
   *(buffer+1) = (char)0xAF;
   unsigned char modrm = 0xC0; // mod == '11'
   unsigned char rm = getRegRMVal(src2);
   unsigned char reg = getRegRMVal(src_dst);
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+2) = (char)modrm;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Mult, const x86_reg &src, uint32_t imm_val, 
                     const x86_reg &dst) : instr_t()
{
   // imul dst, src, imm_val
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x69;
   unsigned char modrm = 0xC0; // mod == '11'
   unsigned char rm = getRegRMVal(src);
   unsigned char reg = getRegRMVal(dst);
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+1) = (char)modrm;
   *(uint32_t*)(buffer+2) = imm_val;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Neg, const x86_reg &src_dst) : instr_t()
{
   // neg src_dst
   char *buffer = &raw_bytes[0];
   *buffer = (char)0xF7; // neg
   unsigned char modrm = 3 << 6; // mod == '11'
   unsigned char rm = getRegRMVal(src_dst);
   unsigned char reg = 3; // reg = '011'
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+1) = (char)modrm;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Nop) : instr_t()
{
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x90; // nop

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Not, const x86_reg &src_dst) : instr_t()
{
   // not src_dst
   char *buffer = &raw_bytes[0];
   *buffer = (char)0xF7; // not
   unsigned char modrm = 3 << 6; // mod == '11'
   unsigned char rm = getRegRMVal(src_dst);
   unsigned char reg = 2; // reg = '010'
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+1) = (char)modrm;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}
   
x86_instr::x86_instr(Or, uint32_t imm_val, const x86_reg &src_dst) : instr_t()
{
   // or src_dst, imm_val
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x81;
   unsigned char modrm = 3 << 6; // mod = '11'
   unsigned char reg = 1;
   unsigned char rm = getRegRMVal(src_dst);
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+1) = (char)modrm;
   *(uint32_t*)(buffer+2) = imm_val;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Or, const x86_reg &src2, 
                     const x86_reg &src_dst) : instr_t()
{
   // and src_dst, src2
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x09;
   unsigned char modrm = 3 << 6; // mod = '11'
   unsigned char reg = getRegRMVal(src2);
   unsigned char rm = getRegRMVal(src_dst);
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+1) = (char)modrm;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Pop, const x86_reg &dst) : instr_t() 
{
   // pop dst reg off stack
   char *buffer = &raw_bytes[0];
   unsigned char opcode = 0x58 + getRegRMVal(dst); // pop %dst
   *buffer = (char)opcode;
  
   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(PopAD) : instr_t() 
{
   // pop all 32-bit GPRs off stack
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x61; // popad
  
   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(PopFD) : instr_t() 
{
   // pop EFLAGS
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x9D; // popfd
  
   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Push, uint32_t val) : instr_t() 
{
   // push 32-bit immediate value onto the stack
   char *buffer = &raw_bytes[5];
   *buffer = (char)0x68; // push imm32
   *(uint32_t*)(buffer+1) = val;
  
   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Push, const x86_reg &src) : instr_t() 
{
   // push 32-bit reg onto the stack
   char *buffer = &raw_bytes[0];
   unsigned char opcode = 0x50 + getRegRMVal(src); // push %src
   *buffer = (char)opcode;
  
   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(PushAD) : instr_t() 
{
   // push all 32-bit GPRs onto stack
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x60; // pushad
  
   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(PushFD) : instr_t() 
{
   // push EFLAGS
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x9C; // pushfd
  
   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Rdpmc) : instr_t()
{
   // rdpmc - read perf ctr specified by ECX into EDX:EAX
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x0F;
   *(buffer+1) = (char)0x33;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Rdtsc) : instr_t()
{
   // rdtsc - read TSC contents into EDX:EAX
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x0F;
   *(buffer+1) = (char)0x31;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Ret) : instr_t()
{
   // return
   char *buffer = &raw_bytes[0];
   *buffer = (char)0xC3; //ret

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(SetCC, CondCodes code, const x86_reg &dst): instr_t()
{
   unsigned reg = dst.getIntNum();
   if(!((reg>=16 && reg<=19) || (reg>=8 && reg<=11))) {
      cerr << "x86_instr(SetCC) - reg is " << reg;
      cerr << " ; only eAX,eBX,eCX,eDX allowed\n";
      assert(0);
   }
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x0F;
   *(buffer+1) = (char)((unsigned char)code + 0x90);
   unsigned char modrm = 3 << 6; // mod = '11'
   modrm |= getRegRMVal(dst);
   *(buffer+2) = (char)modrm;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(SHLd, uint32_t imm_val, const x86_reg &src, 
                     const x86_reg &dst) : instr_t()
{
   // SHLd dst, src, imm_val
   assert(imm_val < 32);
   unsigned char imm8 = (unsigned char)(imm_val & 0xFF);
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x0F;
   *(buffer+1) = (char)0xA4;
   unsigned char modrm = 3 << 6; // mod = '11'
   unsigned char reg = getRegRMVal(src);
   unsigned char rm = getRegRMVal(dst);
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+2) = (char)modrm;
   *(buffer+3) = (char)imm8;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(SHL, uint32_t imm_val, const x86_reg &src_dst) : instr_t()
{
   // SHL src_dst, imm_val
   assert(imm_val < 32);
   unsigned char imm8 = (unsigned char)(imm_val & 0xFF);
   char *buffer = &raw_bytes[0];
   *buffer = (char)0xC1;
   unsigned char modrm = 3 << 6; // mod = '11'
   unsigned char reg = 4;
   unsigned char rm = getRegRMVal(src_dst);
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+1) = (char)modrm;
   *(buffer+2) = (char)imm8;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(SHL, const x86_reg &src2, 
                     const x86_reg &src_dst) : instr_t()
{
   // shl src_dst, src2
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x29;
   unsigned char modrm = 3 << 6; // mod = '11'
   unsigned char reg = getRegRMVal(src2);
   unsigned char rm = getRegRMVal(src_dst);
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+1) = (char)modrm;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(SHRd, uint32_t imm_val, const x86_reg &src, 
                     const x86_reg &dst) : instr_t()
{
   // SHRd dst, src, imm_val
   assert(imm_val < 32);
   unsigned char imm8 = (unsigned char)(imm_val & 0xFF);
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x0F;
   *(buffer+1) = (char)0xAC;
   unsigned char modrm = 3 << 6; // mod = '11'
   unsigned char reg = getRegRMVal(src);
   unsigned char rm = getRegRMVal(dst);
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+2) = (char)modrm;
   *(buffer+3) = (char)imm8;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(SHR, uint32_t imm_val, const x86_reg &src_dst) : instr_t()
{
   // SHR src_dst, imm_val
   assert(imm_val < 32);
   unsigned char imm8 = (unsigned char)(imm_val & 0xFF);
   char *buffer = &raw_bytes[0];
   *buffer = (char)0xC1;
   unsigned char modrm = 3 << 6; // mod = '11'
   unsigned char reg = 5;
   unsigned char rm = getRegRMVal(src_dst);
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+1) = (char)modrm;
   *(buffer+2) = imm8;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Sti) : instr_t() 
{
   // set maskable interrupt flag
   char *buffer = &raw_bytes[0];
   *buffer = (char)0xFB; //sti

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Store, uint32_t imm_val, const x86_reg &addr) : instr_t()
{
   // store imm_val into memory address in addr
   char *buffer = &raw_bytes[0];
   *buffer = (char)0xC7; // mov [addr], imm_val
   unsigned char modrm = getRegRMVal(addr); // mod == '00', reg == '000' 
   *(buffer+1) = (char)modrm;
   *(uint32_t*)(buffer+2) = imm_val;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Store, const x86_reg &val, const x86_reg &addr) : instr_t()
{
   // store contents of val reg into memory address in addr
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x89; // mov [addr], val
   unsigned char modrm = 0; // mod = '00'
   unsigned char reg = getRegRMVal(val);
   unsigned char rm = getRegRMVal(addr);
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+1) = (char)modrm;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Store, const x86_reg &val, int32_t disp32, 
                     const x86_reg &addr) : instr_t()
{
   if(addr == x86_reg::eSP)
      assert(!"x86_instr(Store, val, disp32, %esp) - addr mode not supported\n");

   // store contents of val reg into memory address disp32[addr]
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x89; // mov disp32[addr], val
   unsigned char modrm = (unsigned char)0x80; // mod = '10'
   unsigned char reg = getRegRMVal(val);
   unsigned char rm = getRegRMVal(addr);
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+1) = (char)modrm;
   *(int32_t*)(buffer+2) = disp32;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Sbb, uint32_t imm_val, const x86_reg &src_dst) : instr_t()
{
   // sbb src_dst, imm_val
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x81;
   unsigned char modrm = 3 << 6; // mod = '11'
   unsigned char reg = 3 << 3; // reg = '011'
   unsigned char rm = getRegRMVal(src_dst);
   modrm |= reg;
   modrm |= rm;
   *(buffer+1) = (char)modrm;
   *(uint32_t*)(buffer+2) = imm_val;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Sbb, const x86_reg &src2, 
                     const x86_reg &src_dst) : instr_t()
{
   // sbb src_dst, src2
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x19;
   unsigned char modrm = 3 << 6; // mod = '11'
   unsigned char reg = getRegRMVal(src2);
   unsigned char rm = getRegRMVal(src_dst);
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+1) = (char)modrm;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Sub, uint32_t imm_val, const x86_reg &src_dst) : instr_t()
{
   // sub src_dst, imm_val
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x81;
   unsigned char modrm = 3 << 6; // mod = '11'
   unsigned char reg = 5;
   unsigned char rm = getRegRMVal(src_dst);
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+1) = (char)modrm;
   *(uint32_t*)(buffer+2) = imm_val;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Sub, const x86_reg &src2, 
                     const x86_reg &src_dst) : instr_t()
{
   // sub src_dst, src2
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x29;
   unsigned char modrm = 3 << 6; // mod = '11'
   unsigned char reg = getRegRMVal(src2);
   unsigned char rm = getRegRMVal(src_dst);
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+1) = (char)modrm;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

x86_instr::x86_instr(Test, uint32_t imm_val, const x86_reg &src) : instr_t()
{
   // test src, imm_val
   char *buffer = &raw_bytes[0];
   *buffer = (char)0xF7;
   unsigned char modrm = 0xC0; // mod = '11', reg = '000'
   unsigned char rm = getRegRMVal(src);
   modrm |= rm;
   *(buffer+1) = (char)modrm;
   *(uint32_t*)(buffer+2) = imm_val;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}


x86_instr::x86_instr(Xor, uint32_t imm_val, const x86_reg &src_dst) : instr_t()
{
   // xor src_dst, imm_val
   char *buffer = &raw_bytes[0];
   *buffer = (char)0x81;
   unsigned char modrm = 3 << 6; // mod = '11'
   unsigned char reg = 6; // reg = '110'
   unsigned char rm = getRegRMVal(src_dst);
   modrm |= (reg << 3);
   modrm |= rm;
   *(buffer+1) = (char)modrm;
   *(uint32_t*)(buffer+2) = imm_val;

   // Recreate other parts (as a bonus, verifies correctness)
   unsigned insn_type;
   ia32_instruction insn;
   num_bytes = get_instruction((unsigned char*)buffer, insn_type, insn);
}

// ---------------------------------------------------------------------

void x86_instr::getInformation(registerUsage *ru,
			       memUsage *mu,
			       disassemblyFlags *disassFlags,
			       controlFlowInfo *cfi,
			       relocateInfo *rel) const 
{
   unsigned insn_type;
   ia32_instruction insn;
   (void)get_instruction((const unsigned char*)&raw_bytes[0], insn_type, insn);

   if(ru) {
      updateRegisterUsage(ru, insn_type, insn);
   }
   if(mu) {
      assert(!"getInformation: memory usage nyi");
   }
   if(disassFlags) {
      updateDisassemblyFlags(*disassFlags, insn_type, insn);
   }
   if(cfi) {
      x86_instr::x86_cfi *the_cfi = (x86_instr::x86_cfi *)cfi;
      updateControlFlowInfo(the_cfi, insn_type, insn);
   }
   if(rel) {
      assert(!"getInformation: relocate info nyi");
   }
}

void x86_instr::getDisassembly(disassemblyFlags &df) const 
{
   getInformation(NULL, NULL, &df, NULL, NULL);
}

void x86_instr::getRelocationInfo(relocateInfo &rel) const 
{
   getInformation(NULL, NULL, NULL, NULL, &rel);
}

void updateRegisterUsage_addReg(instr_t::registerUsage *ru, unsigned int sema,
                                unsigned int i, const x86_reg &reg)
{
   if(i == 0) {
      switch(sema) {
      case s1R: case s1R2R:
         /* definitelyUsedBeforeWritten */
         *(x86_reg_set*)(ru->definitelyUsedBeforeWritten) += reg;
         break;
      case s1RW: case s1RW2R: case s1RW2RW:
      case s1RW2R3R: case s1RW2RW3R:
         /* definitelyUsedBeforeWritten & definitelyWritten */
         *(x86_reg_set*)(ru->definitelyUsedBeforeWritten) += reg;
         *(x86_reg_set*)(ru->definitelyWritten) += reg;
         break;
      case s1W: case s1W2R: case s1W2R3R:
      case s1W2W3R: case s1W2RW3R: case s1W2R3RW:
         /* definitelyWritten */
         *(x86_reg_set*)(ru->definitelyWritten) += reg;
         break;
      default:
         break;
      }
   }
   else if(i == 1) {
      switch(sema) {
      case s1R2R: case s1W2R: case s1RW2R: case s1W2R3R:
      case s1W2R3RW: case s1RW2R3R:
         /* definitelyUsedBeforeWritten */
         *(x86_reg_set*)(ru->definitelyUsedBeforeWritten) += reg;
         break;
      case s1RW2RW: case s1W2RW3R: case s1RW2RW3R:
         /* definitelyUsedBeforeWritten & definitelyWritten */
         *(x86_reg_set*)(ru->definitelyUsedBeforeWritten) += reg;
         *(x86_reg_set*)(ru->definitelyWritten) += reg;
         break;
      case s1W2W3R:
         /* definitelyWritten */
         *(x86_reg_set*)(ru->definitelyWritten) += reg;
         break;
      default:
         break;
      }
   }
   else {
      switch(sema) {
      case s1W2R3R: case s1W2W3R: case s1W2RW3R: case s1RW2R3R: case s1RW2RW3R:
         /* definitelyUsedBeforeWritten */
         *(x86_reg_set*)(ru->definitelyUsedBeforeWritten) += reg;
         break;
      case s1W2R3RW:
         /* definitelyUsedBeforeWritten & definitelyWritten */
         *(x86_reg_set*)(ru->definitelyUsedBeforeWritten) += reg;
         *(x86_reg_set*)(ru->definitelyWritten) += reg;
         break;
      default:
         break;
      }
   }
}

void x86_instr::updateRegisterUsage(instr_t::registerUsage *ru, 
                                    unsigned insn_type,
                                    const ia32_instruction &insn) const
{
   unsigned char modrm, mod, reg, rm;
   unsigned char op1, op2;

   assert(ru);
   assert(ru->definitelyUsedBeforeWritten);
   assert(ru->maybeUsedBeforeWritten);
   assert(ru->definitelyWritten);
   assert(ru->maybeWritten);

   const ia32_entry &entry = insn.getEntry();
   unsigned int width = (insn_type & PREFIX_OPR ? 2 : 4);
   unsigned int sema = entry.opsema & ((1<<FPOS)-1);

   // have to special case enter/leave
   if((entry.opsema == (fENTER << FPOS)) ||
      (entry.opsema == (fLEAVE << FPOS))) {
      // ESP,EBP both definitelyWritten & definitelyUsedBeforeWritten
      *(x86_reg_set*)(ru->definitelyWritten) += 
         x86_reg_set(x86_reg_set::raw, 0x00303000, 0, 0);
      *(x86_reg_set*)(ru->definitelyUsedBeforeWritten) += 
         x86_reg_set(x86_reg_set::raw, 0x00303000, 0, 0);
   }

   for(unsigned int i=0; i<3; ++i) {
      const ia32_operand& op = entry.operands[i];
      if(op.admet) {
         const ia32_memacc &mac = insn.getMac(i);
         
         unsigned int opwidth = width;
         if(op.optype == op_b) opwidth = 1;
         else if(op.optype == op_w) opwidth = 2;
         else if(op.optype == op_d) opwidth = 4;
         else if(op.optype == op_c) opwidth = width/2;

	 if(insn.getModRmOffset()) {
	    modrm = *(const unsigned char*)(&raw_bytes[0] + insn.getModRmOffset());
	    mod = modrm >> 6;
	    reg = (modrm >> 3) & 7;
	    rm = modrm & 7;
	 }

	 switch(op.admet) {
	 case am_A:  /* address = segment + offset (word or dword) */
	    /* no registers used, since both seg & off explicit */
	    break;
	 case am_C: { /* control register - for MOV (20,22) */
	    /* reg field selects control register, rm field selects GPR */
	    op2 = *(const unsigned char*)(&raw_bytes[0] + insn.getOpcodeOffset() + 1);
            x86_reg cr(x86_reg::controlReg, reg);
	    if(op2 == 0x22) { /* 22 - GPR read, CR written */
	       *(x86_reg_set*)(ru->definitelyWritten) += cr;
	    }
            else { /* 20 - GPR written, CR read */
	       *(x86_reg_set*)(ru->definitelyUsedBeforeWritten) += cr;
            }
	    break;
         }
	 case am_D: { /* debug register - for MOV (21,23) */
	    /* reg field specifies debug register, rm field selects GPR */
	    op2 = *(const unsigned char*)(&raw_bytes[0] + insn.getOpcodeOffset() + 1);
            x86_reg dr(x86_reg::debugReg, reg);
	    if(op2 == 0x23) { /* 23 - GPR read, DR written */
	       *(x86_reg_set*)(ru->definitelyWritten) += dr;
	    }
            else { /* 23 - GPR written, DR read */
	       *(x86_reg_set*)(ru->definitelyUsedBeforeWritten) += dr;
	    }
	    break;
         }
	 case am_E: { /* register or memory location, decoding needed */
            if(!mac.is) {
               x86_reg gpr(x86_reg::generalModRMReg, rm, opwidth);
               updateRegisterUsage_addReg(ru, sema, i, gpr);
            }
            else {
               unsigned adjust = (mac.addr32 ? 16 : 8);
               if(mac.regs[0] != -1)
                  *(x86_reg_set*)(ru->definitelyUsedBeforeWritten) += 
                     x86_reg(x86_reg::rawIntReg, mac.regs[0]+adjust);
               if(mac.regs[1] != -1)
                  *(x86_reg_set*)(ru->definitelyUsedBeforeWritten) += 
                     x86_reg(x86_reg::rawIntReg, mac.regs[1]+adjust);
            }
            break;
         }
	 case am_F: { /* flags register - for pushf(d) & popf(d) */
            op1 = *(const unsigned char*)(&raw_bytes[0] + insn.getOpcodeOffset());
            
	    if(op1 == 0x9D) { /* popf(d) - EFLAGS written, eSP read/written */
               *(x86_reg_set*)(ru->definitelyWritten) += x86_reg::EFLAGS;
            }
            else { /* pushf(d) - EFLAGS read, eSP read/written */
               *(x86_reg_set*)(ru->definitelyUsedBeforeWritten) += x86_reg::EFLAGS;
            }
	    break;
         }
	 case am_G: { /* general purpose register, selected by reg field */
	    x86_reg gpr(x86_reg::generalModRMReg, reg, opwidth);
            updateRegisterUsage_addReg(ru, sema, i, gpr);
            break;
         }
	 case am_I:  /* immediate data */
	    break;
	 case am_J:  /* instruction pointer offset - for relative jmp/call */
	    break;
	 case am_M:  /* memory operand */
	    break;
	 case am_O:  /* operand offset - for MOV (A0-A3) */
	    break;
	 case am_P:  /* MMX register, selected by reg field */
	    //cerr << "MJB FIXME: updateRegisterUsage - am_P nyi\n";
	    break;
	 case am_Q:  /* MMX register or memory location, decoding needed */
	    //cerr << "MJB FIXME: updateRegisterUsage - am_Q nyi\n";
	    //if(!mac.is)
	    //   
            //else
	    //   
	    break;
	 case am_R: { /* general purpose register, selected by mod field */
	    x86_reg gpr(x86_reg::generalModRMReg, rm, opwidth);
            updateRegisterUsage_addReg(ru, sema, i, gpr);
            break;
         }
	 case am_S: { /* segment register, selected by reg field */
	    x86_reg the_reg(x86_reg::segmentReg, reg);
            updateRegisterUsage_addReg(ru, sema, i, the_reg);
	    break;
         }
	 case am_T:  /* test register, selected by reg field */
            assert(!"updateRegisterUsage - am_T unsupported\n");
	    break;
	 case am_V:  /* XMM register, selected by reg field */
	    //cerr << "MJB FIXME: updateRegisterUsage - am_V nyi\n";
	    break;
	 case am_W:  /* XMM register or memory location, decoding needed */
	    //cerr << "MJB FIXME: updateRegisterUsage - am_W nyi\n";
	    //if(!mac.is)
	    // 
	    //else
	    //
	    break;
	 case am_X:  /* memory at DS:eSI */
            *(x86_reg_set*)(ru->definitelyUsedBeforeWritten) += x86_reg::DS;
            *(x86_reg_set*)(ru->definitelyUsedBeforeWritten) += x86_reg::eSI;
            break;
         case am_Y:  /* memory at ES:eDI */
	    *(x86_reg_set*)(ru->definitelyUsedBeforeWritten) += x86_reg::ES;
            *(x86_reg_set*)(ru->definitelyUsedBeforeWritten) += x86_reg::eDI;
            break;
	 case am_reg: { /* register implicitly encoded in opcode */
            if(op.optype < x86_reg::max_reg_num) {
               x86_reg the_reg = (x86_reg&)x86_reg::getRegByBit(op.optype);
               updateRegisterUsage_addReg(ru, sema, i, the_reg);
            }
            else {
               // hacks for cmpxchg8b to represent two regs as one
               if(op.optype == r_ECXEBX) {
                  updateRegisterUsage_addReg(ru, sema, i, x86_reg::eCX);
                  updateRegisterUsage_addReg(ru, sema, i, x86_reg::eBX);
               }
               else if(op.optype == r_EDXEAX) {
                  updateRegisterUsage_addReg(ru, sema, i, x86_reg::eDX);
                  updateRegisterUsage_addReg(ru, sema, i, x86_reg::eAX);
               }
               else
                  assert(!"x86_instr::updateRegisterUsage - unknown am_reg type\n");
            }
	    break;
         }
	 case am_allgprs: { /* for pusha(d) & popa(d) */
            op1 = *(const unsigned char*)(&raw_bytes[0] + insn.getOpcodeOffset());
            
            for(int k=0; k<8; k++) {
               if(op1 == 0x61) { /* popa(d) - all GPRs written */
                  *(x86_reg_set*)(ru->definitelyWritten) += x86_reg(x86_reg::generalModRMReg, k, opwidth);
               }
               else { /* pusha(d) - all GPRs read */
                  *(x86_reg_set*)(ru->definitelyUsedBeforeWritten) += x86_reg(x86_reg::generalModRMReg, k, opwidth);
               }
            }
	    break;
         }
	 case am_stackPSH: case am_stackPOP:  /* stack push/pop */
	    break;
	 default:
	    assert(!"updateRegisterUsage - unknown addressing method\n");
	 }
      }
      else
	 break;
   }
}

void x86_instr::updateDisassemblyFlags(disassemblyFlags &df, 
                                       unsigned insn_type,
                                       const ia32_instruction &insn) const
{
   pdstring result;
   
   x86_cfi cfi;
   getControlFlowInfo(&cfi);
   if(cfi.x86_fields.isUd2) { // linux BUG() macro
      unsigned short line = *(const unsigned short*)&raw_bytes[2];
      unsigned file = *(const unsigned *)&raw_bytes[4];
      result += "ud2 [ BUG() : file=" + tohex(file) 
                + " line=" + tohex(line) + " ]";
   }
   else {
      const ia32_entry &entry = insn.getEntry();
      if(entry.name != 0)
         result += pdstring(entry.name);
      else {
         result += pdstring(insn.getName());
      }

      bool push = false;
      unsigned char mod = 0;
      unsigned char reg = 0;
      unsigned char rm = 0;
      if(entry.hasModRM) {
         unsigned char modrm = raw_bytes[insn.getModRmOffset()];
         mod = modrm >> 6;
         reg = (modrm >> 3) & 7;
         rm = modrm & 7;
      }
      for(int k=0; k<3; k++) {
         unsigned admet = entry.operands[k].admet;
         unsigned optype = entry.operands[k].optype;
         if(admet != 0) {
            const ia32_memacc &mac = insn.getMac(k);
            if((mac.is == true) && (admet < am_stackPSH)) {
               unsigned adjust = (mac.addr32 ? 16 : 8);
               result += " ";
               if(admet == am_X)
                  result += "%ds:[" + x86_reg::disassx(mac.regs[0]+adjust) + "]";
               else if(admet == am_Y)
                  result += "%es:[" + x86_reg::disassx(mac.regs[0]+adjust) + "]";
               else if((mac.regs[0] == -1) && (mac.regs[1] == -1)) {
                  if(mac.imm == 0)
                     cerr << "Warning: x86_instr::updateDisassemblyFlags() - no regs or immediate address\n";
                  else
                     result += "[" + tohex(mac.imm) + "]";
               }
               else {
                  if(mac.imm) result += tohex(mac.imm);
                  result += "[";
                  if(mac.regs[0] != -1) 
                     result += x86_reg::disassx(mac.regs[0]+adjust);
                  if(mac.regs[1] != -1) {
                     if(mac.regs[0] != -1) result += "+";
                     result += x86_reg::disassx(mac.regs[1]+adjust);
                  }
                  if(mac.scale != 0) {
                     if(mac.regs[1] != -1) result += "*";
                     result += num2string(mac.scale);
                  }
                  result += "]";
               }
            }
            else { // must be a register access
               switch(admet) {
               case am_A: { // absolute address
                  unsigned short seg = *(const unsigned short*)&raw_bytes[5];
                  unsigned offset = *(const unsigned *)&raw_bytes[1];
                  result += " " + tohex(seg) + ":" + tohex(offset);
                  break;
               }
               case am_C: // control reg
                  result += " %cr" + num2string(reg);
                  break;
               case am_D:// debug reg
                  result += " %dr" + num2string(reg);
                  break;
               case am_E: // GPR - can't be mem access as determined above 
               case am_R: // GPR
                  result += " " + x86_reg::disassx(rm+16); // assumes 32-bit reg
                  break;
               case am_F: // EFLAGS reg
                  if(push) k = 3; // no need to show %eflags or implicit %esp
                  break;
               case am_G: // GPR
                  result += " " + x86_reg::disassx(reg+16); // assumes 32-bit reg
                  break;
               case am_I: { // immediate data
                  int imm_off = insn.getImmedOffset();
                  result += " ";
                  switch(num_bytes - imm_off) {
                  case 1:
                     result += tohex(*(const unsigned char*)(&raw_bytes[0]+imm_off));
                     break;
                  case 2:
                     result += tohex(*(const unsigned short*)(&raw_bytes[0]+imm_off));
                     break;
                  case 4:
                     result += tohex(*(const unsigned *)(&raw_bytes[0]+imm_off));
                     break;
                  default:
                     cout << "x86_instr::updateDisassemblyFlags - unhandled immediate data, imm_off=" << imm_off << ", raw_bytes=0x";
                     print_raw_bytes();
                     cout << endl;
                     result += "imm_data";
                     break;
                  }
                  if(push) k = 3; // only want to show pushed val, not implicit %esp
                  break;
               }
               case am_J: { // pc-relative offset
                  const kptr_t newPC = get_target((const unsigned char*)&raw_bytes[0], 
                                                  insn_type, num_bytes, df.pc);
                  result += " " + addr2hex(newPC);
                  if (df.disassemble_destfuncaddr)
                     df.destfunc_result += df.disassemble_destfuncaddr(newPC);
                  break;
               }
               case am_P: // MMX reg
                  result += " MM" + num2string(reg);
                  break;
               case am_Q: // MMX reg - can't be mem access due to check above
                  result += " MM" + num2string(rm);
                  break;
               case am_S: // segment reg
                  result += x86_reg::disassx(reg+24); // min_segment_reg = 24
                  break;
               case am_T: // test reg ??
                  result += " test?" + num2string(reg);
                  break;
               case am_V: // XMM reg
                  result += " XMM" + num2string(reg);
                  break;
               case am_W: // XMM reg - can't be mem access due to check above
                  result += " XMM" + num2string(rm);
                  break;
               case am_reg:
                  if(optype < r_EDXEAX)
                     result += " " + x86_reg::disassx(optype);
                  if(push) k = 3; // only want to show pushed reg, not implicit %esp
                  break;
               case am_stackPSH:
                  push = true;
                  break;
               case am_stackPOP:
                  k = 3; // only want to show popped reg, not implicit %esp
                  break;
               default:
                  break;
               }
            }
         }
      }
   }
   df.result = result;
}

void x86_instr::updateControlFlowInfo(x86_instr::x86_cfi *cfi, 
                                      unsigned insn_type,
                                      const ia32_instruction &insn) const
{
   assert(cfi);
   if(insn_type & IS_CALL) {
      cfi->fields.isCall = 1;
      cfi->fields.controlFlowInstruction = 1;
      if(insn_type & REL_X) {
	 cfi->destination = instr_t::controlFlowInfo::pcRelative;
	 cfi->offset_nbytes = displacement((const unsigned char*)&raw_bytes[0], insn_type);
      }
      else if(insn_type & INDIR) {
	 assert(insn.getModRmOffset());
	 unsigned char modrm = *(const unsigned char*)(&raw_bytes[0] + insn.getModRmOffset());
	 unsigned char mod = modrm >> 6;
	 //unsigned char reg = (modrm >> 3) & 7;
	 unsigned char rm = modrm & 7;

	 unsigned int widthInBytes = (insn_type & PREFIX_OPR ? 2 : 4);
	 if(mod != 3) { /* memIndirect */
	    cfi->destination = instr_t::controlFlowInfo::memIndirect;
	    /* set memaddr appropriately based on insn.mac[0] */
            instr_t::address::addrtype atype = instr_t::address::addrUndefined;
            reg_t *reg1 = NULL;
	    reg_t *reg2 = NULL;
	    if(insn.getMac(0).regs[0] != -1)
	       reg1 = (reg_t *) new x86_reg(x86_reg::generalModRMReg,
                                            insn.getMac(0).regs[0],
                                            widthInBytes);
	    if(insn.getMac(0).regs[1] != -1)
	       reg2 = (reg_t *) new x86_reg(x86_reg::generalModRMReg,
                                            insn.getMac(0).regs[1],
                                            widthInBytes);
            if(rm == 4) { // has SIB
               unsigned char sib, base;
               sib = *(const unsigned char*)(&raw_bytes[0] + insn.getModRmOffset() + 1);
               base = sib & 7;
               if((base == 5) && (mod == 0))
                  // disp[reg2<<scale]
                  atype = instr_t::address::scaledSingleRegPlusOffset;
               else
                  // disp[reg1][reg2<<scale]
                  atype = instr_t::address::scaledDoubleRegPlusOffset;
            }
            else
               // disp[reg1]
               atype = instr_t::address::singleRegPlusOffset;

	    (void)new(&cfi->memaddr) instr_t::address(atype, reg1, reg2,
                                                      insn.getMac(0).imm,
                                                      insn.getMac(0).scale);
	    
	 }
         else { /* regIndirect */
	    cfi->destination = instr_t::controlFlowInfo::regRelative; 
	    /* set destreg1 to register specified in rm field */
	    cfi->destreg1 = (reg_t *) new x86_reg(x86_reg::generalModRMReg,
						  (unsigned int)rm,
						  widthInBytes);
	 }
      }
      else
	 assert(!"x86_instr::updateControlFlowInfo - unhandled call type\n");
   }
   else if(insn_type & IS_JUMP) {
      cfi->fields.isBranch = 1;
      cfi->fields.isAlways = 1;
      cfi->fields.controlFlowInstruction = 1;
      if(insn_type & PTR_WX) {
	 cfi->x86_fields.isFarJmp = 1;
	 cfi->destination = instr_t::controlFlowInfo::absolute;
	 cfi->offset_nbytes = *(const unsigned long*)(&raw_bytes[1]);
	 cfi->farSegment = *(const unsigned short *)(&raw_bytes[5]);
      }
      else if(insn_type & (REL_X | REL_B)) {
	cfi->destination = instr_t::controlFlowInfo::pcRelative;
	cfi->offset_nbytes = displacement((const unsigned char*)&raw_bytes[0], insn_type);
      }
      else if(insn_type & INDIR) {
	 assert(insn.getModRmOffset());
	 unsigned char modrm = *(const unsigned char*)(&raw_bytes[0] + insn.getModRmOffset());
	 unsigned char mod = modrm >> 6;
	 unsigned char reg = (modrm >> 3) & 7;
	 unsigned char rm = modrm & 7;

	 assert(reg == 4); 
	 unsigned int widthInBytes = (insn_type & PREFIX_OPR ? 2 : 4);
	 if(mod != 3) { /* memIndirect */
	    cfi->destination = instr_t::controlFlowInfo::memIndirect;
	    /* set memaddr appropriately based on insn.mac[0] */
	    instr_t::address::addrtype atype = instr_t::address::addrUndefined;
            reg_t *reg1 = NULL;
	    reg_t *reg2 = NULL;
	    if(insn.getMac(0).regs[0] != -1)
	       reg1 = (reg_t *) new x86_reg(x86_reg::generalModRMReg,
                                            insn.getMac(0).regs[0],
                                            widthInBytes);
	    if(insn.getMac(0).regs[1] != -1)
	       reg2 = (reg_t *) new x86_reg(x86_reg::generalModRMReg,
                                            insn.getMac(0).regs[1],
                                            widthInBytes);
            if(rm == 4) { // has SIB
               unsigned char sib, base;
               sib = *(const unsigned char*)(&raw_bytes[0] + insn.getModRmOffset() + 1);
               base = sib & 7;
               if((base == 5) && (mod == 0))
                  // disp[reg2<<scale]
                  atype = instr_t::address::scaledSingleRegPlusOffset;
               else
                  // disp[reg1][reg2<<scale]
                  atype = instr_t::address::scaledDoubleRegPlusOffset;
            }
            else
               // disp[reg1]
               atype = instr_t::address::singleRegPlusOffset;

	    (void)new(&cfi->memaddr) instr_t::address(atype, reg1, reg2,
                                                      insn.getMac(0).imm,
                                                      insn.getMac(0).scale);
         }
         else { /* regIndirect */
	    cfi->destination = instr_t::controlFlowInfo::regRelative;
	    /* set destreg1 to register specified in rm field */
	    cfi->destreg1 = (reg_t *) new x86_reg(x86_reg::generalModRMReg,
						  (unsigned int)rm,
						  widthInBytes);
         }
      }
      else
	 assert(!"x86_instr::updateControlFlowInfo - unhandled jmp type\n");
   }
   else if(insn_type & IS_JCC) {
      cfi->conditionReg = (reg_t*) new x86_reg(x86_reg::EFLAGS);
      cfi->condition = insn.getCond().tttn;
      cfi->fields.isBranch = 1;
      cfi->fields.isConditional = 1;
      cfi->fields.controlFlowInstruction = 1;
      cfi->destination = instr_t::controlFlowInfo::pcRelative;
      cfi->offset_nbytes = displacement((const unsigned char*)&raw_bytes[0], insn_type);
   }
   else if(insn_type & IS_RET) {
      cfi->fields.isRet = 1;
      cfi->fields.controlFlowInstruction = 1;
   }
   else if(insn_type & IS_RETF) {
      cfi->fields.isRet = 1;
      cfi->x86_fields.isFarRet = 1;
      cfi->fields.controlFlowInstruction = 1;
   }
   else if(insn_type & IS_INT) {
      cfi->x86_fields.isInt = 1;
      cfi->fields.controlFlowInstruction = 1;
   }
   else if((insn_type & ILLEGAL) && (num_bytes == 8)) {
      // kludge to handle linux kernel BUG() macro
      if(((unsigned char)(raw_bytes[0]) == 0x0F) && 
         ((unsigned char)(raw_bytes[1]) == 0x0B)) {
	 cfi->x86_fields.isUd2 = 1;
	 cfi->fields.controlFlowInstruction = 1;
      }
   }
}

// bool x86_instr::isBranch() const 
// {
//    return (insn_type & (IS_JCC | IS_JMP));
// }

bool x86_instr::isCall() const 
{
   unsigned insn_type;
   ia32_instruction insn;
   (void)get_instruction((const unsigned char*)&raw_bytes[0], insn_type, insn);
   return (insn_type & IS_CALL);
}

bool x86_instr::isUnanalyzableCall() const 
{
   unsigned insn_type;
   ia32_instruction insn;
   (void)get_instruction((const unsigned char*)&raw_bytes[0], insn_type, insn);
   return (insn_type & (IS_CALL | INDIR));
}

bool x86_instr::isCmp() const 
{
   assert(!"x86_instr::isCmp() nyi");
   return false;
}

bool x86_instr::isNop() const 
{
   assert(!"x86_instr::isNop() nyi");
   return false;
}

bool x86_instr::isRestore() const 
{
   assert(!"x86_instr::isRestore() nyi");
   return false;
}

bool x86_instr::isSave() const 
{
   assert(!"x86_instr::isSave() nyi");
   return false;
}

bool x86_instr::isMov() const 
{
   assert(!"x86_instr::isMov() nyi");
   return false;
}

bool x86_instr::isBranchToFixedAddress(kptr_t insnAddr, 
				       kptr_t &brancheeAddr) const 
{
   kaddrdiff_t offset = 0;
   unsigned insn_type;
   ia32_instruction insn;
   (void)get_instruction((const unsigned char*)&raw_bytes[0], insn_type, insn);
   if((insn_type & IS_JUMP) || (insn_type & IS_JCC)) {
      offset = displacement((const unsigned char*)&raw_bytes[0], insn_type);
      if(offset) {
         offset += getNumBytes();
         brancheeAddr = insnAddr + offset;
         return true;
      }
      return false;
   }
   return false;
}

bool x86_instr::isCallInstr(kaddrdiff_t &offset) const 
{
   unsigned insn_type;
   ia32_instruction insn;
   (void)get_instruction((const unsigned char*)&raw_bytes[0], insn_type, insn);
   if(insn_type & IS_CALL) {
      offset = displacement((const unsigned char*)&raw_bytes[0], insn_type);
      if(offset)
         offset += getNumBytes();
      return true;
   }
   return false;
}

bool x86_instr::isCallToFixedAddress(kptr_t insnAddr, kptr_t &calleeAddr) const
{
   kaddrdiff_t offset = 0;
   if (isCallInstr(offset)) {
      //if (offset == 0) {
      //   cerr << "WARNING: isCallToFixedAddress: non-PC-relative call @ insnAddr = "
//	      << addr2hex(insnAddr) << endl;
      //}
      calleeAddr = insnAddr + offset;
      return true;
   }
   else
      return false;
}

x86_instr::dependence x86_instr::calcDependence(const instr_t */*nextInstr*/) const
{
   assert(!"x86_instr::calcDependence() nyi");
   return depnone;
}

instr_t* x86_instr::relocate(kptr_t old_insnaddr, kptr_t new_insnaddr) const 
{
   // if not a pc-relative control-transfer insn, return a copy of original
   // else, adjust the pc-relative offset
   x86_instr *copy = NULL;
   x86_cfi cfi;
   unsigned insn_type;
   ia32_instruction insn;
   (void)get_instruction((const unsigned char*)&raw_bytes[0], insn_type, insn);
   updateControlFlowInfo(&cfi, insn_type, insn);
   if(cfi.fields.controlFlowInstruction && 
      cfi.destination == controlFlowInfo::pcRelative) {
      int32_t new_offset = 0;
      kptr_t destAddr = 0;
      if(insn_type & IS_CALL) {
         (void)isCallToFixedAddress(old_insnaddr, destAddr);
         if(insn_type & REL_W)
            copy = new x86_instr(x86_instr::call, 0);
      }
      else {
         (void)isBranchToFixedAddress(old_insnaddr, destAddr);
         if(insn_type & IS_JUMP) {
            if((insn_type & REL_B) || (insn_type & REL_W))
               copy = new x86_instr(x86_instr::jmp, (int32_t)0);
         }
         else { // JCC
            if((insn_type & REL_B) || (insn_type & REL_W)) {
               unsigned cc;
               if(insn_type & REL_B) {
                  cc = (unsigned)(raw_bytes[0] - (char)0x70);
                  assert(raw_bytes[0] != (char)0xE3);
               }
               else if(insn_type & REL_W)
                  cc = (unsigned)(raw_bytes[2] - (char)0x80);
               else
                  cc = (unsigned)(raw_bytes[1] - (char)0x80);
               copy = new x86_instr(x86_instr::jcc, (x86_instr::CondCodes)cc,
                                    (int32_t)0);
            }
         }
      }
      if(copy == NULL)
         copy = new x86_instr(*this);
      new_offset = destAddr - (new_insnaddr + copy->getNumBytes());
      copy->changeBranchOffset(new_offset);
   }
   else // not a pc-releative CTI
      copy = new x86_instr(*this);
   return (instr_t*) copy;
}

void x86_instr::changeBranchOffset(int new_insnbytes_offset) 
{
   char *buffer = &raw_bytes[0];
   unsigned insn_type;
   ia32_instruction insn;
   (void) get_instruction((unsigned char*)buffer, insn_type, insn);
   if((insn_type & IS_CALL) || (insn_type & IS_JUMP))
      *(int32_t*)(buffer+1) = new_insnbytes_offset;
   else if(insn_type & IS_JCC)
      *(int32_t*)(buffer+2) = new_insnbytes_offset;
   else
      assert(!"x86_instr::changeBranchOffset - insn is not call, jmp, or jcc\n");
}

unsigned char x86_instr::getRegRMVal(const x86_reg &reg)
{
   // returns appropriate ModRM 'reg' or 'rm' field value
   return reg.getRegRMVal();
}

void x86_instr::gen32imm(insnVec_t *piv, uint32_t val, const x86_reg &rd)
{
   instr_t *i;
   i = new x86_instr(mov, val, rd);
   piv->appendAnInsn(i);
}

void x86_instr::gen64imm(insnVec_t */*piv*/, uint64_t /*val*/, 
                         const x86_reg &/*rd*/, const x86_reg &/*rtemp*/)
{
   assert(!"x86_instr::gen64imm() not supported");
}

void x86_instr::genImmAddrGeneric(insnVec_t *piv, uint32_t addr, 
                                  const x86_reg &rd, bool)
{
   gen32imm(piv, addr, rd);
}

void x86_instr::genImmAddrGeneric(insnVec_t *, uint64_t, 
                                  const x86_reg &, bool)
{
   assert(!"x86_instr::genImmAddrGeneric() not supported for 64-bit addr");
}

void x86_instr::genCallAndLink(insnVec_t *piv, kptr_t from, kptr_t to)
{
   instr_t *i = new x86_instr(call, (kaddrdiff_t)(to - from));
   piv->appendAnInsn(i);
}

void x86_instr::genCallAndLinkGeneric_unknownFromAndToAddr(insnVec_t *piv)
{
   const kptr_t unknown = 0;
   genCallAndLink(piv, unknown, unknown);
}

void x86_instr::genCallAndLinkGeneric_unknownFromAddr(insnVec_t *piv, kptr_t to)
{
   const kptr_t unknown = 0;
   genCallAndLink(piv, unknown, to);
}

// Return the number of insns in [STARTADDR, ENDADDR)
unsigned x86_instr::numInsnsInRange(kptr_t startaddr, kptr_t endaddr) 
{
   unsigned n = 0;
   unsigned long curraddr = startaddr;
   while (curraddr < endaddr) {
      n++;
      x86_instr i((const char *)curraddr);
      curraddr += i.getNumBytes();
   }
   return n;
}
