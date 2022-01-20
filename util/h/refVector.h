// refVector.h
// reference-counted vector

#include "common/h/Vector.h"
#include "util/h/refCounterK.h"

#ifndef _REF_VECTOR_H_
#define _REF_VECTOR_H_

template <class T>
class refVector {
 private:
   typedef refCounterK<pdvector<T> > rctr;
   rctr x;
   
 public:
   // A few typedefs to be compatible with stl algorithms.
   typedef T        value_type;
   typedef T*       pointer;
   typedef const T* const_pointer;
   typedef T*       iterator;
   typedef const T* const_iterator;
   typedef T&       reference;
   typedef const T& const_reference;

   refVector() : x(rctr::MANUAL_CONSTRUCT()) {
      (void)new((void*)x.getManualConstructObject())pdvector<T>();
   }
   refVector(const refVector &src) : x(src.x) {} // fast
   refVector(const pdvector<T> &src) : x(rctr::MANUAL_CONSTRUCT()) { // not fast
      (void)new((void*)x.getManualConstructObject())pdvector<T>(src);
   }

   // initializing as 'sum' of two vectors (two refVector<T>'s or two pdvector<T>'s):
   refVector(const refVector<T> &src1,
             const refVector<T> &src2) : x(rctr::MANUAL_CONSTRUCT()) { // not fast
      (void)new((void*)x.getManualConstructObject())pdvector<T>(src1.x.getData(), src2.x.getData());
   }
   refVector(const pdvector<T> &src1,
             const pdvector<T> &src2) : x(rctr::MANUAL_CONSTRUCT()) { // not fast
      (void)new((void*)x.getManualConstructObject())pdvector<T>(src1, src2);
   }

   // initialize as a refVector<T>/pdvector<T> plus one T:
   refVector(const refVector<T> &src1,
             const T &src2) : x(rctr::MANUAL_CONSTRUCT()) { // not fast
      (void)new((void*)x.getManualConstructObject())pdvector<T>(src1.x.getData(), src2);
   }
   refVector(const pdvector<T> &src1,
             const T &src2) : x(rctr::MANUAL_CONSTRUCT()) { // not fast
      (void)new((void*)x.getManualConstructObject())pdvector<T>(src1, src2);
   }

   refVector(const T &t, unsigned count) : x(rctr::MANUAL_CONSTRUCT()) { // not fast
      (void)new((void*)x.getManualConstructObject())pdvector<T>(t, count);
   }
   refVector(unsigned sz) : x(rctr::MANUAL_CONSTRUCT()) { // not fast
      (void)new((void*)x.getManualConstructObject())pdvector<T>(sz);
   }

  ~refVector() {} // fast unless ref count is now zero

   refVector& operator=(const refVector &other) {
      x = other.x; // fast
      return *this;
   }

   refVector& operator+=(const refVector &other) {
      if (x.isSafeToModifyInPlace())
         x.getModifiableData().operator+=(other);
      else {
         // we avoid calling prepareToModifyInPlace() followed by operator+=(),
         // because the former will have to reallocate, call vector<>'s copy ctor,
         // only to, in all liklihood, repeat similar actions for the operator+=().
         // So we are more clever:

         const pdvector<unsigned> &oldUnderlyingObject = x.getData();
         
         pdvector<unsigned> *newUnderlyingObject = x.dereferenceAndReconstructManually();
            // deref will be quick, and wont invalidate oldUnderlyingObject, since
            // ref count was >= 2 in this case.

         // Invoke one of pdvector<>'s operator+() - style constructor:
         (void)new((void*)newUnderlyingObject)pdvector<unsigned>
            (oldUnderlyingObject, other);
      }
      return *this;
   }
   refVector& operator+=(const T &item) {
      if (x.isSafeToModifyInPlace())
         x.getModifiableData().operator+=(item);
      else {
         // we *could* call x.prepareToModifyInPlace(), but that would
         // invoke the copy ctor on the underlying object, which is
         // quite expensive for the pdvector class!  This would then be
         // followed by an operator+=() on the pdvector class, which is
         // *also* expensive!  So we do better by doing things
         // manually:

         const pdvector<unsigned> &oldUnderlyingObject = x.getData();

         pdvector<unsigned> *newUnderlyingObject = x.dereferenceAndReconstructManually();
            // the dereference will be quick, and won't invalidate oldUnderlyingObject,
            // since the ref count was >= 2 before the call.

         // invoke one of pdvector<>'s operator+()-style constructors:
         (void)new((void*)newUnderlyingObject)pdvector<unsigned>(oldUnderlyingObject,
                                                               item);
      }

      return *this;
   }
   refVector operator+(const T &item) const {
      refVector result(this, item);
         // allocates new vector; copy-constructs
      return result; // fast copy-ctor
   }

