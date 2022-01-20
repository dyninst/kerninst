INSTANTIATE_TEMPLATES = true
USES_X11 = true
USES_TCLTK = true
#USING_PURIFY = true
#DISTRIBUTION = true
#USES_GPROF = true
#NOASSERTS = true

TO_CORE = ../..

# Include standard make configuration stuff that applies to everything
# in the paradyn tree.

include $(TO_CORE)/make.config 

# Build for the 64-bit kernels
CXXFLAGS += -D_KDRV64_
ASFLAGS	+= -xarch=v8plusa

# Now make any necessary architecture specific changes to variables:

#OPTIMIZE = 
DEBUG_FLAG = -g
ifdef OPTIMIZE
DEBUG_FLAG =
endif

#ULTRASPARCFLAG = -mcpu=ultrasparc

CXXFLAGS 	+=  $(STLFRIENDLYWARNINGS) $(OPTIMIZE) $(ULTRASPARCFLAG) -D_BIG_ENDIAN_
LIBS		+= 

ifdef NOASSERTS
CXXFLAGS	+= -DNDEBUG
endif

# Include the module-specific Makefile, which defines everything about
# the module that is common across architectures.

include $(TO_CORE)/kerninstapi/make.module.tmpl

# Now make any necessary architecture specific changes to variables:
SRCS += \
	$(TO_KERNINSTD)/basicblock/src/sparc_basicblock.C \
	$(TO_KERNINSTD)/basicblock/src/sparc_bb.C \
	$(TO_KERNINSTD)/bitwise_dataflow_fn/src/sparc_bitwise_dataflow_fn.C \
	$(TO_KERNINSTD)/bitwise_dataflow_fn/src/sparc_mbdf_window.C \
	$(TO_KERNINSTD)/codeObject/src/pcIndepWithFallThruCodeObject.C \
	$(TO_KERNINSTD)/codeObject/src/pcIndepNoFallThruCodeObject.C \
	$(TO_KERNINSTD)/codeObject/src/interProcCallCodeObject.C \
	$(TO_KERNINSTD)/codeObject/src/interProcTailCallCodeObject.C \
	$(TO_KERNINSTD)/codeObject/src/callToObtainPCCodeObject.C \
	$(TO_KERNINSTD)/codeObject/src/condBranchCodeObject.C \
	$(TO_KERNINSTD)/codeObject/src/branchAlwaysCodeObject.C \
	$(TO_KERNINSTD)/codeObject/src/interProcCondBranchCodeObject.C \
	$(TO_KERNINSTD)/codeObject/src/interProcBranchAlwaysCodeObject.C \
	$(TO_KERNINSTD)/codeObject/src/jumpTableCodeObject.C \
	$(TO_KERNINSTD)/codeObject/src/simpleJumpTable32CodeObject.C \
	$(TO_KERNINSTD)/codeObject/src/offsetJumpTable32CodeObject.C \
	$(TO_KERNINSTD)/codeObject/src/recursiveCallCodeObject.C \
	$(TO_KERNINSTD)/codeObject/src/intraProcJmpToFixedAddrCodeObject.C \
	$(TO_KERNINSTD)/emitters/src/sparc_tempBufferEmitter.C \
        $(TO_KERNINSTD)/insnVec/src/sparc_insnVec.C \
        $(TO_KERNINSTD)/function/src/sparc_funkshun.C \
	$(TO_KERNINSTD)/relocatableCode/src/sparc_relocatableCode.C \
	$(TO_KERNINSTD)/sparc_instr/src/sparc_instr.C \
	$(TO_KERNINSTD)/sparc_reg/src/sparc_reg.C \
	$(TO_KERNINSTD)/sparc_reg_set/src/sparc_reg_set.C \
	$(TO_KERNINSTD)/sparc_reg_set/src/sparc_fp_regset.C \
	$(TO_KERNINSTD)/sparc_reg_set/src/sparc_int_regset.C \
	$(TO_KERNINSTD)/sparc_reg_set/src/sparc_misc_regset.C \
	$(TO_KERNINSTD)/sparc_reg_set/src/regSetManager.C \
	$(TO_KERNINSTD)/sparc_reg_set/src/fnRegInfo.C \
	$(TO_KERNINSTD)/usualJump/src/usualJump.C \
	$(TO_KPERFMON)/outliningMgr/src/outlinedGroup.C \
	$(TO_KPERFMON)/outliningMgr/src/hotBlockCalcOracle.C \
	$(TO_KPERFMON)/outliningMgr/src/funcsToIncludeInGroupOracle.C \
	$(TO_KPERFMON)/outliningMgr/src/blockOrderingOrigSeq.C \
	$(TO_KPERFMON)/outliningMgr/src/blockOrderingLongestInlinedPaths.C \
	$(TO_KPERFMON)/outliningMgr/src/blockOrderingGreedyHottestPath.C \
	$(TO_KPERFMON)/outliningMgr/src/blockOrderingChains.C \
	$(TO_KPERFMON)/outliningMgr/src/outliningEmit.C \
	$(TO_KPERFMON)/outliningMgr/src/outliningTest.C \
	$(TO_KPERFMON)/outliningMgr/src/groupMgr.C \
	$(TO_KPERFMON)/outliningMgr/src/usparc_icache.C \
	$(TO_KPERFMON)/traceBuffer/src/traceBuffer.C \
	$(TO_KPERFMON)/virtualization/src/vtimerMgr.C \
	$(TO_KPERFMON)/virtualization/src/vtimerSupport.C \
	$(TO_KPERFMON)/virtualization/src/globalLock.C \
	$(TO_KPERFMON)/virtualization/src/all_vtimers_emit.C \
	$(TO_KPERFMON)/virtualization/src/hash_table_emit.C \
	$(TO_KPERFMON)/virtualization/src/stacklist_emit.C \
	$(TO_KPERFMON)/virtualization/src/cswitchout_emit.C \
	$(TO_KPERFMON)/virtualization/src/cswitchin_emit.C \
	$(TO_KPERFMON)/virtualization/src/vtimer_asserts.C \
	$(TO_KPERFMON)/virtualization/src/vtimerMgr_ultrasparc_2_7.C \
	$(TO_KAPI)/codegen/src/emit-sparc.C \
	$(TO_KAPI)/src/kapi_hwcounter_set_sparc.C

VPATH += $(TO_KERNINSTD)/sparc_instr/src \
	 $(TO_KERNINSTD)/sparc_reg/src \
	 $(TO_KERNINSTD)/sparc_reg_set/src

IFLAGS += -I$(TO_KERNINSTD)/sparc_instr/src \
	  -I$(TO_KERNINSTD)/sparc_reg/src \
	  -I$(TO_KERNINSTD)/sparc_reg_set/src

# Include the "standard program template".  This defines all the
# common targets like "clean", "install", etc.

include $(TO_CORE)/make.library.tmpl


