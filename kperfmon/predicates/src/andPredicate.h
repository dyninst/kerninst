// andPredicate.h

#ifndef _AND_PREDICATE_H_
#define _AND_PREDICATE_H_

#include "predicate.h"
//#include "util/h/hashFns.h"

class andPredicate : public predicate {
 private:
   predicate *left, *right;

 public:
   andPredicate(const predicate &ileft, const predicate &iright);
   andPredicate(const andPredicate &src);
  ~andPredicate();

   andPredicate& operator=(const andPredicate &src);

   predicate *dup() const {
      return new andPredicate(*left, *right);
   }

   // Main method: generates AST tree for the predicate
   kapi_bool_expr getKapiSnippet() const;
};

#endif
