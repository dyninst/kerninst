// driver.h

#ifndef _DRIVER_H_
#define _DRIVER_H_

#include "common/h/Vector.h"
#include "util/h/mrtime.h"
#include "util/h/StringPool.h"
#include "moduleMgr.h"
#include "kernelDriver.h"
#include "hiddenFns.h"

struct basic_sym_info {
    kptr_t addr;
    unsigned size;
    basic_sym_info(kptr_t iaddr, int isize) : addr(iaddr), size(isize) {
    }
    bool operator<(const basic_sym_info &s) const {
	return (addr < s.addr);
    }
    bool operator==(const basic_sym_info &s) const {
	return (addr == s.addr);
    }
};

bool parse1now(const pdstring &modName,
               const function_t *thisFn,
               kptr_t nextSymbolAddr, // could be (kptr_t)-1
               kptr_t maximumNextFnStartAddr, // data_start + data_len
               dictionary_hash<kptr_t,bool> &interProcJumps,
               pdvector<hiddenFn> &hiddenFns,
               bool verbose_fnParse,
               bool verbose_skips);

void doStuff(unsigned char *mmapptr, unsigned long filelen,
	     bool kernel, const char *justPrintRoutine);

void gather(bool verbose_memUsage,
            bool verbose_skips,
            bool verbose_returningFuncs,
            bool verbose_symbolTable,
            bool verbose_symbolTableTiming, // subset of above
            bool verbose_removeAliases,
            bool verbose_fnParse,
            bool verbose_fnParseTiming,
            bool verbose_summaryFnParse,
            bool verbose_hiddenFns,
            kernelDriver &,
            moduleMgr &theModuleMgr,
            dictionary_hash<kptr_t, bool> &interProcJumps,
            pdvector<hiddenFn> &hiddenFns);

void gather2(bool verbose_memUsage,
             bool verbose_summaryFnParse,
             bool verbose_fnParseTiming, // also passed to gather()
             bool verbose_resolveInterprocJumps,
             bool verbose_bbDFS,
             bool verbose_interProcRegAnalysis,
             bool verbose_interProcRegAnalysisTiming,
             bool verbose_numSuccsPreds,
             bool verbose_fnsFiddlingWithPrivRegs,
             moduleMgr &theModuleMgr,
             dictionary_hash<kptr_t, bool> &interProcJumps);

void gatherMods(moduleMgr &theModuleMgr, const moduleMgr &theModuleMgrAsConst, 
                dictionary_hash<pdstring, kptr_t> &crudeModuleEnds,
                dictionary_hash<kptr_t,bool> &interProcJumps,
                pdvector<basic_sym_info> &allModulesAllSymbols,
                pdvector<kptr_t> &functionsThatReturnForTheCaller,
          pdvector< pair<pdstring,pdstring> > &functionNamesThatReturnForTheCaller,
                pdvector<hiddenFn> &hiddenFns,
                mrtime_t &findNextSymbolStartAddr_totalTime,
                unsigned numModules, char *user_buffer, unsigned long &offset,
                bool verbose_symbolTable, bool verbose_removeAliases, 
                bool verbose_fnParse, bool verbose_skips,
                bool verbose_returningFuncs);

#endif
