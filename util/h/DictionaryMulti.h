// DictionaryMulti.h
// A multimap dictionary: a key can map to a whole bunch of (a vector of) values.

#ifndef _DICTIONARY_MULTI_H_
#define _DICTIONARY_MULTI_H_

#include "util/h/Dictionary.h"

template <class K, class V, class ALLOC_VEC_OF_V=vec_stdalloc<V> >
class dictionary_hash_multi {
 public:
   typedef pdvector<V, ALLOC_VEC_OF_V> vectype;
 private:
   dictionary_hash<K, vectype> dict;
   unsigned sz; // can't rely on dict.size(); sz may be higher due to "multi" properties

   // prevent copying
   dictionary_hash_multi(const dictionary_hash_multi<K,V,ALLOC_VEC_OF_V> &);
   dictionary_hash_multi &operator=(const dictionary_hash_multi<K,V,ALLOC_VEC_OF_V> &);
   
 public:
   dictionary_hash_multi(unsigned (*hf)(const K &),
                         unsigned nbins=101,
                         double max_bin_load=0.7,
                         double bin_grow_factor=1.6) :
         dict(hf, nbins, max_bin_load, bin_grow_factor)
   {
      sz = 0;
   }
  ~dictionary_hash_multi() {}

   unsigned size() const { return sz; }

   bool defines(const K &k) const {
      return dict.defines(k);
   }

   const vectype &get(const K &k) const {
      return dict.get(k);
   }
   const vectype &get_and_remove(const K &k) const {
      return dict.get_and_remove(k);
   }

   void remove(const K &k, const V &v) {
      // asserts that it existed
      vectype &vec = dict.get(k);

      vectype::iterator iter = find(vec.begin(), vec.end(), v);
      assert(iter != vec.end());

      // sanity check.  There shouldn't be yet another match:
      assert(find(iter+1, vec.end(), v) == vec.end());

      *iter = vec.back();
      vec.pop_back();

      if (vec.size() == 0)
         dict.undef(k);
      
      assert(sz);
      --sz;
   }

   void set(const K &k, const V &v) {
      // unlike a regular dictionary, this routine doesn't barf if key already exists.
      // On the other hand, it *does* check that the value isn't a duplicate, too
      // WARNING: checking that value isn't a duplicate is linear, so this class
      // doesn't work well when we expect lots of matches to the same key.
      
      if (!dict.defines(k)) {
         dict.set(k, vectype(1, v));
      }
      else {
         vectype &vec = dict.get(k);

         // assert we're not about to add a duplicate value for this key:
         assert(find(vec.begin(), vec.end(), v) == vec.end());

         vec += v;
      }

      ++sz;
   }

   void clear() {
      dict.clear();
      sz = 0;
   }
};

#endif
