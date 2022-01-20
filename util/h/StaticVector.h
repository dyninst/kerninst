// StaticVector.h
// like Vector.h, but no allocation/deallocation is done.  We assume
// that someone else takes care of all those details.
// Makes copying, constructors fast.  But also makes some methods
// like += impossible...all methods must be const.

#ifndef _STATIC_VECTOR_H_
#define _STATIC_VECTOR_H_

#include <assert.h>

template <class T>
class static_vector {
 private:
   const T *data;
   unsigned len;

 public:
   static_vector(const T *idata, unsigned ilen) {
      data = idata;
      len = ilen;
   }
   static_vector(const static_vector &src) {
      data = src.data;
      len = src.len;
   }
  ~static_vector() {}

   static_vector& operator=(const static_vector &src) {
      data = src.data;
      len = src.len;
      return *this;
   }

   const T *get_data() const {return data;}

   const T &operator[](unsigned ndx) const {
      assert(ndx < len);
      return data[ndx];
   }

   unsigned size() const {
      return len;
   }
};

#endif
