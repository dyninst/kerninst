// StaticString.h
// Holds a ptr to a null-terminated char* which is statically allocated
// (i.e. we don't new or delete anything in this class!)
// For the same reason, we don't really need a ref ctr; copying isn't that slow.

#ifndef _STATIC_STRING_H_
#define _STATIC_STRING_H_

#include "common/h/debugOstream.h"
#include "util/h/StaticVector.h"
#include <iostream.h>
#include <string.h>

class StaticString : public static_vector<char> {
 typedef static_vector<char> inherited;
 public:
   StaticString(const char *istr) : inherited(istr, strlen(istr)) {}
   StaticString(const StaticString &src) : inherited(src) {}
   class Dummy {};
   StaticString(Dummy) : inherited(NULL, -1U) {}
   StaticString() : inherited(NULL, -1U) {}
  ~StaticString() {}

   void set(const char *istr) {
      StaticString newval(istr);
      operator=(newval);
   }

   StaticString &operator=(const StaticString &src) {
      (void)inherited::operator=(src);
      return *this;
   }

   const char *c_str() const {return get_data();}
      // c_str() is preferred syntax because that's what STL string & rope use
   
   unsigned size() const {return inherited::size();}

   char operator[](unsigned ndx) const {
      return inherited::operator[](ndx);
   }

   bool operator==(const StaticString &cmp) const {
      return (size() == cmp.size() && 0==strcmp(get_data(), cmp.get_data()));
   }
   bool operator==(const char *cmp_str) const {
      return (0==strcmp(get_data(), cmp_str));
   }
};

extern ostream &operator<<(ostream &os, const StaticString &src);
extern debug_ostream &operator<<(debug_ostream &os, const StaticString &src);

#endif
