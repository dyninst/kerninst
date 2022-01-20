// loggedAllocRequests.h

#ifndef _KERNINST_LOGGED_ALLOC_REQUESTS_
#define _KERNINST_LOGGED_ALLOC_REQUESTS_

#include "kernInstIoctls.h" // kptr_t

typedef struct oneAllocReq {
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
} oneAllocReq;

int OAR_allocate(oneAllocReq *oar, unsigned nbytes,
                 unsigned reqdAlign, unsigned allocType);

typedef struct loggedAllocRequests {
   oneAllocReq *reqs;
   unsigned num_reqs;
} loggedAllocRequests;


loggedAllocRequests* initialize_logged_allocRequests(void);
void destroy_logged_allocRequests(loggedAllocRequests *lar);

void LAR_undo(loggedAllocRequests *lar);
int LAR_alloc(loggedAllocRequests *lar,  unsigned nbytes, 
              unsigned requiredAlignment, unsigned allocType, 
              kptr_t *result, kptr_t *result_mappable);
int LAR_free(loggedAllocRequests *lar, 
             kptr_t kernelAddr, kptr_t mappedKernelAddr, 
             unsigned nbytes);

#endif
