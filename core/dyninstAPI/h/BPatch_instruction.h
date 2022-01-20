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

#ifndef _BPatch_instruction_h_
#define _BPatch_instruction_h_

#include "BPatch_dll.h"

class InstrucIter;

class BPATCH_DLL_EXPORT BPatch_instruction {

 public:
  // maximum number of memory accesses per instruction; platform dependent
#if defined(i386_unknown_nt4_0)
  // Translation from C++ to VC++ 6.0
#define nmaxacc_NP 2
#elif defined(i386_unknown_linux2_0) \
   || defined(x86_64_unknown_linux2_4) /* Blind duplication - Ray */
  static const unsigned int nmaxacc_NP = 2;
#elif defined (__XLC__) || defined(__xlC__)
#define nmaxacc_NP 1
#else
  static const unsigned int nmaxacc_NP = 1;
#endif

 protected:

  unsigned int nacc;
  void *instr; // Can't mention instruction here
  unsigned char *buffer;
  int length;
  bool isLoad[nmaxacc_NP];
  bool isStore[nmaxacc_NP];
  int preFcn[nmaxacc_NP];       // prefetch function (-1 = none)
  int condition[nmaxacc_NP];    // -1 means no condition, all other values are machine specific
                                // conditions, currently (8/13/02) the tttn field on x86
  bool nonTemporal[nmaxacc_NP]; // non-temporal (cache non-polluting) write on x86

 public:

  BPatch_instruction(const void *_buffer,
		     unsigned char _length);

  virtual ~BPatch_instruction() { if (buffer != NULL) delete[] buffer; }

  void getInstruction(unsigned char *&_buffer, unsigned char &_length) {
    _buffer = buffer;
    _length = length;
  }

  // Not yet implemented
  char *getMnemonic() const { return NULL; }

 public:

  bool equals(const BPatch_instruction* mp) const { return mp ? equals(*mp) : false; }

  bool equals(const BPatch_instruction& rp) const
  {
    bool res;

    res = 
      (isLoad == rp.isLoad) &&
      (isStore == rp.isStore) &&
      (preFcn == rp.preFcn) &&
      (condition == rp.condition) && 
      (nonTemporal == rp.nonTemporal);

    return res;
  }

  bool hasALoad() const { return nacc == 1 ? isLoad[0] : (isLoad[0] || isLoad[1]); }
  bool hasAStore() const { return nacc == 1 ? isStore[0] : (isStore[0] || isStore[1]); }
  bool hasAPrefetch_NP() const { return preFcn[0] >= 0; }

  unsigned int getNumberOfAccesses() const { return nacc; }
  bool isALoad(int which = 0) const { return isLoad[which]; }
  bool isAStore(int which = 0) const { return isStore[which]; }
  bool isAPrefetch_NP(int which = 0) const { return preFcn[which] >= 0; }
  bool isConditional_NP(int which = 0) const { return condition[which] >= 0; }
  bool isNonTemporal_NP(int which = 0) const { return nonTemporal[which]; }
  int  prefetchType_NP(int which = 0) const { return preFcn[which]; }
  int  conditionCode_NP(int which = 0) const { return condition[which]; }
};

#endif
