// springboardSpace.C

extern "C" void _kerninst_springboard_space_();

//unsigned char _kerninst_springboard_space_[16384];



#define nops_8 asm("nop;nop;nop;nop;nop;nop;nop;nop;");
#define nops_16 nops_8 nops_8
#define nops_32 nops_16 nops_16
#define nops_64 nops_32 nops_32
#define nops_128 nops_64 nops_64
#define nops_256 nops_128 nops_128
#define nops_512 nops_256 nops_256
#define nops_1024 nops_512 nops_512
#define nops_2048 nops_1024 nops_1024
#define nops_4096 nops_2048 nops_2048
#define nops_8192 nops_4096 nops_4096
#define nops_16384 nops_8192 nops_8192

void _kerninst_springboard_space_() {
   nops_16384;
}

