// snippet.h

#ifndef _SNIPPET_H_
#define _SNIPPET_H_

#include "predicate.h"
#include "primitive.h"

// fwd decls, instead of #include's, lead to smaller .o files when templates
// are in any way involved.
class kapi_snippet;

class snippet {
 public:
   enum placement {preinsn, prereturn}; // postinsn to be added some day.
   
 private:
   placement pl;
   predicate *pred;
   primitive *prim;
   bool isPredicated;

 public:
   snippet(const snippet &src) : 
       pl(src.pl), prim(src.prim->dup()), isPredicated(src.isPredicated) {
       if (src.isPredicated) {
	   pred = src.pred->dup();
       }
   }
   snippet(placement ipl, const predicate &ipred, const primitive &iprim) :
      pl(ipl), pred(ipred.dup()), prim(iprim.dup()), isPredicated(true) {
   }
   snippet(placement ipl, const primitive &iprim) :
      pl(ipl), pred(0), prim(iprim.dup()), isPredicated(false) {
   }

   snippet &operator=(const snippet &src);
   
  ~snippet();

   snippet *dup() const { return new snippet(*this); }

   placement getPlacement() const { return pl; }

   kapi_snippet getKapiSnippet() const {
       if (isPredicated) {
	   // Compute the predicate and execute the primitive only if
	   // the predicate evaluated to true
	   return kapi_if_expr(pred->getKapiSnippet(), prim->getKapiSnippet());
       }
       else {
	   // No predicates -- execute the primitive unconditionally
	   return prim->getKapiSnippet();
       }
   }
};

#endif
