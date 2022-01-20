// orPredicate.h

#ifndef _OR_PREDICATE_H_
#define _OR_PREDICATE_H_

#include "predicate.h"

class orPredicate : public predicate {
 private:
   predicate *left, *right;

 public:
   orPredicate(const predicate &ileft, const predicate &iright);
   orPredicate(const orPredicate &src);
  ~orPredicate();

   orPredicate& operator=(const orPredicate &src);

   predicate *dup() const {
      return new orPredicate(*left, *right);
   }

   // Main method: generates AST tree for the predicate
   kapi_bool_expr getKapiSnippet() const;
};

#endif
