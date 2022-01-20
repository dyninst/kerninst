#include <utility>
#include "common/h/Vector.h"
#include "funkshun.h"
#include "moduleMgr.h"
#include "instrumenter.h"
#include "instr.h"
#include "insnVec.h"
#include "kapi.h"

#ifdef sparc_sun_solaris2_7
#include "sparc_instr.h" //for sparc_cfi
#elif defined(i386_unknown_linux2_4)
#include "x86_instr.h" // for x86_cfi
#elif defined(rs6000_ibm_aix5_1)
#include "power_instr.h" // for power_cfi
#endif

extern moduleMgr *global_moduleMgr;
extern instrumenter *theGlobalInstrumenter;

// Return the starting address of a function along with its size. Note
// that for non-contiguous functions we have to return a vector
static pdvector< pair<kptr_t,unsigned> >
getFnCodeRangesForDisassFromMem(const function_t *fn) {
   // A helper function for calculating the last parameter to
   // pendingDisassFnFromMemRequest::make_request()

   const moduleMgr &theModuleMgr = *global_moduleMgr;
   
   const fnCode &theFnOrigCode = fn->getOrigCode();
   pdvector< pair<kptr_t,unsigned> > result = theFnOrigCode.getCodeRanges();
   if (result.size() == 0) {
      // What we have here is a function that is so badly mauled, not only is
      // it unparsed, but it also has a blank entry for orig code.
      // Let's do it a favor by constructing a decent range: from the entry of
      // that function to the entry of the next highest function.

      unsigned numBytes = 1000;
      
      const function_t *nextFn =
         theModuleMgr.findNextHighestFnOrNULL(fn->getEntryAddr());
      if (nextFn == NULL)
         numBytes = 1000; // arbitrary
      else {
         numBytes = nextFn->getEntryAddr() - fn->getEntryAddr();
         if (numBytes > 1000)
            numBytes = 1000;
      }
      
      result += make_pair(fn->getEntryAddr(), numBytes);
      assert(result.size() == 1);
   }

   return result;
}

// Read-in the binary contents of the given basic block into *pdata.
// The vector will have a single pair in it: <start_addr, contents>
static void getBBContents(const kapi_function &kfunc, bbid_t bbid,
			  pdvector< pair<kptr_t, pdvector<uint32_t> > > *pdata)
{
    kapi_basic_block kbb;
    if (kfunc.findBasicBlockById(bbid, &kbb)) {
	assert(false && "Basic block not found");
    }
    kptr_t start_addr = kbb.getStartAddr();
    unsigned bytes_in_range = (kbb.getEndAddrPlus1() - start_addr);
    pdvector<uint32_t> vrange =
	theGlobalInstrumenter->peek_kernel_contig(start_addr, bytes_in_range);

    pdata->push_back(pair<kptr_t, pdvector<uint32_t> >(start_addr, vrange));
}

// Read-in the binary contents of the given function into *pdata.
// If the function is non-contiguous (has several chunks) we return a vector
// of pairs <chunk_start, chunk_contents>
static void getFnContents(const kapi_function &kfunc,
			  pdvector< pair<kptr_t, pdvector<uint32_t> > > *pdata)
{
    kptr_t entryAddr = kfunc.getEntryAddr();
    const moduleMgr &theModuleMgr = *global_moduleMgr; // we need const !
    const function_t &fn = theModuleMgr.findFn(entryAddr, true);
    pdvector< pair<kptr_t,unsigned> > rangesToDisass =     
	getFnCodeRangesForDisassFromMem(&fn);

    pdvector< pair<kptr_t, unsigned> >::const_iterator irange =
	rangesToDisass.begin();
    for (; irange != rangesToDisass.end(); irange++) {
	pdvector<uint32_t> vrange = theGlobalInstrumenter->peek_kernel_contig(
	    irange->first, irange->second);
	pdata->push_back(pair<kptr_t, pdvector<uint32_t> >(irange->first,
							   vrange));
    }
}

