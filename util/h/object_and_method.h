// object_and_method.h

template <class RETT, class CLASS, class ARG>
class objectptr_and_method_type {
 private:
   CLASS *objectptr; // the object
   RETT (CLASS::*method)(ARG); // the method
 public:
   explicit objectptr_and_method_type(CLASS *iobjectptr,
                                   RETT (CLASS::*imethod)(ARG)) :
      objectptr(iobjectptr), method(imethod) { }
   RETT operator()(ARG a) {
      return (objectptr->*method)(a);
   }
};

template <class RETT, class CLASS, class ARG>
inline
objectptr_and_method_type<RETT, CLASS, ARG>
objectptr_and_method(CLASS *objectptr, RETT (CLASS::*method)(ARG)) {
   return objectptr_and_method_type<RETT, CLASS, ARG>(objectptr, method);
}

#define method(m) objectptr_and_method(this, &m)
