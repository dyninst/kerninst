// valueInRangePredicate.C
// A boolean predicate which evaluates to true iff a given
// integer value falls within a given range.

#include "valueInRangePredicate.h"

valueInRangePredicate::valueInRangePredicate(const kapi_snippet &ivalue,
					     kapi_int_t ilower, 
					     kapi_int_t iupper) :
    predicate(), value(ivalue), lower(ilower), upper(iupper)
{
}

predicate *valueInRangePredicate::dup() const
{
   return new valueInRangePredicate(*this);
}

// Main method: generates AST tree for the predicate
kapi_bool_expr valueInRangePredicate::getKapiSnippet() const
{
    return kapi_bool_expr(kapi_log_and,
			  kapi_bool_expr(kapi_ge, value,
					 kapi_const_expr(lower)),
			  kapi_bool_expr(kapi_le, value,
					 kapi_const_expr(upper)));
}