// Read-in the contents of the given memory range into *pdata.
// The vector will have a single pair in it: <start_addr, contents>
static void getRangeContents(kptr_t start, kptr_t finish,
			     pdvector< pair<kptr_t, pdvector<uint32_t> > > *pdata)
{
    unsigned bytes_in_range = (finish - start);
    pdvector<uint32_t> vrange =
	theGlobalInstrumenter->peek_kernel_contig(start, bytes_in_range);
    pdata->push_back(pair<kptr_t, pdvector<uint32_t> >(start, vrange));
}
			     
// Construct the fnCode object from the data
static void getFnCode(const pdvector< pair<kptr_t, pdvector<uint32_t> > > &data,
		      fnCode *fnCode)
{
    // .first: address   .second: a contiguous chunk as an insnVec
    pdvector< pair<kptr_t, pdvector<uint32_t> > >::const_iterator citer = 
	data.begin();
    pdvector< pair<kptr_t, pdvector<uint32_t> > >::const_iterator cfinish =
	data.end();
    for (; citer != cfinish; ++citer) {
	const kptr_t chunkStartAddr = citer->first;
	const pdvector<uint32_t> &chunkData = citer->second;
	insnVec_t *chunkInsnVec = insnVec_t::getInsnVec(chunkData.begin(), 4*chunkData.size());
      
	fnCode->addChunk(chunkStartAddr, chunkInsnVec, false); 
        // false -- don't resort chunks yet
    }
    fnCode->sortChunksNow();
}

// Get the original code for the function (as if no instrumentation took place)
const fnCode* getFnCodeOrig(const kapi_function &kfunc)
{
    const moduleMgr &theModuleMgr = *global_moduleMgr;
    kptr_t fnEntryAddr = kfunc.getEntryAddr();
    const function_t &fn = theModuleMgr.findFn(fnEntryAddr, true);
    const fnCode &theFnCode = fn.getOrigCode();

    return &theFnCode;
}

// Get the original code for the basic block (as if no instrumentation took
// place)
ierr getBBCodeOrig(const kapi_function &kfunc, bbid_t bbid, fnCode *pCode)
{
    const moduleMgr &theModuleMgr = *global_moduleMgr;
    kptr_t fnEntryAddr = kfunc.getEntryAddr();
    const function_t &fn = theModuleMgr.findFn(fnEntryAddr, true);
    const fnCode &theFnCode = fn.getOrigCode();

    kapi_basic_block kbb;
    ierr rv;
    if ((rv = kfunc.findBasicBlockById(bbid, &kbb)) < 0) {
	return rv;
    }

    kptr_t startaddr = kbb.getStartAddr();
    kptr_t finishaddr = kbb.getEndAddrPlus1();
    insnVec_t *theInsnVec = insnVec_t::getInsnVec();
    assert(theInsnVec != NULL);
    kptr_t addr = startaddr;
    while (addr < finishaddr) {
	instr_t *i = instr_t::getInstr(*theFnCode.get1Insn(addr));
        assert(i != NULL);
	theInsnVec->appendAnInsn(i);
	addr += i->getNumBytes();
    }
    pCode->addChunk(startaddr, theInsnVec, true);

    return 0;
}

