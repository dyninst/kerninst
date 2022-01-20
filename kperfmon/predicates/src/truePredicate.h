// truePredicate.h
// Ariel Tamches
// predicate that is always true

#ifndef _TRUE_PREDICATE_H_
#define _TRUE_PREDICATE_H_

#include "predicate.h"

class truePredicate : public predicate {
 public:
   truePredicate();
   truePredicate(const truePredicate &src);
   
  ~truePredicate();

   truePredicate &operator=(const truePredicate &);

   predicate *dup() const;

   // Main method: generates AST tree for the predicate
   kapi_bool_expr getKapiSnippet() const;
};

#endif
