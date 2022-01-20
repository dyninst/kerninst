// GrowableStringPool.C

#include "util/h/GrowableStringPool.h"

GrowableStringPool::GrowableStringPool(const char *ipool, unsigned len) {
   assert(size() == 0);
   append(ipool, len);
   assert(size() == len);
}

GrowableStringPool::~GrowableStringPool() {
}

void GrowableStringPool::append(const char *str, unsigned len) {
   const unsigned oldsize = size();
   const unsigned newsize = oldsize + len;
      
   pool.reserve_roundup(newsize);
   // will likely make any existing StaticString's corrupt, since the pool will
   // likely move in memory
      
   while (len--)
      pool += *str++;
      
   assert(pool.size() = newsize);
}