// Construct the disass object from the function code
static void disassFnCode(const fnCode &fnCode,
			 const kapi_function *pfunc,
			 pdstring (*disassemble_destfuncaddr)(kptr_t),
			 kapi_disass_object *pdis)
{
    fnCode::const_iterator chunkiter = fnCode.begin();
    fnCode::const_iterator chunkfinish = fnCode.end();
    for (; chunkiter != chunkfinish; ++chunkiter) {
	const fnCode::codeChunk &theChunk = *chunkiter;

	kptr_t addr = chunkiter->startAddr;
	kapi_disass_chunk kchunk(addr);

	fnCode::codeChunk::const_iterator insniter = theChunk.begin();
	fnCode::codeChunk::const_iterator insnfinish = theChunk.end();
	for (; insniter != insnfinish; ++insniter) {
	    instr_t::disassemblyFlags fl;
	    fl.pc = addr;
	    fl.disassemble_destfuncaddr = disassemble_destfuncaddr;

	    // We want to provide a disassembly of this instruction unless:
	    // the function has been parsed OK yet this "insn" (probably data)
	    // doesn't fall in some basic block.
	    // In particular, we do (try to) disassemble this insn in the case
	    // where the function is unparsed (in skips.txt or failed to parse)
	    // or even when there was not function specified (range disass)
	    const bool tryToDisassembleThisInsn = 
		(pfunc == 0 || pfunc->isUnparsed() || 
		 (pfunc->getBasicBlockIdByAddress(addr) != (bbid_t)-1));
	    // (bbid_t)-1 if e.g. the fn is fried

	    const instr_t *insn = *insniter;
	    pdstring str;

	    if (tryToDisassembleThisInsn) {
		try {
		    insn->getDisassembly(fl);
		    str += fl.result;
		    if (fl.destfunc_result.length() > 0)
			str += " ";
		}
		catch (instr_t::unimplinsn) {
		    str += " (data?)";
		}
		catch (instr_t::unknowninsn) {
		    str += " (data?)";
		}
	    }
	    else {
		str += " (data?)";
	    }

#ifndef i386_unknown_linux2_4
	    uint32_t raw = insn->getRaw();
#else
            const char *raw = ((const x86_instr*)insn)->getRawBytes();
#endif
	    bool has_dest_func =  (fl.destfunc_result.length() > 0);
	    pdstring dest_func_info;
	    if (has_dest_func) {
#ifdef sparc_sun_solaris2_7
	        sparc_instr::sparc_cfi cfi;
#elif defined(i386_unknown_linux2_4)
	        x86_instr::x86_cfi cfi;
#elif defined(rs6000_ibm_aix5_1)
	        power_instr::power_cfi cfi;
#endif
		insn->getControlFlowInfo(&cfi);
#ifndef i386_unknown_linux2_4
		dest_func_info += num2string(fl.pc + cfi.offset_nbytes);
#else
		dest_func_info += num2string(fl.pc + insn->getNumBytes() + cfi.offset_nbytes);
#endif
		dest_func_info += " \"";
		dest_func_info += fl.destfunc_result;
		dest_func_info += "\"";
	    }
		
#ifndef i386_unknown_linux2_4
	    kapi_disass_insn kinsn(&raw, insn->getNumBytes(), str.c_str(),
				   has_dest_func, dest_func_info.c_str());
#else
            kapi_disass_insn kinsn(raw, insn->getNumBytes(), str.c_str(),
				   has_dest_func, dest_func_info.c_str());
#endif
	    kchunk.push_back(kinsn);

	    addr += insn->getNumBytes();
	}
	pdis->push_back(kchunk);
    }
}

// Return the name of a function starting at addr. Used to display
// resolved callee names next to call instructions
static pdstring disassemble_destfuncaddr(kptr_t addr)
{
    assert(global_moduleMgr);
    const moduleMgr &mm = *global_moduleMgr;

    pair<pdstring, const function_t*> modAndFn =
	mm.findModAndFnOrNULL(addr, true); // true --> entryOnly
    if (modAndFn.second == NULL) {
	return pdstring();
    }

    const pdstring &modName = modAndFn.first;
    const function_t *fn = modAndFn.second;

    return pdstring("(") + modName + "/" + fn->getPrimaryName().c_str() + ")";
}

kapi_disass_insn::kapi_disass_insn(const void *iraw, unsigned inum_bytes,
				   const char *idisassembly,
				   bool ihas_dest_func,
				   const char *idest_func_info) :
    num_bytes(inum_bytes), disassembly(strdup(idisassembly)), 
    has_dest_func(ihas_dest_func)
{
    if ((raw = malloc(num_bytes)) == 0) {
	assert(false && "Out of memory");
    }
    memcpy(raw, iraw, num_bytes);
    if (has_dest_func) {
	dest_func_info = strdup(idest_func_info);
    }
}

