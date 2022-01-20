// x86_parse.h
#ifndef _X86_PARSE_H
#define _X86_PARSE_H

#include "common/h/Types.h" // Address
#include <stdlib.h>

// The code necessary for parsing x86 instruction is borrowed from the
// Dyninst ia32 decoder. Of course, we modify it for our own purposes.

/*--------------------------- IA-32 decoder ---------------------------*/

// Code originally taken from dyninstAPI/src/arch-ia32.C

// tables and pseudotables
enum {
  t_ill=0, t_oneB, t_twoB, t_prefixedSSE, t_coprocEsc, t_grp, 
  t_sse, t_grpsse, t_3dnow, t_done=99
};

// groups
enum {
  Grp1a=0, Grp1b, Grp1c, Grp1d, Grp2, Grp3a, Grp3b, Grp4, Grp5, Grp6, Grp7,
  Grp8, Grp9, Grp11, Grp12, Grp13, Grp14, Grp15, Grp16, GrpAMD
};

// SSE
enum {
  SSE10=0, SSE11, SSE12, SSE13, SSE14, SSE15, SSE16, SSE17,
  SSE28, SSE29, SSE2A, SSE2B, SSE2C, SSE2D, SSE2E, SSE2F,
  SSE50, SSE51, SSE52, SSE53, SSE54, SSE55, SSE56, SSE57,
  SSE58, SSE59, SSE5A, SSE5B, SSE5C, SSE5D, SSE5E, SSE5F,
  SSE60, SSE61, SSE62, SSE63, SSE64, SSE65, SSE66, SSE67,
  SSE68, SSE69, SSE6A, SSE6B, SSE6C, SSE6D, SSE6E, SSE6F,
  SSE70, SSE74, SSE75, SSE76,
  SSE7E, SSE7F,
  SSEC2, SSEC4, SSEC5, SSEC6,
  SSED1, SSED2, SSED3, SSED4, SSED5, SSED6, SSED7,
  SSED8, SSED9, SSEDA, SSEDB, SSEDC, SSEDD, SSEDE, SSEDF,
  SSEE0, SSEE1, SSEE2, SSEE3, SSEE4, SSEE5, SSEE6, SSEE7,
  SSEE8, SSEE9, SSEEA, SSEEB, SSEEC, SSEED, SSEEE, SSEEF,
  SSEF1, SSEF2, SSEF3, SSEF4, SSEF5, SSEF6, SSEF7,
  SSEF8, SSEF9, SSEFA, SSEFB, SSEFC, SSEFD, SSEFE
};

// SSE groups
enum {
  G12SSE010B=0, G12SSE100B, G12SSE110B,
  G13SSE010B, G13SSE100B, G13SSE110B,
  G14SSE010B, G14SSE011B, G14SSE110B, G14SSE111B,
};

// addressing methods (see appendix A-2)
// I've added am_reg (for registers implicitely encoded in instruciton), 
// and am_stackX for stack operands [this kinda' messy since there are 
// actually two operands: the stack byte/word/dword and the (E)SP register 
// itself - but is better than naught]
// added: am_reg, am_stack, am_allgprs
enum { am_A=1, am_C, am_D, am_E, am_F, am_G, am_I, am_J, am_M, am_O, // 10
       am_P, am_Q, am_R, am_S, am_T, am_V, am_W, am_X, am_Y, am_reg, // 20
       am_stackPSH, am_stackPOP, am_allgprs }; // PuSH and POP produce different addresses

// operand types - idem, but I invented quite a few to make implicit operands explicit.
enum { op_a=1, op_b, op_c, op_d, op_dq, op_p, op_pd, op_pi, op_ps, 
       op_q, op_s, op_sd, op_ss, op_si, op_v, op_w, op_lea, op_allgprs, op_512 };

