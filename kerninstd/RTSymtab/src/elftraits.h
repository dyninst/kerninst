#ifndef _ELFTRAITS_H_
#define _ELFTRAITS_H_

#ifdef sparc_sun_solaris2_7
#include <sys/types.h>
#endif
#include <libelf.h>
#include <libelf.h> // includes <sys/types.h> for us
#include "util/h/kdrvtypes.h"

class elfcommon {
 public:
	static const unsigned int ET_STT_NOTYPE =  STT_NOTYPE;
	static const unsigned int ET_STT_FILE = STT_FILE;
	static const unsigned int ET_STT_SECTION = STT_SECTION;
	static const unsigned int ET_STT_LOPROC = STT_LOPROC;
	static const unsigned int ET_STT_HIPROC = STT_HIPROC;
	static const unsigned int ET_STT_FUNC = STT_FUNC;
	static const unsigned int ET_STT_OBJECT = STT_OBJECT;
	static const unsigned int ET_CLASS32 = ELFCLASS32;
	static const unsigned int ET_DATA2MSB = ELFDATA2MSB;
	static const unsigned int ET_CURRENT = EV_CURRENT;
	static const Elf_Cmd ET_C_READ = ELF_C_READ;
	static const Elf_Kind ET_K_ELF = ELF_K_ELF;

	typedef Elf ET_Elf;
	typedef Elf_Scn ET_Scn;
	typedef Elf_Data ET_Data;
	typedef Elf_Cmd ET_Cmd;
	typedef Elf_Kind ET_Kind;

	static ET_Scn* et_getscn(ET_Elf *elf, size_t index) {
		return elf_getscn(elf, index);
	}
	static ET_Scn* et_nextscn(ET_Elf *elf, ET_Scn *scn) {
		return elf_nextscn(elf, scn);
	}
	static unsigned et_version(unsigned ver) {
		return elf_version(ver);
	}
	static ET_Elf *et_begin(int fildes, ET_Cmd cmd, ET_Elf *ref) {
		return elf_begin(fildes, cmd, ref);
	}
	static int et_end(ET_Elf *elf) {
		return elf_end(elf);
	}
	static ET_Kind et_kind(ET_Elf *elf) {
		return elf_kind(elf);
	}
	static int et_errno() {
		return elf_errno();
	}
	static ET_Data *et_getdata(ET_Scn *scn, ET_Data *data) {
		return elf_getdata(scn, data);
	}
};

template<class addr_t>
class elftraits : public elfcommon {
};

template<>
class elftraits<uint32_t> : public elfcommon {
 public:
	typedef Elf32_Sym ET_Sym;
	typedef Elf32_Addr ET_Addr;
	typedef Elf32_Word ET_Size;
	typedef Elf32_Ehdr ET_Ehdr;
	typedef Elf32_Shdr ET_Shdr;

	static unsigned char ET_ST_BIND(unsigned char info) {
		return ELF32_ST_BIND(info);
	}
	static unsigned char ET_ST_TYPE(unsigned char info) {
		return ELF32_ST_TYPE(info);
	}
	static ET_Shdr* et_getshdr(ET_Scn *scn) {
		return elf32_getshdr(scn);
	}
	static ET_Ehdr* et_getehdr(ET_Elf *elf) {
		return elf32_getehdr(elf);
	}
};

template<>
class elftraits<uint64_t> : public elfcommon {
 public:
	typedef Elf64_Sym ET_Sym;
	typedef Elf64_Addr ET_Addr;
	typedef Elf64_Xword ET_Size;
	typedef Elf64_Ehdr ET_Ehdr;
	typedef Elf64_Shdr ET_Shdr;

	static unsigned char ET_ST_BIND(unsigned char info) {
		return ELF64_ST_BIND(info);
	}
	static unsigned char ET_ST_TYPE(unsigned char info) {
		return ELF64_ST_TYPE(info);
	}
	static ET_Shdr* et_getshdr(ET_Scn *scn) {
		return elf64_getshdr(scn);
	}
	static ET_Ehdr* et_getehdr(ET_Elf *elf) {
		return elf64_getehdr(elf);
	}
};

typedef elftraits<dptr_t> DELF;
typedef elftraits<kptr_t> KELF;

#endif