kapi_disass_insn::kapi_disass_insn(const kapi_disass_insn &src) :
    num_bytes(src.num_bytes), disassembly(strdup(src.disassembly)), 
    has_dest_func(src.has_dest_func)
{
    if ((raw = malloc(num_bytes)) == 0) {
	assert(false && "Out of memory");
    }
    memcpy(raw, src.raw, num_bytes);
    if (has_dest_func) {
	dest_func_info = strdup(src.dest_func_info);
    }
}

kapi_disass_insn::~kapi_disass_insn()
{
    free(raw);
    free(disassembly);
    if (has_dest_func) {
	free(dest_func_info);
    }
}

// Get the binary representation of the instruction
const void *kapi_disass_insn::getRaw() const
{
    return raw;
}

// Get the size of the binary representation
unsigned kapi_disass_insn::getNumBytes() const
{
    return num_bytes;
}

// Get the textual representation of the instruction
const char *kapi_disass_insn::getDisassembly() const 
{
    return disassembly;
}

// True if insn is a call and we know its destination
bool kapi_disass_insn::hasDestFunc() const 
{
    return has_dest_func;
}

// The name of the function we are calling
const char *kapi_disass_insn::getDestFuncInfo() const 
{
    return dest_func_info;
}

kapi_disass_chunk::kapi_disass_chunk(kptr_t istart_addr) : 
    start_addr(istart_addr)
{
}

kptr_t kapi_disass_chunk::getStartAddr() const
{
    return start_addr;
}

void kapi_disass_chunk::push_back(const kapi_disass_insn &kinsn)
{
    rep.push_back(kinsn);
}

kapi_disass_chunk::const_iterator kapi_disass_chunk::begin() const
{
    return rep.begin();
}

kapi_disass_chunk::const_iterator kapi_disass_chunk::end() const
{
    return rep.end();
}

kapi_disass_object::kapi_disass_object()
{
}

void kapi_disass_object::push_back(const kapi_disass_chunk &kchunk)
{
    rep.push_back(kchunk);
}

kapi_disass_object::const_iterator kapi_disass_object::begin() const
{
    return rep.begin();
}

kapi_disass_object::const_iterator kapi_disass_object::end() const
{
    return rep.end();
}

// Disassemble the specified function/bb and fill-in the disass object
// If useOrigImage is true, we disassemble the original image (as if
// no instrumentation took place)
ierr kapi_manager::getDisassObject(const kapi_function &kfunc, bbid_t bbid,
				   bool useOrigImage, kapi_disass_object *pdis)
{
    bool basicBlock = (bbid != (bbid_t)-1);
    fnCode theCode(fnCode::empty);
    const fnCode *pCode;

    if (!useOrigImage) {
	// Need to disassemble the current image of the function/bb
	pdvector< pair<kptr_t, pdvector<uint32_t> > > data;
	if (!kfunc.isUnparsed() && basicBlock) {
	    // Disassemble the selected basic block
	    getBBContents(kfunc, bbid, &data);
	}
	else {
	    // Disassemble the entire function: either the basic block was not
	    // specified or the function can not be parsed
	    getFnContents(kfunc, &data);
	}
	
	getFnCode(data, &theCode);
	pCode = &theCode;
    }
    else {
	// Need to disassemble the original contents of the function/bb, that
	// is contents before any instrumentation took place
	if (basicBlock) {
	    getBBCodeOrig(kfunc, bbid, &theCode);
	    pCode = &theCode;
	}
	else {
	    pCode = getFnCodeOrig(kfunc);
	}
    }
    disassFnCode(*pCode, &kfunc, disassemble_destfuncaddr, pdis);

    return 0;
}

// Disassemble the specified range of addresses and fill-in the disass
// object
ierr kapi_manager::getDisassObject(kptr_t start, kptr_t finish,
				   kapi_disass_object *pdis)
{
    pdvector< pair<kptr_t, pdvector<uint32_t> > > data;
    getRangeContents(start, finish, &data);

    fnCode theCode(fnCode::empty);
    getFnCode(data, &theCode);

    disassFnCode(theCode, 0 /* no function */, disassemble_destfuncaddr, pdis);

    return 0;
}
