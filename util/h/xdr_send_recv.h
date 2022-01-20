/*
 * Copyright (c) 1996-1999 Barton P. Miller
 * 
 * We provide the Paradyn Parallel Performance Tools (below
 * described as Paradyn") on an AS IS basis, and do not warrant its
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

#ifndef _XDR_SEND_RECV_KI_H_
#define _XDR_SEND_RECV_KI_H_

#include "pdutil/h/xdr_send_recv.h"
#include "common/h/triple.h"
#include "util/h/StringPool.h"
#include <utility> // pair

bool P_xdr_send(XDR *xdr, const StringPool &);
bool P_xdr_recv(XDR *xdr, StringPool &);

bool read1_bool(XDR *xdr);
#if !defined(i386_unknown_nt4_0)
uint16_t read1_uint16(XDR *xdr);
#endif
uint32_t read1_uint32(XDR *xdr);
unsigned read1_unsigned(XDR *xdr);
pdstring read1_string(XDR *xdr);

//--------------------
// New style XDR send/recv for pairs and triples

template <class P1, class P2>
bool P_xdr_send(XDR *xdr, const std::pair<P1, P2> &p) {
   return P_xdr_send(xdr, p.first) &&
          P_xdr_send(xdr, p.second);
}

template <class P1, class P2>
bool P_xdr_recv(XDR *xdr, std::pair<P1, P2> &p) {
   return P_xdr_recv(xdr, p.first) &&
          P_xdr_recv(xdr, p.second);
}

template <class P1, class P2, class P3>
bool P_xdr_send(XDR *xdr, const triple<P1, P2, P3> &p) {
   return P_xdr_send(xdr, p.first) &&
          P_xdr_send(xdr, p.second) &&
          P_xdr_send(xdr, p.third);
}

template <class P1, class P2, class P3>
bool P_xdr_recv(XDR *xdr, triple<P1, P2, P3> &p) {
   return P_xdr_recv(xdr, p.first) &&
          P_xdr_recv(xdr, p.second) &&
          P_xdr_recv(xdr, p.third);
}

#endif
