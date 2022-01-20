INSTANTIATE_TEMPLATES = true
#EVEN_EXPENSIVE_ASSERTS = true
#USING_PURIFY = true
#APP_PURE_OPTIONS += 
#USES_GPROF = true
#DISTRIBUTION = true

TO_CORE = ../../..
TO_KERNINST_DRIVER = $(TO_CORE)/kerninstdriver
TO_KERNINSTD = $(TO_CORE)/kerninstd

KERNEL = true

USES_ELF = true

# Include standard make configuration stuff that applies to everything
# in the paradyn tree.

include $(TO_CORE)/make.config 

CXXFLAGS += -D_KDRV64_

ifdef EVEN_EXPENSIVE_ASSERTS
CXXFLAGS += -D_DO_EVEN_EXPENSIVE_ASSERTS_
endif

# Now make any necessary architecture specific changes to variables:

#NOASSERTS = true
OPTIMIZE = 
DEBUG_FLAG = -g
ifdef OPTIMIZE
DEBUG_FLAG = 
endif

ifdef KERNEL
CXXFLAGS	+= -D_KERNEL_POSSIBLE_
endif

# ultrasparc flag is presently commented out because gcc 2.95 actually emits
# ultrasparc instructions in that case, which binutils 2.9.1's assembler
# doesn't recognize
#ULTRASPARCFLAG = -mcpu=ultrasparc

CXXFLAGS 	+=  $(STLFRIENDLYWARNINGS) $(OPTIMIZE) $(ULTRASPARCFLAG) -D_BIG_ENDIAN_
LIBS		+= 

ifdef NOASSERTS
CXXFLAGS	+= -DNDEBUG
endif

# Include the module-specific Makefile, which defines everything about
# the module that is common across architectures.

include $(TO_CORE)/kerninstd/kerninstd/make.module.tmpl

# Now make any necessary architecture specific changes to variables:

SYSLIBS	     += -lsocket