   reference operator[](unsigned i) {
      // probably optimial as is
      x.prepareToModifyInPlace();
      return x.getModifiableData().operator[](i);
   }
   const_reference operator[](unsigned i) const {
      return x.getData().operator[](i);
   }

   void pop_back() {
      // probably optimal as is, assuming pdvector<>'s pop_back() never reallocates
      x.prepareToModifyInPlace();
      x.getModifiableData().pop_back();
   }
   
   void swap(refVector &other) {
      x.swap(other);
   }

   reference front() { return *begin(); }
   const_reference front() const { return *begin(); }

   reference back() { return *(end()-1); }
   const_reference back() const { return *(end()-1); }

   unsigned size() const { return x.getData().size(); }

   void resize(unsigned nsz, bool exact=true) {
      // XXX -- not optimal when prepareToModifyInPlace() has to invoke
      // copy-ctor for the underlying object, only to be followed by an
      // additional semantics-altering operation that could in theory have
      // been done at the same time, perhaps avoiding a second copy.
      x.prepareToModifyInPlace();
      x.getModifiableData().resize(nsz, exact);
   }

   void shrink(unsigned nsz) {
      // assuming pdvector<>'s shrink() doesn't ever reallocate, this is
      // probably optimal as is.
      x.prepareToModifyInPlace();
      x.getModifiableData().shrink(nsz);
   }
   void clear() {
      // probably optimal as is
      x.prepareToModifyInPlace();
      x.getModifiableData().clear();
   }
   void zap() {
      // XXX -- not optimal when prepareToModifyInPlace() has to invoke
      // copy-ctor for the underlying object, only to be followed by an
      // additional semantics-altering operation that could in theory have
      // been done at the same time, perhaps avoiding a second copy.
      x.prepareToModifyInPlace();
      x.getModifiableData().zap();
   }

   void grow(unsigned nsz, bool exact=false) {
      // XXX -- not optimal when prepareToModifyInPlace() has to invoke
      // copy-ctor for the underlying object, only to be followed by an
      // additional semantics-altering operation that could in theory have
      // been done at the same time, perhaps avoiding a second copy.
      x.prepareToModifyInPlace();
      x.getModifiableData().grow(nsz, exact);
   }

   void reserve_exact(unsigned n) {
      // XXX -- not optimal when prepareToModifyInPlace() has to invoke
      // copy-ctor for the underlying object, only to be followed by an
      // additional semantics-altering operation that could in theory have
      // been done at the same time, perhaps avoiding a second copy.
      x.prepareToModifyInPlace();
      x.getModifiableData().reserve_exact(n);
   }
   void reserve_roundup(unsigned n) {
      // XXX -- not optimal when prepareToModifyInPlace() has to invoke
      // copy-ctor for the underlying object, only to be followed by an
      // additional semantics-altering operation that could in theory have
      // been done at the same time, perhaps avoiding a second copy.
      x.prepareToModifyInPlace();
      x.getModifiableData().reserve_roundup(n);
   }

   unsigned capacity() const { return x.getData().capacity(); }

   iterator reserve_for_inplace_construction(unsigned nelems) {
      // XXX -- not optimal when prepareToModifyInPlace() has to invoke
      // copy-ctor for the underlying object, only to be followed by an
      // additional semantics-altering operation that could in theory have
      // been done at the same time, perhaps avoiding a second copy.
      x.prepareToModifyInPlace();
      return x.getModifiableData().reserve_for_inplace_construction(nelems);
   }
   iterator append_with_inplace_construction() {
      // XXX -- not optimal when prepareToModifyInPlace() has to invoke
      // copy-ctor for the underlying object, only to be followed by an
      // additional semantics-altering operation that could in theory have
      // been done at the same time, perhaps avoiding a second copy.
      x.prepareToModifyInPlace();
      return x.getModifiableData().append_with_inplace_construction();
   }

   iterator begin() {
      // probably optimial
      x.prepareToModifyInPlace();
      return x.getModifiableData().begin();
   }
   const_iterator begin() const {
      return x.getData().begin();
   }

   iterator end() {
      // probably optimal
      x.prepareToModifyInPlace();
      return x.getModifiableData().end();
   }
   const_iterator end() const {
      return x.getData().end();
   }
};

#endif