// registers [only fancy names, not used right now]
enum { r_AL=0, r_CL, r_DL, r_BL, r_AH, r_CH, r_DH, r_BH,
       r_AX, r_CX, r_DX, r_BX, r_SP, r_BP, r_SI, r_DI,
       r_EAX, r_ECX, r_EDX, r_EBX,
       r_ESP, r_EBP, r_ESI, r_EDI,
       r_ES, r_CS, r_SS, r_DS, r_FS, r_GS,
       r_EFLAGS, r_EIP, r_FP,
       r_CR0, r_CR1, r_CR2, r_CR3, r_CR5,
       r_DR0, r_DR1, r_DR2, r_DR3, r_DR4, r_DR5, r_DR6, r_DR7,
       r_EDXEAX, r_ECXEBX };
// last two are hacks for cmpxch8b which would have 5 operands otherwise!!!

// registers used for memory access
enum { mEAX=0, mECX, mEDX, mEBX,
       mESP, mEBP, mESI, mEDI };

enum { mAX=0, mCX, mDX, mBX,
       mSP, mBP, mSI, mDI };

/*-------------------- x86 instruction declarations --------------------*/

// Code originally taken from dyninstAPI/src/arch-x86.h

/* operand types */
typedef unsigned char byte_t;   /* a byte operand */
typedef short word_t;  /* a word (16-bit) operand */
typedef int dword_t;   /* a double word (32-bit) operand */

/* operand sizes */
#define byteSzB (1)    /* size of a byte operand */
#define wordSzB (2)    /* size of a word operand */
#define dwordSzB (4)   /* size of a dword operand */
#define qwordSzB (8)   /* size of a qword operand */
#define dqwordSzB (16)   /* size of a double qword (oword) operand */

/* The following values are or'ed together to form an instr type descriptor */

/* the instruction types of interest */
#define IS_CALL (1<<1)   /* call instruction */
#define IS_RET  (1<<2)   /* near return instruction */
#define IS_RETF (1<<3)   /* far return instruction */
#define IS_JUMP (1<<4)   /* jump instruction */
#define IS_JCC  (1<<5)   /* conditional jump instruction */
#define IS_INT  (1<<6)   /* interrupt */
#define ILLEGAL (1<<7)   /* illegal instruction */

/* addressing modes for calls and jumps */
#define REL_B       (1<<10)  /* relative address, byte offset */
#define REL_W       (1<<11)  /* relative address, word offset */
#define REL_D       (1<<12)  /* relative address, dword offset */
#define REL_X       (1<<13)  /* relative address, word or dword offset */
#define INDIR       (1<<14)  /* register or memory indirect */
#define PTR_WW      (1<<15)  /* 4-byte pointer */
#define PTR_WD      (1<<16)  /* 6-byte pointer */
#define PTR_WX      (1<<17)  /* 4 or 6-byte pointer */

/* prefixes */
#define PREFIX_INST (1<<20) /* instruction prefix */
#define PREFIX_SEG  (1<<21) /* segment override prefix */
#define PREFIX_OPR  (1<<22) /* operand size override */
#define PREFIX_ADDR (1<<23) /* address size override */

/* end of instruction type descriptor values */


/* opcodes of some instructions */
#define PUSHAD   (0x60)
#define PUSHFD   (0x9C)
#define POPAD    (0x61)
#define POPFD    (0x9D)
#define PUSH_DS  (0x1E)
#define POP_DS   (0X1F)
#define NOP      (0x90)

#define JCXZ     (0xE3)

#define JE_R8    (0x74)
#define JNE_R8   (0x75)
#define JL_R8    (0x7C)
#define JLE_R8   (0x7E)
#define JG_R8    (0x7F)
#define JGE_R8   (0x7D)

#define FSAVE    (0x9BDD)
#define FSAVE_OP (6)

#define FRSTOR   (0xDD)
#define FRSTOR_OP (4)

/* limits */
#define MIN_IMM8 (-128)
#define MAX_IMM8 (127)
#define MIN_IMM16 (-32768)
#define MAX_IMM16 (32767)

