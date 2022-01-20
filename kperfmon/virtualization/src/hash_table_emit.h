// hash_table_emit.h

#ifndef _HASH_TABLE_EMIT_H_
#define _HASH_TABLE_EMIT_H_

#include "util/h/kdrvtypes.h"
#include "tempBufferEmitter.h"
#include "sparc_reg_set.h"
#include "fnRegInfo.h"

// -----------------------------------------------------------------------------
// hash_table_initialize():
// -----------------------------------------------------------------------------

void emit_initialize_hashtable(tempBufferEmitter &em,
                               dptr_t hashTable_kerninstdAddr);
   // doesn't take in hashTable_kernelAddr since it doesn't need to write
   // any such pointers into the hash table (at least not during initialization)

// -----------------------------------------------------------------------------
// hash_table_add() and hash_table_pack():
// -----------------------------------------------------------------------------

fnRegInfo emit_hashtable_add(tempBufferEmitter &,
                             const fnRegInfo &, // params, return value & avail regs
                             kptr_t hashTable_kernelAddr);
   // returns fnRegInfo for hash table pack.
                        
void emit_hashtable_pack(tempBufferEmitter &em,
                         kptr_t hashTable_kernelAddr,
                         const fnRegInfo & // params, avail regs, and return val
                         );

// -----------------------------------------------------------------------------
// hashfunc():
// -----------------------------------------------------------------------------

void emit_hashfunc(tempBufferEmitter &,
                   bool inlinedEmit,
                   const fnRegInfo &theFnRegInfo);

// -----------------------------------------------------------------------------
// hash_table_remove():
// -----------------------------------------------------------------------------

void emit_hashtable_remove(tempBufferEmitter &em,
                           kptr_t hashTableKernelAddr,
                           const fnRegInfo &theFnRegInfo);

#endif
