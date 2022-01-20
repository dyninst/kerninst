// StringPool.h

#ifndef _STRING_POOL_H_
#define _STRING_POOL_H_

#include "util/h/StaticString.h"
#include <assert.h>
#include <string.h>

struct XDR; // fwd declaration to obviate need to #include rpcUtil.h

class StringPool {
 private:
   char *pool;
   unsigned num_bytes;

   // explicitly disallow these expensive operations
   StringPool(const StringPool &src) {
      // warning: not a real copy!
      pool = src.pool;
      num_bytes = src.num_bytes;
   }
   
   StringPool& operator=(const StringPool &);
   
 public:
   StringPool(XDR *xdr);
   
   StringPool(const char *ipool, unsigned len) {
      // len includes all \0's, including the last one
      pool = new char[num_bytes=len];
      assert(pool);

      memcpy(pool, ipool, len);
   }

   class Dummy {};
   StringPool(Dummy) {
      // sorry for this hack; needed by igen
      pool = NULL;
      num_bytes = -1U;
   }

   void set_contents_hack(char *newpool, unsigned len) {
      // "newpool" must have been allocated with new char[]
      assert(pool == NULL);
      pool = newpool;
      
      assert(num_bytes == 0);
      num_bytes = len;
   }
      

//     StringPool makeAQuickCopy() const {
//        return StringPool(*this);
//     }
   
   ~StringPool() {
      delete [] pool;
      pool = NULL;
      num_bytes = 0;
   }

   StaticString getStaticString(unsigned offset) const {
      // offset is a byte offset
      assert(offset < num_bytes);
      const char *ptr = &pool[offset];
      return StaticString(ptr);
   }
   unsigned StaticString2Offset(const StaticString &str) const {
      return str.c_str() - pool;
   }
   const char *get_raw() const {
      return pool;
   }
   unsigned getNumBytes() const {
      return num_bytes;
   }
};

#endif