// Size of floating point information saved by FSAVE
#define FSAVE_STATE_SIZE 108

enum dynamic_call_address_mode {
  REGISTER_DIRECT, REGISTER_INDIRECT,
  REGISTER_INDIRECT_DISPLACED, SIB, DISPLACED
};

class ia32_instruction;
/*
   get_instruction: get the instruction that starts at instr.
   return the size of the instruction and set instType to a type descriptor
*/
unsigned get_instruction(const unsigned char *addr, unsigned &instType,
			 ia32_instruction &instr);

/* get the target of a jump or call */
Address get_target(const unsigned char *instr, unsigned type, unsigned size,
		   Address addr);

/* get the displacement of a jump or call */
int displacement(const unsigned char *instr, unsigned type);

/*--------------------------- IA-32 decoder ---------------------------*/

// Code originally taken from dyninstAPI/src/arch-ia32.h

#define PREFIX_LOCK   0xF0
#define PREFIX_REPNZ  0xF2
#define PREFIX_REP    0xF3

#define PREFIX_SEGCS  0x2E
#define PREFIX_SEGSS  0x36
#define PREFIX_SEGDS  0x3E
#define PREFIX_SEGES  0x26
#define PREFIX_SEGFS  0x64
#define PREFIX_SEGGS  0x65

#define PREFIX_BRANCH0 0x2E
#define PREFIX_BRANCH1 0x3E

#define PREFIX_SZOPER  0x66
#define PREFIX_SZADDR  0x67


class ia32_prefixes {
 public:
   friend ia32_prefixes& ia32_decode_prefixes(const unsigned char* addr, ia32_prefixes&);
 private:
   unsigned int count;
   // At most 4 prefixes are allowed for Intel 32-bit CPUs
   // There also 4 groups, so this array is 0 if no prefix
   // from that group is present, otherwise it contains the
   // prefix opcode
   unsigned char prfx[4];
 public:
   unsigned int const getCount() const { return count; }
   unsigned char getPrefix(unsigned char group) const { return prfx[group]; }
};

ia32_prefixes&
ia32_decode_prefixes(const unsigned char* addr, ia32_prefixes&);


struct ia32_memacc {
   bool is;
   bool read;
   bool write;
   bool nt;     // non-temporal, e.g. movntq...
   bool prefetch;

   bool addr32; // true if 32-bit addressing, false otherwise
   int imm;
   int scale;
   int regs[2]; // register encodings (in ISA order): 0-7
                // (E)AX, (E)CX, (E)DX, (E)BX
                // (E)SP, (E)BP, (E)SI, (E)DI

   int size;
   int sizehack;  // register (E)CX or string based
   int prefetchlvl; // prefetch level
   int prefetchstt; // prefetch state (AMD)

   ia32_memacc() : is(false), read(false), write(false), nt(false),
       prefetch(false), addr32(true), imm(0), scale(0), size(0), sizehack(0),
       prefetchlvl(-1), prefetchstt(-1) {
      regs[0] = -1;
      regs[1] = -1;
   }
 
   /* ia32_memacc() { */
/*       regs[0] = -1; */
/*       regs[1] = -1; */
/*    } */

   ia32_memacc(const ia32_memacc &src) : is(src.is), read(src.read), 
      write(src.write), nt(src.nt), prefetch(src.prefetch), addr32(src.addr32),
      imm(src.imm), scale(src.scale), size(src.size), sizehack(src.sizehack),
      prefetchlvl(src.prefetchlvl), prefetchstt(src.prefetchstt) {
      regs[0] = src.regs[0];
      regs[1] = src.regs[1];
   }

   void set16(int reg0, int reg1, int disp) { 
      is = true;
      addr32  = false; 
      regs[0] = reg0; 
      regs[1] = reg1; 
      imm     = disp;
   }

   void set32(int reg, int disp) { 
      is = true;
      regs[0] = reg; 
      imm     = disp;
   }

