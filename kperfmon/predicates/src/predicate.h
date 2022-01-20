// predicate.h
// Ariel Tamches

#ifndef _PREDICATE_H_
#define _PREDICATE_H_

#include "common/h/String.h"
#include "kapi.h"

class predicate {
 public:
   virtual predicate *dup() const = 0;
   virtual ~predicate() {}

   // Main method: generates AST tree for the predicate
   virtual kapi_bool_expr getKapiSnippet() const = 0;
};

#endif