SRCS	+= $(TO_KERNINSTD)/basicblock/src/sparc_basicblock.C \
	   $(TO_KERNINSTD)/basicblock/src/sparc_bb.C \
	   $(TO_KERNINSTD)/bitwise_dataflow_fn/src/sparc_bitwise_dataflow_fn.C \
	   $(TO_KERNINSTD)/bitwise_dataflow_fn/src/sparc_mbdf_window.C \
	   $(TO_KERNINSTD)/cpcInterface/src/cpcInterface.C \
	   $(TO_KERNINSTD)/codeObject/src/sparc_codeObjectCreationOracle.C \
	   $(TO_KERNINSTD)/codeObject/src/pcIndepWithFallThruCodeObject.C \
	   $(TO_KERNINSTD)/codeObject/src/pcIndepNoFallThruCodeObject.C \
	   $(TO_KERNINSTD)/codeObject/src/interProcCallCodeObject.C \
	   $(TO_KERNINSTD)/codeObject/src/interProcTailCallCodeObject.C \
	   $(TO_KERNINSTD)/codeObject/src/callToObtainPCCodeObject.C \
	   $(TO_KERNINSTD)/codeObject/src/recursiveCallCodeObject.C \
	   $(TO_KERNINSTD)/codeObject/src/condBranchCodeObject.C \
	   $(TO_KERNINSTD)/codeObject/src/branchAlwaysCodeObject.C \
	   $(TO_KERNINSTD)/codeObject/src/interProcCondBranchCodeObject.C \
	   $(TO_KERNINSTD)/codeObject/src/interProcBranchAlwaysCodeObject.C \
	   $(TO_KERNINSTD)/codeObject/src/jumpTableCodeObject.C \
	   $(TO_KERNINSTD)/codeObject/src/simpleJumpTable32CodeObject.C \
	   $(TO_KERNINSTD)/codeObject/src/offsetJumpTable32CodeObject.C \
	   $(TO_KERNINSTD)/codeObject/src/intraProcJmpToFixedAddrCodeObject.C \
	   $(TO_KERNINSTD)/emitters/src/doflush.s \
	   $(TO_KERNINSTD)/emitters/src/sparc_tempBufferEmitter.C \
	   $(TO_KERNINSTD)/emitters/src/sparc_directToMemoryEmitter.C \
	   $(TO_KERNINSTD)/function/src/sparc_cfgCreationOracle.C \
	   $(TO_KERNINSTD)/function/src/sparc_funkshun.C \
	   $(TO_KERNINSTD)/function/src/sparc_misc.C \
	   $(TO_KERNINSTD)/insnVec/src/sparc_insnVec.C \
	   $(TO_KERNINSTD)/kernelDriver/src/pcrRegister.C \
	   $(TO_KERNINSTD)/kernelDriver/src/picRegister.C \
	   $(TO_KERNINSTD)/kerninstd/src/section.C \
	   $(TO_KERNINSTD)/kerninstd/src/gatherMod-sol.C \
	   $(TO_KERNINSTD)/kmem/src/sparc_kmem.C \
	   $(TO_KERNINSTD)/launchSite/src/sparc_launcher.C \
	   $(TO_KERNINSTD)/launchSite/src/sparc_callLauncher.C \
	   $(TO_KERNINSTD)/launchSite/src/sparc_QuickBranchLauncher.C \
	   $(TO_KERNINSTD)/launchSite/src/sparc_SpringBoardLauncher.C \
	   $(TO_KERNINSTD)/outlining/src/fnAndBlockId.C \
	   $(TO_KERNINSTD)/relocatableCode/src/sparc_relocatableCode.C \
	   $(TO_KERNINSTD)/sparc_instr/src/sparc_instr.C \
	   $(TO_KERNINSTD)/sparc_reg/src/sparc_reg.C \
	   $(TO_KERNINSTD)/sparc_reg_set/src/sparc_reg_set.C \
	   $(TO_KERNINSTD)/sparc_reg_set/src/sparc_fp_regset.C \
	   $(TO_KERNINSTD)/sparc_reg_set/src/sparc_int_regset.C \
	   $(TO_KERNINSTD)/sparc_reg_set/src/sparc_misc_regset.C \
	   $(TO_KERNINSTD)/sparc_reg_set/src/regSetManager.C \
	   $(TO_KERNINSTD)/usualJump/src/usualJump.C \
	   $(TO_KERNINSTD)/LRAndR/src/sparc_LRAndR.C \
	   $(TO_KERNINSTD)/LRAndR/src/sparc_branchAlways_LRAndR.C \
	   $(TO_KERNINSTD)/LRAndR/src/sparc_branchNever_LRAndR.C \
	   $(TO_KERNINSTD)/LRAndR/src/sparc_call_LRAndR.C \
	   $(TO_KERNINSTD)/LRAndR/src/sparc_condBranch_0FreeAfterInsn_LRAndR.C \
	   $(TO_KERNINSTD)/LRAndR/src/sparc_condBranch_1FreeAfterInsn_LRAndR.C \
	   $(TO_KERNINSTD)/LRAndR/src/sparc_condBranch_na_2FreeBeforeInsn_LRAndR.C \
	   $(TO_KERNINSTD)/LRAndR/src/sparc_doneOrRetry_LRAndR.C \
	   $(TO_KERNINSTD)/LRAndR/src/sparc_jmpl_LRAndR.C \
	   $(TO_KERNINSTD)/LRAndR/src/sparc_nonDCTI_LRAndR.C \
	   $(TO_KERNINSTD)/LRAndR/src/sparc_unwound_callRestore_LRAndR.C \
	   $(TO_KERNINSTD)/LRAndR/src/sparc_unwound_callWriteO7_LRAndR.C \
	   $(TO_KERNINSTD)/LRAndR/src/sparc_v9Return_LRAndR.C \
	   $(TO_KERNINST_DRIVER)/src/ari_ops.s

VPATH +=   $(TO_KERNINST_DRIVER)/src \
	   $(TO_KERNINSTD)/sparc_instr/src \
	   $(TO_KERNINSTD)/sparc_reg/src \
	   $(TO_KERNINSTD)/sparc_reg_set/src

IFLAGS +=  -I$(TO_KERNINST_DRIVER)/src \
	   -I$(TO_KERNINSTD)/sparc_instr/src \
	   -I$(TO_KERNINSTD)/sparc_reg/src \
	   -I$(TO_KERNINSTD)/sparc_reg_set/src

ASFLAGS += -xarch=v8plusa

# Include the "standard program template".  This defines all the
# common targets like "clean", "install", etc.

include $(TO_CORE)/make.program.tmpl
