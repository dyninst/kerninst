// pidInSetPredicate.h
// True iff the currently running kthread's process id is a member of a specified set
// of pids.

// Obtaining pid from kthread: check kernel code for the macro ttoproc(),
// which returns the process ptr.  Given that, obtain the pid * via
// proc->p_pidp, which is of type "pid*", a structure containing lots of pid-related
// information.  Given that, obtain the actual pid via
// p_pidp->pid_id
// which is of type pid_t
// which is typedefed in <sys/types.h> to be a long.  For now that means 32 bits.

// So in summary:
// curthread->t_procp->p_pidp->pid_t
// Gives the pid of the currently running kthread.
// Yuck, too many loads.

// Assembly code:
// Note: curthread is always %g7
// ld [%g7 + T_PROCP_OFFSET], R1
// ld [R1 + T_PID_P_OFFSET], R1
// ld [R1 + PID_T_OFFSET], R1
// now R1 contains the pid (it's of type: long [32 bits])

#ifndef _PID_IN_SET_PREDICATE_H_
#define _PID_IN_SET_PREDICATE_H_

#include "predicate.h"
#include "common/h/Vector.h"

class pidInSetPredicate : public predicate {
 private:
   pdvector<long> pidsToMatch;

 public:

   pidInSetPredicate(const pdvector<long> &ipids_to_match) :
       predicate(), pidsToMatch(ipids_to_match) {
   }

   pidInSetPredicate(const pidInSetPredicate &src) : predicate(),
                                                     pidsToMatch(src.pidsToMatch) {
   }

  ~pidInSetPredicate() {
   }

   pidInSetPredicate& operator=(const pidInSetPredicate &src) {
      pidsToMatch = src.pidsToMatch; // already protected against x=x, right?
      return *this;
   }

   predicate *dup() const {
      return new pidInSetPredicate(*this);
   }

   // Main method: generates AST tree for the predicate
   kapi_bool_expr getKapiSnippet() const;
};

#endif