   void set32sib(int base, int scal, int indx, int disp) {
      is = true;
      regs[0] = base;
      regs[1] = indx;
      scale   = scal;
      imm     = disp;
   }

   void setXY(int reg, int _size, int _addr32) {
      is = true;
      regs[0] = reg;
      size = _size;
      addr32 = _addr32;
   }
};


enum sizehacks {
   shREP=1,
   shREPECMPS,
   shREPESCAS,
   shREPNECMPS,
   shREPNESCAS
};


struct ia32_condition {
   bool is;
   // TODO: add a field/hack for ECX [not needed for CMOVcc, but for Jcc]
   int tttn;

   ia32_condition() : is(false) {}
   void set(int _tttn) { is = true; tttn = _tttn; }
};


// Operand Semantics - these make explicit all the implicit stuff in the 
// Intel tables. They are needed for memory access, but may be useful for 
// other things: dataflow etc. Instructions that do not deal with memory 
// are not tested, so caveat emptor...
// Also note that the stack is never specified as an operand in Intel 
// tables, so it has to be dealt with here.

enum { 
   sNONE=0, // the instr does something that can't be classified as rd/wr
   s1R,     // reads one operand, e.g. jumps
   s1W,     // e.g. lea
   s1RW,    // one operand read and written, e.g. inc
   s1R2R,   // reads two operands, e.g. cmp
   s1W2R,   // second operand read, first operand written (e.g. mov)
   s1RW2R,  // two operands read, first written (e.g. add)
   s1RW2RW, // e.g. xchg
   s1W2R3R, // e.g. imul
   s1W2W3R, // e.g. les
   s1W2RW3R, // some mul
   s1W2R3RW, // (stack) push & pop
   s1RW2R3R, // shld/shrd
   s1RW2RW3R, // [i]div, cmpxch8b
}; // should be no more than 2^16 otherwise adjust FPOS below

#define FPOS 16

enum {
   fNT=1,   // non-temporal
   fPREFETCHNT,
   fPREFETCHT0,
   fPREFETCHT1,
   fPREFETCHT2,
   fPREFETCHAMDE,
   fPREFETCHAMDW,
   fCALL,
   fNEARRET,
   fFARRET,
   fIRET,
   fENTER,
   fLEAVE,
   fXLAT,
   fIO,
   fSEGDESC,
   fCOND,
   fCMPXCH,
   fCMPXCH8,
   fINDIRCALL,
   fINDIRJUMP,
   fFXSAVE,
   fFXRSTOR,
   fCLFLUSH,
   fREP,   // only rep prefix allowed: ins, movs, outs, lods, stos
   fSCAS,
   fCMPS
};

struct ia32_operand {  // operand as given in Intel book tables
   unsigned int admet;  // addressing method
   unsigned int optype; // operand type;
};


// An instruction table entry
struct ia32_entry {
   char *name;           // name of the instruction (for debbuging only)
   unsigned int otable;  // next opcode table; if t_done it is the current one
   unsigned char tabidx; // index to look at, 0 if easy to deduce from opcode
   bool hasModRM;        // true if the instruction has a MOD/RM byte
   ia32_operand operands[3];  // operand descriptors
   unsigned int legacyType;   // legacy type of the instruction (e.g. IS_CALL)

   // code to decode memory access - this field should be seen as two 16 bit 
   // fields. the lower half gives operand semantics, e.g. s1RW2R, the upper 
   // half is a fXXX hack if needed
   unsigned int opsema;  
};

