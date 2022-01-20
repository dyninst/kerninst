// truePredicate.C

#include "truePredicate.h"

truePredicate::truePredicate() : predicate() { }

truePredicate::truePredicate(const truePredicate &src) : predicate(src) { }

truePredicate::~truePredicate() {
}

truePredicate &truePredicate::operator=(const truePredicate &) {
   return *this;
}

predicate *truePredicate::dup() const {
   return new truePredicate(*this);
}

// Main method: generates AST tree for the predicate
kapi_bool_expr truePredicate::getKapiSnippet() const
{
    return kapi_bool_expr(true);
}
