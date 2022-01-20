// andPredicate.C

#include "andPredicate.h"

andPredicate::andPredicate(const predicate &ileft,
                           const predicate &iright) : predicate() {
   left = ileft.dup();
   right = iright.dup();
}

andPredicate::andPredicate(const andPredicate &src) : predicate() {
   left = src.left->dup();
   right = src.right->dup();
}

andPredicate::~andPredicate() {
   delete left;
   delete right;

   left = right = NULL; // help purify find mem leaks
}

andPredicate &andPredicate::operator=(const andPredicate &src) {
   if (&src == this)
      return *this;

   delete left;
   left = src.left->dup();
   
   delete right;
   right = src.right->dup();

   return *this;
}

// Main method: generates AST tree for the predicate
kapi_bool_expr andPredicate::getKapiSnippet() const
{
    kapi_bool_expr left_snippet = left->getKapiSnippet();
    kapi_bool_expr right_snippet = right->getKapiSnippet();
    kapi_bool_expr rv(kapi_log_and, left_snippet, right_snippet);

    return rv;
}

