// GrowableStringPool.h
// The string pool can grow in size
// Therefore, char* references must be treated gently, for they are volatile!

#ifndef _GROWABLE_STRING_POOL_H_
#define _GROWABLE_STRING_POOL_H_

#include "util/h/StaticString.h"
#include "common/h/Vector.h"
#include <assert.h>

class GrowableStringPool {
 private:
   // we choose pdvector<char> over string because the former grows more gracefully
   pdvector<char> pool;

 public:
   GrowableStringPool(const char *ipool, unsigned len);
  ~GrowableStringPool();

   void append(const char *str, unsigned len);

   StaticString getStaticString(unsigned offset) const {
      // offset is a byte offset
      assert(offset < pool.size());
      const char *ptr = pool.begin() + offset;
      return StaticString(ptr);
   }
};

#endif