class ia32_instruction {
   friend unsigned get_instruction(const unsigned char* addr, 
                                   unsigned &insnType,
                                   ia32_instruction &instr);
   friend unsigned int ia32_decode_operands (const ia32_prefixes& pref, 
					     const ia32_entry& gotit, 
					     const unsigned char* addr, 
					     ia32_instruction& instruct);
   template <unsigned int capa>
   friend ia32_instruction& ia32_decode(const unsigned char* addr, 
					ia32_instruction& instruct);
   friend unsigned int ia32_decode_operands (const ia32_prefixes& pref, 
					     const ia32_entry& gotit, 
					     const unsigned char* addr, 
					     ia32_instruction& instruct,
					     ia32_memacc *mac = NULL);
   friend ia32_instruction& ia32_decode_FP(const ia32_prefixes& pref, 
					   const unsigned char* addr,
					   ia32_instruction& instruct);
   friend unsigned int ia32_emulate_old_type(ia32_instruction& instruct);
   friend ia32_instruction& ia32_decode_FP(unsigned int opcode, 
					   const ia32_prefixes& pref,
					   const unsigned char* addr, 
					   ia32_instruction& instruct,
					   ia32_memacc *mac = NULL);

   unsigned int   size;
   ia32_prefixes  prf;
   ia32_memacc    mac[3];
   ia32_condition cond;
   unsigned int   legacy_type;
   int            modrm_offset; // 0 if no modrm byte
   int            opcode_offset; // 0 if no prefix
   int            immed_offset; // 0 if no immediate data
   ia32_entry     *entry;
   char           *name; // needed if not defined in entry->name 

 public:
   ia32_instruction(ia32_memacc* _mac = NULL, ia32_condition* _cnd = NULL,
		    ia32_entry* _entry = NULL, char* _name = NULL)
      : size(0), legacy_type(0), modrm_offset(0), 
        opcode_offset(0), immed_offset(0), entry(_entry), name(_name) {

      if(_mac) {
	 mac[0] = *_mac;
	 mac[1] = *(_mac+1);
	 mac[2] = *(_mac+2);
      }
      if(_cnd)
	 cond = *_cnd;
   }

   ia32_instruction(const ia32_instruction &src)
      : size(src.size), prf(src.prf), mac(src.mac),
        cond(src.cond), legacy_type(src.legacy_type), 
        modrm_offset(src.modrm_offset), 
        opcode_offset(src.opcode_offset), immed_offset(src.immed_offset), 
        entry(src.entry), name(src.name) {}

   ~ia32_instruction() {}

   unsigned int getSize() const { return size; }
   unsigned int getLegacyType() const { return legacy_type; }
   int getModRmOffset() const { return modrm_offset; }
   int getOpcodeOffset() const { return opcode_offset; }
   int getImmedOffset() const { return immed_offset; }
   const ia32_memacc& getMac(int which) const { return mac[which]; }
   const ia32_condition& getCond() const { return cond; }
   const ia32_entry& getEntry() const { return *entry; }
   const char* getName() const { return name; }
};

// VG(02/07/2002): Information that the decoder can return is
//   #defined below. The decoder always returns the size of the 
//   instruction because that has to be determined anyway.
//   Please don't add things that should be external to the
//   decoder, e.g.: how may bytes a relocated instruction needs
//   IMHO that stuff should go into inst-x86...

#define IA32_DECODE_PREFIXES	(1<<0)
#define IA32_DECODE_MNEMONICS	(1<<1)
#define IA32_DECODE_OPERANDS	(1<<2)
#define IA32_DECODE_JMPS	(1<<3)
#define IA32_DECODE_MEMACCESS	(1<<4)
#define IA32_DECODE_CONDITION 	(1<<5)

#define IA32_FULL_DECODER 0xFFFFFFFFUL
#define IA32_SIZE_DECODER 0

template <unsigned int capabilities>
ia32_instruction& ia32_decode(const unsigned char* addr, ia32_instruction&);
// If typing the template every time is a pain, the following should help:
#define ia32_decode_all  ia32_decode<IA32_FULL_DECODER>
#define ia32_decode_size ia32_decode<IA32_SIZE_DECODER>
#define ia32_size(a,i)   ia32_decode_size((a),(i)).size

#endif /* _X86_PARSE_H */
