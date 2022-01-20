// loggedAllocRequests.h

#ifndef _LOGGED_ALLOC_REQUESTS_H_
#define _LOGGED_ALLOC_REQUESTS_H_

#include "util/h/kdrvtypes.h"
#include "common/h/Vector.h"
#include <sys/kmem.h>
#include <sys/cmn_err.h>

template <class T>
class myallocator {
 public:
   static T *alloc(unsigned nelems) {
      return (T*)kmem_alloc(nelems * sizeof(T), KM_SLEEP);
   }
   static void free(T *vec, unsigned nelems) {
      kmem_free(vec, nelems * sizeof(T));
   }
};

class loggedAllocRequests {
 private:
   struct oneAllocReq {
      // This is what the user knows about:
      kptr_t prettyAddr;
      kptr_t prettyMappedAddr;
      unsigned prettyNumBytes;
      unsigned allocType;

      // And this is what we know are the *real* values (behind the scenes, we may
      // have added padding to achieve the user's desired alignment)
      kptr_t actuallyAllocatedAt;
      kptr_t actuallyMappedAt; // will differ if nucleus-allocated
      unsigned actualNumBytesAllocated;

      oneAllocReq(kptr_t iprettyAddr,
                  kptr_t iprettyMappedAddr,
                  unsigned iprettyNumBytes,
                  unsigned iAllocType,
                  kptr_t iactuallyAllocatedAt,
                  kptr_t iactuallyMappedAt,
                  unsigned iactualNumBytesAllocated)
         : prettyAddr(iprettyAddr),
           prettyMappedAddr(iprettyMappedAddr),
           prettyNumBytes(iprettyNumBytes),
           allocType(iAllocType),
           actuallyAllocatedAt(iactuallyAllocatedAt),
           actuallyMappedAt(iactuallyMappedAt),
           actualNumBytesAllocated(iactualNumBytesAllocated)
      {}
   };

   typedef pdvector<oneAllocReq, myallocator<oneAllocReq> > vectype;
   vectype v;

   loggedAllocRequests(const loggedAllocRequests &);
   loggedAllocRequests &operator=(const loggedAllocRequests &);
   
 public:
   loggedAllocRequests() {
   }
  ~loggedAllocRequests() {
      if (v.size() > 0)
         cmn_err(CE_CONT, "loggedAllocRequests dtor: size is %d, should be zero.  Will probably be leaked memory as a result\n",
                 v.size());
   }

   void undo();
      // kerninstd is shutting down; now's our chance to avoid mem leaks
   
   void alloc(unsigned nbytes,
              unsigned requiredAlignment,
              unsigned allocType,
              kptr_t &result,
              kptr_t &result_mappable);
      // the above params are "pretty" version; actual behind-the-scenes values
      // can differ depending on padding added for alignment reasons.
      // tryForNucleusText: if true, we try, but if we can't allocate in the
      // nucleus, we return non-nucleus memory.  If this situation is not to
      // your liking, check the return address for non-nucleus, and delete
      // it if so.

   void free(kptr_t kernelAddr, kptr_t mappedKernelAddr, 
	     unsigned nbytes);
      // the above params are "pretty" version; actual behind-the-scenes values
      // can differ depending on padding added for alignment reasons
};

#endif
