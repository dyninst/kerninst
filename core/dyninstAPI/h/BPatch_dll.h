/*
 * Copyright (c) 1996-2004 Barton P. Miller
 * 
 * We provide the Paradyn Parallel Performance Tools (below
 * described as "Paradyn") on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 * 
 * This license is for research uses.  For such uses, there is no
 * charge. We define "research use" to mean you may freely use it
 * inside your organization for whatever purposes you see fit. But you
 * may not re-distribute Paradyn or parts of Paradyn, in any form
 * source or binary (including derivatives), electronic or otherwise,
 * to any other organization or entity without our permission.
 * 
 * (for other uses, please contact us at paradyn@cs.wisc.edu)
 * 
 * All warranties, including without limitation, any warranty of
 * merchantability or fitness for a particular purpose, are hereby
 * excluded.
 * 
 * By your use of Paradyn, you understand and agree that we (or any
 * other person or entity with proprietary rights in Paradyn) are
 * under no obligation to provide either maintenance services,
 * update services, notices of latent defects, or correction of
 * defects for Paradyn.
 * 
 * Even if advised of the possibility of such damages, under no
 * circumstances shall we (or any other person or entity with
 * proprietary rights in the software licensed hereunder) be liable
 * to you or any third party for direct, indirect, or consequential
 * damages of any character regardless of type of action, including,
 * without limitation, loss of profits, loss of use, loss of good
 * will, or computer failure or malfunction.  You agree to indemnify
 * us (and any other person or entity with proprietary rights in the
 * software licensed hereunder) for any and all liability it may
 * incur to third parties resulting from your use of Paradyn.
 */
#ifndef _BPatch_dll_h_
#define _BPatch_dll_h_

// TEMPORARY PARADYND FLOWGRAPH KLUGE
// If we are building BPatch classes into paradynd we want BPATCH_DLL_EXPORT 
// to be defined as the empty string (for all platforms). This currently tests
// SHM_SAMPLING because it is defined for paradynd and not for the dyninst
// dll or dyninst clients, read '#if PARADYND'. 
#ifdef SHM_SAMPLING

#define	BPATCH_DLL_EXPORT

// otherwise we are not building paradynd
#else

#if defined(i386_unknown_nt4_0) || defined(mips_unknown_ce2_11) //ccw 6 apr 2001
// we are building for a Windows target 

// we get numerous spurious warnings about having some template classes
// needing to have a dll-interface if instances of these classes are
// to be used by classes whose public interfaces are exported from a DLL.
// Specifing the template classes with a DLL export interface doesn't 
// satisfy the compiler.  Until the compiler handles instantiated
// templates exported from DLLs better, we disable the warning when building
// or using the dyninstAPI DLL.
//
#pragma warning(disable:4251)

#ifdef BPATCH_DLL_BUILD
// we are building the dyninstAPI DLL
#define	BPATCH_DLL_EXPORT	__declspec(dllexport)

#else

// we are not building the dyninstAPI DLL
#define	BPATCH_DLL_EXPORT	__declspec(dllimport)
#if _MSC_VER >= 1300
#define	BPATCH_DLL_IMPORT   1
#endif
#endif	// BPATCH_DLL_BUILD

#else

// we are not building for a Windows target 
#define	BPATCH_DLL_EXPORT

#endif

#endif // TEMPORARY PARADYND FLOWGRAPH KLUGE


// declare our version string
extern "C" BPATCH_DLL_EXPORT const char V_libdyninstAPI[];

#endif /* _BPatch_dll_h_ */
