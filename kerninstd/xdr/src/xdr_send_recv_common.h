// xdr_send_recv_common.h

#ifndef _XDR_SEND_RECV_COMMON_H_
#define _XDR_SEND_RECV_COMMON_H_

#include <inttypes.h> // uint16_t, etc.

// Forward declarations avoid the need to #include files, which invariably lead
// to lots of elf WEAK symbols in the .o file that don't get weeded out until 
// link time.  We prefer quicker compiling, smaller .o files, and quicker 
// linking (duh).

class basicblock_t;
class instr_t;
class insnVec_t;
class mbdf_window;
class monotone_bitwise_dataflow_fn;
class relocatableCode_t;
class fnCodeObjects;
class basicBlockCodeObjects;

#include "tempBufferEmitter.h"
#include "trick_module.h"
#include "trick_classes.h" // trick_regset & friends

bool P_xdr_send(XDR *, const basicblock_t &);

bool P_xdr_send(XDR *, const mbdf_window &);
bool P_xdr_recv(XDR *, mbdf_window &);

bool P_xdr_send(XDR *, const monotone_bitwise_dataflow_fn &);
bool P_xdr_recv(XDR *, monotone_bitwise_dataflow_fn &);
bool P_xdr_send(XDR *, const trick_mbdf &);
bool P_xdr_recv(XDR *, trick_mbdf &);

bool P_xdr_send(XDR *, const trick_fn &);
bool P_xdr_recv(XDR *, trick_fn &);

bool P_xdr_send(XDR *, const instr_t &);
bool P_xdr_recv(XDR *, instr_t &);

bool P_xdr_send(XDR *, const insnVec_t &);
bool P_xdr_recv(XDR *, insnVec_t &);

bool P_xdr_send(XDR *, const loadedModule &);
bool P_xdr_send(XDR *, const trick_module &);
bool P_xdr_recv(XDR *, trick_module &);

bool P_xdr_send(XDR *, const trick_regset &);
bool P_xdr_recv(XDR *, trick_regset &);

bool P_xdr_send(XDR *, const relocatableCode_t &);
bool P_xdr_recv(XDR *, relocatableCode_t &);
bool P_xdr_send(XDR *, const trick_relocatableCode &);
bool P_xdr_recv(XDR *, trick_relocatableCode &);

bool P_xdr_send(XDR *xdr, const tempBufferEmitter::insnOffset &);
bool P_xdr_recv(XDR *xdr, tempBufferEmitter::insnOffset &);

bool P_xdr_send(XDR *xdr, const fnCodeObjects &);
bool P_xdr_recv(XDR *xdr, fnCodeObjects &);

bool P_xdr_send(XDR *xdr, const basicBlockCodeObjects &);
bool P_xdr_recv(XDR *xdr, basicBlockCodeObjects &);

#endif
