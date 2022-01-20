#ifndef _VALUE_IN_RANGE_PREDICATE_H_
#define _VALUE_IN_RANGE_PREDICATE_H_

// A boolean predicate which evaluates to true iff a given
// integer value falls within a given range.

#include "predicate.h"

class valueInRangePredicate : public predicate {
    kapi_snippet value;
    kapi_int_t lower;
    kapi_int_t upper;
 public:
    valueInRangePredicate(const kapi_snippet &ivalue,
			  kapi_int_t ilower, kapi_int_t upper);

    predicate *dup() const;

    // Main method: generates AST tree for the predicate
    kapi_bool_expr getKapiSnippet() const;
};

#endif
