// orPredicate.C

#include "orPredicate.h"
#include "util/h/rope-utils.h" // addr2hex()
#include "util/h/kdrvtypes.h" // dptr_t

orPredicate::orPredicate(const predicate &ileft,
                         const predicate &iright) : predicate() {
   left = ileft.dup();
   right = iright.dup();
}

orPredicate::orPredicate(const orPredicate &src) : predicate() {
   left = src.left->dup();
   right = src.right->dup();
}

orPredicate::~orPredicate() {
   delete left;
   delete right;

   left = right = NULL; // help purify find mem leaks
}

orPredicate &orPredicate::operator=(const orPredicate &src) {
   if (&src == this)
      return *this;

   delete left;
   left = src.left->dup();
   
   delete right;
   right = src.right->dup();

   return *this;
}

// Main method: generates AST tree for the predicate
kapi_bool_expr orPredicate::getKapiSnippet() const
{
    kapi_bool_expr left_snippet = left->getKapiSnippet();
    kapi_bool_expr right_snippet = right->getKapiSnippet();
    kapi_bool_expr rv(kapi_log_or, left_snippet, right_snippet);

    return rv;
}
