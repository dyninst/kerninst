#include "pidInSetPredicate.h"

// Main method: generates AST tree for the predicate
kapi_bool_expr pidInSetPredicate::getKapiSnippet() const
{
    kapi_pid_expr pid_expr;

    // Deal with the first pid in the list separately
    pdvector<long>::const_iterator pid_iter = pidsToMatch.begin();
    assert(pid_iter != pidsToMatch.end());
    kapi_bool_expr rv(kapi_eq, pid_expr, kapi_const_expr(*pid_iter));
    pid_iter++;

    // Chain the rest of the pid's to the first one with OR operations
    for (; pid_iter != pidsToMatch.end(); pid_iter++) {
	rv = kapi_bool_expr(kapi_log_or, rv,
			    kapi_bool_expr(kapi_eq, pid_expr, 
					   kapi_const_expr(*pid_iter)));
    }
    
    return rv;
}
